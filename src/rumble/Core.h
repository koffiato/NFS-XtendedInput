// Shared rumble math and settings. MIT; original rumble (c) 2026 Martin Penkava.
#pragma once
#include "../input/ConfigDefaults.h"
#include <cmath>
#include <float.h>
#include <algorithm>

// Bounded values shared by settings and native Underground events.
float RumbleClamp(float value, float lo = 0.0f, float hi = 1.0f) {
  if (!_finite(value)) return lo;
  return value < lo ? lo : value > hi ? hi : value;
}

float RumbleRampValue(float value, float lo, float hi) {
  return RumbleClamp((value - lo) / (hi - lo));
}

enum RumbleCategory {
  kRumbleImpacts,  // hits, scrapes, spike strips, blown tyres
  kRumbleSurface,  // kerbs, bumps, landings, road texture
  kRumbleTyres,    // lockup, wheelspin, slides
  kRumbleCar,      // NOS, shifts, engine
  kRumbleCategoryCount,
};

// Disabled means no rumble patches or device output.
bool  bRumbleEnabled                             = ConfigDefaults::Rumble::Enabled;
bool  bRumbleSwapMotors                          = ConfigDefaults::Rumble::SwapMotors;
float fRumbleIntensity                           = ConfigDefaults::Rumble::Intensity;
float fRumbleLeftScale                           = ConfigDefaults::Rumble::LeftMotorScale;  // low frequency (heavy) motor
float fRumbleRightScale                          = ConfigDefaults::Rumble::RightMotorScale;  // high frequency (light) motor
float fRumbleCategoryScale[kRumbleCategoryCount] = {ConfigDefaults::Rumble::Impacts, ConfigDefaults::Rumble::Surface,
                                                    ConfigDefaults::Rumble::Tyres, ConfigDefaults::Rumble::Car};

bool bRumbleGearShift = ConfigDefaults::Rumble::GearShift;

// Native Underground curve points (seconds, 0-255).
struct RumbleEnvelopePoint {
  float fTime;
  float fValue;
};

// Strongest effect per motor. No additional envelope shaping in the mixer.
struct RumbleMixer {
  float fMotor[2];

  void Begin() { fMotor[0] = fMotor[1] = 0.0f; }

  void Add(RumbleCategory category, float left, float right) {
    float scale = fRumbleCategoryScale[category];
    if (left * scale > fMotor[0]) fMotor[0] = left * scale;
    if (right * scale > fMotor[1]) fMotor[1] = right * scale;
  }

  static int ToMotor(float value, float scale) {
    float v = value * fRumbleIntensity * scale;
    if (!(v > 0.0f)) return 0;  // also NaN
    if (v >= 1.0f) return 65535;
    return (int)(v * 65535.0f);
  }

  void Resolve(int& leftMotor, int& rightMotor) const {
    leftMotor   = ToMotor(fMotor[0], fRumbleLeftScale);
    rightMotor  = ToMotor(fMotor[1], fRumbleRightScale);
    if (bRumbleSwapMotors) {
      int tmp    = leftMotor;
      leftMotor  = rightMotor;
      rightMotor = tmp;
    }
  }
};

