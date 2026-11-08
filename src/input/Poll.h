#pragma once
#include <cstdlib>

void InputDeadzone(SHORT& x, SHORT& y, int limit, bool paired) {
  bool insideX = abs((int)x) < limit, insideY = abs((int)y) < limit;
  if (paired) { if (insideX && insideY) x = y = 0; }
  else { if (insideX) x = 0; if (insideY) y = 0; }
}

bool InputPadActive(const XINPUT_GAMEPAD& p) {
  return p.wButtons || p.sThumbLX || p.sThumbLY || p.sThumbRX || p.sThumbRY || p.bLeftTrigger || p.bRightTrigger;
}

void InputCombinePads(XINPUT_GAMEPAD& a, const XINPUT_GAMEPAD& b) {
  a.wButtons |= b.wButtons;
  a.sThumbLX |= b.sThumbLX; a.sThumbLY |= b.sThumbLY;
  a.sThumbRX |= b.sThumbRX; a.sThumbRY |= b.sThumbRY;
  a.bLeftTrigger |= b.bLeftTrigger; a.bRightTrigger |= b.bRightTrigger;
}

HRESULT UpdateControllerState() {
  XINPUT_STATE combined = {};
  bool connected = false;
  for (int port = 0; port < (bXInputOmniMode ? MAX_CONTROLLERS : 2); ++port) {
    CONTROLLER_STATE& controller = g_Controllers[port];
    controller.state = {};
    controller.bConnected = XInputGetState(port, &controller.state) == ERROR_SUCCESS;
    if (!controller.bConnected) { controller.state = {}; continue; }
    XINPUT_GAMEPAD& pad = controller.state.Gamepad;
    InputDeadzone(pad.sThumbLX, pad.sThumbLY, port == 1 ? INPUT_DEADZONE_LS_P2 : INPUT_DEADZONE_LS, INPUT_SCANNERS);
    InputDeadzone(pad.sThumbRX, pad.sThumbRY, port == 1 ? INPUT_DEADZONE_RS_P2 : INPUT_DEADZONE_RS, INPUT_SCANNERS);
    connected = true;
    if ((port == 0 || bXInputOmniMode) && InputPadActive(pad)) {
      InputSetActiveDevice(LASTCONTROLLED_CONTROLLER);
#ifdef HAS_RUMBLE
      RumbleLastActivePad = port;
#endif
    }
    if (bXInputOmniMode) InputCombinePads(combined.Gamepad, pad);
  }
  if (bXInputOmniMode) {
    for (int port = 0; port < 2; ++port) g_Controllers[port] = {combined, connected};
  }
  return S_OK;
}

void ReadXInput_Extra();
void UpdateGameInput();
void UpdateGameDeviceStatus();
void __stdcall ReadControllerData() {
  if (!bGlobalDoPolling) return;
  if (bBlockThisPoll) { bBlockThisPoll = false; return; }
  UpdateControllerState();
  UpdateGameDeviceStatus();
#if INPUT_SCANNERS
  RumblePoll();
#endif
  ReadXInput_Extra();
  if (KeyboardReadingMode == KB_READINGMODE_BUFFERED) GetKeyboardState(VKeyStates[0]);
  UpdateGameInput();
}
