// Carbon world-map frontend hooks. MIT; see LICENSE.
#include "Target.h"
#ifdef GAME_CARBON
#include "../_vs/stdafx.h"
#include "State.h"
#include "Addresses.h"
#include "NativeFrontendAPI.h"
#include "CarbonFrontend.h"

#pragma runtime_checks("", off)
// world map fixes for Carbon
void __stdcall FEngine_ProcessPadsForPackage_Hook(void* FEPackage) {
  unsigned int thethis = 0;
  _asm mov     thethis, ecx;

  if (cFEng_IsPackageInControl_Fast(WORLDMAPMAIN_FNG_NAMEHASH) || cFEng_IsPackageInControl_Fast(WORLDMAPQUICKLIST_FNG_NAMEHASH) ||
      cFEng_IsPackageInControl_Fast(WORLDMAPWORLDVIEW_FNG_NAMEHASH)) {
    FEObjectCallbackStruct callback  = {NULL, &FEObjectCallback_Function};
    void*                  cbpointer = &callback;

    FEPackage_ForAllObjects(FEPackage, &cbpointer);
  }
  return FEngine_ProcessPadsForPackage((void*)thethis, FEPackage);
}

void* __stdcall FEPackage_FindObjectByHash_Hide_Hook(unsigned int namehash) {
  unsigned int thethis = 0;
  _asm mov     thethis, ecx;

  FEngSetInvisible(FEPackage_FindObjectByHash((void*)thethis, WORLDMAP_BUTTONGROUP_CONSOLE));

  return FEPackage_FindObjectByHash((void*)thethis, namehash);
}

void* __stdcall FEPackage_FindObjectByHash_Show_Hook(unsigned int namehash) {
  unsigned int thethis = 0;
  _asm mov     thethis, ecx;

  FEngSetVisible(FEPackage_FindObjectByHash((void*)thethis, WORLDMAP_BUTTONGROUP_CONSOLE));

  return FEPackage_FindObjectByHash((void*)thethis, namehash);
}

bool bQuickListFocused = false;

void __stdcall FEWorldMapStateManager_HandleScreenTick_Hook() {
  unsigned int thethis = 0;
  _asm mov     thethis, ecx;

  if ((*(int*)GAMEFLOWMANAGER_STATUS_ADDR == 3) && *(int*)WORLDMAP_INSTANCE_ADDR) {
    // change text from "Free Roam" to "Back" during FE for the console UI element
    FE_String_Printf(FEngFindObject(*(char**)(*(int*)WORLDMAP_INSTANCE_ADDR + 0xC), 0x0EF36117), GetLocalizedString(0x8CD567B8));
    FE_String_Printf(FEngFindObject(*(char**)(*(int*)WORLDMAP_INSTANCE_ADDR + 0xC), 0xE2DACDBC), GetLocalizedString(0x8CD567B8));
  }

  if (bUseDynamicFEngSwitching) {
    if (bQuickListFocused && *(int*)WORLDMAP_INSTANCE_ADDR && cFEng_IsPackageInControl_Fast(WORLDMAPQUICKLIST_FNG_NAMEHASH)) {
      FEngSetInvisible(FEngFindObject(*(char**)(*(int*)WORLDMAP_INSTANCE_ADDR + 0xC), WORLDMAP_BUTTONGROUP_CONSOLE));
      FEngSetInvisible(FEngFindObject(*(char**)(*(int*)WORLDMAP_INSTANCE_ADDR + 0xC), WORLDMAP_BUTTONGROUP_PC));
    } else if (*(int*)WORLDMAP_INSTANCE_ADDR) {
      if (bConsoleFEng)
        FEngSetVisible(FEngFindObject(*(char**)(*(int*)WORLDMAP_INSTANCE_ADDR + 0xC), WORLDMAP_BUTTONGROUP_CONSOLE));
      else
        FEngSetVisible(FEngFindObject(*(char**)(*(int*)WORLDMAP_INSTANCE_ADDR + 0xC), WORLDMAP_BUTTONGROUP_PC));
    }
  }

  return FEWorldMapStateManager_HandleScreenTick((void*)thethis);
}

void __stdcall WorldMap_SetQuickListInFocus_Hook() {
  unsigned int thethis = 0;
  _asm mov     thethis, ecx;

  bQuickListFocused = true;

  return WorldMap_SetQuickListInFocus((void*)thethis);
}

void __stdcall WorldMap_UnfocusQuickList_Hook() {
  unsigned int thethis = 0;
  _asm mov     thethis, ecx;

  bQuickListFocused = false;

  if (bConsoleFEng)
    FEngSetVisible(FEngFindObject(*(char**)(*(int*)WORLDMAP_INSTANCE_ADDR + 0xC), WORLDMAP_BUTTONGROUP_CONSOLE));
  else
    FEngSetVisible(FEngFindObject(*(char**)(*(int*)WORLDMAP_INSTANCE_ADDR + 0xC), WORLDMAP_BUTTONGROUP_PC));

  return WorldMap_UnfocusQuickList((void*)thethis);
}

void FEPrintf_Hook_WorldMap(char* pkg_name, unsigned int object_hash, char* fmt, char* str) {
  FE_String_Printf(FEngFindObject(*(char**)(*(int*)WORLDMAP_INSTANCE_ADDR + 0xC), 0xCB235910), str);
  FE_String_Printf(FEngFindObject(*(char**)(*(int*)WORLDMAP_INSTANCE_ADDR + 0xC), 0x300B8082), str);
}
#pragma runtime_checks("", restore)
#endif
