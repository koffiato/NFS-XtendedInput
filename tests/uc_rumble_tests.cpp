// Recovered Undercover controller feedback and native interface wiring tests.
#define NOMINMAX
#ifndef GAME_UC
#define GAME_UC
#endif
#define RUMBLE_TESTING
#include <Windows.h>
#include <XInput.h>
#include <cassert>
#include <cmath>
#include <cstdio>

double fixedTime = 10.0;
float simTime = 2.5f;
XINPUT_VIBRATION sent = {};
DWORD outputResult = ERROR_SUCCESS;

BOOL WINAPI TestQPC(LARGE_INTEGER* p) {
  p->QuadPart = (LONGLONG)(fixedTime * 1000000.0 + 0.5);
  return TRUE;
}
BOOL WINAPI TestQPF(LARGE_INTEGER* p) {
  p->QuadPart = 1000000;
  return TRUE;
}
DWORD WINAPI TestSetState(DWORD, XINPUT_VIBRATION* value) {
  sent = *value;
  return outputResult;
}

#define QueryPerformanceCounter TestQPC
#define QueryPerformanceFrequency TestQPF
#define XInputSetState TestSetState
#include "../src/Main.hpp"

bool Near(float a, float b, float eps = 0.0001f) {
  return fabsf(a - b) < eps;
}

struct Body {
  void** table;
  void* interfaces;
  float velocity[3];
  bool vehicle;
} bodies[2];

void* bodyTable[27] = {};
void __fastcall Velocity(Body* body, void*, float* out) {
  memcpy(out, body->velocity, 12);
}
void* __cdecl Resolve(void* handle) {
  return handle == (void*)1 ? &bodies[0] : handle == (void*)2 ? &bodies[1] : NULL;
}
void* __fastcall Find(void* list, void*, void* handle) {
  return handle == (void*)IVEHICLE_IHANDLE_ADDR && ((Body*)list)->vehicle ? list : NULL;
}

RumbleWaveRecord record = {2.0f, 0.6f, 0.3f, 0.0f, 2.0f, 0.6f, 0.3f, 20.0f};
unsigned int lastKey = 0;
int instances = 0;
bool collectionPresent = true;
bool collisionStereo = false;
float bumpThreshold = 0.2f, bumpGain = 0.5f;
RumbleWaveRecord collisionRecord = {0, 0, 0, 0, 0, 1, 0, 200};
const void* __fastcall Attribute(const void*, void*, unsigned int key, unsigned int index) {
  lastKey = key;
  if (key == ATTRIB_KEY_FFB_BUMP_THRESHOLD) return &bumpThreshold;
  if (key == ATTRIB_KEY_FFB_BUMP_GAIN) return &bumpGain;
  if (key == 0x751D40E5 || key == 0x96A192E8 || key == 0x7125C7EC ||
      key == 0xAEF4FB7D || key == 0x1C2333F7 || key == 0x3C714126)
    return (index == 0 || (collisionStereo && index == 2)) ? &collisionRecord : NULL;
  return index == 0 ? &record : NULL;
}
void* __cdecl FindCollection(unsigned int cls, unsigned int name) {
  assert(cls == 0xD93FC53A && name == 0xEEC2271A);
  return collectionPresent ? &collisionRecord : NULL;
}
void* __fastcall ConstructInstance(void* instance, void*, void* collection, unsigned int flags) {
  assert(collection == &collisionRecord && flags == 0);
  ++instances;
  return instance;
}
void __fastcall DestroyInstance(void*, void*) { --instances; }


void Reset(IFeedback& f) {
  fixedTime = 10.0;
  simTime = 2.5f;
  bRumbleAppActive = bRumbleEnabled = true;
  bRumbleTelemetry = false;
  RumbleTriggerMode = kTriggersOff;
  outputResult = ERROR_SUCCESS;
  g_Controllers[0].bConnected = true;
  bodies[0] = {bodyTable, &bodies[0], {}, true};
  bodies[1] = {bodyTable, &bodies[1], {}, true};
  f.Init(NULL, 0);
  f.ResumeEffects();
  sent = {};
}

void Tick(IFeedback& f, double dt = 0.01) {
  fixedTime += dt;
  f.EndUpdate();
  f.PollEffects();
}

COLLISION_INFO Contact(int flags = 2) {
  COLLISION_INFO hit = {};
  hit.flags = flags;
  hit.objA = (void*)1;
  hit.objB = (void*)2;
  hit.normal[0] = 1.0f;
  hit.closingVel[0] = 15.0f;
  return hit;
}

// Reproduce the native stack layout independently of the C++ method signature.
// Restore ESP even for the broken adapter so a regression reports the missing
// cleanup instead of corrupting the test's own return address.
__declspec(naked) int __cdecl CollisionStackDelta(IFeedback*, const COLLISION_INFO*, bool) {
  __asm {
    push ebp
    mov ebp, esp
    push ebx
    mov ebx, esp
    mov ecx, [ebp + 8]
    mov edx, [ecx]
    push 0x40400000  // velocityChange.z = 3
    push 0x40000000  // velocityChange.y = 2
    push 0x3F800000  // velocityChange.x = 1
    push [ebp + 16] // iamA
    push [ebp + 12] // CollisionInfo*
    call dword ptr [edx + 0x4C]
    mov eax, esp
    sub eax, ebx
    mov esp, ebx
    pop ebx
    pop ebp
    ret
  }
}

void* deviceList = NULL;
void* deviceFeedback = NULL;
void* __fastcall ConstructList(void* list, void*, unsigned int capacity) {
  assert(capacity == 4);
  deviceList = list;
  deviceFeedback = NULL;
  return list;
}
void __fastcall AddFeedback(void* list, void*, void* handle, void* iface) {
  assert(list == deviceList && handle == (void*)IFEEDBACK_IHANDLE_ADDR);
  deviceFeedback = iface;
}
void* __fastcall FindFeedback(void* list, void*, void* handle) {
  assert(list == deviceList && handle == (void*)IFEEDBACK_IHANDLE_ADDR);
  return deviceFeedback;
}
void InitTestProfile() {}

void CheckDeviceFeedback() {
  auto originalConstruct = UTL_Com_Object_IList_Constructor;
  auto originalAdd = UTL_Com_Object_IList_Add;
  auto originalFind = UTL_Com_Object_IList_Find;
  auto originalProfile = InitProfileSettings;
  UTL_Com_Object_IList_Constructor = (decltype(originalConstruct))ConstructList;
  UTL_Com_Object_IList_Add = (decltype(originalAdd))AddFeedback;
  UTL_Com_Object_IList_Find = (decltype(originalFind))FindFeedback;
  InitProfileSettings = InitTestProfile;
  RumbleTriggerMode = kTriggersOff;
  bRumbleTelemetry = false;
  bRumbleAppActive = true;
  g_Controllers[0].bConnected = true;
  bXInputOmniMode = false;
  bGlobalDoPolling = false;

  for (int enabled = 0; enabled <= 1; ++enabled) {
    bRumbleEnabled = enabled != 0;
    InputDevice device(0);
    void** table = *(void***)&device;

    // Slot 9 is GetInterfaces (+0x24 in vtable)
    auto getInterfaces = (void*(__thiscall*)(InputDevice*))table[9];
    void* iface = getInterfaces(&device);
    if (!enabled) {
      assert(!iface && !deviceFeedback);
    } else {
      assert(iface == &device.mFFB);
      IFeedback* feedback = (IFeedback*)RumbleFindInterface(iface, IFEEDBACK_IHANDLE_ADDR);
      assert(feedback == &device.mFFB);
      assert(feedback->mOwnerList == deviceList);

      void** feedbackTable = *(void***)feedback;
      // Slot 2: ResumeEffects
      ((void(__thiscall*)(IFeedback*))feedbackTable[2])(feedback);
      // Slot 4: BeginUpdate
      ((void(__thiscall*)(IFeedback*))feedbackTable[4])(feedback);
      // Slot 11: UpdateNOS
      ((void(__thiscall*)(IFeedback*, bool, float))feedbackTable[11])(feedback, true, 1.0f);
      auto endUpdate = (void(__thiscall*)(IFeedback*))feedbackTable[5];
      auto poll = (void(__thiscall*)(InputDevice*))table[4];
      sent = {};
      for (int tick = 0; tick < 2; ++tick) {
        fixedTime += 0.01;
        endUpdate(feedback);
        poll(&device);
      }
      assert(sent.wLeftMotorSpeed > 60000 && sent.wRightMotorSpeed == 0);
      // Slot 8: StopVibration
      ((void(__thiscall*)(InputDevice*))table[8])(&device);
      assert(!sent.wLeftMotorSpeed && !sent.wRightMotorSpeed);
    }
    device.mFFB.Detach();
    delete[] device.fDeviceScalar;
  }
  bGlobalDoPolling = true;
  UTL_Com_Object_IList_Constructor = originalConstruct;
  UTL_Com_Object_IList_Add = originalAdd;
  UTL_Com_Object_IList_Find = originalFind;
  InitProfileSettings = originalProfile;
}

struct VehicleInterface { void** table; void* owner; };
void* chassisTable[10] = {};
void* rigidBodyTable[12] = {};
void* inputTable[10] = {};
void* vehicleTable[50] = {};
VehicleInterface chassis = {chassisTable}, rigidBody = {rigidBodyTable};
VehicleInterface vehicleInput = {inputTable}, vehicle = {vehicleTable};
VehicleInterface simable = {};
void* __fastcall FindVehicleInterface(void*, void*, void* handle) {
  switch ((uintptr_t)handle) {
    case 0x00402940: return &chassis;
    case 0x00401A80: return &rigidBody;
    case 0x00401CA0: return &vehicleInput;
    case 0x00402990: return &vehicle;
    default: return NULL;
  }
}

float __fastcall Speed(void*, void*) { return 32.0f; }
float __fastcall Gas(void*, void*) { return 0.85f; }
float __fastcall Brake(void*, void*) { return 0.35f; }
float __fastcall HandBrake(void*, void*) { return 0.6f; }
int __fastcall Wheels(void*, void*) { return 4; }
float linearVelocity[3] = {32.0f, 0.0f, 0.0f}, angularVelocity[3] = {0.0f, 0.0f, 1.5f};
const float* __fastcall LinearVelocity(void*, void*) { return linearVelocity; }
const float* __fastcall AngularVelocity(void*, void*) { return angularVelocity; }

void* gTestSimable = &simable;
void* TestGetPlayerSimable() { return gTestSimable; }

void* queriedWheelManager = NULL;
unsigned int queriedWheelPort = 0;
int __fastcall WheelConnected(void* manager, void*, unsigned int port) {
  queriedWheelManager = manager;
  queriedWheelPort = port;
  return 7;
}

void CheckControlsMenuWheelProbe() {
  auto original = UCWheelConnected;
  UCWheelConnected = (decltype(original))WheelConnected;
  // The native call passes manager in ECX and one port argument on the stack.
  auto query = (int(__thiscall*)(void*, unsigned int))UCWheelConnectedHook;
  assert(query(NULL, 0) == 0);
  assert(queriedWheelManager == NULL);
  int manager = 0;
  assert(query(&manager, 3) == 7);
  assert(queriedWheelManager == &manager && queriedWheelPort == 3);
  UCWheelConnected = original;
}

void CheckVehicleReader() {
  auto originalFind = UTL_Com_Object_IList_Find;
  UTL_Com_Object_IList_Find = (decltype(originalFind))FindVehicleInterface;
  RumbleGetPlayerSimableOverride = TestGetPlayerSimable;
  gTestSimable = &simable;

  vehicleTable[0xAC / 4] = Speed;
  inputTable[0x08 / 4] = Brake;
  inputTable[0x0C / 4] = Gas;
  inputTable[0x1C / 4] = HandBrake;
  rigidBodyTable[0x24 / 4] = LinearVelocity;
  rigidBodyTable[0x28 / 4] = AngularVelocity;
  chassisTable[0x14 / 4] = Wheels;

  RumbleVehicleState state = {};
  assert(RumbleReadVehicle(state));
  assert(state.nWheels == 4 && state.fSpeed == 32.0f);
  assert(state.fGas == 0.85f && state.fBrake == 0.35f && state.fHandBrake == 0.6f);
  assert(state.fLinVel[0] == 32.0f && state.fAngVel[2] == 1.5f);

  gTestSimable = NULL;
  assert(!RumbleReadVehicle(state));

  UTL_Com_Object_IList_Find = originalFind;
  RumbleGetPlayerSimableOverride = NULL;
}

void CheckNativeMixerVectors() {
  FILE* file = fopen("tests/fixtures/uc-console-mixer.txt", "r");
  assert(file);
  char line[256];
  int cases = 0;
  while (fgets(line, sizeof(line), file)) {
    if (line[0] == '#') continue;
    unsigned int seed;
    float milliseconds;
    int expectedLeft, expectedRight;
    assert(sscanf(line, "%u %f %d %d", &seed, &milliseconds, &expectedLeft, &expectedRight) == 4);
    unsigned int rng = seed;
    auto next = [&]() -> unsigned int {
      rng = rng*1664525u+1013904223u;
      return (rng >> 16) & 0xFFFFu;
    };
    auto seedWave = [&](float& phase, float& amplitude, float& frequency, float& offset) {
      phase = 0;
      amplitude = (next()%33)/128.0f;
      frequency = (next()%97)/4.0f;
      offset = ((int)(next()%33)-8)/128.0f;
    };
    IFeedback f;
    Reset(f);
    for (RumbleWave* wave : {&f.mRoadWave, &f.mSkidWave, &f.mSlipWave, &f.mCollisionWave})
      for (int ch = 0; ch < 4; ++ch)
        seedWave(wave->fPhase[ch], wave->fAmplitude[ch], wave->fFrequency[ch], wave->fOffset[ch]);
    for (RumbleOscillator* wave : {&f.mEffects.driftLeft, &f.mEffects.driftRight})
      seedWave(wave->phase, wave->amplitude, wave->frequency, wave->offset);
    for (RumbleRamp* ramp : {&f.mEffects.potentialLeft, &f.mEffects.potentialRight, &f.mEffects.nos,
                        &f.mEffects.engineLeft, &f.mEffects.engineRight, &f.mEffects.shiftLeft,
                        &f.mEffects.shiftRight, &f.mEffects.bumpLeft, &f.mEffects.bumpRight}) {
      ramp->remaining = (next()%4+1)/8.0f;
      ramp->duration = 0.5f;
      ramp->amplitude = (next()%97)/128.0f;
      ramp->floor = (next()%33)/256.0f;
    }
    Tick(f, milliseconds/1000.0);
    if (abs(f.mEffects.outputLeft-expectedLeft) > 2 || abs(f.mEffects.outputRight-expectedRight) > 2) {
      printf("Native mixer vector %u: got %d/%d, expected %d/%d\n", seed,
             f.mEffects.outputLeft,f.mEffects.outputRight,expectedLeft,expectedRight);
      assert(false);
    }
    ++cases;
  }
  fclose(file);
  assert(cases == 64);
}

int main() {
  CheckControlsMenuWheelProbe();
  CheckDeviceFeedback();
  CheckVehicleReader();

  bodyTable[24] = (void*)Velocity; // native UC collision reads [vtable+0x60], not MW's +0x68
  RumbleUCFindCollection = FindCollection;
  RumbleUCConstructInstance = (decltype(RumbleUCConstructInstance))ConstructInstance;
  RumbleUCDestroyInstance = (decltype(RumbleUCDestroyInstance))DestroyInstance;
  RumbleResolveSimable = Resolve;
  RumbleSimTimePtr = &simTime;
  UTL_Com_Object_IList_Find = (decltype(UTL_Com_Object_IList_Find))Find;
  Attrib_Instance_GetAttributePointer = (decltype(Attrib_Instance_GetAttributePointer))Attribute;

  CheckNativeMixerVectors();

  const void* surface[2] = {(const void*)0x12345678, (const void*)0x87654321};
  assert(RumbleSurfaceCollection(surface) == (const void*)0x12345678);

  IFeedback feedback;
  Reset(feedback);
  void** vtable = *(void***)&feedback;

  COLLISION_INFO abiContact = Contact();
  feedback.PauseEffects();
  int stackDelta = CollisionStackDelta(&feedback, &abiContact, true);
  printf("Native collision call stack delta: %d bytes (expected 0)\n", stackDelta);
  if (stackDelta != 0) return 1;
  feedback.ResumeEffects();

  // Verify the 21-slot Undercover IFeedback vtable ABI
  auto vBeginUpdate        = (void(__thiscall*)(IFeedback*))vtable[4];
  auto vUpdateRoad         = (void(__thiscall*)(IFeedback*, bool, const SimSurface&, float))vtable[6];
  auto vUpdateRPM          = (void(__thiscall*)(IFeedback*, float, float, float))vtable[9];
  auto vUpdateShiftPotential = (void(__thiscall*)(IFeedback*, ShiftPotential))vtable[10];
  auto vUpdateEngineBlown  = (void(__thiscall*)(IFeedback*, bool))vtable[12];
  auto vUpdateShifting     = (void(__thiscall*)(IFeedback*, bool))vtable[13];
  auto vUpdateDriftSlip    = (void(__thiscall*)(IFeedback*, bool, float, float))vtable[14];
  auto vDriftTyreSlip      = (void(__thiscall*)(IFeedback*, const SimSurface&, float, float))vtable[15];
  auto vUpdateSteering     = (void(__thiscall*)(IFeedback*, float))vtable[17];
  // Literal native signature: 0x7BADF7 reserves 12 bytes for a by-value vector,
  // then 0x7BAE1B/1C push iamA and CollisionInfo*. /RTC1 checks callee cleanup.
  auto vReportCollision    = (void(__thiscall*)(IFeedback*, const COLLISION_INFO&, bool, RumbleUCVector))vtable[19];
  auto vGetControllerType  = (int(__thiscall*)(IFeedback*))vtable[20];

  // Slot 20: GetControllerType returns 0 on Undercover
  assert(vGetControllerType(&feedback) == 0);

  // Slot 6 is the road callback used by ProduceFeedback at 0x7BA9F0.
  const SimSurface& roadSurface = *(const SimSurface*)surface;
  vBeginUpdate(&feedback);
  vUpdateRoad(&feedback, true, roadSurface, 10.0f);
  assert(Near(feedback.mRoadWave.fAmplitude[0], record.fAmplitudeMin));
  assert(feedback.mWheelArg == 1 && RumbleTelemetry.nWheel == 1);

  // Slot 17 is a wheel-only no-op.
  vUpdateSteering(&feedback, 0.5f);

  // Slot 9: UpdateRPM updates throttle and powerband telemetry
  vUpdateRPM(&feedback, 0.9f, 0.75f, 5000.0f);
  assert(Near(RumbleTelemetry.fThrottle, 0.9f));
  assert(Near(RumbleTelemetry.fPowerband, 0.75f));

  // Slot 10: UpdateShiftPotential
  vUpdateShiftPotential(&feedback, SHIFT_POTENTIAL_PERFECT);
  assert(feedback.mEffects.potential == (int)SHIFT_POTENTIAL_PERFECT);

  // Slot 12: UpdateEngineBlown
  vUpdateEngineBlown(&feedback, true);
  assert(feedback.mEffects.engineOn == true);

  // Slot 13: UpdateShifting
  vUpdateShifting(&feedback, true);
  assert(feedback.mEffects.shifting == true);

  // Native BeginUpdate clears phases as well as parameters in all six wave
  // groups. The executable audit independently executes its stores for 3 ports.
  Reset(feedback);
  for (RumbleWave* wave : {&feedback.mRoadWave, &feedback.mSkidWave,
                          &feedback.mSlipWave, &feedback.mCollisionWave})
    for (int ch = 0; ch < 4; ++ch) wave->fPhase[ch] = 0.25f*(ch+1);
  feedback.mEffects.driftLeft = feedback.mEffects.driftRight = {0.75f,0.5f,60,0.1f};
  feedback.mEffects.nos.Start(1, 0.5f, 1.25f);
  vBeginUpdate(&feedback);
  for (RumbleWave* wave : {&feedback.mRoadWave, &feedback.mSkidWave,
                          &feedback.mSlipWave, &feedback.mCollisionWave})
    for (int ch = 0; ch < 4; ++ch)
      assert(!wave->fPhase[ch] && !wave->fFrequency[ch] && !wave->fAmplitude[ch] && !wave->fOffset[ch]);
  assert(!feedback.mEffects.driftLeft.phase && !feedback.mEffects.driftRight.phase);
  assert(Near(feedback.mEffects.nos.remaining, 1.25f)); // ramp times survive Begin

  // Stream consecutive producer frames at varying rates, checking the previous
  // frame's Push and this frame's Run independently. A continuously integrated
  // phase sounds different and fails on frame two.
  Reset(feedback);
  record = {4,0.6f,0.1f,0,4,0.6f,0.1f,20};
  const double frameDeltas[] = {1.0/30,1.0/60,1.0/120,0.011,0.027};
  int previous = 0;
  for (int frame = 0; frame < 100; ++frame) {
    double dt = frameDeltas[frame % 5];
    vBeginUpdate(&feedback);
    for (int wheel = 0; wheel < 4; ++wheel)
      vUpdateRoad(&feedback, wheel < 2, roadSurface, 10);
    Tick(feedback, dt);
    float phase = (float)dt*4;
    int expected = (int)((0.1f+0.6f*sinf(6.2831854820251465f*phase))*65535);
    assert(abs((int)sent.wLeftMotorSpeed-previous) <= 1 && !sent.wRightMotorSpeed);
    assert(abs(feedback.mEffects.outputLeft-expected) <= 1);
    assert(Near(feedback.mRoadWave.fPhase[0], phase));
    previous = expected;
  }

  // Literal console transform vectors, including strict piecewise boundaries.
  assert(Near(RumbleTyreSlip(-0.751f, 0.1f), 1));
  assert(Near(RumbleTyreSlip(-0.75f, 0.1f), 0));
  assert(Near(RumbleTyreSlip(1.1f, 0.2f), 0.25f));
  assert(Near(RumbleTyreSkid(0.2f, 0.1f, 0.3f, 0.5f), 0.25f));
  assert(Near(RumbleTyreMap(1, 2, 2), 0));
  assert(Near(RumbleTyreDriftSlip(0.2f, 0.1f), 0.2f));
  assert(Near(RumbleTyreDriftSlip(0.3f, 0.1f), 0));
  assert(Near(RumbleTyreDriftSlip(0.65f, 0.1f), 0.2f));
  assert(Near(RumbleTyreDriftSkid(0.2f, 0.1f, 0.3f), 0.35f));

  // Each tyre callback evaluates its native surface key using normalized input.
  record = {0,0,0,0,0,1,0,1};
  auto vSkid = (void(__thiscall*)(IFeedback*, const SimSurface&, float,float,float,float))vtable[7];
  auto vSlip = (void(__thiscall*)(IFeedback*, const SimSurface&, float,float))vtable[8];
  auto vDriftSkid = (void(__thiscall*)(IFeedback*, const SimSurface&, float,float,float))vtable[16];
  vBeginUpdate(&feedback);
  vSkid(&feedback, roadSurface, 0.2f, 0.1f, 0.3f, 0.5f);
  assert(lastKey == ATTRIB_KEY_FFB_SKID && Near(feedback.mSkidWave.fAmplitude[0], 0.25f));
  vSlip(&feedback, roadSurface, 1.1f, 0.2f);
  assert(lastKey == ATTRIB_KEY_FFB_SLIP && Near(feedback.mSlipWave.fAmplitude[0], 0.25f));
  vBeginUpdate(&feedback);
  vDriftTyreSlip(&feedback, roadSurface, 0.2f, 0.1f);
  assert(lastKey == ATTRIB_KEY_FFB_SLIP && Near(feedback.mSlipWave.fAmplitude[0], 0.2f));
  vDriftSkid(&feedback, roadSurface, 0.2f, 0.1f, 0.3f);
  assert(lastKey == ATTRIB_KEY_FFB_DRIFT_SKID && Near(feedback.mSkidWave.fAmplitude[0], 0.35f));

  // Slot 14 writes TWO waves: amplitude = angle*.5+.2, Hz = lateral*30+60.
  Reset(feedback);
  simTime = 0;
  vUpdateDriftSlip(&feedback, true, 0.4f, 1.0f);
  assert(Near(feedback.mEffects.driftLeft.amplitude, 0.4f));
  assert(Near(feedback.mEffects.driftRight.frequency, 90));
  Tick(feedback, 0.0025);
  Tick(feedback, 0.0025);
  int driftMotor = (int)(0.4f * sinf(6.2831854820251465f * 0.225f) * 65535.0f);
  assert(abs((int)sent.wLeftMotorSpeed - driftMotor) <= 1);
  assert(sent.wLeftMotorSpeed == sent.wRightMotorSpeed);
  feedback.BeginUpdate();
  assert(!feedback.mEffects.driftLeft.amplitude && !feedback.mEffects.driftLeft.phase);
  simTime = 2.5f;
  vUpdateDriftSlip(&feedback, true, -0.2f, 0.5f);
  assert(Near(feedback.mEffects.driftLeft.phase, 2500));
  assert(Near(feedback.mEffects.driftLeft.amplitude, 0.1f));
  assert(Near(feedback.mEffects.driftLeft.frequency, 75));
  vUpdateDriftSlip(&feedback, false, 1, 1);
  assert(!feedback.mEffects.driftLeft.phase && !feedback.mEffects.driftRight.amplitude);
  feedback.PauseEffects();
  vUpdateDriftSlip(&feedback, true, 1, 1);
  assert(!feedback.mEffects.driftLeft.amplitude);

  // Xbox Run advances shift/potential/engine ramps but omits their samples.
  Reset(feedback);
  vUpdateShiftPotential(&feedback, SHIFT_POTENTIAL_PERFECT);
  vUpdateEngineBlown(&feedback, true);
  vUpdateShifting(&feedback, true);
  Tick(feedback); Tick(feedback);
  assert(!sent.wLeftMotorSpeed && !sent.wRightMotorSpeed);
  assert(feedback.mEffects.shiftLeft.remaining < 0.5f);

  // Slot 18: strict threshold, parity routing, later wheel replaces earlier.
  auto vBump = (void(__thiscall*)(IFeedback*, unsigned int, float, const SimSurface&))vtable[18];
  Reset(feedback);
  vBump(&feedback, 0, 0.2f, roadSurface);
  assert(!feedback.mEffects.bumpLeft.remaining);
  vBump(&feedback, 0, 1.0f, roadSurface);
  vBump(&feedback, 2, 0.6f, roadSurface);
  vBump(&feedback, 1, 0.8f, roadSurface);
  vBump(&feedback, 4, 100.0f, roadSurface);
  assert(Near(feedback.mEffects.bumpLeft.amplitude, 0.3f));
  assert(Near(feedback.mEffects.bumpRight.amplitude, 0.4f));
  assert(Near(feedback.mEffects.bumpLeft.duration, 0.3f) && Near(feedback.mEffects.bumpLeft.floor, 0.1f));
  Tick(feedback); Tick(feedback);
  assert(abs((int)sent.wLeftMotorSpeed - (int)((0.3f-0.2f/30)*65535)) <= 1);

  // Collision waves use the feedback collection, not the contact surface.
  const float mph = 2.236989974975586f;
  for (int owner = 0; owner < 2; ++owner) {
    Reset(feedback);
    COLLISION_INFO hit = Contact(1);
    hit.objAVel[0] = bodies[0].velocity[0] = 24;
    hit.objBVel[0] = bodies[1].velocity[0] = 40;
    vReportCollision(&feedback, hit, owner == 0, RumbleUCVector{1,2,3});
    assert(lastKey == 0xAEF4FB7D);
    assert(Near(feedback.mCollisionWave.fAmplitude[0], 10*mph/200));
    assert(!instances && !feedback.mEffects.collision.remaining);
  }
  Reset(feedback);
  COLLISION_INFO hit = Contact(1);
  hit.objAVel[0] = 40; bodies[0].velocity[0] = 35;
  vReportCollision(&feedback, hit, true, RumbleUCVector{});
  assert(lastKey == 0x3C714126 && Near(feedback.mCollisionWave.fAmplitude[0], 5*mph/200));
  Reset(feedback);
  hit = Contact(1);
  hit.objAVel[0] = bodies[0].velocity[0] = 40;
  bodies[1].velocity[0] = -20;
  bodies[1].vehicle = false;
  vReportCollision(&feedback, hit, true, RumbleUCVector{});
  assert(lastKey == 0x1C2333F7 && Near(feedback.mCollisionWave.fAmplitude[0], 40*mph/200));
  Reset(feedback);
  hit = Contact(2);
  hit.objAVel[0] = 20; bodies[0].velocity[0] = 5;
  vReportCollision(&feedback, hit, true, RumbleUCVector{});
  assert(lastKey == 0x7125C7EC && Near(feedback.mCollisionWave.fAmplitude[0], 15*mph/200));
  hit.normal[0] = 0; hit.normal[1] = 1;
  vReportCollision(&feedback, hit, true, RumbleUCVector{});
  assert(lastKey == 0x96A192E8);
  feedback.BeginUpdate();
  assert(!feedback.mCollisionWave.fAmplitude[0]);
  hit = Contact(3); hit.force = 80;
  vReportCollision(&feedback, hit, true, RumbleUCVector{});
  assert(lastKey == 0x751D40E5 && Near(feedback.mCollisionWave.fAmplitude[0], 0.4f));
  // Offset-only records let us check end-to-end motor mixing deterministically.
  collisionRecord.fOffsetMin = collisionRecord.fOffsetMax = 0.3f;
  vReportCollision(&feedback, hit, true, RumbleUCVector{});
  Tick(feedback); Tick(feedback);
  assert(abs((int)sent.wLeftMotorSpeed - (int)(0.3f*65535)) <= 1 && !sent.wRightMotorSpeed);
  collisionStereo = true;
  feedback.BeginUpdate();
  vReportCollision(&feedback, hit, true, RumbleUCVector{});
  Tick(feedback); Tick(feedback);
  assert(sent.wLeftMotorSpeed == sent.wRightMotorSpeed && sent.wLeftMotorSpeed > 0);
  collectionPresent = false;
  feedback.BeginUpdate();
  vReportCollision(&feedback, hit, true, RumbleUCVector{});
  assert(!instances && !feedback.mCollisionWave.fAmplitude[0]);
  feedback.PauseEffects();
  assert(!sent.wLeftMotorSpeed && !sent.wRightMotorSpeed);

  puts("Undercover console curves, mixer, collision collection, motor output and native ABI checks passed");
  return 0;
}
