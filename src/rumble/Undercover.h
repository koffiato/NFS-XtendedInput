// Undercover native collision attribute bindings.
#pragma once

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
