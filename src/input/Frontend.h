#pragma once

#if INPUT_SCANNERS
void(*InitJoystick)() = (void(*)())INITJOY_ADDR;


// UG1 uses fastcall and works without this so we won't apply it to UG1
#ifndef GAME_UG
ScannerConfig* FindScannerConfig_Custom(unsigned int inJoyEvent, int confignum, int unk)
{
	for (unsigned int i = 0; i < MAX_JOY_EVENT; i++)
	{
		if (ScannerConfigs[i].JoyEvent == inJoyEvent)
			return &(ScannerConfigs[i]);
	}

	return 0;
}
#endif

#ifdef GAME_UG
struct ActualJoypadData
{
	uint32_t destructor;
	char unkdata1[0x130];
	uint32_t type;
	char name[32];
	char unkdata2[0xE4];
	uint32_t config_index;
	uint32_t secondary;
	char unkdata3[0x2C];
}settings_controllers[3];

int settings_cave1_exit = 0x00413C1D;
int settings_cave1_exit_2 = 0x413AF1;
void __declspec(naked) settings_cave1()
{
	_asm
	{
		cmp edx, 2
		mov [esp+0x18], ebx
		mov [esp+0x1C], eax
		jnz settings_cont
		jmp settings_cave1_exit

		settings_cont:
		jmp settings_cave1_exit_2;
	}
}

uint32_t cur_setting_idx;
uint32_t cur_setting_idx_secondary;

// entry: 0x004141DF
int settings_cave2_exit = 0x00414217;
void __declspec(naked) settings_cave2()
{
	_asm
	{
		mov [esi+0x84], eax
		mov eax, ds:0x736344
		mov byte ptr [esi+0x80], 1
		mov cur_setting_idx, ecx
		mov cur_setting_idx_secondary, edx
		jmp settings_cave2_exit
	}
}

// entry: 0x004144EC
int settings_cave3_exit = 0x00414507;
void __declspec(naked) settings_cave3()
{
	_asm
	{
		//mov eax, ds:0x736344
		mov esi, cur_setting_idx
		jmp settings_cave3_exit
	}
}

#pragma runtime_checks( "", off )
#define JOYCONFIG_EMPTYSTR "Not bindable"
// joy config menu
// entrypoint: 0x00414830

void __stdcall SetConfigButtonText(int JoyMapIndex, int Secondary)
{
	unsigned int ObjHash = 0;
	_asm mov ObjHash, ecx

	char* FEngPkgName = "PC_JoyPad_Config.fng";
	if (*(int*)CONFIG_JOY_INDEX_ADDR == 0)
		FEngPkgName = "PC_Keyboard_Config.fng";

	if (*(int*)CONFIG_JOY_INDEX_ADDR == 0)
	{
		// Keyboard config

		if ((JoyMapIndex == 3) || (JoyMapIndex == 4))
		{
			if (JoyMapIndex == 3)
			{
				if (Secondary == 0)
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ControlsTextsPC[SteerLeftVKey]);
				else
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), JOYCONFIG_EMPTYSTR);
			}
			if (JoyMapIndex == 4)
			{
				if (Secondary == 0)
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ControlsTextsPC[SteerRightVKey]);
				else
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), JOYCONFIG_EMPTYSTR);
			}
		}
		else
		{
			if (Secondary == 0)
				FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ControlsTextsPC[ScannerConfigs[MapKeyboardConfigToEvent(JoyMapIndex)].keycode]);
			else
				FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), JOYCONFIG_EMPTYSTR);
		}
	}
	else
	{
		// JoyPad config
		if ((JoyMapIndex >= 1) && (JoyMapIndex <= 4))
		{
			if (JoyMapIndex == 1)
			{
				if (Secondary == 0)
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ConvertBitmaskToControlString(ControllerIconMode, ScannerConfigs[MapJoypadConfigToEvent(JoyMapIndex)].BitmaskStuff));
				else
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ConvertBitmaskToControlString(ControllerIconMode, ScannerConfigs[MapSecondaryJoypadConfigToEvent(JoyMapIndex)].BitmaskStuff));
			}
			if (JoyMapIndex == 2)
			{
				if (Secondary == 0)
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ConvertBitmaskToControlString(ControllerIconMode, ScannerConfigs[MapJoypadConfigToEvent(JoyMapIndex)].BitmaskStuff));
				else
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ConvertBitmaskToControlString(ControllerIconMode, ScannerConfigs[MapSecondaryJoypadConfigToEvent(JoyMapIndex)].BitmaskStuff));
			}
			if (JoyMapIndex == 3)
			{
				if (Secondary == 0)
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ConvertBitmaskToControlString(ControllerIconMode, SteerLeftAxis));
				else
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ConvertBitmaskToControlString(ControllerIconMode, SteerLeftButton));
			}
			if (JoyMapIndex == 4)
			{
				if (Secondary == 0)
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ConvertBitmaskToControlString(ControllerIconMode, SteerRightAxis));
				else
					FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ConvertBitmaskToControlString(ControllerIconMode, SteerRightButton));
			}
		}
		else
		{
			if (Secondary == 0)
				FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), ConvertBitmaskToControlString(ControllerIconMode, ScannerConfigs[MapJoypadConfigToEvent(JoyMapIndex)].BitmaskStuff));
			else
				FEPrintf((void*)FEngFindString(FEngPkgName, ObjHash), JOYCONFIG_EMPTYSTR);
		}
	}
}
#pragma runtime_checks( "", restore )
#endif


void InitCustomKBInput() {
  InitJoystick();
  *(int*)DEVICE_COUNT_ADDR = 2;
  InputRegisterKeyboard(*(HWND*)GAME_HWND_ADDR);
}

#else
#ifndef GAME_WORLD
bool bIsHudVisible() {
  if (*(int*)GAMEFLOWMANAGER_STATUS_ADDR == 6) {
    int FirstLocalPlayer = **(int**)PLAYER_ILISTABLE_ADDR;
    if (FirstLocalPlayer) {
      int LocalPlayerVtable                           = *(int*)(FirstLocalPlayer);
      int(__thiscall * LocalPlayer_GetHUD)(void* dis) = (int(__thiscall*)(void*)) * (int*)(LocalPlayerVtable + PLAYER_GETHUD_VTABLE_OFFSET);
      int LocalPlayerHUD                              = LocalPlayer_GetHUD((void*)FirstLocalPlayer);
      if (LocalPlayerHUD) {
        int LocalPlayerHUDVtable                          = *(int*)(LocalPlayerHUD);
        int(__thiscall * FEngHud_IsHudVisible)(void* dis) = (int(__thiscall*)(void*)) * (int*)(LocalPlayerHUDVtable + FENGHUD_ISVISIBLE_VTABLE_OFFSET);

        if (FEngHud_IsHudVisible((void*)LocalPlayerHUD))  // is HUD being drawn at all
          return true;
      }
    }
  }
  return false;
}
#else
bool bIsHudVisible() { return false; }

// world, for whatever reason, uses direct pointers to the class instead of accessing the device scalar array normally
// copied this code snippet from Carbon 0x00696166
unsigned int loc_75E375 = 0x75E375 + MainBase;
unsigned int loc_75E363 = 0x75E363 + MainBase;
void __declspec(naked) UnscrewWorldDeviceScalars() {
  _asm
  {
        mov ecx, [ebx+0x18]
        shl esi, 4
        add esi, ecx
        test esi, esi
        jnz unscrew_jnz
        jmp loc_75E363
        unscrew_jnz:
        jmp loc_75E375
  }
}

unsigned int loc_75E237 = 0x75E237 + MainBase;
unsigned int loc_75E225 = 0x75E225 + MainBase;
void __declspec(naked) UnscrewWorldDeviceScalars2() {
  _asm
  {
        mov ecx, [ebx + 0x18]
        shl edi, 4
        add edi, ecx
        mov [ebp-8], edi
        test edi, edi
        jnz unscrew2_jnz
        jmp loc_75E225
        unscrew2_jnz :
        jmp loc_75E237
  }
}

unsigned int loc_75E25F = 0x75E25F + MainBase;
void __declspec(naked) UnscrewWorldDeviceScalars3() {
  _asm
  {
        mov edx, [ebx + 0x18]
        shl edi, 4
        add edi, edx
        jmp loc_75E25F
  }
}

#endif
// Mouselook stuff start

#pragma runtime_checks("", off)
float DebugWorldCameraMover_16bit_max  = 65536.0f;
bool  bDisableDebugWorldCameraUpdating = true;

// caves necessary as original values are in 16-bit values, this extends it to 32-bits

int DebugWorldCameraMover_Cave1_Exit     = DEBUGWORLDCAMERAMOVER_CAVE1_EXIT;
int DebugWorldCameraMover_Cave1_FullExit = DEBUGWORLDCAMERAMOVER_CAVE1_FULLEXIT;
void __declspec(naked) DebugWorldCameraMover_Cave1() {
  // clang-format off
  if (bDisableDebugWorldCameraUpdating) _asm jmp DebugWorldCameraMover_Cave1_FullExit;
#ifndef DEBUGWORLDCAMERAMOVER_MMX
  _asm {
      mov eax, MouseLook_XSpeed
#ifdef DEBUGWORLDCAMERAMOVER_FSTP
      fstp st
#endif
      mov[esp + DEBUGWORLDCAMERAMOVER_STACK_OFFSET], eax
      jmp DebugWorldCameraMover_Cave1_Exit
  }
#else  // NFS Undercover and World use the MMX registers and SSE2 instructions extensively
#ifdef GAME_UC
  _asm {
      movss xmm2, dword ptr[esp + 0x28]
      cvtps2pd xmm0, xmm0
      mov eax, MouseLook_XSpeed
      cvtsi2ss xmm1, eax
      jmp DebugWorldCameraMover_Cave1_Exit
  }
#endif
#ifdef GAME_WORLD
  _asm {
    movss xmm1, DebugWorldCameraMover_16bit_max
    movss xmm0, dword ptr[ebp - 0x18]
    mulss xmm0, xmm1 
    cvttss2si eax, xmm0
    movss xmm0, dword ptr[ebp - 0x20]
    movaps xmm2, xmm0
    mulss xmm2, xmm0
    movss xmm0, dword ptr[ebp - 0x1C]
    movaps xmm3, xmm0 
    mulss xmm3, xmm0
    addss xmm2, xmm3
    mov ecx, MouseLook_XSpeed
    add edi, ecx
    jmp DebugWorldCameraMover_Cave1_Exit
  }
#endif
#endif
  // clang-format on
}

int DebugWorldCameraMover_Cave2_Exit = DEBUGWORLDCAMERAMOVER_CAVE2_EXIT;
void __declspec(naked) DebugWorldCameraMover_Cave2() {
#ifndef DEBUGWORLDCAMERAMOVER_MMX
  // clang-format off
  _asm
  {
        mov eax, MouseLook_YSpeed
        mov	[esp+DEBUGWORLDCAMERAMOVER_STACK_OFFSET2], eax
        jmp DebugWorldCameraMover_Cave2_Exit
  }
#else  // NFS Undercover and World use the MMX registers and SSE2 instructions extensively
#ifdef GAME_UC
  _asm
  {
        mov edx, MouseLook_YSpeed
        movzx ecx, ax
        cvtsi2ss xmm0, edx
        jmp DebugWorldCameraMover_Cave2_Exit
  }
#endif
#ifdef GAME_WORLD
  _asm { 
    add eax, MouseLook_YSpeed
    mov[ebp - 4], eax
    jmp DebugWorldCameraMover_Cave2_Exit 
  }
#endif
#endif
  // clang-format on
}

// during DebugWorldCameraMover - set the cursor position to the center of the window for relative calculations
void __stdcall DebugWorldCameraMover_Update_Hook(float unk) {
  unsigned int thethis;
  _asm mov     thethis, ecx;

  // add original speeds
  MouseLook_XSpeed += *(short*)(thethis + DEBUGWORLDCAMERAMOVER_XSPEED_OFFSET);
  MouseLook_YSpeed += *(short*)(thethis + DEBUGWORLDCAMERAMOVER_YSPEED_OFFSET);

#ifndef DEBUGWORLDCAMERAMOVER_MMX
  if (!MouseLook_XSpeed && !MouseLook_YSpeed && !CurrValues[0][DEBUGACTION_MOVE_UP] && !CurrValues[0][DEBUGACTION_MOVE_DOWN])
    bDisableDebugWorldCameraUpdating = true;
  else
    bDisableDebugWorldCameraUpdating = false;
#else
  if (!MouseLook_XSpeed && !MouseLook_YSpeed && !CurrValues[0][DEBUGACTION_MOVE_UP] && !CurrValues[0][DEBUGACTION_MOVE_DOWN] &&
      !CurrValues[0][DEBUGACTION_MOVE_LEFT] && !CurrValues[0][DEBUGACTION_MOVE_RIGHT])
    bDisableDebugWorldCameraUpdating = true;
  else
    bDisableDebugWorldCameraUpdating = false;
#endif
  return DebugWorldCameraMover_Update((void*)thethis, unk);
}

// constructor/destructor hooks for mouse hiding

void* __stdcall DebugWorldCameraMover_Constructor_Hook(void* vector1, void* vector2, int joyport, int unk) {
  unsigned int thethis;
  _asm mov     thethis, ecx;
  bInDebugWorldCamera = true;
  RECT windowRect;
  GetWindowRect(*(HWND*)GAME_HWND_ADDR, &windowRect);
  int CenterX = ((windowRect.right - windowRect.left) / 2) + windowRect.left;
  int CenterY = ((windowRect.bottom - windowRect.top) / 2) + windowRect.top;
  SetCursorPos(CenterX, CenterY);

  return DebugWorldCameraMover_Constructor((void*)thethis, vector1, vector2, joyport, unk);
}
void* DebugWorldCameraMover_Destructor_Hook() {
  unsigned int thethis;
  _asm mov     thethis, ecx;

  bInDebugWorldCamera = false;

  return DebugWorldCameraMover_Destructor((void*)thethis);
}

#pragma runtime_checks("", restore)

// Mouselook stuff end
#endif

#ifdef GAME_UG
void HandleInGameConfigMenu();
#endif

void UpdateGameDeviceStatus() {
#if INPUT_SCANNERS
  *(int*)DEVICE_COUNT_ADDR = g_Controllers[0].bConnected ? 2 : 1;
  *(unsigned char*)JOYSTICKTYPE_P1_ADDR = KeyboardState || g_Controllers[0].bConnected ? 1 : 0xFF;
  *(unsigned char*)JOYSTICKTYPE_P2_ADDR = g_Controllers[1].bConnected || SecondKB ? 0 : 0xFF;
#endif
}

void ReadXInput_Extra() {
  if (!g_Controllers[0].bConnected || !bGlobalDoPolling) return;
  const XINPUT_GAMEPAD& pad = g_Controllers[0].state.Gamepad;
#ifndef NO_QUIT_BUTTON
  WORD back = pad.wButtons & XINPUT_GAMEPAD_BACK;
  if (back && back != bQuitButtonOldState) {
#ifdef GAME_UG
    *(char*)FE_WINDOWS_KEY_CODE_ADDR = 'Q';
#else
    FESendKeystroke('Q');
#endif
  }
  bQuitButtonOldState = back;
#endif
#ifdef GAME_UG
  if (*(int*)GAMEFLOWMANAGER_STATUS_ADDR == 3) {
    if (pad.wButtons & XINPUT_GAMEPAD_Y) *(char*)FE_WINDOWS_KEY_CODE_ADDR = 'D';
    if (pad.bLeftTrigger > TRIGGER_ACTIVATION_THRESHOLD) *(char*)FE_WINDOWS_KEY_CODE_ADDR = 'T';
  }
#endif
#ifdef GAME_MW
    // simulate mouse cursor for map movement
    if (*(int*)GAMEFLOWMANAGER_STATUS_ADDR == 6) {
      if (g_Controllers[0].state.Gamepad.sThumbRX || g_Controllers[0].state.Gamepad.sThumbRY) bLastUsedVirtualMouse = true;

      if (bLastUsedVirtualMouse && LastControlledDevice == LASTCONTROLLED_CONTROLLER && cFEng_FindPackage("WorldMapMain.fng")) {
        if (bUseWin32Cursor) {
          if (!bEnteredWorldMapOnce && !bConfineMouse)  // force confine a mouse if not already, we don't want the mouse to stray away from the game if we're
                                                        // using a controller
          {
            RECT windowRect;
            GetWindowRect(*(HWND*)GAME_HWND_ADDR, &windowRect);
            ClipCursor(&windowRect);
          }

          POINT MousePos;
          GetCursorPos(&MousePos);
          SetCursorPos(MousePos.x + ((g_Controllers[0].state.Gamepad.sThumbRX) / 0x1000), MousePos.y + (-g_Controllers[0].state.Gamepad.sThumbRY / 0x1000));
        } else {
          *(int*)FEMOUSECURSOR_X_ADDR += (g_Controllers[0].state.Gamepad.sThumbRX / 0x1000);
          *(int*)FEMOUSECURSOR_Y_ADDR += (-g_Controllers[0].state.Gamepad.sThumbRY / 0x1000);
        }

        bMousePressedDown    = g_Controllers[0].state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
        bEnteredWorldMapOnce = true;
      } else {
        if (bEnteredWorldMapOnce && !bConfineMouse && bUseWin32Cursor) ClipCursor(NULL);
        bEnteredWorldMapOnce  = false;
        bLastUsedVirtualMouse = false;
      }
    } else {
      if (bEnteredWorldMapOnce && !bConfineMouse && bUseWin32Cursor) ClipCursor(NULL);
      bEnteredWorldMapOnce  = false;
      bLastUsedVirtualMouse = false;
    }

#endif
}

void UpdateGameInput() {
#ifdef GAME_UG
  if (*(int*)GAMEFLOWMANAGER_STATUS_ADDR == 3) HandleInGameConfigMenu();
#elif !INPUT_SCANNERS
  // read mouse info for Debug World Camera
  if (bInDebugWorldCamera && bGlobalDoPolling) {
    RECT  windowRect;
    POINT MousePos;
    GetCursorPos(&MousePos);
    GetWindowRect(*(HWND*)GAME_HWND_ADDR, &windowRect);
    int CenterX = ((windowRect.right - windowRect.left) / 2) + windowRect.left;
    int CenterY = ((windowRect.bottom - windowRect.top) / 2) + windowRect.top;

#ifndef GAME_WORLD
    // add mouse speed to the original speed
    MouseLook_XSpeed = -(int)((float)((MousePos.x - CenterX) * 1000) * MouseLookSensitivity);
    MouseLook_YSpeed = -(int)((float)((MousePos.y - CenterY) * 1000) * MouseLookSensitivity);
#else
    MouseLook_XSpeed = -(int)((float)((MousePos.x - CenterX) * 100) * MouseLookSensitivity);
    MouseLook_YSpeed = -(int)((float)((MousePos.y - CenterY) * 100) * MouseLookSensitivity);
#endif
    SetCursorPos(CenterX, CenterY);
  }

#endif
}
