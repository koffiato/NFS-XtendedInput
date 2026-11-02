// MW console controller ramps. MIT; see LICENSE and docs/mw-rumble-x360.md.
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

struct MWEffects {
  MWRamp collision;
  MWRamp potentialLeft, potentialRight, nos, engineLeft, engineRight, shiftLeft, shiftRight;
  bool nosOn, engineOn, shifting;
  int potential;
  int outputLeft, outputRight;
  double feedbackTime;

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
    mixer.Add(kRumbleCar, potentialLeft.Sample(dt), potentialRight.Sample(dt));
    mixer.Add(kRumbleCar, nos.Sample(dt), 0);
    mixer.Add(kRumbleCar, engineLeft.Sample(dt), engineRight.Sample(dt));
    mixer.Add(kRumbleCar, shiftLeft.Sample(dt), shiftRight.Sample(dt));
  }
};
