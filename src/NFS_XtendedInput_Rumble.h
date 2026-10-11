/*
// clang-format off
//
//    MIT License
//    Need for Speed (Black Box, MW & newer) - Xtended Input plugin
//    Bringing native XInput to NFS
//
// clang-format on
*/

// Rumble (MW only, for now)
//
// The game asks the input device for its IFeedback interface (InputDevice::GetInterfaces) and then feeds it
// the console force feedback events every sim tick (LocalPlayer::DoFFB @ 0x6EE490) plus collisions
// (LocalPlayer::OnCollision). PC ships those methods as empty stubs, so we provide our own implementation
// and drive the XInput motors from it.
//
// The game never stops effects on focus loss and keeps a raw pointer to the interface, so:
// - the object is embedded in InputDevice (same place as the game's own device, +0x2C) and is never deleted
// - motors are zeroed on pause, focus loss, disconnect, missed ticks (watchdog) and DLL unload

#pragma once

#include <roapi.h>
#include <windows.gaming.input.h>

enum RumbleCategory {
  kRumbleImpacts,  // hits, scrapes, spike strips, blown tyres
  kRumbleSurface,  // kerbs, bumps, landings, road texture
  kRumbleTyres,    // lockup, wheelspin, slides
  kRumbleCar,      // NOS, shifts, engine
  kRumbleCategoryCount,
};

// [Rumble] Enabled is the only switch: off means no feedback interface, no patches, no per-tick work
bool  bRumbleEnabled                             = true;
bool  bRumbleSwapMotors                          = false;
float fRumbleIntensity                           = 1.0f;
float fRumbleLeftScale                           = 1.0f;  // low frequency (heavy) motor
float fRumbleRightScale                          = 1.0f;  // high frequency (light) motor
float fRumbleCategoryScale[kRumbleCategoryCount] = {1.0f, 1.0f, 1.0f, 1.0f};

// ducking: textures drop to this level while an impact pulse is playing
const float RUMBLE_DUCK_LEVEL     = 0.3f;
const float RUMBLE_DUCK_THRESHOLD = 0.05f;
// fatigue fade: a texture running longer than FATIGUE_START seconds fades to FATIGUE_FLOOR over FATIGUE_RAMP seconds,
// and recovers once it has been quiet for FATIGUE_RESET seconds. Pulses never fade.
const float RUMBLE_FATIGUE_START = 3.0f;
const float RUMBLE_FATIGUE_RAMP  = 5.0f;
const float RUMBLE_FATIGUE_FLOOR = 0.6f;
const float RUMBLE_FATIGUE_RESET = 2.5f;

// pad that last produced input (used in omni mode, where both game ports read every pad)
int  RumbleLastActivePad = 0;
bool bRumbleAppActive    = true;
// bumped whenever all pads are force-stopped, so every feedback object resyncs its motor state
unsigned int RumbleStopGeneration = 0;

void(__thiscall* UTL_Com_Object_IList_Add)(void* thethis, void* handle, void* iface) = (void(__thiscall*)(void*, void*, void*))UTL_ILIST_ADD_ADDR;

double RumbleGetTime() {
  static LARGE_INTEGER freq = {0};
  if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return (double)now.QuadPart / (double)freq.QuadPart;
}

// the Xbox motors have 101 levels (0-100 %); round 0-65535 values to them so near-identical values don't cost a call
int RumbleRoundToPercent(int value) { return (value * 100 + 32767) / 65535 * 65535 / 100; }

// --- impulse triggers ([Rumble] ImpulseTriggers) ---
// Only Windows.Gaming.Input reaches the trigger motors, and it can't target one pad, so auto mode needs a single pad.
// Loaded at runtime so the Win7 builds still start.

enum RumbleTriggerMode {
  kTriggersOff,
  kTriggersOn,    // always, to every WGI gamepad
  kTriggersAuto,  // only while exactly one real Xbox pad (not Steam's virtual one) is connected
  kTriggerModeCount,
};

int   RumbleTriggerMode          = kTriggersAuto;
float fRumbleTriggerIntensity    = 1.0f;
bool  bRumbleTriggerPedalScaling = true;  // trigger effects grow with how far the pedal is pressed

namespace RumbleWGI {
typedef HRESULT(WINAPI* RoInitialize_t)(RO_INIT_TYPE);
typedef HRESULT(WINAPI* RoGetActivationFactory_t)(HSTRING, REFIID, void**);
typedef HRESULT(WINAPI* WindowsCreateStringReference_t)(PCWSTR, UINT32, HSTRING_HEADER*, HSTRING*);

using ABI::Windows::Gaming::Input::GamepadVibration;
using ABI::Windows::Gaming::Input::IGameController;
using ABI::Windows::Gaming::Input::IGamepad;
using ABI::Windows::Gaming::Input::IGamepadStatics;
using ABI::Windows::Gaming::Input::IRawGameController;
using ABI::Windows::Gaming::Input::IRawGameControllerStatics;
typedef ABI::Windows::Foundation::Collections::IVectorView<ABI::Windows::Gaming::Input::Gamepad*> GamepadList;

const UINT16 STEAM_VIRTUAL_VID = 0x28DE;  // Valve

bool                       bTried      = false;
IGamepadStatics*           pStatics    = NULL;
IRawGameControllerStatics* pRawStatics = NULL;  // Windows 10 1607+, only used to spot Steam's virtual pad

const int MAX_PADS = 8;
IGamepad* Pads[MAX_PADS];  // cached, the list is refreshed once a second
int       PadCount    = 0;
bool      bSteamPad   = false;  // one of the cached pads is Steam Input's virtual controller
double    PadListTime = -1.0;

HRESULT GetFactory(WindowsCreateStringReference_t createStringRef, RoGetActivationFactory_t roGetFactory, const wchar_t* className, UINT32 length,
                   REFIID iid, void** factory) {
  HSTRING_HEADER header;
  HSTRING        name;
  HRESULT        hr = createStringRef(className, length, &header, &name);
  if (FAILED(hr)) return hr;
  return roGetFactory(name, iid, factory);
}

bool Init() {
  if (bTried) return pStatics != NULL;
  bTried          = true;
  HMODULE combase = LoadLibraryExA("combase.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!combase) return false;
  RoInitialize_t                 roInitialize    = (RoInitialize_t)GetProcAddress(combase, "RoInitialize");
  RoGetActivationFactory_t       roGetFactory    = (RoGetActivationFactory_t)GetProcAddress(combase, "RoGetActivationFactory");
  WindowsCreateStringReference_t createStringRef = (WindowsCreateStringReference_t)GetProcAddress(combase, "WindowsCreateStringReference");
  if (!roInitialize || !roGetFactory || !createStringRef) return false;

  // like SDL: prefer a single-threaded apartment (what the game's own COM users expect); if the thread is already in
  // the multithreaded one that fails with RPC_E_CHANGED_MODE and the existing apartment is used
  if (roInitialize(RO_INIT_SINGLETHREADED) == RPC_E_CHANGED_MODE) roInitialize(RO_INIT_MULTITHREADED);
  static const wchar_t gamepadClass[] = L"Windows.Gaming.Input.Gamepad";
  static const wchar_t rawClass[]     = L"Windows.Gaming.Input.RawGameController";
  if (FAILED(GetFactory(createStringRef, roGetFactory, gamepadClass, _countof(gamepadClass) - 1, __uuidof(IGamepadStatics), (void**)&pStatics)))
    pStatics = NULL;
  if (FAILED(GetFactory(createStringRef, roGetFactory, rawClass, _countof(rawClass) - 1, __uuidof(IRawGameControllerStatics), (void**)&pRawStatics)))
    pRawStatics = NULL;
  return pStatics != NULL;
}

// Steam Input's virtual controller carries Valve's vendor id on the raw controller (SDL checks the same)
bool IsSteamVirtualPad(IGamepad* pad) {
  if (!pRawStatics) return false;
  bool             steam = false;
  IGameController* ctrl  = NULL;
  if (SUCCEEDED(pad->QueryInterface(__uuidof(IGameController), (void**)&ctrl)) && ctrl) {
    IRawGameController* raw = NULL;
    if (SUCCEEDED(pRawStatics->FromGameController(ctrl, &raw)) && raw) {
      UINT16 vid = 0;
      if (SUCCEEDED(raw->get_HardwareVendorId(&vid))) steam = vid == STEAM_VIRTUAL_VID;
      raw->Release();
    }
    ctrl->Release();
  }
  return steam;
}

void RefreshPads() {
  for (int i = 0; i < PadCount; i++) Pads[i]->Release();
  PadCount          = 0;
  bSteamPad         = false;
  GamepadList* list = NULL;
  if (FAILED(pStatics->get_Gamepads(&list)) || !list) return;
  unsigned int count = 0;
  list->get_Size(&count);
  for (unsigned int i = 0; i < count && PadCount < MAX_PADS; i++) {
    IGamepad* pad = NULL;
    if (SUCCEEDED(list->GetAt(i, &pad)) && pad) {
      if (IsSteamVirtualPad(pad)) bSteamPad = true;
      Pads[PadCount++] = pad;
    }
  }
  list->Release();
}

// whether output goes through WGI (main motors and triggers) instead of XInput; re-checks the pads once a second
bool ShouldUse() {
  if (RumbleTriggerMode == kTriggersOff || !Init()) return false;
  double now = RumbleGetTime();
  if (PadListTime < 0.0 || now - PadListTime > 1.0) {
    PadListTime = now;
    RefreshPads();
  }
  if (RumbleTriggerMode == kTriggersAuto) return PadCount == 1 && !bSteamPad;
  return PadCount > 0;
}

void Send(double leftMotor, double rightMotor, double leftTrigger, double rightTrigger) {
  GamepadVibration vib = {leftMotor, rightMotor, leftTrigger, rightTrigger};
  for (int i = 0; i < PadCount; i++) Pads[i]->put_Vibration(vib);
}
}  // namespace RumbleWGI

// fromDllMain: no WinRT calls under the loader lock (the gamepads stop with the process anyway)
void RumbleStopAllPads(bool fromDllMain = false) {
  if (!bRumbleEnabled) return;  // we never started anything
  XINPUT_VIBRATION vib = {0, 0};
  for (DWORD i = 0; i < MAX_CONTROLLERS; i++) XInputSetState(i, &vib);
  if (!fromDllMain && RumbleWGI::pStatics) RumbleWGI::Send(0, 0, 0, 0);
  RumbleStopGeneration++;
}

void RumbleInstallPatches() {
  if (!bRumbleEnabled) return;

  // DoFFB skips UpdateShifting for cars with IEngineDamage (jumps at 0x6EE7F0 / 0x6EE7FF to 0x6EE836), which includes
  // player cars. Point both at the shifting code (0x6EE801); it only uses locals valid on both paths.
  if (*(uint16_t*)0x006EE7F0 == 0x44EB && *(uint16_t*)0x006EE7FF == 0x35EB) {
    injector::WriteMemory<uint8_t>(0x006EE7F1, 0x0F, true);
    injector::WriteMemory<uint8_t>(0x006EE800, 0x00, true);
  }

  // LocalPlayer::OnCollision skips ReportCollision when the save's hidden Rumble byte is 0 (`je` at 0x6F23EA).
  // The ini is the only switch, so drop that jump; the null-settings check before it stays.
  if (*(uint16_t*)0x006F23EA == 0x2B74) injector::WriteMemory<uint16_t>(0x006F23EA, 0x9090, true);
}

// --- player vehicle access ---

void*(__thiscall* UTL_Com_Object_IList_Find)(void* thethis, void* handle) = (void*(__thiscall*)(void*, void*))UTL_ILIST_FIND_ADDR;
const void*(__thiscall* Attrib_Instance_GetAttributePointer)(const void* thethis, unsigned int key, unsigned int index) =
    (const void*(__thiscall*)(const void*, unsigned int, unsigned int))ATTRIB_GETATTRIBPTR_ADDR;

template <typename R, typename... Args>
R RumbleVCall(void* obj, int offset, Args... args) {
  return (*(R(__thiscall**)(void*, Args...))(*(char**)obj + offset))(obj, args...);
}

// UTL::COM QueryInterface: look the handle up in the object's interface list (iface +4)
void* RumbleFindInterface(void* iface, unsigned int handle) { return UTL_Com_Object_IList_Find(*(void**)((char*)iface + 4), (void*)handle); }

const int RUMBLE_MAX_WHEELS = 4;

struct RumbleVehicleState {
  int   nWheels;
  float fSpeed;  // m/s
  float fGas;
  float fBrake;
  float fHandBrake;
  bool  bOnGround[RUMBLE_MAX_WHEELS];
  float fCompression[RUMBLE_MAX_WHEELS];  // m
  float fSlip[RUMBLE_MAX_WHEELS];         // m/s, signed: + wheelspin, - braking
  float fSkid[RUMBLE_MAX_WHEELS];         // m/s, lateral
  float fWheelAV[RUMBLE_MAX_WHEELS];      // rad/s
  float fLinVel[3];                       // world m/s
  float fAngVel[3];                       // world rad/s
};

// the local player's ISimable, or NULL outside a race / while the player list is empty or being rebuilt
void* RumbleGetPlayerSimable() {
  if (*(int*)GAMEFLOWMANAGER_STATUS_ADDR != 6) return NULL;
  void** players = *(void***)PLAYER_ILISTABLE_ADDR;
  if (!players || *(int*)PLAYER_ILISTABLE_COUNT_ADDR <= 0 || !players[0]) return NULL;
  return RumbleVCall<void*>(players[0], 0x04);  // IPlayer::GetSimable
}

bool RumbleReadVehicleUnsafe(RumbleVehicleState& s) {
  void* simable = RumbleGetPlayerSimable();
  if (!simable) return false;

  void* susp  = RumbleFindInterface(simable, ISUSPENSION_IHANDLE_ADDR);
  void* body  = RumbleFindInterface(simable, IRIGIDBODY_IHANDLE_ADDR);
  void* veh   = RumbleFindInterface(simable, IVEHICLE_IHANDLE_ADDR);
  void* input = RumbleFindInterface(simable, IINPUT_IHANDLE_ADDR);
  if (!susp || !body || !veh || !input) return false;

  s.fSpeed           = RumbleVCall<float>(veh, 0x98);  // IVehicle::GetAbsoluteSpeed
  const char* ctrl   = RumbleVCall<const char*>(input, 0x08);  // IInput::GetControls
  s.fGas             = *(const float*)(ctrl + 0x14);
  s.fBrake           = *(const float*)(ctrl + 0x18);
  s.fHandBrake       = *(const float*)(ctrl + 0x1C);
  const float* lin   = RumbleVCall<const float*>(body, 0x24);  // IRigidBody::GetLinearVelocity
  const float* ang   = RumbleVCall<const float*>(body, 0x28);  // IRigidBody::GetAngularVelocity
  memcpy(s.fLinVel, lin, sizeof(s.fLinVel));
  memcpy(s.fAngVel, ang, sizeof(s.fAngVel));

  s.nWheels = RumbleVCall<int>(susp, 0x08);
  if (s.nWheels > RUMBLE_MAX_WHEELS) s.nWheels = RUMBLE_MAX_WHEELS;
  for (unsigned int i = 0; i < (unsigned int)s.nWheels; i++) {
    s.bOnGround[i]    = RumbleVCall<bool>(susp, 0x24, i);
    s.fCompression[i] = RumbleVCall<float>(susp, 0x28, i);
    s.fSlip[i]        = RumbleVCall<float>(susp, 0x30, i);
    s.fSkid[i]        = RumbleVCall<float>(susp, 0x38, i);
    s.fWheelAV[i]     = RumbleVCall<float>(susp, 0x50, i);
  }
  return true;
}

// the vehicle path is reverse engineered, so a wrong guess must not take the game down
bool RumbleReadVehicle(RumbleVehicleState& s) {
  __try {
    return RumbleReadVehicleUnsafe(s);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool RumbleReadLinearVelocityUnsafe(float* velocity) {
  void* simable = RumbleGetPlayerSimable();
  if (!simable) return false;
  void* body = RumbleFindInterface(simable, IRIGIDBODY_IHANDLE_ADDR);
  if (!body) return false;
  memcpy(velocity, RumbleVCall<const float*>(body, 0x24), 3 * sizeof(float));  // IRigidBody::GetLinearVelocity
  return true;
}

// the player's current body velocity (world m/s), for the speed change across a collision
bool RumbleReadLinearVelocity(float* velocity) {
  __try {
    return RumbleReadLinearVelocityUnsafe(velocity);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

// simulation clock (Sim::GetTime), seconds; advances only when the physics steps
float RumbleGetSimTime() { return *(const float*)SIM_TIME_ADDR; }

// --- surfaces ---

const void*(__cdecl* Attrib_FindCollection)(unsigned int classKey, unsigned int collectionKey) =
    (const void*(__cdecl*)(unsigned int, unsigned int))ATTRIB_FINDCOLLECTION_ADDR;

const unsigned int ATTRIB_CLASS_SIMSURFACE   = 0xFB111FEF;
const unsigned int SIMSURFACE_KEY_BLOWN_TIRE = 0xD929E923;  // DoFFB swaps every wheel's surface for this one when spiked

const void* RumbleBlownTireCollection = NULL;

// whether a SimSurface (an Attrib::Instance, collection at +4) is the spiked / blown tyre surface
bool RumbleIsBlownTireSurface(const void* surface) {
  if (!RumbleBlownTireCollection) RumbleBlownTireCollection = Attrib_FindCollection(ATTRIB_CLASS_SIMSURFACE, SIMSURFACE_KEY_BLOWN_TIRE);
  return RumbleBlownTireCollection && *(const void* const*)((const char*)surface + 4) == RumbleBlownTireCollection;
}

// --- telemetry: [Rumble] Telemetry = 1 logs what the game feeds us to scripts\NFS_XtendedInput_Rumble.log ---

bool  bRumbleTelemetry     = false;
FILE* RumbleTelemetryFile  = NULL;
bool  bRumbleTelemetryDead = false;

const int   RUMBLE_MAX_SURFACES      = 128;
const void* RumbleSurfaces[RUMBLE_MAX_SURFACES];  // collection pointers, index = id in the log
int         RumbleSurfaceCount = 0;

void RumbleTelemetryPrintf(const char* fmt, ...) {
  if (!bRumbleTelemetry || bRumbleTelemetryDead) return;
  if (!RumbleTelemetryFile) {
    if (fopen_s(&RumbleTelemetryFile, "scripts\\NFS_XtendedInput_Rumble.log", "w") != 0 || !RumbleTelemetryFile) {
      bRumbleTelemetryDead = true;
      return;
    }
    fprintf(RumbleTelemetryFile, "# S,id,collection,name,renderNoise[freq amp minSpeed maxSpeed],wheelEffect[freq intensity],friction[6],FFB_ROLL/SKID/SLIP[4 x 8]\n");
    fprintf(RumbleTelemetryFile, "# T,time,speed,gas,brake,handbrake,powerband,throttle,nos,shifting,shiftPotential,{surf,ground,comp,slip,skid,av,fbSlip,fbSkid}x4,linVel[3],angVel[3]\n");
    fprintf(RumbleTelemetryFile, "# C,time,type,flags,closingVel,normalSpeed,slidingSpeed,speedChange,strength,measuredSpeedChange\n");
  }
  va_list args;
  va_start(args, fmt);
  vfprintf(RumbleTelemetryFile, fmt, args);
  va_end(args);
}

void RumbleTelemetryFlush() {
  if (RumbleTelemetryFile) fflush(RumbleTelemetryFile);
}

void RumbleTelemetryDumpFFBWave(const void* surface, unsigned int key) {
  for (unsigned int i = 0; i < 4; i++) {
    const float* w = (const float*)Attrib_Instance_GetAttributePointer(surface, key, i);
    if (w)
      RumbleTelemetryPrintf(",%g %g %g %g %g %g %g %g", w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
    else
      RumbleTelemetryPrintf(",-");
  }
}

void RumbleTelemetryDumpSurfaceUnsafe(int id, const void* surface) {
  const char* layout = *(const char* const*)((const char*)surface + 8);
  const char* name   = layout ? *(const char* const*)(layout + 0xDC) : NULL;
  RumbleTelemetryPrintf("S,%d,%p,%s", id, RumbleSurfaces[id], name ? name : "?");
  if (layout) {
    const float* rn = (const float*)(layout + 0xCC);
    const float* fr = (const float*)(layout + 0xE0);
    RumbleTelemetryPrintf(",%g %g %g %g,%u %u,%g %g %g %g %g %g", rn[0], rn[1], rn[2], rn[3], *(const unsigned short*)(layout + 0xF8),
                          *(const unsigned char*)(layout + 0xFA), fr[0], fr[1], fr[2], fr[3], fr[4], fr[5]);
  }
  RumbleTelemetryDumpFFBWave(surface, 0xBA1297DA);  // FFB_ROLL
  RumbleTelemetryDumpFFBWave(surface, 0x0C149044);  // FFB_SKID
  RumbleTelemetryDumpFFBWave(surface, 0x8FD11D27);  // FFB_SLIP
}

// returns the surface's id in the log, dumping its data the first time it is seen
int RumbleTelemetrySurfaceId(const void* surface) {
  const void* collection = *(const void* const*)((const char*)surface + 4);
  for (int i = 0; i < RumbleSurfaceCount; i++)
    if (RumbleSurfaces[i] == collection) return i;
  if (RumbleSurfaceCount >= RUMBLE_MAX_SURFACES) return -1;

  int id             = RumbleSurfaceCount++;
  RumbleSurfaces[id] = collection;
  __try {
    RumbleTelemetryDumpSurfaceUnsafe(id, surface);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    RumbleTelemetryPrintf(",<fault>");
  }
  RumbleTelemetryPrintf("\n");
  return id;
}

// what DoFFB fed us this tick, per wheel
struct RumbleTelemetryTick {
  int    nWheel;
  int    iSurface[RUMBLE_MAX_WHEELS];
  float  fSlip[RUMBLE_MAX_WHEELS];
  float  fSkid[RUMBLE_MAX_WHEELS];
  float  fPowerband;
  float  fThrottle;
  double fStartTime;
  double fLastFlushTime;
} RumbleTelemetry;

void RumbleTelemetryWriteTick(double now, const RumbleVehicleState* vehicle, bool nos, bool shifting, int shiftPotential) {
  if (!RumbleTelemetry.fStartTime) RumbleTelemetry.fStartTime = now;
  RumbleVehicleState v;
  memset(&v, 0, sizeof(v));
  bool hasVehicle = vehicle != NULL;
  if (hasVehicle) v = *vehicle;

  RumbleTelemetryPrintf("T,%.4f,%.2f,%.2f,%.2f,%.2f,%.3f,%.2f,%d,%d,%d", now - RumbleTelemetry.fStartTime, v.fSpeed, v.fGas, v.fBrake, v.fHandBrake,
                        RumbleTelemetry.fPowerband, RumbleTelemetry.fThrottle, nos, shifting, shiftPotential);
  for (int i = 0; i < RUMBLE_MAX_WHEELS; i++) {
    bool wheel = i < RumbleTelemetry.nWheel;
    RumbleTelemetryPrintf(",%d,%d,%.4f,%.2f,%.2f,%.1f,%.2f,%.2f", wheel ? RumbleTelemetry.iSurface[i] : -1, hasVehicle && v.bOnGround[i], v.fCompression[i],
                          v.fSlip[i], v.fSkid[i], v.fWheelAV[i], wheel ? RumbleTelemetry.fSlip[i] : 0.0f, wheel ? RumbleTelemetry.fSkid[i] : 0.0f);
  }
  RumbleTelemetryPrintf(",%.2f %.2f %.2f,%.3f %.3f %.3f%s\n", v.fLinVel[0], v.fLinVel[1], v.fLinVel[2], v.fAngVel[0], v.fAngVel[1], v.fAngVel[2],
                        hasVehicle ? "" : ",novehicle");

  if (now - RumbleTelemetry.fLastFlushTime > 1.0) {
    RumbleTelemetry.fLastFlushTime = now;
    RumbleTelemetryFlush();
  }
}

// gear shift feel presets ([Rumble] GearShift): heavy motor, light motor, right impulse trigger, length in seconds
enum RumbleShiftPreset {
  kShiftOff,
  kShiftLow,
  kShiftMedium,
  kShiftStrong,
  kShiftMax,  // everything at full power
  kShiftPresetCount,
};
struct RumbleShiftFeel {
  float fLeft;
  float fRight;
  float fTrigger;
  float fTime;
  float fFloor;  // where the knock has decayed to when it cuts off
};
const RumbleShiftFeel RumbleShiftFeels[kShiftPresetCount] = {
    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f},      // off
    {0.3f, 0.0f, 0.3f, 0.1f, 0.6f},      // low
    {0.55f, 0.15f, 0.6f, 0.12f, 0.6f},   // medium
    {0.9f, 0.45f, 0.9f, 0.16f, 0.6f},    // strong
    {1.0f, 1.0f, 1.0f, 0.22f, 0.8f},     // max
};
int RumbleShiftPreset = kShiftMedium;

// envelopes from the console Rumble.cpp (time in seconds, value 0-255)
struct RumbleEnvelopePoint {
  float fTime;
  float fValue;
};

const RumbleEnvelopePoint SmallRumblePoints[] = {{0.0f, 255.0f}, {0.05f, 200.0f}, {0.07f, 80.0f}, {0.8f, 0.0f}};
const RumbleEnvelopePoint BigRumblePoints[]   = {{0.0f, 255.0f}, {0.15f, 255.0f}, {0.2f, 120.0f}, {1.2f, 0.0f}};
// shift timing cues (not from the console code): Perfect = light double tap, Miss = rough buzz
const RumbleEnvelopePoint PerfectShiftPoints[] = {{0.0f, 255.0f}, {0.07f, 255.0f}, {0.07f, 0.0f}, {0.13f, 0.0f}, {0.13f, 255.0f}, {0.2f, 255.0f}, {0.2f, 0.0f}};
const RumbleEnvelopePoint MissShiftPoints[]    = {{0.0f, 255.0f}, {0.15f, 180.0f}, {0.15f, 0.0f}};
// the same cues on the right trigger: Perfect = one crisp tick, Miss = two
const RumbleEnvelopePoint TriggerPerfectPoints[] = {{0.0f, 255.0f}, {0.03f, 255.0f}, {0.03f, 0.0f}};
const RumbleEnvelopePoint TriggerMissPoints[]    = {{0.0f, 255.0f}, {0.025f, 255.0f}, {0.025f, 0.0f}, {0.085f, 0.0f}, {0.085f, 255.0f}, {0.11f, 255.0f}, {0.11f, 0.0f}};
// kerb / bump thump (not from the console code): one sharp knock per wheel, so crossing a kerb gives the natural front-rear double knock
const RumbleEnvelopePoint BumpPoints[] = {{0.0f, 255.0f}, {0.04f, 255.0f}, {0.12f, 0.0f}};

// PS2 surface wave record (FFBWaveRecord), one per channel: 0 left low, 1 left high, 2 right low, 3 right high.
// Between x_min and x_max the wave's frequency, amplitude and offset are lerped from the _min to the _max values.
struct RumbleWaveRecord {
  float fFrequencyMin, fAmplitudeMin, fOffsetMin, fXMin;
  float fFrequencyMax, fAmplitudeMax, fOffsetMax, fXMax;
};

// one surface wave effect (road roll or skid): each channel keeps the strongest wheel's parameters for the tick
struct RumbleWave {
  float fPhase[4];
  float fFrequency[4];
  float fAmplitude[4];
  float fOffset[4];

  void Begin() {
    for (int ch = 0; ch < 4; ch++) fFrequency[ch] = fAmplitude[ch] = fOffset[ch] = 0.0f;
  }

  // surface: the SimSurface passed by the game, key: FFB_ROLL / FFB_SKID / FFB_SLIP
  void Add(const void* surface, unsigned int key, float x) {
    // a non-array attribute returns the same record for every index: it only describes channel 0
    const void* first  = Attrib_Instance_GetAttributePointer(surface, key, 0);
    bool        single = first && Attrib_Instance_GetAttributePointer(surface, key, 1) == first;
    for (unsigned int ch = 0; ch < (single ? 1u : 4u); ch++) {
      const RumbleWaveRecord* w = (const RumbleWaveRecord*)(ch == 0 ? first : Attrib_Instance_GetAttributePointer(surface, key, ch));
      if (!w || x < w->fXMin || !(w->fXMin < w->fXMax)) continue;
      float t = (x - w->fXMin) / (w->fXMax - w->fXMin);
      if (t > 1.0f) t = 1.0f;
      float freq = w->fFrequencyMin + (w->fFrequencyMax - w->fFrequencyMin) * t;
      float amp  = w->fAmplitudeMin + (w->fAmplitudeMax - w->fAmplitudeMin) * t;
      float off  = w->fOffsetMin + (w->fOffsetMax - w->fOffsetMin) * t;
      if (freq > fFrequency[ch]) fFrequency[ch] = freq;
      if (amp > fAmplitude[ch]) fAmplitude[ch] = amp;
      if (off > fOffset[ch]) fOffset[ch] = off;
    }
  }

  // advances the phases and returns the left / right motor values (0-1)
  void Sample(float dt, float& left, float& right) {
    float v[4];
    for (int ch = 0; ch < 4; ch++) {
      fPhase[ch] += fFrequency[ch] * dt;
      fPhase[ch] -= floorf(fPhase[ch]);
      v[ch] = fOffset[ch] + fAmplitude[ch] * sinf(6.2831853f * fPhase[ch]);
      if (v[ch] < 0.0f) v[ch] = 0.0f;
    }
    left  = v[0] + v[1];
    right = v[2] + v[3];
    if (left > 1.0f) left = 1.0f;
    if (right > 1.0f) right = 1.0f;
  }
};

const unsigned int ATTRIB_KEY_FFB_ROLL = 0xBA1297DA;
const unsigned int ATTRIB_KEY_FFB_SKID = 0x0C149044;
const unsigned int ATTRIB_KEY_FFB_SLIP = 0x8FD11D27;

// measured with the Telemetry log (~120 ticks/s): normal roads stay under ~3 m/s of compression speed,
// mounting a kerb is a ~9 cm step in one tick (~11-12 m/s)
const float RUMBLE_BUMP_MIN_VELOCITY  = 4.0f;   // m/s of suspension compression speed
const float RUMBLE_BUMP_FULL_VELOCITY = 14.0f;
const float RUMBLE_BUMP_MIN_SPEED     = 2.0f;   // m/s car speed
const float RUMBLE_BUMP_MAX_SIM_STEP  = 0.1f;   // s; longer gaps (loading, restarts) aren't one physics step
const float RUMBLE_SURFACE_WAVE_SCALE = 0.4f;   // console wave data is authored for a single big motor
const float RUMBLE_SKID_WAVE_SCALE    = 0.35f;
const float RUMBLE_LOCKUP_MIN_SLIP    = 3.0f;   // m/s of braking slip
const float RUMBLE_LOCKUP_FULL_SLIP   = 15.0f;
const float RUMBLE_SPIN_MIN_SLIP      = 3.0f;   // m/s of wheelspin
const float RUMBLE_SPIN_FULL_SLIP     = 20.0f;
const float RUMBLE_SLIDE_MIN_SKID     = 2.0f;   // m/s of lateral tyre speed
const float RUMBLE_SLIDE_FULL_SKID    = 20.0f;
// measured collision speed change below this share of the contact-normal estimate means the body hadn't taken the hit yet
const float RUMBLE_COLLISION_MIN_MEASURED = 0.25f;

// PS2 InputEffectState ramp: starts at amplitude, falls linearly to the floor over its time, then cuts to 0
struct RumbleRamp {
  float fTime;
  float fMaxTime;
  float fAmplitude;
  float fFloor;

  void Start(float amplitude, float floor, float time) {
    fTime      = time;
    fMaxTime   = time;
    fAmplitude = amplitude;
    fFloor     = floor;
  }

  void Stop() { fTime = 0.0f; }

  void Advance(float dt) {
    if (fTime > 0.0f) fTime -= dt;
  }

  float GetValue() const {
    if (fTime <= 0.0f || fMaxTime <= 0.0f) return 0.0f;
    return fFloor + (fAmplitude - fFloor) * fTime / fMaxTime;
  }
};

struct RumblePulse {
  const RumbleEnvelopePoint* pPoints;
  int                        nPoints;
  float                      fTime;
  float                      fAmplitude;

  void Start(const RumbleEnvelopePoint* points, int count, float amplitude) {
    // don't let a weak hit cut off a stronger one that is still playing
    if (pPoints && GetValue() >= amplitude * (points[0].fValue / 255.0f)) return;
    pPoints    = points;
    nPoints    = count;
    fTime      = 0.0f;
    fAmplitude = amplitude;
  }

  void Advance(float dt) {
    if (!pPoints) return;
    fTime += dt;
    if (fTime >= pPoints[nPoints - 1].fTime) pPoints = NULL;
  }

  // 0-1, linear interpolation between envelope points
  float GetValue() const {
    if (!pPoints) return 0.0f;
    int i = 0;
    while (i < nPoints - 1 && fTime >= pPoints[i + 1].fTime) i++;
    if (i >= nPoints - 1) return 0.0f;
    float t = (fTime - pPoints[i].fTime) / (pPoints[i + 1].fTime - pPoints[i].fTime);
    return fAmplitude * (pPoints[i].fValue + (pPoints[i + 1].fValue - pPoints[i].fValue) * t) / 255.0f;
  }
};

// running time of a texture, for the fatigue fade
struct RumbleTextureState {
  float fActiveTime;
  float fQuietTime;

  float Update(bool active, float dt) {
    if (active) {
      fActiveTime += dt;
      fQuietTime = 0.0f;
    } else {
      fQuietTime += dt;
      if (fQuietTime >= RUMBLE_FATIGUE_RESET) fActiveTime = 0.0f;
    }
    if (fActiveTime <= RUMBLE_FATIGUE_START) return 1.0f;
    float t = (fActiveTime - RUMBLE_FATIGUE_START) / RUMBLE_FATIGUE_RAMP;
    if (t > 1.0f) t = 1.0f;
    return 1.0f - (1.0f - RUMBLE_FATIGUE_FLOOR) * t;
  }
};

// collects every effect's motor values for one tick and resolves them to the final motor output
struct RumbleMixer {
  float fDt;
  float fPulse[2];
  float fTexture[2];
  bool  bImpactPlaying;

  void Begin(float dt) {
    fDt            = dt;
    fPulse[0]      = fPulse[1] = 0.0f;
    fTexture[0]    = fTexture[1] = 0.0f;
    bImpactPlaying = false;
  }

  void AddPulse(RumbleCategory category, float left, float right) {
    float scale = fRumbleCategoryScale[category];
    if (category == kRumbleImpacts && (left > RUMBLE_DUCK_THRESHOLD || right > RUMBLE_DUCK_THRESHOLD) && scale > 0.0f) bImpactPlaying = true;
    if (left * scale > fPulse[0]) fPulse[0] = left * scale;
    if (right * scale > fPulse[1]) fPulse[1] = right * scale;
  }

  void AddTexture(RumbleCategory category, RumbleTextureState& state, float left, float right) {
    float fatigue = state.Update(left > 0.0f || right > 0.0f, fDt);
    float scale   = fRumbleCategoryScale[category] * fatigue;
    if (left * scale > fTexture[0]) fTexture[0] = left * scale;
    if (right * scale > fTexture[1]) fTexture[1] = right * scale;
  }

  static int ToMotor(float value, float scale) {
    float v = value * fRumbleIntensity * scale;
    if (!(v > 0.0f)) return 0;  // also NaN
    if (v >= 1.0f) return 65535;
    return (int)(v * 65535.0f);
  }

  void Resolve(int& leftMotor, int& rightMotor) const {
    float duck  = bImpactPlaying ? RUMBLE_DUCK_LEVEL : 1.0f;
    float left  = fPulse[0] > fTexture[0] * duck ? fPulse[0] : fTexture[0] * duck;
    float right = fPulse[1] > fTexture[1] * duck ? fPulse[1] : fTexture[1] * duck;
    leftMotor   = ToMotor(left, fRumbleLeftScale);
    rightMotor  = ToMotor(right, fRumbleRightScale);
    if (bRumbleSwapMotors) {
      int tmp    = leftMotor;
      leftMotor  = rightMotor;
      rightMotor = tmp;
    }
  }
};

struct SimSurface;

// Sim::Collision::Info, 0x80 bytes
struct COLLISION_INFO {
  float       position[3];
  void*       objAsurface;
  float       normal[3];
  int         flags;  // type : 3, objAImmobile, objADetached, objBImmobile, objBDetached, sliding
  float       closingVel[3];
  float       force;
  float       armA[3];
  void*       objA;
  float       armB[3];
  void*       objB;
  float       objAVel[3];
  float       impulseA;
  float       objBVel[3];
  float       impulseB;
  float       slidingVel[3];
  void*       objBsurface;
};
static_assert(sizeof(COLLISION_INFO) == 0x80, "COLLISION_INFO size mismatch");

// vtable layout must match the game's IFeedback (15 slots, MW PC v1.3 vtable 0x8A7BD4)
// packed to 4 so the double member doesn't push the object off +0x2C inside InputDevice
#pragma pack(push, 4)
#pragma warning(push)
#pragma warning(disable : 4121)  // intentional, see above
class IFeedback {
 public:
  void* mOwnerList;  // the owning device's UTL::COM::Object::_IList (the game reads it at +4)

  int    mDeviceIndex;
  int    mPad;
  int    mLastLeft;
  int    mLastRight;
  bool   mPaused;
  bool   mNOSEngaged;
  double mLastUpdateTime;
  unsigned int mStopGeneration;
  RumblePulse  mPulseLeft;   // collisions
  RumblePulse  mPulseRight;  // collisions
  bool         mOnWGI;       // last output went through Windows.Gaming.Input (else XInput)
  RumbleTextureState mNOSTexture;
  RumbleMixer  mMixer;

  // inputs for this tick, reset in BeginUpdate (the game only calls some of these when they apply)
  bool           mShifting;
  bool           mEngineBlown;
  ShiftPotential mShiftPotential;
  float          mScrapeSpeed;  // m/s, from sliding collision contacts
  // previous tick, for edge detection; survives pauses so a held NOS doesn't kick again on resume
  bool           mPrevNOSEngaged;
  bool           mPrevShifting;
  bool           mPrevEngineBlown;
  ShiftPotential mPrevShiftPotential;

  RumbleRamp         mNOSKick;
  RumbleRamp         mShiftTick;
  RumbleRamp         mEngineBlownRamp;
  RumblePulse        mShiftCueLeft;
  RumblePulse        mShiftCueRight;
  float              mScrapeLevel;
  RumbleTextureState mScrapeTexture;

  // surface and tyres
  RumbleVehicleState mVehicle;
  bool               mHasVehicle;
  bool               mHasPrevVehicle;
  float              mPrevSimTime;  // sim clock at the previous suspension sample
  float              mPrevCompression[RUMBLE_MAX_WHEELS];
  bool               mPrevOnGround[RUMBLE_MAX_WHEELS];
  RumblePulse        mBumpLeft;
  RumblePulse        mBumpRight;
  RumbleWave         mRoadWave;
  RumbleWave         mSkidWave;
  RumbleWave         mBlownTireWave;  // the spiked surface's road wave, an Impacts effect
  RumblePulse        mLandingLeft;    // GROUND collisions, a Surface effect
  RumblePulse        mLandingRight;
  float              mWheelSkid[RUMBLE_MAX_WHEELS];  // from DoFFB, |lateral m/s|
  int                mWheelArg;                      // wheel DoFFB is reporting
  RumbleTextureState mRoadTexture;
  RumbleTextureState mBlownTireTexture;
  RumbleTextureState mSlideTexture;
  RumbleTextureState mLockupTexture;
  RumbleTextureState mSpinTexture;
  // impulse triggers (Forza layout: LT = braking slip, RT = throttle slip, plus the shift knock and cues on RT)
  float              mLockupLevel;  // 0-1, this tick
  float              mSpinLevel;
  float              mSlideLevel;
  float              mSpinPhase;
  RumblePulse        mTriggerCue;
  RumbleTextureState mTriggerLockupTexture;
  RumbleTextureState mTriggerSpinTexture;
  RumbleTextureState mTriggerSlideTexture;
  RumbleTextureState mTriggerNOSTexture;
  float              mLockupPhase;         // main motor stutter
  float              mTriggerLockupPhase;  // LT pulses

  // impulse triggers (0-65535 like the motors), last sent values
  int mLastTriggerLeft;
  int mLastTriggerRight;

  void Init(void* ownerList, int deviceIndex) {
    mOwnerList      = ownerList;
    mDeviceIndex    = deviceIndex;
    mPad            = deviceIndex;
    mLastLeft       = 0;
    mLastRight      = 0;
    mLastTriggerLeft  = 0;
    mLastTriggerRight = 0;
    mPaused         = true;
    mNOSEngaged     = false;
    mLastUpdateTime = 0.0;
    mStopGeneration = RumbleStopGeneration;
    mOnWGI          = false;
    memset(&mPulseLeft, 0, sizeof(RumblePulse));
    memset(&mPulseRight, 0, sizeof(RumblePulse));
    memset(&mNOSTexture, 0, sizeof(RumbleTextureState));
    memset(&mMixer, 0, sizeof(RumbleMixer));
    memset(&mScrapeTexture, 0, (char*)(&mTriggerLockupPhase + 1) - (char*)&mScrapeTexture);  // everything from the scrape texture on
    mPrevNOSEngaged     = false;
    mPrevShifting       = false;
    mPrevEngineBlown    = false;
    mPrevShiftPotential = SHIFT_POTENTIAL_NONE;
    ClearEffects();
  }

  void ClearEffects() {
    mPulseLeft.pPoints     = NULL;
    mPulseRight.pPoints    = NULL;
    mShiftCueLeft.pPoints  = NULL;
    mShiftCueRight.pPoints = NULL;
    mBumpLeft.pPoints      = NULL;
    mBumpRight.pPoints     = NULL;
    mLandingLeft.pPoints   = NULL;
    mLandingRight.pPoints  = NULL;
    mTriggerCue.pPoints    = NULL;
    mLockupLevel           = 0.0f;
    mSpinLevel             = 0.0f;
    mSlideLevel            = 0.0f;
    mHasPrevVehicle        = false;
    mRoadWave.Begin();
    mSkidWave.Begin();
    mBlownTireWave.Begin();
    mNOSKick.Stop();
    mShiftTick.Stop();
    mEngineBlownRamp.Stop();
    mNOSEngaged         = false;
    mShifting           = false;
    mEngineBlown        = false;
    mShiftPotential     = SHIFT_POTENTIAL_NONE;
    mScrapeSpeed        = 0.0f;
    mScrapeLevel        = 0.0f;
  }

  void Send(int left, int right, int triggerLeft = 0, int triggerRight = 0) {
    if (mStopGeneration != RumbleStopGeneration) {
      // all pads got zeroed behind our back
      mStopGeneration   = RumbleStopGeneration;
      mLastLeft         = 0;
      mLastRight        = 0;
      mLastTriggerLeft  = 0;
      mLastTriggerRight = 0;
    }

    bool useWGI = RumbleWGI::ShouldUse();
    if (useWGI != mOnWGI) {
      // switching paths: zero the one we leave so nothing sticks, and resend everything on the new one
      if (mOnWGI) {
        RumbleWGI::Send(0, 0, 0, 0);
      } else {
        XINPUT_VIBRATION off = {0, 0};
        XInputSetState(mPad, &off);
      }
      mOnWGI    = useWGI;
      mLastLeft = mLastRight = mLastTriggerLeft = mLastTriggerRight = -1;
    }

    left  = RumbleRoundToPercent(left);
    right = RumbleRoundToPercent(right);

    if (useWGI) {
      triggerLeft  = RumbleRoundToPercent(triggerLeft);
      triggerRight = RumbleRoundToPercent(triggerRight);
      if (left == mLastLeft && right == mLastRight && triggerLeft == mLastTriggerLeft && triggerRight == mLastTriggerRight) return;
      RumbleWGI::Send(left / 65535.0, right / 65535.0, triggerLeft / 65535.0, triggerRight / 65535.0);
      mLastLeft         = left;
      mLastRight        = right;
      mLastTriggerLeft  = triggerLeft;
      mLastTriggerRight = triggerRight;
      return;
    }
    mLastTriggerLeft = mLastTriggerRight = 0;  // XInput can't drive the triggers

    int pad = bXInputOmniMode ? RumbleLastActivePad : mDeviceIndex;
    if (pad != mPad) {
      XINPUT_VIBRATION off = {0, 0};
      XInputSetState(mPad, &off);
      mPad      = pad;
      mLastLeft = mLastRight = -1;
    }

    if (left == mLastLeft && right == mLastRight) return;

    XINPUT_VIBRATION vib = {(WORD)left, (WORD)right};
    if (XInputSetState(mPad, &vib) == ERROR_SUCCESS) {
      mLastLeft  = left;
      mLastRight = right;
    } else {
      // not connected - resend once it comes back
      mLastLeft = mLastRight = -1;
    }
  }

  void Stop() {
    ClearEffects();
    Send(0, 0);
  }

  // called every frame from PollDevice: stop the motors if the game stopped ticking us without pausing
  void Watchdog() {
    if (mPaused || (mLastLeft <= 0 && mLastRight <= 0 && mLastTriggerLeft <= 0 && mLastTriggerRight <= 0)) return;
    if (!bRumbleAppActive || RumbleGetTime() - mLastUpdateTime > 0.25) {
      mPaused = true;
      Stop();
    }
  }

  // never deleted: it's embedded in InputDevice
  virtual void* ScalarDtor(unsigned int /*flags*/) { return this; }

  virtual void PauseEffects() {
    mPaused = true;
    Stop();
  }

  virtual void ResumeEffects() {
    if (mPaused) {
      mPaused         = false;
      mLastUpdateTime = RumbleGetTime();
    }
  }

  virtual void ResetEffects() { PauseEffects(); }

  virtual void BeginUpdate() {
    mNOSEngaged     = false;
    mShifting       = false;
    mEngineBlown    = false;
    mShiftPotential = SHIFT_POTENTIAL_NONE;
    mWheelArg       = 0;
    for (int i = 0; i < RUMBLE_MAX_WHEELS; i++) mWheelSkid[i] = 0.0f;
    mRoadWave.Begin();
    mSkidWave.Begin();
    mBlownTireWave.Begin();
    RumbleTelemetry.nWheel = 0;
  }

  // kerbs and bumps: a sharp rise in a grounded wheel's compression knocks once per wheel (landings are collisions).
  // Measured over the sim clock, so frame rate doesn't matter.
  void UpdateBumps() {
    if (!mHasVehicle) {
      mHasPrevVehicle = false;
      return;
    }
    float simTime = RumbleGetSimTime();
    if (mHasPrevVehicle && simTime == mPrevSimTime) return;  // the physics hasn't stepped since the last sample
    float simDt = simTime - mPrevSimTime;
    if (mHasPrevVehicle && simDt > 0.0f && simDt < RUMBLE_BUMP_MAX_SIM_STEP && mVehicle.fSpeed > RUMBLE_BUMP_MIN_SPEED) {
      for (int i = 0; i < mVehicle.nWheels; i++) {
        if (!mVehicle.bOnGround[i] || !mPrevOnGround[i]) continue;
        float velocity = (mVehicle.fCompression[i] - mPrevCompression[i]) / simDt;
        if (!(velocity >= RUMBLE_BUMP_MIN_VELOCITY)) continue;
        float strength = Clamp01((velocity - RUMBLE_BUMP_MIN_VELOCITY) / (RUMBLE_BUMP_FULL_VELOCITY - RUMBLE_BUMP_MIN_VELOCITY));
        strength       = 0.35f + 0.65f * strength;
        mBumpLeft.Start(BumpPoints, _countof(BumpPoints), strength * 0.8f);
        mBumpRight.Start(BumpPoints, _countof(BumpPoints), strength * 0.5f);
      }
    }
    for (int i = 0; i < mVehicle.nWheels; i++) {
      mPrevCompression[i] = mVehicle.fCompression[i];
      mPrevOnGround[i]    = mVehicle.bOnGround[i];
    }
    mPrevSimTime    = simTime;
    mHasPrevVehicle = true;
  }

  // lockup, wheelspin and slides; adds their textures to the mixer
  void MixTyres(float dt) {
    float lockup = 0.0f, spin = 0.0f;
    if (mHasVehicle && mVehicle.fSpeed > 3.0f) {
      bool braking = mVehicle.fBrake > 0.3f || mVehicle.fHandBrake > 0.3f;
      for (int i = 0; i < mVehicle.nWheels; i++) {
        if (!mVehicle.bOnGround[i]) continue;
        float slip = mVehicle.fSlip[i];
        if (braking && -slip > RUMBLE_LOCKUP_MIN_SLIP) {
          float l = Clamp01((-slip - RUMBLE_LOCKUP_MIN_SLIP) / (RUMBLE_LOCKUP_FULL_SLIP - RUMBLE_LOCKUP_MIN_SLIP));
          if (l > lockup) lockup = l;
        }
        if (mVehicle.fGas > 0.3f && slip > RUMBLE_SPIN_MIN_SLIP) {
          float s = Clamp01((slip - RUMBLE_SPIN_MIN_SLIP) / (RUMBLE_SPIN_FULL_SLIP - RUMBLE_SPIN_MIN_SLIP));
          if (s > spin) spin = s;
        }
      }
    } else if (mHasVehicle && mVehicle.fGas > 0.3f) {
      // burnout from standstill
      for (int i = 0; i < mVehicle.nWheels; i++) {
        float s = Clamp01((mVehicle.fSlip[i] - RUMBLE_SPIN_MIN_SLIP) / (RUMBLE_SPIN_FULL_SLIP - RUMBLE_SPIN_MIN_SLIP));
        if (s > spin) spin = s;
      }
    }

    // lockup: a tyre juddering against the road, light motor buzz with a ~7 Hz heavy motor stutter (the heavy motor
    // can't spin up and down fast enough for more)
    mLockupPhase += dt * 7.0f;
    mLockupPhase -= floorf(mLockupPhase);
    float stutter = mLockupPhase < 0.5f ? 1.0f : 0.3f;
    mMixer.AddTexture(kRumbleTyres, mLockupTexture, lockup > 0.0f ? (0.15f + 0.25f * lockup) * stutter : 0.0f, lockup > 0.0f ? 0.25f + 0.35f * lockup : 0.0f);
    // wheelspin: smooth light-motor whirr with a bit of weight
    mMixer.AddTexture(kRumbleTyres, mSpinTexture, spin > 0.0f ? 0.1f * spin : 0.0f, spin > 0.0f ? 0.15f + 0.3f * spin : 0.0f);

    // slides: the console's per-surface skid wave on the heavy motor (front wheels x0.25 like the PS2), plus a light
    // hiss on the light motor; skids already have the sign stripped by DoFFB
    float skid = 0.0f;
    for (int i = 0; i < RUMBLE_MAX_WHEELS; i++) {
      float s = mWheelSkid[i] * (i < 2 ? 0.25f : 1.0f);
      if (s > skid) skid = s;
    }
    float waveLeft, waveRight;
    mSkidWave.Sample(dt, waveLeft, waveRight);
    float slide = Clamp01((skid - RUMBLE_SLIDE_MIN_SKID) / (RUMBLE_SLIDE_FULL_SKID - RUMBLE_SLIDE_MIN_SKID));
    mMixer.AddTexture(kRumbleTyres, mSlideTexture, waveLeft * RUMBLE_SKID_WAVE_SCALE, slide > 0.0f ? 0.05f + 0.15f * slide : 0.0f);

    mLockupLevel = lockup;
    mSpinLevel   = spin;
    mSlideLevel  = slide;
  }

  // pedal coupling: off with the pedal (nearly) released, so a resting finger feels no phantom buzz
  static float PedalGain(float pedal, float gate) {
    if (pedal < gate) return 0.0f;
    if (!bRumbleTriggerPedalScaling) return 1.0f;
    return 0.35f + 0.65f * powf(pedal, 0.7f);
  }

  // continuous trigger textures get the same fatigue fade as the motor textures
  float TriggerTexture(RumbleTextureState& state, float value, float dt) { return value * state.Update(value > 0.0f, dt); }

  // each trigger takes the strongest effect, never the sum
  void MixTriggers(float dt, float& leftTrigger, float& rightTrigger) {
    float gas   = mHasVehicle ? mVehicle.fGas : 0.0f;
    float brake = mHasVehicle ? mVehicle.fBrake : 0.0f;
    float tyres = fRumbleCategoryScale[kRumbleTyres];
    float car   = fRumbleCategoryScale[kRumbleCar];

    // LT: lockup, ABS-style ~12 Hz on/off pulses (the trigger motors are quick enough), steady with a stutter when severe
    mTriggerLockupPhase += dt * 12.0f;
    mTriggerLockupPhase -= floorf(mTriggerLockupPhase);
    float lockup = 0.0f;
    if (mLockupLevel > 0.0f) {
      bool  on    = mTriggerLockupPhase < 0.5f;
      float pulse = mLockupLevel > 0.8f ? (on ? 1.0f : 0.8f) : (on ? 1.0f : 0.0f);
      lockup      = (0.2f + 0.4f * mLockupLevel) * pulse * PedalGain(brake, 0.05f);
    }
    float lockupFatigue = mTriggerLockupTexture.Update(mLockupLevel > 0.0f && brake >= 0.05f, dt);
    leftTrigger         = lockup * lockupFatigue * tyres;

    // RT: wheelspin with an ~11 Hz grain so it reads differently from the lockup pulses
    mSpinPhase += dt * 11.0f;
    mSpinPhase -= floorf(mSpinPhase);
    float throttle = PedalGain(gas, 0.1f);
    float spin     = mSpinLevel > 0.0f ? (0.15f + 0.35f * mSpinLevel) * (1.0f + 0.2f * sinf(6.2831853f * mSpinPhase)) * throttle : 0.0f;
    float slide    = mSlideLevel > 0.0f ? (0.1f + 0.2f * mSlideLevel) * throttle : 0.0f;  // power-on slides only
    float nos      = mNOSEngaged ? 0.12f * throttle : 0.0f;

    float right = TriggerTexture(mTriggerSpinTexture, spin, dt) * tyres;
    float v     = TriggerTexture(mTriggerSlideTexture, slide, dt) * tyres;
    if (v > right) right = v;
    v = TriggerTexture(mTriggerNOSTexture, nos, dt) * car;
    if (v > right) right = v;
    v = mShiftTick.GetValue() * RumbleShiftFeels[RumbleShiftPreset].fTrigger * car;
    if (v > right) right = v;
    v = mTriggerCue.GetValue() * car;
    if (v > right) right = v;
    rightTrigger = right;

    leftTrigger *= fRumbleTriggerIntensity;
    rightTrigger *= fRumbleTriggerIntensity;
  }

  // edge-triggered car effects (PS2 InputEffectState values unless noted)
  void UpdateCarEffects() {
    if (mNOSEngaged && !mPrevNOSEngaged) mNOSKick.Start(1.0f, 0.5f, 0.5f);  // PS2: 2 s, shortened
    if (!mNOSEngaged) mNOSKick.Stop();

    if (mEngineBlown && !mPrevEngineBlown) mEngineBlownRamp.Start(1.0f, 0.6f, 1.0f);
    if (!mEngineBlown) mEngineBlownRamp.Stop();

    if (mShifting && !mPrevShifting && RumbleShiftFeels[RumbleShiftPreset].fTime > 0.0f)
      mShiftTick.Start(1.0f, RumbleShiftFeels[RumbleShiftPreset].fFloor, RumbleShiftFeels[RumbleShiftPreset].fTime);  // PS2 uses 0.4 for 0.5 s

    if (mShiftPotential != mPrevShiftPotential) {
      if (mShiftPotential == SHIFT_POTENTIAL_PERFECT) {
        mShiftCueRight.Start(PerfectShiftPoints, _countof(PerfectShiftPoints), 0.4f);
        mTriggerCue.Start(TriggerPerfectPoints, _countof(TriggerPerfectPoints), 0.35f);
      } else if (mShiftPotential == SHIFT_POTENTIAL_MISS) {
        mShiftCueLeft.Start(MissShiftPoints, _countof(MissShiftPoints), 0.35f);
        mShiftCueRight.Start(MissShiftPoints, _countof(MissShiftPoints), 0.5f);
        mTriggerCue.Start(TriggerMissPoints, _countof(TriggerMissPoints), 0.25f);
      }
    }

    mPrevNOSEngaged     = mNOSEngaged;
    mPrevEngineBlown    = mEngineBlown;
    mPrevShifting       = mShifting;
    mPrevShiftPotential = mShiftPotential;
  }

  virtual void EndUpdate() {
    if (!bRumbleEnabled) return;  // GetInterfaces hides the object when disabled

    double now = RumbleGetTime();
    float  dt  = (float)(now - mLastUpdateTime);
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;
    mLastUpdateTime = now;

    mPulseLeft.Advance(dt);
    mPulseRight.Advance(dt);
    mShiftCueLeft.Advance(dt);
    mShiftCueRight.Advance(dt);
    mNOSKick.Advance(dt);
    mShiftTick.Advance(dt);
    mEngineBlownRamp.Advance(dt);
    mBumpLeft.Advance(dt);
    mBumpRight.Advance(dt);
    mLandingLeft.Advance(dt);
    mLandingRight.Advance(dt);
    mTriggerCue.Advance(dt);
    UpdateCarEffects();

    mHasVehicle = !mPaused && RumbleReadVehicle(mVehicle);
    UpdateBumps();

    // scrape: sliding contacts keep it alive, it fades quickly once they stop
    float scrapeTarget = mScrapeSpeed > 1.0f ? (mScrapeSpeed - 1.0f) / 20.0f : 0.0f;
    if (scrapeTarget > 1.0f) scrapeTarget = 1.0f;
    mScrapeLevel = scrapeTarget > mScrapeLevel ? scrapeTarget : mScrapeLevel - dt / 0.15f;
    if (mScrapeLevel < 0.0f) mScrapeLevel = 0.0f;
    mScrapeSpeed = 0.0f;

    if (bRumbleTelemetry && !mPaused) RumbleTelemetryWriteTick(now, mHasVehicle ? &mVehicle : NULL, mNOSEngaged, mShifting, mShiftPotential);

    if (!bRumbleAppActive || mPaused) {
      Send(0, 0);
      return;
    }

    mMixer.Begin(dt);
    mMixer.AddPulse(kRumbleImpacts, mPulseLeft.GetValue(), mPulseRight.GetValue());
    mMixer.AddTexture(kRumbleImpacts, mScrapeTexture, mScrapeLevel * 0.1f, mScrapeLevel * 0.4f);
    float blownLeft, blownRight;  // spiked / blown tyre wobble
    mBlownTireWave.Sample(dt, blownLeft, blownRight);
    mMixer.AddTexture(kRumbleImpacts, mBlownTireTexture, blownLeft * RUMBLE_SURFACE_WAVE_SCALE, blownRight * RUMBLE_SURFACE_WAVE_SCALE);

    mMixer.AddPulse(kRumbleSurface, mBumpLeft.GetValue(), mBumpRight.GetValue());
    mMixer.AddPulse(kRumbleSurface, mLandingLeft.GetValue(), mLandingRight.GetValue());
    float roadLeft, roadRight;
    mRoadWave.Sample(dt, roadLeft, roadRight);
    mMixer.AddTexture(kRumbleSurface, mRoadTexture, roadLeft * RUMBLE_SURFACE_WAVE_SCALE, roadRight * RUMBLE_SURFACE_WAVE_SCALE);

    MixTyres(dt);

    mMixer.AddPulse(kRumbleCar, mNOSKick.GetValue(), mNOSKick.GetValue() * 0.3f);
    mMixer.AddTexture(kRumbleCar, mNOSTexture, mNOSEngaged ? 0.15f : 0.0f, mNOSEngaged ? 0.30f : 0.0f);
    mMixer.AddPulse(kRumbleCar, mShiftTick.GetValue() * RumbleShiftFeels[RumbleShiftPreset].fLeft, mShiftTick.GetValue() * RumbleShiftFeels[RumbleShiftPreset].fRight);
    mMixer.AddPulse(kRumbleCar, mEngineBlownRamp.GetValue(), mEngineBlownRamp.GetValue());
    mMixer.AddPulse(kRumbleCar, mShiftCueLeft.GetValue(), mShiftCueRight.GetValue());

    int left, right;
    mMixer.Resolve(left, right);
    float leftTrigger = 0.0f, rightTrigger = 0.0f;
    if (mOnWGI) MixTriggers(dt, leftTrigger, rightTrigger);
    Send(left, right, RumbleMixer::ToMotor(leftTrigger, 1.0f), RumbleMixer::ToMotor(rightTrigger, 1.0f));
  }

  // DoFFB calls Slip, Skid, then RoadNoise for each wheel in turn
  virtual void UpdateRoadNoise(bool /*front*/, const SimSurface& surface, float speed) {
    (RumbleIsBlownTireSurface(&surface) ? mBlownTireWave : mRoadWave).Add(&surface, ATTRIB_KEY_FFB_ROLL, speed);
    mWheelArg++;
    if (bRumbleTelemetry && RumbleTelemetry.nWheel < RUMBLE_MAX_WHEELS) RumbleTelemetry.iSurface[RumbleTelemetry.nWheel] = RumbleTelemetrySurfaceId(&surface);
    RumbleTelemetry.nWheel++;
  }
  virtual void UpdateTireSkid(bool front, const SimSurface& surface, float skid) {
    mSkidWave.Add(&surface, ATTRIB_KEY_FFB_SKID, front ? skid * 0.25f : skid);  // PS2 damps the front wheels
    if (mWheelArg < RUMBLE_MAX_WHEELS) mWheelSkid[mWheelArg] = skid;
    if (RumbleTelemetry.nWheel < RUMBLE_MAX_WHEELS) RumbleTelemetry.fSkid[RumbleTelemetry.nWheel] = skid;
  }
  virtual void UpdateTireSlip(bool /*front*/, const SimSurface& /*surface*/, float slip) {
    if (RumbleTelemetry.nWheel < RUMBLE_MAX_WHEELS) RumbleTelemetry.fSlip[RumbleTelemetry.nWheel] = slip;
  }
  virtual void UpdateRPM(float powerband, float /*overrev*/, float throttle) {
    RumbleTelemetry.fPowerband = powerband;
    RumbleTelemetry.fThrottle  = throttle;
  }
  virtual void UpdateShiftPotential(ShiftPotential potential) { mShiftPotential = potential; }
  virtual void UpdateNOS(bool engaged, float /*nosLevel*/) { mNOSEngaged = engaged; }
  virtual void UpdateEngineBlown(bool blown) { mEngineBlown = blown; }
  virtual void UpdateShifting(bool shifting) { mShifting = shifting; }

  static float Length(const float* v) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
  static float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

  // the player's speed change across the hit, |body velocity - pre-impact velocity| like the PS2; falls back to the
  // closing speed along the normal when the body can't be read or hasn't taken the hit yet. -1 if invalid.
  static float CollisionSpeedChange(const COLLISION_INFO& cinfo, bool iamA, bool& measured) {
    const float* n        = cinfo.normal;
    const float* cv       = cinfo.closingVel;
    float        estimate = fabsf(cv[0] * n[0] + cv[1] * n[1] + cv[2] * n[2]);
    if ((cinfo.flags & 7) == 1) estimate *= 0.5f;

    const float* preVel = iamA ? cinfo.objAVel : cinfo.objBVel;
    float        vel[3];
    float        dv = estimate;
    measured        = false;
    if (RumbleReadLinearVelocity(vel)) {
      float d[3]   = {vel[0] - preVel[0], vel[1] - preVel[1], vel[2] - preVel[2]};
      float change = Length(d);
      if (change >= estimate * RUMBLE_COLLISION_MIN_MEASURED) {
        dv       = change;
        measured = true;
      }
    }
    return dv == dv ? dv : -1.0f;  // NaN
  }

  // PS2 GameDevice::ReportCollision strength, from the speed change dv (m/s)
  static float CollisionMagnitude(const COLLISION_INFO& cinfo, bool iamA, float dv) {
    if (dv < 0.0f) return 0.0f;

    switch (cinfo.flags & 7) {
      case 2:  // WORLD (walls)
        return Clamp01(dv / 30.0f);
      case 3:  // GROUND (landings, bottoming out)
        return Clamp01((dv - 1.0f) / 3.0f);
      case 1: {  // OBJECT (cars, props)
        if (dv >= 5.0f) return Clamp01((dv - 5.0f) / 20.0f);

        const float* myVel    = iamA ? cinfo.objAVel : cinfo.objBVel;
        const float* otherVel = iamA ? cinfo.objBVel : cinfo.objAVel;
        float        mySpeed  = Length(myVel);
        float        other    = Length(otherVel);
        if (mySpeed > 0.0f && other > 0.0f) {
          float dot = (myVel[0] * otherVel[0] + myVel[1] * otherVel[1] + myVel[2] * otherVel[2]) / (mySpeed * other);
          if (dot > 0.1f) {  // rear-end
            float x = 0.5f * (mySpeed > other ? mySpeed : other);
            return x > 5.0f ? Clamp01((x - 5.0f) / 20.0f) : 0.0f;
          }
        }
        if (mySpeed > 2.0f && other > 2.0f) return Clamp01((mySpeed - 20.0f) / 30.0f);  // contact at speed
        return 0.0f;
      }
      default:
        return 0.0f;
    }
  }

  virtual void ReportCollision(const COLLISION_INFO& cinfo, bool iamA) {
    if (!bRumbleEnabled || mPaused) return;

    bool  sliding  = (cinfo.flags & 0x80) != 0;
    bool  measured = false;
    float dv       = sliding ? 0.0f : CollisionSpeedChange(cinfo, iamA, measured);
    float strength = sliding ? 0.0f : CollisionMagnitude(cinfo, iamA, dv);
    if (bRumbleTelemetry) {
      const float* n  = cinfo.normal;
      const float* cv = cinfo.closingVel;
      RumbleTelemetryPrintf("C,%.4f,%d,0x%02X,%.2f,%.2f,%.2f,%.2f,%.2f,%d\n", RumbleGetTime() - RumbleTelemetry.fStartTime, cinfo.flags & 7, cinfo.flags & 0xFF,
                            Length(cv), fabsf(cv[0] * n[0] + cv[1] * n[1] + cv[2] * n[2]), Length(cinfo.slidingVel), dv, strength, measured);
    }

    // sliding contacts feed the scrape texture instead of starting a hit
    if (sliding) {
      float speed = Length(cinfo.slidingVel);
      if (speed > mScrapeSpeed) mScrapeSpeed = speed;
      return;
    }

    if (strength <= 0.0f) return;

    // landings and bottoming out are a Surface effect, everything else an Impact
    bool         ground = (cinfo.flags & 7) == 3;
    RumblePulse& light  = ground ? mLandingRight : mPulseRight;
    RumblePulse& heavy  = ground ? mLandingLeft : mPulseLeft;
    light.Start(SmallRumblePoints, _countof(SmallRumblePoints), strength);
    // the heavy motor only kicks in for proper hits
    if (strength > 0.4f) heavy.Start(BigRumblePoints, _countof(BigRumblePoints), strength);

#ifdef _DEBUG
    char buf[160];
    sprintf_s(buf, "[XtendedInput] collision: type %d, closingVel %.2f m/s, strength %.2f\n", cinfo.flags & 7, Length(cinfo.closingVel), strength);
    OutputDebugStringA(buf);
#endif
  }
};
#pragma warning(pop)
#pragma pack(pop)
static_assert(sizeof(void*) == 4, "32-bit only");
