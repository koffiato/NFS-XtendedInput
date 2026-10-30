// Controller rumble port from koffiato/NFS-XtendedInput (MIT).
// Original rumble: Copyright (c) 2026 Martin Penkava.
// U2 adapter uses native feedback events and wheel data; no MW addresses.
#pragma once
#include <cmath>
#include <cstdarg>
#include <float.h>
#include <algorithm>
#include <roapi.h>
#include <windows.gaming.input.h>

enum RumbleCategory {
  kRumbleImpacts,  // hits, scrapes, spike strips, blown tyres
  kRumbleSurface,  // kerbs, bumps, landings, road texture
  kRumbleTyres,    // lockup, wheelspin, slides
  kRumbleCar,      // NOS, shifts, engine
  kRumbleCategoryCount,
};

// Disabled means no rumble patches or device output.
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

double RumbleGetTime() {
  static LARGE_INTEGER freq = {0};
  if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return (double)now.QuadPart / (double)freq.QuadPart;
}

// the Xbox motors have 101 levels (0-100 %); round 0-65535 values to them so near-identical values don't cost a call
int RumbleRoundToPercent(int value) { return (value * 100 + 32767) / 65535 * 65535 / 100; }

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

// Underground 2 US 1.2. These sites are validated together before any rumble write.
// Event dispatcher: cdecl(type, port, amount). The original only queues console
// motor curves; steering-wheel forces have a separate path and remain intact.
const uintptr_t RUMBLE_EVENT_ADDR = 0x005727B0;
const uintptr_t RUMBLE_UPDATE_CALL = 0x005814AD;
const uintptr_t RUMBLE_UPDATE_ADDR = 0x00572610;
const uintptr_t RUMBLE_SETTING_ADDR = 0x005F6397;
const uintptr_t RUMBLE_COLLISION_SETTING_ADDR = 0x005A9A24;
const uintptr_t RUMBLE_EVENT_TABLE_ADDR = 0x007FB7D8;
const uintptr_t RUMBLE_PLAYERS_ADDR = 0x008900AC;
const uintptr_t RUMBLE_PLAYER_COUNT_ADDR = 0x008900BC;
const uintptr_t RUMBLE_SIM_DT_ADDR = 0x00865154;
const uintptr_t RUMBLE_PACKAGE_ACTIVE_ADDR = 0x0052CF60;
const int RUMBLE_EVENT_COUNT = 12;
const int RUMBLE_PORT_COUNT = 2;

struct RumbleNativeCurve {
  const RumbleEnvelopePoint* points;
  int count;
};
struct RumbleNativeEvent {
  int type;
  unsigned char priority, enabled;
  unsigned short reserved;
  const RumbleNativeCurve* curve;
  const float* params; // input min/max, amplitude min/max; road adds initial-value min/max
};
static_assert(sizeof(RumbleNativeEvent) == 16, "U2 feedback table layout");

bool bRumbleInstalled = false;
bool bRumbleTelemetry = false;
bool bRumbleOnWGI = false;
double RumbleLastFrame = 0.0;
float RumbleSimClock = 1.0f;
FILE* RumbleLog = NULL;
bool bRumbleLogTried = false;

float RumbleClamp(float value, float lo = 0.0f, float hi = 1.0f) {
  if (!_finite(value)) return lo;
  return value < lo ? lo : value > hi ? hi : value;
}
float RumbleRampValue(float value, float lo, float hi) {
  return RumbleClamp((value - lo) / (hi - lo));
}

struct RumblePort {
  RumblePulse event[RUMBLE_EVENT_COUNT];
  RumbleEnvelopePoint points[RUMBLE_EVENT_COUNT][8];
  RumbleTextureState texture[kRumbleCategoryCount];
  RumbleTextureState triggerTexture[2];
  RumbleRamp shift;
  float lastShiftEvent;
  float lastNOS;
  float spin, lockup;
  int wanted[4];
  int sent[4];
};
RumblePort RumblePorts[RUMBLE_PORT_COUNT] = {};
bool bRumbleWGIActive = false;
int RumbleLastWGI[4] = {};
double RumbleLastWGISend = 0;

void RumbleTelemetry(const char* fmt, ...) {
  if (!bRumbleTelemetry) return;
  if (!bRumbleLogTried) {
    bRumbleLogTried = true;
    CIniReader ini("");
    char path[MAX_PATH];
    strncpy_s(path, ini.GetIniPath(), _TRUNCATE);
    char* ext = strrchr(path, '.');
    if (ext) *ext = '\0';
    strcat_s(path, "_Rumble.log");
    RumbleLog = fopen(path, "w");
    if (RumbleLog) setvbuf(RumbleLog, NULL, _IONBF, 0);
  }
  if (!RumbleLog) return;
  va_list args;
  va_start(args, fmt);
  vfprintf(RumbleLog, fmt, args);
  va_end(args);
}

void RumbleResetPort(int port) {
  ZeroMemory(&RumblePorts[port], sizeof(RumblePorts[port]));
  RumblePorts[port].lastShiftEvent = -100.0f;
  RumblePorts[port].lastNOS = -100.0f;
}

void RumbleStopAllPads(bool fromDllMain = false) {
  if (!bRumbleInstalled) return;
  XINPUT_VIBRATION off = {};
  for (int port = 0; port < RUMBLE_PORT_COUNT; ++port) {
    const RumblePort& state = RumblePorts[port];
    if (fromDllMain || state.sent[0] || state.sent[1]) XInputSetState(port, &off);
    RumbleResetPort(port);
  }
  // WinRT must never be entered under the DLL loader lock.
  if (!fromDllMain && bRumbleWGIActive) {
    RumbleWGI::Send(0, 0, 0, 0);
    bRumbleWGIActive = false;
    ZeroMemory(RumbleLastWGI, sizeof(RumbleLastWGI));
  }
}

bool RumbleCanPlay() {
  if (!bRumbleInstalled) return false;
  HWND window = *(HWND*)GAME_HWND_ADDR;
  if (!window || GetForegroundWindow() != window || IsIconic(window)) return false;
  if (*(int*)GAMEFLOWMANAGER_STATUS_ADDR != 6) return false;
  typedef bool(__cdecl* PackageActive)(const char*);
  PackageActive active = (PackageActive)RUMBLE_PACKAGE_ACTIVE_ADDR;
  return !active("UI_Pause.fng") && !active("UI_PauseOptions.fng") &&
         !active("UI_PauseOptionsMain.fng") && !active("UI_PC_Customize_Options.fng");
}

// Called from the existing input poll and from the frame hook. It also catches
// game transitions that stop feedback updates while controller polling continues.
void RumblePoll() {
  if (!bRumbleInstalled) return;
  if (!RumbleCanPlay() || RumbleGetTime() - RumbleLastFrame > 0.25) {
    RumbleStopAllPads();
    return;
  }
  for (int port = 0; port < RUMBLE_PORT_COUNT; ++port) {
    if (!g_Controllers[port].bConnected) {
      XINPUT_VIBRATION off = {};
      XInputSetState(port, &off);
      RumbleResetPort(port);
    }
  }
}

// Native events: collision, road, drift, suspension, tyre transitions, NOS,
// shifting, auxiliary car effect, drag RPM, yaw and wheel slip respectively.
const RumbleCategory RumbleEventCategory[RUMBLE_EVENT_COUNT] = {
  kRumbleImpacts, kRumbleSurface, kRumbleTyres, kRumbleSurface,
  kRumbleImpacts, kRumbleImpacts, kRumbleCar, kRumbleCar,
  kRumbleCar, kRumbleCar, kRumbleTyres, kRumbleTyres
};
bool RumbleEventIsPulse(int type) { return type == 0 || type == 3 || type == 4 || type == 5 || type == 8; }

void __cdecl RumbleEvent(int type, int port, float amount) {
  if ((unsigned)port >= RUMBLE_PORT_COUNT || (unsigned)type >= RUMBLE_EVENT_COUNT ||
      !g_Controllers[port].bConnected || !RumbleCanPlay() || !_finite(amount)) return;
  const RumbleNativeEvent& native = ((const RumbleNativeEvent*)RUMBLE_EVENT_TABLE_ADDR)[type];
  // Preserve the game's disabled effects (notably types 4/5).
  if (!native.enabled || amount <= native.params[0]) return;
  float t = RumbleRampValue(amount, native.params[0], native.params[1]);
  float amplitude = native.params[2] + (native.params[3] - native.params[2]) * t;
  RumblePort& state = RumblePorts[port];
  if (type == 7) {
    const RumbleShiftFeel& feel = RumbleShiftFeels[RumbleShiftPreset];
    if (RumbleSimClock - state.lastShiftEvent > 0.10f)
      state.shift.Start(1.0f, feel.fFloor, feel.fTime);
    state.lastShiftEvent = RumbleSimClock;
    return;
  }
  if (type == 6) state.lastNOS = RumbleSimClock;
  RumblePulse& pulse = state.event[type];
  // Repeated physics samples sustain a curve without resetting its attack each tick.
  if (!pulse.pPoints || (RumbleEventIsPulse(type) && pulse.GetValue() < amplitude * native.curve->points[0].fValue / 255.0f)) {
    memcpy(state.points[type], native.curve->points, native.curve->count * sizeof(RumbleEnvelopePoint));
    if (type == 1) state.points[type][0].fValue = native.params[4] + (native.params[5] - native.params[4]) * t;
    pulse.pPoints = state.points[type];
    pulse.nPoints = native.curve->count;
    pulse.fTime = 0;
    pulse.fAmplitude = amplitude;
  } else if (!RumbleEventIsPulse(type)) {
    pulse.fAmplitude = amplitude;
  }
  RumbleTelemetry("E,%.4f,%d,%d,%.5f,%.5f\n", RumbleSimClock, port, type, amount, amplitude);
}

// U2's car -> mover -> rigid body/wheels. Read only while racing, guarded during
// car replacement and loading. These are U2 layouts, not MW's UTL interfaces.
bool RumbleReadSlip(int port, float& spin, float& lockup) {
  spin = lockup = 0;
  __try {
    int count = *(int*)RUMBLE_PLAYER_COUNT_ADDR;
    if (count <= port || count > RUMBLE_PORT_COUNT) return false;
    char* player = ((char**)RUMBLE_PLAYERS_ADDR)[port];
    char* car = player ? *(char**)(player + 4) : NULL;
    char* mover = car ? *(char**)(car + 0x34) : NULL;
    char* body = mover ? *(char**)(mover + 0x20) : NULL;
    if (!body) return false;
    const float* velocity = (const float*)(body + 0xA0);
    float speed = sqrtf(velocity[0]*velocity[0] + velocity[1]*velocity[1] + velocity[2]*velocity[2]);
    if (!_finite(speed) || speed < 1.0f || speed > 200.0f) return false;
    float slips[4];
    int grounded = 0;
    for (int i = 0; i < 4; ++i) {
      char* wheel = *(char**)(mover + 0x38 + i*4);
      char* tyre = wheel ? *(char**)(wheel + 0x14) : NULL;
      if (!tyre) return false;
      float radius = *(float*)(tyre + 0x10);
      float angular = *(float*)(wheel + 0x28);
      float grip = *(float*)(wheel + 0x170);
      if (!_finite(angular) || !_finite(grip) || !(radius > 0.15f && radius < 0.6f)) return false;
      bool onGround = grip > 0.005f;
      grounded += onGround;
      slips[i] = onGround ? (fabsf(angular)*radius - speed) / (speed > 3.0f ? speed : 3.0f) : 0.0f;
      if (-slips[i] > lockup) lockup = -slips[i];
    }
    if (!grounded) { lockup = 0; return true; }
    spin = (std::max)((slips[0]+slips[1])*0.5f, (slips[2]+slips[3])*0.5f);
    spin = RumbleRampValue(spin, 0.04f, 0.5f);
    lockup = RumbleRampValue(lockup, 0.04f, 0.25f);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// Scanner state follows configured bindings, including keyboard and alternate maps.
float RumblePedal(int port, int digital, int analog, int alternate) {
  if (EventStates[port][digital] || EventStatesKB[port][digital]) return 1.0f;
  float value = (std::max)(EventStates[port][analog], EventStates[port][alternate]) / 255.0f;
  if (EventStatesKB[port][analog] || EventStatesKB[port][alternate]) value = 1.0f;
  return RumbleClamp(value);
}

void RumbleSendOutputs() {
  static int connected = 0;
  static double lastTopologyCheck = -1;
  double now = RumbleGetTime();
  if (lastTopologyCheck < 0 || now - lastTopologyCheck > 1.0) {
    connected = 0;
    lastTopologyCheck = now;
    for (int port = 0; port < MAX_CONTROLLERS; ++port) {
      XINPUT_STATE state = {};
      connected += XInputGetState(port, &state) == ERROR_SUCCESS;
    }
  }
  // WGI cannot associate its list order with XInput slots. Auto is safe only
  // with exactly one pad, driving port 0; always mode intentionally broadcasts.
  bool useWGI = (RumbleTriggerMode == kTriggersOn || (connected == 1 && g_Controllers[0].bConnected)) && RumbleWGI::ShouldUse();
  if (useWGI != bRumbleOnWGI) {
    RumbleStopAllPads();
    bRumbleOnWGI = useWGI;
    // Cleared output is sent now; new effects arrive on the next simulation tick.
    return;
  }
  if (useWGI) {
    int combined[4] = {};
    for (int p = 0; p < RUMBLE_PORT_COUNT; ++p)
      for (int m = 0; m < 4; ++m) combined[m] = (std::max)(combined[m], RumblePorts[p].wanted[m]);
    if (memcmp(combined, RumbleLastWGI, sizeof(combined)) || now - RumbleLastWGISend > 1.0) {
      RumbleWGI::Send(combined[0]/65535.0, combined[1]/65535.0, combined[2]/65535.0, combined[3]/65535.0);
      memcpy(RumbleLastWGI, combined, sizeof(combined));
      RumbleLastWGISend = now;
    }
    bRumbleWGIActive = combined[0] || combined[1] || combined[2] || combined[3];
  } else {
    for (int p = 0; p < RUMBLE_PORT_COUNT; ++p) {
      RumblePort& state = RumblePorts[p];
      if (state.wanted[0] == state.sent[0] && state.wanted[1] == state.sent[1]) continue;
      XINPUT_VIBRATION vibration = {(WORD)state.wanted[0], (WORD)state.wanted[1]};
      if (XInputSetState(p, &vibration) == ERROR_SUCCESS) memcpy(state.sent, state.wanted, sizeof(state.sent));
    }
  }
}

void __cdecl RumbleFrame() {
  // Retain the game's channel housekeeping and steering-wheel path.
  ((void(__cdecl*)())RUMBLE_UPDATE_ADDR)();
  RumbleLastFrame = RumbleGetTime();
  float dt = *(float*)RUMBLE_SIM_DT_ADDR;
  if (!RumbleCanPlay() || !(dt > 0.0f && dt <= 0.1f)) { RumbleStopAllPads(); return; }
  RumbleSimClock += dt;
  for (int port = 0; port < RUMBLE_PORT_COUNT; ++port) {
    if (!g_Controllers[port].bConnected) { RumbleResetPort(port); continue; }
    RumblePort& state = RumblePorts[port];
    RumbleMixer mixer = {};
    mixer.Begin(dt);
    float textures[kRumbleCategoryCount][2] = {};
    for (int type = 0; type < RUMBLE_EVENT_COUNT; ++type) {
      float level = state.event[type].GetValue();
      RumbleCategory category = RumbleEventCategory[type];
      // U2 authored one large actuator; keep texture lighter than knocks.
      float left = level * (category == kRumbleCar ? 0.25f : 0.65f);
      float right = level * (category == kRumbleTyres ? 0.7f : 0.25f);
      if (RumbleEventIsPulse(type)) mixer.AddPulse(category, left, right);
      else {
        textures[category][0] = (std::max)(textures[category][0], left);
        textures[category][1] = (std::max)(textures[category][1], right);
      }
      state.event[type].Advance(dt);
    }
    float spin = 0, lockup = 0;
    if (!RumbleReadSlip(port, spin, lockup)) spin = lockup = 0;
    float gas = RumblePedal(port, JOY_EVENT_THROTTLE, JOY_EVENT_THROTTLE_ANALOG, JOY_EVENT_THROTTLE_ANALOG_ALTERNATE);
    float brake = RumblePedal(port, JOY_EVENT_BRAKE, JOY_EVENT_BRAKE_ANALOG, JOY_EVENT_BRAKE_ANALOG_ALTERNATE);
    spin *= gas;
    lockup *= brake;
    textures[kRumbleTyres][0] = (std::max)(textures[kRumbleTyres][0], spin*0.25f);
    textures[kRumbleTyres][1] = (std::max)(textures[kRumbleTyres][1], lockup*0.6f);
    for (int c = 0; c < kRumbleCategoryCount; ++c)
      mixer.AddTexture((RumbleCategory)c, state.texture[c], textures[c][0], textures[c][1]);
    const RumbleShiftFeel& feel = RumbleShiftFeels[RumbleShiftPreset];
    float shift = state.shift.GetValue();
    mixer.AddPulse(kRumbleCar, shift*feel.fLeft, shift*feel.fRight);
    state.shift.Advance(dt);
    mixer.Resolve(state.wanted[0], state.wanted[1]);
    float pedalL = bRumbleTriggerPedalScaling ? brake : (brake > 0 ? 1.0f : 0.0f);
    float pedalR = bRumbleTriggerPedalScaling ? gas : (gas > 0 ? 1.0f : 0.0f);
    float rawSpin = gas > 0 ? spin/gas : 0;
    float rawLockup = brake > 0 ? lockup/brake : 0;
    float triggerL = rawLockup*pedalL*0.6f*fRumbleCategoryScale[kRumbleTyres];
    float triggerR = rawSpin*pedalR*0.5f*fRumbleCategoryScale[kRumbleTyres];
    if (RumbleSimClock - state.lastNOS < 0.1f)
      triggerR = (std::max)(triggerR, 0.25f*pedalR*fRumbleCategoryScale[kRumbleCar]);
    triggerL *= state.triggerTexture[0].Update(triggerL > 0, dt);
    triggerR *= state.triggerTexture[1].Update(triggerR > 0, dt);
    triggerR = (std::max)(triggerR, shift*feel.fTrigger*fRumbleCategoryScale[kRumbleCar]);
    state.wanted[2] = RumbleMixer::ToMotor(triggerL, fRumbleTriggerIntensity);
    state.wanted[3] = RumbleMixer::ToMotor(triggerR, fRumbleTriggerIntensity);
    for (int m = 0; m < 4; ++m) state.wanted[m] = RumbleRoundToPercent(state.wanted[m]);
    RumbleTelemetry("F,%.4f,%d,%.3f,%.3f,%d,%d,%d,%d\n", RumbleSimClock, port, spin, lockup,
                    state.wanted[0], state.wanted[1], state.wanted[2], state.wanted[3]);
  }
  RumbleSendOutputs();
}

bool RumbleValidateHooks() {
  __try {
    const unsigned char event[] = {0x8B,0x44,0x24,0x04,0x56};
    const unsigned char update[] = {0xE8,0x5E,0x11,0xFF,0xFF};
    const unsigned char setting[] = {0x0F,0x95,0xC3};
    const unsigned char collisionSetting[] = {0xE8,0xFE,0x19,0x04,0x00,0x85,0xC0,0x74,0x13};
    const unsigned char package[] = {0xA1,0xD0,0x84,0x83,0x00};
    if (memcmp((void*)RUMBLE_EVENT_ADDR,event,sizeof(event)) ||
        memcmp((void*)RUMBLE_UPDATE_CALL,update,sizeof(update)) ||
        memcmp((void*)RUMBLE_SETTING_ADDR,setting,sizeof(setting)) ||
        memcmp((void*)(RUMBLE_COLLISION_SETTING_ADDR-7),collisionSetting,sizeof(collisionSetting)) ||
        memcmp((void*)RUMBLE_PACKAGE_ACTIVE_ADDR,package,sizeof(package))) return false;
    for (int i = 0; i < RUMBLE_EVENT_COUNT; ++i) {
      const RumbleNativeEvent& e = ((const RumbleNativeEvent*)RUMBLE_EVENT_TABLE_ADDR)[i];
      if (e.type != i || !e.curve || !e.params || e.curve->count < 2 || e.curve->count > 8 ||
          !(e.params[0] < e.params[1])) return false;
      for (int j = 0; j < e.curve->count; ++j) {
        const RumbleEnvelopePoint& p = e.curve->points[j];
        if (!_finite(p.fTime) || !_finite(p.fValue) || p.fTime < 0 || p.fTime > 10 ||
            (j && p.fTime < e.curve->points[j-1].fTime)) return false;
      }
    }
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void RumbleInit() {
  CIniReader ini("");
  bRumbleEnabled = ini.ReadInteger("Rumble", "Enabled", 1) != 0;
  bRumbleTelemetry = ini.ReadInteger("Rumble", "Telemetry", 0) != 0;
  if (!bRumbleEnabled) return;
  fRumbleIntensity = RumbleClamp(ini.ReadFloat("Rumble", "Intensity", 1), 0, 2);
  fRumbleLeftScale = RumbleClamp(ini.ReadFloat("Rumble", "LeftMotorScale", 1), 0, 2);
  fRumbleRightScale = RumbleClamp(ini.ReadFloat("Rumble", "RightMotorScale", 1), 0, 2);
  bRumbleSwapMotors = ini.ReadInteger("Rumble", "SwapMotors", 0) != 0;
  char* categories[] = {"Impacts", "Surface", "Tyres", "Car"};
  for (int c = 0; c < kRumbleCategoryCount; ++c)
    fRumbleCategoryScale[c] = RumbleClamp(ini.ReadFloat("Rumble", categories[c], 1), 0, 2);
  RumbleShiftPreset = (std::max)(0, (std::min)(kShiftPresetCount-1, ini.ReadInteger("Rumble", "GearShift", 2)));
  RumbleTriggerMode = (std::max)(0, (std::min)(kTriggerModeCount-1, ini.ReadInteger("Rumble", "ImpulseTriggers", 2)));
  fRumbleTriggerIntensity = RumbleClamp(ini.ReadFloat("Rumble", "TriggerIntensity", 1), 0, 2);
  bRumbleTriggerPedalScaling = ini.ReadInteger("Rumble", "TriggerPedalScaling", 1) != 0;
  if (!RumbleValidateHooks()) {
    OutputDebugStringA("XtendedInput rumble: unsupported U2 executable or conflicting hook; rumble disabled.\n");
    RumbleTelemetry("Rumble disabled: unsupported executable or conflicting hooks.\n");
    return;
  }
  for (int p = 0; p < RUMBLE_PORT_COUNT; ++p) RumbleResetPort(p);
  injector::MakeJMP(RUMBLE_EVENT_ADDR, RumbleEvent, true);
  injector::MakeCALL(RUMBLE_UPDATE_CALL, RumbleFrame, true);
  // The INI owns the switch. Replace `setne bl` with `mov bl,1; nop` in the
  // car's feedback producer, without modifying the profile's vibration byte.
  injector::WriteMemory<unsigned short>(RUMBLE_SETTING_ADDR, 0x01B3, true);
  injector::WriteMemory<unsigned char>(RUMBLE_SETTING_ADDR+2, 0x90, true);
  // Collision production queries that same hidden byte through Player::GetFeedback.
  // Keep its call and other collision guards, bypass only the false-setting branch.
  injector::MakeNOP(RUMBLE_COLLISION_SETTING_ADDR, 2, true);
  bRumbleInstalled = true;
  RumbleTelemetry("U2 rumble initialized; impulse mode=%d\n", RumbleTriggerMode);
}


