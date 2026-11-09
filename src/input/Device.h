// Later-game input device and action polling. MIT; see LICENSE.
#pragma once

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
  virtual void* dtor(bool free_memory) {
#ifdef HAS_RUMBLE
    mFFB.Detach();
#endif
    if (fDeviceScalar) {
      delete[] fDeviceScalar;
      fDeviceScalar = nullptr;
    }
    if (free_memory) {
      ::operator delete(this);
    }
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
#ifdef HAS_RUMBLE
    mFFB.PollEffects();
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
        bool bCheckKeyboard = (fDeviceIndex == 0) || (fDeviceIndex == 1 && bAllowTwoPlayerKB && KeyboardReadingMode == KB_READINGMODE_UNBUFFERED_RAW);
        if (bCheckKeyboard) {
          bCurrentVKeyState = InputKeyPressed(fDeviceIndex, VKeyBindings_PRIMARY[i]);
          bCurrentVKeyState_SECONDARY = InputKeyPressed(fDeviceIndex, VKeyBindings_SECONDARY[i]);
          bCurrentVKeyState |= bCurrentVKeyState_SECONDARY;
        } else {
          bCurrentVKeyState = false;
        }

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
#if defined(GAME_CARBON) || defined(GAME_PROSTREET) || defined(GAME_UC)
  // Native slot 6 is a controller-capability query. Start/Stop are 7/8;
  // GetInterfaces must be slot 9 (+0x24), as LocalPlayer::SetControllerPort expects.
  virtual bool PSControllerCapability() { return true; }
#endif
  virtual void StartVibration() {
  }
  virtual void StopVibration() {
#ifdef HAS_RUMBLE
    if (bRumbleEnabled) mFFB.Send(0, 0);
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
#if defined(GAME_CARBON) || defined(GAME_PROSTREET) || defined(GAME_UC)
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
#if defined(GAME_CARBON) || defined(GAME_PROSTREET) || defined(GAME_UC)
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
