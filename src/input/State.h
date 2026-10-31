#pragma once
#include <windows.h>
#include <XInput.h>
#if defined(GAME_UG) || defined(GAME_UG2)
#define INPUT_SCANNERS 1
#else
#define INPUT_SCANNERS 0
#endif
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
} g_Controllers[MAX_CONTROLLERS] = {};

WORD INPUT_DEADZONE_LS = (WORD)(0.24f * 32767);
WORD INPUT_DEADZONE_RS = (WORD)(0.24f * 32767);
WORD INPUT_DEADZONE_LS_P2 = (WORD)(0.24f * 32767);
WORD INPUT_DEADZONE_RS_P2 = (WORD)(0.24f * 32767);
WORD SHIFT_ANALOG_THRESHOLD = (WORD)(0.75f * 32767);
WORD FEUPDOWN_ANALOG_THRESHOLD = (WORD)(0.50f * 32767);
WORD TRIGGER_ACTIVATION_THRESHOLD = (WORD)(0.12f * 255);
WORD bQuitButtonOldState = 0;

BYTE VKeyStates[2][256] = {};
unsigned int KeyboardReadingMode = 0;
unsigned int LastControlledDevice = 0;
unsigned int LastControlledDeviceOldState = 0;
unsigned int ControllerIconMode = 0;
bool KeyboardState = true;
bool bAllowTwoPlayerKB = false;
HANDLE FirstKB = NULL;
HANDLE SecondKB = NULL;
bool bXInputOmniMode = false;
bool bPassConnStatus = true;
bool bGlobalDoPolling = true;
bool bBlockThisPoll = false;
bool bInDebugWorldCamera = false;
bool bEnableSplashTakeover = true;
bool bConsoleFEng = false;
bool bMouseLook = true;
float MouseLookSensitivity = 1.0f;
int MouseLook_XSpeed = 0;
int MouseLook_YSpeed = 0;

RECT windowRect;
bool bMousePressedDown = false;
bool bMousePressedDownOldState = false;
bool bMouse2PressedDown = false;
bool bMouse2PressedDownOldState = false;
bool bMouse3PressedDown = false;
bool bMouse3PressedDownOldState = false;
bool bConfineMouse = false;
int TimeSinceLastMouseMovement = 0;
int MouseWheelValue = 0;
float fFEScale = 1.0f;
bool bUseWin32Cursor = true;
bool bUseCustomCursor = true;
bool bLastUsedVirtualMouse = false;
bool bUseDynamicFEngSwitching = !INPUT_SCANNERS;
bool bEnableMouseHiding = true;
HCURSOR NFSCursor;
#define MOUSEHIDE_TIME 5000

float GetAnalogStickValue(int port, WORD binding) {
  return InputAnalogValue(g_Controllers[port].state.Gamepad, binding);
}
uint16_t GetAnalogActivity() { return InputAnalogActivity(g_Controllers[0].state.Gamepad); }
bool bIsAnyAnalogActive() {
  const XINPUT_GAMEPAD& p = g_Controllers[0].state.Gamepad;
  return p.sThumbLX || p.sThumbLY || p.sThumbRX || p.sThumbRY || p.bLeftTrigger || p.bRightTrigger;
}

void InputSetActiveDevice(unsigned int device) {
  LastControlledDevice = device;
  if (bUseDynamicFEngSwitching) bConsoleFEng = device == LASTCONTROLLED_CONTROLLER;
}
