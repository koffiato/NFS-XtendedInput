// Integration checks against the supported executable's mapped feedback code/data.
// Executes native feedback routines in a private mapped PE; device output is mocked.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <XInput.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <fstream>
#include <vector>
#include "../src/inc/injector/injector.hpp"

#define MAX_CONTROLLERS 4
// Map the supported PE at its fixed base; the test EXE is linked above it.
// Native feedback instructions run with synthetic game state and mocked output.
#define RUMBLE_IMAGE_BASE 0x00400000u
#define GAME_HWND_ADDR (RUMBLE_IMAGE_BASE + 0x00470990)
#define GAMEFLOWMANAGER_STATUS_ADDR (RUMBLE_IMAGE_BASE + 0x004654A4)
#define JOY_EVENT_THROTTLE 0x91
#define JOY_EVENT_THROTTLE_ANALOG 0x92
#define JOY_EVENT_THROTTLE_ANALOG_ALTERNATE 0x93
#define JOY_EVENT_BRAKE 0x94
#define JOY_EVENT_BRAKE_ANALOG 0x95
#define JOY_EVENT_BRAKE_ANALOG_ALTERNATE 0x96
struct { XINPUT_STATE state; bool bConnected; } g_Controllers[4] = {};
WORD EventStates[2][256] = {};
bool EventStatesKB[2][256] = {};
bool enabled = true, focused = true;
int calls[4] = {};
XINPUT_VIBRATION motors[4] = {};
HWND TestForeground() { return focused ? (HWND)0x1234 : NULL; }
BOOL TestIconic(HWND) { return FALSE; }
DWORD WINAPI TestSetState(DWORD pad, XINPUT_VIBRATION* value) {
  assert(pad < 2); ++calls[pad]; motors[pad] = *value;
  return g_Controllers[pad].bConnected ? ERROR_SUCCESS : ERROR_DEVICE_NOT_CONNECTED;
}
DWORD WINAPI TestGetState(DWORD pad, XINPUT_STATE*) {
  return g_Controllers[pad].bConnected ? ERROR_SUCCESS : ERROR_DEVICE_NOT_CONNECTED;
}
struct InputConfig {
  InputConfig(const char*) {}
  int ReadInteger(const char*, const char* key, int fallback) {
    if (!strcmp(key,"Enabled")) return enabled;
    if (!strcmp(key,"ImpulseTriggers")) return 0;
    return fallback;
  }
  float ReadFloat(const char*, const char* key, float fallback) {
    if (!strcmp(key,"Enabled")) return enabled ? 1.0f : 0.0f;
    if (!strcmp(key,"ImpulseTriggers")) return 0;
    return fallback;
  }
  const char* GetIniPath() { return "rumble-tests.ini"; }
};
#define GetForegroundWindow TestForeground
#define IsIconic TestIconic
#define XInputSetState TestSetState
#define XInputGetState TestGetState
#include "../src/rumble/Underground.h"

void LoadImage(const char* path) {
  // Reserved in the suspended child before CRT heaps can occupy the fixed image.
  auto* game = (char*)RUMBLE_IMAGE_BASE;
  std::ifstream stream(path, std::ios::binary);
  assert(stream.good());
  std::vector<char> image((std::istreambuf_iterator<char>(stream)), {});
  auto* dos = (IMAGE_DOS_HEADER*)image.data();
  auto* nt = (IMAGE_NT_HEADERS*)(image.data()+dos->e_lfanew);
  assert(nt->OptionalHeader.ImageBase == 0x400000);
  assert(nt->OptionalHeader.SizeOfImage <= 0x600000);
  auto* sec = IMAGE_FIRST_SECTION(nt);
  for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i)
    if (sec[i].SizeOfRawData)
      memcpy(game+sec[i].VirtualAddress,image.data()+sec[i].PointerToRawData,sec[i].SizeOfRawData);

}

int main(int argc, char** argv) {
  if (argc == 2) {
    char executable[MAX_PATH], command[MAX_PATH*3];
    assert(GetModuleFileNameA(NULL, executable, MAX_PATH));
    sprintf_s(command, "\"%s\" \"%s\" --native-child", executable, argv[1]);
    STARTUPINFOA startup = {}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION child = {};
    // Retry only address reservation: ASLR may place an initial heap in the
    // game range. Once reserved, test failures propagate without retrying.
    bool reserved = false;
    for (int attempt=0; attempt<32 && !reserved; ++attempt) {
      assert(CreateProcessA(NULL, command, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &startup, &child));
      void* image = VirtualAllocEx(child.hProcess, (void*)RUMBLE_IMAGE_BASE, 0x600000,
                                  MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
      reserved = image == (void*)RUMBLE_IMAGE_BASE;
      if (!reserved) { TerminateProcess(child.hProcess,1); CloseHandle(child.hThread); CloseHandle(child.hProcess); }
    }
    if (!reserved) { fprintf(stderr,"Could not reserve fixed native image in test child.\n"); return 1; }
    ResumeThread(child.hThread);
    DWORD result = 1;
    if (WaitForSingleObject(child.hProcess,60000) == WAIT_OBJECT_0) GetExitCodeProcess(child.hProcess,&result);
    else TerminateProcess(child.hProcess,1);
    CloseHandle(child.hThread); CloseHandle(child.hProcess);
    return (int)result;
  }
  assert(argc == 3 && !strcmp(argv[2], "--native-child"));
  // Both adapters use this same parser, including finite bounds and legacy GearShift values.
  RumbleLoadSettings([](const char* key, float fallback) {
    if (!strcmp(key,"Intensity")) return INFINITY;
    if (!strcmp(key,"Car")) return -5.0f;
    if (!strcmp(key,"GearShift")) return 999.0f;
    if (!strcmp(key,"ImpulseTriggers")) return -1.0f;
    return fallback;
  });
  assert(fRumbleIntensity == 0 && fRumbleCategoryScale[kRumbleCar] == 0);
  assert(bRumbleGearShift && RumbleTriggerMode == kTriggersOff);
  RumbleLoadSettings([](const char*, float fallback) { return fallback; });
  LoadImage(argv[1]);
  assert(RumbleValidateHooks());
  unsigned char original = *(unsigned char*)RUMBLE_OUTPUT_ADDR;
  *(unsigned char*)RUMBLE_OUTPUT_ADDR = 0x90;
  assert(!RumbleValidateHooks()); RumbleInit();
  assert(!bRumbleInstalled && *(unsigned char*)RUMBLE_EVENT_ADDR == 0x8B);
  *(unsigned char*)RUMBLE_OUTPUT_ADDR = original;
  for (uintptr_t site : {RUMBLE_OUTPUT_CALL, RUMBLE_STOP_CALL}) {
    unsigned char displacement = *(unsigned char*)(site+1);
    *(unsigned char*)(site+1) ^= 1;
    assert(!RumbleValidateHooks()); RumbleInit(); assert(!bRumbleInstalled);
    *(unsigned char*)(site+1) = displacement;
  }
  enabled = false; RumbleInit(); assert(!bRumbleInstalled);
  enabled = true; RumbleInit();
  assert(bRumbleInstalled && *(unsigned char*)RUMBLE_OUTPUT_ADDR == 0xC3);
  for (uintptr_t site : {RUMBLE_OUTPUT_CALL, RUMBLE_STOP_CALL}) {
    assert(*(unsigned char*)site == 0xE8);
    assert(site+5+*(int*)(site+1) == (uintptr_t)RumbleNativeOutput);
  }
  assert(*(unsigned char*)RUMBLE_EVENT_ADDR == 0x8B); // original dispatcher preserved
  assert(*(unsigned short*)RUMBLE_SETTING_ADDR == 0x01B3);
  assert(*(unsigned short*)RUMBLE_COLLISION_SETTING_ADDR == 0x9090);
  unsigned char notPaused[] = {0xB0,0,0xC3};
  memcpy((void*)RUMBLE_PACKAGE_ACTIVE_ADDR,notPaused,sizeof(notPaused));
  *(HWND*)GAME_HWND_ADDR = (HWND)0x1234;
  *(int*)GAMEFLOWMANAGER_STATUS_ADDR = 6;
  static int race = 6; *(int**)0x8900D8 = &race;
  *(int*)0x7FAA14 = 1; *(int*)0x8651AC = 1000;
  *(int*)RUMBLE_PLAYER_COUNT_ADDR = 0;
  *(float*)RUMBLE_SIM_DT_ADDR = .075f;
  *(float*)0x865198 = .075f; // native feedback delta
  g_Controllers[0].bConnected = g_Controllers[1].bConnected = true;
  auto event = (void(__cdecl*)(int,int,float))RUMBLE_EVENT_ADDR;
  assert(RumbleCanPlay());
  assert(fRumbleLeftScale == 1.0f);
  RumbleNativeOutput(0,0,64);
  assert(motors[0].wLeftMotorSpeed == RumbleMixer::ToMotor(64/255.0f,1.0f));
  // The empty function is shared by unrelated game systems. They may pass no
  // motor arguments at all; even zeros on that stack must not stop feedback.
  auto sharedStub = (void(__cdecl*)(int,int,int))RUMBLE_OUTPUT_ADDR;
  int beforeStub = motors[0].wLeftMotorSpeed;
  sharedStub(0,0,0);
  assert(motors[0].wLeftMotorSpeed == beforeStub);
  fRumbleLeftScale = 1; RumbleStopAllPads(); // remaining checks use unscaled native levels
  // A strong collision must displace road effects and reach both body motors.
  // Unrelated shared-stub calls between frames must leave playback intact.
  event(0,0,1.99f); assert(!RumbleChannels(0)->curve[0]);
  for (int i=0;i<3;++i) event(1,0,.025f);
  event(0,0,11);
  bool collision = false;
  for (int i=0;i<3;++i) collision |= RumbleChannels(0)->priority[i] == 8;
  assert(collision);
  RumbleFrame();
  assert(motors[0].wLeftMotorSpeed == 65535 && motors[0].wRightMotorSpeed == 65535);
  sharedStub(0,0,0); sharedStub(0,0,0);
  assert(motors[0].wLeftMotorSpeed == 65535 && motors[0].wRightMotorSpeed == 65535);
  RumbleFrame(); assert(motors[0].wLeftMotorSpeed);
  RumbleStopAllPads();
  event(3,0,3); RumbleFrame();
  assert(motors[0].wLeftMotorSpeed == 65535 && motors[0].wRightMotorSpeed == 65535);
  RumbleStopAllPads();
  event(7,1,1);
  assert(RumbleChannels(1)->curve[0] && RumbleChannels(1)->gain[0] == 3);
  for(int i=0;i<3;++i) RumbleFrame();
  assert(fabsf(RumbleChannels(1)->age[0]-.075f) < .00001f);
  RumbleFrame();
  assert(RumbleChannels(1)->output == 60);
  assert(motors[1].wLeftMotorSpeed == 60*257 && !motors[1].wRightMotorSpeed);
  assert(!motors[0].wLeftMotorSpeed);
  RumbleStopAllPads();
  event(4,0,1); assert(!RumbleChannels(0)->curve[0]);
  for(int i=0;i<3;++i) event(7,0,1);
  for(int i=0;i<3;++i) assert(RumbleChannels(0)->curve[i]);
  RumbleChannels(0)->age[0]=.19f;event(7,0,1);
  assert(RumbleChannels(0)->age[0] == .19f); // native equal-gain rejection
  RumbleStopAllPads();
  RumbleNativeOutput(0,128,128);assert(motors[0].wLeftMotorSpeed==128*257 && !motors[0].wRightMotorSpeed);
  RumbleNativeOutput(0,129,129);assert(motors[0].wRightMotorSpeed==65535);
  ((void(__cdecl*)(int))0x5726A0)(0);assert(!motors[0].wLeftMotorSpeed && !motors[0].wRightMotorSpeed);
  RumbleNativeOutput(-1,255,255); RumbleNativeOutput(2,255,255);
  event(7,0,1);RumbleFrame();assert(motors[0].wLeftMotorSpeed);
  focused=false;RumblePoll();assert(!motors[0].wLeftMotorSpeed && !RumbleChannels(0)->curve[0]);focused=true;
  event(7,0,1);RumbleFrame();*((unsigned char*)RUMBLE_PACKAGE_ACTIVE_ADDR+1)=1;
  RumblePoll();assert(!motors[0].wLeftMotorSpeed);*((unsigned char*)RUMBLE_PACKAGE_ACTIVE_ADDR+1)=0;
  event(7,0,1);RumbleFrame();*(float*)RUMBLE_SIM_DT_ADDR=0;
  RumbleFrame();assert(!motors[0].wLeftMotorSpeed);*(float*)RUMBLE_SIM_DT_ADDR=.075f;
  event(7,0,1);RumbleFrame();g_Controllers[0].bConnected=false;
  RumblePoll();assert(!motors[0].wLeftMotorSpeed);g_Controllers[0].bConnected=true;
  event(7,0,1);RumbleFrame();RumbleLastFrame-=1;RumblePoll();assert(!motors[0].wLeftMotorSpeed);
  event(7,0,1);RumbleFrame();*(int*)GAMEFLOWMANAGER_STATUS_ADDR=3;
  RumblePoll();assert(!motors[0].wLeftMotorSpeed);*(int*)GAMEFLOWMANAGER_STATUS_ADDR=6;
  RumbleNativeOutput(0,255,255); focused=false;
  RumbleNativeOutput(0,255,255);assert(!motors[0].wLeftMotorSpeed && !motors[0].wRightMotorSpeed);focused=true;
  assert(RumbleSlipTrigger(1,1)==0);
  RumbleTriggerMode=kTriggersAuto;
  assert(RumbleSlipTrigger(1,0)==0 && RumbleSlipTrigger(0,1)==0);
  assert(RumbleSlipTrigger(1,1)==65535 && RumbleSlipTrigger(1,.5f)==32767);
  float eased = 0;
  int firstBrake = RumblePedalTrigger(1,1,.01f,eased);
  assert(firstBrake > 4000 && firstBrake < 4500); // a solver step cannot hit full output
  for (int i=0;i<99;++i) RumblePedalTrigger(1,1,.01f,eased);
  assert(eased > .998f); // sustained lockup retains full strength
  float finer = 0;
  for (int i=0;i<200;++i) RumblePedalTrigger(1,1,.005f,finer);
  assert(fabsf(eased-finer) < .0001f); // same response at different frame rates
  assert(RumblePedalTrigger(1,0,.01f,eased)==0 && eased==0);
  // Saturation at half pedal gets a softer midrange; high demand stays strong.
  float middle = 0, upper = 0;
  for (int i=0;i<100;++i) {
    RumblePedalTrigger(1,.5f,.01f,middle);
    RumblePedalTrigger(1,.9f,.01f,upper);
  }
  assert(middle > .436f && middle < .438f);
  assert(upper > .875f && upper < .878f);
  bRumbleTriggerPedalScaling=false;
  assert(RumbleSlipTrigger(1,0)==0 && RumbleSlipTrigger(1,.5f)==65535);
  bRumbleTriggerPedalScaling=true;RumbleTriggerMode=kTriggersOff;
  // Binding-aware pedals: alternate analog and keyboard maps are honored.
  EventStates[0][JOY_EVENT_THROTTLE_ANALOG_ALTERNATE] = 128;
  assert(RumblePedal(0,0x91,0x92,0x93) > 0.50f && RumblePedal(0,0x91,0x92,0x93) < 0.51f);
  EventStatesKB[0][JOY_EVENT_THROTTLE] = true;
  assert(RumblePedal(0,0x91,0x92,0x93) == 1);
  // Read actual U2-layout fixtures: rolling, wheelspin, lockup, air, reverse, stale pointers.
  char player[16] = {}, car[64] = {}, mover[128] = {}, body[192] = {};
  char wheels[4][0x180] = {}, tyres[4][32] = {};
  *(char**)RUMBLE_PLAYERS_ADDR = player; *(int*)RUMBLE_PLAYER_COUNT_ADDR = 1;
  *(char**)(player+4) = car; *(char**)(car+0x34) = mover; *(char**)(mover+0x20) = body;
  *(float*)(body+0xA0) = 10;
  for (int i = 0; i < 4; ++i) {
    *(char**)(mover+0x38+i*4) = wheels[i]; *(char**)(wheels[i]+0x14) = tyres[i];
    *(float*)(tyres[i]+0x10) = 0.3f; *(float*)(wheels[i]+0x28) = 10/0.3f;
    *(float*)(wheels[i]+0x170) = 1;
    *(unsigned char*)(wheels[i]+0x11C) = 1;
    *(float*)(wheels[i]+0x70) = 1; // wheel forward X axis
    *(float*)(wheels[i]+0xF0) = 10; // contact-point rolling speed
  }
  float spin, brake;
  assert(RumbleReadSlip(0,spin,brake) && spin < 0.01f && brake < 0.01f);
  // Braking at the native traction limit needs a cue before rotation stops.
  *(float*)(wheels[0]+0x164) = 1000;
  *(float*)(wheels[0]+0x170) = 1;
  *(float*)(wheels[0]+0x124) = 1000;
  assert(RumbleReadSlip(0,spin,brake) && !spin && brake == 1);
  // A single saturated tyre gets a modest load-weighted reduction; all tyres
  // at the limit and genuine lockup retain the full-strength brake cue.
  *(float*)(wheels[1]+0x164) = 1000;
  assert(RumbleReadSlip(0,spin,brake) && fabsf(brake-.875f)<.0001f);
  *(float*)(wheels[1]+0x124) = 1000;
  assert(RumbleReadSlip(0,spin,brake) && brake == 1);
  *(float*)(wheels[1]+0x124) = 0;
  *(float*)(wheels[1]+0x164) = 0;
  // Mild braking, or saturated cornering with little longitudinal force,
  // must stay quiet despite high combined force utilization.
  *(float*)(wheels[0]+0x170) = .25f;
  assert(RumbleReadSlip(0,spin,brake) && !brake);
  *(float*)(wheels[0]+0x170) = .7f;
  assert(RumbleReadSlip(0,spin,brake) && brake > .35f && brake < .37f);
  *(float*)(wheels[0]+0x170) = .8f;
  assert(RumbleReadSlip(0,spin,brake) && brake > .53f && brake < .55f);
  *(float*)(wheels[0]+0x170) = .9f;
  assert(RumbleReadSlip(0,spin,brake) && brake > .74f && brake < .76f);
  *(float*)(wheels[0]+0x170) = 1;
  *(float*)(wheels[0]+0x124) = 100;
  *(float*)(wheels[0]+0x128) = 1000;
  assert(RumbleReadSlip(0,spin,brake) && !brake);
  *(float*)(wheels[0]+0x124) = 0;
  *(float*)(wheels[0]+0x128) = 0;
  *(float*)(wheels[0]+0x164) = 0;
  // Powered cornering at the traction limit can have almost no wheel-speed
  // mismatch (as in the capture). Unpowered or engine-braking wheels stay out.
  *(float*)(wheels[0]+0x164) = 6;
  *(float*)(wheels[0]+0x124) = 3.172f;
  *(float*)(wheels[0]+0x128) = -5.486f;
  *(float*)(wheels[0]+0x108) = 1;
  assert(RumbleReadSlip(0,spin,brake) && spin > .69f && spin < .70f);
  // Saturation dominated by lateral force must remain a gentle throttle cue.
  *(float*)(wheels[0]+0x128) = 12;
  assert(RumbleReadSlip(0,spin,brake) && spin > .43f && spin < .44f);
  *(float*)(wheels[0]+0x128) = 0;
  assert(RumbleReadSlip(0,spin,brake) && fabsf(spin-1.0f)<.0001f);
  *(float*)(wheels[0]+0x170) = .5f;
  assert(RumbleReadSlip(0,spin,brake) && !spin);
  *(float*)(wheels[0]+0x170) = .75f;
  assert(RumbleReadSlip(0,spin,brake) && fabsf(spin-.4261493f)<.0001f);
  *(float*)(wheels[0]+0x170) = .9f;
  assert(RumbleReadSlip(0,spin,brake) && fabsf(spin-.8043267f)<.0001f);
  *(float*)(wheels[0]+0x170) = 1;
  // A lightly loaded saturated inside tyre must not dominate the force cue.
  *(float*)(wheels[1]+0x108) = 1;
  *(float*)(wheels[1]+0x164) = 18;
  *(float*)(wheels[1]+0x170) = .5f;
  *(float*)(wheels[1]+0x124) = 3;
  assert(RumbleReadSlip(0,spin,brake) && fabsf(spin-.4261493f)<.0001f);
  *(float*)(wheels[0]+0x28) = 20/.3f;
  assert(RumbleReadSlip(0,spin,brake) && fabsf(spin-1.0f)<.0001f); // real wheelspin still wins
  *(float*)(wheels[0]+0x28) = 10/.3f;
  *(float*)(wheels[1]+0x108) = 0;
  *(float*)(wheels[1]+0x164) = 0;
  *(float*)(wheels[1]+0x124) = 0;
  *(float*)(wheels[1]+0x170) = 1;
  *(float*)(wheels[0]+0x108) = 0;
  assert(RumbleReadSlip(0,spin,brake) && !spin);
  *(float*)(wheels[0]+0x108) = -1;
  assert(RumbleReadSlip(0,spin,brake) && !spin);
  *(float*)(wheels[0]+0x108) = 0;
  *(float*)(wheels[0]+0x124) = 0;
  *(float*)(wheels[0]+0x128) = 0;
  *(float*)(wheels[0]+0x164) = 0;
  // Steered front wheel: sideways world motion is not rolling-speed deficit.
  *(float*)(wheels[0]+0x70) = 0; *(float*)(wheels[0]+0x78) = 1;
  *(float*)(wheels[0]+0xF0) = 20; *(float*)(wheels[0]+0xF8) = 10;
  assert(RumbleReadSlip(0,spin,brake) && spin < .01f && !brake);
  // Mid-range rotational slip builds more gently than the former linear cue.
  *(float*)(wheels[0]+0x28) = 12.7f/.3f;
  assert(RumbleReadSlip(0,spin,brake) && fabsf(spin-.4375f)<.0001f && !brake);
  // Powered front-wheel spin in the same corner must remain detectable.
  *(float*)(wheels[0]+0x28) = 15/0.3f;
  assert(RumbleReadSlip(0,spin,brake) && fabsf(spin-1.0f)<.0001f && !brake);
  *(float*)(wheels[0]+0x70) = 1; *(float*)(wheels[0]+0x78) = 0;
  *(float*)(wheels[0]+0xF0) = 10; *(float*)(wheels[0]+0xF8) = 0;
  *(float*)(wheels[0]+0x28) = 10/0.3f;
  // Captured gripping sample: X rolling speed matches tyre speed; Z does not.
  // Use the captured axis projections in an identity wheel orientation.
  *(float*)(wheels[0]+0x90) = 0; *(float*)(wheels[0]+0x98) = 1;
  *(float*)(tyres[0]+0x10) = .29775f;
  *(float*)(wheels[0]+0x28) = 145.78094f;
  *(float*)(wheels[0]+0xF0) = 43.44373f;
  *(float*)(wheels[0]+0xF8) = .18243f;
  assert(RumbleReadSlip(0,spin,brake) && !spin && !brake);
  // Captured rear-wheel lockup: zero rotation at 6.75 m/s must reach full cue.
  *(float*)(wheels[0]+0x28) = 0;
  *(float*)(wheels[0]+0xF0) = 6.75383f;
  assert(RumbleReadSlip(0,spin,brake) && !spin && brake == 1);
  *(float*)(tyres[0]+0x10) = .3f;
  *(float*)(wheels[0]+0x28) = 10/.3f;
  *(float*)(wheels[0]+0xF0) = 10; *(float*)(wheels[0]+0xF8) = 0;
  // One spinning front wheel must not be cancelled by its slower partner.
  *(float*)(wheels[0]+0x28) = 15/0.3f;
  *(float*)(wheels[1]+0x28) = 7/0.3f;
  assert(RumbleReadSlip(0,spin,brake) && fabsf(spin-1.0f)<.0001f && !brake);
  // Moderate rolling-speed differences during turns are not lockup.
  *(float*)(wheels[0]+0x28) = 7/0.3f;
  assert(RumbleReadSlip(0,spin,brake) && !brake);
  *(float*)(wheels[2]+0x28) = *(float*)(wheels[3]+0x28) = 20/0.3f;
  assert(RumbleReadSlip(0,spin,brake) && fabsf(spin-1.0f)<.0001f);
  *(float*)(wheels[0]+0x28) = 0;
  *(float*)(wheels[0]+0x170) = 0; // lost tyre force does not mean airborne
  assert(RumbleReadSlip(0,spin,brake) && brake > 0.9f);
  *(float*)(wheels[0]+0x28) = 2/0.3f; // severe slip need not reach exactly zero rotation
  assert(RumbleReadSlip(0,spin,brake) && brake > .99f);
  for (int i = 0; i < 4; ++i) *(unsigned char*)(wheels[i]+0x11C) = 0;
  assert(RumbleReadSlip(0,spin,brake) && !spin && !brake);
  for (int i = 0; i < 4; ++i) {
    *(float*)(wheels[i]+0x170) = 1; *(float*)(wheels[i]+0x28) = -10/0.3f;
    *(unsigned char*)(wheels[i]+0x11C) = 1;
  }
  assert(RumbleReadSlip(0,spin,brake) && spin < 0.01f && brake < 0.01f);
  // The final frame output must not impose the former 90% brake limit.
  *(float*)(wheels[0]+0x28) = 0;
  EventStates[0][JOY_EVENT_BRAKE_ANALOG] = 255;
  RumbleTriggerMode = kTriggersAuto;
  RumblePorts[0].brakeLevel = 1;
  RumbleFrame();
  assert(RumblePorts[0].wanted[2] == 65535);
  *(float*)(wheels[1]+0x28) = 20/.3f;
  RumblePorts[0].gasLevel = 1;
  RumbleFrame();
  assert(RumblePorts[0].wanted[3] == 65535);
  EventStates[0][JOY_EVENT_BRAKE_ANALOG] = 0;
  RumbleTriggerMode = kTriggersOff;
  RumbleStopAllPads();
  *(char**)(car+0x34) = (char*)1;
  assert(!RumbleReadSlip(0,spin,brake));
  // Sustained native levels never acquire a fade; impacts do not duck other motors.
  RumbleMixer mix = {};
  for (int i = 0; i < 100; ++i) { mix.Begin(); mix.Add(kRumbleTyres,1,0.8f); }
  int l, r; mix.Resolve(l,r); assert(l == 65535 && r > 52000);
  mix.Add(kRumbleImpacts,0.5f,0); mix.Resolve(l,r); assert(l == 65535 && r > 52000);
  mix.Begin(); mix.Add(kRumbleImpacts,1,0);
  bRumbleSwapMotors = true; mix.Resolve(l,r); assert(!l && r == 65535);
  bRumbleSwapMotors = false;
  fRumbleCategoryScale[kRumbleImpacts] = 0; mix.Begin(); mix.Add(kRumbleImpacts,1,1);
  mix.Resolve(l,r); assert(!l && !r);
  assert(RumbleMixer::ToMotor(100,2) == 65535 && RumbleClamp(INFINITY,0,2) == 0);
  puts("PASS: original native scheduler/timing, output hook, motor thresholds, routing, stops and slip fixtures");
}
