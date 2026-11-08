#include "Main.hpp"

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
#ifdef GAME_WORLD
    MainBase = (uintptr_t)GetModuleHandleA(NULL) - 0x400000;
#endif
    Init();
  } else if (reason == DLL_PROCESS_DETACH) {
#if INPUT_SCANNERS || defined(HAS_RUMBLE)
    RumbleStopAllPads(true);
#endif
#if INPUT_SCANNERS
    if (RumbleLog) fclose(RumbleLog);
#endif
  }
  return TRUE;
}
