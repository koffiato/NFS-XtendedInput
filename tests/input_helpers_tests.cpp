// Shared input contracts, without game code or controller hardware.
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <XInput.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include "../src/input/Keyboard.h"
#include "../src/input/Analog.h"

#ifdef GAME_PROSTREET
#include "../src/NFS_XtendedInput.h"
#include "../src/NFS_XtentedInput_ActionID.h"
#include "../src/NFS_XtendedInput_XInputConfig.h"
#else
#include "../src/ug/NFSU_XtendedInput_XInputConfig.h"
#endif

int main() {
  static_assert(NFS_HASH("VK_SPACE") == 0x5A95E06B, "NFS INI hash contract");
  assert(InputStringHash(nullptr) == 0);
  assert(InputStringHash("") == 0xFFFFFFFFu);
  assert(InputStringHash("vk_space", true) == NFS_HASH("VK_SPACE"));
  const char highByte[] = {(char)0xFF, 0};
  assert(InputStringHash(highByte) == 0xFFFFFFDEu); // signed-byte legacy hash
  assert(InputVirtualKey("VK_SPACE", false) == VK_SPACE);
  assert(InputVirtualKey("vk_space", false) == 0);
  assert(InputVirtualKey("vk_space", true) == VK_SPACE);
  assert(InputVirtualKey("VK_HANGEUL", false) == VK_HANGUL); // legacy spelling
  assert(InputVirtualKey("unknown", true) == 0);

  assert((WORD)ConvertXInputNameToBitmask("XINPUT_GAMEPAD_Y") == 0x8000);
  assert(ConvertXInputNameToBitmask("XINPUT_GAMEPAD_LB") == XINPUT_GAMEPAD_LEFT_SHOULDER);
  assert(ConvertXInputNameToBitmask("XINPUT_GAMEPAD_RB") == XINPUT_GAMEPAD_RIGHT_SHOULDER);
  assert(ConvertXInputNameToBitmask("XINPUT_GAMEPAD_LT") == 0);
  assert(ConvertXInputOtherConfigDef("XINPUT_GAMEPAD_A") == 0);
  assert(ConvertXInputOtherConfigDef("XINPUT_GAMEPAD_RS_LEFT") == XINPUT_GAMEPAD_RS_LEFT_CONFIGDEF);
  assert(ConvertXInputOtherConfigDef("unknown") == 0);
  assert(!strcmp(ConvertXInputBitmaskToName(XINPUT_GAMEPAD_LEFT_SHOULDER), "XINPUT_GAMEPAD_LEFT_SHOULDER"));
  assert(!strcmp(ConvertBitmaskToControlString(0, XINPUT_GAMEPAD_LS_DOWN_CONFIGDEF), "Left stick Down"));
  assert(!strcmp(ConvertBitmaskToControlString(1, XINPUT_GAMEPAD_A), "Cross"));
  assert(!strcmp(ConvertBitmaskToControlString(1, XINPUT_GAMEPAD_RT_CONFIGDEF), "R2"));
  assert(!strcmp(ConvertBitmaskToControlString(0, XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_B), unboundstr));
#ifdef GAME_PROSTREET
  assert(ConvertXInputNameToBitmask("xinput_gamepad_a") == XINPUT_GAMEPAD_A);
  assert(ConvertXInputOtherConfigDef("xinput_gamepad_lt") == XINPUT_GAMEPAD_LT_CONFIGDEF);
  assert(ConvertXInputOtherConfigDef("XINPUT_GAMEPAD_DPAD") == 0);
  assert(!strcmp(ConvertXInputBitmaskToName(0), "-1"));
  assert(!strcmp(ConvertBitmaskToControlString(CONTROLLERICON_PS3, XINPUT_GAMEPAD_A), "Cross"));
  assert(FindPCRemapActionID((uint32_t)PCRemapActionList.size()) == NULL_ACTION);
  assert(FindPCRemapActionID(UINT32_MAX) == NULL_ACTION);
  assert(FindPCRemapActionID(0) == GAMEACTION_GAS);
#else
  assert(ConvertXInputNameToBitmask("xinput_gamepad_a") == 0);
  assert(ConvertXInputOtherConfigDef("xinput_gamepad_lt") == 0);
  assert(ConvertXInputOtherConfigDef("XINPUT_GAMEPAD_DPAD") == XINPUT_GAMEPAD_DPAD_CONFIGDEF);
  assert(!strcmp(ConvertXInputBitmaskToName(0), "0"));
  assert(!strcmp(ConvertBitmaskToControlString(1, XINPUT_GAMEPAD_START), "Options (Start)"));
#endif

  XINPUT_GAMEPAD pad = {};
  assert(InputAnalogActivity(pad) == 0);
  pad.bLeftTrigger = 128;
  assert(InputAnalogValue(pad, XINPUT_GAMEPAD_LT_CONFIGDEF) == 128 / 255.0f);
  assert(InputAnalogActivity(pad) == 0); // remapping requires full activation
  pad.bLeftTrigger = pad.bRightTrigger = 255;
  assert(InputAnalogActivity(pad) == XINPUT_GAMEPAD_LT_CONFIGDEF); // original priority
  pad = {};
  pad.sThumbLX = -32768;
  assert(InputAnalogValue(pad, XINPUT_GAMEPAD_LS_X_CONFIGDEF) == -32768 / 32767.0f);
  assert(InputAnalogValue(pad, XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF) == 32768 / 32767.0f);
  assert(InputAnalogValue(pad, XINPUT_GAMEPAD_LS_RIGHT_CONFIGDEF) == 0);
  assert(InputAnalogActivity(pad) == XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF);
  pad = {};
  pad.sThumbRY = 32767;
  assert(InputAnalogValue(pad, XINPUT_GAMEPAD_RS_UP_CONFIGDEF) == 1);
  assert(InputAnalogValue(pad, XINPUT_GAMEPAD_RS_DOWN_CONFIGDEF) == 0);
  assert(InputAnalogValue(pad, 0xFFFF) == 0);
  puts("Shared input contracts passed.");
}
