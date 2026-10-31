// Native MW/ProStreet surface records and sampled feedback waves. MIT; see LICENSE.
#pragma once
#include "Core.h"
#include <cstring>

const void*(__thiscall* Attrib_Instance_GetAttributePointer)(const void* thethis, unsigned int key, unsigned int index) =
    (const void*(__thiscall*)(const void*, unsigned int, unsigned int))ATTRIB_GETATTRIBPTR_ADDR;

const void* RumbleGetAttributePointer(const void* surface, unsigned int key, unsigned int index) {
  __try { return surface ? Attrib_Instance_GetAttributePointer(surface, key, index) : NULL; }
  __except (EXCEPTION_EXECUTE_HANDLER) { return NULL; }
}

// --- surfaces ---

#ifdef GAME_MW
const void*(__cdecl* Attrib_FindCollection)(unsigned int classKey, unsigned int collectionKey) =
    (const void*(__cdecl*)(unsigned int, unsigned int))ATTRIB_FINDCOLLECTION_ADDR;

const unsigned int ATTRIB_CLASS_SIMSURFACE   = 0xFB111FEF;
const unsigned int SIMSURFACE_KEY_BLOWN_TIRE = 0xD929E923;  // DoFFB swaps every wheel's surface for this one when spiked

const void* RumbleBlownTireCollection = NULL;

// whether a SimSurface (an Attrib::Instance, collection at +4) is the spiked / blown tyre surface
bool RumbleIsBlownTireSurface(const void* surface) {
  if (!RumbleBlownTireCollection) RumbleBlownTireCollection = Attrib_FindCollection(ATTRIB_CLASS_SIMSURFACE, SIMSURFACE_KEY_BLOWN_TIRE);
  return RumbleBlownTireCollection && *(const void* const*)((const char*)surface + 4) == RumbleBlownTireCollection;
}
#else
// PS damaged-tyre surfaces remain in Surface until their collection key is verified.
bool RumbleIsBlownTireSurface(const void*) { return false; }
#endif

const void* RumbleSurfaceCollection(const void* surface) {
#ifdef GAME_PROSTREET
  return *(const void* const*)surface;
#else
  return *(const void* const*)((const char*)surface + 4);
#endif
}

// Interpolate frequency, amplitude and offset across each native input range.
struct RumbleWaveRecord {
  float fFrequencyMin, fAmplitudeMin, fOffsetMin, fXMin;
  float fFrequencyMax, fAmplitudeMax, fOffsetMax, fXMax;
};

// Copy the native record before mixing: missing/invalid PC attributes are
// silent, and never become NaNs or invalid motor values. PS uses its own live
// Attrib::Instance reader, not an MW collection/layout cast.
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
    for (int ch = 0; ch < 4; ch++) fFrequency[ch] = fAmplitude[ch] = fOffset[ch] = 0.0f;
  }

  // surface: the SimSurface passed by the game, key: FFB_ROLL / FFB_SKID / FFB_SLIP
  void Add(const void* surface, unsigned int key, float x) {
    if (!_finite(x)) return;
    // a non-array attribute returns the same record for every index: it only describes channel 0
    const void* first  = RumbleGetAttributePointer(surface, key, 0);
    bool        single = first && RumbleGetAttributePointer(surface, key, 1) == first;
    for (unsigned int ch = 0; ch < (single ? 1u : 4u); ch++) {
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

  // advances the phases and returns the left / right motor values (0-1)
  void Sample(float dt, float& left, float& right) {
    float v[4];
    for (int ch = 0; ch < 4; ch++) {
      fPhase[ch] += fFrequency[ch] * dt;
      fPhase[ch] -= floorf(fPhase[ch]);
      v[ch] = fOffset[ch] + fAmplitude[ch] * sinf(6.2831853f * fPhase[ch]);
      if (v[ch] < 0.0f) v[ch] = 0.0f;
    }
    left  = v[0] + v[1];
    right = v[2] + v[3];
    if (left > 1.0f) left = 1.0f;
    if (right > 1.0f) right = 1.0f;
  }
};

const unsigned int ATTRIB_KEY_FFB_ROLL = 0xBA1297DA;
const unsigned int ATTRIB_KEY_FFB_SKID = 0x0C149044;
const unsigned int ATTRIB_KEY_FFB_SLIP = 0x8FD11D27;

