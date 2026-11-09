// Native frontend ABI and shared services. MIT; see LICENSE.
#pragma once
#include "../_vs/stdafx.h"
#include <cstddef>

enum FEObjType {
  FE_None         = 0,
  FE_Image        = 1,
  FE_String       = 2,
  FE_Model        = 3,
  FE_List         = 4,
  FE_Group        = 5,
  FE_CodeList     = 6,
  FE_Movie        = 7,
  FE_Effect       = 8,
  FE_ColoredImage = 9,
  FE_AnimImage    = 10,
  FE_SimpleImage  = 11,
  FE_MultiImage   = 12,
  FE_UserMin      = 256,
};

enum FEObject_Flags {
  FF_HideInEdit            = 0x80000000u,
  FF_AffectAllScripts      = 0x40000000,
  FF_PerspectiveProjection = 0x20000000,
  FF_IsButton              = 0x10000000,
  FF_ObjectLocked          = 0x8000000,
  FF_IgnoreButton          = 0x4000000,
  FF_Dirty                 = 0x2000000,
  FF_DirtyTransform        = 0x1000000,
  FF_DirtyColor            = 0x800000,
  FF_DirtyCode             = 0x400000,
  FF_CodeSuppliedResource  = 0x200000,
  FF_UsesLibraryObject     = 0x100000,
  FF_DontNavigate          = 0x80000,
  FF_SaveStaticTracks      = 0x40000,
  FF_MouseObject           = 0x20000,
};

struct FEObject {
  void*          pDestructorCallback;
  long           Padding[2];
  unsigned long  GUID;
  unsigned long  NameHash;
  char*          pName;
  FEObjType      Type;
  unsigned long  Flags;
  unsigned short RenderContext;
  unsigned short ResourceIndex;
  unsigned long  Handle;
  unsigned long  UserParam;
  unsigned char* pData;
  unsigned long  DataSize;
  int            Responses;
  int            Scripts;
  void*          pCurrentScript;
  void*          Cached;
};

struct FEObjectCallbackStruct {
  void* Destructor;
  void* Function;
};

static_assert(sizeof(FEObject) == 0x44, "Native FEObject size");
static_assert(offsetof(FEObject, Type) == 0x18, "Native FEObject type offset");
static_assert(offsetof(FEObject, Flags) == 0x1C, "Native FEObject flags offset");

extern FEObject* (*FEngFindObject)(char* pkg_name, unsigned int obj_hash);
bool cFEng_IsPackageInControl_Fast(unsigned int pkg_name_hash);
bool __stdcall FEObjectCallback_Function(FEObject* inobj);
#ifndef GAME_MW
void FEngSetVisible(void* object);
void FEngSetInvisible(void* object);
extern void(__thiscall* FEPackage_ForAllObjects)(void* package, void* callback);
extern void(__thiscall* FEngine_ProcessPadsForPackage)(void* engine, void* package);
extern char* (*GetLocalizedString)(unsigned int hash);
#endif
#ifdef GAME_CARBON
extern void*(__thiscall* FEPackage_FindObjectByHash)(void* package, unsigned int hash);
extern void(__thiscall* FEWorldMapStateManager_HandleScreenTick)(void* manager);
extern void(__thiscall* WorldMap_UnfocusQuickList)(void* map);
extern void(__thiscall* WorldMap_SetQuickListInFocus)(void* map);
#endif
