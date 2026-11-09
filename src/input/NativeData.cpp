// Storage for target-specific native bindings and default texture tables. MIT; see LICENSE.
#include "../_vs/stdafx.h"
#include "Addresses.h"
#include "StringHash.h"

#ifdef GAME_MW
unsigned int DebugWorldCameraMover_Update_Addr = 0x0047E7B0;

unsigned int FE_DefaultButtonTexHashes[FE_TEX_HASH_COUNT] = {0,
                                                             FE_ACCEPT_TEX_HASH,
                                                             FE_CANCEL_TEX_HASH,
                                                             FE_BUTTON4_TEX_HASH,
                                                             FE_BUTTON5_TEX_HASH,
                                                             FE_LTRIGGER_TEX_HASH,
                                                             FE_RTRIGGER_TEX_HASH,
                                                             FE_START_TEX_HASH,
                                                             FE_SELECT_TEX_HASH,
                                                             FE_BUTTON1_TEX_HASH,
                                                             FE_BUTTON2_TEX_HASH,
                                                             FE_BUTTON3_TEX_HASH,
                                                             FE_RANALOG_TEX_HASH,
                                                             FE_RANALOG_UP_TEX_HASH,
                                                             FE_RANALOG_UP_SWEDISH_TEX_HASH,
                                                             FE_RANALOG_UP_FRENCH_TEX_HASH,
                                                             FE_RANALOG_DOWN_TEX_HASH,
                                                             FE_RANALOG_DOWN_SWEDISH_TEX_HASH,
                                                             FE_RANALOG_DOWN_FRENCH_TEX_HASH,
                                                             FE_LANALOG_TEX_HASH,
                                                             FE_LANALOG_UP_TEX_HASH,
                                                             FE_LANALOG_UP_SWEDISH_TEX_HASH,
                                                             FE_LANALOG_UP_FRENCH_TEX_HASH,
                                                             FE_LANALOG_DOWN_TEX_HASH,
                                                             FE_LANALOG_DOWN_SWEDISH_TEX_HASH,
                                                             FE_LANALOG_DOWN_FRENCH_TEX_HASH,
                                                             FE_DPAD_TEX_HASH,
                                                             FE_DPAD_UPDOWN_TEX_HASH,
                                                             FE_DPAD_LEFTRIGHT_TEX_HASH,
                                                             FE_DPAD_UP_TEX_HASH,
                                                             FE_DPAD_DOWN_TEX_HASH,
                                                             FE_DPAD_LEFT_TEX_HASH,
                                                             FE_DPAD_RIGHT_TEX_HASH};

int (*FEPrintf)(const char* pkg_name, void* FEObject, const char* format, ...) = (int (*)(const char*, void*, const char*, ...))FEPRINTF_ADDRESS;

void* (*FEngFindObject_Title)(char* pkg_name, unsigned int obj_hash)           = (void* (*)(char*, unsigned int))FENG_FINDOBJECT_ADDR;

char* SplashPkgName   = "MW_LS_Splash.fng";

char* WSSplashPkgName = "WS_MW_LS_Splash.fng";
#endif

#ifdef GAME_CARBON
unsigned int DebugWorldCameraMover_Update_Addr = 0x004932A0;

unsigned int FE_DefaultButtonTexHashes[FE_TEX_HASH_COUNT] = {
    0,
    FE_ACCEPT_TEX_HASH,
    FE_CANCEL_TEX_HASH,
    FE_BUTTON4_TEX_HASH,
    FE_BUTTON5_TEX_HASH,
    FE_LTRIGGER_TEX_HASH,
    FE_RTRIGGER_TEX_HASH,
    FE_BUTTON0_TEX_HASH,
    FE_START_TEX_HASH,
    FE_SELECT_TEX_HASH,
    FE_BUTTON1_TEX_HASH,
    FE_BUTTON2_TEX_HASH,
    FE_BUTTON3_TEX_HASH,
    FE_RANALOG_TEX_HASH,
    FE_RANALOG_UP_TEX_HASH,
    FE_RANALOG_UP_SWEDISH_TEX_HASH,
    FE_RANALOG_UP_FRENCH_TEX_HASH,
    FE_RANALOG_DOWN_TEX_HASH,
    FE_RANALOG_DOWN_SWEDISH_TEX_HASH,
    FE_RANALOG_DOWN_FRENCH_TEX_HASH,
    FE_LANALOG_TEX_HASH,
    FE_LANALOG_UP_TEX_HASH,
    FE_LANALOG_UP_SWEDISH_TEX_HASH,
    FE_LANALOG_UP_FRENCH_TEX_HASH,
    FE_LANALOG_DOWN_TEX_HASH,
    FE_LANALOG_DOWN_SWEDISH_TEX_HASH,
    FE_LANALOG_DOWN_FRENCH_TEX_HASH,
    FE_DPAD_TEX_HASH,
    FE_DPAD_UPDOWN_TEX_HASH,
    FE_DPAD_LEFTRIGHT_TEX_HASH,
    FE_DPAD_UP_TEX_HASH,
    FE_DPAD_DOWN_TEX_HASH,
    FE_DPAD_LEFT_TEX_HASH,
    FE_DPAD_RIGHT_TEX_HASH,
};

int (*FE_String_Printf)(void* FEObject, const char* fmt, ...)        = (int (*)(void*, const char*, ...))FE_STRING_PRINTF_ADDRESS;

void* (*FEngFindObject_Title)(char* pkg_name, unsigned int obj_hash) = (void* (*)(char*, unsigned int))FENG_FINDOBJECT_ADDR;

char* SplashPkgName = "DEMO_SPLASH.fng";

void (*InitProfileSettings)() = (void (*)())0x00679BF0;
#endif

#ifdef GAME_PROSTREET
unsigned int DebugWorldCameraMover_Update_Addr = 0x00651BA0;

unsigned int FE_DefaultButtonTexHashes[FE_TEX_HASH_COUNT] = {
    0,
    FE_ACCEPT_TEX_HASH,
    FE_CANCEL_TEX_HASH,
    FE_BUTTON4_TEX_HASH,
    FE_BUTTON5_TEX_HASH,
    FE_LTRIGGER_TEX_HASH,
    FE_RTRIGGER_TEX_HASH,
    FE_BUTTON0_TEX_HASH,
    FE_START_TEX_HASH,
    FE_SELECT_TEX_HASH,
    FE_BUTTON1_TEX_HASH,
    FE_BUTTON2_TEX_HASH,
    FE_BUTTON3_TEX_HASH,
    FE_RANALOG_TEX_HASH,
    FE_RANALOG_UP_TEX_HASH,
    FE_RANALOG_UP_SWEDISH_TEX_HASH,
    FE_RANALOG_UP_FRENCH_TEX_HASH,
    FE_RANALOG_DOWN_TEX_HASH,
    FE_RANALOG_DOWN_SWEDISH_TEX_HASH,
    FE_RANALOG_DOWN_FRENCH_TEX_HASH,
    FE_LANALOG_TEX_HASH,
    FE_LANALOG_UP_TEX_HASH,
    FE_LANALOG_UP_SWEDISH_TEX_HASH,
    FE_LANALOG_UP_FRENCH_TEX_HASH,
    FE_LANALOG_DOWN_TEX_HASH,
    FE_LANALOG_DOWN_SWEDISH_TEX_HASH,
    FE_LANALOG_DOWN_FRENCH_TEX_HASH,
    FE_DPAD_TEX_HASH,
    FE_DPAD_UPDOWN_TEX_HASH,
    FE_DPAD_LEFTRIGHT_TEX_HASH,
    FE_DPAD_UP_TEX_HASH,
    FE_DPAD_DOWN_TEX_HASH,
    FE_DPAD_LEFT_TEX_HASH,
    FE_DPAD_RIGHT_TEX_HASH,
};

int (*FE_String_Printf)(void* FEObject, const char* fmt, ...)        = (int (*)(void*, const char*, ...))FE_STRING_PRINTF_ADDRESS;

void* (*FEngFindObject_Title)(char* pkg_name, unsigned int obj_hash) = (void* (*)(char*, unsigned int))FENG_FINDOBJECT_ADDR;

char* SplashPkgName = "Splash.fng";

void (*InitProfileSettings)() = (void (*)())0x006A56C0;
#endif

#ifdef GAME_UC
float smallfloat = 0.000001f;

void (*InitProfileSettings)() = (void (*)())0x006774C0;

unsigned int DebugWorldCameraMover_Update_Addr = 0x004E4040;
#endif

#ifdef GAME_WORLD
unsigned int MainBase;

float smallfloat = 0.000001f;

unsigned int DebugWorldCameraMover_Update_Addr;
#endif
