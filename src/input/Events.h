#pragma once

#if INPUT_SCANNERS
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
	int ci = 0;
	if ((int)Joystick == JOY2_ADDR)
		ci = 1;

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
		axis = (char)(GetAnalogStickValue(ci, XINPUT_GAMEPAD_LS_X_CONFIGDEF) * (float)(0x7F)) + 0x80;
		break;
	case XINPUT_GAMEPAD_RS_X_CONFIGDEF:
		axis = (char)(GetAnalogStickValue(ci, XINPUT_GAMEPAD_RS_X_CONFIGDEF) * (float)(0x7F)) + 0x80;
		break;
	case XINPUT_GAMEPAD_LS_Y_CONFIGDEF:
		axis = (char)(GetAnalogStickValue(ci, XINPUT_GAMEPAD_LS_Y_CONFIGDEF) * (float)(0x7F)) + 0x80;
		break;
	case XINPUT_GAMEPAD_RS_Y_CONFIGDEF:
		axis = (char)(GetAnalogStickValue(ci, XINPUT_GAMEPAD_RS_Y_CONFIGDEF) * (float)(0x7F)) + 0x80;
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
	int ci = 0;
	if ((int)Joystick == JOY2_ADDR)
		ci = 1;

	BYTE axis = 0;
	char signed_axis = 0;

	switch (inScannerConfig->BitmaskStuff)
	{
	case XINPUT_GAMEPAD_LS_X_CONFIGDEF:
		axis = (BYTE)(GetAnalogStickValue(ci, XINPUT_GAMEPAD_LS_X_CONFIGDEF) * (float)(0x7F));
		break;
	case XINPUT_GAMEPAD_RS_X_CONFIGDEF:
		axis = (BYTE)(GetAnalogStickValue(ci, XINPUT_GAMEPAD_RS_X_CONFIGDEF) * (float)(0x7F));
		break;
	case XINPUT_GAMEPAD_LS_Y_CONFIGDEF:
		axis = (BYTE)(GetAnalogStickValue(ci, XINPUT_GAMEPAD_LS_Y_CONFIGDEF) * (float)(0x7F));
		break;
	case XINPUT_GAMEPAD_RS_Y_CONFIGDEF:
		axis = (BYTE)(GetAnalogStickValue(ci, XINPUT_GAMEPAD_RS_Y_CONFIGDEF) * (float)(0x7F));
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

	// get KB digital steering
	switch (KeyboardReadingMode)
	{
	case KB_READINGMODE_UNBUFFERED_RAW:
		if (SteerLeftVKey && VKeyStates[JoystickPort][SteerLeftVKey])
			DigitalSteerValueKB = DigitalSteerValueKB + 1.0;
		if (SteerRightVKey && VKeyStates[JoystickPort][SteerRightVKey])
			DigitalSteerValueKB = DigitalSteerValueKB - 1.0;
		break;
	case KB_READINGMODE_BUFFERED:
		if (SteerLeftVKey && VKeyStates[0][SteerLeftVKey] >> 7)
			DigitalSteerValueKB = DigitalSteerValueKB + 1.0;
		if (SteerRightVKey && VKeyStates[0][SteerRightVKey] >> 7)
			DigitalSteerValueKB = DigitalSteerValueKB - 1.0;
		break;
	case KB_READINGMODE_UNBUFFERED_ASYNC:
	default:
		if (SteerLeftVKey && GetAsyncKeyState(SteerLeftVKey) >> 15)
			DigitalSteerValueKB = DigitalSteerValueKB + 1.0;
		if (SteerRightVKey && GetAsyncKeyState(SteerRightVKey) >> 15)
			DigitalSteerValueKB = DigitalSteerValueKB - 1.0;
		break;
	}

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


#else
bool bCheckSecondBind() { return FE_Secondary_Up || FE_Secondary_Down || FE_Secondary_Left || FE_Secondary_Right; }
bool bGetAnalogDigitalActivationState(int port, WORD binding, int action) {
  int threshold = action == GAMEACTION_SHIFTUP || action == GAMEACTION_SHIFTDOWN ? SHIFT_ANALOG_THRESHOLD : FEUPDOWN_ANALOG_THRESHOLD;
  return InputAnalogPressed(g_Controllers[port].state.Gamepad, binding, threshold, TRIGGER_ACTIVATION_THRESHOLD);
}
class InputDevice {
  long Padding1;
  long FeedbackHandle;
  long UnkHandle;
  long UnkHandle2;
  long Padding2;

 public:
  DeviceScalar* fDeviceScalar;
  float*        fPrevValues;
  float*        fCurrentValues;
  int           fDeviceIndex;
  float         fControllerCurve;
#ifdef HAS_RUMBLE
  IFeedback mFFB;  // +0x2C, same as the game's own device
#endif
#ifdef GAME_PROSTREET
  bool     bRemappingMode;
  uint32_t RemapType;
  uint32_t RemapIndex;
  uint32_t RemapIsPrimary;
#endif

  InputDevice(int DeviceIndex) {
    memset(&Padding1, 0, sizeof(long) * 5);
    UTL_Com_Object_IList_Constructor(&Padding1, 4);
    fDeviceIndex     = DeviceIndex;
    fControllerCurve = 1.0f;
    fPrevValues      = PrevValues[DeviceIndex];
    fCurrentValues   = CurrValues[DeviceIndex];
    fDeviceScalar    = new (std::nothrow) DeviceScalar[MAX_ACTIONID];
#ifdef HAS_RUMBLE
    mFFB.Init(&Padding1, DeviceIndex);
    if (bRumbleEnabled) UTL_Com_Object_IList_Add(&Padding1, (void*)IFEEDBACK_IHANDLE_ADDR, &mFFB);
#endif
#ifdef GAME_PROSTREET
    bRemappingMode = false;
    RemapType      = 0;
    RemapIndex     = 0;
    RemapIsPrimary = 0;
#endif
#ifndef NO_PROFILE_SETTINGS
    InitProfileSettings();
#endif
  }
  virtual void* dtor(bool something) {
#ifdef HAS_RUMBLE
    mFFB.Detach();
#endif
    delete (this);
    return this;
  }
  virtual bool IsConnected() {
    if ((LastControlledDevice == LASTCONTROLLED_CONTROLLER) && bPassConnStatus) return g_Controllers[fDeviceIndex].bConnected;
    return true;
  }
  virtual bool IsWheel() { return false; }
  virtual void Initialize() {

    for (unsigned int i = 0; i < MAX_ACTIONID; i++) {
      if (bIsActionDigitalButton((ActionID)i))
        fDeviceScalar[i].fType = kDigitalButton;
      else
        fDeviceScalar[i].fType = kJoyAxis;

      fDeviceScalar[i].fName_CRC32   = stringhash32(ActionIDStr[i]);
      fDeviceScalar[i].fPrevValue    = &PrevValues[fDeviceIndex][i];
      fDeviceScalar[i].fCurrentValue = &CurrValues[fDeviceIndex][i];
    }
#if defined(GAME_UC) || defined(GAME_WORLD)
    LoadMapping(NFSProfileName);
#else
    LoadMapping("");
#endif
  }
  virtual void PollDevice() {
#ifdef HAS_CONSOLE_RUMBLE
    mFFB.PollEffects();
#elif defined(GAME_PROSTREET)
    mFFB.Watchdog();
#endif
    if (!bGlobalDoPolling) return;
    if (bBlockThisPoll) { bBlockThisPoll = false; return; }

    ReadControllerData();
    WORD wButtons                    = g_Controllers[fDeviceIndex].state.Gamepad.wButtons;
    WORD SecondBind                  = 0;
    int  rdi                         = fDeviceIndex;
    bool bCurrentXInputKeyState      = false;
    bool bCurrentXInputKeyState2     = false;
    bool bCurrentVKeyState           = false;
    bool bCurrentVKeyState_SECONDARY = false;
    bool bDoPolling                  = true;

#ifdef GAME_MW
    bool bInShowcase = false;
    if (*(int*)GAMEFLOWMANAGER_STATUS_ADDR == 3) {
      if (cFEng_FindPackage(FE_SHOWCASE_FNG_NAME) || cFEng_FindPackage("UI_DebugCarCustomize.fng") || *(bool*)CARGUYSCAMERA_ADDR) bInShowcase = true;
    }
#endif
#ifdef GAME_PROSTREET
    if (bRemappingMode) {
      if (RemapType == 1) {
        if (HandleKeyboardRemap(RemapIndex, RemapIsPrimary)) {
          g_bRemapClear           = false;
          g_bRemapClear_IsPrimary = std::numeric_limits<std::uint32_t>::max();
          g_iRemapClear_BtnIndex  = std::numeric_limits<std::uint32_t>::max();
          bRemappingMode          = false;
        }
      }

      if (RemapType == 3) {
        if (HandleGamepadRemap(RemapIndex, RemapIsPrimary, fDeviceIndex)) {
          g_bRemapClear           = false;
          g_bRemapClear_IsPrimary = std::numeric_limits<std::uint32_t>::max();
          g_iRemapClear_BtnIndex  = std::numeric_limits<std::uint32_t>::max();
          bRemappingMode          = false;
        }
      }
    }
#endif
    for (unsigned int j = 0; j < MAX_ACTIONID; j++) {
      ActionID i = (ActionID)j;

#ifdef GAME_UC
      if (bIsActionFrontEnd((ActionID)i) && bIsHudVisible()) bDoPolling = false;
#else
      if (bIsActionFrontEnd((ActionID)i) && bIsHudVisible() && (i != FRONTENDACTION_START)) bDoPolling = false;
#endif

#ifdef GAME_WORLD
      // I *truly* do not understand this game. So, the inputs will only start working if a button was pressed BEFORE entering the game.
      // So I'll set an action to a high state while in FE. FE isn't even controlled by the controller, so it should be safe.
      if (*(int*)GAMEFLOWMANAGER_STATUS_ADDR != 6) {
        CurrValues[fDeviceIndex][i] = 1.0;
        bDoPolling                  = false;
      }
#endif

      if (bDoPolling) {
        float fresult = 0;
        rdi           = fDeviceIndex;
        bCurrentVKeyState = InputKeyPressed(0, VKeyBindings_PRIMARY[i]);
        bCurrentVKeyState_SECONDARY = InputKeyPressed(0, VKeyBindings_SECONDARY[i]);

        bCurrentVKeyState |= bCurrentVKeyState_SECONDARY;

        if (bIsActionDebug((ActionID)i)) {
          rdi = 1;  // debug (camera) actions are always read from the second port, but the PC version omits it entirely so this is a workaround
        }

        wButtons = g_Controllers[rdi].state.Gamepad.wButtons;

        if (bIsActionAnalog((ActionID)i)) {
          if (bIsBindingAnalog(XInputBindings_PRIMARY[i])) {
            fresult = GetAnalogStickValue(rdi, XInputBindings_PRIMARY[i]);
          } else {
            bCurrentXInputKeyState = wButtons & XInputBindings_PRIMARY[i];
            // keyboard & digital buttons
            if (bCurrentXInputKeyState) fresult = 1.0f;
          }

          if (bIsBindingAnalog(XInputBindings_SECONDARY[i])) {
            if (fresult == 0.0f) fresult = GetAnalogStickValue(rdi, XInputBindings_SECONDARY[i]);
          } else {
            if (!bCurrentXInputKeyState) {
              bCurrentXInputKeyState = wButtons & XInputBindings_SECONDARY[i];
              // keyboard & digital buttons
              if (bCurrentXInputKeyState) fresult = 1.0f;
            }
          }

          if (bCurrentVKeyState) fresult = 1.0f;
        } else {
          if (bCheckSecondBind() && bIsActionFrontEndNavigation((ActionID)i)) {
            switch (i) {
              case FRONTENDACTION_UP:
                SecondBind = FE_Secondary_Up;
                break;
              case FRONTENDACTION_DOWN:
                SecondBind = FE_Secondary_Down;
                break;
              case FRONTENDACTION_LEFT:
                SecondBind = FE_Secondary_Left;
                break;
              case FRONTENDACTION_RIGHT:
                SecondBind = FE_Secondary_Right;
                break;
              default:
                SecondBind = 0;
                break;
            }

            if (bIsBindingAnalog(SecondBind))  // digital directions with an axis
            {
              bCurrentXInputKeyState2 = bGetAnalogDigitalActivationState(rdi, SecondBind, i);
            }

            else
              bCurrentXInputKeyState2 = wButtons & SecondBind;
          } else {
            SecondBind              = 0;
            bCurrentXInputKeyState2 = false;
          }

          if (bIsBindingAnalog(XInputBindings_PRIMARY[i]) || bIsBindingAnalog(XInputBindings_SECONDARY[i]))  // digital directions with an axis
          {
            bCurrentXInputKeyState = bGetAnalogDigitalActivationState(rdi, XInputBindings_PRIMARY[i], i);
            if (!bCurrentXInputKeyState) bCurrentXInputKeyState = bGetAnalogDigitalActivationState(rdi, XInputBindings_SECONDARY[i], i);
            if (bCurrentXInputKeyState || bCurrentXInputKeyState2) fresult = 1.0f;
          } else {
            bCurrentXInputKeyState = wButtons & XInputBindings_PRIMARY[i];
            if (!bCurrentXInputKeyState) bCurrentXInputKeyState = wButtons & XInputBindings_SECONDARY[i];
            // determine final digital status
            if (bCurrentXInputKeyState || bCurrentXInputKeyState2) fresult = 1.0f;
          }
          if (bCurrentVKeyState) fresult = 1.0f;
        }

        // write value to the main array
        CurrValues[fDeviceIndex][i] = fresult;

        // update prev values
        if (CurrValues[fDeviceIndex][i] != PrevValues[fDeviceIndex][i]) {
          // do one-time triggers here...
#ifdef GAME_MW
          if (bIsActionRelativePosition((ActionID)i) && bInShowcase) {
            if (CurrValues[fDeviceIndex][i] > 0)
              CurrValues[fDeviceIndex][i] = CurrValues[fDeviceIndex][i] - PrevValues[fDeviceIndex][i];
            else
              CurrValues[fDeviceIndex][i] = 0;
          }
#endif
          PrevValues[fDeviceIndex][i] = CurrValues[fDeviceIndex][i];
        } else if (bIsActionDigitalButton((ActionID)i)) {
          if (CurrValues[fDeviceIndex][i] > 0) CurrValues[fDeviceIndex][i] = 0;
        }
      }
      bDoPolling = true;
    }
  }
  virtual int  GetNumDeviceScalar() { return MAX_ACTIONID; }
#if defined(GAME_CARBON) || defined(GAME_PROSTREET)
  // Native slot 6 is a controller-capability query. Start/Stop are 7/8;
  // GetInterfaces must be slot 9 (+0x24), as LocalPlayer::SetControllerPort expects.
  virtual bool PSControllerCapability() { return true; }
#endif
  virtual void StartVibration() {
  }
  virtual void StopVibration() {
#ifdef HAS_CONSOLE_RUMBLE
    if (bRumbleEnabled) mFFB.Send(0, 0);
#elif defined(GAME_PROSTREET)
    if (bRumbleEnabled) mFFB.PauseEffects();
#endif
    // this seems to be only called when the controller is unplugged...
  }
#ifdef HAS_RUMBLE
  virtual int GetInterfaces() { return bRumbleEnabled ? (int)&mFFB : 0; }
#else
  virtual int GetInterfaces() { return 0; }
#endif
  virtual int GetSecondaryDevice() { return 0; }
#ifndef GAME_MW
#if defined(GAME_CARBON) || defined(GAME_PROSTREET)
  virtual int GetBatteryLife(unsigned int /*query*/) { return 0; }
#else
  virtual int  GetBatteryLife() { return 0; }
#endif
  virtual bool IsBatteryLow() { return false; }
  virtual int  GetControllerTypeMask() { return 0; }
#endif
  virtual bool DeviceHasChanged() { return false; }
  virtual bool DeviceHasAnyActivity() { return false; }
  virtual void* SaveCurrentState() { return NULL; }
  virtual void  RestoreToState(float* unk) { return; }
#ifndef GAME_MW
#if defined(GAME_CARBON) || defined(GAME_PROSTREET)
  virtual float GetDeviceNoiseAllowance() { return 0.0f; }
#else
  virtual int GetDeviceNoiseAllowance() { return 0; }
#endif
#endif
#ifdef GAME_PROSTREET
  virtual void ReservedPS19(unsigned int) {}
  virtual void GoRemapButton(uint32_t type, uint32_t button, uint32_t isPrimary, uint32_t unk) {
    bRemappingMode = true;
    RemapType      = type;
    RemapIndex     = button;
    RemapIsPrimary = isPrimary;
  }
#endif
};
#ifdef HAS_RUMBLE
static_assert(offsetof(InputDevice, mFFB) == 0x2C, "IFeedback must sit at the same offset as in the game's device");
#endif

InputDevice* InputDeviceFactory(int DeviceIndex) {
  InputDevice* out = new InputDevice(DeviceIndex);

  return out;
}


#endif
