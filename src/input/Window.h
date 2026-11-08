#pragma once
#include <vector>
#include <cstddef>

LRESULT(WINAPI* GameWndProc)(HWND, UINT, WPARAM, LPARAM);

LRESULT WINAPI CustomWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
#ifdef HAS_RUMBLE
  if (message == WM_SETFOCUS || message == WM_ACTIVATEAPP) bRumbleAppActive = message == WM_SETFOCUS || wParam != FALSE;
  if (message == WM_KILLFOCUS || message == WM_DESTROY) bRumbleAppActive = false;
#endif
#if INPUT_SCANNERS || defined(HAS_RUMBLE)
  if (message == WM_KILLFOCUS || message == WM_CLOSE || message == WM_DESTROY || (message == WM_ACTIVATEAPP && !wParam)) RumbleStopAllPads();
#endif
  switch (message) {
    case WM_INPUT:
      if (KeyboardReadingMode == KB_READINGMODE_UNBUFFERED_RAW) {
        UINT size = 0;
        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER)) != 0 || size < sizeof(RAWINPUTHEADER)) return 0;
        std::vector<BYTE> data(size);
        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, data.data(), &size, sizeof(RAWINPUTHEADER)) == size &&
            size >= offsetof(RAWINPUT, data) + sizeof(RAWKEYBOARD))
          InputRawKeyboard(*(RAWINPUT*)data.data());
        return 0;
      }
      break;
    case WM_KEYDOWN:
      if (KeyboardReadingMode != KB_READINGMODE_UNBUFFERED_RAW) InputSetActiveDevice(LASTCONTROLLED_KB);
      break;
#ifndef GAME_UG
    case WM_LBUTTONDOWN: InputSetActiveDevice(LASTCONTROLLED_KB); bMousePressedDown = true; return 0;
    case WM_LBUTTONUP: bMousePressedDown = false; return 0;
    case WM_RBUTTONDOWN: InputSetActiveDevice(LASTCONTROLLED_KB); bMouse3PressedDown = true; return 0;
    case WM_RBUTTONUP: bMouse3PressedDown = false; return 0;
    case WM_MBUTTONDOWN: InputSetActiveDevice(LASTCONTROLLED_KB); bMouse2PressedDown = true; return 0;
    case WM_MBUTTONUP: bMouse2PressedDown = false; return 0;
    case WM_SETFOCUS:
      if (bConfineMouse) { GetWindowRect(window, &windowRect); ClipCursor(&windowRect); }
      break;
#if !INPUT_SCANNERS && !defined(NO_FENG)
    case WM_MOUSEWHEEL: InputSetActiveDevice(LASTCONTROLLED_KB); MouseWheelValue = GET_WHEEL_DELTA_WPARAM(wParam); return 0;
    case WM_MOUSEMOVE: if (!bUseWin32Cursor) bLastUsedVirtualMouse = false; return 0;
#endif
#endif
  }
  return GameWndProc(window, message, wParam, lParam);
}

void InstallWindowHook() {
#if defined(WNDPROC_POINTER_ADDR)
  GameWndProc = *(LRESULT(WINAPI**)(HWND, UINT, WPARAM, LPARAM))WNDPROC_POINTER_ADDR;
  injector::WriteMemory<uintptr_t>(WNDPROC_POINTER_ADDR, (uintptr_t)&CustomWndProc, true);
#endif
}

ATOM WINAPI RegisterClassHook(WNDCLASSEXA* windowClass) {
  windowClass->hCursor = NULL;
  return RegisterClassExA(windowClass);
}
