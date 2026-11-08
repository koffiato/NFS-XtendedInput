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

#include "input/State.h"

#define FE_SPLASH_TEXT_XBOX    "Press MENU or A button"
#define FE_SPLASH_TEXT_XBOX360 "Press START or A button"
#define FE_SPLASH_TEXT_PS4     "Press OPTIONS or X button"
#define FE_SPLASH_TEXT_PS3     "Press START or X button"
#define FE_SPLASH_TEXT_SWITCH  "Press + button"
#define FE_SPLASH_TEXT_PC      "CLICK or ENTER to continue"

char defMappingName[MAX_PATH] = "scripts\\NFS_XtendedInput.default.ini";
constexpr const char* userMappingName = "NFS_XtendedInput.usermap.ini";
constexpr const char* userMappingDir  = "scripts\\XtendedInputMaps";
constexpr const char* cursorSkinPath  = "scripts\\nfs_cursor.cur";

