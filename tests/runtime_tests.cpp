// Exercise the same runtime for every target without a game or controller.
#define NOMINMAX
#include <windows.h>
#include <XInput.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstddef>

XINPUT_STATE samples[4] = {};
bool connected[4] = {};
SHORT asyncKeys[256] = {};
int motorStops = 0;
int padReads = 0;
RAWINPUT rawSample = {};
UINT rawSize = offsetof(RAWINPUT, data) + sizeof(RAWKEYBOARD);
DWORD WINAPI ReadPad(DWORD port, XINPUT_STATE* state) {
  assert(port < 4);
  ++padReads;
  *state = samples[port];
  return connected[port] ? ERROR_SUCCESS : ERROR_DEVICE_NOT_CONNECTED;
}
DWORD WINAPI WritePad(DWORD, XINPUT_VIBRATION* value) {
  if (!value->wLeftMotorSpeed && !value->wRightMotorSpeed) ++motorStops;
  return ERROR_SUCCESS;
}
SHORT WINAPI ReadKey(int key) { return asyncKeys[key]; }
UINT WINAPI ReadRaw(HRAWINPUT, UINT, LPVOID data, PUINT size, UINT) {
  if (!data) { *size = rawSize; return 0; }
  assert(*size == rawSize);
  memcpy(data, &rawSample, rawSize);
  return rawSize;
}
#define XInputGetState ReadPad
#define XInputSetState WritePad
#define GetAsyncKeyState ReadKey
#define GetRawInputData ReadRaw
#include "../src/Main.hpp"

LRESULT WINAPI OriginalWindow(HWND, UINT, WPARAM, LPARAM) { return 77; }

void CheckSettings() {
  InputConfig config;
  // Each test binary lives under build/tests and owns its adjacent INI.
#if defined(GAME_UG) || defined(GAME_UG2)
  DeleteFileA(config.GetIniPath()); // Remove only this test executable's own INI.
  assert(!config.EnsureDefault(999)); // absent resource must not create an empty INI
  assert(GetFileAttributesA(config.GetIniPath()) == INVALID_FILE_ATTRIBUTES);
  InitConfig(); // Startup must create the missing INI, not just expose a helper.
  assert(GetFileAttributesA(config.GetIniPath()) != INVALID_FILE_ATTRIBUTES);
  assert(config.ReadFloat("Rumble","LeftMotorScale",0)==1.0f);
  assert(!strcmp(config.ReadString("Events","JOY_EVENT_THROTTLE_ANALOG",""),"XINPUT_GAMEPAD_RT"));
  assert(*config.ReadString("EventsKB","JOY_EVENT_THROTTLE",""));
  config.WriteString("Rumble","LeftMotorScale","0.7");
  InitConfig();
  assert(config.ReadFloat("Rumble","LeftMotorScale",0)==0.7f);
  assert(DeleteFileA(config.GetIniPath()));
#endif
  config.WriteString("Input", "KeyboardReadingMode", "0");
  config.WriteString("Input", "XInputOmniMode", "0");
  config.WriteString("Deadzone", INPUT_SCANNERS ? "DeadzonePercentLS" : "PercentLS", "0.25");
  config.WriteString("Deadzone", INPUT_SCANNERS ? "DeadzonePercentLS_P2" : "PercentLS_P2", "0.75");
  config.WriteString("Deadzone", INPUT_SCANNERS ? "DeadzonePercent_AnalogTriggerDigital" : "Percent_AnalogTriggerDigital", "0.2");
  config.WriteString("Rumble", "Enabled", "0");
  config.WriteString("Test", "Invalid", "NaN");
  config.WriteString("Test", "Empty", "bad");
  assert(config.ReadFloat("Test", "Invalid", 7) == 7);
  assert(config.ReadFloat("Test", "Empty", 9) == 9);
#if defined(GAME_UG) || defined(GAME_UG2)
  assert(config.EnsureDefault());
  assert(config.ReadInteger("Rumble","Enabled",1)==0); // existing customization survives
#endif
  InitConfig();
  assert(INPUT_DEADZONE_LS == (WORD)(0.25f * 32767));
  assert(INPUT_DEADZONE_LS_P2 == (WORD)(0.75f * 32767));
  assert(TRIGGER_ACTIVATION_THRESHOLD == 51);
  assert(ConvertVKNameToValue("a") == 'A');
  assert(DeleteFileA(config.GetIniPath()));
}

void CheckPolling() {
  connected[0] = connected[1] = true;
  samples[0].Gamepad.sThumbLX = 100;
  samples[0].Gamepad.sThumbLY = 10000;
  samples[1].Gamepad.sThumbLX = 10000;
  UpdateControllerState();
  assert(g_Controllers[0].bConnected && g_Controllers[1].bConnected);
  assert(g_Controllers[0].state.Gamepad.sThumbLX == (INPUT_SCANNERS ? 100 : 0));
  assert(g_Controllers[1].state.Gamepad.sThumbLX == 0);
  assert(LastControlledDevice == LASTCONTROLLED_CONTROLLER);
  connected[0] = connected[1] = false;
  UpdateControllerState();
  assert(!g_Controllers[0].bConnected && !g_Controllers[1].bConnected);
  assert(!InputPadActive(g_Controllers[0].state.Gamepad));
  assert(!InputPadActive(g_Controllers[1].state.Gamepad));
  bXInputOmniMode = true;
  connected[2] = true;
  samples[2].Gamepad.wButtons = XINPUT_GAMEPAD_A;
  UpdateControllerState();
  assert(g_Controllers[0].bConnected && g_Controllers[1].bConnected);
  assert(g_Controllers[0].state.Gamepad.wButtons == XINPUT_GAMEPAD_A);
  assert(g_Controllers[1].state.Gamepad.wButtons == XINPUT_GAMEPAD_A);
  connected[2] = false;
  UpdateControllerState();
  assert(!g_Controllers[0].bConnected && !g_Controllers[1].bConnected);
  assert(!InputPadActive(g_Controllers[0].state.Gamepad));
  bXInputOmniMode = false;
  int before = padReads;
  SetPollingState(false);
  ReadControllerData();
  assert(padReads == before);
  SetPollingState(true);
  BlockNextPoll();
  ReadControllerData();
  assert(padReads == before && !GetBlockNextPoll());
}

void CheckKeyboard() {
  KeyboardReadingMode = KB_READINGMODE_UNBUFFERED_RAW;
  bAllowTwoPlayerKB = true;
  RAWINPUT raw = {};
  raw.header.dwType = RIM_TYPEKEYBOARD;
  raw.header.hDevice = (HANDLE)1;
  raw.data.keyboard.VKey = 'A';
  InputRawKeyboard(raw);
  assert(InputKeyPressed(0, 'A') && !InputKeyPressed(1, 'A'));
  raw.header.hDevice = (HANDLE)2;
  raw.data.keyboard.VKey = VK_CONTROL;
  raw.data.keyboard.Flags = RI_KEY_E0;
  InputRawKeyboard(raw);
  assert(InputKeyPressed(1, VK_RCONTROL) && !InputKeyPressed(0, VK_RCONTROL));
  raw.data.keyboard.Flags |= RI_KEY_BREAK;
  InputRawKeyboard(raw);
  assert(!InputKeyPressed(1, VK_RCONTROL));
  rawSample.header.dwType = RIM_TYPEKEYBOARD;
  rawSample.header.hDevice = FirstKB;
  rawSample.data.keyboard.VKey = 'Z';
  // A keyboard packet is smaller than sizeof(RAWINPUT), whose union also holds a mouse.
  assert(rawSize < sizeof(RAWINPUT));
  assert(CustomWndProc(NULL, WM_INPUT, 0, 0) == 0);
  assert(InputKeyPressed(0, 'Z'));
  rawSample.data.keyboard.Flags = RI_KEY_BREAK;
  CustomWndProc(NULL, WM_INPUT, 0, 0);
  assert(!InputKeyPressed(0, 'Z'));
  assert(!InputKeyPressed(0, 256));
  KeyboardReadingMode = KB_READINGMODE_BUFFERED;
  VKeyStates[0]['A'] = 1;
  assert(!InputKeyPressed(0, 'A'));
  VKeyStates[0]['A'] = 0x80;
  assert(InputKeyPressed(1, 'A'));
  KeyboardReadingMode = KB_READINGMODE_UNBUFFERED_ASYNC;
  asyncKeys['B'] = (SHORT)0x8000;
  assert(InputKeyPressed(0, 'B') && KB_GetCurrentPressedKey() == 'B');
  asyncKeys['B'] = 0;
}

void CheckBindings() {
  XINPUT_GAMEPAD pad = {};
  pad.bLeftTrigger = 51;
  assert(!InputAnalogPressed(pad, XINPUT_GAMEPAD_LT_CONFIGDEF, 16000, TRIGGER_ACTIVATION_THRESHOLD));
  pad.bLeftTrigger = 52;
  assert(InputAnalogPressed(pad, XINPUT_GAMEPAD_LT_CONFIGDEF, 16000, TRIGGER_ACTIVATION_THRESHOLD));
  pad.sThumbLX = -16000;
  assert(!InputAnalogPressed(pad, XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF, 16000, 51));
  pad.sThumbLX = -16001;
  assert(InputAnalogPressed(pad, XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF, 16000, 51));
#if INPUT_SCANNERS
  ScannerConfig config = {};
  config.JoyEvent = JOY_EVENT_FENG_SELECT;
  config.param = 0xFF;
  config.BitmaskStuff = XINPUT_GAMEPAD_A;
  auto scan = [&]() { return Scanner_DigitalUpOrDown_XInput(NULL, NULL, 0, &config, NULL); };
  g_Controllers[0].state.Gamepad.wButtons = XINPUT_GAMEPAD_A;
  assert(scan() == (int)(config.JoyEvent | 0xFF00));
  assert(scan() == 0);
  g_Controllers[0].state.Gamepad.wButtons = 0;
  assert(scan() == (int)config.JoyEvent && scan() == 0);
  config.BitmaskStuff = XINPUT_GAMEPAD_LT_CONFIGDEF;
  auto down = [&]() { return Scanner_DigitalDownAnalog_XInput(NULL, NULL, 0, &config, NULL); };
  g_Controllers[0].state.Gamepad.bLeftTrigger = 255;
  assert(down() == (int)config.JoyEvent && down() == 0);
  g_Controllers[0].state.Gamepad.bLeftTrigger = 0;
  assert(down() == 0);
  g_Controllers[0].state.Gamepad.bLeftTrigger = 255;
  assert(down() == (int)config.JoyEvent); // release rearms the next press
  config.JoyEvent = JOY_EVENT_THROTTLE_ANALOG;
  for (int value = 0; value <= 255; ++value) {
    g_Controllers[0].state.Gamepad.bLeftTrigger = (BYTE)value;
    assert(Scanner_Analog(NULL, NULL, 0, &config, NULL) == (int)(config.JoyEvent | value << 8));
  }
#endif
}

int main() {
  GameWndProc = OriginalWindow;
  CheckSettings();
  CheckPolling();
  CheckKeyboard();
  CheckBindings();
#if INPUT_SCANNERS
  bRumbleInstalled = true;
  RumblePorts[0].sent[0] = 100;
#endif
#if INPUT_SCANNERS || defined(GAME_MW) || defined(GAME_PROSTREET)
  bRumbleEnabled = true;
#endif
  assert(CustomWndProc(NULL, WM_KILLFOCUS, 0, 0) == 77);
#if INPUT_SCANNERS || defined(GAME_MW) || defined(GAME_PROSTREET)
  assert(motorStops > 0);
#endif
  puts("Shared runtime checks passed.");
}
