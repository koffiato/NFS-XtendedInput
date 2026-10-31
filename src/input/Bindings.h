// Shared controller bindings and menu lookup. MIT; see LICENSE.
#pragma once
#include <windows.h>
#include <XInput.h>
#include <cstdint>
#include "StringHash.h"

// other gamepad stuff -- sticks & triggers
#define XINPUT_GAMEPAD_LT_CONFIGDEF       0xABC0
#define XINPUT_GAMEPAD_RT_CONFIGDEF       0xABC1
#define XINPUT_GAMEPAD_LS_X_CONFIGDEF     0xABC2
#define XINPUT_GAMEPAD_LS_Y_CONFIGDEF     0xABC3
#define XINPUT_GAMEPAD_RS_X_CONFIGDEF     0xABC4
#define XINPUT_GAMEPAD_RS_Y_CONFIGDEF     0xABC5
#define XINPUT_GAMEPAD_LS_UP_CONFIGDEF    0xABC7
#define XINPUT_GAMEPAD_LS_DOWN_CONFIGDEF  0xABC8
#define XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF  0xABC9
#define XINPUT_GAMEPAD_LS_RIGHT_CONFIGDEF 0xABCA
#define XINPUT_GAMEPAD_RS_UP_CONFIGDEF    0xABCB
#define XINPUT_GAMEPAD_RS_DOWN_CONFIGDEF  0xABCC
#define XINPUT_GAMEPAD_RS_LEFT_CONFIGDEF  0xABCD
#define XINPUT_GAMEPAD_RS_RIGHT_CONFIGDEF 0xABCE

#define XINPUT_GAMEPAD_DPAD_CONFIGDEF 0xDAD0  // only for internal use
#define XINPUT_GAMEPAD_LS_CONFIGDEF   0xDAD1  // only for internal use
#define XINPUT_GAMEPAD_RS_CONFIGDEF   0xDAD2  // only for internal use

#define XINPUT_GAMEPAD_DPAD_UPDOWN_CONFIGDEF    0xDAD3  // only for internal use
#define XINPUT_GAMEPAD_DPAD_LEFTRIGHT_CONFIGDEF 0xDAD4  // only for internal use

namespace InputBindings {
struct Binding {
  unsigned int hash;
  WORD value;
  const char* name;
  int textIndex;
  bool analog;

  constexpr Binding(WORD value, const char* name, int textIndex, bool analog)
      : hash(compiler_stringhash(name)), value(value), name(name), textIndex(textIndex), analog(analog) {}
};

static constexpr Binding bindings[] = {
  {XINPUT_GAMEPAD_DPAD_UP, "XINPUT_GAMEPAD_DPAD_UP", 10, false},
  {XINPUT_GAMEPAD_DPAD_DOWN, "XINPUT_GAMEPAD_DPAD_DOWN", 11, false},
  {XINPUT_GAMEPAD_DPAD_LEFT, "XINPUT_GAMEPAD_DPAD_LEFT", 12, false},
  {XINPUT_GAMEPAD_DPAD_RIGHT, "XINPUT_GAMEPAD_DPAD_RIGHT", 13, false},
  {XINPUT_GAMEPAD_START, "XINPUT_GAMEPAD_START", 7, false},
  {XINPUT_GAMEPAD_BACK, "XINPUT_GAMEPAD_BACK", 6, false},
  {XINPUT_GAMEPAD_LEFT_THUMB, "XINPUT_GAMEPAD_LEFT_THUMB", 8, false},
  {XINPUT_GAMEPAD_RIGHT_THUMB, "XINPUT_GAMEPAD_RIGHT_THUMB", 9, false},
  {XINPUT_GAMEPAD_LEFT_SHOULDER, "XINPUT_GAMEPAD_LEFT_SHOULDER", 4, false},
  {XINPUT_GAMEPAD_RIGHT_SHOULDER, "XINPUT_GAMEPAD_RIGHT_SHOULDER", 5, false},
  {XINPUT_GAMEPAD_A, "XINPUT_GAMEPAD_A", 0, false},
  {XINPUT_GAMEPAD_B, "XINPUT_GAMEPAD_B", 1, false},
  {XINPUT_GAMEPAD_X, "XINPUT_GAMEPAD_X", 2, false},
  {XINPUT_GAMEPAD_Y, "XINPUT_GAMEPAD_Y", 3, false},
  {XINPUT_GAMEPAD_LT_CONFIGDEF, "XINPUT_GAMEPAD_LT", 26, true},
  {XINPUT_GAMEPAD_RT_CONFIGDEF, "XINPUT_GAMEPAD_RT", 27, true},
  {XINPUT_GAMEPAD_LS_X_CONFIGDEF, "XINPUT_GAMEPAD_LS_X", 20, true},
  {XINPUT_GAMEPAD_LS_Y_CONFIGDEF, "XINPUT_GAMEPAD_LS_Y", 21, true},
  {XINPUT_GAMEPAD_RS_X_CONFIGDEF, "XINPUT_GAMEPAD_RS_X", 14, true},
  {XINPUT_GAMEPAD_RS_Y_CONFIGDEF, "XINPUT_GAMEPAD_RS_Y", 15, true},
  {XINPUT_GAMEPAD_LS_UP_CONFIGDEF, "XINPUT_GAMEPAD_LS_UP", 24, true},
  {XINPUT_GAMEPAD_LS_DOWN_CONFIGDEF, "XINPUT_GAMEPAD_LS_DOWN", 25, true},
  {XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF, "XINPUT_GAMEPAD_LS_LEFT", 22, true},
  {XINPUT_GAMEPAD_LS_RIGHT_CONFIGDEF, "XINPUT_GAMEPAD_LS_RIGHT", 23, true},
  {XINPUT_GAMEPAD_RS_UP_CONFIGDEF, "XINPUT_GAMEPAD_RS_UP", 18, true},
  {XINPUT_GAMEPAD_RS_DOWN_CONFIGDEF, "XINPUT_GAMEPAD_RS_DOWN", 19, true},
  {XINPUT_GAMEPAD_RS_LEFT_CONFIGDEF, "XINPUT_GAMEPAD_RS_LEFT", 16, true},
  {XINPUT_GAMEPAD_RS_RIGHT_CONFIGDEF, "XINPUT_GAMEPAD_RS_RIGHT", 17, true},
};

inline WORD ParseBinding(const char* name, bool analog, bool ignoreCase, bool allowDpad = false) {
  unsigned int hash = InputStringHash(name, ignoreCase);
  if (!analog) {
    if (hash == NFS_HASH("XINPUT_GAMEPAD_LB")) return XINPUT_GAMEPAD_LEFT_SHOULDER;
    if (hash == NFS_HASH("XINPUT_GAMEPAD_RB")) return XINPUT_GAMEPAD_RIGHT_SHOULDER;
  } else if (allowDpad && hash == NFS_HASH("XINPUT_GAMEPAD_DPAD")) {
    return XINPUT_GAMEPAD_DPAD_CONFIGDEF;
  }
  for (const Binding& binding : bindings)
    if (binding.analog == analog && binding.hash == hash) return binding.value;
  return 0;
}

inline WORD ParseButton(const char* name, bool ignoreCase) {
  return ParseBinding(name, false, ignoreCase);
}

inline WORD ParseAnalog(const char* name, bool ignoreCase, bool allowDpad = false) {
  return ParseBinding(name, true, ignoreCase, allowDpad);
}

inline const Binding* Find(WORD value) {
  for (const Binding& binding : bindings)
    if (binding.value == value) return &binding;
  return nullptr;
}

inline const char* Name(WORD value, const char* unbound = "-1") {
  const Binding* binding = Find(value);
  return binding ? binding->name : unbound;
}

template <size_t N>
inline const char* ControlText(WORD value, const char* const (&labels)[N], const char* unbound) {
  static_assert(N >= 28, "Controller label table must cover every stick and button");
  const Binding* binding = Find(value);
  return binding ? labels[binding->textIndex] : unbound;
}
}  // namespace InputBindings
