// One settings contract for both input engines.
#pragma once
#include "Output.h"

template <class Reader>
void RumbleLoadSettings(Reader read) {
  bRumbleEnabled = read("Enabled", 1.0f) != 0;
  bRumbleSwapMotors = read("SwapMotors", 0.0f) != 0;
  fRumbleIntensity = RumbleClamp(read("Intensity", 1), 0, 2);
  fRumbleLeftScale = RumbleClamp(read("LeftMotorScale", 1), 0, 2);
  fRumbleRightScale = RumbleClamp(read("RightMotorScale", 1), 0, 2);
  const char* keys[] = {"Impacts", "Surface", "Tyres", "Car"};
  for (int i = 0; i < kRumbleCategoryCount; ++i)
    fRumbleCategoryScale[i] = RumbleClamp(read(keys[i], 1), 0, 2);
  RumbleShiftPreset = (int)RumbleClamp(read("GearShift", kShiftMedium), 0, kShiftPresetCount-1);
  RumbleTriggerMode = (int)RumbleClamp(read("ImpulseTriggers", kTriggersAuto), 0, kTriggerModeCount-1);
  fRumbleTriggerIntensity = RumbleClamp(read("TriggerIntensity", 1), 0, 2);
  bRumbleTriggerPedalScaling = read("TriggerPedalScaling", 1) != 0;
}
