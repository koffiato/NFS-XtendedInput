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
const void* __fastcall TestAttribute(const void* surface, void*, unsigned int, unsigned int index) {
  assert(surface == expectedSurface); // also detects a float being misread as a surface
  return index == 0 ? &record : NULL;
}

struct NativeInterface { void** vtable; void* owner; void* handle; };
void* playerTable[16] = {}, *bodyTable[12] = {}, *suspensionTable[32] = {}, *inputTable[9] = {}, *transmissionTable[6] = {};
NativeInterface simable = {}, player = {playerTable}, body = {bodyTable}, suspension = {suspensionTable},
                input = {inputTable}, transmission = {transmissionTable};
float velocity[3] = {30,0,0}, angular[3] = {0,0,1};
void* __fastcall GetSimable(void*,void*) { return &simable; }
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
bool __fastcall Shifting(void*,void*) { return true; }
void* __fastcall Find(void*,void*,void* handle) {
  if (handle == (void*)IRIGIDBODY_IHANDLE_ADDR) return &body;
  if (handle == (void*)ISUSPENSION_IHANDLE_ADDR) return &suspension;
  if (handle == (void*)IINPUT_IHANDLE_ADDR) return &input;
  if (handle == (void*)ITRANSMISSION_IHANDLE_ADDR) return &transmission;
  return NULL;
}

void TestVehicle() {
  playerTable[1] = GetSimable;
  bodyTable[9] = Linear; bodyTable[10] = Angular; bodyTable[11] = Speed;
  inputTable[2] = Brake; inputTable[3] = Gas; inputTable[8] = Handbrake;
  suspensionTable[2] = Skid; suspensionTable[3] = Slip; suspensionTable[5] = Wheels;
  suspensionTable[13] = Ground; suspensionTable[31] = NativeRoad;
  transmissionTable[5] = Shifting;
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
  assert(s.fLinVel[0] == 30 && s.fAngVel[2] == 1 && RumblePSIsShifting());
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
  assert(f.mPulseLeft.GetValue() > 0 && f.mPulseRight.GetValue() > 0);
  f.EndUpdate();
  assert(sent[0].wLeftMotorSpeed > 0 && sent[0].wRightMotorSpeed > 0);
  assert(originalCollisions == 0);
  // The reversed participant order works too.
  hit.objA = simable.handle; hit.objB = NULL; f.ClearEffects();
  RumblePSCollisionHook(listener, NULL, hit);
  assert(f.mPulseRight.GetValue() > 0);
  hit.objA = (void*)0x999; f.ClearEffects();
  RumblePSCollisionHook(listener, NULL, hit);
  assert(f.mPulseRight.GetValue() == 0); // stale/unrelated contact
  hit.objA = simable.handle; f.PauseEffects();
  RumblePSCollisionHook(listener, NULL, hit);
  assert(f.mPulseRight.GetValue() == 0);
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
  f.mVehicle.fSpeed = 0;
  for (int i=0;i<4;++i) f.mWheelSkid[i] = 3.14159f;
  f.mSkidWave.Begin(); f.mSkidWave.Add(surface,ATTRIB_KEY_FFB_SKID,3.14f);
  f.mMixer.Begin(); f.MixTyres(0.016f);
  int left,right; f.mMixer.Resolve(left,right);
  assert(f.mSlideLevel == 0 && left == 0 && right == 0); // pi at rest must not buzz
  f.ClearEffects();
}

void TestCarEffects(IFeedback& f) {
  f.ClearEffects();
  f.mPrevNOSEngaged = f.mPrevShifting = f.mPrevEngineBlown = false;
  f.mShiftPotential = SHIFT_POTENTIAL_PERFECT;
  f.UpdateCarEffects();
  assert(f.mShiftTick.GetValue() == 0); // potential alone adds no fabricated cue
  f.mShifting = true; f.mNOSEngaged = true;
  f.UpdateCarEffects();
  f.mShiftTick.Advance(0.25f); f.mNOSKick.Advance(1.0f);
  assert(fabsf(f.mShiftTick.GetValue()-0.4f) < 0.00001f);
  assert(fabsf(f.mNOSKick.GetValue()-0.75f) < 0.00001f);
  f.UpdateCarEffects(); // held inputs must not restart either ramp
  f.mShiftTick.Advance(0.25f); f.mNOSKick.Advance(1.0f);
  assert(f.mShiftTick.GetValue() == 0 && f.mNOSKick.GetValue() == 0);
  f.mShifting = false; f.mNOSEngaged = false; f.UpdateCarEffects();
  bRumbleGearShift = false;
  f.mShifting = true; f.UpdateCarEffects(); assert(f.mShiftTick.GetValue() == 0);
  bRumbleGearShift = true;
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
  assert(f.mRoadWave.fFrequency[0] == 7.5f); // live speed=50; native road signal=.5 is not speed
  RumbleVCall<void>(&f, 0x40, (void*)surface, 18.0f, 1.0f, 1.0f); // drift skid
  assert(f.mWheelSkid[2] == 18);
  RumbleVCall<void>(&f, 0x2C, true, 1.0f); // NOS
  RumbleVCall<void>(&f, 0x14); // End
  assert(!f.mPaused && sends > 0 && (sent[0].wLeftMotorSpeed || sent[0].wRightMotorSpeed));
  RumbleVCall<void>(&device, 0x20); // native StopVibration at slot 8
  assert(f.mPaused && sent[0].wLeftMotorSpeed == 0 && sent[0].wRightMotorSpeed == 0);
  RumbleVCall<void>(&f, 0x08);

  COLLISION_INFO hit = {};
  hit.flags = 2; hit.normal[0] = 1; hit.closingVel[0] = 20;
  RumblePSVector delta = {20, 0, 0};
  RumbleVCall<void>(&f, 0x4C, &hit, true, delta); // 20 stack bytes, including by-value vector
  assert(f.mPulseLeft.GetValue() > 0 && f.mPulseRight.GetValue() > 0);
  assert(RumbleVCall<int>(&f, 0x50) == 5); // final slot returns an int
  TestCollisionListener(f);
  TestPSTyreUnits(f,surface);
  TestCarEffects(f);
  f.ResumeEffects();
  f.Send(30000, 20000);
  bRumbleAppActive = false; f.Watchdog();
  assert(f.mPaused && !sent[0].wLeftMotorSpeed && !sent[0].wRightMotorSpeed);
  bRumbleAppActive = true;
  f.ResumeEffects(); f.Send(30000,20000); f.mLastUpdateTime = RumbleGetTime()-1;
  f.Watchdog();
  assert(f.mPaused && !sent[0].wLeftMotorSpeed && !sent[0].wRightMotorSpeed);

  record.fAmplitudeMin = (std::numeric_limits<float>::quiet_NaN)();
  f.mHasVehicle = true; f.mVehicle.fSpeed = 50;
  f.BeginUpdate(); f.UpdateRoadNoise(0, 0.5f, *(SimSurface*)surface);
  assert(f.mRoadWave.fFrequency[0] == 0); // malformed live records cannot poison output
  assert(!RumbleGetAttributePointer(NULL, ATTRIB_KEY_FFB_ROLL, 0));
  assert(RumbleTelemetrySurfaceId(NULL) == -1);
  assert(RumbleTelemetrySurfaceId((void*)1) == -1);
  RumbleTelemetryFlush(); rewind(RumbleTelemetryFile);
  char log[8192] = {}; fread(log,1,sizeof(log)-1,RumbleTelemetryFile);
  assert(strstr(log,"PS-live") && strstr(log,"T,") && strstr(log,"C,"));
  fclose(RumbleTelemetryFile); RumbleTelemetryFile = NULL; bRumbleTelemetry = false;
  f.Detach(); assert(RumblePSDevices[0] == NULL);
  delete[] device.fDeviceScalar;
  puts("ProStreet callback ABI, live curves, routing and lifecycle checks passed.");
}
