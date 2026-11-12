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

void CheckDefault(InputConfig& ini, const char* section, const char* key, int expected) {
  assert(ini.ReadInteger(section, key, -9999) == expected);
}
void CheckDefault(InputConfig& ini, const char* section, const char* key, float expected) {
  assert(ini.ReadFloat(section, key, -9999) == expected);
}
void CheckDefault(InputConfig& ini, const char* section, const char* key, const char* expected) {
  assert(!strcmp(ini.ReadString(section, key, "missing"), expected));
}

void CheckGeneratedSettings(InputConfig& ini) {
  using namespace ConfigDefaults;
#define CONFIG_BEGIN(section)
#define CONFIG_END()
#define CONFIG_SECTION(section, note)
#define CONFIG_SETTING(scope, section, key, type, value, comment) { \
  std::string name = (IsScanner && !strcmp(#section, "Deadzone") ? "Deadzone" : "") + std::string(#key); \
  if ((scope) & Target) CheckDefault(ini, #section, name.c_str(), ConfigDefaults::section::key); \
  else assert(!*ini.ReadString(#section, name.c_str(), "")); \
}
#include "../config/settings.def"
#undef CONFIG_SETTING
#undef CONFIG_SECTION
#undef CONFIG_BEGIN
#undef CONFIG_END
}

void CheckSettings() {
  InputConfig config;
  // Each test binary lives under build/tests and owns its adjacent INI.
#if !INPUT_SCANNERS
  InputConfig bindings("NFS_XtendedInput.default.ini");
  DeleteFileA(bindings.GetIniPath());
#endif
  DeleteFileA(config.GetIniPath()); // Remove only this test executable's own INI.
  assert(!config.EnsureDefault(999)); // absent resource must not create an empty INI
  assert(GetFileAttributesA(config.GetIniPath()) == INVALID_FILE_ATTRIBUTES);
  char workingDirectory[MAX_PATH], windowsDirectory[MAX_PATH];
  assert(GetCurrentDirectoryA(MAX_PATH, workingDirectory));
  assert(GetWindowsDirectoryA(windowsDirectory, MAX_PATH));
  assert(SetCurrentDirectoryA(windowsDirectory));
  InitConfig(); // Startup must create the missing INI, not just expose a helper.
  assert(SetCurrentDirectoryA(workingDirectory));
  assert(GetFileAttributesA(config.GetIniPath()) != INVALID_FILE_ATTRIBUTES);
  // First-run creation must preserve the complete generated text and comments.
  HMODULE module = (HMODULE)&__ImageBase;
  HRSRC resource = FindResourceA(module, MAKEINTRESOURCEA(101), RT_RCDATA);
  DWORD resourceSize = SizeofResource(module, resource);
  assert(resource && resourceSize);
  std::vector<char> created(resourceSize);
  FILE* createdFile = fopen(config.GetIniPath(), "rb");
  assert(createdFile && fread(created.data(), 1, created.size(), createdFile) == created.size());
  assert(fgetc(createdFile) == EOF);
  fclose(createdFile);
  assert(created[0] == ';');
  assert(!memcmp(created.data(), LockResource(LoadResource(module, resource)), resourceSize));
  CheckGeneratedSettings(config);
#if defined(GAME_UG2) || defined(HAS_RUMBLE)
  assert(config.ReadInteger("Rumble", "ImpulseTriggers", -1) == 0);
#endif
  assert(INPUT_DEADZONE_LS == (WORD)(0.10f * 32767));
  assert(INPUT_DEADZONE_RS == INPUT_DEADZONE_LS);
  assert(INPUT_DEADZONE_LS_P2 == INPUT_DEADZONE_LS);
  assert(INPUT_DEADZONE_RS_P2 == INPUT_DEADZONE_LS);
#if INPUT_SCANNERS
  assert(!strcmp(config.ReadString("Events","JOY_EVENT_THROTTLE_ANALOG",""),"XINPUT_GAMEPAD_RT"));
  assert(*config.ReadString("EventsKB","JOY_EVENT_THROTTLE",""));
#else
  assert(GetFileAttributesA(bindings.GetIniPath()) != INVALID_FILE_ATTRIBUTES);
  assert(!strcmp(defMappingName, bindings.GetIniPath()));
  bindings.WriteString("Test", "Preserved", "yes");
#endif
  config.WriteString("Rumble","LeftMotorScale","0.7");
  InitConfig();
  assert(config.ReadFloat("Rumble","LeftMotorScale",0)==0.7f);
#if !INPUT_SCANNERS
  assert(!strcmp(bindings.ReadString("Test","Preserved",""),"yes"));
  assert(DeleteFileA(bindings.GetIniPath()));
#endif
  assert(DeleteFileA(config.GetIniPath()));
  HANDLE empty = CreateFileA(config.GetIniPath(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
  assert(empty != INVALID_HANDLE_VALUE);
  CloseHandle(empty);
  InitConfig(); // An existing empty file is also a customization to preserve.
  assert(INPUT_DEADZONE_LS == (WORD)(0.10f * 32767));
  assert(SHIFT_ANALOG_THRESHOLD == (WORD)(ConfigDefaults::Deadzone::Percent_Shifting * 32767));
  WIN32_FILE_ATTRIBUTE_DATA attributes;
  assert(GetFileAttributesExA(config.GetIniPath(), GetFileExInfoStandard, &attributes));
  assert(attributes.nFileSizeHigh == 0 && attributes.nFileSizeLow == 0);
  assert(DeleteFileA(config.GetIniPath()));
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
  assert(config.EnsureDefault());
  assert(config.ReadInteger("Rumble","Enabled",1)==0); // existing customization survives
  InitConfig();
  assert(INPUT_DEADZONE_LS == (WORD)(0.25f * 32767));
  assert(INPUT_DEADZONE_LS_P2 == (WORD)(0.75f * 32767));
  assert(TRIGGER_ACTIVATION_THRESHOLD == 51);
  assert(ConvertVKNameToValue("a") == 'A');
  assert(DeleteFileA(config.GetIniPath()));
#if !INPUT_SCANNERS
  assert(DeleteFileA(bindings.GetIniPath()));
#endif
}

void CheckRumbleSettings() {
#if INPUT_SCANNERS || defined(HAS_RUMBLE)
  RumbleLoadSettings([](const char*, float fallback) { return fallback; });
  assert(RumbleTriggerMode == kTriggersOff);
  assert(!RumbleWGI::ShouldUse() && !RumbleWGI::bTried);
  InputConfig config;
  for (int mode : {kTriggersOn, kTriggersAuto}) {
    config.WriteString("Rumble", "ImpulseTriggers", mode == kTriggersOn ? "1" : "2");
    assert(config.EnsureDefault());
    RumbleLoadSettings([&](const char* key, float fallback) { return config.ReadFloat("Rumble", key, fallback); });
    assert(RumbleTriggerMode == mode);
  }
  assert(DeleteFileA(config.GetIniPath()));
  RumbleLoadSettings([](const char*, float fallback) { return fallback; });
  assert(RumbleTriggerMode == kTriggersOff);
#endif
}

void CheckRemapping() {
#ifdef GAME_UG
  InputConfig config;
  for (unsigned int index : {3u, 4u}) {
    unsigned int event = index == 3 ? JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT : JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT;
    SetUGKeyboardBinding(index, false, 'A');
    assert((index == 3 ? SteerLeftVKey : SteerRightVKey) == 'A');
    assert(ScannerConfigs[event].keycode == 'A');
    assert(!strcmp(config.ReadString("EventsKB", JoyEventNames[event], ""), "A"));
    SetUGKeyboardBinding(index, true, 'B');
    assert(ScannerConfigs[event].keycode == 'A');
    SetUGKeyboardBinding(index, false, 0);
    assert(!ScannerConfigs[event].keycode);
    SetUGGamepadBinding(index, false, XINPUT_GAMEPAD_LT_CONFIGDEF, true);
    assert((index == 3 ? SteerLeftAxis : SteerRightAxis) == XINPUT_GAMEPAD_LT_CONFIGDEF);
    assert(ScannerConfigs[event].BitmaskStuff == XINPUT_GAMEPAD_LT_CONFIGDEF);
    SetUGGamepadBinding(index, true, XINPUT_GAMEPAD_A, false);
    assert((index == 3 ? SteerLeftButton : SteerRightButton) == XINPUT_GAMEPAD_A);
    assert(ScannerConfigs[event].BitmaskStuff == XINPUT_GAMEPAD_LT_CONFIGDEF);
    SetUGGamepadBinding(index, false, 0, false);
    SetUGGamepadBinding(index, true, 0, false);
    assert(!ScannerConfigs[event].BitmaskStuff);
    assert(!(index == 3 ? SteerLeftButton : SteerRightButton));
  }
  for (unsigned int index : {1u, 2u, 5u, 6u, 7u, 8u, 9u, 10u, 12u, 14u}) {
    unsigned int event = MapJoypadConfigToEvent(index);
    SetUGGamepadBinding(index, false, XINPUT_GAMEPAD_LT_CONFIGDEF, true);
    assert(ScannerConfigs[event].BitmaskStuff == XINPUT_GAMEPAD_LT_CONFIGDEF);
    assert(ScannerConfigs[event].ScannerFunctionPointer == (index <= 2
        ? (unsigned int)&Scanner_Analog : ScannerDigitalFunction(event, true)));
    SetUGGamepadBinding(index, true, XINPUT_GAMEPAD_A, false);
    assert(ScannerConfigs[event].BitmaskStuff == XINPUT_GAMEPAD_LT_CONFIGDEF);
    if (index <= 2) {
      unsigned int secondary = MapSecondaryJoypadConfigToEvent(index);
      assert(ScannerConfigs[secondary].BitmaskStuff == XINPUT_GAMEPAD_A);
      assert(ScannerConfigs[secondary].ScannerFunctionPointer == (unsigned int)&Scanner_DigitalUpOrDown);
      SetUGGamepadBinding(index, true, 0, false);
      assert(!ScannerConfigs[secondary].BitmaskStuff);
    }
    SetUGGamepadBinding(index, false, XINPUT_GAMEPAD_B, false);
    assert(ScannerConfigs[event].BitmaskStuff == XINPUT_GAMEPAD_B);
    SetUGGamepadBinding(index, false, 0, false);
    assert(!ScannerConfigs[event].BitmaskStuff);
    unsigned int keyboard = MapKeyboardConfigToEvent(index);
    SetUGKeyboardBinding(index, false, 'B');
    assert(ScannerConfigs[keyboard].keycode == 'B');
    assert(!strcmp(config.ReadString("EventsKB", JoyEventNames[keyboard], ""), "B"));
    SetUGKeyboardBinding(index, false, 0);
    assert(!ScannerConfigs[keyboard].keycode);
  }
  assert(DeleteFileA(config.GetIniPath()));
#endif
}

void CheckProfileFiles() {
#if !INPUT_SCANNERS
  char previousDirectory[MAX_PATH], tempDirectory[MAX_PATH], testDirectory[MAX_PATH];
  char previousDefault[MAX_PATH];
  strcpy_s(previousDefault, defMappingName);
  assert(GetCurrentDirectoryA(MAX_PATH, previousDirectory));
  assert(GetTempPathA(MAX_PATH, tempDirectory));
  assert(GetTempFileNameA(tempDirectory, "xip", 0, testDirectory));
  assert(DeleteFileA(testDirectory) && CreateDirectoryA(testDirectory, NULL));
  assert(SetCurrentDirectoryA(testDirectory));
  assert(CreateDirectoryA("scripts", NULL));
  strcpy_s(defMappingName, "template.ini");
  FILE* fixture = fopen(defMappingName, "w");
  assert(fixture);
  fputs("[Events]\nGAMEACTION_GAS = XINPUT_GAMEPAD_RT\n[EventsKB]\nGAMEACTION_GAS = W\n", fixture);
  fclose(fixture);

  std::string first = EnsureUserMapping("First");
  std::string second = EnsureUserMapping("Second");
  assert(first != second && first != defMappingName);
  mINI::INIFile firstFile(first), secondFile(second);
  mINI::INIStructure ini;
  assert(firstFile.read(ini) && ini["Events"]["GAMEACTION_GAS"] == "XINPUT_GAMEPAD_RT");
  ini["Events"]["GAMEACTION_GAS"] = "XINPUT_GAMEPAD_A";
  assert(firstFile.write(ini));
  assert(EnsureUserMapping("First") == first);
  assert(firstFile.read(ini) && ini["Events"]["GAMEACTION_GAS"] == "XINPUT_GAMEPAD_A");
  assert(secondFile.read(ini) && ini["Events"]["GAMEACTION_GAS"] == "XINPUT_GAMEPAD_RT");
#ifdef GAME_PROSTREET
  gProfileName = "First";
  SaveBindingToIni(GAMEACTION_GAS, false, XINPUT_GAMEPAD_B);
  SaveBindingToIniKB(GAMEACTION_GAS, true, 'A');
  assert(firstFile.read(ini));
  assert(ini["Events_Secondary"]["GAMEACTION_GAS"] == "XINPUT_GAMEPAD_B");
  assert(ini["EventsKB"]["GAMEACTION_GAS"] == "A");
  SaveBindingToIni(GAMEACTION_GAS, false, 0);
  SaveBindingToIniKB(GAMEACTION_GAS, true, 0);
  assert(firstFile.read(ini));
  assert(ini["Events_Secondary"]["GAMEACTION_GAS"].empty() && ini["EventsKB"]["GAMEACTION_GAS"].empty());
  gProfileName.clear();
#endif
  assert(DeleteFileA(defMappingName));
  assert(EnsureUserMapping("MissingTemplate") == defMappingName);
  assert(!PathFileExists(UserMappingPath("MissingTemplate").c_str()));
  assert(DeleteFileA(first.c_str()) && DeleteFileA(second.c_str()));
  for (const char* profile : {"First", "Second", "MissingTemplate"}) {
    std::string directory = std::string(userMappingDir) + '\\' + profile;
    assert(RemoveDirectoryA(directory.c_str()));
  }
  assert(RemoveDirectoryA(userMappingDir) && RemoveDirectoryA("scripts"));
  assert(SetCurrentDirectoryA(previousDirectory) && RemoveDirectoryA(testDirectory));
  strcpy_s(defMappingName, previousDefault);
#endif
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
  connected[3] = true;
  samples[2].Gamepad.wButtons = XINPUT_GAMEPAD_A;
  samples[2].Gamepad.sThumbLX = -10000;
  samples[2].Gamepad.bLeftTrigger = 120;
  samples[3].Gamepad.sThumbLX = 5000;
  samples[3].Gamepad.bLeftTrigger = 200;
  UpdateControllerState();
  assert(g_Controllers[0].bConnected && g_Controllers[1].bConnected);
  assert(g_Controllers[0].state.Gamepad.wButtons == XINPUT_GAMEPAD_A);
  assert(g_Controllers[0].state.Gamepad.sThumbLX == -10000);
  assert(g_Controllers[0].state.Gamepad.bLeftTrigger == 200);
  assert(g_Controllers[1].state.Gamepad.wButtons == XINPUT_GAMEPAD_A);
  assert(g_Controllers[1].state.Gamepad.sThumbLX == -10000);
  assert(g_Controllers[1].state.Gamepad.bLeftTrigger == 200);
  connected[2] = false;
  connected[3] = false;
  samples[2] = {};
  samples[3] = {};
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

#if INPUT_SCANNERS
void CheckScannerSteering() {
  alignas(float) char driver[REALDRIVER_STEER_OFFSET + sizeof(float)] = {};
  auto steer = [&](int port) {
    RealDriver_JoyHandler_Steer(port, 0, 0, driver);
    return *(float*)(driver + REALDRIVER_STEER_OFFSET);
  };
  SteerLeftVKey = 'L';
  SteerRightVKey = 'R';
  SteerLeftAxis = XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF;
  SteerRightAxis = XINPUT_GAMEPAD_LS_RIGHT_CONFIGDEF;
  SteerLeftButton = XINPUT_GAMEPAD_A;
  SteerRightButton = XINPUT_GAMEPAD_B;
  g_Controllers[0] = g_Controllers[1] = {};
  memset(VKeyStates, 0, sizeof(VKeyStates));
  memset(asyncKeys, 0, sizeof(asyncKeys));

  KeyboardReadingMode = KB_READINGMODE_UNBUFFERED_RAW;
  VKeyStates[0]['L'] = VKeyStates[1]['R'] = 1;
  assert(steer(0) == 1 && steer(1) == -1); // Independent raw keyboards.
  VKeyStates[0]['R'] = 1;
  assert(steer(0) == 0); // Opposite keys cancel.
  VKeyStates[0]['R'] = 0;

  KeyboardReadingMode = KB_READINGMODE_BUFFERED;
  assert(steer(0) == 0 && steer(1) == 0); // Low bit is not a buffered press.
  VKeyStates[0]['L'] = 0x80;
  assert(steer(0) == 1 && steer(1) == 1); // Both ports use the shared buffer.

  KeyboardReadingMode = KB_READINGMODE_UNBUFFERED_ASYNC;
  asyncKeys['R'] = 1;
  assert(steer(0) == 0 && steer(1) == 0); // Ignore the async transition bit.
  asyncKeys['R'] = (SHORT)0x8000;
  assert(steer(0) == -1 && steer(1) == -1);
  asyncKeys['R'] = 0;
  SteerLeftVKey = SteerRightVKey = 0;
  asyncKeys[0] = (SHORT)0x8000;
  assert(steer(0) == 0 && steer(1) == 0); // Unbound keys stay inactive.
  asyncKeys[0] = 0;

  g_Controllers[0].state.Gamepad.sThumbLX = -32768;
  assert(steer(0) == 1 && steer(1) == 0); // Clamp the negative stick endpoint.
  g_Controllers[0].state.Gamepad.sThumbLX = 32767;
  g_Controllers[0].state.Gamepad.wButtons = XINPUT_GAMEPAD_B;
  assert(steer(0) == -1); // Analog and digital steering saturate together.
  g_Controllers[0].state.Gamepad.wButtons = XINPUT_GAMEPAD_A;
  assert(steer(0) == 0);
  g_Controllers[0] = {};
}

void CheckScannerAxes() {
  const WORD bindings[] = {XINPUT_GAMEPAD_LS_X_CONFIGDEF, XINPUT_GAMEPAD_RS_X_CONFIGDEF,
                           XINPUT_GAMEPAD_LS_Y_CONFIGDEF, XINPUT_GAMEPAD_RS_Y_CONFIGDEF};
  SHORT XINPUT_GAMEPAD::* const axes[] = {&XINPUT_GAMEPAD::sThumbLX, &XINPUT_GAMEPAD::sThumbRX,
                                        &XINPUT_GAMEPAD::sThumbLY, &XINPUT_GAMEPAD::sThumbRY};
  const SHORT samples[] = {-32768, -16384, 0, 16384, 32767};
  const BYTE expected[] = {1, 65, 128, 191, 255};
  for (int port = 0; port < 2; ++port) {
    void* joystick = port ? (void*)JOY2_ADDR : NULL;
    for (int axis = 0; axis < 4; ++axis) {
      for (int sample = 0; sample < 5; ++sample) {
        g_Controllers[0] = g_Controllers[1] = {};
        g_Controllers[port].state.Gamepad.*axes[axis] = samples[sample];
        ScannerConfig config = {};
        config.JoyEvent = JOY_EVENT_CARSEL_ORBIT_LEFTRIGHT;
        config.BitmaskStuff = bindings[axis];
        EventStates[port][config.JoyEvent] = 0xFFFF;
        assert(Scanner_Analog(NULL, NULL, 0, &config, joystick) == (int)(config.JoyEvent | expected[sample] << 8));
        assert(Scanner_Analog(NULL, NULL, 0, &config, joystick) == 0);

        config.JoyEvent = JOY_EVENT_CARSEL_ORBIT_UPDOWN;
        EventStates[port][config.JoyEvent] = 0xFFFF;
        BYTE inverted = (BYTE)-expected[sample];
        assert(Scanner_Analog(NULL, NULL, 0, &config, joystick) == (int)(config.JoyEvent | inverted << 8));

        for (unsigned int event : {JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT_ANALOG, JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT_ANALOG}) {
          config.JoyEvent = event;
          EventStates[port][event] = 0;
          const bool left = event == JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT_ANALOG;
          const int dragAxis = (int)expected[sample] - 128;
          const int encoded = (left ? dragAxis < 0 : dragAxis > 0) ? (int)(event | (BYTE)dragAxis << 8) : 0;
          assert(Scanner_Analog_DragSteer(NULL, NULL, 0, &config, joystick) == encoded);
          assert(Scanner_Analog_DragSteer(NULL, NULL, 0, &config, joystick) == 0);
        }
      }
    }
  }
  g_Controllers[0] = g_Controllers[1] = {};
}
#endif

#if !INPUT_SCANNERS && !defined(NO_FENG)
#if defined(GAME_CARBON) || defined(GAME_PROSTREET)
const char* printedTitle = nullptr;
void* TestTitleObject(char* package, unsigned int hash) {
  assert(package == SplashPkgName && hash == 0xC4DF3FF2);
  return (void*)0x1234;
}
int TestTitlePrint(void* object, const char* text, ...) {
  assert(object == (void*)0x1234);
  printedTitle = text;
  return 0;
}
#endif

void CheckTitleScreen() {
  bool previousConsole = bConsoleFEng;
  unsigned int previousIcons = ControllerIconMode;
  IconFallbackMode previousMode = CurrentIconMode;
  CurrentIconMode = ICON_MODE_CUSTOM;
  const char* expected[] = {"Press MENU or A button", "Press OPTIONS or X button", "Press START or A button",
                            "Press START or X button", "Press + button", "Press START or A button"};
  for (unsigned int mode = 0; mode < 6; ++mode) {
    ControllerIconMode = mode;
    bConsoleFEng = false;
    assert(!strcmp(TitleScreenPrompt(), "CLICK or ENTER to continue"));
    bConsoleFEng = true;
    assert(!strcmp(TitleScreenPrompt(), expected[mode]));
  }
  ControllerIconMode = 999;
  assert(!strcmp(TitleScreenPrompt(), "Press MENU or A button"));

  // Check built-in icon prompts
  CurrentIconMode = ICON_MODE_BUILTIN;
  bConsoleFEng = false;
  assert(!strcmp(TitleScreenPrompt(), "CLICK or ENTER to continue"));
  bConsoleFEng = true;
  ControllerIconMode = CONTROLLERICON_PS4;
#if defined(GAME_MW) || defined(GAME_CARBON)
  assert(!strcmp(TitleScreenPrompt(), "Press OPTIONS or X button"));
  ControllerIconMode = CONTROLLERICON_XBOXONE;
  assert(!strcmp(TitleScreenPrompt(), "Press START or X button"));
#else
  assert(!strcmp(TitleScreenPrompt(), "Press MENU or A button"));
#endif

  // Check plaintext fallback prompts
  CurrentIconMode = ICON_MODE_PLAINTEXT;
  bConsoleFEng = true;
  assert(!strcmp(TitleScreenPrompt(), "CLICK or ENTER to continue"));
  bConsoleFEng = false;
  assert(!strcmp(TitleScreenPrompt(), "CLICK or ENTER to continue"));

  CurrentIconMode = ICON_MODE_CUSTOM;
#if defined(GAME_CARBON) || defined(GAME_PROSTREET)
  // Exercise the separately compiled title module through replaceable native bindings.
  auto previousFind = FEngFindObject_Title;
  auto previousPrint = FE_String_Printf;
  FEngFindObject_Title = TestTitleObject;
  FE_String_Printf = TestTitlePrint;
  ControllerIconMode = CONTROLLERICON_PS4;
  for (bool console : {false, true}) {
    bConsoleFEng = console;
    printedTitle = nullptr;
    SetTitleScreenText();
    assert(printedTitle && !strcmp(printedTitle, console ? "Press OPTIONS or X button" : "CLICK or ENTER to continue"));
  }
  FEngFindObject_Title = previousFind;
  FE_String_Printf = previousPrint;
#endif
  bConsoleFEng = previousConsole;
  ControllerIconMode = previousIcons;
  CurrentIconMode = previousMode;
}
#endif

void CheckFallbacks() {
  // 1. Cursor fallback tests
  bool origWin32 = bUseWin32Cursor;
  bool origCustom = bUseCustomCursor;
  HCURSOR origCursor = NFSCursor;

  // Custom cursor file missing -> falls back to game's native cursor (bUseWin32Cursor = false)
  bUseWin32Cursor = true;
  bUseCustomCursor = true;
  ResolveCursorFallback();
  assert(!bUseWin32Cursor);
  assert(!bUseCustomCursor);
  assert(NFSCursor == NULL);
#if !INPUT_SCANNERS && !defined(NO_FENG)
  assert(!GetUseWin32Cursor());
#endif

  // Custom cursor disabled -> uses Win32 arrow cursor IDC_ARROW
  bUseWin32Cursor = true;
  bUseCustomCursor = false;
  ResolveCursorFallback();
  assert(bUseWin32Cursor);
  assert(!bUseCustomCursor);
  assert(NFSCursor != NULL);
#if !INPUT_SCANNERS && !defined(NO_FENG)
  assert(GetUseWin32Cursor());
#endif

  // Win32 cursor disabled -> no Win32 cursor
  bUseWin32Cursor = false;
  bUseCustomCursor = false;
  ResolveCursorFallback();
  assert(!bUseWin32Cursor);
  assert(NFSCursor == NULL);
#if !INPUT_SCANNERS && !defined(NO_FENG)
  assert(!GetUseWin32Cursor());
#endif

  bUseWin32Cursor = origWin32;
  bUseCustomCursor = origCustom;
  NFSCursor = origCursor;

  // 2. Icon fallback tests
  IconFallbackMode origMode = CurrentIconMode;
  bool origBuiltin = bHaveBuiltinIcons;
  bool origConsole = bConsoleFEng;
  bool origDynamic = bUseDynamicFEngSwitching;
  char origFilename[128];
  strcpy_s(origFilename, ButtonTexFilename);

  // Missing button file with built-in icons available -> ICON_MODE_BUILTIN
  strcpy_s(ButtonTexFilename, "nonexistent_buttons.tpk");
  bHaveBuiltinIcons = true;
  CurrentIconMode = ICON_MODE_CUSTOM;
  ResolveIconFallback();
  assert(CurrentIconMode == ICON_MODE_BUILTIN);

  // Missing button file without built-in icons -> ICON_MODE_PLAINTEXT
  bHaveBuiltinIcons = false;
  CurrentIconMode = ICON_MODE_CUSTOM;
  bConsoleFEng = true;
  ResolveIconFallback();
  assert(CurrentIconMode == ICON_MODE_PLAINTEXT);
  assert(!bConsoleFEng);

  // Existing button file -> ICON_MODE_CUSTOM
  char existingFile[MAX_PATH];
  assert(GetModuleFileNameA(NULL, existingFile, MAX_PATH) > 0);
  strcpy_s(ButtonTexFilename, existingFile);
  CurrentIconMode = ICON_MODE_PLAINTEXT;
  ResolveIconFallback();
  assert(CurrentIconMode == ICON_MODE_CUSTOM);

  // Device switching behavior in plaintext mode vs custom/builtin
  bUseDynamicFEngSwitching = true;
  CurrentIconMode = ICON_MODE_PLAINTEXT;
  InputSetActiveDevice(LASTCONTROLLED_CONTROLLER);
  assert(!bConsoleFEng);
  InputSetActiveDevice(LASTCONTROLLED_KB);
  assert(!bConsoleFEng);

  CurrentIconMode = ICON_MODE_CUSTOM;
  InputSetActiveDevice(LASTCONTROLLED_CONTROLLER);
  assert(bConsoleFEng);
  InputSetActiveDevice(LASTCONTROLLED_KB);
  assert(!bConsoleFEng);

  CurrentIconMode = ICON_MODE_BUILTIN;
  InputSetActiveDevice(LASTCONTROLLED_CONTROLLER);
  assert(bConsoleFEng);
  InputSetActiveDevice(LASTCONTROLLED_KB);
  assert(!bConsoleFEng);

  bHaveBuiltinIcons = origBuiltin;
  // Built-in icon capabilities per target
#if defined(GAME_MW) || defined(GAME_CARBON) || defined(GAME_UG)
  assert(HasBuiltinIcons() == true);
#else
  assert(HasBuiltinIcons() == false);
#endif

  CurrentIconMode = origMode;
  bHaveBuiltinIcons = origBuiltin;
  bConsoleFEng = origConsole;
  bUseDynamicFEngSwitching = origDynamic;
  strcpy_s(ButtonTexFilename, origFilename);

  // 3. Cursor hiding and input suppression tests
#ifndef GAME_UG
  bool origCursorHidden = bCursorHidden;
  bool origMouse1 = bMousePressedDown;
  bool origMouse2 = bMouse2PressedDown;
  bool origMouse3 = bMouse3PressedDown;
  int origWheel = MouseWheelValue;
  unsigned int origDevice = LastControlledDevice;

  // When cursor is hidden, clicks, buttons and wheel must NOT register
  bCursorHidden = true;
  LastControlledDevice = LASTCONTROLLED_CONTROLLER;
  bMousePressedDown = false;
  bMouse2PressedDown = false;
  bMouse3PressedDown = false;
  MouseWheelValue = 0;

  // LBUTTONDOWN while hidden
  CustomWndProc(NULL, WM_LBUTTONDOWN, 0, 0);
  assert(!bMousePressedDown);
  assert(LastControlledDevice == LASTCONTROLLED_CONTROLLER);

  // RBUTTONDOWN while hidden
  CustomWndProc(NULL, WM_RBUTTONDOWN, 0, 0);
  assert(!bMouse3PressedDown);
  assert(LastControlledDevice == LASTCONTROLLED_CONTROLLER);

  // MBUTTONDOWN while hidden
  CustomWndProc(NULL, WM_MBUTTONDOWN, 0, 0);
  assert(!bMouse2PressedDown);
  assert(LastControlledDevice == LASTCONTROLLED_CONTROLLER);

#if !INPUT_SCANNERS && !defined(NO_FENG)
  // MOUSEWHEEL while hidden
  CustomWndProc(NULL, WM_MOUSEWHEEL, MAKEWPARAM(0, 120), 0);
  assert(MouseWheelValue == 0);
  assert(LastControlledDevice == LASTCONTROLLED_CONTROLLER);
#endif

  // When cursor is visible, inputs register normally
  bCursorHidden = false;
  CustomWndProc(NULL, WM_LBUTTONDOWN, 0, 0);
  assert(bMousePressedDown);
  assert(LastControlledDevice == LASTCONTROLLED_KB);
  CustomWndProc(NULL, WM_LBUTTONUP, 0, 0);
  assert(!bMousePressedDown);

  LastControlledDevice = LASTCONTROLLED_CONTROLLER;
  CustomWndProc(NULL, WM_RBUTTONDOWN, 0, 0);
  assert(bMouse3PressedDown);
  assert(LastControlledDevice == LASTCONTROLLED_KB);
  CustomWndProc(NULL, WM_RBUTTONUP, 0, 0);
  assert(!bMouse3PressedDown);

  LastControlledDevice = LASTCONTROLLED_CONTROLLER;
  CustomWndProc(NULL, WM_MBUTTONDOWN, 0, 0);
  assert(bMouse2PressedDown);
  assert(LastControlledDevice == LASTCONTROLLED_KB);
  CustomWndProc(NULL, WM_MBUTTONUP, 0, 0);
  assert(!bMouse2PressedDown);

#if !INPUT_SCANNERS && !defined(NO_FENG)
  LastControlledDevice = LASTCONTROLLED_CONTROLLER;
  CustomWndProc(NULL, WM_MOUSEWHEEL, MAKEWPARAM(0, 120), 0);
  assert(MouseWheelValue == 120);
  assert(LastControlledDevice == LASTCONTROLLED_KB);
  MouseWheelValue = 0;
#endif

  bCursorHidden = origCursorHidden;
  bMousePressedDown = origMouse1;
  bMouse2PressedDown = origMouse2;
  bMouse3PressedDown = origMouse3;
  MouseWheelValue = origWheel;
  LastControlledDevice = origDevice;

  // Active controller input clears TimeSinceLastMouseMovement when mouse hiding is enabled
  bool origHiding = bEnableMouseHiding;
  DWORD origMovementTime = TimeSinceLastMouseMovement;
  bEnableMouseHiding = true;
  TimeSinceLastMouseMovement = 123456;
  InputSetActiveDevice(LASTCONTROLLED_CONTROLLER);
  assert(TimeSinceLastMouseMovement == 0);

  bEnableMouseHiding = false;
  TimeSinceLastMouseMovement = 654321;
  InputSetActiveDevice(LASTCONTROLLED_CONTROLLER);
  assert(TimeSinceLastMouseMovement == 654321);

  bEnableMouseHiding = true;
  TimeSinceLastMouseMovement = 999999;
  InputSetActiveDevice(LASTCONTROLLED_KB);
  assert(TimeSinceLastMouseMovement == 999999);

  bEnableMouseHiding = origHiding;
  TimeSinceLastMouseMovement = origMovementTime;
  LastControlledDevice = origDevice;
#endif
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
  g_Controllers[1].state.Gamepad.bRightTrigger = 255;
  assert(GetAnalogActivity(0) == 0);
  assert(GetAnalogActivity(1) == XINPUT_GAMEPAD_RT_CONFIGDEF);
  g_Controllers[1].state.Gamepad.bRightTrigger = 0;
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
  CheckRumbleSettings();
  CheckRemapping();
  CheckProfileFiles();
  CheckPolling();
  CheckKeyboard();
  CheckBindings();
  CheckFallbacks();
#if !INPUT_SCANNERS && !defined(NO_FENG)
  CheckTitleScreen();
#endif
#if INPUT_SCANNERS
  CheckScannerSteering();
  CheckScannerAxes();
  bRumbleInstalled = true;
  RumblePorts[0].sent[0] = 100;
#endif
#if INPUT_SCANNERS || defined(HAS_RUMBLE)
  bRumbleEnabled = true;
#endif
  assert(CustomWndProc(NULL, WM_KILLFOCUS, 0, 0) == 77);
#if INPUT_SCANNERS || defined(HAS_RUMBLE)
  assert(motorStops > 0);
#endif
  puts("Shared runtime checks passed.");
}
