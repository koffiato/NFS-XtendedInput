// Shared normalized controller reads. MIT; see LICENSE.
#pragma once
#include "Bindings.h"

inline bool InputAnalogPressed(const XINPUT_GAMEPAD& pad, WORD binding, int stickThreshold, int triggerThreshold) {
  switch (binding) {
    case XINPUT_GAMEPAD_LT_CONFIGDEF: return pad.bLeftTrigger > triggerThreshold;
    case XINPUT_GAMEPAD_RT_CONFIGDEF: return pad.bRightTrigger > triggerThreshold;
    case XINPUT_GAMEPAD_LS_UP_CONFIGDEF: return pad.sThumbLY > stickThreshold;
    case XINPUT_GAMEPAD_LS_DOWN_CONFIGDEF: return pad.sThumbLY < -stickThreshold;
    case XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF: return pad.sThumbLX < -stickThreshold;
    case XINPUT_GAMEPAD_LS_RIGHT_CONFIGDEF: return pad.sThumbLX > stickThreshold;
    case XINPUT_GAMEPAD_RS_UP_CONFIGDEF: return pad.sThumbRY > stickThreshold;
    case XINPUT_GAMEPAD_RS_DOWN_CONFIGDEF: return pad.sThumbRY < -stickThreshold;
    case XINPUT_GAMEPAD_RS_LEFT_CONFIGDEF: return pad.sThumbRX < -stickThreshold;
    case XINPUT_GAMEPAD_RS_RIGHT_CONFIGDEF: return pad.sThumbRX > stickThreshold;
    default: return false;
  }
}

// Preserve the games' 32767 divisor: the negative endpoint is slightly below -1.
inline float InputAnalogValue(const XINPUT_GAMEPAD& pad, WORD bind) {
  float result = 0.0f;
  switch (bind) {
    case XINPUT_GAMEPAD_LT_CONFIGDEF:
      result = pad.bLeftTrigger / 255.0f;
      break;
    case XINPUT_GAMEPAD_RT_CONFIGDEF:
      result = pad.bRightTrigger / 255.0f;
      break;
    case XINPUT_GAMEPAD_LS_X_CONFIGDEF:
      result = pad.sThumbLX / (float)(0x7FFF);
      break;
    case XINPUT_GAMEPAD_RS_X_CONFIGDEF:
      result = pad.sThumbRX / (float)(0x7FFF);
      break;
    case XINPUT_GAMEPAD_LS_Y_CONFIGDEF:
      result = pad.sThumbLY / (float)(0x7FFF);
      break;
    case XINPUT_GAMEPAD_RS_Y_CONFIGDEF:
      result = pad.sThumbRY / (float)(0x7FFF);
      break;
    case XINPUT_GAMEPAD_LS_UP_CONFIGDEF:
      if (pad.sThumbLY > 0) result = pad.sThumbLY / (float)(0x7FFF);
      break;
    case XINPUT_GAMEPAD_LS_DOWN_CONFIGDEF:
      if (pad.sThumbLY < 0) result = -pad.sThumbLY / (float)(0x7FFF);
      break;
    case XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF:
      if (pad.sThumbLX < 0) result = -pad.sThumbLX / (float)(0x7FFF);
      break;
    case XINPUT_GAMEPAD_LS_RIGHT_CONFIGDEF:
      if (pad.sThumbLX > 0) result = pad.sThumbLX / (float)(0x7FFF);
      break;
    case XINPUT_GAMEPAD_RS_UP_CONFIGDEF:
      if (pad.sThumbRY > 0) result = pad.sThumbRY / (float)(0x7FFF);
      break;
    case XINPUT_GAMEPAD_RS_DOWN_CONFIGDEF:
      if (pad.sThumbRY < 0) result = -pad.sThumbRY / (float)(0x7FFF);
      break;
    case XINPUT_GAMEPAD_RS_LEFT_CONFIGDEF:
      if (pad.sThumbRX < 0) result = -pad.sThumbRX / (float)(0x7FFF);
      break;
    case XINPUT_GAMEPAD_RS_RIGHT_CONFIGDEF:
      if (pad.sThumbRX > 0) result = pad.sThumbRX / (float)(0x7FFF);
      break;
    default:
      break;
  }
  return result;
}

inline uint16_t InputAnalogActivity(const XINPUT_GAMEPAD& pad) {
  if (InputAnalogValue(pad, XINPUT_GAMEPAD_LT_CONFIGDEF) >= 1.0f) return XINPUT_GAMEPAD_LT_CONFIGDEF;
  if (InputAnalogValue(pad, XINPUT_GAMEPAD_RT_CONFIGDEF) >= 1.0f) return XINPUT_GAMEPAD_RT_CONFIGDEF;
  if (InputAnalogValue(pad, XINPUT_GAMEPAD_LS_UP_CONFIGDEF) >= 1.0f) return XINPUT_GAMEPAD_LS_UP_CONFIGDEF;
  if (InputAnalogValue(pad, XINPUT_GAMEPAD_LS_DOWN_CONFIGDEF) >= 1.0f) return XINPUT_GAMEPAD_LS_DOWN_CONFIGDEF;
  if (InputAnalogValue(pad, XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF) >= 1.0f) return XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF;
  if (InputAnalogValue(pad, XINPUT_GAMEPAD_LS_RIGHT_CONFIGDEF) >= 1.0f) return XINPUT_GAMEPAD_LS_RIGHT_CONFIGDEF;
  if (InputAnalogValue(pad, XINPUT_GAMEPAD_RS_UP_CONFIGDEF) >= 1.0f) return XINPUT_GAMEPAD_RS_UP_CONFIGDEF;
  if (InputAnalogValue(pad, XINPUT_GAMEPAD_RS_DOWN_CONFIGDEF) >= 1.0f) return XINPUT_GAMEPAD_RS_DOWN_CONFIGDEF;
  if (InputAnalogValue(pad, XINPUT_GAMEPAD_RS_LEFT_CONFIGDEF) >= 1.0f) return XINPUT_GAMEPAD_RS_LEFT_CONFIGDEF;
  if (InputAnalogValue(pad, XINPUT_GAMEPAD_RS_RIGHT_CONFIGDEF) >= 1.0f) return XINPUT_GAMEPAD_RS_RIGHT_CONFIGDEF;
  return 0;
}
