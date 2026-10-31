// Shared NFS string hashing. MIT; see LICENSE.
#pragma once
#include <cctype>

// NFS uses 32-bit wraparound and signed characters, starting from -1.
// Unsigned arithmetic keeps overflow defined for runtime and constexpr callers.
static constexpr unsigned int compiler_stringhash(const char* text) {
  if (!text) return 0;
  unsigned int hash = ~0u;
  while (*text) hash = hash * 33u + (unsigned int)(signed char)*text++;
  return hash;
}

template <unsigned int V>
static constexpr unsigned int compiler_stringhash_template = V;
#define NFS_HASH(s) compiler_stringhash_template<compiler_stringhash(s)>

inline unsigned int InputStringHash(const char* text, bool ignoreCase = false) {
  if (!text) return 0;
  unsigned int hash = ~0u;
  while (*text) {
    char value = *text++;
    if (ignoreCase) value = (char)std::toupper((unsigned char)value);
    hash = hash * 33u + (unsigned int)(signed char)value;
  }
  return hash;
}

inline int bStringHash(char* text) { return (int)InputStringHash(text); }
