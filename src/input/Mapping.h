#pragma once

#if INPUT_SCANNERS
void SetupScannerConfig()
{
	InputConfig inireader("");

	unsigned int inXInputConfigDef = 0;

	for (unsigned int i = 0; i < MAX_JOY_EVENT; i++)
	{
		ScannerConfigs[i].JoyEvent = i;
		ScannerConfigs[i].param = 0xFF;
		// keyboard VK codes
		ScannerConfigs[i].keycode = ConvertVKNameToValue(inireader.ReadString("EventsKB", JoyEventNames[i], ""));

		inXInputConfigDef = ConvertXInputOtherConfigDef(inireader.ReadString("Events", JoyEventNames[i], ""));
		if (!inXInputConfigDef)
		{
			if (bIsEventDigitalDownOnly(i))
				ScannerConfigs[i].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalDown;
			else
				ScannerConfigs[i].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalUpOrDown;

			ScannerConfigs[i].BitmaskStuff = ConvertXInputNameToBitmask(inireader.ReadString("Events", JoyEventNames[i], ""));
		}
		else
		{
			// triggers or sticks or dpad
			ScannerConfigs[i].BitmaskStuff = inXInputConfigDef;
			//ScannerConfigs[i].BitmaskStuff2 = inXInputConfigDef;

			if (bIsEventAnalog(i))
				ScannerConfigs[i].ScannerFunctionPointer = (unsigned int)&Scanner_Analog;
			else if ((inXInputConfigDef != XINPUT_GAMEPAD_DPAD_CONFIGDEF))
			{
				if (bIsEventDigitalDownOnly(i))
					ScannerConfigs[i].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalDownAnalog;
				else
					ScannerConfigs[i].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalAnalog;
			}
			// this is horribly broken, drag analog steering goes across all lanes...
			if ((i == JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT_ANALOG) || (i == JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT_ANALOG))
				ScannerConfigs[i].ScannerFunctionPointer = (unsigned int)&Scanner_Analog_DragSteer;
		}

		// JOY_EVENT_STEER
		if ((i == JOY_EVENT_STEER) || (i == JOY_EVENT_STEER_ANALOG))
		{
			ScannerConfigs[i].ScannerFunctionPointer = (unsigned int)&Scanner_CombinedSteering;
		}
	}

	ScannerConfigs[JOY_EVENT_TYPE_CHANGED].ScannerFunctionPointer = (unsigned int)&Scanner_TypeChanged;
#ifdef GAME_UG2
	ScannerConfigs[JOY_EVENT_LIVE_TYPE_CHANGED].ScannerFunctionPointer = (unsigned int)&Scanner_TypeChanged;
#endif
	//ScannerConfigs[JOY_EVENT_ANY].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalAnyButton;

	// read steering axis & buttons
	SteerLeftButton = ConvertXInputNameToBitmask(inireader.ReadString("Events", "SteerLeftButton", ""));
	SteerRightButton = ConvertXInputNameToBitmask(inireader.ReadString("Events", "SteerRightButton", ""));
	SteerLeftAxis = ConvertXInputOtherConfigDef(inireader.ReadString("Events", "SteerLeftAxis", "XINPUT_GAMEPAD_LS_LEFT"));
	SteerRightAxis = ConvertXInputOtherConfigDef(inireader.ReadString("Events", "SteerRightAxis", "XINPUT_GAMEPAD_LS_RIGHT"));

	// read steering VK codes
	SteerLeftVKey = ConvertVKNameToValue(inireader.ReadString("EventsKB", "KeyboardSteerLeft", "VK_LEFT"));
	SteerRightVKey = ConvertVKNameToValue(inireader.ReadString("EventsKB", "KeyboardSteerRight", "VK_RIGHT"));
}
#ifdef GAME_UG
#define CONFIG_BINDING_DELAY 500
#define UNBINDER_KEY VK_F5
void SaveBindingToIni(int JoyEvent, uint16_t bind)
{
	InputConfig inireader("");
	inireader.WriteString("Events", JoyEventNames[JoyEvent], ConvertXInputBitmaskToName(bind));
}

void SaveSteerBindingToIni(char* name, uint16_t bind)
{
	InputConfig inireader("");
	inireader.WriteString("Events", name, ConvertXInputBitmaskToName(bind));
}

void SaveBindingToIniKB(int JoyEvent, int bind)
{
	InputConfig inireader("");
	inireader.WriteString("EventsKB", JoyEventNames[JoyEvent], VKeyStrings[bind]);
}

void SaveSteerBindingToIniKB(char* name, int bind)
{
	InputConfig inireader("");
	inireader.WriteString("EventsKB", name, VKeyStrings[bind]);
}

uint32_t BindAcceptTimeBase;
uint32_t BindAllowTime;
uint32_t OldSettingIdx;
void HandleInGameConfigMenu()
{
	if (OldSettingIdx != cur_setting_idx)
	{
		BindAcceptTimeBase = timeGetTime();
		BindAllowTime = BindAcceptTimeBase + CONFIG_BINDING_DELAY;
		OldSettingIdx = cur_setting_idx;
	}

	if (cur_setting_idx)
	{
		WORD wButtons = g_Controllers[0].state.Gamepad.wButtons;
		
		// we want to delay the button reads for just a bit so we don't end up accidentally binding buttons
		if (timeGetTime() > BindAllowTime)
		{
			// keyboard config
			if (*(int*)CONFIG_JOY_INDEX_ADDR == 0)
			{
				int KBkey = KB_GetCurrentPressedKey();
				if (KBkey != -1)
				{
					// REMOVE BINDING BY PRESSING A BUTTON ON KEYBOARD
					if (KBkey == UNBINDER_KEY)
					{
						if ((cur_setting_idx == 3) || (cur_setting_idx == 4))
						{
							if (cur_setting_idx == 3)
							{
								if (cur_setting_idx_secondary == 0)
								{
									SteerLeftVKey = 0;
									ScannerConfigs[JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT].keycode = SteerLeftVKey;
									SaveSteerBindingToIniKB("KeyboardSteerLeft", SteerLeftVKey);
									SaveBindingToIniKB(JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT, SteerLeftVKey);
								}
							}
							if (cur_setting_idx == 4)
							{
								if (cur_setting_idx_secondary == 0)
								{
									SteerRightVKey = 0;
									ScannerConfigs[JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT].keycode = SteerRightVKey;
									SaveSteerBindingToIniKB("KeyboardSteerRight", SteerRightVKey);
									SaveBindingToIniKB(JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT, SteerRightVKey);
								}
							}
						}
						else
						{
							if (cur_setting_idx_secondary == 0)
							{
								ScannerConfigs[MapKeyboardConfigToEvent(cur_setting_idx)].keycode = 0;
								SaveBindingToIniKB(MapKeyboardConfigToEvent(cur_setting_idx), ScannerConfigs[MapKeyboardConfigToEvent(cur_setting_idx)].keycode);
							}
						}

						cur_setting_idx = 0;
						return;
					}

					if ((cur_setting_idx == 3) || (cur_setting_idx == 4))
					{
						if (cur_setting_idx == 3)
						{
							if (cur_setting_idx_secondary == 0)
							{
								SteerLeftVKey = KBkey;
								ScannerConfigs[JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT].keycode = SteerLeftVKey;
								SaveSteerBindingToIniKB("KeyboardSteerLeft", SteerLeftVKey);
								SaveBindingToIniKB(JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT, SteerLeftVKey);
							}
						}
						if (cur_setting_idx == 4)
						{
							if (cur_setting_idx_secondary == 0)
							{
								SteerRightVKey = KBkey;
								ScannerConfigs[JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT].keycode = SteerRightVKey;
								SaveSteerBindingToIniKB("KeyboardSteerRight", SteerRightVKey);
								SaveBindingToIniKB(JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT, SteerRightVKey);
							}
						}
					}
					else
					{
						if (cur_setting_idx_secondary == 0)
						{
							ScannerConfigs[MapKeyboardConfigToEvent(cur_setting_idx)].keycode = KBkey;
							if (bIsEventDigitalDownOnly(MapKeyboardConfigToEvent(cur_setting_idx)))
								ScannerConfigs[MapKeyboardConfigToEvent(cur_setting_idx)].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalDown;
							else
								ScannerConfigs[MapKeyboardConfigToEvent(cur_setting_idx)].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalUpOrDown;
							SaveBindingToIniKB(MapKeyboardConfigToEvent(cur_setting_idx), ScannerConfigs[MapKeyboardConfigToEvent(cur_setting_idx)].keycode);
						}
					}
					cur_setting_idx = 0;
					return;
				}
			}
			// JoyPad config
			else
			{
				// REMOVE BINDING BY PRESSING A BUTTON ON KEYBOARD
				if (KB_GetCurrentPressedKey() == UNBINDER_KEY)
				{
					if (((cur_setting_idx >= 1) && (cur_setting_idx <= 4)))
					{
						if ((cur_setting_idx_secondary == 0))
						{
							if ((cur_setting_idx == 1) || (cur_setting_idx == 2))
							{
								ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff = 0;
								SaveBindingToIni(MapJoypadConfigToEvent(cur_setting_idx), ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff);
							}
							if (cur_setting_idx == 3)
							{
								SteerLeftAxis = 0;
								ScannerConfigs[JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT].BitmaskStuff = SteerLeftAxis;
								SaveSteerBindingToIni("SteerLeftAxis", SteerLeftAxis);
								SaveBindingToIni(JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT, SteerLeftAxis);
							}
							if (cur_setting_idx == 4)
							{
								SteerRightAxis = 0;
								ScannerConfigs[JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT].BitmaskStuff = SteerRightAxis;
								SaveSteerBindingToIni("SteerRightAxis", SteerRightAxis);
								SaveBindingToIni(JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT, SteerRightAxis);
							}
						}
						else
						{
							if ((cur_setting_idx == 1) || (cur_setting_idx == 2))
							{
								ScannerConfigs[MapSecondaryJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff = 0;
								SaveBindingToIni(MapSecondaryJoypadConfigToEvent(cur_setting_idx), ScannerConfigs[MapSecondaryJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff);
							}
							if (cur_setting_idx == 3)
							{
								SteerLeftButton = 0;
								// TODO: map drag steering to digital steering (or redo its entire handler)
								SaveSteerBindingToIni("SteerLeftButton", SteerLeftButton);
							}
							if (cur_setting_idx == 4)
							{
								SteerRightButton = 0;
								// TODO: map drag steering to digital steering (or redo its entire handler)
								SaveSteerBindingToIni("SteerRightButton", SteerRightButton);
							}
						}
					}
					else
					{
						if (cur_setting_idx_secondary == 0)
						{
							ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff = 0;
							SaveBindingToIni(MapJoypadConfigToEvent(cur_setting_idx), ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff);
						}
					}
					cur_setting_idx = 0;
					return;
				}

				
				if (((cur_setting_idx >= 1) && (cur_setting_idx <= 4)))
				{
					if ((cur_setting_idx_secondary == 0))
					{
						// analog ONLY bindings -- reads only the analog axis and nothing else!
						uint16_t act = GetAnalogActivity();
						if (act)
						{
							if (cur_setting_idx == 1)
							{
								// throttle analog
								ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff = act;
								ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].ScannerFunctionPointer = (unsigned int)&Scanner_Analog;
								SaveBindingToIni(MapJoypadConfigToEvent(cur_setting_idx), ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff);
							}
							if (cur_setting_idx == 2)
							{
								// brakes analog
								ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff = act;
								ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].ScannerFunctionPointer = (unsigned int)&Scanner_Analog;
								SaveBindingToIni(MapJoypadConfigToEvent(cur_setting_idx), ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff);
							}
							if (cur_setting_idx == 3)
							{
								// steer left analog
								SteerLeftAxis = act;
								ScannerConfigs[JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT].BitmaskStuff = SteerLeftAxis;
								SaveSteerBindingToIni("SteerLeftAxis", SteerLeftAxis);
								SaveBindingToIni(JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT, SteerLeftAxis);
							}
							if (cur_setting_idx == 4)
							{
								// steer right analog
								SteerRightAxis = act;
								ScannerConfigs[JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT].BitmaskStuff = SteerRightAxis;
								SaveSteerBindingToIni("SteerRightAxis", SteerRightAxis);
								SaveBindingToIni(JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT, SteerRightAxis);
							}
							cur_setting_idx = 0;
							return;
						}
					}
					else
					{
						// digital ONLY bindings -- reads only the buttons (on the secondary bind)
						if (wButtons)
						{
							if (cur_setting_idx == 1)
							{
								// throttle digital
								ScannerConfigs[MapSecondaryJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff = wButtons;
								ScannerConfigs[MapSecondaryJoypadConfigToEvent(cur_setting_idx)].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalUpOrDown;
								SaveBindingToIni(MapSecondaryJoypadConfigToEvent(cur_setting_idx), ScannerConfigs[MapSecondaryJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff);
							}
							if (cur_setting_idx == 2)
							{
								// brakes digital
								ScannerConfigs[MapSecondaryJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff = wButtons;
								ScannerConfigs[MapSecondaryJoypadConfigToEvent(cur_setting_idx)].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalUpOrDown;
								SaveBindingToIni(MapSecondaryJoypadConfigToEvent(cur_setting_idx), ScannerConfigs[MapSecondaryJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff);
							}
							if (cur_setting_idx == 3)
							{
								// steer left digital
								SteerLeftButton = wButtons;
								// TODO: map drag steering to digital steering (or redo its entire handler)
								SaveSteerBindingToIni("SteerLeftButton", SteerLeftButton);
							}
							if (cur_setting_idx == 4)
							{
								// steer right digital
								SteerRightButton = wButtons;
								// TODO: map drag steering to digital steering (or redo its entire handler)
								SaveSteerBindingToIni("SteerRightButton", SteerRightButton);
							}
							cur_setting_idx = 0;
							return;
						}
					}
				}
				// all other bindings - these can be bound to both analogs and digitals
				else
				{
					uint16_t act = GetAnalogActivity();
					if (act)
					{
						if (cur_setting_idx_secondary == 0)
						{
							ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff = act;
							if (bIsEventDigitalDownOnly(MapJoypadConfigToEvent(cur_setting_idx)))
								ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalDownAnalog;
							else
								ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalAnalog;
							SaveBindingToIni(MapJoypadConfigToEvent(cur_setting_idx), ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff);
						}
						cur_setting_idx = 0;
						return;
					}
					else if (wButtons)
					{
						// detect which button is pressed
						if (cur_setting_idx_secondary == 0)
						{
							ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff = wButtons;
							if (bIsEventDigitalDownOnly(MapJoypadConfigToEvent(cur_setting_idx)))
								ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalDown;
							else
								ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].ScannerFunctionPointer = (unsigned int)&Scanner_DigitalUpOrDown;
							SaveBindingToIni(MapJoypadConfigToEvent(cur_setting_idx), ScannerConfigs[MapJoypadConfigToEvent(cur_setting_idx)].BitmaskStuff);
						}
						cur_setting_idx = 0;
						return;
					}
				}
			}
		}
	}
}
#endif


#else
void        LoadMapping(std::string userProfileName);
void        ClearAllMappings();
std::string gProfileName;
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
void SaveBindingToIni(ActionID id, bool isPrimary, uint16_t bind) {
  std::string mapFile = userMappingDir;
  mapFile += '\\';
  mapFile += gProfileName;
  mapFile += '\\';
  mapFile += userMappingName;
  if (!PathFileExists(mapFile.c_str())) {
    if (!PathFileExists(userMappingDir)) CreateDirectory(userMappingDir, NULL);
    std::string dirName = userMappingDir;
    dirName += '\\';
    dirName += gProfileName;
    if (!PathFileExists(dirName.c_str())) CreateDirectory(dirName.c_str(), NULL);
    if (CopyFile(defMappingName, mapFile.c_str(), TRUE) == FALSE) mapFile = defMappingName;
  }

  mINI::INIFile      inifile(mapFile);
  mINI::INIStructure ini;
  inifile.read(ini);

  if (bind == 0) {
    if (ini.has(isPrimary ? "Events" : "Events_Secondary")) {
      ini[isPrimary ? "Events" : "Events_Secondary"][ActionIDStr[id]] = "";
      inifile.write(ini, true);
    }
    return;
  }

  ini[isPrimary ? "Events" : "Events_Secondary"][ActionIDStr[id]] = ConvertXInputBitmaskToName(bind);
  inifile.write(ini, true);
}

void SaveBindingToIniKB(ActionID id, bool isPrimary, int bind) {
  std::string mapFile = userMappingDir;
  mapFile += '\\';
  mapFile += gProfileName;
  mapFile += '\\';
  mapFile += userMappingName;
  if (!PathFileExists(mapFile.c_str())) {
    if (!PathFileExists(userMappingDir)) CreateDirectory(userMappingDir, NULL);
    std::string dirName = userMappingDir;
    dirName += '\\';
    dirName += gProfileName;
    if (!PathFileExists(dirName.c_str())) CreateDirectory(dirName.c_str(), NULL);
    if (CopyFile(defMappingName, mapFile.c_str(), TRUE) == FALSE) mapFile = defMappingName;
  }

  mINI::INIFile      inifile(mapFile);
  mINI::INIStructure ini;
  inifile.read(ini);

  if (bind == 0) {
    if (ini.has(isPrimary ? "EventsKB" : "EventsKB_Secondary")) {
      ini[isPrimary ? "EventsKB" : "EventsKB_Secondary"][ActionIDStr[id]] = "";
      inifile.write(ini, true);
    }
    return;
  }

  ini[isPrimary ? "EventsKB" : "EventsKB_Secondary"][ActionIDStr[id]] = VKeyStrings[bind];
  inifile.write(ini, true);
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
  int      KBkey = g_bRemapClear ? 0 : KB_GetCurrentPressedKey();
  ActionID id    = FindPCRemapActionID(index);
  if (KBkey != -1) {
    ClearExistingMapKB(KBkey);
    if (isPrimary)
      VKeyBindings_PRIMARY[id] = KBkey;
    else
      VKeyBindings_SECONDARY[id] = KBkey;
    SaveBindingToIniKB(id, isPrimary, KBkey);
    return true;
  }
  return false;
}

bool HandleGamepadRemap(uint32_t index, uint32_t isPrimary, int fDeviceIndex) {
  ActionID id = FindPCRemapActionID(index);
  if (g_bRemapClear) {
    if (isPrimary)
      XInputBindings_PRIMARY[id] = 0;
    else
      XInputBindings_SECONDARY[id] = 0;
    SaveBindingToIni(id, isPrimary, 0);
    if (bIsActionTextureBindable(id)) SetBindingButtonTexture(id, isPrimary ? XInputBindings_PRIMARY[id] : XInputBindings_SECONDARY[id]);
    return true;
  }

  uint16_t act = GetAnalogActivity();
  if (act) {
    ClearExistingMap(act);
    if (isPrimary)
      XInputBindings_PRIMARY[id] = act;
    else
      XInputBindings_SECONDARY[id] = act;
    SaveBindingToIni(id, isPrimary, act);
    if (bIsActionTextureBindable(id)) SetBindingButtonTexture(id, isPrimary ? XInputBindings_PRIMARY[id] : XInputBindings_SECONDARY[id]);
    return true;
  }

  WORD buttons = g_Controllers[fDeviceIndex].state.Gamepad.wButtons;
  if (buttons) {
    ClearExistingMap(buttons);
    if (isPrimary)
      XInputBindings_PRIMARY[id] = buttons;
    else
      XInputBindings_SECONDARY[id] = buttons;
    SaveBindingToIni(id, isPrimary, buttons);
    if (bIsActionTextureBindable(id)) SetBindingButtonTexture(id, isPrimary ? XInputBindings_PRIMARY[id] : XInputBindings_SECONDARY[id]);
    return true;
  }

  return false;
}
#pragma runtime_checks("", off)
void __stdcall ResetMappingsToDefault() {
  void*    that;
  _asm mov that, ecx

  std::string mapFile = userMappingDir;
  mapFile += '\\';
  mapFile += gProfileName;
  mapFile += '\\';
  mapFile += userMappingName;
  if (!PathFileExists(mapFile.c_str())) {
    if (!PathFileExists(userMappingDir)) CreateDirectory(userMappingDir, NULL);
    std::string dirName = userMappingDir;
    dirName += '\\';
    dirName += gProfileName;
    if (!PathFileExists(dirName.c_str())) CreateDirectory(dirName.c_str(), NULL);
    if (CopyFile(defMappingName, mapFile.c_str(), TRUE) == FALSE) mapFile = defMappingName;
  } else {
    DeleteFile(mapFile.c_str());
  }
  ClearAllMappings();
  LoadMapping(gProfileName);

  //for (unsigned int i = 0; i < MAX_ACTIONID; i++) {
  //  SaveBindingToIni((ActionID)i, true, XInputBindings_PRIMARY[i]);
  //  SaveBindingToIni((ActionID)i, false, XInputBindings_SECONDARY[i]);
  //  SaveBindingToIniKB((ActionID)i, true, VKeyBindings_PRIMARY[i]);
  //  SaveBindingToIniKB((ActionID)i, false, VKeyBindings_SECONDARY[i]);
  //}

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
  std::string mapFile = userMappingDir;

  if (userProfileName.empty()) {
    mapFile = defMappingName;
  } else {
    mapFile += '\\';
    mapFile += userProfileName;
    mapFile += '\\';
    mapFile += userMappingName;
    if (!PathFileExists(mapFile.c_str())) {
      if (!PathFileExists(userMappingDir)) CreateDirectory(userMappingDir, NULL);
      std::string dirName = userMappingDir;
      dirName += '\\';
      dirName += userProfileName;
      if (!PathFileExists(dirName.c_str())) CreateDirectory(dirName.c_str(), NULL);
      if (CopyFile(defMappingName, mapFile.c_str(), TRUE) == FALSE) mapFile = defMappingName;
    }
  }

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
