// One settings contract for both input engines.
#pragma once
#include "Output.h"

template <class Reader>
void RumbleLoadSettings(Reader read) {
  bRumbleEnabled = read("Enabled", ConfigDefaults::Rumble::Enabled) != 0;
  bRumbleSwapMotors = read("SwapMotors", ConfigDefaults::Rumble::SwapMotors) != 0;
  fRumbleIntensity = RumbleClamp(read("Intensity", ConfigDefaults::Rumble::Intensity), 0, 2);
  fRumbleLeftScale = RumbleClamp(read("LeftMotorScale", ConfigDefaults::Rumble::LeftMotorScale), 0, 2);
  fRumbleRightScale = RumbleClamp(read("RightMotorScale", ConfigDefaults::Rumble::RightMotorScale), 0, 2);
  const char* keys[] = {"Impacts", "Surface", "Tyres", "Car"};
  const float defaults[] = {ConfigDefaults::Rumble::Impacts, ConfigDefaults::Rumble::Surface,
                            ConfigDefaults::Rumble::Tyres, ConfigDefaults::Rumble::Car};
  for (int i = 0; i < kRumbleCategoryCount; ++i)
    fRumbleCategoryScale[i] = RumbleClamp(read(keys[i], defaults[i]), 0, 2);
  // Older INIs used 1..4 for custom presets; any positive value now enables the game effect.
  bRumbleGearShift = RumbleClamp(read("GearShift", ConfigDefaults::Rumble::GearShift), 0, 1) > 0;
  RumbleTriggerMode = (int)RumbleClamp(read("ImpulseTriggers", ConfigDefaults::Rumble::ImpulseTriggers), 0, kTriggerModeCount-1);
  fRumbleTriggerIntensity = RumbleClamp(read("TriggerIntensity", ConfigDefaults::Rumble::TriggerIntensity), 0, 2);
  bRumbleTriggerPedalScaling = read("TriggerPedalScaling", ConfigDefaults::Rumble::TriggerPedalScaling) != 0;
}
