#pragma once
#include <roapi.h>
#include <windows.gaming.input.h>
#include "Core.h"

double RumbleGetTime() {
  static LARGE_INTEGER freq = {0};
  if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return (double)now.QuadPart / (double)freq.QuadPart;
}

// the Xbox motors have 101 levels (0-100 %); round 0-65535 values to them so near-identical values don't cost a call
int RumbleRoundToPercent(int value) { return (value * 100 + 32767) / 65535 * 65535 / 100; }

enum RumbleTriggerMode {
  kTriggersOff,
  kTriggersOn,    // always, to every WGI gamepad
  kTriggersAuto,  // only while exactly one real Xbox pad (not Steam's virtual one) is connected
  kTriggerModeCount,
};

int   RumbleTriggerMode          = kTriggersAuto;
float fRumbleTriggerIntensity    = 1.0f;
bool  bRumbleTriggerPedalScaling = true;  // trigger effects grow with how far the pedal is pressed

namespace RumbleWGI {
typedef HRESULT(WINAPI* RoInitialize_t)(RO_INIT_TYPE);
typedef HRESULT(WINAPI* RoGetActivationFactory_t)(HSTRING, REFIID, void**);
typedef HRESULT(WINAPI* WindowsCreateStringReference_t)(PCWSTR, UINT32, HSTRING_HEADER*, HSTRING*);

using ABI::Windows::Gaming::Input::GamepadVibration;
using ABI::Windows::Gaming::Input::IGameController;
using ABI::Windows::Gaming::Input::IGamepad;
using ABI::Windows::Gaming::Input::IGamepadStatics;
using ABI::Windows::Gaming::Input::IRawGameController;
using ABI::Windows::Gaming::Input::IRawGameControllerStatics;
typedef ABI::Windows::Foundation::Collections::IVectorView<ABI::Windows::Gaming::Input::Gamepad*> GamepadList;

const UINT16 STEAM_VIRTUAL_VID = 0x28DE;  // Valve

bool                       bTried      = false;
IGamepadStatics*           pStatics    = NULL;
IRawGameControllerStatics* pRawStatics = NULL;  // Windows 10 1607+, only used to spot Steam's virtual pad

const int MAX_PADS = 8;
IGamepad* Pads[MAX_PADS];  // cached, the list is refreshed once a second
int       PadCount    = 0;
bool      bSteamPad   = false;  // one of the cached pads is Steam Input's virtual controller
double    PadListTime = -1.0;

HRESULT GetFactory(WindowsCreateStringReference_t createStringRef, RoGetActivationFactory_t roGetFactory, const wchar_t* className, UINT32 length,
                   REFIID iid, void** factory) {
  HSTRING_HEADER header;
  HSTRING        name;
  HRESULT        hr = createStringRef(className, length, &header, &name);
  if (FAILED(hr)) return hr;
  return roGetFactory(name, iid, factory);
}

bool Init() {
  if (bTried) return pStatics != NULL;
  bTried          = true;
  HMODULE combase = LoadLibraryExA("combase.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!combase) return false;
  RoInitialize_t                 roInitialize    = (RoInitialize_t)GetProcAddress(combase, "RoInitialize");
  RoGetActivationFactory_t       roGetFactory    = (RoGetActivationFactory_t)GetProcAddress(combase, "RoGetActivationFactory");
  WindowsCreateStringReference_t createStringRef = (WindowsCreateStringReference_t)GetProcAddress(combase, "WindowsCreateStringReference");
  if (!roInitialize || !roGetFactory || !createStringRef) return false;

  // like SDL: prefer a single-threaded apartment (what the game's own COM users expect); if the thread is already in
  // the multithreaded one that fails with RPC_E_CHANGED_MODE and the existing apartment is used
  if (roInitialize(RO_INIT_SINGLETHREADED) == RPC_E_CHANGED_MODE) roInitialize(RO_INIT_MULTITHREADED);
  static const wchar_t gamepadClass[] = L"Windows.Gaming.Input.Gamepad";
  static const wchar_t rawClass[]     = L"Windows.Gaming.Input.RawGameController";
  if (FAILED(GetFactory(createStringRef, roGetFactory, gamepadClass, _countof(gamepadClass) - 1, __uuidof(IGamepadStatics), (void**)&pStatics)))
    pStatics = NULL;
  if (FAILED(GetFactory(createStringRef, roGetFactory, rawClass, _countof(rawClass) - 1, __uuidof(IRawGameControllerStatics), (void**)&pRawStatics)))
    pRawStatics = NULL;
  return pStatics != NULL;
}

// Steam Input's virtual controller carries Valve's vendor id on the raw controller (SDL checks the same)
bool IsSteamVirtualPad(IGamepad* pad) {
  if (!pRawStatics) return false;
  bool             steam = false;
  IGameController* ctrl  = NULL;
  if (SUCCEEDED(pad->QueryInterface(__uuidof(IGameController), (void**)&ctrl)) && ctrl) {
    IRawGameController* raw = NULL;
    if (SUCCEEDED(pRawStatics->FromGameController(ctrl, &raw)) && raw) {
      UINT16 vid = 0;
      if (SUCCEEDED(raw->get_HardwareVendorId(&vid))) steam = vid == STEAM_VIRTUAL_VID;
      raw->Release();
    }
    ctrl->Release();
  }
  return steam;
}

void RefreshPads() {
  for (int i = 0; i < PadCount; i++) Pads[i]->Release();
  PadCount          = 0;
  bSteamPad         = false;
  GamepadList* list = NULL;
  if (FAILED(pStatics->get_Gamepads(&list)) || !list) return;
  unsigned int count = 0;
  list->get_Size(&count);
  for (unsigned int i = 0; i < count && PadCount < MAX_PADS; i++) {
    IGamepad* pad = NULL;
    if (SUCCEEDED(list->GetAt(i, &pad)) && pad) {
      if (IsSteamVirtualPad(pad)) bSteamPad = true;
      Pads[PadCount++] = pad;
    }
  }
  list->Release();
}

// whether output goes through WGI (main motors and triggers) instead of XInput; re-checks the pads once a second
bool ShouldUse() {
  if (RumbleTriggerMode == kTriggersOff || !Init()) return false;
  double now = RumbleGetTime();
  if (PadListTime < 0.0 || now - PadListTime > 1.0) {
    PadListTime = now;
    RefreshPads();
  }
  if (RumbleTriggerMode == kTriggersAuto) return PadCount == 1 && !bSteamPad;
  return PadCount > 0;
}

void Send(double leftMotor, double rightMotor, double leftTrigger, double rightTrigger) {
  GamepadVibration vib = {leftMotor, rightMotor, leftTrigger, rightTrigger};
  for (int i = 0; i < PadCount; i++) Pads[i]->put_Vibration(vib);
}
}  // namespace RumbleWGI

