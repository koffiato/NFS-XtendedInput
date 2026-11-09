// Recovered Carbon controller behavior; native interfaces, clock and output are mocked.
#define NOMINMAX
#ifndef GAME_CARBON
#define GAME_CARBON
#endif
#define RUMBLE_TESTING
#include <Windows.h>
#include <XInput.h>
#include <cassert>
#include <cstdio>
double fixedTime = 10;
float simTime = 2.5f;
XINPUT_VIBRATION sent = {};
DWORD outputResult = ERROR_SUCCESS;
BOOL WINAPI TestQPC(LARGE_INTEGER* p) { p->QuadPart = (LONGLONG)(fixedTime * 1000000 + 0.5); return TRUE; }
BOOL WINAPI TestQPF(LARGE_INTEGER* p) { p->QuadPart = 1000000; return TRUE; }
DWORD WINAPI TestSetState(DWORD, XINPUT_VIBRATION* value) { sent = *value; return outputResult; }
#define QueryPerformanceCounter TestQPC
#define QueryPerformanceFrequency TestQPF
#define XInputSetState TestSetState
#include "../src/Main.hpp"

bool Near(float a, float b) { return fabsf(a - b) < 0.00001f; }
struct Body {
  void** table;
  void* interfaces;
  float velocity[3];
  bool vehicle;
} bodies[2];
void* bodyTable[27] = {};
void __fastcall Velocity(Body* body, void*, float* out) { memcpy(out, body->velocity, 12); }
void* __cdecl Resolve(void* handle) {
  return handle == (void*)1 ? &bodies[0] : handle == (void*)2 ? &bodies[1] : NULL;
}
void* __fastcall Find(void* list, void*, void* handle) {
  return handle == (void*)IVEHICLE_IHANDLE_ADDR && ((Body*)list)->vehicle ? list : NULL;
}
RumbleWaveRecord record = {2, 0.6f, 0.3f, 0, 2, 0.6f, 0.3f, 20};
const void* __fastcall Attribute(const void*, void*, unsigned int, unsigned int index) {
  return index == 0 ? &record : NULL;
}
struct DraftInfo {
  unsigned char enabled;
  unsigned char inSlipstream;
  unsigned char pad[0x16];
  float bonus;
  float wakeTime;
};
static_assert(sizeof(DraftInfo) == 0x20, "DraftInfo size mismatch");

void Reset(IFeedback& f) {
  fixedTime = 10;
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
  hit.normal[0] = 1;
  hit.closingVel[0] = 15;
  return hit;
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
  assert(list == deviceList && handle == (void*)0x00679170);
  deviceFeedback = iface;
}
void* __fastcall FindFeedback(void* list, void*, void* handle) {
  assert(list == deviceList && handle == (void*)0x00679170);
  return deviceFeedback;
}
void InitTestProfile() {}

void CheckDeviceFeedback() {
  // Native SetControllerPort (0x7651EE) calls device slot 9, then queries
  // IFeedback through the returned interface's owner list at +4. Testing the
  // effect object alone misses a shifted device vtable and yields silent pads.
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
  bGlobalDoPolling = false; // exercise feedback polling without native input APIs

  for (int enabled = 0; enabled <= 1; ++enabled) {
    bRumbleEnabled = enabled != 0;
    InputDevice device(0);
    void** table = *(void***)&device;
    auto getInterfaces = (void*(__thiscall*)(InputDevice*))table[9];
    void* iface = getInterfaces(&device);
    if (!enabled) {
      assert(!iface && !deviceFeedback);
    } else {
      assert(iface == &device.mFFB);
      IFeedback* feedback = (IFeedback*)RumbleFindInterface(iface, 0x00679170);
      assert(feedback == &device.mFFB);
      assert(feedback->mOwnerList == deviceList);
      void** feedbackTable = *(void***)feedback;
      ((void(__thiscall*)(IFeedback*))feedbackTable[2])(feedback);
      ((void(__thiscall*)(IFeedback*))feedbackTable[4])(feedback);
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
      ((void(__thiscall*)(InputDevice*))table[8])(&device); // native StopVibration
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
void* suspensionTable[21] = {};
void* rigidBodyTable[11] = {};
void* inputTable[9] = {};
void* vehicleTable[44] = {};
VehicleInterface suspension = {suspensionTable}, rigidBody = {rigidBodyTable};
VehicleInterface vehicleInput = {inputTable}, vehicle = {vehicleTable};
VehicleInterface simable = {};
void* testSimable = &simable;
void* TestGetPlayerSimable() { return testSimable; }
void* __fastcall FindVehicleInterface(void*, void*, void* handle) {
  // Literal handles recovered from the native constructors, independent of
  // input/Addresses.h: 0x6DB9F0 registers suspension; 0x6DB810 is transmission.
  switch ((uintptr_t)handle) {
    case 0x00403CE0: return &suspension;
    case 0x00403750: return &rigidBody;
    case 0x00403AA0: return &vehicleInput;
    case 0x00403D30: return &vehicle;
    default: return NULL;
  }
}
float __fastcall Speed(void*, void*) { return 25.0f; }
float __fastcall Gas(void*, void*) { return 0.75f; }
float __fastcall Brake(void*, void*) { return 0.2f; }
float __fastcall HandBrake(void*, void*) { return 0.4f; }
float __fastcall Clutch(void*, void*) { return 0.9f; }
int __fastcall Wheels(void*, void*) { return 4; }
bool __fastcall Ground(void*, void*, unsigned int wheel) { return wheel != 3; }
float __fastcall WheelValue(void*, void*, unsigned int wheel) { return 1.0f + wheel; }
float linearVelocity[3] = {25, 0, 0}, angularVelocity[3] = {0, 0, 1};
const float* __fastcall LinearVelocity(void*, void*) { return linearVelocity; }
const float* __fastcall AngularVelocity(void*, void*) { return angularVelocity; }

void CheckVehicleReader() {
  auto originalFind = UTL_Com_Object_IList_Find;
  UTL_Com_Object_IList_Find = (decltype(originalFind))FindVehicleInterface;
  RumbleGetPlayerSimableOverride = TestGetPlayerSimable;
  testSimable = &simable;
  vehicleTable[0xAC / 4] = Speed;
  inputTable[0x08 / 4] = Brake;
  inputTable[0x0C / 4] = Gas;
  inputTable[0x1C / 4] = HandBrake;
  inputTable[0x20 / 4] = Clutch;
  rigidBodyTable[0x24 / 4] = LinearVelocity;
  rigidBodyTable[0x28 / 4] = AngularVelocity;
  suspensionTable[0x08 / 4] = Wheels;
  suspensionTable[0x24 / 4] = Ground;
  for (int offset : {0x28, 0x30, 0x38, 0x50}) suspensionTable[offset / 4] = WheelValue;
  RumbleVehicleState state = {};
  assert(RumbleReadVehicle(state));
  assert(state.nWheels == 4 && state.fSpeed == 25);
  assert(state.fGas == 0.75f && state.fBrake == 0.2f && state.fHandBrake == 0.4f);
  assert(state.bOnGround[0] && !state.bOnGround[3]);
  assert(state.fSlip[2] == 3 && state.fSkid[2] == 3 && state.fWheelAV[2] == 3);
  assert(state.fLinVel[0] == 25 && state.fAngVel[2] == 1);
  testSimable = NULL;
  assert(!RumbleReadVehicle(state));
  UTL_Com_Object_IList_Find = originalFind;
  RumbleGetPlayerSimableOverride = NULL;
}

int main() {
  CheckDeviceFeedback();
  CheckVehicleReader();
  bodyTable[26] = (void*)Velocity;
  RumbleResolveSimable = Resolve;
  RumbleSimTimePtr = &simTime;
  UTL_Com_Object_IList_Find = (decltype(UTL_Com_Object_IList_Find))Find;
  Attrib_Instance_GetAttributePointer = (decltype(Attrib_Instance_GetAttributePointer))Attribute;

  // Carbon SimSurface stores mCollection at +0x00 (unlike MW's +0x04).
  const void* surface[2] = {(const void*)0x12345678, (const void*)0x87654321};
  assert(RumbleSurfaceCollection(surface) == (const void*)0x12345678);

  IFeedback feedback;
  Reset(feedback);
  void** vtable = *(void***)&feedback;

  // Verify the 18-slot Carbon vtable ABI (slots 14..17 at +0x38..+0x44).
  auto vBeginUpdate = (void(__thiscall*)(IFeedback*))vtable[4];
  auto vUpdateDrafting = (void(__thiscall*)(IFeedback*, const void*))vtable[14];
  auto vUpdateDrift = (void(__thiscall*)(IFeedback*, float))vtable[15];
  auto vUpdateSteering = (void(__thiscall*)(IFeedback*, float))vtable[16];
  auto vReportCollision = (void(__thiscall*)(IFeedback*, const COLLISION_INFO&, bool))vtable[17];

  // Slot 16: UpdateSteering is a safe no-op on controller devices.
  vUpdateSteering(&feedback, 0.75f);

  // Slot 14: UpdateDrafting pulse (< 1.5s wake) vs slipstream attenuation (>= 1.5s wake).
  DraftInfo draft = {};
  vUpdateDrafting(&feedback, NULL);
  vUpdateDrafting(&feedback, &draft);
  assert(feedback.mEffects.draft.amplitude == 0 && feedback.mEffects.draftScale == 1.0f);

  draft.enabled = 1;
  draft.wakeTime = 0.75f;  // f = 1 - 0.75 * (2/3) = 0.5
  vUpdateDrafting(&feedback, &draft);
  assert(Near(feedback.mEffects.draft.phase, 2500.0f));
  assert(Near(feedback.mEffects.draft.amplitude, 0.7f));
  assert(Near(feedback.mEffects.draft.frequency, 30.0f));
  assert(Near(feedback.mEffects.draftScale, 1.0f));

  // Paused drafting ignores the < 1.5s wave write, but still writes the >= 1.5s 0.5x scale.
  Reset(feedback);
  feedback.PauseEffects();
  vUpdateDrafting(&feedback, &draft);
  assert(feedback.mEffects.draft.amplitude == 0);
  draft.wakeTime = 1.5f;
  draft.inSlipstream = 0;
  vUpdateDrafting(&feedback, &draft);
  assert(Near(feedback.mEffects.draftScale, 1.0f));
  draft.inSlipstream = 1;
  vUpdateDrafting(&feedback, &draft);
  assert(Near(feedback.mEffects.draftScale, 0.5f));

  // Verify draftScale halves motor output before clamping, and BeginUpdate restores 1.0f.
  feedback.ResumeEffects();
  feedback.UpdateNOS(true, 1.0f);
  Tick(feedback);
  Tick(feedback);
  // NOS after 10 ms is 0.9975; scaled by 0.5 is 0.49875 -> 32685.
  assert(abs((int)sent.wLeftMotorSpeed - 32685) <= 1 && sent.wRightMotorSpeed == 0);
  vBeginUpdate(&feedback);
  assert(Near(feedback.mEffects.draftScale, 1.0f));
  assert(feedback.mEffects.draft.amplitude == 0 && feedback.mEffects.draft.frequency == 0);

  // Slot 15: UpdateDrift two-stage threshold and paused behavior.
  Reset(feedback);
  feedback.mEffects.drift.offset = 0.25f;
  vUpdateDrift(&feedback, 0.01f);
  assert(feedback.mEffects.drift.amplitude == 0 && Near(feedback.mEffects.drift.offset, 0.25f));

  // First stage: 0.01 < |slipAngle| <= 0.015 preserves offset and uses 8.16326523 scale.
  vUpdateDrift(&feedback, -0.0125f);
  float expectedT1 = (0.0125f - 0.01f) * 8.16326523f;
  assert(Near(feedback.mEffects.drift.phase, 2500.0f));
  assert(Near(feedback.mEffects.drift.amplitude, expectedT1 * 0.6f + 0.4f));
  assert(Near(feedback.mEffects.drift.frequency, 60.0f));
  assert(Near(feedback.mEffects.drift.offset, 0.25f));

  // Second stage: |slipAngle| > 0.015 zeroes offset (copying +0x140) and uses 8.24742222 scale.
  feedback.PauseEffects();  // Xbox 0x8244AED0 writes drift wave even while paused.
  simTime = 0.0f;
  vUpdateDrift(&feedback, 0.075625f);  // (0.075625 - 0.015) * 8.24742222 = 0.5
  assert(Near(feedback.mEffects.drift.phase, 0.0f));
  assert(Near(feedback.mEffects.drift.amplitude, 0.7f));
  assert(Near(feedback.mEffects.drift.frequency, 60.0f));
  assert(Near(feedback.mEffects.drift.offset, 0.0f));
  feedback.ResumeEffects();
  Tick(feedback, 0.0025);  // phase = 60 * 0.0025 = 0.15; sin(2*pi*0.15) * 0.7
  Tick(feedback, 0.0025);
  int expectedDriftRight = (int)(0.7f * sinf(6.2831854820251465f * 0.15f) * 65535.0f);
  assert(sent.wLeftMotorSpeed == 0 && abs((int)sent.wRightMotorSpeed - expectedDriftRight) <= 1);

  // Slot 17: ReportCollision uses Carbon's 0.25f rear-end scale and 0.75f moving-contact scale.
  for (int owner = 0; owner < 2; ++owner) {
    Reset(feedback);
    COLLISION_INFO hit = Contact(1);
    hit.objAVel[0] = bodies[0].velocity[0] = 40;
    hit.objBVel[0] = bodies[1].velocity[0] = 40;
    vReportCollision(&feedback, hit, owner == 0);
    // rearDelta = 40 * 0.25 = 10 -> (10 - 5) / 20 = 0.25
    assert(Near(feedback.mEffects.collision.amplitude, 0.25f));
  }
  Reset(feedback);
  COLLISION_INFO hit = Contact(1);
  hit.objAVel[0] = bodies[0].velocity[0] = 40;
  hit.objBVel[0] = bodies[1].velocity[0] = -20;
  bodies[1].vehicle = false;
  vReportCollision(&feedback, hit, true);
  // ((40 - 20) / 30) * 0.75 = 0.5
  assert(Near(feedback.mEffects.collision.amplitude, 0.5f));

  puts("Carbon device hookup, motor output and effect checks passed");
  return 0;
}
