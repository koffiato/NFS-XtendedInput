// Native surface records and console feedback waves for the later games. MIT; see LICENSE.
#pragma once
#include "Core.h"
#include <cstring>

const void*(__thiscall* Attrib_Instance_GetAttributePointer)(const void* thethis, unsigned int key, unsigned int index) =
    (const void*(__thiscall*)(const void*, unsigned int, unsigned int))ATTRIB_GETATTRIBPTR_ADDR;

const void* RumbleGetAttributePointer(const void* surface, unsigned int key, unsigned int index) {
  __try { return surface ? Attrib_Instance_GetAttributePointer(surface, key, index) : NULL; }
  __except (EXCEPTION_EXECUTE_HANDLER) { return NULL; }
}

bool RumbleReadSurfaceFloat(const void* surface, unsigned int key, float& value) {
  __try {
    const float* ptr = (const float*)RumbleGetAttributePointer(surface, key, 0);
    if (!ptr || !_finite(*ptr)) return false;
    value = *ptr;
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// --- surfaces ---

const void* RumbleSurfaceCollection(const void* surface) {
#ifdef GAME_MW
  return *(const void* const*)((const char*)surface + 4);
#else
  return *(const void* const*)surface;
#endif
}

// Interpolate frequency, amplitude and offset across each native input range.
struct RumbleWaveRecord {
  float fFrequencyMin, fAmplitudeMin, fOffsetMin, fXMin;
  float fFrequencyMax, fAmplitudeMax, fOffsetMax, fXMax;
};

// Copy the native record before mixing: missing/invalid PC attributes are
// silent, and never become NaNs or invalid motor values.
bool RumbleReadWaveRecord(const void* surface, unsigned int key, unsigned int index, RumbleWaveRecord& out) {
  __try {
    const void* ptr = RumbleGetAttributePointer(surface, key, index);
    if (!ptr) return false;
    memcpy(&out, ptr, sizeof(out));
    const float* values = (const float*)&out;
    for (int i = 0; i < 8; ++i) if (!_finite(values[i])) return false;
    return out.fFrequencyMin >= 0 && out.fFrequencyMin <= 1000 &&
           out.fFrequencyMax >= 0 && out.fFrequencyMax <= 1000 &&
           fabsf(out.fAmplitudeMin) <= 4 && fabsf(out.fAmplitudeMax) <= 4 &&
           fabsf(out.fOffsetMin) <= 4 && fabsf(out.fOffsetMax) <= 4 && out.fXMin < out.fXMax;
  } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// one surface wave effect (road roll or skid): each channel keeps the strongest wheel's parameters for the tick
struct RumbleWave {
  float fPhase[4];
  float fFrequency[4];
  float fAmplitude[4];
  float fOffset[4];

  void Begin() {
    for (int ch = 0; ch < 4; ch++) {
#ifdef GAME_UC
      // UC BeginUpdate (0x82610A48) zeroes all four fields, including phase.
      // Keeping MW/CB's phase continuity changes UC's frame-to-frame texture.
      fPhase[ch] = 0.0f;
#endif
      fFrequency[ch] = fAmplitude[ch] = fOffset[ch] = 0.0f;
    }
  }

  // surface: the SimSurface passed by the game, key: FFB_ROLL / FFB_SKID / FFB_SLIP
  void Add(const void* surface, unsigned int key, float x) {
    if (!_finite(x)) return;
    for (unsigned int ch = 0; ch < 4; ch++) {
      RumbleWaveRecord record;
      if (!RumbleReadWaveRecord(surface, key, ch, record)) continue;
      const RumbleWaveRecord* w = &record;
      if (x < w->fXMin) continue;
      float t = (x - w->fXMin) / (w->fXMax - w->fXMin);
      if (t > 1.0f) t = 1.0f;
      float freq = w->fFrequencyMin + (w->fFrequencyMax - w->fFrequencyMin) * t;
      float amp  = w->fAmplitudeMin + (w->fAmplitudeMax - w->fAmplitudeMin) * t;
      float off  = w->fOffsetMin + (w->fOffsetMax - w->fOffsetMin) * t;
      if (freq > fFrequency[ch]) fFrequency[ch] = freq;
      if (amp > fAmplitude[ch]) fAmplitude[ch] = amp;
      if (off > fOffset[ch]) fOffset[ch] = off;
    }
  }

  // Advance phases and sum channel pairs. Console retains signed, unclipped values
  // until effects have been combined in the motor mixer.
  void Sample(float dt, float& left, float& right) {
    float v[4];
    for (int ch = 0; ch < 4; ch++) {
      v[ch] = fOffset[ch];
      if (fAmplitude[ch] * fFrequency[ch] > 0) {
        fPhase[ch] += fFrequency[ch] * dt;
        if (fPhase[ch] > 1) fPhase[ch] -= 1;
        v[ch] += fAmplitude[ch] * sinf(6.2831854820251465f * fPhase[ch]);
      }
    }
    left  = v[0] + v[1];
    right = v[2] + v[3];
  }
};

const unsigned int ATTRIB_KEY_FFB_ROLL = 0xBA1297DA;
const unsigned int ATTRIB_KEY_FFB_SKID = 0x0C149044;
const unsigned int ATTRIB_KEY_FFB_SLIP = 0x8FD11D27;
const unsigned int ATTRIB_KEY_FFB_DRIFT_SKID = 0x35A8EADA;
const unsigned int ATTRIB_KEY_FFB_BUMP_THRESHOLD = 0xE289A6B0;
const unsigned int ATTRIB_KEY_FFB_BUMP_GAIN = 0x2D1022A0;

