// Recovered Undercover controller feedback and native interface wiring tests.
#define NOMINMAX
#define GAME_UC
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
const void* __fastcall Attribute(const void*, void*, unsigned int, unsigned int index) {
  return index == 0 ? &record : NULL;
}

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

int main() {
  CheckControlsMenuWheelProbe();
  CheckDeviceFeedback();
  CheckVehicleReader();

  bodyTable[24] = (void*)Velocity; // native UC collision reads [vtable+0x60], not MW's +0x68
  RumbleMWResolveSimable = Resolve;
  RumbleSimTimePtr = &simTime;
  UTL_Com_Object_IList_Find = (decltype(UTL_Com_Object_IList_Find))Find;
  Attrib_Instance_GetAttributePointer = (decltype(Attrib_Instance_GetAttributePointer))Attribute;

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
  auto vReservedUC15       = (void(__thiscall*)(IFeedback*, uintptr_t, uintptr_t, uintptr_t))vtable[15];
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

  // Slots 15 and 17 are safe no-ops.
  vReservedUC15(&feedback, 0, 0, 0);
  vUpdateSteering(&feedback, 0.5f);

  // Slot 9: UpdateRPM updates throttle and powerband telemetry
  vUpdateRPM(&feedback, 0.9f, 0.75f, 5000.0f);
  assert(Near(RumbleTelemetry.fThrottle, 0.9f));
  assert(Near(RumbleTelemetry.fPowerband, 0.75f));

  // Slot 10: UpdateShiftPotential
  vUpdateShiftPotential(&feedback, SHIFT_POTENTIAL_PERFECT);
  assert(feedback.mMW.potential == (int)SHIFT_POTENTIAL_PERFECT);

  // Slot 12: UpdateEngineBlown
  vUpdateEngineBlown(&feedback, true);
  assert(feedback.mMW.engineOn == true);

  // Slot 13: UpdateShifting
  vUpdateShifting(&feedback, true);
  assert(feedback.mMW.shifting == true);

  // Slot 14: UpdateDriftSlip (active, slipAngle, lateralSlip)
  Reset(feedback);
  feedback.mMW.drift.offset = 0.25f;

  // Inactive drift: does not update drift wave
  vUpdateDriftSlip(&feedback, false, 0.05f, 0.0f);
  assert(feedback.mMW.drift.amplitude == 0.0f && Near(feedback.mMW.drift.offset, 0.25f));

  // Active drift, below threshold (|slipAngle| <= 0.01)
  vUpdateDriftSlip(&feedback, true, 0.01f, 0.0f);
  assert(feedback.mMW.drift.amplitude == 0.0f && Near(feedback.mMW.drift.offset, 0.25f));

  // First stage: 0.01 < |slipAngle| <= 0.015
  vUpdateDriftSlip(&feedback, true, -0.0125f, 0.0f);
  float expectedT1 = (0.0125f - 0.01f) * 8.16326523f;
  assert(Near(feedback.mMW.drift.phase, 2500.0f));
  assert(Near(feedback.mMW.drift.amplitude, expectedT1 * 0.6f + 0.4f));
  assert(Near(feedback.mMW.drift.frequency, 60.0f));
  assert(Near(feedback.mMW.drift.offset, 0.25f));

  // Second stage: |slipAngle| > 0.015
  feedback.PauseEffects();
  simTime = 0.0f;
  vUpdateDriftSlip(&feedback, true, 0.075625f, 0.0f);
  assert(Near(feedback.mMW.drift.phase, 0.0f));
  assert(Near(feedback.mMW.drift.amplitude, 0.7f));
  assert(Near(feedback.mMW.drift.frequency, 60.0f));
  assert(Near(feedback.mMW.drift.offset, 0.0f));
  feedback.ResumeEffects();
  Tick(feedback, 0.0025);
  Tick(feedback, 0.0025);
  int expectedDriftRight = (int)(0.7f * sinf(6.2831854820251465f * 0.15f) * 65535.0f);
  assert(sent.wLeftMotorSpeed == 0 && abs((int)sent.wRightMotorSpeed - expectedDriftRight) <= 1);

  // Slot 19: ReportCollision uses 0.25f rear-end scale and 0.75f moving-contact scale
  for (int owner = 0; owner < 2; ++owner) {
    Reset(feedback);
    COLLISION_INFO hit = Contact(1);
    hit.objAVel[0] = bodies[0].velocity[0] = 40.0f;
    hit.objBVel[0] = bodies[1].velocity[0] = 40.0f;
    vReportCollision(&feedback, hit, owner == 0, RumbleUCVector{1.0f, 2.0f, 3.0f});
    // rearDelta = 40 * 0.25 = 10 -> (10 - 5) / 20 = 0.25
    assert(Near(feedback.mMW.collision.amplitude, 0.25f));
  }

  Reset(feedback);
  COLLISION_INFO hit = Contact(1);
  hit.objAVel[0] = bodies[0].velocity[0] = 40.0f;
  hit.objBVel[0] = bodies[1].velocity[0] = -20.0f;
  bodies[1].vehicle = false;
  vReportCollision(&feedback, hit, true, RumbleUCVector{});
  // ((40 - 20) / 30) * 0.75 = 0.5
  assert(Near(feedback.mMW.collision.amplitude, 0.5f));

  puts("Undercover controls menu, device, motor output, vtable ABI, and collision checks passed");
  return 0;
}
