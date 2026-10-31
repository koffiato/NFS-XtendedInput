/*
// clang-format off
// 
//    MIT License
//    Need for Speed (Black Box, MW & newer) - Xtended Input plugin
//    Bringing native XInput to NFS
//    
//    Copyright (c) 2022–2023 Lovro Plese (Xan/Tenjoin)
//    Copyright (c) 2023 Berkay Yigit <mail@berkay.link>
//
// clang-format on
*/

#pragma once

#include "input/StringHash.h"

#define FE_SPLASH_TEXT_XBOX    "Press MENU or A button"
#define FE_SPLASH_TEXT_XBOX360 "Press START or A button"
#define FE_SPLASH_TEXT_PS4     "Press OPTIONS or X button"
#define FE_SPLASH_TEXT_PS3     "Press START or X button"
#define FE_SPLASH_TEXT_SWITCH  "Press + button"
#define FE_SPLASH_TEXT_PC      "CLICK or ENTER to continue"

#define LASTCONTROLLED_KB         0
#define LASTCONTROLLED_CONTROLLER 1
unsigned int LastControlledDevice         = 0;  // 0 = keyboard, 1 = controller
unsigned int LastControlledDeviceOldState = 0;

#define CONTROLLERICON_XBOXONE 0
#define CONTROLLERICON_PS4     1
#define CONTROLLERICON_XBOX360 2
#define CONTROLLERICON_PS3     3
#define CONTROLLERICON_SWITCH  4
#define CONTROLLERICON_XB360MW 5

unsigned int ControllerIconMode = 0;  // read modes above

bool bInDebugWorldCamera   = false;
bool bGlobalDoPolling      = true;
bool bBlockThisPoll        = false;
bool bEnableSplashTakeover = true;
#ifndef NO_FENG
bool bConsoleFEng          = false;

constexpr const char* cfgIniName      = "scripts\\NFS_XtendedInput.ini";
constexpr const char* defMappingName  = "scripts\\NFS_XtendedInput.default.ini";
constexpr const char* userMappingName = "NFS_XtendedInput.usermap.ini";
constexpr const char* userMappingDir  = "scripts\\XtendedInputMaps";
constexpr const char* cursorSkinPath  = "scripts\\nfs_cursor.cur";

#endif