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
} g_Controllers[MAX_CONTROLLERS] = {};

constexpr float DEFAULT_STICK_DEADZONE = ConfigDefaults::Deadzone::PercentLS;
WORD INPUT_DEADZONE_LS = (WORD)(ConfigDefaults::Deadzone::PercentLS * 32767);
WORD INPUT_DEADZONE_RS = (WORD)(ConfigDefaults::Deadzone::PercentRS * 32767);
WORD INPUT_DEADZONE_LS_P2 = (WORD)(ConfigDefaults::Deadzone::PercentLS_P2 * 32767);
WORD INPUT_DEADZONE_RS_P2 = (WORD)(ConfigDefaults::Deadzone::PercentRS_P2 * 32767);
WORD SHIFT_ANALOG_THRESHOLD = (WORD)(ConfigDefaults::Deadzone::Percent_Shifting * 32767);
WORD FEUPDOWN_ANALOG_THRESHOLD = (WORD)(ConfigDefaults::Deadzone::Percent_AnalogStickDigital * 32767);
WORD TRIGGER_ACTIVATION_THRESHOLD = (WORD)(ConfigDefaults::Deadzone::Percent_AnalogTriggerDigital * 255);
WORD bQuitButtonOldState = 0;

BYTE VKeyStates[2][256] = {};
unsigned int KeyboardReadingMode = ConfigDefaults::Input::KeyboardReadingMode;
unsigned int LastControlledDevice = ConfigDefaults::Icons::FirstControlDevice;
unsigned int LastControlledDeviceOldState = 0;
unsigned int ControllerIconMode = ConfigDefaults::Icons::ControllerIconMode;
bool KeyboardState = true;
bool bAllowTwoPlayerKB = ConfigDefaults::Input::AllowTwoPlayerKB;
HANDLE FirstKB = NULL;
HANDLE SecondKB = NULL;
bool bXInputOmniMode = ConfigDefaults::Input::XInputOmniMode;
bool bPassConnStatus = ConfigDefaults::Input::PassConnStatus;
bool bGlobalDoPolling = true;
bool bBlockThisPoll = false;
bool bInDebugWorldCamera = false;
bool bEnableSplashTakeover = ConfigDefaults::Icons::EnableSplashTakeover;
bool bConsoleFEng = false;
bool bMouseLook = ConfigDefaults::Input::MouseLook;
float MouseLookSensitivity = ConfigDefaults::Input::MouseLookSensitivity;
int MouseLook_XSpeed = 0;
int MouseLook_YSpeed = 0;

RECT windowRect;
bool bMousePressedDown = false;
bool bMousePressedDownOldState = false;
bool bMouse2PressedDown = false;
bool bMouse2PressedDownOldState = false;
bool bMouse3PressedDown = false;
bool bMouse3PressedDownOldState = false;
bool bConfineMouse = ConfigDefaults::Input::ConfineMouse;
int TimeSinceLastMouseMovement = 0;
int MouseWheelValue = 0;
float fFEScale = 1.0f;
bool bUseWin32Cursor = ConfigDefaults::Input::UseWin32Cursor;
bool bUseCustomCursor = ConfigDefaults::Input::UseCustomCursor;
bool bLastUsedVirtualMouse = false;
bool bUseDynamicFEngSwitching = ConfigDefaults::Icons::UseDynamicFEngSwitching;
bool bEnableMouseHiding = ConfigDefaults::Input::EnableMouseHiding;
HCURSOR NFSCursor;
#define MOUSEHIDE_TIME 5000

float GetAnalogStickValue(int port, WORD binding) {
  return InputAnalogValue(g_Controllers[port].state.Gamepad, binding);
}
uint16_t GetAnalogActivity(int port = 0) {
  if (port < 0 || port >= MAX_CONTROLLERS) port = 0;
  return InputAnalogActivity(g_Controllers[port].state.Gamepad);
}
void InputSetActiveDevice(unsigned int device) {
  LastControlledDevice = device;
  if (bUseDynamicFEngSwitching) bConsoleFEng = device == LASTCONTROLLED_CONTROLLER;
}
