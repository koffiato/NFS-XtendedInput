// Integration checks against the supported executable's mapped feedback code/data.
// No game code or controller hardware is executed: output and the frame callback are mocked.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <XInput.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <fstream>
#include <vector>
#include "../includes/injector/injector.hpp"

#define MAX_CONTROLLERS 4
// A private image buffer allows the harness to run at any Windows load address.
// Only the feedback data and hook bytes are copied; game callbacks are mocked.
#pragma section(".game", read, write)
__declspec(allocate(".game")) char gameArena[0x600000] = {};
#define RUMBLE_IMAGE_BASE ((uintptr_t)gameArena)
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
struct CIniReader {
  CIniReader(const char*) {}
  int ReadInteger(const char*, const char* key, int fallback) {
    if (!strcmp(key,"Enabled")) return enabled;
    if (!strcmp(key,"ImpulseTriggers")) return 0;
    return fallback;
  }
  float ReadFloat(const char*, const char*, float fallback) { return fallback; }
  const char* GetIniPath() { return "rumble-tests.ini"; }
};
#define GetForegroundWindow TestForeground
#define IsIconic TestIconic
#define XInputSetState TestSetState
#define XInputGetState TestGetState
#include "../NFSU_XtendedInput_Rumble.h"

void LoadImage(const char* path) {
  std::ifstream stream(path, std::ios::binary);
  assert(stream.good());
  std::vector<char> image((std::istreambuf_iterator<char>(stream)), {});
  auto* dos = (IMAGE_DOS_HEADER*)image.data();
  auto* nt = (IMAGE_NT_HEADERS*)(image.data()+dos->e_lfanew);
  assert(nt->OptionalHeader.ImageBase == 0x400000);
  assert(nt->OptionalHeader.SizeOfImage <= sizeof(gameArena));
  DWORD oldProtect;
  assert(VirtualProtect(gameArena,sizeof(gameArena),PAGE_EXECUTE_READWRITE,&oldProtect));
  auto* sec = IMAGE_FIRST_SECTION(nt);
  for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
    if (!strcmp((const char*)sec[i].Name,".rdata") || !strcmp((const char*)sec[i].Name,".data"))
      memcpy(gameArena+sec[i].VirtualAddress,image.data()+sec[i].PointerToRawData,sec[i].SizeOfRawData);
  }
  uintptr_t sites[] = {RUMBLE_EVENT_ADDR,RUMBLE_UPDATE_CALL,RUMBLE_UPDATE_ADDR,
    RUMBLE_SETTING_ADDR,RUMBLE_COLLISION_SETTING_ADDR-7,RUMBLE_PACKAGE_ACTIVE_ADDR};
  for (uintptr_t site : sites) {
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
      uintptr_t start = RUMBLE_IMAGE_BASE+sec[i].VirtualAddress;
      if (site >= start && site+32 <= start+sec[i].SizeOfRawData)
        memcpy((void*)site,image.data()+sec[i].PointerToRawData+site-start,32);
    }
  }
  // The copied event records contain pointers into the original fixed image.
  // Rebase only those data pointers; copied instructions are never executed.
  uintptr_t delta = RUMBLE_IMAGE_BASE - nt->OptionalHeader.ImageBase;
  std::vector<RumbleNativeCurve*> curves;
  for (int i = 0; i < RUMBLE_EVENT_COUNT; ++i) {
    auto& event = ((RumbleNativeEvent*)RUMBLE_EVENT_TABLE_ADDR)[i];
    event.params = (const float*)((uintptr_t)event.params + delta);
    auto* curve = (RumbleNativeCurve*)((uintptr_t)event.curve + delta);
    event.curve = curve;
    if (std::find(curves.begin(),curves.end(),curve) == curves.end()) {
      curve->points = (const RumbleEnvelopePoint*)((uintptr_t)curve->points + delta);
      curves.push_back(curve);
    }
  }
}

int main(int argc, char** argv) {
  assert(argc == 2);
  LoadImage(argv[1]);
  assert(RumbleValidateHooks());
  unsigned char original = *(unsigned char*)RUMBLE_UPDATE_CALL;
  *(unsigned char*)RUMBLE_UPDATE_CALL = 0x90;
  assert(!RumbleValidateHooks());
  RumbleInit(); // mismatch must install none of the rumble patches
  assert(!bRumbleInstalled && *(unsigned char*)RUMBLE_EVENT_ADDR == 0x8B);
  *(unsigned char*)RUMBLE_UPDATE_CALL = original;
  enabled = false;
  RumbleInit();
  assert(!bRumbleInstalled && *(unsigned char*)RUMBLE_EVENT_ADDR == 0x8B);
  enabled = true;
  RumbleInit();
  assert(bRumbleInstalled && *(unsigned char*)RUMBLE_EVENT_ADDR == 0xE9);
  assert(*(unsigned short*)RUMBLE_SETTING_ADDR == 0x01B3);
  assert(*(unsigned char*)(RUMBLE_SETTING_ADDR+2) == 0x90);
  assert(*(unsigned short*)RUMBLE_COLLISION_SETTING_ADDR == 0x9090);
  // Frame and package calls use tiny return stubs rather than executing the game.
  *(unsigned char*)RUMBLE_UPDATE_ADDR = 0xC3;
  unsigned char notPaused[] = {0xB0,0,0xC3};
  memcpy((void*)RUMBLE_PACKAGE_ACTIVE_ADDR,notPaused,sizeof(notPaused));
  *(HWND*)GAME_HWND_ADDR = (HWND)0x1234;
  *(int*)GAMEFLOWMANAGER_STATUS_ADDR = 6;
  *(int*)RUMBLE_PLAYER_COUNT_ADDR = 0;
  *(float*)RUMBLE_SIM_DT_ADDR = 1.0f/120;
  g_Controllers[0].bConnected = g_Controllers[1].bConnected = true;
  assert(RumbleCanPlay());
  // Dispatcher patch has the right ABI, and port 1 never drives port 0.
  ((void(__cdecl*)(int,int,float))RUMBLE_EVENT_ADDR)(0,1,11.0f);
  RumbleFrame();
  assert(motors[1].wLeftMotorSpeed > 0 && !motors[0].wLeftMotorSpeed);
  for (int i = 0; i < 120; ++i) RumbleFrame();
  assert(!motors[1].wLeftMotorSpeed && !motors[1].wRightMotorSpeed);
  RumbleEvent(-1,0,1); RumbleEvent(12,0,1); RumbleEvent(0,-1,1); RumbleEvent(0,2,1);
  RumbleEvent(4,0,1); // disabled native tyre event stays disabled
  assert(!RumblePorts[0].event[4].pPoints);
  RumbleEvent(0,0,11); RumbleFrame();
  assert(motors[0].wLeftMotorSpeed > 0);
  focused = false;
  RumblePoll();
  assert(!motors[0].wLeftMotorSpeed && !RumblePorts[0].event[0].pPoints);
  focused = true;
  // Pause, frozen simulation, menu transitions and missed feedback frames stop output.
  RumbleEvent(0,0,11); RumbleFrame();
  *((unsigned char*)RUMBLE_PACKAGE_ACTIVE_ADDR+1) = 1;
  RumblePoll(); assert(!motors[0].wLeftMotorSpeed);
  *((unsigned char*)RUMBLE_PACKAGE_ACTIVE_ADDR+1) = 0;
  RumbleEvent(0,0,11); RumbleFrame();
  *(float*)RUMBLE_SIM_DT_ADDR = 0;
  RumbleFrame(); assert(!motors[0].wLeftMotorSpeed);
  *(float*)RUMBLE_SIM_DT_ADDR = 1.0f/120;
  RumbleEvent(0,0,11); RumbleFrame();
  *(int*)GAMEFLOWMANAGER_STATUS_ADDR = 3;
  RumblePoll(); assert(!motors[0].wLeftMotorSpeed);
  *(int*)GAMEFLOWMANAGER_STATUS_ADDR = 6;
  RumbleEvent(0,0,11); RumbleFrame();
  RumbleLastFrame -= 1;
  RumblePoll(); assert(!motors[0].wLeftMotorSpeed);
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
  }
  float spin, brake;
  assert(RumbleReadSlip(0,spin,brake) && spin < 0.01f && brake < 0.01f);
  *(float*)(wheels[2]+0x28) = *(float*)(wheels[3]+0x28) = 20/0.3f;
  assert(RumbleReadSlip(0,spin,brake) && spin > 0.9f);
  *(float*)(wheels[0]+0x28) = 0;
  assert(RumbleReadSlip(0,spin,brake) && brake > 0.9f);
  for (int i = 0; i < 4; ++i) *(float*)(wheels[i]+0x170) = 0;
  assert(RumbleReadSlip(0,spin,brake) && !spin && !brake);
  for (int i = 0; i < 4; ++i) {
    *(float*)(wheels[i]+0x170) = 1; *(float*)(wheels[i]+0x28) = -10/0.3f;
  }
  assert(RumbleReadSlip(0,spin,brake) && spin < 0.01f && brake < 0.01f);
  *(char**)(car+0x34) = (char*)1;
  assert(!RumbleReadSlip(0,spin,brake));
  // Fatigue leaves pulses intact, and category muting/swap/clamp work in the shared mixer.
  RumbleMixer mix = {}; RumbleTextureState texture = {};
  for (int i = 0; i < 100; ++i) { mix.Begin(0.1f); mix.AddTexture(kRumbleTyres,texture,1,0); }
  int l, r; mix.Resolve(l,r); assert(l > 39000 && l < 40000);
  mix.AddPulse(kRumbleImpacts,1,0); mix.Resolve(l,r); assert(l == 65535);
  bRumbleSwapMotors = true; mix.Resolve(l,r); assert(!l && r == 65535);
  bRumbleSwapMotors = false;
  fRumbleCategoryScale[kRumbleImpacts] = 0; mix.Begin(0.01f); mix.AddPulse(kRumbleImpacts,1,1);
  mix.Resolve(l,r); assert(!l && !r);
  assert(RumbleMixer::ToMotor(100,2) == 65535 && RumbleClamp(INFINITY,0,2) == 0);
  puts("PASS: hooks, native curves, routing, lifecycle, pedal maps, U2 telemetry, mixer");
}
