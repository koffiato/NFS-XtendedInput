// XtendedInput -- one runtime with native game adapters. MIT; see LICENSE.
#pragma once
#include "_vs/stdafx.h"
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <XInput.h>
#include <timeapi.h>
#include <injector/injector.hpp>
#include <Shlwapi.h>
#pragma comment(lib, "xinput.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "winmm.lib")
#define MINI_CASE_SENSITIVE
#include <mINI/src/mini/ini.h>
#include "input/State.h"
#include "input/Config.h"
#include "input/Addresses.h"
#if INPUT_SCANNERS
#ifdef GAME_UG
#include "input/UGEvents.h"
#include "input/UGFrontend.h"
#else
#include "input/UG2Events.h"
#include "input/UG2Frontend.h"
#endif
#else
#include "input/Actions.h"
#if defined(GAME_CARBON) || defined(GAME_PROSTREET)
float FEActivationFloat = 0.999999f;
#endif
#ifndef NO_FENG
#include "input/NativeFrontend.h"
#endif
void*(__thiscall* UTL_Com_Object_IList_Constructor)(void* thethis, unsigned int unk) = (void*(__thiscall*)(void*, unsigned int))UTL_ILIST_CONSTRUCTOR_ADDR;
void(__thiscall* DebugWorldCameraMover_Update)(void* thethis, float unk)             = (void(__thiscall*)(void*, float))DebugWorldCameraMover_Update_Addr;

void*(__thiscall* DebugWorldCameraMover_Constructor)(void* thethis, void* vector1, void* vector2, int joyport,
                                                     int unk)      = (void*(__thiscall*)(void*, void*, void*, int, int))DEBUGWORLDCAMERAMOVER_CONSTRUCTOR_ADDR;
void*(__thiscall* DebugWorldCameraMover_Destructor)(void* thethis) = (void*(__thiscall*)(void*))DEBUGWORLDCAMERAMOVER_DESTRUCTOR_ADDR;

#endif
#include "input/Controls.h"
void DummyFunc() {}
void __stdcall DummyFuncStd(BOOL) {}
#include "input/Types.h"
#include "input/KeyboardState.h"
#include "input/Window.h"
#include "input/Poll.h"
#include "input/Frontend.h"
#include "input/Mapping.h"
#if INPUT_SCANNERS
#include "input/Scanners.h"
#else
#include "input/Device.h"
#endif
#include "input/Settings.h"
#include "input/Hooks.h"

void FixWorkingDirectory() {
  char ExecutablePath[_MAX_PATH];

  GetModuleFileName(GetModuleHandle(""), ExecutablePath, _MAX_PATH);
  ExecutablePath[strrchr(ExecutablePath, '\\') - ExecutablePath] = 0;
  SetCurrentDirectory(ExecutablePath);
}

int Init() {
  FixWorkingDirectory();
  ZeroMemory(g_Controllers, sizeof(g_Controllers));
  InitConfig();
  InstallGameHooks();
  InstallWindowHook();
#if INPUT_SCANNERS
  RumbleInit();
#else
  InputRegisterKeyboard(*(HWND*)GAME_HWND_ADDR);
#endif
  return 0;
}

extern "C" __declspec(dllexport) bool GetPollingState() { return bGlobalDoPolling; }

extern "C" __declspec(dllexport) bool SetPollingState(bool state) {
  bGlobalDoPolling = state;
  return state;
}

// Blocks a poll for exactly 1 poll cycle -- useful for frame-accurate blocks
extern "C" __declspec(dllexport) void BlockNextPoll() {
  bBlockThisPoll = true;
}

extern "C" __declspec(dllexport) bool GetBlockNextPoll() { return bBlockThisPoll; }

#if !INPUT_SCANNERS && !defined(NO_FENG)
extern "C" __declspec(dllexport) float GetFEScale() { return fFEScale; }

extern "C" __declspec(dllexport) float SetFEScale(float val) {
  fFEScale = val;
  return fFEScale;
}

extern "C" __declspec(dllexport) bool GetUseWin32Cursor() { return bUseWin32Cursor; }
#endif
