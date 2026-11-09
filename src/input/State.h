/*
// clang-format off
//
//    MIT License
//    Need for Speed (Black Box, MW & newer) - Xtended Input plugin
//    Bringing native XInput to NFS
//
//    Copyright (c) 2022-2023 Lovro Plese (Xan/Tenjoin)
//    Copyright (c) 2023 Berkay Yigit <mail@berkay.link>
//
// clang-format on
*/

#pragma once
#include <windows.h>
#include <XInput.h>
#include "ConfigDefaults.h"
#include "Analog.h"

#define MAX_CONTROLLERS 4
#define LASTCONTROLLED_KB 0
#define LASTCONTROLLED_CONTROLLER 1
#define KB_READINGMODE_UNBUFFERED_ASYNC 0
#define KB_READINGMODE_BUFFERED 1
#define KB_READINGMODE_UNBUFFERED_RAW 2
#define CONTROLLERICON_XBOXONE 0
#define CONTROLLERICON_PS4 1
#define CONTROLLERICON_XBOX360 2
#define CONTROLLERICON_PS3 3
#define CONTROLLERICON_SWITCH 4
#define CONTROLLERICON_XB360MW 5

struct CONTROLLER_STATE {
  XINPUT_STATE state;
  bool bConnected;
};
extern CONTROLLER_STATE g_Controllers[MAX_CONTROLLERS];

extern WORD INPUT_DEADZONE_LS;
extern WORD INPUT_DEADZONE_RS;
extern WORD INPUT_DEADZONE_LS_P2;
extern WORD INPUT_DEADZONE_RS_P2;
extern WORD SHIFT_ANALOG_THRESHOLD;
extern WORD FEUPDOWN_ANALOG_THRESHOLD;
extern WORD TRIGGER_ACTIVATION_THRESHOLD;
extern WORD bQuitButtonOldState;

extern BYTE VKeyStates[2][256];
extern unsigned int KeyboardReadingMode;
extern unsigned int LastControlledDevice;
extern unsigned int ControllerIconMode;
extern bool KeyboardState;
extern bool bAllowTwoPlayerKB;
extern HANDLE FirstKB;
extern HANDLE SecondKB;
extern bool bXInputOmniMode;
extern bool bPassConnStatus;
extern bool bGlobalDoPolling;
extern bool bBlockThisPoll;
extern bool bInDebugWorldCamera;
extern bool bEnableSplashTakeover;
extern bool bConsoleFEng;
extern bool bMouseLook;
extern float MouseLookSensitivity;
extern int MouseLook_XSpeed;
extern int MouseLook_YSpeed;

extern RECT windowRect;
extern bool bMousePressedDown;
extern bool bMousePressedDownOldState;
extern bool bMouse2PressedDown;
extern bool bMouse2PressedDownOldState;
extern bool bMouse3PressedDown;
extern bool bMouse3PressedDownOldState;
extern bool bConfineMouse;
extern int TimeSinceLastMouseMovement;
extern int MouseWheelValue;
extern float fFEScale;
extern bool bUseWin32Cursor;
extern bool bUseCustomCursor;
extern bool bLastUsedVirtualMouse;
extern bool bUseDynamicFEngSwitching;
extern bool bEnableMouseHiding;
extern HCURSOR NFSCursor;
#define MOUSEHIDE_TIME 5000

float GetAnalogStickValue(int port, WORD binding);
uint16_t GetAnalogActivity(int port = 0);
void InputSetActiveDevice(unsigned int device);

#define FE_SPLASH_TEXT_XBOX    "Press MENU or A button"
#define FE_SPLASH_TEXT_XBOX360 "Press START or A button"
#define FE_SPLASH_TEXT_PS4     "Press OPTIONS or X button"
#define FE_SPLASH_TEXT_PS3     "Press START or X button"
#define FE_SPLASH_TEXT_SWITCH  "Press + button"
#define FE_SPLASH_TEXT_PC      "CLICK or ENTER to continue"

extern char defMappingName[MAX_PATH];
constexpr const char* userMappingName = "NFS_XtendedInput.usermap.ini";
constexpr const char* userMappingDir  = "scripts\\XtendedInputMaps";
constexpr const char* cursorSkinPath  = "scripts\\nfs_cursor.cur";
