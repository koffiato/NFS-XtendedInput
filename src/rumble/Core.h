// Shared rumble math and settings. MIT; original rumble (c) 2026 Martin Penkava.
#pragma once
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
bool  bRumbleEnabled                             = true;
bool  bRumbleSwapMotors                          = false;
float fRumbleIntensity                           = 1.0f;
float fRumbleLeftScale                           = 1.0f;  // low frequency (heavy) motor
float fRumbleRightScale                          = 1.0f;  // high frequency (light) motor
float fRumbleCategoryScale[kRumbleCategoryCount] = {1.0f, 1.0f, 1.0f, 1.0f};

bool bRumbleGearShift = true;

// envelopes from the console Rumble.cpp (time in seconds, value 0-255)
struct RumbleEnvelopePoint {
  float fTime;
  float fValue;
};

const RumbleEnvelopePoint SmallRumblePoints[] = {{0.0f, 255.0f}, {0.05f, 200.0f}, {0.07f, 80.0f}, {0.8f, 0.0f}};
const RumbleEnvelopePoint BigRumblePoints[]   = {{0.0f, 255.0f}, {0.15f, 255.0f}, {0.2f, 120.0f}, {1.2f, 0.0f}};
const RumbleEnvelopePoint SingleRoadBumpPoints[] = {{0.0f, 140.0f}, {0.07f, 0.0f}};

struct RumbleRamp {
  float fTime;
  float fMaxTime;
  float fAmplitude;
  float fFloor;

  void Start(float amplitude, float floor, float time) {
    fTime      = time;
    fMaxTime   = time;
    fAmplitude = amplitude;
    fFloor     = floor;
  }

  void Stop() { fTime = 0.0f; }

  void Advance(float dt) {
    if (fTime > 0.0f) fTime -= dt;
  }

  float GetValue() const {
    if (fTime <= 0.0f || fMaxTime <= 0.0f) return 0.0f;
    return fFloor + (fAmplitude - fFloor) * fTime / fMaxTime;
  }
};

struct RumblePulse {
  const RumbleEnvelopePoint* pPoints;
  int                        nPoints;
  float                      fTime;
  float                      fAmplitude;

  void Start(const RumbleEnvelopePoint* points, int count, float amplitude) {
    // don't let a weak hit cut off a stronger one that is still playing
    if (pPoints && GetValue() >= amplitude * (points[0].fValue / 255.0f)) return;
    pPoints    = points;
    nPoints    = count;
    fTime      = 0.0f;
    fAmplitude = amplitude;
  }

  void Advance(float dt) {
    if (!pPoints) return;
    fTime += dt;
    if (fTime >= pPoints[nPoints - 1].fTime) pPoints = NULL;
  }

  // 0-1, linear interpolation between envelope points
  float GetValue() const {
    if (!pPoints) return 0.0f;
    int i = 0;
    while (i < nPoints - 1 && fTime >= pPoints[i + 1].fTime) i++;
    if (i >= nPoints - 1) return 0.0f;
    float t = (fTime - pPoints[i].fTime) / (pPoints[i + 1].fTime - pPoints[i].fTime);
    return fAmplitude * (pPoints[i].fValue + (pPoints[i + 1].fValue - pPoints[i].fValue) * t) / 255.0f;
  }
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

