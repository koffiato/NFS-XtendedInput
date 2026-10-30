// One entry point and shared feedback implementation for all seven targets.
#if defined(GAME_UG) || defined(GAME_UG2)
#include "../underground/dllmain.cpp"
#else
#include "_win32/dllmain.cpp"
#endif
