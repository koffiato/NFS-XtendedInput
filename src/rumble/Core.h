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

// ducking: textures drop to this level while an impact pulse is playing
const float RUMBLE_DUCK_LEVEL     = 0.3f;
const float RUMBLE_DUCK_THRESHOLD = 0.05f;
// fatigue fade: a texture running longer than FATIGUE_START seconds fades to FATIGUE_FLOOR over FATIGUE_RAMP seconds,
// and recovers once it has been quiet for FATIGUE_RESET seconds. Pulses never fade.
const float RUMBLE_FATIGUE_START = 3.0f;
const float RUMBLE_FATIGUE_RAMP  = 5.0f;
const float RUMBLE_FATIGUE_FLOOR = 0.6f;
const float RUMBLE_FATIGUE_RESET = 2.5f;

enum RumbleShiftPreset {
  kShiftOff,
  kShiftLow,
  kShiftMedium,
  kShiftStrong,
  kShiftMax,  // everything at full power
  kShiftPresetCount,
};
struct RumbleShiftFeel {
  float fLeft;
  float fRight;
  float fTrigger;
  float fTime;
  float fFloor;  // where the knock has decayed to when it cuts off
};
const RumbleShiftFeel RumbleShiftFeels[kShiftPresetCount] = {
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f},      // off
    {0.3f, 0.0f, 0.3f, 0.1f, 0.6f},      // low
    {0.55f, 0.15f, 0.6f, 0.12f, 0.6f},   // medium
    {0.9f, 0.45f, 0.9f, 0.16f, 0.6f},    // strong
    {1.0f, 1.0f, 1.0f, 0.22f, 0.8f},     // max
};
int RumbleShiftPreset = kShiftMedium;

// envelopes from the console Rumble.cpp (time in seconds, value 0-255)
struct RumbleEnvelopePoint {
  float fTime;
  float fValue;
};

const RumbleEnvelopePoint SmallRumblePoints[] = {{0.0f, 255.0f}, {0.05f, 200.0f}, {0.07f, 80.0f}, {0.8f, 0.0f}};
const RumbleEnvelopePoint BigRumblePoints[]   = {{0.0f, 255.0f}, {0.15f, 255.0f}, {0.2f, 120.0f}, {1.2f, 0.0f}};
// shift timing cues (not from the console code): Perfect = light double tap, Miss = rough buzz
const RumbleEnvelopePoint PerfectShiftPoints[] = {{0.0f, 255.0f}, {0.07f, 255.0f}, {0.07f, 0.0f}, {0.13f, 0.0f}, {0.13f, 255.0f}, {0.2f, 255.0f}, {0.2f, 0.0f}};
const RumbleEnvelopePoint MissShiftPoints[]    = {{0.0f, 255.0f}, {0.15f, 180.0f}, {0.15f, 0.0f}};
// the same cues on the right trigger: Perfect = one crisp tick, Miss = two
const RumbleEnvelopePoint TriggerPerfectPoints[] = {{0.0f, 255.0f}, {0.03f, 255.0f}, {0.03f, 0.0f}};
const RumbleEnvelopePoint TriggerMissPoints[]    = {{0.0f, 255.0f}, {0.025f, 255.0f}, {0.025f, 0.0f}, {0.085f, 0.0f}, {0.085f, 255.0f}, {0.11f, 255.0f}, {0.11f, 0.0f}};
// kerb / bump thump (not from the console code): one sharp knock per wheel, so crossing a kerb gives the natural front-rear double knock
const RumbleEnvelopePoint BumpPoints[] = {{0.0f, 255.0f}, {0.04f, 255.0f}, {0.12f, 0.0f}};

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

// running time of a texture, for the fatigue fade
struct RumbleTextureState {
  float fActiveTime;
  float fQuietTime;

  float Update(bool active, float dt) {
    if (active) {
      fActiveTime += dt;
      fQuietTime = 0.0f;
    } else {
      fQuietTime += dt;
      if (fQuietTime >= RUMBLE_FATIGUE_RESET) fActiveTime = 0.0f;
    }
    if (fActiveTime <= RUMBLE_FATIGUE_START) return 1.0f;
    float t = (fActiveTime - RUMBLE_FATIGUE_START) / RUMBLE_FATIGUE_RAMP;
    if (t > 1.0f) t = 1.0f;
    return 1.0f - (1.0f - RUMBLE_FATIGUE_FLOOR) * t;
  }
};

// collects every effect's motor values for one tick and resolves them to the final motor output
struct RumbleMixer {
  float fDt;
  float fPulse[2];
  float fTexture[2];
  bool  bImpactPlaying;

  void Begin(float dt) {
    fDt            = dt;
    fPulse[0]      = fPulse[1] = 0.0f;
    fTexture[0]    = fTexture[1] = 0.0f;
    bImpactPlaying = false;
  }

  void AddPulse(RumbleCategory category, float left, float right) {
    float scale = fRumbleCategoryScale[category];
    if (category == kRumbleImpacts && (left > RUMBLE_DUCK_THRESHOLD || right > RUMBLE_DUCK_THRESHOLD) && scale > 0.0f) bImpactPlaying = true;
    if (left * scale > fPulse[0]) fPulse[0] = left * scale;
    if (right * scale > fPulse[1]) fPulse[1] = right * scale;
  }

  void AddTexture(RumbleCategory category, RumbleTextureState& state, float left, float right) {
    float fatigue = state.Update(left > 0.0f || right > 0.0f, fDt);
    float scale   = fRumbleCategoryScale[category] * fatigue;
    if (left * scale > fTexture[0]) fTexture[0] = left * scale;
    if (right * scale > fTexture[1]) fTexture[1] = right * scale;
  }

  static int ToMotor(float value, float scale) {
    float v = value * fRumbleIntensity * scale;
    if (!(v > 0.0f)) return 0;  // also NaN
    if (v >= 1.0f) return 65535;
    return (int)(v * 65535.0f);
  }

  void Resolve(int& leftMotor, int& rightMotor) const {
    float duck  = bImpactPlaying ? RUMBLE_DUCK_LEVEL : 1.0f;
    float left  = fPulse[0] > fTexture[0] * duck ? fPulse[0] : fTexture[0] * duck;
    float right = fPulse[1] > fTexture[1] * duck ? fPulse[1] : fTexture[1] * duck;
    leftMotor   = ToMotor(left, fRumbleLeftScale);
    rightMotor  = ToMotor(right, fRumbleRightScale);
    if (bRumbleSwapMotors) {
      int tmp    = leftMotor;
      leftMotor  = rightMotor;
      rightMotor = tmp;
    }
  }
};

