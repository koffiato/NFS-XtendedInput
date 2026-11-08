#pragma once

#pragma runtime_checks("", off)
#if INPUT_SCANNERS
void InstallGameHooks()
{
#ifdef GAME_UG
	// hook FEng globally in FEPkgMgr_SendMessageToPackage
	injector::MakeJMP(FENG_SENDMSG_HOOK_ADDR, FEngGlobalCave, true);

	// snoop last activated FEng package
	injector::MakeCALL(0x004F3BC3, SnoopLastFEPackage, true);

	// reroute ScannerConfig table
	injector::WriteMemory(SCANNERCONFIG_POINTER_ADDR, ScannerConfigs, true);
	*(int*)0x00704140 = MAX_JOY_EVENT;
	settings_controllers[0].type = 0x13;
	settings_controllers[1].type = 0x15;
	injector::WriteMemory<int>(0x00413ADD, (int)(&settings_controllers[0].type), true);
	injector::MakeJMP(0x00413C0A, settings_cave1, true);

	injector::MakeJMP(0x004141DF, settings_cave2, true);
	injector::MakeJMP(0x004144EC, settings_cave3, true);

	// disable writing to the device counter
	injector::MakeNOP(0x00405BE8, 6, true);
	injector::MakeNOP(0x00405D2F, 6, true);
	injector::MakeNOP(0x00406463, 5, true);
	injector::MakeNOP(0x0040648F, 5, true);
	injector::MakeNOP(0x004064BB, 5, true);

	injector::MakeJMP(0x00414830, SetConfigButtonText, true);

	injector::MakeCALL(ACTUALREADJOYDATA_CALL_ADDR2, ReadControllerData, true);
	injector::MakeCALL(ACTUALREADJOYDATA_CALL_ADDR3, ReadControllerData, true);
#endif
#ifdef GAME_UG2
	// custom config finder (because it returns 0 with the original function)
	injector::MakeJMP(FINDSCANNERCONFIG_ADDR, FindScannerConfig_Custom, true);
	// Win32 mouse injection
	injector::MakeCALL(0x00581470, UpdateFECursorPos, true);
	// disable cursor hiding
	injector::MakeJMP(0x005D268C, 0x5D269C, true);
	// disable PC_CURSOR texture to avoid duplicate cursors
	injector::WriteMemory<unsigned int>(0x0050B4EA, 0, true);
	injector::MakeNOP(DISABLE_WHEEL_ADDR, 5, true);
#endif

	// custom steering handler
	injector::WriteMemory<unsigned int>(STEER_HANDLER_ADDR1, (int)&RealDriver_JoyHandler_Steer, true);
	injector::WriteMemory<unsigned int>(STEER_HANDLER_ADDR2, (int)&DummyFunc, true);

	// kill DInput8 joypad reading & event generation
	injector::MakeJMP(EVENTGEN_JMP_ADDR_ENTRY, EVENTGEN_JMP_ADDR_EXIT, true);
	// this kills DInput enumeration COMPLETELY -- even the keyboard
	injector::MakeJMP(DINPUTENUM_JMP_ADDR_ENTRY, DINPUTENUM_JMP_ADDR_EXIT, true);
	// kill game input reading
	injector::MakeJMP(JOYBUFFER_JMP_ADDR_ENTRY, JOYBUFFER_JMP_ADDR_EXIT, true);

	// Replace ActualReadJoystickData with ReadControllerData
	injector::MakeCALL(ACTUALREADJOYDATA_CALL_ADDR1, ReadControllerData, true);

	// KB input init
	injector::MakeCALL(INITJOY_CALL_ADDR, InitCustomKBInput, true);


}
#else
#pragma runtime_checks("", off)

#ifdef GAME_PROSTREET

void __stdcall GoRemapButtonHook(uint32_t type, uint32_t button_index, uint32_t isPrimary, uint32_t unk) {
  InputDevice* that;
  _asm mov     that, ecx;
  return that->GoRemapButton(type, button_index, isPrimary, unk);
}

void __stdcall GoClearButtonHook(uint32_t type, uint32_t button_index, uint32_t isPrimary, uint32_t unk) {
  InputDevice* that;
  _asm mov     that, ecx;
  g_bRemapClear           = true;
  g_bRemapClear_IsPrimary = isPrimary;
  g_iRemapClear_BtnIndex  = button_index;
  return that->GoRemapButton(type, button_index, isPrimary, unk);
}

bool __stdcall GetRemappingModeHook() {
  InputDevice* that;
  _asm mov     that, ecx;

  return that->bRemappingMode;
}

void GetMappingStringHook(char* str, uint32_t type, uint32_t button_index, uint32_t isPrimary, uint32_t unk) {
  if (g_bRemapClear && button_index == g_iRemapClear_BtnIndex && isPrimary == g_bRemapClear_IsPrimary) {
    strcpy(str, "---");
    return;
  }
  if (type == 1) {
    strcpy(str,
           ControlsTextsPC[isPrimary ? VKeyBindings_PRIMARY[FindPCRemapActionID(button_index)] : VKeyBindings_SECONDARY[FindPCRemapActionID(button_index)]]);
    return;
  }

  if (type == 3) {
    strcpy(str, ConvertBitmaskToControlString(ControllerIconMode, isPrimary ? XInputBindings_PRIMARY[FindPCRemapActionID(button_index)]
                                                                            : XInputBindings_SECONDARY[FindPCRemapActionID(button_index)]));
    return;
  }

  strcpy(str, "---");
}

uint32_t GetNumExternalDevices() { return 1; }

uint32_t GetNumExternalDevicesZero() { return 0; }

const char* __stdcall GetXtendedInputDeviceName(uint32_t type, uint32_t index) {
  if (type == 1) return "Keyboard";
  if (type == 3) return "XInput Controller";

  return "Unknown Input Device";
}

#endif

#ifndef GAME_WORLD
void*(__thiscall* FastMem_Alloc)(void* FastMem, int size, char* debug_str) = (void*(__thiscall*)(void*, int, char*))FASTMEM_ALLOC_ADDR;
#endif

#ifdef GAME_MW
void* (*FastMem_InitListAllocator)()                               = (void* (*)())FASTMEM_INITLISTALLOCATOR_ADDR;
void*(__stdcall* FastMem_ListAllocator)(void* CurrentListPos, void* NextListPos,
                                        InputMapEntry* inInputMap) = (void*(__stdcall*)(void*, void*, InputMapEntry*))FASTMEM_LISTALLOCATOR_ADDR;
void*(__thiscall* STL_List_Add)(void* thethis, int num)            = (void*(__thiscall*)(void*, int))STL_LIST_ADD_ADDR;

void* __stdcall InputMapping_Constructor(InputDevice* device, void* AttribCollection) {
  unsigned int thethis      = 0;
  unsigned int list_current = 0;
  unsigned int list_next    = 0;
  int          alloc_result = 0;

  _asm mov thethis, ecx;

  // use game's own memory management to avoid trouble
  *(void**)(thethis + 4) = (void*)FastMem_InitListAllocator();
  list_current           = (unsigned int)*(void**)(thethis + 4);

  for (unsigned int i = 0; i < MAX_ACTIONID; i++) {
    if (VKeyBindings_PRIMARY[i] != 0 || VKeyBindings_SECONDARY[i] != 0 || XInputBindings_PRIMARY[i] != 0 || XInputBindings_SECONDARY[i] != 0)  // is it mapped
    {
      InputMapEntry map;
      if (device->fDeviceScalar[i].fType == kDigitalButton)
        map.UpdateType = kPress;
      else
        map.UpdateType = kUpdate;
      map.LowerDZ           = 0.0;
      map.UpperDZ           = 0.0;
      map.Action            = (ActionID)i;
      map.DeviceScalarIndex = i;
      map.PreviousValue     = -1.0f;
      map.CurrentValue      = -1.0f;
      list_next             = *(int*)(list_current + 4);
      alloc_result          = (int)FastMem_ListAllocator((void*)list_current, (void*)list_next, &map);
      *(int*)(list_current + 4)   = alloc_result;
      **(int**)(alloc_result + 4) = alloc_result;
    }
  }

  return (void*)thethis;
}
#else

#ifndef GAME_WORLD
void* __stdcall FastMem_ListAllocator(void* CurrentListPos, void* NextListPos, InputMapEntry* inInputMap) {
  void* result = FastMem_Alloc((void*)GLOBAL_FASTMEM_ADDR, INIT_LIST_ALLOC_SIZE, NULL);
  if (result) {
    *(void**)result = CurrentListPos;
    *(void**)((int)result + 4) = NextListPos;
    memcpy((void*)((int)result + 8), inInputMap, sizeof(InputMapEntry));
  }
  return result;
}
#else
void* (*Managed_Alloc)(int size) = (void* (*)(int))MANAGED_ALLOCATOR_ADDR;
void* __stdcall FastMem_ListAllocator(void* CurrentListPos, void* NextListPos, InputMapEntry* inInputMap) {
  void* result = Managed_Alloc(INIT_LIST_ALLOC_SIZE);
  if (result) {
    *(void**)result            = CurrentListPos;
    *(void**)((int)result + 4) = NextListPos;
    memcpy((void*)((int)result + 8), inInputMap, sizeof(InputMapEntry));
  }
  return result;
}
#endif
void* __stdcall InputMapping_Constructor(InputDevice* device, void* AttribCollection) {
  volatile unsigned int thethis = 0;
  volatile unsigned int list_current = 0;
  volatile unsigned int list_next = 0;
  volatile int alloc_result = 0;

  _asm mov thethis, ecx;

  // use game's own memory management to avoid trouble
  *(void**)(thethis) = (void*)thethis;
  *(void**)(thethis + 4) = (void*)thethis;
  *(int*)(thethis + 8) = 0;

  for (unsigned int i = 0; i < MAX_ACTIONID; i++) {
    if (VKeyBindings_PRIMARY[i] != 0 || VKeyBindings_SECONDARY[i] != 0 || XInputBindings_PRIMARY[i] != 0 || XInputBindings_SECONDARY[i] != 0)  // is it mapped
    {
      InputMapEntry map;
      if (device->fDeviceScalar[i].fType == kDigitalButton)
        map.UpdateType = kPress;
      else
        map.UpdateType = kUpdate;

      map.LowerDZ = 0.0;
      map.UpperDZ = 0.0;
      map.Action = (ActionID)i;
      map.DeviceScalarIndex = i;
      map.ComboDeviceScalarIndex = -1;
      map.PreviousValue = -1.0f;
      map.CurrentValue = -1.0f;
      list_current = *(int*)(thethis + 4);
      list_next = *(int*)(list_current + 4);
      alloc_result = (int)FastMem_ListAllocator((void*)thethis, (void*)list_next, &map);

      *(int*)alloc_result = thethis;
      *(int*)(alloc_result + 4) = *(int*)(thethis + 4);
      **(int**)(thethis + 4) = alloc_result;
      *(int*)(thethis + 8) += 1;
      *(int*)(thethis + 4) = alloc_result;
    }
  }

  return (void*)thethis;
}

#ifndef NO_FENG
void HandleSelectCarCamera(void* SelectCarObj, SHORT XAxis, SHORT YAxis) {
  if (XAxis || YAxis || bMousePressedDown) {
    if (bMousePressedDown) {
      float HSpeed = *(int*)FEMOUSECURSOR_CARORBIT_X_ADDR;
      float VSpeed = *(int*)FEMOUSECURSOR_CARORBIT_Y_ADDR;

      if (HSpeed > 25.0) HSpeed = 25.0;
      if (HSpeed < -25.0) HSpeed = -25.0;
      if (VSpeed > 25.0) VSpeed = 25.0;
      if (VSpeed < -25.0) VSpeed = -25.0;

      if (HSpeed)
        SelectCarCameraMover_SetHRotateSpeed(SelectCarObj, HSpeed, true);
      else
        SelectCarCameraMover_SetHRotateSpeed(SelectCarObj, 0, false);
      if (VSpeed)
        SelectCarCameraMover_SetVRotateSpeed(SelectCarObj, VSpeed, true);
      else
        SelectCarCameraMover_SetVRotateSpeed(SelectCarObj, 0, false);
    }

    if (XAxis) {
      float HSpeed = -XAxis / float(0x7FFF);
      SelectCarCameraMover_SetHRotateSpeed(SelectCarObj, HSpeed, true);
    }
    if (YAxis) {
      float VSpeed = YAxis / float(0x7FFF);
      SelectCarCameraMover_SetVRotateSpeed(SelectCarObj, VSpeed, true);
    }
  } else {
    SelectCarCameraMover_SetHRotateSpeed(SelectCarObj, 0, false);
    SelectCarCameraMover_SetVRotateSpeed(SelectCarObj, 0, false);
  }

  if (g_Controllers[0].state.Gamepad.bLeftTrigger || (MouseWheelValue < 0)) {
    if (MouseWheelValue)  // TODO: maybe enhance it with a proper accel delta???
      SelectCarCameraMover_SetZoomSpeed(SelectCarObj, 1.0, true);
    if (g_Controllers[0].state.Gamepad.bLeftTrigger) {
      float Speed = g_Controllers[0].state.Gamepad.bLeftTrigger / float(0xFF);
      SelectCarCameraMover_SetZoomSpeed(SelectCarObj, Speed, true);
    }
  }

  else if (g_Controllers[0].state.Gamepad.bRightTrigger || (MouseWheelValue > 0)) {
    if (MouseWheelValue) SelectCarCameraMover_SetZoomSpeed(SelectCarObj, -1.0, true);
    if (g_Controllers[0].state.Gamepad.bRightTrigger) {
      float Speed = g_Controllers[0].state.Gamepad.bRightTrigger / float(0xFF);
      SelectCarCameraMover_SetZoomSpeed(SelectCarObj, -Speed, true);
    }
  } else
    SelectCarCameraMover_SetZoomSpeed(SelectCarObj, 0, false);
}

// fix for Carbon & Pro Street Photo Mode
void __stdcall FEPhotoModeStateManager_HandleScreenTick_Hook() {
  unsigned int thethis = 0;
  _asm mov thethis, ecx;
  void* SelectCarObj = (void*)*(int*)(thethis + SELECTCAROBJ_OFFSET);
#ifdef GAME_PROSTREET
  HandleSelectCarCamera(SelectCarObj, g_Controllers[0].state.Gamepad.sThumbLX, g_Controllers[0].state.Gamepad.sThumbLY);
#else
  HandleSelectCarCamera(SelectCarObj, g_Controllers[0].state.Gamepad.sThumbRX, g_Controllers[0].state.Gamepad.sThumbRY);
#endif
}

// fix for ALL zooming (ingame and FE)
void __stdcall FEPhotoModeStateManager_HandleLTrigger_Hook() { return; }

void __stdcall FEPhotoModeStateManager_HandleRTrigger_Hook() { return; }

#endif  // GAME_UC
#pragma runtime_checks("", restore)
#endif


#ifdef GAME_UC
int(__thiscall* UCWheelConnected)(void*, unsigned int) = (int(__thiscall*)(void*, unsigned int))0x007577C0;
int __fastcall UCWheelConnectedHook(void* manager, void*, unsigned int port) {
  // DInput initialization is disabled below, leaving the Logitech manager
  // NULL. The Controls menu probes it without the guards used elsewhere.
  return manager ? UCWheelConnected(manager, port) : 0;
}
#endif

void InstallGameHooks() {
#ifdef GAME_PROSTREET
  RumbleInstallPatches(); // validate before any device can expose IFeedback
#endif

  injector::WriteMemory<unsigned int>(INPUTDEVICE_FACTORY_INITIALIZER_ADDR, (unsigned int)&InputDeviceFactory, true);
  injector::MakeCALL(INPUTMAPPING_CONSTRUCTOR_CALL_ADDR, InputMapping_Constructor, true);
#ifndef GAME_MW
  // Lower hardcoded deadzone to 0.000001 - VERY IMPORTANT
  injector::WriteMemory<unsigned int>(DEADZONE_FLOAT_POINTER_ADDR, (unsigned int)SMALL_FLOAT_ADDR, true);
#endif

  // kill DInput initialization
  injector::MakeNOP(DINPUT_KILL_ADDR, 5, true);
#ifdef GAME_UC
  // Both Controls menu variants call LogitechEZWheelWrapper with [0xDA3BF0].
  // Keep native behavior when present; an absent manager means no wheel.
  injector::MakeCALL(0x005A4A0E, UCWheelConnectedHook, true);
  injector::MakeCALL(0x005A4C4F, UCWheelConnectedHook, true);
#endif

#ifndef NO_FENG
  // Mouse stuff
  injector::MakeJMP(FEMOUSE_UPDATE_CALL_ADDR, UpdateFECursorPos, true);

  // kill mouse wheel updating from InputDevice vars
  injector::MakeNOP(FEMOUSE_WHEEL_UPDATE_ADDR, 5, true);

  // window class register hook to change the default mouse cursor
  injector::MakeNOP(REGISTERCLASSHOOK_CALL_ADDR, 6, true);
  injector::MakeCALL(REGISTERCLASSHOOK_CALL_ADDR, RegisterClassHook, true);

  if (bUseWin32Cursor) {
    // disable PC_CURSOR rendering
    injector::MakeJMP(FEMOUSE_RENDER_JMP_FROM, FEMOUSE_RENDER_JMP_TO, true);
    // disable Win32 cursor hiding
    injector::MakeJMP(WIN32_MOUSEHIDE_JMP_FROM, WIN32_MOUSEHIDE_JMP_TO, true);
#ifndef GAME_MW
    injector::MakeNOP(SHOWCURSOR_HOOK_ADDR1, 6, true);
    injector::MakeCALL(SHOWCURSOR_HOOK_ADDR1, DummyFuncStd, true);
    injector::MakeNOP(SHOWCURSOR_HOOK_ADDR2, 6, true);
    injector::MakeCALL(SHOWCURSOR_HOOK_ADDR2, DummyFuncStd, true);
#endif

    if (LastControlledDevice == LASTCONTROLLED_CONTROLLER) bConsoleFEng = true;

  } else
    SetCursor(NULL);

  // kill garage camera control with FE pad buttons (leave only the stick)
  injector::MakeJMP(FENG_SHOWCASECAM_JMP_FROM, FENG_SHOWCASECAM_JMP_TO, true);

  // FEngHud -- joy handler fix
  injector::MakeJMP(FENGHUD_JOYHANDLER_JMP_FROM, FENGHUD_JOYHANDLER_JMP_TO, true);

  // FE VISUAL STUFF

  // custom Win32 mouse cursor
  if (bUseWin32Cursor) {
    if (bUseCustomCursor)
      NFSCursor = LoadCursorFromFile(cursorSkinPath);
    else
      NFSCursor = LoadCursor(NULL, IDC_ARROW);
  }

  // FORCE CONSOLE OBJECTS TO BE RENDERABLE ON PC
  injector::WriteMemory<unsigned int>(FENG_HIDEPCOBJ_VT_ADDR, (unsigned int)&FEObjectCallback_Function, true);

#ifdef GAME_MW

  // dereference the current function after initing to maximize compatibility
  MouseStateArrayOffsetUpdater_Address = *(unsigned int*)FE_MOUSEUPDATER_CALLBACK_VT_ADDR;
  MouseStateArrayOffsetUpdater         = (bool(__thiscall*)(void*, void*))MouseStateArrayOffsetUpdater_Address;
  // it looks decieving, it's not related to mouse, it's for accessing all FEObjects during FE updating
  injector::WriteMemory<unsigned int>(FE_MOUSEUPDATER_CALLBACK_VT_ADDR, (unsigned int)&MouseStateArrayOffsetUpdater_Callback_Hook, true);

  if (bUseDynamicFEngSwitching) {
    injector::MakeNOP(CFENG_RENDEROBJ_NOP_ADDR, CFENG_RENDEOBJ_NOP_AMOUNT, true);  // cFEng render object
    injector::MakeJMP(FENG_SETVISIBLE_ADDR, FEngSetVisible, true);
    injector::MakeNOP(0x0052AEC3, 2, true);
    injector::MakeNOP(0x0052F33C, 2, true);
    injector::MakeNOP(0x0052F3FA, 6, true);
    injector::MakeNOP(0x00562C60, 2, true);
  } else {
    // make MW PC behave exactly like console version here... hide all PC elements instead
    if (LastControlledDevice == LASTCONTROLLED_CONTROLLER) {
      injector::WriteMemory<unsigned char>(0x005A45B9, FE_CONTROL_FLAG_PC, true);
      injector::WriteMemory<unsigned char>(0x00514CCD, FE_CONTROL_FLAG_PC, true);
      injector::WriteMemory<unsigned char>(0x0052AEC2, FE_CONTROL_FLAG_PC, true);
      injector::WriteMemory<unsigned char>(0x0052F33B, FE_CONTROL_FLAG_PC, true);
      injector::WriteMemory<unsigned char>(0x0052F3F9, FE_CONTROL_FLAG_PC, true);
      injector::WriteMemory<unsigned char>(0x00562C5F, FE_CONTROL_FLAG_PC, true);
    }
  }

  // remove the Controls option entry - it crashes the game
  injector::MakeJMP(0x0052905D, 0x005290B4, true);

  // hook for the performance tuning screen to show the setting names
  injector::WriteMemory<unsigned int>(0x008A2A78, (unsigned int)&CustomTuningScreen_NotificationMessage_Hook, true);
#else
  // Press START button initial hook...
  if (bEnableSplashTakeover) injector::MakeCALL(PRESS_START_HOOK_ADDR, FEngSetLanguageHash_Hook, true);

  if (bUseDynamicFEngSwitching) {
    injector::MakeNOP(CFENG_RENDEROBJ_NOP_ADDR, CFENG_RENDEOBJ_NOP_AMOUNT, true);  // cFEng render object
    injector::MakeCALL(CFENG_SERVICE_CALL_ADDR, cFEng_Service_Hook, true);
    injector::MakeCALL(FENGINE_PROCESSPADSFORPACKAGE_CALL_ADDR, FEngine_ProcessPadsForPackage_Hook, true);
#ifdef GAME_PROSTREET
    injector::MakeNOP(CFENG_RENDEROBJ_NOP2_ADDR, CFENG_RENDEOBJ_NOP_AMOUNT, true);  // cFEng render object
#endif
  } else {
    if (LastControlledDevice == LASTCONTROLLED_CONTROLLER) {
      injector::WriteMemory<unsigned char>(CFENG_RENDEROBJ_FLAG_ADDR, FE_CONTROL_FLAG_PC, true);
    }
  }
  // force analog zooming in FE orbit camera
  injector::MakeJMP(FE_ANALOGZOOM_JMP_FROM, FE_ANALOGZOOM_JMP_TO, true);
  // remove deadzone for FE activations...
  injector::WriteMemory<int>(FE_DEADZONE_POINTER_ADDR, (int)&FEActivationFloat, true);

  injector::MakeJMP(FEPHOTOMODE_HANDLESCREENTICK_HOOK_ADDR, FEPhotoModeStateManager_HandleScreenTick_Hook, true);
  injector::WriteMemory<unsigned int>(FEPHOTOMODE_HANDLELTRIGGER_HOOK_ADDR, (unsigned int)&FEPhotoModeStateManager_HandleLTrigger_Hook, true);
  injector::WriteMemory<unsigned int>(FEPHOTOMODE_HANDLERTRIGGER_HOOK_ADDR, (unsigned int)&FEPhotoModeStateManager_HandleRTrigger_Hook, true);

#ifdef GAME_CARBON
  injector::MakeCALL(0x005C3916, FEPackage_FindObjectByHash_Show_Hook, true);
  injector::MakeCALL(0x005CDD6D, FEPackage_FindObjectByHash_Hide_Hook, true);
  injector::MakeCALL(0x005B9023, FEPackage_FindObjectByHash_Hide_Hook, true);
  injector::MakeCALL(0x005AF3EE, FEPackage_FindObjectByHash_Hide_Hook, true);

  injector::MakeCALL(0x005AED20, FEPrintf_Hook_WorldMap, true);

  injector::MakeNOP(0x008577AC, 2, true);

  injector::WriteMemory<unsigned int>(FEWORLDMAPSTATEMANAGER_TICK_VT_ADDR, (unsigned int)&FEWorldMapStateManager_HandleScreenTick_Hook, true);

  if (bUseDynamicFEngSwitching) {
    injector::MakeCALL(0x005C670E, WorldMap_UnfocusQuickList_Hook, true);
    injector::MakeCALL(0x005CE0B9, WorldMap_SetQuickListInFocus_Hook, true);
    injector::MakeCALL(0x005CE309, WorldMap_SetQuickListInFocus_Hook, true);

  } else {
    if (LastControlledDevice == LASTCONTROLLED_CONTROLLER) {
      injector::WriteMemory<unsigned int>(0x005C3910, WORLDMAP_BUTTONGROUP_CONSOLE, true);
      injector::WriteMemory<unsigned int>(0x005CDD67, WORLDMAP_BUTTONGROUP_CONSOLE, true);
      injector::WriteMemory<unsigned int>(0x005B901D, WORLDMAP_BUTTONGROUP_CONSOLE, true);
      injector::WriteMemory<unsigned int>(0x005AF3E8, WORLDMAP_BUTTONGROUP_CONSOLE, true);
    }
  }

#endif  // GAME_CARBON
#ifdef GAME_PROSTREET
  // fix the "view details" string update in the FEHubMap screen
  injector::MakeCALL(0x007ECD78, FE_SetLanguageHash_Hook, true);
  injector::MakeCALL(0x007ECD59, FE_SetLanguageHash_Hook, true);
  injector::MakeCALL(0x007ECD0D, FE_SetLanguageHash_Hook, true);
  // remapping
  injector::MakeCALL(0x007028AA, GetRemappingModeHook, true);
  injector::MakeCALL(0x006FC03B, GoRemapButtonHook, true);
  injector::MakeCALL(0x6FC0E9, GoClearButtonHook, true);
  injector::MakeJMP(0x006A5940, GetNumExternalDevices, true);
  injector::MakeCALL(0x005352FF, GetNumExternalDevicesZero, true);
  injector::MakeCALL(0x005354FC, GetNumExternalDevicesZero, true);
  injector::MakeCALL(0x00535596, GetNumExternalDevicesZero, true);
  injector::MakeJMP(0x006A5950, GetXtendedInputDeviceName, true);
  injector::MakeCALL(0x0070BCEC, GetMappingStringHook, true);
  injector::MakeCALL(0x0070BD0A, GetMappingStringHook, true);
  injector::WriteMemory<uintptr_t>(0x00991AFC, (uintptr_t)&ResetMappingsToDefault, true);
  // sell car string
  injector::WriteMemory<uint32_t>(0x005C6216, 0x6DBAAF8C, true);
  injector::WriteMemory<uint32_t>(0x005D8723, 0x6DBAAF8C, true);
  injector::WriteMemory<uint32_t>(0x005DDAF1, 0x6DBAAF8C, true);
  injector::WriteMemory<uint32_t>(0x005E2B3C, 0x6DBAAF8C, true);
  injector::WriteMemory<uint32_t>(0x005EB296, 0x6DBAAF8C, true);
  injector::WriteMemory<uint32_t>(0x007EACC0, 0x6DBAAF8C, true);
  injector::WriteMemory<uint32_t>(0x007EAD0B, 0x6DBAAF8C, true);

#endif

#endif

  SetUnbindableButtonTextures();
#endif  // GAME_UC

#ifdef GAME_WORLD
  // RECALCULATE FUNCTION POINTERS -- they shift around
  UTL_Com_Object_IList_Constructor = (void*(__thiscall*)(void*, unsigned int))UTL_ILIST_CONSTRUCTOR_ADDR;
  Managed_Alloc = (void* (*)(int))MANAGED_ALLOCATOR_ADDR;

  loc_75E375 = 0x75E375 + MainBase;
  loc_75E363 = 0x75E363 + MainBase;
  loc_75E237 = 0x75E237 + MainBase;
  loc_75E225 = 0x75E225 + MainBase;
  loc_75E25F = 0x75E25F + MainBase;

  injector::MakeJMP(0x0075E358 + MainBase, UnscrewWorldDeviceScalars, true);
  injector::MakeJMP(0x0075E217 + MainBase, UnscrewWorldDeviceScalars2, true);
  injector::MakeJMP(0x0075E254 + MainBase, UnscrewWorldDeviceScalars3, true);
#endif
  if (bMouseLook) {
    // Mouselook hook
    // dereference the current function after initing to maximize compatibility
    DebugWorldCameraMover_Update_Addr = *(unsigned int*)DEBUGWORLDCAMERAMOVER_UPDATE_VT_ADDR;
    DebugWorldCameraMover_Update      = (void(__thiscall*)(void*, float))DebugWorldCameraMover_Update_Addr;
    injector::WriteMemory<unsigned int>(DEBUGWORLDCAMERAMOVER_UPDATE_VT_ADDR, (unsigned int)&DebugWorldCameraMover_Update_Hook, true);

    injector::MakeJMP(DEBUGWORLDCAMERAMOVER_CAVE1_ENTRY, DebugWorldCameraMover_Cave1, true);
    injector::MakeJMP(DEBUGWORLDCAMERAMOVER_CAVE2_ENTRY, DebugWorldCameraMover_Cave2, true);
#ifdef GAME_WORLD
    DebugWorldCameraMover_Cave1_Exit     = DEBUGWORLDCAMERAMOVER_CAVE1_EXIT;
    DebugWorldCameraMover_Cave1_FullExit = DEBUGWORLDCAMERAMOVER_CAVE1_FULLEXIT;
    DebugWorldCameraMover_Cave2_Exit     = DEBUGWORLDCAMERAMOVER_CAVE2_EXIT;
    DebugWorldCameraMover_Constructor    = (void*(__thiscall*)(void*, void*, void*, int, int))DEBUGWORLDCAMERAMOVER_CONSTRUCTOR_ADDR;
    DebugWorldCameraMover_Destructor     = (void*(__thiscall*)(void*))DEBUGWORLDCAMERAMOVER_DESTRUCTOR_ADDR;
#endif

    // implement DebugWorldCameraMover detection -- for cursor hiding
    injector::MakeCALL(DEBUGWORLDCAMERAMOVER_CONSTRUCTOR_HOOK_ADDR, DebugWorldCameraMover_Constructor_Hook, true);
    injector::MakeCALL(DEBUGWORLDCAMERAMOVER_DESTRUCTOR_HOOK_ADDR, DebugWorldCameraMover_Destructor_Hook, true);
  }

  // per-user profile configs
#if defined(GAME_CARBON) || defined(GAME_PROSTREET)
  OnLoadDoneFunc = *(uintptr_t*)ONLOADDONE_FUNC_VTABLE_ADDR;
  injector::WriteMemory<uintptr_t>(ONLOADDONE_FUNC_VTABLE_ADDR, (uintptr_t)&OnLoadDoneHook, true);

#ifdef GAME_CARBON
  injector::WriteMemory<uintptr_t>(ONLOADDONE_FUNC_VTABLE_ADDR2, (uintptr_t)&OnLoadDoneHook, true);
#endif
#endif

#if defined(HAS_CONSOLE_RUMBLE) && !defined(GAME_PROSTREET)
  RumbleInstallPatches();
#endif

#ifdef GAME_MW
  MemCardLoadAddr = reinterpret_cast<uintptr_t>(injector::MakeCALL(0x00563F33, MemoryCard_Load_Hook).get_raw<void>());
  injector::MakeCALL(0x00563CF7, MemoryCard_Load_Hook);
#endif

}
#endif
#pragma runtime_checks("", restore)
