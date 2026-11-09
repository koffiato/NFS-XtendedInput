// Undercover Xbox 360 callback transforms. Addresses use the memory-layout XexTool dump.
#pragma once

float RumbleUCMap(float x, float lo, float hi) { // 0x821E10F8
  return hi - lo > 0.000001f ? RumbleClamp((x - lo) / (hi - lo)) : 0.0f;
}
float RumbleUCSlip(float slip, float grip) { // 0x82610D30
  if (slip < -0.75f) return 1.0f;
  return slip > grip ? RumbleUCMap(slip, grip, 2.0f) * 0.5f : 0.0f;
}
float RumbleUCSkid(float skid, float range, float threshold, float load) { // 0x82610BE8
  float level = skid < threshold ? RumbleUCMap(skid, range, threshold)
                                : 1.0f - RumbleUCMap(skid, threshold, range + threshold);
  return powf(level, (1.0f - load) * 2.0f) * load;
}
float RumbleUCDriftSlip(float slip, float grip) { // 0x82611338
  if (slip < -0.75f) return 1.0f;
  if (slip > grip && slip < 0.3f) return RumbleUCMap(slip, grip, 0.3f) * 0.4f;
  return slip > 0.3f ? (1.0f - RumbleUCMap(slip, 0.3f, 1.0f)) * 0.4f : 0.0f;
}
float RumbleUCDriftSkid(float skid, float range, float threshold) { // 0x826114A8
  float angle = skid > 1.5707963705062866f ? 3.1415927410125732f - skid : fabsf(skid);
  if (angle < threshold) return RumbleUCMap(angle, range, threshold) * 0.7f;
  if (angle < 0.785385012626648f)
    return (1.0f - RumbleUCMap(angle, threshold, range + threshold)) * 0.7f + 0.1f;
  return RumbleUCMap(angle, 0.785385012626648f, 0.9599149823188782f);
}

// Native PC ReportCollision obtains this same feedback collection. Construct a
// real Attrib::Instance so inherited attributes and collection references work.
void* (__cdecl* RumbleUCFindCollection)(unsigned int, unsigned int) =
    (void* (__cdecl*)(unsigned int, unsigned int))0x00464A20;
void* (__thiscall* RumbleUCConstructInstance)(void*, void*, unsigned int) =
    (void* (__thiscall*)(void*, void*, unsigned int))0x00462C80;
void (__thiscall* RumbleUCDestroyInstance)(void*) = (void (__thiscall*)(void*))0x00468820;

void RumbleUCAddCollision(RumbleWave& wave, unsigned int key, float input) {
  if (!_finite(input) || !(input > 0)) return;
  uintptr_t instance[4] = {};
  bool constructed = false;
  __try {
    __try {
      void* collection = RumbleUCFindCollection(0xD93FC53A, 0xEEC2271A);
      if (collection) {
        RumbleUCConstructInstance(instance, collection, 0);
        constructed = true;
        wave.Add(instance, key, input);
      }
    } __finally {
      if (constructed) RumbleUCDestroyInstance(instance);
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
