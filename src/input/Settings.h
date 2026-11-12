#pragma once
#include "Config.h"
#include <string>
#include <algorithm>

void InitConfig() {
  InputConfig ini;
  if (!ini.EnsureDefault()) OutputDebugStringA("XtendedInput: unable to create default settings INI.\n");
#if !INPUT_SCANNERS
  InputConfig bindings("NFS_XtendedInput.default.ini");
  strcpy_s(defMappingName, bindings.GetIniPath());
  if (!bindings.EnsureDefault(102)) OutputDebugStringA("XtendedInput: unable to create default bindings INI.\n");
#endif
  KeyboardReadingMode = ini.ReadInteger("Input", "KeyboardReadingMode", ConfigDefaults::Input::KeyboardReadingMode);
  bAllowTwoPlayerKB = ini.ReadInteger("Input", "AllowTwoPlayerKB", ConfigDefaults::Input::AllowTwoPlayerKB) != 0;
  bXInputOmniMode = ini.ReadInteger("Input", "XInputOmniMode", ConfigDefaults::Input::XInputOmniMode) != 0;
  bPassConnStatus = ini.ReadInteger("Input", "PassConnStatus", ConfigDefaults::Input::PassConnStatus) != 0;
  bMouseLook = ini.ReadInteger("Input", "MouseLook", ConfigDefaults::Input::MouseLook) != 0;
  MouseLookSensitivity = ini.ReadFloat("Input", "MouseLookSensitivity", ConfigDefaults::Input::MouseLookSensitivity);
  bConfineMouse = ini.ReadInteger("Input", "ConfineMouse", ConfigDefaults::Input::ConfineMouse) != 0;
  bUseWin32Cursor = ini.ReadInteger("Input", "UseWin32Cursor", ConfigDefaults::Input::UseWin32Cursor) != 0;
  bUseCustomCursor = ini.ReadInteger("Input", "UseCustomCursor", ConfigDefaults::Input::UseCustomCursor) != 0;
  bEnableMouseHiding = ini.ReadInteger("Input", "EnableMouseHiding", ConfigDefaults::Input::EnableMouseHiding) != 0;

  const char* keys[] = {"PercentLS", "PercentRS", "PercentLS_P2", "PercentRS_P2", "Percent_Shifting", "Percent_AnalogStickDigital", "Percent_AnalogTriggerDigital"};
  WORD* thresholds[] = {&INPUT_DEADZONE_LS, &INPUT_DEADZONE_RS, &INPUT_DEADZONE_LS_P2, &INPUT_DEADZONE_RS_P2, &SHIFT_ANALOG_THRESHOLD, &FEUPDOWN_ANALOG_THRESHOLD, &TRIGGER_ACTIVATION_THRESHOLD};
  const float defaults[] = {
    ConfigDefaults::Deadzone::PercentLS, ConfigDefaults::Deadzone::PercentRS,
    ConfigDefaults::Deadzone::PercentLS_P2, ConfigDefaults::Deadzone::PercentRS_P2,
    ConfigDefaults::Deadzone::Percent_Shifting, ConfigDefaults::Deadzone::Percent_AnalogStickDigital,
    ConfigDefaults::Deadzone::Percent_AnalogTriggerDigital,
  };
  for (int i = 0; i < 7; ++i) {
    std::string key = INPUT_SCANNERS ? std::string("Deadzone") + keys[i] : keys[i];
    float value = ini.ReadFloat("Deadzone", key.c_str(), defaults[i]);
    value = (std::max)(0.0f, (std::min)(1.0f, value));
    *thresholds[i] = (WORD)(value * (i == 6 ? 255 : 32767));
  }
  ControllerIconMode = ini.ReadInteger("Icons", "ControllerIconMode", ConfigDefaults::Icons::ControllerIconMode);
  LastControlledDevice = ini.ReadInteger("Icons", "FirstControlDevice", ConfigDefaults::Icons::FirstControlDevice);
  bUseDynamicFEngSwitching = ini.ReadInteger("Icons", "UseDynamicFEngSwitching", ConfigDefaults::Icons::UseDynamicFEngSwitching) != 0;
  bEnableSplashTakeover = ini.ReadInteger("Icons", "EnableSplashTakeover", ConfigDefaults::Icons::EnableSplashTakeover) != 0;
#if !INPUT_SCANNERS && !defined(NO_FENG)
  strcpy_s(ButtonTexFilename, ini.ReadString("Icons", "ButtonTexFilename", ConfigDefaults::Icons::ButtonTexFilename));
#endif
#ifdef HAS_RUMBLE
  RumbleLoadSettings([&](const char* key, float fallback) { return ini.ReadFloat("Rumble", key, fallback); });
  bRumbleTelemetry = ini.ReadInteger("Rumble", "Telemetry", ConfigDefaults::Rumble::Telemetry) != 0;
#endif
#if INPUT_SCANNERS
  SetupScannerConfig();
#endif
  ResolveCursorFallback();
  ResolveIconFallback();
}
