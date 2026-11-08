// x86 ABI/lifecycle checks. All output and game callbacks are mocked.
// Optional argument: a locally captured mapped PE, never required in CI.
#define NOMINMAX
#include <Windows.h>
#include <XInput.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <fstream>
#include <vector>

double fixedTime = 10;
BOOL WINAPI TestQPC(LARGE_INTEGER* p) { p->QuadPart = (LONGLONG)(fixedTime*1000000 + 0.5); return TRUE; }
BOOL WINAPI TestQPF(LARGE_INTEGER* p) { p->QuadPart = 1000000; return TRUE; }
#define QueryPerformanceCounter TestQPC
#define QueryPerformanceFrequency TestQPF
XINPUT_VIBRATION sent[4] = {};
int sends = 0;
DWORD WINAPI TestSetState(DWORD pad, XINPUT_VIBRATION* v) {
  assert(pad < 4); sent[pad] = *v; ++sends; return ERROR_SUCCESS;
}
#define XInputSetState TestSetState
#pragma section(".game", read, write)
__declspec(allocate(".game")) char gameArena[0x1000000] = {};
#define RUMBLE_PS_IMAGE_BASE ((uintptr_t)gameArena)
#define GAME_PROSTREET
#include "../src/Main.hpp"

void __fastcall TestListCtor(void*, void*, int) {}
void* registered = NULL;
void __fastcall TestListAdd(void* owner, void*, void* handle, void* iface) {
  assert(handle == (void*)IFEEDBACK_IHANDLE_ADDR);
  assert(*(void**)((char*)iface+4) == owner);
  registered = iface;
}
void TestProfile() {}

RumbleWaveRecord record = {5, 0.1f, 0.2f, 0, 10, 0.2f, 0.4f, 100};
const void* expectedSurface = NULL;
float bumpThreshold = 0.25f, bumpGain = 0.8f;
unsigned int lastWaveKey = 0;
const void* __fastcall TestAttribute(const void* surface, void*, unsigned int key, unsigned int index) {
  assert(surface == expectedSurface); // also detects a float being misread as a surface
  if (key == ATTRIB_KEY_FFB_BUMP_THRESHOLD) return &bumpThreshold;
  if (key == ATTRIB_KEY_FFB_BUMP_GAIN) return &bumpGain;
  lastWaveKey = key;
  return index == 0 ? &record : NULL;
}

struct NativeInterface { void** vtable; void* owner; void* handle; };
void* playerTable[16] = {}, *bodyTable[12] = {}, *suspensionTable[32] = {}, *inputTable[9] = {};
void* simableTable[27] = {};
NativeInterface simable = {simableTable}, player = {playerTable}, body = {bodyTable}, suspension = {suspensionTable},
                input = {inputTable};
NativeInterface otherSimable = {simableTable};
float velocity[3] = {30,0,0}, angular[3] = {0,0,1};
float otherVelocity[3] = {25,0,0};
void* __fastcall GetSimable(void*,void*) { return &simable; }
void __fastcall CollisionVelocity(void* obj,void*,float* out) { memcpy(out,obj == &otherSimable ? otherVelocity : velocity,12); }
void* __cdecl Resolve(void* handle) { return handle == (void*)0x1234 ? &simable : handle == (void*)0x5678 ? &otherSimable : NULL; }
void Tick(IFeedback& f, double dt = 0.01) { fixedTime += dt; f.EndUpdate(); f.PollEffects(); }
const float* __fastcall Linear(void*,void*) { return velocity; }
const float* __fastcall Angular(void*,void*) { return angular; }
float __fastcall Speed(void*,void*) { return 30; }
float __fastcall Gas(void*,void*) { return 0.75f; }
float __fastcall Brake(void*,void*) { return 0.2f; }
float __fastcall Handbrake(void*,void*) { return 0.1f; }
int __fastcall Wheels(void*,void*) { return 4; }
bool __fastcall Ground(void*,void*,unsigned int wheel) { return wheel != 3; }
float __fastcall Slip(void*,void*,unsigned int wheel) { return 10.0f*wheel; }
float __fastcall Skid(void*,void*,unsigned int wheel) { return 3.0f*wheel; }
float __fastcall NativeRoad(void*,void*,unsigned int wheel) { return 0.3f+0.1f*wheel; }
void* __fastcall Find(void*,void*,void* handle) {
  if (handle == (void*)IRIGIDBODY_IHANDLE_ADDR) return &body;
  if (handle == (void*)ISUSPENSION_IHANDLE_ADDR) return &suspension;
  if (handle == (void*)IINPUT_IHANDLE_ADDR) return &input;
  if (handle == (void*)IVEHICLE_IHANDLE_ADDR) return &simable;
  return NULL;
}

void TestVehicle() {
  playerTable[1] = GetSimable;
  bodyTable[9] = Linear; bodyTable[10] = Angular; bodyTable[11] = Speed;
  inputTable[2] = Brake; inputTable[3] = Gas; inputTable[8] = Handbrake;
  suspensionTable[2] = Skid; suspensionTable[3] = Slip; suspensionTable[5] = Wheels;
  suspensionTable[13] = Ground; suspensionTable[31] = NativeRoad;
  void* players[] = {&player};
  *(void***)RumblePSAddress(PLAYER_ILISTABLE_ADDR) = players;
  *(int*)RumblePSAddress(PLAYER_ILISTABLE_COUNT_ADDR) = 1;
  *(int*)RumblePSAddress(GAMEFLOWMANAGER_STATUS_ADDR) = 6;
  UTL_Com_Object_IList_Find = (decltype(UTL_Com_Object_IList_Find))Find;
  RumbleVehicleState s = {};
  assert(RumbleReadVehicle(s));
  assert(s.fSpeed == 30 && s.fGas == 0.75f && s.fBrake == 0.2f && s.fHandBrake == 0.1f);
  assert(s.nWheels == 4 && !s.bOnGround[3] && s.bOnGround[0]);
  assert(s.fSlip[2] == 20 && s.fSkid[2] == 6 && s.fWheelAV[2] == 0 && s.fNativeRoad[2] == 0.5f);
  assert(s.fLinVel[0] == 30 && s.fAngVel[2] == 1);
  *(int*)RumblePSAddress(PLAYER_ILISTABLE_COUNT_ADDR) = 0;
  assert(!RumbleReadVehicle(s)); // rebuilding the player list
  *(int*)RumblePSAddress(GAMEFLOWMANAGER_STATUS_ADDR) = 3;
}

// Replay the producer's ordering at 0x4D0FB9..0x4D105A: it pushes the surface
// BEFORE calling the slot-0x7C getter. It consumes only the wheel index, leaving
// surface on the stack; the producer then pushes its scalar and wheel index.
// A typed call with the adapter's own declaration would hide an ordering bug.
__declspec(naked) void ReplayNativeRoad(void* feedback, unsigned int wheel, const void* surface) {
  __asm {
    push ebx
    push esi
    push edi
    mov ebx, [esp+16]
    mov edi, [esp+20]
    lea esi, suspension
    push [esp+24]
    push edi
    mov ecx, esi
    mov eax, [esi]
    call [eax+0x7C]
    mov edx, [ebx]
    push ecx
    fstp dword ptr [esp]
    push edi
    mov ecx, ebx
    call [edx+0x48]
    pop edi
    pop esi
    pop ebx
    ret
  }
}

int originalCollisions = 0;
void __fastcall OriginalCollision(void*, void*, const COLLISION_INFO&) { ++originalCollisions; }
void TestCollisionListener(IFeedback& f) {
  void* contextTable[4] = {}; contextTable[3] = GetSimable;
  alignas(4) char listenerStorage[0x30] = {};
  *(void***)listenerStorage = contextTable;
  void* listener = listenerStorage+0x28;
  *(IFeedback**)((char*)listener+4) = &f;
  simable.handle = (void*)0x1234;
  RumblePSOriginalCollision = (decltype(RumblePSOriginalCollision))OriginalCollision;
  COLLISION_INFO hit = {}; hit.flags = 2; hit.normal[0] = 1; hit.closingVel[0] = 20;
  // Reproduce the case dropped by the stock objA lookup: world A, local car B.
  hit.objB = simable.handle;
  f.ClearEffects(); f.ResumeEffects();
  RumblePSCollisionHook(listener, NULL, hit);
  assert(f.mMW.collision.amplitude > 0);
  Tick(f); Tick(f);
  assert(sent[0].wLeftMotorSpeed > 0 && sent[0].wRightMotorSpeed == 0);
  assert(originalCollisions == 0);
  // The reversed participant order works too.
  hit.objA = simable.handle; hit.objB = NULL; f.ClearEffects();
  RumblePSCollisionHook(listener, NULL, hit);
  assert(f.mMW.collision.amplitude > 0);
  hit.objA = (void*)0x999; f.ClearEffects();
  RumblePSCollisionHook(listener, NULL, hit);
  assert(f.mMW.collision.amplitude == 0); // stale/unrelated contact
  hit.objA = simable.handle; f.PauseEffects();
  RumblePSCollisionHook(listener, NULL, hit);
  assert(f.mMW.collision.amplitude == 0);
  *(void**)((char*)listener+4) = NULL;
  RumblePSCollisionHook(listener, NULL, hit);
  assert(originalCollisions == 1); // foreign/native devices keep the original handler
}

void TestPSTyreUnits(IFeedback& f, void* surface) {
  f.ClearEffects(); f.mHasVehicle = true; memset(&f.mVehicle,0,sizeof(f.mVehicle));
  f.mVehicle.nWheels = 4; f.mVehicle.fSpeed = 30; f.mVehicle.fGas = 1;
  for (int i=0;i<4;++i) { f.mVehicle.bOnGround[i] = true; f.mVehicle.fSlip[i] = 0.2f; }
  f.mMixer.Begin(); f.MixTyres(0.016f);
  assert(f.mSpinLevel > 0); // .2 ratio is real wheelspin; MW's 3 m/s gate would miss it
  f.mVehicle.fGas = 0; f.mVehicle.fBrake = 1;
  for (int i=0;i<4;++i) f.mVehicle.fSlip[i] = -0.2f;
  f.mMixer.Begin(); f.MixTyres(0.016f); assert(f.mLockupLevel > 0);
  f.mVehicle.fBrake = 0;
  for (int i=0;i<4;++i) f.mVehicle.fSlip[i] = 0;
  f.mWheelSkid[2] = 0.2f;
  f.mMixer.Begin(); f.MixTyres(0.016f); assert(f.mSlideLevel > 0);

  f.ClearEffects();
}

bool Near(float a, float b) { return fabsf(a-b) < 0.00001f; }

// Optional numeric vectors produced by executing the user's local Xbox image.
// The normal test suite needs no console executable, assets or Python packages.
int CheckConsoleReference(const char* path) {
  std::ifstream file(path); assert(file.good());
  IFeedback f; f.Init(NULL,0); f.ResumeEffects();
  void* surface[4] = {(void*)0x1234}; expectedSurface = surface;
  Attrib_Instance_GetAttributePointer = (decltype(Attrib_Instance_GetAttributePointer))TestAttribute;
  record = {0,0,0,0,1,1,1,1};
  char kind; int cases = 0;
  while (file >> kind) {
    f.ClearEffects(); f.ResumeEffects();
    float actual = 0, expected = 0, x, y, z, load;
    if (kind == 'C') {
      int slot,enabled,previous,value; file >> slot >> enabled >> previous >> value;
      f.mPaused = !enabled;
      if (slot == 10) f.mMW.potential = previous;
      else if (slot == 11) f.mMW.nosOn = previous != 0;
      else if (slot == 12) f.mMW.engineOn = previous != 0;
      else f.mMW.shifting = previous != 0;
      MWRamp* ramps[] = {&f.mMW.collision,NULL,&f.mMW.potentialLeft,&f.mMW.potentialRight,
        &f.mMW.nos,NULL,&f.mMW.engineLeft,&f.mMW.engineRight,&f.mMW.shiftLeft,&f.mMW.shiftRight,
        &f.mMW.bumpLeft,&f.mMW.bumpRight};
      for (auto* ramp : ramps) {
        MWRamp seed; file >> seed.remaining >> seed.duration >> seed.amplitude >> seed.floor;
        if (ramp) *ramp = seed;
      }
      if (slot == 10) RumbleVCall<void>(&f,0x28,(ShiftPotential)value);
      else if (slot == 11) RumbleVCall<void>(&f,0x2C,value != 0,1.0f);
      else RumbleVCall<void>(&f,slot*4,value != 0);
      for (auto* ramp : ramps) {
        MWRamp target; file >> target.remaining >> target.duration >> target.amplitude >> target.floor;
        MWRamp current = ramp ? *ramp : MWRamp{};
        if (!(Near(current.remaining,target.remaining) && Near(current.duration,target.duration) &&
              Near(current.amplitude,target.amplitude) && Near(current.floor,target.floor))) {
          printf("Car ramp reference mismatch, case %d slot %d\n",cases,slot); return 1;
        }
      }
      int cached; file >> cached;
      actual = (float)(slot == 10 ? f.mMW.potential : slot == 11 ? f.mMW.nosOn : slot == 12 ? f.mMW.engineOn : f.mMW.shifting);
      expected = (float)cached;
    } else if (kind == 'R') {
      file >> x >> expected; RumbleVCall<void>(&f,0x18,true,(SimSurface*)surface,x);
      actual = f.mRoadWave.fFrequency[0];
    } else if (kind == 'B') {
      unsigned int wheel; float remaining,duration,amplitude,floor;
      file >> wheel >> x >> bumpThreshold >> bumpGain >> remaining >> duration >> amplitude >> floor;
      f.UpdateRoadNoise(wheel,x,*(SimSurface*)surface);
      MWRamp& ramp = (wheel & 1) ? f.mMW.bumpRight : f.mMW.bumpLeft;
      if (!(Near(ramp.remaining,remaining) && Near(ramp.duration,duration) && Near(ramp.floor,floor))) {
        printf("Bump reference mismatch, case %d wheel %u signal %g threshold %g gain %g: {%g,%g,%g,%g} vs {%g,%g,%g,%g}\n",
          cases,wheel,x,bumpThreshold,bumpGain,ramp.remaining,ramp.duration,ramp.amplitude,ramp.floor,remaining,duration,amplitude,floor);
        return 1;
      }
      actual = ramp.amplitude; expected = amplitude;
    } else if (kind == 'M') {
      float dt; file >> dt;
      RumbleWave* waves[] = {&f.mRoadWave,&f.mSkidWave,&f.mSlipWave,NULL,NULL,NULL};
      for (int group=0; group<6; ++group) for (int ch=0; ch<4; ++ch) {
        float phase,amp,freq,offset; file >> phase >> amp >> freq >> offset;
        if (waves[group]) {
          waves[group]->fPhase[ch]=phase; waves[group]->fAmplitude[ch]=amp;
          waves[group]->fFrequency[ch]=freq; waves[group]->fOffset[ch]=offset;
        }
      }
      MWRamp* ramps[] = {&f.mMW.collision,NULL,&f.mMW.potentialLeft,&f.mMW.potentialRight,
        &f.mMW.nos,NULL,&f.mMW.engineLeft,&f.mMW.engineRight,&f.mMW.shiftLeft,&f.mMW.shiftRight,
        &f.mMW.bumpLeft,&f.mMW.bumpRight};
      for (auto* ramp : ramps) {
        MWRamp value; file >> value.remaining >> value.duration >> value.amplitude >> value.floor;
        if (ramp) *ramp = value;
      }
      float left,right; file >> left >> right;
      f.mMixer.Begin(); f.mMW.Mix(dt,f.mMixer); f.MixTyres(dt);
      int actualLeft,actualRight; f.mMixer.Resolve(actualLeft,actualRight);
      assert(abs(actualLeft-RumbleMixer::ToMotor(left,1)) <= 2);
      assert(abs(actualRight-RumbleMixer::ToMotor(right,1)) <= 2);
    } else {
      file >> x >> y;
      if (kind == 'S') f.UpdateTireSlip(*(SimSurface*)surface,x,y);
      else if (kind == 'D') f.UpdateDriftSlip(*(SimSurface*)surface,x,y);
      else {
        file >> z;
        if (kind == 'K') { file >> load; f.UpdateTireSkid(*(SimSurface*)surface,x,y,z,load); }
        else { assert(kind == 'F'); f.UpdateDriftSkid(*(SimSurface*)surface,x,y,z); }
      }
      file >> expected;
      actual = (kind == 'S' || kind == 'D') ? f.mSlipWave.fFrequency[0] : f.mSkidWave.fFrequency[0];
    }
    assert(file.good() || file.eof());
    if (!Near(actual,expected)) {
      printf("Console reference mismatch, case %d (%c): %.9g vs %.9g\n",cases,kind,actual,expected);
      return 1;
    }
    ++cases;
  }
  printf("%d original Xbox 360 callback/mixer cases matched the compiled ProStreet adapter.\n",cases);
  return cases ? 0 : 1;
}

void TestCarEffects(IFeedback& f) {
  f.ClearEffects(); f.ResumeEffects();
  f.UpdateShiftPotential(SHIFT_POTENTIAL_PERFECT);
  f.UpdateShifting(true); f.UpdateEngineBlown(true);
  f.mMixer.Begin(); f.mMW.Mix(0.01f,f.mMixer);
  int left,right; f.mMixer.Resolve(left,right);
  assert(left == 0 && right == 0); // console samples these ramps but discards the result
  assert(Near(f.mMW.shiftRight.remaining,0.49f));
  f.UpdateNOS(true,1);
  f.mMixer.Begin(); f.mMW.Mix(1,f.mMixer); f.mMixer.Resolve(left,right);
  assert(left == RumbleMixer::ToMotor(0.75f,1) && right == 0);
  f.UpdateNOS(true,1); assert(Near(f.mMW.nos.remaining,1));
  f.UpdateNOS(false,0); assert(f.mMW.nos.remaining == 0);
  f.ClearEffects();
}

void TestCollisionRules(IFeedback& f) {
  COLLISION_INFO hit = {}; hit.flags = 2; hit.objA = (void*)0x1234;
  hit.normal[0] = 1; hit.closingVel[0] = 100;
  hit.objAVel[0] = velocity[0] = 30;
  f.ClearEffects(); f.ReportCollision(hit,true,{100,0,0});
  assert(f.mMW.collision.amplitude == 0); // no closing-normal or by-value-vector fallback
  hit.flags = 3; hit.objAVel[0] = 27.5f;
  f.ReportCollision(hit,true,{}); assert(Near(f.mMW.collision.amplitude,0.5f));
  f.ClearEffects(); hit.flags = 1; hit.objB = (void*)0x5678;
  hit.objAVel[0] = 30; hit.objBVel[0] = 25;
  f.ReportCollision(hit,true,{}); assert(Near(f.mMW.collision.amplitude,0.125f));
  f.ClearEffects(); hit.objA = (void*)0x5678; hit.objB = (void*)0x1234;
  velocity[0] = 25; otherVelocity[0] = 30;
  f.ReportCollision(hit,false,{}); assert(Near(f.mMW.collision.amplitude,0.125f));
  f.ClearEffects(); hit.objA = (void*)0x1234; hit.objB = (void*)0x5678;
  hit.objAVel[0] = velocity[0] = 40; hit.objBVel[0] = otherVelocity[0] = 0;
  hit.objBVel[1] = otherVelocity[1] = 10;
  f.ReportCollision(hit,true,{}); assert(Near(f.mMW.collision.amplitude,0.5f));
  velocity[0] = 30; otherVelocity[0] = 25; otherVelocity[1] = 0;
  f.ClearEffects();
}

void TestConsoleInputs(IFeedback& f, const SimSurface& surface) {
  assert(Near(RumblePSSlip(-0.8f,0.2f),1));
  assert(Near(RumblePSSlip(-0.75f,0.2f),0));
  assert(Near(RumblePSSlip(0.2f,0.2f),0));
  assert(Near(RumblePSSlip(1.1f,0.2f),0.25f));
  assert(Near(RumblePSSkid(0.2f,0.1f,0.2f,0.5f),0.5f));
  assert(Near(RumblePSSkid(0.25f,0.1f,0.2f,0.5f),0.25f));
  assert(Near(RumblePSSkid(3.14159f,0.1f,0.2f,0.5f),0));
  assert(Near(RumblePSDriftSlip(0.2f,0.1f),0.2f));
  assert(Near(RumblePSDriftSlip(0.3f,0.1f),0)); // exact peak boundary has a retail discontinuity
  assert(Near(RumblePSDriftSkid(0.2f,0.1f,0.2f),0.8f));
  assert(Near(RumblePSDriftSkid(0.96f,0.1f,0.2f),1));
  assert(RumblePSMap(1,1,1) == 0 && RumblePSMap(1,2,1) == 0);

  f.ClearEffects(); f.ResumeEffects(); f.BeginUpdate();
  RumbleVCall<void>(&f,0x48,1u,0.5f,&surface);
  assert(f.mRoadWave.fFrequency[0] == 0); // no speed-based roll substitution
  assert(Near(f.mMW.bumpRight.amplitude,0.4f) && f.mMW.bumpLeft.remaining == 0);
  assert(Near(f.mMW.bumpRight.Sample(0.15f),0.25f)); // 300ms, floor .1
  RumbleVCall<void>(&f,0x48,3u,0.3f,&surface);
  assert(Near(f.mMW.bumpRight.amplitude,0.24f) && Near(f.mMW.bumpRight.remaining,0.3f));
  RumbleVCall<void>(&f,0x48,0u,0.25f,&surface);
  assert(f.mMW.bumpLeft.remaining == 0); // strict threshold
  RumbleVCall<void>(&f,0x48,2u,0.5f,&surface);
  assert(Near(f.mMW.bumpLeft.amplitude,0.4f));
  f.mMW.bumpLeft = {}; f.mMW.bumpRight = {};
  RumbleVCall<void>(&f,0x48,4u,10.0f,&surface);
  assert(f.mMW.bumpLeft.remaining == 0 && f.mMW.bumpRight.remaining == 0);
  RumbleVCall<void>(&f,0x40,&surface,0.2f,0.1f,0.2f);
  assert(lastWaveKey == ATTRIB_KEY_FFB_DRIFT_SKID);
  RumbleVCall<void>(&f,0x3C,&surface,0.2f,0.1f);
  assert(lastWaveKey == ATTRIB_KEY_FFB_SLIP);
  f.ClearEffects();
}

void TestTiming(IFeedback& f, const SimSurface& surface) {
  f.ClearEffects(); f.ResumeEffects(); f.BeginUpdate();
  f.UpdateRoadNoise(1,0.5f,surface);
  Tick(f);
  assert(sent[0].wLeftMotorSpeed == 0 && sent[0].wRightMotorSpeed == 0);
  int pending = f.mMW.outputRight;
  assert(pending > 0 && f.mMW.outputLeft == 0);
  Tick(f); assert(sent[0].wRightMotorSpeed == pending);
  float age = f.mMW.bumpRight.remaining;
  f.PauseEffects(); assert(!sent[0].wRightMotorSpeed);
  fixedTime += 0.1; f.PollEffects(); assert(f.mMW.bumpRight.remaining == age);
  f.ResumeEffects(); assert(sent[0].wRightMotorSpeed > 0);
  bRumbleAppActive = false; f.PollEffects(); assert(!sent[0].wRightMotorSpeed);
  bRumbleAppActive = true;
  f.ClearEffects(); f.ResumeEffects();
  f.UpdateNOS(true,1);
  Tick(f); Tick(f);
  float nosAge = f.mMW.nos.remaining;
  // No new EndUpdate or feedback is required for a native ramp to keep aging.
  fixedTime += 0.3; f.PollEffects();
  assert(sent[0].wLeftMotorSpeed > 0);
  assert(Near(f.mMW.nos.remaining,nosAge-0.3f));
  // A long poll gap consumes its actual duration; no synthetic delta cap.
  fixedTime += 1; f.PollEffects();
  assert(Near(f.mMW.nos.remaining,nosAge-1.3f));
  f.ClearEffects();
}

void TestRemainingSlots(IFeedback& f, const SimSurface& surface) {
  f.ClearEffects(); f.ResumeEffects();
  f.mRoadWave.fPhase[0] = 0.125f;
  assert(RumbleVCall<void*>(&f,0x00,0u) == &f); // embedded interface must not delete its owner
  RumbleVCall<void>(&f,0x10);
  RumbleVCall<void>(&f,0x18,true,&surface,50.0f); // slot 6 is implemented even though DoFFB uses 18
  assert(f.mRoadWave.fFrequency[0] > 0);
  f.mMW.collision.Start(0.5f,0,0.5f);
  RumbleVCall<void>(&f,0x24,0.4f,0.9f,0.7f); // RPM: telemetry only
  RumbleVCall<void>(&f,0x38,(uintptr_t)123); // reserved slot 14
  RumbleVCall<void>(&f,0x44,0.5f); // steering: native controller no-op
  assert(f.mMW.collision.amplitude == 0.5f);
  RumbleVCall<void>(&f,0x14);
  assert(f.mMW.collision.amplitude == 0.5f); // native EndUpdate is a no-op
  RumbleVCall<void>(&f,0x0C); // Reset clears collision/output, pauses, preserves surface phases
  assert(f.mPaused && f.mMW.collision.amplitude == 0);
  assert(f.mRoadWave.fPhase[0] == 0.125f);
  assert(RumbleVCall<int>(&f,0x50) == 5); // PC-only device query, not an Xbox effect slot
  f.ClearEffects(); f.ResumeEffects();
}

void TestTracingPreservesMotors(IFeedback& f, const SimSurface& surface) {
  int left[2],right[2];
  for (int tracing=0; tracing<2; ++tracing) {
    bRumbleTelemetry = tracing != 0;
    fixedTime = 50;
    f.Init(NULL,0); f.ResumeEffects(); f.BeginUpdate();
    f.UpdateRoadNoise(1,0.6f,surface);
    f.UpdateTireSlip(surface,-0.8f,0.2f);
    f.UpdateTireSkid(surface,0.25f,0.1f,0.2f,0.5f);
    f.UpdateDriftSkid(surface,0.4f,0.1f,0.2f);
    Tick(f); Tick(f);
    left[tracing] = f.mLastLeft; right[tracing] = f.mLastRight;
  }
  assert(left[0] == left[1] && right[0] == right[1]);
  assert(left[0] > 0 && right[0] > 0);
  f.ClearEffects();
}

void MapFixture(const char* path) {
  if (!path) return;
  std::ifstream stream(path, std::ios::binary);
  assert(stream.good());
  std::vector<char> data((std::istreambuf_iterator<char>(stream)), {});
  auto* dos = (IMAGE_DOS_HEADER*)data.data();
  auto* nt = (IMAGE_NT_HEADERS*)(data.data()+dos->e_lfanew);
  auto* section = IMAGE_FIRST_SECTION(nt);
  for (int i=0; i<nt->FileHeader.NumberOfSections; ++i) {
    // Only native code/data below 0x1400000 is needed for these checks.
    auto& s = section[i];
    if (s.VirtualAddress >= 0x1000000) continue;
    size_t count = (std::min)((size_t)s.SizeOfRawData, (size_t)(0x1000000-s.VirtualAddress));
    if (s.VirtualAddress < 0x1000000 && s.PointerToRawData+count <= data.size())
      memcpy(gameArena+s.VirtualAddress, data.data()+s.PointerToRawData, count);
  }
  assert(RumbleValidateProStreet()); // checks actual native bytes before mocks
  *(unsigned int*)RumblePSAddress(0x98BE6C) ^= 1;
  assert(!RumbleValidateProStreet()); // one changed slot must fail closed
  *(unsigned int*)RumblePSAddress(0x98BE6C) ^= 1;
}

int main(int argc, char** argv) {
  if (argc == 3 && !strcmp(argv[1],"--console-reference")) return CheckConsoleReference(argv[2]);
  FILE* writer = RumbleOpenLog("build/tests/shared-telemetry.log");
  assert(writer); fputs("live log",writer); fflush(writer);
  FILE* reader = _fsopen("build/tests/shared-telemetry.log","r",_SH_DENYNO);
  assert(reader); char sharedLog[16] = {}; fread(sharedLog,1,8,reader);
  assert(!strcmp(sharedLog,"live log")); fclose(reader); fclose(writer);
  MapFixture(argc > 1 ? argv[1] : NULL);
  *(int*)RumblePSAddress(GAMEFLOWMANAGER_STATUS_ADDR) = 3; // no race: vehicle reader must safely decline
  RumbleTriggerMode = kTriggersOff;
  UTL_Com_Object_IList_Constructor = (decltype(UTL_Com_Object_IList_Constructor))TestListCtor;
  UTL_Com_Object_IList_Add = (decltype(UTL_Com_Object_IList_Add))TestListAdd;
  InitProfileSettings = TestProfile;
  Attrib_Instance_GetAttributePointer = (decltype(Attrib_Instance_GetAttributePointer))TestAttribute;
  TestVehicle();
  simableTable[26] = CollisionVelocity;
  RumbleMWResolveSimable = Resolve;
  g_Controllers[0].bConnected = true;

  InputDevice device(0);
  assert(registered == &device.mFFB);
  assert((char*)registered-(char*)&device == 0x2C);
  assert(RumbleVCall<int>(&device, 0x24) == (int)registered); // native GetInterfaces slot
  assert(RumbleVCall<bool>(&device, 0x18)); // PS's extra capability slot
  bRumbleEnabled = false;
  assert(RumbleVCall<int>(&device, 0x24) == 0);
  bRumbleEnabled = true;

  IFeedback& f = device.mFFB;
  void* surface[4] = {(void*)0x1234}; // PS collection at +0; test reader supplies its records
  expectedSurface = surface;
  assert(fopen_s(&RumbleTelemetryFile, "build/tests/prostreet-telemetry.log", "w+") == 0);
  bRumbleTelemetry = true;
  RumbleVCall<void>(&f, 0x08); // Resume
  RumbleVCall<void>(&f, 0x10); // Begin
  f.mHasVehicle = true; f.mVehicle.fSpeed = 50;
  ReplayNativeRoad(&f, 2, surface); // native pushes wheel, road signal, surface
  RumbleVCall<void>(&f, 0x20, (void*)surface, 12.0f, 1.0f); // surface + two floats
  RumbleVCall<void>(&f, 0x1C, (void*)surface, 15.0f, 1.0f, 1.0f, 4.0f); // five arguments
  assert(f.mWheelSkid[2] == 15 && f.mWheelSkid[0] == 0);
  assert(RumbleTelemetry.fSlip[2] == 12);
  assert(f.mSlipWave.fFrequency[0] > 0); // body slip uses the live surface record
  RumbleWave expectedSlip = f.mSlipWave;
  f.mSkidWave.Begin();
  float nativeLeft, nativeRight;
  expectedSlip.Sample(0.016f, nativeLeft, nativeRight);
  f.mMixer.Begin(); f.MixTyres(0.016f);
  int actualLeft, actualRight; f.mMixer.Resolve(actualLeft, actualRight);
  assert(actualLeft == RumbleMixer::ToMotor(nativeLeft,1) && actualRight == RumbleMixer::ToMotor(nativeRight,1));
  assert(Near(f.mMW.bumpLeft.amplitude,0.4f)); // native scalar drives an indexed ramp
  RumbleVCall<void>(&f, 0x40, (void*)surface, 18.0f, 1.0f, 1.0f); // drift skid
  assert(f.mWheelSkid[2] == 18);
  RumbleVCall<void>(&f, 0x2C, true, 1.0f); // NOS
  RumbleVCall<void>(&f, 0x14); // End
  Tick(f); Tick(f);
  assert(!f.mPaused && sends > 0 && (sent[0].wLeftMotorSpeed || sent[0].wRightMotorSpeed));
  RumbleVCall<void>(&device, 0x20); // native StopVibration at slot 8
  assert(sent[0].wLeftMotorSpeed == 0 && sent[0].wRightMotorSpeed == 0);
  RumbleVCall<void>(&f, 0x08);

  COLLISION_INFO hit = {};
  hit.flags = 2; hit.normal[0] = 1; hit.closingVel[0] = 20; hit.objA = (void*)0x1234;
  RumblePSVector delta = {20, 0, 0};
  RumbleVCall<void>(&f, 0x4C, &hit, true, delta); // 20 stack bytes, including by-value vector
  assert(f.mMW.collision.amplitude > 0);
  assert(RumbleVCall<int>(&f, 0x50) == 5); // final slot returns an int
  TestCollisionListener(f);
  TestPSTyreUnits(f,surface);
  TestCarEffects(f);
  TestCollisionRules(f);
  TestConsoleInputs(f,*(SimSurface*)surface);
  TestTiming(f,*(SimSurface*)surface);
  TestRemainingSlots(f,*(SimSurface*)surface);
  TestTracingPreservesMotors(f,*(SimSurface*)surface);
  f.ResumeEffects();

  record.fAmplitudeMin = (std::numeric_limits<float>::quiet_NaN)();
  f.mHasVehicle = true; f.mVehicle.fSpeed = 50;
  f.BeginUpdate(); f.UpdateTireSlip(*(SimSurface*)surface,1.5f,0.2f);
  assert(f.mSlipWave.fFrequency[0] == 0); // malformed live records cannot poison output
  bumpGain = (std::numeric_limits<float>::quiet_NaN)();
  f.UpdateRoadNoise(0,0.5f,*(SimSurface*)surface);
  assert(f.mMW.bumpLeft.remaining == 0);
  assert(!RumbleGetAttributePointer(NULL, ATTRIB_KEY_FFB_ROLL, 0));
  assert(RumbleTelemetrySurfaceId(NULL) == -1);
  assert(RumbleTelemetrySurfaceId((void*)1) == -1);
  RumbleTelemetryFlush(); rewind(RumbleTelemetryFile);
  char log[32768] = {}; fread(log,1,sizeof(log)-1,RumbleTelemetryFile);
  assert(strstr(log,"PS-live") && strstr(log,"T,") && strstr(log,"C,"));
  assert(strstr(log,"E,") && strstr(log,"M,"));
  fclose(RumbleTelemetryFile); RumbleTelemetryFile = NULL; bRumbleTelemetry = false;
  f.Detach(); assert(RumblePSDevices[0] == NULL);
  delete[] device.fDeviceScalar;
  puts("ProStreet Xbox callback transforms, stereo bumps, collision routing, ABI and lifecycle checks passed.");
}
