// Black Box controller ramps and waves. MIT; see LICENSE and docs/*-rumble-x360.md.
#pragma once
#include "Core.h"

// Xbox state is {remaining, duration, amplitude, floor}, in seconds.
struct MWRamp {
  float remaining, duration, amplitude, floor;
  void Start(float a, float f, float seconds) { remaining = duration = seconds; amplitude = a; floor = f; }
  float Sample(float seconds) {
    if (!(remaining > 0 && duration > 0)) return 0;
    remaining -= seconds;
    return (std::max)(0.0f, floor + (amplitude - floor) / duration * remaining);
  }
};

#if defined(GAME_CARBON) || defined(GAME_UC)
// Xbox state is {phase, amplitude, frequency, offset}; BeginUpdate clears all but phase.
struct MWWave {
  float phase, amplitude, frequency, offset;
  void Begin() { amplitude = frequency = offset = 0.0f; }
  float Sample(float seconds) {
    float v = offset;
    if (amplitude * frequency > 0.0f) {
      phase += frequency * seconds;
      if (phase > 1.0f) phase -= 1.0f;
      v += amplitude * sinf(6.2831854820251465f * phase);
    }
    return v;
  }
};
#endif

struct MWEffects {
  MWRamp collision;
  MWRamp potentialLeft, potentialRight, nos, engineLeft, engineRight, shiftLeft, shiftRight;
#ifdef GAME_PROSTREET
  MWRamp bumpLeft, bumpRight;
#endif
#ifdef GAME_CARBON
  MWWave draft;
#endif
#if defined(GAME_CARBON) || defined(GAME_UC)
  MWWave drift;
#endif
#ifdef GAME_CARBON
  float draftScale = 1.0f;
#endif
  bool nosOn, engineOn, shifting;
  int potential;
  int outputLeft, outputRight;
#ifndef GAME_PROSTREET
  double feedbackTime;
#endif

#if defined(GAME_CARBON) || defined(GAME_UC)
  void Begin() {
#ifdef GAME_CARBON
    draftScale = 1.0f;
    draft.Begin();
#endif
    drift.Begin();
  }
#ifdef GAME_CARBON
  void Draft(const void* info, float simTime, bool paused) {
    if (!info || !*(const unsigned char*)info) return;
    float wake = *(const float*)((const char*)info + 0x1C);
    if (!_finite(wake)) return;
    if (wake < 1.5f) {
      if (paused) return;
      float f = 1.0f - wake * (2.0f / 3.0f);
      draft.frequency = f * 60.0f;
      draft.amplitude = f * 0.6f + 0.4f;
      draft.phase = simTime * 1000.0f;
    } else if (*(const unsigned char*)((const char*)info + 1)) {
      draftScale = 0.5f;
    }
  }
#endif
  void Drift(float slipAngle, float simTime) {
    if (!_finite(slipAngle)) return;
    float simTimeMs = simTime * 1000.0f;
    float a = fabsf(slipAngle);
    if (!(a > 0.01f)) return;
    float t1 = (std::min)((a - 0.01f) * 8.16326523f, 1.0f);
    drift.phase = simTimeMs;
    drift.amplitude = t1 * 0.6f + 0.4f;
    drift.frequency = 60.0f;
    if (!(a > 0.015f)) return;
    float t2 = (std::min)((a - 0.015f) * 8.24742222f, 1.0f);
    drift = {simTimeMs, t2 * 0.6f + 0.4f, 60.0f, 0.0f};
  }
#endif

  void Collision(float strength) {
    if (!(strength > 0)) return;
    if (collision.remaining > 0) {
      collision.remaining = (std::max)(collision.remaining, strength);
      collision.duration = (std::max)(collision.duration, strength);
      collision.amplitude = (std::max)(collision.amplitude, strength);
      collision.floor = 0;
    } else collision.Start(strength, 0, strength);
  }

  void NOS(bool on) {
    if (on == nosOn) return;
    nosOn = on;
    nos = {};
    if (on) nos.Start(1, 0.5f, 2);
  }
  void Engine(bool on) {
    if (on == engineOn) return;
    engineOn = on;
    if (on) {
      engineLeft.Start(1, 0.6f, 1);
      engineRight.Start((std::min)(engineRight.amplitude, 1.0f), 0.6f, 1);
    } else engineLeft = engineRight = {};
  }
  void Shift(bool on) {
    if (on == shifting) return;
    shifting = on;
    if (on) { shiftLeft.Start(0.4f, 0.4f, 0.5f); shiftRight.Start(0.4f, 0.4f, 0.5f); }
  }
  void Potential(int value) {
    if (potential != 0) potentialLeft = potentialRight = {};
    int previous = potential;
    potential = value;
    if (value < 3 || value > 5 || previous >= value) return;
    float level = value == 3 ? 0.5f : value == 4 ? 0.75f : 1.0f;
    potentialLeft.Start(level, level, 3);
    potentialRight.Start(level, level, 3);
  }
  void Mix(float dt, RumbleMixer& mixer) {
    mixer.Add(kRumbleImpacts, collision.Sample(dt), 0);
#ifdef GAME_PROSTREET
    // Retail PS Run (0x825A8CE0) advances these ramps but discards their
    // results. Only collision, NOS and indexed bumps reach the motor maxima.
    potentialLeft.Sample(dt); potentialRight.Sample(dt);
    engineLeft.Sample(dt); engineRight.Sample(dt);
    shiftLeft.Sample(dt); shiftRight.Sample(dt);
    mixer.Add(kRumbleSurface, bumpLeft.Sample(dt), bumpRight.Sample(dt));
#else
    mixer.Add(kRumbleCar, potentialLeft.Sample(dt), potentialRight.Sample(dt));
    mixer.Add(kRumbleCar, engineLeft.Sample(dt), engineRight.Sample(dt));
    mixer.Add(kRumbleCar, shiftLeft.Sample(dt), shiftRight.Sample(dt));
#endif
    mixer.Add(kRumbleCar, nos.Sample(dt), 0);
#ifdef GAME_CARBON
    mixer.Add(kRumbleCar, 0, draft.Sample(dt));
#endif
#if defined(GAME_CARBON) || defined(GAME_UC)
    mixer.Add(kRumbleTyres, 0, drift.Sample(dt));
#endif
  }
};
