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
#define GAME_UG
#define GAME_HWND_ADDR (RUMBLE_IMAGE_BASE + 0x00336380)
#define GAMEFLOWMANAGER_STATUS_ADDR (RUMBLE_IMAGE_BASE + 0x0037A920)
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

void __declspec(naked) NativeEvent(int type, int port, float amount) {
  __asm {
    push esi
    mov edx, [esp+8]
    mov esi, [esp+12]
    shl edx, 4
    add edx, 0x7045A8
    push dword ptr [esp+16]
    mov eax, 0x574D90
    call eax
    add esp, 4
    pop esi
    ret
  }
}
void __declspec(naked) NativeFrame() {
  __asm {
    pushad
    mov eax, 0x4479D8
    call eax
    popad
    ret
  }
}
bool Near(float a, float b) { return fabsf(a-b) < .00001f; }
void __declspec(naked) NativeVibrationToggle(int port) {
  __asm {
    pushad
    mov eax, [esp+36]
    mov edx, 0x4BF0E2
    call edx
    popad
    ret
  }
}
float Sample(const RumbleNativeCurve* curve, float age) {
  if (age < curve->points[0].fTime || age >= curve->points[curve->count-1].fTime) return 0;
  for(int i=1;i<curve->count;++i) {
    const auto& a=curve->points[i-1]; const auto& b=curve->points[i];
    if(age < b.fTime) return a.fValue+(b.fValue-a.fValue)*(age-a.fTime)/(b.fTime-a.fTime);
  }
  return 0;
}
unsigned seed = 0x12345678;
unsigned Random() { seed=1664525*seed+1013904223; return seed; }
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

  LoadImage(argv[1]);
  assert(RumbleValidateHooks());
  // A modified sampler loop must refuse all writes.
  *(unsigned char*)0x4479E9 ^= 1;
  RumbleInit(); assert(!bRumbleInstalled);
  *(unsigned char*)0x4479E9 ^= 1;
  enabled = false; RumbleInit(); assert(!bRumbleInstalled);
  enabled = true; RumbleInit(); assert(bRumbleInstalled && bRumbleNativeReady);
  assert(*(unsigned char*)RUMBLE_EVENT_ADDR == 0x57);
  assert(*(unsigned short*)RUMBLE_SETTING_ADDR == 0x950F);
  assert(*(unsigned short*)RUMBLE_COLLISION_SETTING_ADDR == 0x1574);
  assert(*(unsigned short*)RUMBLE_AUX_SETTING_ADDR == 0x3974);
  // Return after the real native loop and the real hooked housekeeping call.
  // This private-image-only change never runs the game's main entry point.
  *(unsigned char*)0x447A01 = 0xC3;
  *(int*)0x7345C8 = 0;
  *(void**)0x73B230 = *(void**)0x73B250 = NULL;
  static int race = 1;
  *(int**)0x7361F0 = &race;
  *(int*)GAMEFLOWMANAGER_STATUS_ADDR = 6;
  *(int*)0x6F1D44 = 1;
  *(HWND*)GAME_HWND_ADDR = (HWND)0x1234;
  *(float*)RUMBLE_SIM_DT_ADDR = .075f;
  g_Controllers[0].bConnected = g_Controllers[1].bConnected = true;
  static unsigned char players[2][0x40] = {};
  for(int port=0;port<2;++port) {
    ((unsigned char**)0x7361B4)[port]=players[port];
    players[port][0x21]=1;
  }
  // Run the options handler's actual toggle/profile-store fragment. Return
  // before its frontend refresh calls in this private test image only.
  *(unsigned char*)0x4BF0FD=0xC3;
  auto table = (RumbleNativeEvent*)RUMBLE_EVENT_TABLE_ADDR;
  // Native admission and time scaling, both ports; no C++ event scheduler.
  for (int port=0; port<2; ++port) {
    RumbleStopAllPads();
    NativeEvent(7,port,1);
    auto ch = RumbleChannels(port);
    assert(ch->curve[0] == table[7].curve && ch->gain[0] == 3 && ch->age[0] == 0);
    NativeFrame();
    assert(ch->output == 255 && Near(ch->age[0], .025f));
    assert(motors[port].wLeftMotorSpeed == 65535 && motors[port].wRightMotorSpeed == 65535);
    assert(RumblePorts[port].wanted[2] == 0 && RumblePorts[port].wanted[3] == 0);
    for (int i=0;i<3;++i) NativeFrame();
    assert(ch->output == 60 && Near(ch->age[0], .1f));
    assert(motors[port].wLeftMotorSpeed == RumbleMixer::ToMotor(60/255.0f,1));
    assert(motors[port].wRightMotorSpeed == 0);
  }
  RumbleStopAllPads();
  NativeEvent(4,0,1); assert(!RumbleChannels(0)->curve[0]);
  NativeEvent(0,0,1.99f); assert(!RumbleChannels(0)->curve[0]);
  NativeEvent(0,0,2); assert(Near(RumbleChannels(0)->gain[0],.3f));
  RumbleStopAllPads();
  NativeEvent(0,0,99); assert(Near(RumbleChannels(0)->gain[0],2));
  RumbleStopAllPads();
  for (int i=0;i<3;++i) NativeEvent(1,0,.025f);
  auto ch = RumbleChannels(0);
  assert(ch->curve[0] && ch->curve[1] && ch->curve[2]);
  ch->age[0]=.01f; ch->age[1]=.02f; ch->age[2]=.03f;
  NativeEvent(1,0,.025f);
  assert(Near(ch->age[0],.01f) && Near(ch->age[1],.02f) && Near(ch->age[2],.03f));
  NativeEvent(0,0,11);
  int collision=0; for(int i=0;i<3;++i) collision += ch->curve[i] == table[0].curve;
  assert(collision == 1);
  NativeFrame(); assert(ch->output == 255);
  // Sample endpoint is exclusive; gain controls duration and amplitude.
  RumbleStopAllPads(); NativeEvent(7,0,1);
  ch->age[0] = .2f; NativeFrame(); assert(!ch->curve[0] && ch->output == 0);
  // Dispatcher numeric state guards and master/event switches remain native.
  for(int state : {0,2,3,5,6,8,9,10}) {
    race=state; NativeEvent(7,0,1); assert(!ch->curve[0]);
  }
  for(int state : {1,4,7,11}) {
    race=state; NativeEvent(7,0,1); assert(ch->curve[0]); RumbleStopAllPads();
  }
  race=1; *(int*)0x6F1D44=0; NativeEvent(7,0,1); assert(!ch->curve[0]);
  *(int*)0x6F1D44=1;
  // Compare actual native slot admission with an independent selection model.
  for(int test=0;test<2000;++test) {
    RumbleStopAllPads();
    for(int i=0;i<3;++i) {
      ch->curve[i] = Random()%5 ? table[Random()%12].curve : NULL;
      ch->age[i] = (Random()%100)*.001f;
      ch->gain[i] = .3f+(Random()%270)*.01f;
      ch->priority[i] = (signed char)(Random()%11);
    }
    float input = 2+(Random()%901)*.01f;
    float gain = .3f+(2-.3f)*(input-2)/(11-2);
    int expected=-1, sample=0;
    for(int i=0;i<3;++i) {
      if(ch->priority[i]>8) continue;
      if(!ch->curve[i]) { expected=i; break; }
      if(ch->priority[i]==8 && !(gain>ch->gain[i])) continue;
      int level=(int)(Sample(ch->curve[i],ch->age[i])*ch->gain[i]);
      if(expected<0 || ch->priority[i]<ch->priority[expected] ||
          (ch->priority[i]==ch->priority[expected] && level<=sample)) {
        expected=i; sample=level;
      }
    }
    RumbleNativeChannel before=*ch;
    NativeEvent(0,0,input);
    for(int i=0;i<3;++i) {
      if(i==expected) {
        assert(ch->curve[i]==table[0].curve && ch->age[i]==0 && Near(ch->gain[i],gain) && ch->priority[i]==8);
      } else {
        assert(ch->curve[i]==before.curve[i] && ch->age[i]==before.age[i] && ch->gain[i]==before.gain[i] && ch->priority[i]==before.priority[i]);
      }
    }
  }
  RumbleStopAllPads();
  // Adapter packing boundaries, intensity, scales and swapping, independent of
  // curve timing. RumbleFrame reads a completed native channel; it never resamples.
  for(int level : {0,1,127,128,129,255}) {
    ch->output=level; RumbleFrame();
    assert(motors[0].wLeftMotorSpeed == RumbleMixer::ToMotor(level/255.0f,1));
    assert(motors[0].wRightMotorSpeed == (level>128 ? 65535 : 0));
  }
  bRumbleSwapMotors=true; ch->output=128; RumbleFrame();
  assert(motors[0].wLeftMotorSpeed==0 && motors[0].wRightMotorSpeed>0);
  bRumbleSwapMotors=false; fRumbleIntensity=.5f; fRumbleLeftScale=.5f;
  ch->output=255; RumbleFrame();
  assert(motors[0].wLeftMotorSpeed==RumbleMixer::ToMotor(1,.5f));
  fRumbleIntensity=fRumbleLeftScale=1;
  // Pause, focus, disconnect, invalid dt, stale polls: stop and clear slots.
  for(int reason=0; reason<6; ++reason) {
    RumbleStopAllPads(); NativeEvent(7,0,1); NativeFrame();
    assert(motors[0].wLeftMotorSpeed && ch->curve[0]);
    if(reason==0) *(float*)RUMBLE_SIM_DT_ADDR=0;
    if(reason==1) focused=false;
    if(reason==2) g_Controllers[0].bConnected=false;
    if(reason==3) *(float*)RUMBLE_SIM_DT_ADDR=NAN;
    if(reason==4) *(float*)RUMBLE_SIM_DT_ADDR=.2f;
    if(reason==5) { RumbleLastFrame=RumbleGetTime()-1; RumblePoll(); }
    else RumbleFrame();
    assert(!motors[0].wLeftMotorSpeed && !motors[0].wRightMotorSpeed);
    assert(!ch->curve[0] && ch->output==0);
    *(float*)RUMBLE_SIM_DT_ADDR=.075f; focused=true; g_Controllers[0].bConnected=true;
    NativeFrame(); assert(!motors[0].wLeftMotorSpeed);
  }
  RumbleStopAllPads(true);
  // Each player's menu toggle stops active output and clears only that port.
  // Re-enabling cannot replay the cleared effect; fresh events play normally.
  for(int port=0;port<2;++port) {
    for(int other=0;other<2;++other) NativeEvent(7,other,1);
    NativeFrame();
    assert(motors[0].wLeftMotorSpeed && motors[1].wLeftMotorSpeed);
    NativeVibrationToggle(port);
    assert(!players[port][0x21] && !*(unsigned char*)(0x78A385+port*0x38));
    RumblePoll();
    assert(!motors[port].wLeftMotorSpeed && !motors[port].wRightMotorSpeed);
    assert(!RumbleChannels(port)->curve[0]);
    assert(motors[1-port].wLeftMotorSpeed && RumbleChannels(1-port)->curve[0]);
    // Even a direct dispatch cannot reach hardware while the setting is off.
    NativeEvent(7,port,1); NativeFrame();
    assert(!motors[port].wLeftMotorSpeed && !RumbleChannels(port)->curve[0]);
    NativeVibrationToggle(port);
    assert(players[port][0x21] && *(unsigned char*)(0x78A385+port*0x38));
    NativeFrame(); assert(!motors[port].wLeftMotorSpeed);
    NativeEvent(7,port,1); NativeFrame(); assert(motors[port].wLeftMotorSpeed);
    RumbleStopAllPads();
  }
  ((unsigned char**)0x7361B4)[0]=NULL;
  RumblePoll(); assert(!RumbleFeedbackEnabled(0));
  puts("PASS UG1 native dispatch, admission, frame timing, output mapping and lifecycle");
  return 0;
}
