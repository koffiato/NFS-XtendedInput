#pragma once

#if INPUT_SCANNERS
unsigned int ScannerDigitalFunction(unsigned int event, bool analog) {
  if (bIsEventDigitalDownOnly(event))
    return analog ? (unsigned int)&Scanner_DigitalDownAnalog : (unsigned int)&Scanner_DigitalDown;
  return analog ? (unsigned int)&Scanner_DigitalAnalog : (unsigned int)&Scanner_DigitalUpOrDown;
}

void SetupScannerConfig() {
  InputConfig ini;
  for (unsigned int event = 0; event < MAX_JOY_EVENT; ++event) {
    ScannerConfig& config = ScannerConfigs[event];
    config.JoyEvent = event;
    config.param = 0xFF;
    config.keycode = ConvertVKNameToValue(ini.ReadString("EventsKB", JoyEventNames[event], ""));
    WORD analog = (WORD)ConvertXInputOtherConfigDef(ini.ReadString("Events", JoyEventNames[event], ""));
    if (!analog) {
      config.BitmaskStuff = ConvertXInputNameToBitmask(ini.ReadString("Events", JoyEventNames[event], ""));
      config.ScannerFunctionPointer = ScannerDigitalFunction(event, false);
    } else {
      config.BitmaskStuff = analog;
      if (bIsEventAnalog(event)) config.ScannerFunctionPointer = (unsigned int)&Scanner_Analog;
      else if (analog != XINPUT_GAMEPAD_DPAD_CONFIGDEF) config.ScannerFunctionPointer = ScannerDigitalFunction(event, true);
      // Drag lane changes use their dedicated steering scanner.
      if (event == JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT_ANALOG || event == JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT_ANALOG)
        config.ScannerFunctionPointer = (unsigned int)&Scanner_Analog_DragSteer;
    }
    if (event == JOY_EVENT_STEER || event == JOY_EVENT_STEER_ANALOG)
      config.ScannerFunctionPointer = (unsigned int)&Scanner_CombinedSteering;
  }
  ScannerConfigs[JOY_EVENT_TYPE_CHANGED].ScannerFunctionPointer = (unsigned int)&Scanner_TypeChanged;
#ifdef GAME_UG2
  ScannerConfigs[JOY_EVENT_LIVE_TYPE_CHANGED].ScannerFunctionPointer = (unsigned int)&Scanner_TypeChanged;
#endif
  SteerLeftButton = ConvertXInputNameToBitmask(ini.ReadString("Events", "SteerLeftButton", ""));
  SteerRightButton = ConvertXInputNameToBitmask(ini.ReadString("Events", "SteerRightButton", ""));
  SteerLeftAxis = ConvertXInputOtherConfigDef(ini.ReadString("Events", "SteerLeftAxis", "XINPUT_GAMEPAD_LS_LEFT"));
  SteerRightAxis = ConvertXInputOtherConfigDef(ini.ReadString("Events", "SteerRightAxis", "XINPUT_GAMEPAD_LS_RIGHT"));
  SteerLeftVKey = ConvertVKNameToValue(ini.ReadString("EventsKB", "KeyboardSteerLeft", "VK_LEFT"));
  SteerRightVKey = ConvertVKNameToValue(ini.ReadString("EventsKB", "KeyboardSteerRight", "VK_RIGHT"));
}

#ifdef GAME_UG
const unsigned int CONFIG_BINDING_DELAY = 500;
const int UNBINDER_KEY = VK_F5;

void SaveBindingToIni(int event, uint16_t binding) {
  InputConfig ini;
  ini.WriteString("Events", JoyEventNames[event], ConvertXInputBitmaskToName(binding));
}

void SaveBindingToIniKB(int event, int binding) {
  InputConfig ini;
  ini.WriteString("EventsKB", JoyEventNames[event], VKeyStrings[binding]);
}

void SetUGKeyboardBinding(unsigned int index, bool secondary, int binding) {
  if (secondary) return; // The keyboard menu exposes primary bindings only.
  if (index == 3 || index == 4) {
    bool left = index == 3;
    unsigned int event = left ? JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT : JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT;
    (left ? SteerLeftVKey : SteerRightVKey) = (USHORT)binding;
    ScannerConfigs[event].keycode = binding;
    InputConfig ini;
    ini.WriteString("EventsKB", left ? "KeyboardSteerLeft" : "KeyboardSteerRight", VKeyStrings[binding]);
    SaveBindingToIniKB(event, binding);
    return;
  }
  unsigned int event = MapKeyboardConfigToEvent(index);
  ScannerConfigs[event].keycode = binding;
  if (binding) ScannerConfigs[event].ScannerFunctionPointer = ScannerDigitalFunction(event, false);
  SaveBindingToIniKB(event, binding);
}

void SetUGGamepadBinding(unsigned int index, bool secondary, WORD binding, bool analog) {
  if (index == 3 || index == 4) {
    bool left = index == 3;
    InputConfig ini;
    if (secondary) {
      (left ? SteerLeftButton : SteerRightButton) = binding;
      // Digital steering does not also remap drag lane changes.
      ini.WriteString("Events", left ? "SteerLeftButton" : "SteerRightButton", ConvertXInputBitmaskToName(binding));
    } else {
      (left ? SteerLeftAxis : SteerRightAxis) = binding;
      unsigned int event = left ? JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT : JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT;
      ScannerConfigs[event].BitmaskStuff = binding;
      ini.WriteString("Events", left ? "SteerLeftAxis" : "SteerRightAxis", ConvertXInputBitmaskToName(binding));
      SaveBindingToIni(event, binding);
    }
    return;
  }
  bool pedal = index == 1 || index == 2;
  if (secondary && !pedal) return;
  unsigned int event = secondary ? MapSecondaryJoypadConfigToEvent(index) : MapJoypadConfigToEvent(index);
  ScannerConfigs[event].BitmaskStuff = binding;
  if (binding) {
    ScannerConfigs[event].ScannerFunctionPointer = pedal
        ? (secondary ? (unsigned int)&Scanner_DigitalUpOrDown : (unsigned int)&Scanner_Analog)
        : ScannerDigitalFunction(event, analog);
  }
  SaveBindingToIni(event, binding);
}

uint32_t BindAllowTime = 0;
uint32_t OldSettingIdx = 0;

void HandleInGameConfigMenu() {
  unsigned int index = cur_setting_idx;
  bool secondary = cur_setting_idx_secondary != 0;
  if (OldSettingIdx != index) {
    BindAllowTime = timeGetTime() + CONFIG_BINDING_DELAY;
    OldSettingIdx = index;
  }
  // Delay capture so the button that opened the setting is not bound to it.
  if (!index || timeGetTime() <= BindAllowTime) return;
  int key = KB_GetCurrentPressedKey();
  if (*(int*)CONFIG_JOY_INDEX_ADDR == 0) {
    if (key == -1) return;
    SetUGKeyboardBinding(index, secondary, key == UNBINDER_KEY ? 0 : key);
  } else if (key == UNBINDER_KEY) {
    SetUGGamepadBinding(index, secondary, 0, false);
  } else {
    WORD buttons = g_Controllers[0].state.Gamepad.wButtons;
    bool drivingControl = index >= 1 && index <= 4;
    WORD analog = drivingControl && secondary ? 0 : GetAnalogActivity();
    WORD binding = drivingControl ? (secondary ? buttons : analog) : (analog ? analog : buttons);
    if (!binding) return;
    SetUGGamepadBinding(index, secondary, binding, analog != 0);
  }
  cur_setting_idx = 0;
}
#endif


#else
void        LoadMapping(std::string userProfileName);
void        ClearAllMappings();
std::string gProfileName;

// Keep profile creation and fallback behavior identical for load/save/reset.
std::string UserMappingPath(const std::string& profile) {
  return std::string(userMappingDir) + '\\' + profile + '\\' + userMappingName;
}

std::string EnsureUserMapping(const std::string& profile) {
  std::string path = UserMappingPath(profile);
  if (!PathFileExists(path.c_str())) {
    CreateDirectory(userMappingDir, NULL);
    std::string directory = std::string(userMappingDir) + '\\' + profile;
    CreateDirectory(directory.c_str(), NULL);
    if (!CopyFile(defMappingName, path.c_str(), TRUE)) return defMappingName;
  }
  return path;
}
#pragma runtime_checks("", off)
#ifdef GAME_MW
std::string oldProfileName;
uintptr_t   MemCardLoadAddr = 0x005189F0;
void __stdcall MemoryCard_Load_Hook(const char* profileName) {
  uintptr_t that;
  _asm mov  that, ecx;
  reinterpret_cast<void(__thiscall*)(uintptr_t, const char*)>(MemCardLoadAddr)(that, profileName);

  gProfileName = profileName;
  if (gProfileName.compare(oldProfileName) || oldProfileName.empty()) {
    oldProfileName = gProfileName;
    LoadMapping(gProfileName);
  }
}
#endif

#if defined(GAME_CARBON) || defined(GAME_PROSTREET)
char        NFSProfileName[128];
uintptr_t DALManager_GetString_Addr = DALMANAGER_GETSTRING_ADDR;
uintptr_t   gDALManager_Addr          = DALMANAGER_ADDR;
bool GetNFSProfileName() {
    // get it via DALManager
  return reinterpret_cast<bool(__thiscall*)(uintptr_t DALManager, const int valueType, char* getVal, const int getLen, const int arg1, const int arg2,
                                     const int arg3)>(DALManager_GetString_Addr)(gDALManager_Addr, DALMANAGER_PROFILENAME_TYPEVAL, NFSProfileName, 128, -1, -1, -1);
}
#endif

#if defined(GAME_UC) || defined(GAME_WORLD)
constexpr const char* NFSProfileName = "UserProfile";
#endif

#ifdef GAME_CARBON
uintptr_t OnLoadDoneFunc = 0x0058E250;
void __stdcall OnLoadDoneHook(const char* str) {
  uintptr_t that;
  _asm mov  that, ecx;
  reinterpret_cast<void(__thiscall*)(uintptr_t, const char*)>(OnLoadDoneFunc)(that, str);
  GetNFSProfileName();
  if (gProfileName.compare(NFSProfileName) || gProfileName.empty()) {
    gProfileName = NFSProfileName;
    LoadMapping(gProfileName);
  }
}
#endif

#ifdef GAME_PROSTREET
uintptr_t OnLoadDoneFunc = 0x00543CC0;
void __stdcall OnLoadDoneHook() {
  uintptr_t that;
  _asm mov that, ecx;
  reinterpret_cast<void(__thiscall*)(uintptr_t)>(OnLoadDoneFunc)(that);
  GetNFSProfileName();
  if (gProfileName.compare(NFSProfileName) || gProfileName.empty()) {
    gProfileName = NFSProfileName;
    LoadMapping(gProfileName);
  }
}
#endif

#pragma runtime_checks("", restore)


#ifdef GAME_PROSTREET
void SaveMappingValue(ActionID id, const char* section, const char* value) {
  mINI::INIFile file(EnsureUserMapping(gProfileName));
  mINI::INIStructure ini;
  file.read(ini);
  if (!*value && !ini.has(section)) return;
  ini[section][ActionIDStr[id]] = value;
  file.write(ini, true);
}

void SaveBindingToIni(ActionID id, bool isPrimary, uint16_t binding) {
  SaveMappingValue(id, isPrimary ? "Events" : "Events_Secondary", binding ? ConvertXInputBitmaskToName(binding) : "");
}

void SaveBindingToIniKB(ActionID id, bool isPrimary, int binding) {
  SaveMappingValue(id, isPrimary ? "EventsKB" : "EventsKB_Secondary", binding ? VKeyStrings[binding] : "");
}

void ClearExistingMapKB(int bind) {
  for (int i = 0; i < MAX_ACTIONID; i++) {
    if (bActionUserRemappable((ActionID)i)) {
      if (VKeyBindings_PRIMARY[i] == bind) {
        VKeyBindings_PRIMARY[i] = 0;
        SaveBindingToIniKB((ActionID)i, true, 0);
      }
      if (VKeyBindings_SECONDARY[i] == bind) {
        VKeyBindings_SECONDARY[i] = 0;
        SaveBindingToIniKB((ActionID)i, false, 0);
      }
    }
  }
}

void ClearExistingMap(uint16_t bind) {
  for (int i = 0; i < MAX_ACTIONID; i++) {
    if (bActionUserRemappable((ActionID)i)) {
      if (XInputBindings_PRIMARY[i] == bind) {
        XInputBindings_PRIMARY[i] = 0;
        SaveBindingToIni((ActionID)i, true, 0);
      }
      if (XInputBindings_SECONDARY[i] == bind) {
        XInputBindings_SECONDARY[i] = 0;
        SaveBindingToIni((ActionID)i, false, 0);
      }
    }
  }
}

bool HandleKeyboardRemap(uint32_t index, uint32_t isPrimary) {
  int key = g_bRemapClear ? 0 : KB_GetCurrentPressedKey();
  if (key == -1) return false;
  ActionID id = FindPCRemapActionID(index);
  ClearExistingMapKB(key);
  (isPrimary ? VKeyBindings_PRIMARY : VKeyBindings_SECONDARY)[id] = key;
  SaveBindingToIniKB(id, isPrimary, key);
  return true;
}

bool HandleGamepadRemap(uint32_t index, uint32_t isPrimary, int fDeviceIndex) {
  ActionID id = FindPCRemapActionID(index);
  WORD binding = 0;
  if (!g_bRemapClear) {
    binding = GetAnalogActivity(fDeviceIndex);
    if (!binding) binding = g_Controllers[fDeviceIndex].state.Gamepad.wButtons;
    if (!binding) return false;
    ClearExistingMap(binding);
  }
  (isPrimary ? XInputBindings_PRIMARY : XInputBindings_SECONDARY)[id] = binding;
  SaveBindingToIni(id, isPrimary, binding);
  if (bIsActionTextureBindable(id)) SetBindingButtonTexture(id, binding);
  return true;
}
#pragma runtime_checks("", off)
void __stdcall ResetMappingsToDefault() {
  void*    that;
  _asm mov that, ecx

  DeleteFile(UserMappingPath(gProfileName).c_str());
  ClearAllMappings();
  LoadMapping(gProfileName);

  reinterpret_cast<void(__thiscall*)(void*)>(0x0070BDE0)(that);
}
#pragma runtime_checks("", restore)
#endif

void ClearAllMappings() {
  for (unsigned int i = 0; i < MAX_ACTIONID; i++) {
    VKeyBindings_PRIMARY[i] = 0;
    VKeyBindings_SECONDARY[i] = 0;
    XInputBindings_PRIMARY[i] = 0;
    XInputBindings_SECONDARY[i] = 0;
  }
}

void LoadMapping(std::string userProfileName) {
  std::string mapFile = userProfileName.empty() ? defMappingName : EnsureUserMapping(userProfileName);

  mINI::INIFile      inifile(mapFile);
  mINI::INIStructure ini;
  inifile.read(ini);

  for (unsigned int i = 0; i < MAX_ACTIONID; i++) {
    // read the key bindings
    if (ini.has("EventsKB")) {
      if (ini["EventsKB"].has(ActionIDStr[i])) {
        VKeyBindings_PRIMARY[i] = ConvertVKNameToValue(ini["EventsKB"][ActionIDStr[i]].c_str());
      }
    }

    // read the key bindings [SECONDARY]
    if (ini.has("EventsKB_Secondary")) {
      if (ini["EventsKB_Secondary"].has(ActionIDStr[i])) {
        VKeyBindings_SECONDARY[i] = ConvertVKNameToValue(ini["EventsKB_Secondary"][ActionIDStr[i]].c_str());
      }
    }

    if (ini.has("Events")) {
      XInputBindings_PRIMARY[i] = ConvertXInputBinding(ini["Events"][ActionIDStr[i]]);
#ifndef NO_FENG
      if (bIsActionTextureBindable((ActionID)i)) SetBindingButtonTexture((ActionID)i, XInputBindings_PRIMARY[i]);
#endif
    }
    if (ini.has("Events_Secondary")) {
      XInputBindings_SECONDARY[i] = ConvertXInputBinding(ini["Events_Secondary"][ActionIDStr[i]]);
    }
  }

  if (ini.has("Events")) {
    FE_Secondary_Up = ConvertXInputBinding(ini["Events"][FE_SECONDARY_UP_NAME]);

    FE_Secondary_Down = ConvertXInputBinding(ini["Events"][FE_SECONDARY_DOWN_NAME]);

    FE_Secondary_Left = ConvertXInputBinding(ini["Events"][FE_SECONDARY_LEFT_NAME]);

    FE_Secondary_Right = ConvertXInputBinding(ini["Events"][FE_SECONDARY_RIGHT_NAME]);
  }
}

// based on MW -- may change across games

#endif
