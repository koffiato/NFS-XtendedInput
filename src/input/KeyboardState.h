#pragma once

bool InputKeyPressed(int port, unsigned int key) {
  if (!key || key >= 256) return false;
  if (KeyboardReadingMode == KB_READINGMODE_UNBUFFERED_RAW) return VKeyStates[port][key] != 0;
  if (KeyboardReadingMode == KB_READINGMODE_BUFFERED) return (VKeyStates[0][key] & 0x80) != 0;
  return (GetAsyncKeyState(key) & 0x8000) != 0;
}

bool bIgnorableVKey(int key) {
  if (key == VK_MENU || key == VK_CONTROL || key == VK_SHIFT) return true;
#if !INPUT_SCANNERS
  switch (key) {
    case VK_ESCAPE: case VK_LWIN: case VK_RWIN: case VK_APPS:
    case VK_LBUTTON: case VK_RBUTTON: case VK_MBUTTON: case VK_XBUTTON1: case VK_XBUTTON2: return true;
  }
#endif
  return false;
}

int KB_GetCurrentPressedKey() {
  for (int key = 1; key < 255; ++key)
    if (!bIgnorableVKey(key) && InputKeyPressed(0, key)) return key;
  return -1;
}

void InputRawKeyboard(const RAWINPUT& raw) {
  if (raw.header.dwType != RIM_TYPEKEYBOARD || !raw.header.hDevice) return;
  KeyboardState = true;
  InputSetActiveDevice(LASTCONTROLLED_KB);
  if (!FirstKB) FirstKB = raw.header.hDevice;
  if (bAllowTwoPlayerKB && raw.header.hDevice != FirstKB && !SecondKB) SecondKB = raw.header.hDevice;
  int port = raw.header.hDevice == SecondKB ? 1 : 0;
  unsigned int key = raw.data.keyboard.VKey;
  if (key == VK_CONTROL) key = raw.data.keyboard.Flags & RI_KEY_E0 ? VK_RCONTROL : VK_LCONTROL;
  if (key == VK_MENU) key = raw.data.keyboard.Flags & RI_KEY_E0 ? VK_RMENU : VK_LMENU;
  if (key == VK_SHIFT) key = raw.data.keyboard.MakeCode == 0x36 ? VK_RSHIFT : VK_LSHIFT;
  if (key < 256) VKeyStates[port][key] = !(raw.data.keyboard.Flags & RI_KEY_BREAK);
}

void InputRegisterKeyboard(HWND window) {
  if (KeyboardReadingMode != KB_READINGMODE_UNBUFFERED_RAW) return;
  RAWINPUTDEVICE device = {0x01, 0x06, 0, window};
  RegisterRawInputDevices(&device, 1, sizeof(device));
}
