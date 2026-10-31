// Diagnostic logs stay readable while the game is running. MIT; see LICENSE.
#pragma once
#include <cstdio>
#include <share.h>

inline FILE* RumbleOpenLog(const char* path) { return _fsopen(path, "w", _SH_DENYNO); }
