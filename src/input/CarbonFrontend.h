// Carbon world-map callbacks installed by the input hooks. MIT; see LICENSE.
#pragma once
#include "../_vs/stdafx.h"

void __stdcall FEngine_ProcessPadsForPackage_Hook(void* package);
void* __stdcall FEPackage_FindObjectByHash_Hide_Hook(unsigned int hash);
void* __stdcall FEPackage_FindObjectByHash_Show_Hook(unsigned int hash);
void __stdcall FEWorldMapStateManager_HandleScreenTick_Hook();
void __stdcall WorldMap_SetQuickListInFocus_Hook();
void __stdcall WorldMap_UnfocusQuickList_Hook();
void FEPrintf_Hook_WorldMap(char* package, unsigned int objectHash, char* format, char* text);
