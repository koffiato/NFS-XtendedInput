// Shared ProStreet / Undercover Xbox 360 tyre callback transforms. MIT; see LICENSE.
#pragma once
#include "Core.h"

// PS 0x82141608 / UC 0x821E10F8. Degenerate ranges return zero, including
// negative/zero spans supplied by tyre physics.
float RumbleTyreMap(float x, float lo, float hi) {
  return hi - lo > 0.000001f ? RumbleClamp((x - lo) / (hi - lo)) : 0.0f;
}

float RumbleTyreSlip(float slip, float grip) { // PS 0x825AA1E0 / UC 0x82610D30
  if (slip < -0.75f) return 1.0f;
  return slip > grip ? RumbleTyreMap(slip, grip, 2.0f) * 0.5f : 0.0f;
}

float RumbleTyreSkid(float skid, float range, float threshold, float load) { // PS 0x825AA098 / UC 0x82610BE8
  float level = skid < threshold ? RumbleTyreMap(skid, range, threshold)
                                : 1.0f - RumbleTyreMap(skid, threshold, range + threshold);
  return powf(level, (1.0f - load) * 2.0f) * load;
}

float RumbleTyreDriftSlip(float slip, float grip) { // PS 0x825AA710 / UC 0x82611338; normal slip group
  if (slip < -0.75f) return 1.0f;
  if (slip > grip && slip < 0.3f) return RumbleTyreMap(slip, grip, 0.3f) * 0.4f;
  return slip > 0.3f ? (1.0f - RumbleTyreMap(slip, 0.3f, 1.0f)) * 0.4f : 0.0f;
}

float RumbleTyreDriftSkid(float skid, float range, float threshold) { // PS 0x825AA870 / UC 0x826114A8
  float angle = skid > 1.5707963705062866f ? 3.1415927410125732f - skid : fabsf(skid);
  if (angle < threshold) return RumbleTyreMap(angle, range, threshold) * 0.7f;
  if (angle < 0.785385012626648f)
    return (1.0f - RumbleTyreMap(angle, threshold, range + threshold)) * 0.7f + 0.1f;
  return RumbleTyreMap(angle, 0.785385012626648f, 0.9599149823188782f);
}
