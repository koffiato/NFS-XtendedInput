#pragma once

#if INPUT_SCANNERS
// steering axis & buttons
WORD SteerLeftButton = 0; // defaulting digital steer to 0 because of predefined functions
WORD SteerRightButton = 0;
WORD SteerLeftAxis = XINPUT_GAMEPAD_LS_LEFT_CONFIGDEF;
WORD SteerRightAxis = XINPUT_GAMEPAD_LS_RIGHT_CONFIGDEF;

USHORT SteerLeftVKey = VK_LEFT;
USHORT SteerRightVKey = VK_RIGHT;

// Input ScannerConfig - JoyEvent mapping
struct ScannerConfig
{
	unsigned int JoyConfigType = 0xFF; // 0xFF default
	unsigned int unk1; // not read
	unsigned int JoyEvent;
	unsigned int ScannerFunctionPointer;
	uint16_t BitmaskStuff; // used to define which bits are read from the joypad buffer
	uint16_t BitmaskStuff2; // used to define which bits are read from the joypad buffer
	unsigned int unk5; // if prev value was 0x10000 = button texture hash, if 0x10001 = max value, hard to determine
	unsigned int param;
	unsigned int keycode; // normally unknown, we're reutilizing this value
}ScannerConfigs[MAX_JOY_EVENT]; // we're generating scanners for ALL JoyEvents - not something devs normally do

// we use this to track the state of each button for each event
WORD EventStates[2][MAX_JOY_EVENT];
bool EventStatesKB[2][MAX_JOY_EVENT];

int Scanner_DigitalUpOrDown_XInput(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick);
int Scanner_DigitalUpOrDown_KB(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick);
int Scanner_DigitalUpOrDown(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick);
int Scanner_DigitalDown_XInput(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick);
int Scanner_DigitalDown_KB(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick);
int Scanner_DigitalDown(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick);
int Scanner_DigitalAnalog_XInput(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick);
int Scanner_DigitalDownAnalog_XInput(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick);
int Scanner_DigitalAnalog(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick);
int Scanner_DigitalDownAnalog(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick);
int Scanner_Analog(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick);
int Scanner_Analog_DragSteer(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick);
int Scanner_CombinedSteering(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick);
int Scanner_TypeChanged(void* EventNode, unsigned int* unk1, unsigned int unk2, ScannerConfig* inScannerConfig, void* Joystick);
void SetupScannerConfig();

#include "../rumble/Underground.h"
#else
// global var for the action ID values
float PrevValues[2][MAX_ACTIONID];
float CurrValues[2][MAX_ACTIONID];
// button bindings per action ID
WORD XInputBindings_PRIMARY[MAX_ACTIONID];
WORD XInputBindings_SECONDARY[MAX_ACTIONID];
// secondary bindings, exclusively for FE navigation with left stick as well (not just D-Pad)
WORD FE_Secondary_Up;
WORD FE_Secondary_Down;
WORD FE_Secondary_Left;
WORD FE_Secondary_Right;

BYTE VKeyBindings_PRIMARY[MAX_ACTIONID];
BYTE VKeyBindings_SECONDARY[MAX_ACTIONID];


bool bEnteredWorldMapOnce = false;
#ifdef GAME_PROSTREET
bool     g_bRemapClear           = false;
uint32_t g_bRemapClear_IsPrimary = std::numeric_limits<std::uint32_t>::max();
uint32_t g_iRemapClear_BtnIndex  = std::numeric_limits<std::uint32_t>::max();
#endif

enum DeviceScalarType {
  kJoyAxis       = 0,
  kAnalogButton  = 1,
  kDigitalButton = 2,
};

struct DeviceScalarInfo {
  const char*      name;
  DeviceScalarType type;
  int              system_index;
  int              space[10];  // space for multiple binds?
};

class DeviceScalar {
 public:
  DeviceScalarType fType;
  unsigned int     fName_CRC32;
  float*           fPrevValue;
  float*           fCurrentValue;
  DeviceScalar() {
    fType         = (DeviceScalarType)0;
    fName_CRC32   = 0;
    fPrevValue    = NULL;
    fCurrentValue = NULL;
  }
};

enum ShiftPotential {
  SHIFT_POTENTIAL_NONE    = 0,
  SHIFT_POTENTIAL_DOWN    = 1,
  SHIFT_POTENTIAL_UP      = 2,
  SHIFT_POTENTIAL_GOOD    = 3,
  SHIFT_POTENTIAL_PERFECT = 4,
  SHIFT_POTENTIAL_MISS    = 5,
};

#ifdef HAS_RUMBLE
#include "../rumble/Feedback.h"
#endif

enum InputUpdateType {
  kUpdate        = 0,
  kPress         = 1,
  kRelease       = 2,
  kAnalogPress   = 3,
  kAnalogRelease = 4,
  kCenterControl = 5,
};

struct InputMapEntry {
  InputUpdateType UpdateType;
  float           LowerDZ;
  float           UpperDZ;
  ActionID        Action;
  int             DeviceScalarIndex;
#ifndef GAME_MW
  int ComboDeviceScalarIndex;
#endif
  float PreviousValue;
  float CurrentValue;
};


#endif
