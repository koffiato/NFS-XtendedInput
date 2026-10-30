// One settings contract for both input engines.
#pragma once
#include "Output.h"

float RumbleSettingRange(float value, float lo, float hi) {
  if (!_finite(value)) return lo;
  return (std::max)(lo, (std::min)(hi, value));
}

template <class Reader>
void RumbleLoadSettings(Reader read) {
  bRumbleEnabled = read("Enabled", 1.0f) != 0;
  bRumbleSwapMotors = read("SwapMotors", 0.0f) != 0;
  fRumbleIntensity = RumbleSettingRange(read("Intensity", 1), 0, 2);
  fRumbleLeftScale = RumbleSettingRange(read("LeftMotorScale", 1), 0, 2);
  fRumbleRightScale = RumbleSettingRange(read("RightMotorScale", 1), 0, 2);
  const char* keys[] = {"Impacts", "Surface", "Tyres", "Car"};
  for (int i = 0; i < kRumbleCategoryCount; ++i)
    fRumbleCategoryScale[i] = RumbleSettingRange(read(keys[i], 1), 0, 2);
  RumbleShiftPreset = (int)RumbleSettingRange(read("GearShift", kShiftMedium), 0, kShiftPresetCount-1);
  RumbleTriggerMode = (int)RumbleSettingRange(read("ImpulseTriggers", kTriggersAuto), 0, kTriggerModeCount-1);
  fRumbleTriggerIntensity = RumbleSettingRange(read("TriggerIntensity", 1), 0, 2);
  bRumbleTriggerPedalScaling = read("TriggerPedalScaling", 1) != 0;
}
