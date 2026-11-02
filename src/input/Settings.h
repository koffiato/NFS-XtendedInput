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
  KeyboardReadingMode = ini.ReadInteger("Input", "KeyboardReadingMode", 0);
  bAllowTwoPlayerKB = ini.ReadInteger("Input", "AllowTwoPlayerKB", false) != 0;
  bXInputOmniMode = ini.ReadInteger("Input", "XInputOmniMode", false) != 0;
  bPassConnStatus = ini.ReadInteger("Input", "PassConnStatus", true) != 0;
  bMouseLook = ini.ReadInteger("Input", "MouseLook", true) != 0;
  MouseLookSensitivity = ini.ReadFloat("Input", "MouseLookSensitivity", 1.0f);
  bConfineMouse = ini.ReadInteger("Input", "ConfineMouse", false) != 0;
  bUseWin32Cursor = ini.ReadInteger("Input", "UseWin32Cursor", true) != 0;
  bUseCustomCursor = ini.ReadInteger("Input", "UseCustomCursor", true) != 0;
  bEnableMouseHiding = ini.ReadInteger("Input", "EnableMouseHiding", true) != 0;

  const char* keys[] = {"PercentLS", "PercentRS", "PercentLS_P2", "PercentRS_P2", "Percent_Shifting", "Percent_AnalogStickDigital", "Percent_AnalogTriggerDigital"};
  WORD* thresholds[] = {&INPUT_DEADZONE_LS, &INPUT_DEADZONE_RS, &INPUT_DEADZONE_LS_P2, &INPUT_DEADZONE_RS_P2, &SHIFT_ANALOG_THRESHOLD, &FEUPDOWN_ANALOG_THRESHOLD, &TRIGGER_ACTIVATION_THRESHOLD};
  const float defaults[] = {0.24f, 0.24f, 0.24f, 0.24f, INPUT_SCANNERS ? 0.62f : 0.75f, 0.50f, 0.12f};
  for (int i = 0; i < 7; ++i) {
    std::string key = INPUT_SCANNERS ? std::string("Deadzone") + keys[i] : keys[i];
    float value = ini.ReadFloat("Deadzone", key.c_str(), defaults[i]);
    value = (std::max)(0.0f, (std::min)(1.0f, value));
    *thresholds[i] = (WORD)(value * (i == 6 ? 255 : 32767));
  }
  ControllerIconMode = ini.ReadInteger("Icons", "ControllerIconMode", 0);
  LastControlledDevice = ini.ReadInteger("Icons", "FirstControlDevice", 0);
  bUseDynamicFEngSwitching = ini.ReadInteger("Icons", "UseDynamicFEngSwitching", !INPUT_SCANNERS) != 0;
  bEnableSplashTakeover = ini.ReadInteger("Icons", "EnableSplashTakeover", true) != 0;
#if !INPUT_SCANNERS && !defined(NO_FENG)
  strcpy_s(ButtonTexFilename, ini.ReadString("Icons", "ButtonTexFilename", ""));
#endif
#if defined(GAME_MW) || defined(GAME_PROSTREET)
  RumbleLoadSettings([&](const char* key, float fallback) { return ini.ReadFloat("Rumble", key, fallback); });
  bRumbleTelemetry = ini.ReadInteger("Rumble", "Telemetry", 0) != 0;
#endif
#if INPUT_SCANNERS
  SetupScannerConfig();
#endif
}
