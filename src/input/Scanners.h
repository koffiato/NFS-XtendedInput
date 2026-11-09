// Underground event scanners and steering callbacks. MIT; see LICENSE.
#pragma once

int ScannerPort(void* joystick) { return (uintptr_t)joystick == JOY2_ADDR ? 1 : 0; }

bool ScannerPedal(unsigned int event) {
  return event == JOY_EVENT_THROTTLE || event == JOY_EVENT_BRAKE ||
         event == JOY_EVENT_THROTTLE_ANALOG || event == JOY_EVENT_THROTTLE_ANALOG_ALTERNATE ||
         event == JOY_EVENT_BRAKE_ANALOG || event == JOY_EVENT_BRAKE_ANALOG_ALTERNATE;
}

template <class T>
int ScannerEmit(const ScannerConfig& config, T& previous, T value, bool downOnly, bool always = false) {
  bool changed = previous != value;
  previous = value;
  if (downOnly) return changed && value ? config.JoyEvent : 0;
  return changed || always ? config.JoyEvent + (value ? config.param << 8 : 0) : 0;
}

int ScannerKeyboard(ScannerConfig& config, int port, bool downOnly) {
  if (KeyboardReadingMode == KB_READINGMODE_UNBUFFERED_RAW && !config.keycode) return 0;
  if (KeyboardReadingMode != KB_READINGMODE_UNBUFFERED_RAW) port = 0;
  bool value = InputKeyPressed(port, config.keycode);
  return ScannerEmit(config, EventStatesKB[port][config.JoyEvent], value, downOnly, ScannerPedal(config.JoyEvent));
}

bool ScannerAnalogPressed(const ScannerConfig& config, int port) {
  unsigned int event = config.JoyEvent;
  bool shifting = event == JOY_EVENT_SHIFTUP || event == JOY_EVENT_SHIFTDOWN ||
                  event == JOY_EVENT_SHIFTUP_ALTERNATE || event == JOY_EVENT_SHIFTDOWN_ALTERNATE;
  return InputAnalogPressed(g_Controllers[port].state.Gamepad, config.BitmaskStuff,
                            shifting ? SHIFT_ANALOG_THRESHOLD : FEUPDOWN_ANALOG_THRESHOLD, TRIGGER_ACTIVATION_THRESHOLD);
}

int Scanner_DigitalUpOrDown_XInput(void* node, unsigned int* unk1, unsigned int unk2, ScannerConfig* config, void* joystick) {
  int port = ScannerPort(joystick);
  WORD value = g_Controllers[port].state.Gamepad.wButtons & config->BitmaskStuff;
  return ScannerEmit(*config, EventStates[port][config->JoyEvent], value, false, ScannerPedal(config->JoyEvent));
}
int Scanner_DigitalUpOrDown_KB(void* node, unsigned int* unk1, unsigned int unk2, ScannerConfig* config, void* joystick) { return ScannerKeyboard(*config, ScannerPort(joystick), false); }
int Scanner_DigitalUpOrDown(void* node, unsigned int* unk1, unsigned int unk2, ScannerConfig* config, void* joystick) { return Scanner_DigitalUpOrDown_XInput(node, unk1, unk2, config, joystick) | Scanner_DigitalUpOrDown_KB(node, unk1, unk2, config, joystick); }

int Scanner_DigitalAnalog_XInput(void* node, unsigned int* unk1, unsigned int unk2, ScannerConfig* config, void* joystick) {
  int port = ScannerPort(joystick);
  WORD value = ScannerAnalogPressed(*config, port);
  return ScannerEmit(*config, EventStates[port][config->JoyEvent], value, false);
}
int Scanner_DigitalDown_XInput(void* node, unsigned int* unk1, unsigned int unk2, ScannerConfig* config, void* joystick) {
  int port = ScannerPort(joystick);
  WORD value = g_Controllers[port].state.Gamepad.wButtons & config->BitmaskStuff;
  return ScannerEmit(*config, EventStates[port][config->JoyEvent], value, true, ScannerPedal(config->JoyEvent));
}
int Scanner_DigitalDown_KB(void* node, unsigned int* unk1, unsigned int unk2, ScannerConfig* config, void* joystick) { return ScannerKeyboard(*config, ScannerPort(joystick), true); }
int Scanner_DigitalDown(void* node, unsigned int* unk1, unsigned int unk2, ScannerConfig* config, void* joystick) { return Scanner_DigitalDown_XInput(node, unk1, unk2, config, joystick) | Scanner_DigitalDown_KB(node, unk1, unk2, config, joystick); }

int Scanner_DigitalDownAnalog_XInput(void* node, unsigned int* unk1, unsigned int unk2, ScannerConfig* config, void* joystick) {
  int port = ScannerPort(joystick);
  WORD value = ScannerAnalogPressed(*config, port);
  return ScannerEmit(*config, EventStates[port][config->JoyEvent], value, true);
}

int Scanner_DigitalAnalog(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	return Scanner_DigitalAnalog_XInput(EventNode, unk1, unk2, inScannerConfig, Joystick) | Scanner_DigitalUpOrDown_KB(EventNode, unk1, unk2, inScannerConfig, Joystick);
}

int Scanner_DigitalDownAnalog(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	return Scanner_DigitalDownAnalog_XInput(EventNode, unk1, unk2, inScannerConfig, Joystick) | Scanner_DigitalDown_KB(EventNode, unk1, unk2, inScannerConfig, Joystick);
}

int Scanner_Analog(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	int ci = ScannerPort(Joystick);

	BYTE axis = 0;

	switch (inScannerConfig->BitmaskStuff)
	{
	case XINPUT_GAMEPAD_LT_CONFIGDEF:
		axis = g_Controllers[ci].state.Gamepad.bLeftTrigger;
		break;
	case XINPUT_GAMEPAD_RT_CONFIGDEF:
		axis = g_Controllers[ci].state.Gamepad.bRightTrigger;
		break;
	case XINPUT_GAMEPAD_LS_X_CONFIGDEF:
	case XINPUT_GAMEPAD_RS_X_CONFIGDEF:
	case XINPUT_GAMEPAD_LS_Y_CONFIGDEF:
	case XINPUT_GAMEPAD_RS_Y_CONFIGDEF:
		axis = (char)(GetAnalogStickValue(ci, inScannerConfig->BitmaskStuff) * (float)(0x7F)) + 0x80;
		break;
	default:
		break;
	}

	// invert some broken axis
	if (inScannerConfig->JoyEvent == JOY_EVENT_DEBUG_CAMERA_INOUT)
		axis = -axis;
	if (inScannerConfig->JoyEvent == JOY_EVENT_CARSEL_ORBIT_UPDOWN)
		axis = -axis;
#ifdef GAME_UG2
	if (inScannerConfig->JoyEvent == JOY_EVENT_HYDRAULIC_PRESSURIZE_ANALOG_UD)
		axis = -axis;
	if (inScannerConfig->JoyEvent == JOY_EVENT_HYDRAULIC_BOUNCE_ANALOG_UD)
		axis = -axis;
#endif

	if ((axis != EventStates[ci][inScannerConfig->JoyEvent]) || (inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE_ANALOG) || (inScannerConfig->JoyEvent == JOY_EVENT_THROTTLE_ANALOG_ALTERNATE) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE_ANALOG) || (inScannerConfig->JoyEvent == JOY_EVENT_BRAKE_ANALOG_ALTERNATE))
	{
		EventStates[ci][inScannerConfig->JoyEvent] = axis;
		return inScannerConfig->JoyEvent + (axis << 8);
	}

	return 0;
}

int Scanner_Analog_DragSteer(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	int ci = ScannerPort(Joystick);

	BYTE axis = 0;
	char signed_axis = 0;

	switch (inScannerConfig->BitmaskStuff)
	{
	case XINPUT_GAMEPAD_LS_X_CONFIGDEF:
	case XINPUT_GAMEPAD_RS_X_CONFIGDEF:
	case XINPUT_GAMEPAD_LS_Y_CONFIGDEF:
	case XINPUT_GAMEPAD_RS_Y_CONFIGDEF:
		axis = (BYTE)(GetAnalogStickValue(ci, inScannerConfig->BitmaskStuff) * (float)(0x7F));
		break;
	default:
		return 0;
	}
	signed_axis = axis;

	if (axis != EventStates[ci][inScannerConfig->JoyEvent])
	{
		EventStates[ci][inScannerConfig->JoyEvent] = axis;
		if ((inScannerConfig->JoyEvent == JOY_EVENT_DRAG_RACE_CHANGE_LANE_LEFT_ANALOG) && signed_axis < 0)
			return inScannerConfig->JoyEvent + (axis << 8);
		if ((inScannerConfig->JoyEvent == JOY_EVENT_DRAG_RACE_CHANGE_LANE_RIGHT_ANALOG) && signed_axis > 0)
			return inScannerConfig->JoyEvent + (axis << 8);
	}
	return 0;
}

// STEERING HANDLER

void RealDriver_JoyHandler_Steer(int JoystickPort, int JoyEvent, char Value, void* RealDriver)
{
	float* SteerVal = (float*)((int)RealDriver + REALDRIVER_STEER_OFFSET);
	float AnalogSteerValue = GetAnalogStickValue(JoystickPort, SteerLeftAxis) - GetAnalogStickValue(JoystickPort, SteerRightAxis);
	float DigitalSteerValue = 0.0f;
	float DigitalSteerValueKB = 0.0f;
	float FinalSteerValue;

	// get gamepad digital steering
	WORD wButtons = g_Controllers[JoystickPort].state.Gamepad.wButtons;
	if (wButtons & SteerLeftButton)
	{
		DigitalSteerValue = DigitalSteerValue + 1.0;
	}
	if (wButtons & SteerRightButton)
	{
		DigitalSteerValue = DigitalSteerValue - 1.0;
	}

	DigitalSteerValueKB = (float)InputKeyPressed(JoystickPort, SteerLeftVKey) -
	                     (float)InputKeyPressed(JoystickPort, SteerRightVKey);

	FinalSteerValue = AnalogSteerValue + DigitalSteerValue + DigitalSteerValueKB;

	// cap the min/max
	if (FinalSteerValue > 1.0)
		FinalSteerValue = 1.0;
	if (FinalSteerValue < -1.0)
		FinalSteerValue = -1.0;

	*SteerVal = FinalSteerValue;
}

int Scanner_CombinedSteering(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	// always return a value to trigger polling
	return inScannerConfig->JoyEvent + (1 << 8);
}

int Scanner_TypeChanged(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick)
{
	return 0;
}
