// Shared input storage and normalized controller access. MIT; see LICENSE.
#include "../_vs/stdafx.h"
#include "State.h"

// Controller state and activation thresholds.
CONTROLLER_STATE g_Controllers[MAX_CONTROLLERS] = {};
WORD INPUT_DEADZONE_LS = (WORD)(ConfigDefaults::Deadzone::PercentLS * 32767);
WORD INPUT_DEADZONE_RS = (WORD)(ConfigDefaults::Deadzone::PercentRS * 32767);
WORD INPUT_DEADZONE_LS_P2 = (WORD)(ConfigDefaults::Deadzone::PercentLS_P2 * 32767);
WORD INPUT_DEADZONE_RS_P2 = (WORD)(ConfigDefaults::Deadzone::PercentRS_P2 * 32767);
WORD SHIFT_ANALOG_THRESHOLD = (WORD)(ConfigDefaults::Deadzone::Percent_Shifting * 32767);
WORD FEUPDOWN_ANALOG_THRESHOLD = (WORD)(ConfigDefaults::Deadzone::Percent_AnalogStickDigital * 32767);
WORD TRIGGER_ACTIVATION_THRESHOLD = (WORD)(ConfigDefaults::Deadzone::Percent_AnalogTriggerDigital * 255);
WORD bQuitButtonOldState = 0;

// Keyboard and active-device state.
BYTE VKeyStates[2][256] = {};
unsigned int KeyboardReadingMode = ConfigDefaults::Input::KeyboardReadingMode;
unsigned int LastControlledDevice = ConfigDefaults::Icons::FirstControlDevice;
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

// Cursor and frontend state.
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
bool bCursorHidden = false;
HCURSOR NFSCursor;

#if defined(GAME_MW) || defined(GAME_CARBON) || defined(GAME_UG)
bool bHaveBuiltinIcons = true;
#else
bool bHaveBuiltinIcons = false;
#endif

IconFallbackMode CurrentIconMode = ICON_MODE_CUSTOM;
bool bCustomButtonTexLoaded = false;

#if defined(GAME_UG)
char ButtonTexFilename[128] = "GLOBAL\\UG_ConsoleButtons.tpk";
#else
char ButtonTexFilename[128] = "GLOBAL\\XtendedInputButtons.tpk";
#endif

bool HasBuiltinIcons() {
  return bHaveBuiltinIcons;
}

void ResolveCursorFallback() {
  if (bUseWin32Cursor && bUseCustomCursor) {
    DWORD attr = GetFileAttributesA(cursorSkinPath);
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
      bUseWin32Cursor = false;
      bUseCustomCursor = false;
      NFSCursor = NULL;
      return;
    }
    NFSCursor = LoadCursorFromFileA(cursorSkinPath);
    if (!NFSCursor) {
      bUseWin32Cursor = false;
      bUseCustomCursor = false;
      return;
    }
  } else if (bUseWin32Cursor) {
    NFSCursor = LoadCursor(NULL, IDC_ARROW);
  } else {
    NFSCursor = NULL;
  }
}

void ResolveIconFallback() {
  DWORD attr = GetFileAttributesA(ButtonTexFilename);
  if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
    CurrentIconMode = ICON_MODE_CUSTOM;
  } else if (bHaveBuiltinIcons) {
    CurrentIconMode = ICON_MODE_BUILTIN;
  } else {
    CurrentIconMode = ICON_MODE_PLAINTEXT;
    bConsoleFEng = false;
  }
}

float GetAnalogStickValue(int port, WORD binding) {
  return InputAnalogValue(g_Controllers[port].state.Gamepad, binding);
}

uint16_t GetAnalogActivity(int port) {
  if (port < 0 || port >= MAX_CONTROLLERS) port = 0;
  return InputAnalogActivity(g_Controllers[port].state.Gamepad);
}

void InputSetActiveDevice(unsigned int device) {
  LastControlledDevice = device;
  if (device == LASTCONTROLLED_CONTROLLER && bEnableMouseHiding) {
    TimeSinceLastMouseMovement = 0;
  }
  if (bUseDynamicFEngSwitching) {
    if (CurrentIconMode == ICON_MODE_PLAINTEXT)
      bConsoleFEng = false;
    else
      bConsoleFEng = (device == LASTCONTROLLED_CONTROLLER);
  }
}

char defMappingName[MAX_PATH] = "scripts\\NFS_XtendedInput.default.ini";
