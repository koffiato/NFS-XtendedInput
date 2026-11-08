// Controller rumble port from koffiato/NFS-XtendedInput (MIT).
// Original rumble: Copyright (c) 2026 Martin Penkava.
// Underground adapters use native feedback channels; U2 also supplies tyre data.
#pragma once
#include <cstdarg>
#include "Settings.h"
#include "Log.h"

// UG US 1.4 and UG2 US 1.2 sites are validated before any rumble write.
// The test harness supplies a private image; installed games keep their fixed base.
#ifndef RUMBLE_IMAGE_BASE
#define RUMBLE_IMAGE_BASE 0x00400000u
#endif
#ifdef GAME_UG
// Underground US 1.4. The main loop samples all four native channels before
// this call. Keep its EDX=event pointer, ESI=port dispatcher untouched.
const uintptr_t RUMBLE_EVENT_ADDR = RUMBLE_IMAGE_BASE + 0x00174D90;
const uintptr_t RUMBLE_UPDATE_CALL = RUMBLE_IMAGE_BASE + 0x000479FC;
const uintptr_t RUMBLE_UPDATE_ADDR = RUMBLE_IMAGE_BASE + 0x00174EB0;
const uintptr_t RUMBLE_SETTING_ADDR = RUMBLE_IMAGE_BASE + 0x000292A0;
const uintptr_t RUMBLE_COLLISION_SETTING_ADDR = RUMBLE_IMAGE_BASE + 0x00065C50;
const uintptr_t RUMBLE_AUX_SETTING_ADDR = RUMBLE_IMAGE_BASE + 0x00031C7B;
const uintptr_t RUMBLE_EVENT_TABLE_ADDR = RUMBLE_IMAGE_BASE + 0x003045A8;
const uintptr_t RUMBLE_SIM_DT_ADDR = RUMBLE_IMAGE_BASE + 0x0033AD38;
#else
const uintptr_t RUMBLE_EVENT_ADDR = RUMBLE_IMAGE_BASE + 0x001727B0;
const uintptr_t RUMBLE_UPDATE_CALL = RUMBLE_IMAGE_BASE + 0x001814AD;
const uintptr_t RUMBLE_UPDATE_ADDR = RUMBLE_IMAGE_BASE + 0x00172610;
const uintptr_t RUMBLE_SETTING_ADDR = RUMBLE_IMAGE_BASE + 0x001F6397;
const uintptr_t RUMBLE_COLLISION_SETTING_ADDR = RUMBLE_IMAGE_BASE + 0x001A9A24;
const uintptr_t RUMBLE_EVENT_TABLE_ADDR = RUMBLE_IMAGE_BASE + 0x003FB7D8;
const uintptr_t RUMBLE_PLAYERS_ADDR = RUMBLE_IMAGE_BASE + 0x004900AC;
const uintptr_t RUMBLE_PLAYER_COUNT_ADDR = RUMBLE_IMAGE_BASE + 0x004900BC;
const uintptr_t RUMBLE_SIM_DT_ADDR = RUMBLE_IMAGE_BASE + 0x00465154;
const uintptr_t RUMBLE_PACKAGE_ACTIVE_ADDR = RUMBLE_IMAGE_BASE + 0x0012CF60;
#endif
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
static_assert(sizeof(RumbleNativeEvent) == 16, "Underground feedback table layout");

bool bRumbleInstalled = false;
bool bRumbleTelemetry = ConfigDefaults::Rumble::Telemetry;
bool bRumbleOnWGI = false;
double RumbleLastFrame = 0.0;
FILE* RumbleLog = NULL;
bool bRumbleLogTried = false;

struct RumblePort {
  int wanted[4];
  int sent[4];
#ifndef GAME_UG
  float brakeLevel;
  float gasLevel;
#endif
};
RumblePort RumblePorts[RUMBLE_PORT_COUNT] = {};
bool bRumbleWGIActive = false;
int RumbleLastWGI[4] = {};
double RumbleLastWGISend = 0;

void RumbleTelemetry(const char* fmt, ...) {
  if (!bRumbleTelemetry) return;
  if (!bRumbleLogTried) {
    bRumbleLogTried = true;
    InputConfig ini("");
    char path[MAX_PATH];
    strncpy_s(path, ini.GetIniPath(), _TRUNCATE);
    char* ext = strrchr(path, '.');
    if (ext) *ext = '\0';
    strcat_s(path, "_Rumble.log");
    RumbleLog = RumbleOpenLog(path);
    if (RumbleLog) setvbuf(RumbleLog, NULL, _IONBF, 0);
  }
  if (!RumbleLog) return;
  va_list args;
  va_start(args, fmt);
  vfprintf(RumbleLog, fmt, args);
  va_end(args);
}

#ifndef GAME_UG
const uintptr_t RUMBLE_OUTPUT_ADDR = RUMBLE_IMAGE_BASE + 0x22C0;
const uintptr_t RUMBLE_OUTPUT_CALL = RUMBLE_IMAGE_BASE + 0x17266E;
const uintptr_t RUMBLE_STOP_CALL = RUMBLE_IMAGE_BASE + 0x1726C5;
bool bRumbleInFrame = false;
#endif
struct RumbleNativeChannel {
  const RumbleNativeCurve* curve[3];
  float age[3], gain[3];
  signed char priority[3];
  char reserved;
  int output, previous;
};
static_assert(sizeof(RumbleNativeChannel) == 0x30, "Underground channel layout");
RumbleNativeChannel* RumbleChannels(int port) {
#ifdef GAME_UG
  return (RumbleNativeChannel*)(RUMBLE_IMAGE_BASE + 0x33B158 + port*0x60);
#else
  return (RumbleNativeChannel*)(RUMBLE_IMAGE_BASE + 0x4653B0 + port*0x60);
#endif
}
bool bRumbleNativeReady = false;

void RumbleResetPort(int port) {
  ZeroMemory(&RumblePorts[port], sizeof(RumblePorts[port]));
  if (bRumbleNativeReady) ZeroMemory(RumbleChannels(port), 0x60);
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
#ifdef GAME_UG
  // The simulation delta is zero while paused. Native producers also guard
  // loading/cinematic states before dispatch.
  return *(float*)RUMBLE_SIM_DT_ADDR > 0.0f;
#else
  typedef bool(__cdecl* PackageActive)(const char*);
  PackageActive active = (PackageActive)RUMBLE_PACKAGE_ACTIVE_ADDR;
  return !active("UI_Pause.fng") && !active("UI_PauseOptions.fng") &&
         !active("UI_PauseOptionsMain.fng") && !active("UI_PC_Customize_Options.fng");
#endif
}

#ifdef GAME_UG
bool RumbleFeedbackEnabled(int port) {
  // The options handler at 0x4BF0E2 toggles this same per-player byte and
  // stores it in the profile. Native event producers also check it.
  __try {
    const unsigned char* player = ((const unsigned char**)(RUMBLE_IMAGE_BASE + 0x3361B4))[port];
    return player && player[0x21] != 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
#endif

// Called from the existing input poll and from the frame hook. It also catches
// game transitions that stop feedback updates while controller polling continues.
void RumblePoll() {
  if (!bRumbleInstalled) return;
  if (!RumbleCanPlay() || RumbleGetTime() - RumbleLastFrame > 0.25) {
    RumbleStopAllPads();
    return;
  }
  for (int port = 0; port < RUMBLE_PORT_COUNT; ++port) {
    if (!g_Controllers[port].bConnected
#ifdef GAME_UG
        || !RumbleFeedbackEnabled(port)
#endif
    ) {
      // Stop previously sent output once; idle/disabled ports need no device call.
      if (RumblePorts[port].sent[0] || RumblePorts[port].sent[1]) {
        XINPUT_VIBRATION off = {};
        XInputSetState(port, &off);
      }
      RumbleResetPort(port);
    }
  }
}

#ifndef GAME_UG
// U2's car -> mover -> wheels. Read only while racing, guarded during
// car replacement and loading. These are U2 layouts, not MW's UTL interfaces.
bool RumbleReadSlip(int port, float& spin, float& lockup) {
  spin = lockup = 0;
  __try {
    int count = *(int*)RUMBLE_PLAYER_COUNT_ADDR;
    if (count <= port || count > RUMBLE_PORT_COUNT) return false;
    char* player = ((char**)RUMBLE_PLAYERS_ADDR)[port];
    char* car = player ? *(char**)(player + 4) : NULL;
    char* mover = car ? *(char**)(car + 0x34) : NULL;
    if (!mover) return false;
    float fastest = 0;
    float brakeTraction = 0;
    float brakeWeighted = 0, brakeLoad = 0;
    float gasTraction = 0;
    float poweredLoad = 0;
    for (int i = 0; i < 4; ++i) {
      char* wheel = *(char**)(mover + 0x38 + i*4);
      char* tyre = wheel ? *(char**)(wheel + 0x14) : NULL;
      if (!tyre) return false;
      float radius = *(float*)(tyre + 0x10);
      float angular = *(float*)(wheel + 0x28);
      if (!_finite(angular) || !(radius > 0.15f && radius < 0.6f)) return false;
      // Native tyre-force code tests this contact byte independently of grip.
      // A saturated/locked tyre must not disappear when its force ratio drops.
      if (!*(unsigned char*)(wheel + 0x11C)) continue;
      // Native tyre solver stores force/capacity at +170 after clipping the
      // force vector (+124 longitudinal, +128 lateral). Use only the braking
      // component so lateral saturation alone cannot become a brake cue.
      float longitudinal = *(float*)(wheel + 0x124);
      float lateral = *(float*)(wheel + 0x128);
      float utilization = *(float*)(wheel + 0x170);
      float load = *(float*)(wheel + 0x164);
      float driveTorque = *(float*)(wheel + 0x108);
      if (!_finite(longitudinal) || !_finite(lateral) || !_finite(utilization) || !_finite(load)) return false;
      if (load > 0) brakeLoad += load;
      bool powered = load > 0 && _finite(driveTorque) && driveTorque != 0 && driveTorque*angular >= 0;
      if (powered) poweredLoad += load;
      float force = sqrtf(longitudinal*longitudinal + lateral*lateral);
      if (load > 0 && force > .001f && _finite(force)) {
        float braking = RumbleClamp(utilization)*fabsf(longitudinal)/force;
        float progress = RumbleRampValue(braking, .25f, 1.0f);
        brakeTraction = (std::max)(brakeTraction, progress*progress);
        brakeWeighted += progress*progress*load;
        // +108 supplies drive torque to the wheel's native angular equation
        // (59D350). A powered tyre can exhaust grip through cornering before
        // its rotational speed visibly diverges from road speed.
        if (powered) {
          float traction = RumbleRampValue(utilization, .50f, 1.0f);
          // Cornering can saturate grip with little driving force. Keep its
          // cue proportional to the longitudinal share instead of full power.
          gasTraction += traction*traction*load*fabsf(longitudinal)/force;
        }
      }
      // Native contact update stores world point velocity at +F0 (59CF32..49).
      // +70 is the wheel orientation; its X axis is the rolling direction.
      const float* velocity = (const float*)(wheel + 0xF0);
      const float* forward = (const float*)(wheel + 0x70);
      if (bRumbleTelemetry) {
        float projected[3] = {};
        for (int basis=0; basis<3; ++basis)
          for (int axis=0; axis<3; ++axis)
            projected[basis] += velocity[axis] * *(float*)(wheel + 0x70 + basis*16 + axis*4);
        RumbleTelemetry("W,%.4f,%d,%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%d,%.5f,%.5f,%.5f,%.5f,%.5f\n",
          RumbleGetTime(), port, i, radius, angular, velocity[0], velocity[1], velocity[2],
          projected[0], projected[1], projected[2], *(int*)(wheel+0x10C), utilization, longitudinal, lateral, load, driveTorque);
      }
      float length = 0, rolling = 0;
      for (int axis=0; axis<3; ++axis) {
        if (!_finite(velocity[axis]) || !_finite(forward[axis])) return false;
        length += forward[axis]*forward[axis];
        rolling += velocity[axis]*forward[axis];
      }
      if (!(length > .8f && length < 1.2f)) return false;
      float speed = fabsf(rolling);
      if (speed > 200.0f) return false;
      if (speed < 1.0f && fabsf(angular)*radius < 1.0f) continue;
      float slip = (fabsf(angular)*radius - speed) / (speed > 3.0f ? speed : 3.0f);
      fastest = (std::max)(fastest, slip);
      lockup = (std::max)(lockup, -slip);
    }
    // Preserve single-wheel/front-wheel spin instead of diluting it by axle.
    // Weight the grip-limit cue by powered tyre load, but retain individual
    // wheelspin so a spinning wheel is never hidden by the other tyres.
    if (poweredLoad > 0) gasTraction /= poweredLoad;
    float wheelspin = RumbleRampValue(fastest, 0.04f, 0.5f);
    // Ease into rotational slip without weakening sustained heavy wheelspin.
    wheelspin *= .75f + .25f*wheelspin;
    // Lift subtle loaded-cornering detail while easing saturated launch cues.
    gasTraction = powf(RumbleClamp(gasTraction), .85f);
    gasTraction *= (1.40f - .50f*gasTraction) / .90f;
    spin = (std::max)(gasTraction, wheelspin);
    // Retain the strong brake cue, with a little of throttle's load averaging
    // to reduce changes caused by a single tyre reaching its traction limit.
    if (brakeLoad > 0) brakeTraction = .75f*brakeTraction + .25f*brakeWeighted/brakeLoad;
    // Ignore mild braking slip; reserve the brake cue for substantial lockup.
    lockup = (std::max)(brakeTraction, RumbleRampValue(lockup, 0.50f, 0.80f));
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
#endif

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

#ifdef GAME_UG
void __cdecl RumbleFrame() {
  // This call follows the game's inlined four-channel sampling loop. Do not
  // sample again: doing so would advance native curve ages twice per frame.
  // Preserve the separate original feedback/camera housekeeping exactly once.
  ((void(__cdecl*)())RUMBLE_UPDATE_ADDR)();
  RumbleLastFrame = RumbleGetTime();
  float dt = *(float*)RUMBLE_SIM_DT_ADDR;
  if (!RumbleCanPlay() || !(dt > 0 && dt <= 0.1f)) { RumbleStopAllPads(); return; }
  for (int port = 0; port < RUMBLE_PORT_COUNT; ++port) {
    RumblePort& state = RumblePorts[port];
    if (!g_Controllers[port].bConnected || !RumbleFeedbackEnabled(port)) {
      XINPUT_VIBRATION off = {};
      if (state.sent[0] || state.sent[1]) XInputSetState(port, &off);
      RumbleResetPort(port);
      continue;
    }
    int level = RumbleChannels(port)->output;
    state.wanted[0] = RumbleMixer::ToMotor(RumbleClamp(level / 255.0f), fRumbleLeftScale);
    // UG2-derived output convention; UG1 PS2 actuator packing is unverified.
    state.wanted[1] = RumbleMixer::ToMotor(level > 128 ? 1.0f : 0.0f, fRumbleRightScale);
    if (bRumbleSwapMotors) std::swap(state.wanted[0], state.wanted[1]);
    // No verified UG1 tyre telemetry: leave impulse triggers silent.
    state.wanted[2] = state.wanted[3] = 0;
    RumbleTelemetry("F,%.4f,%d,%d,%d,0,0\n", RumbleLastFrame, port, state.wanted[0], state.wanted[1]);
  }
  RumbleSendOutputs();
}

#else
// Native PC arguments are both channel 0's byte level. Match PS2 actuator
// packing: variable heavy motor, binary light motor strictly above 128.
void __cdecl RumbleNativeOutput(int port, int smallLevel, int largeLevel) {
  if ((unsigned)port >= RUMBLE_PORT_COUNT) return;
  RumblePort& state = RumblePorts[port];
  if (!g_Controllers[port].bConnected || !RumbleCanPlay()) {
    RumbleStopAllPads();
    return;
  } else {
    state.wanted[0] = RumbleMixer::ToMotor(RumbleClamp(largeLevel / 255.0f), fRumbleLeftScale);
    state.wanted[1] = RumbleMixer::ToMotor(smallLevel > 128 ? 1.0f : 0.0f, fRumbleRightScale);
    if (bRumbleSwapMotors) std::swap(state.wanted[0], state.wanted[1]);
    if (!smallLevel && !largeLevel) state.wanted[2] = state.wanted[3] = 0;
  }
  if (!bRumbleInFrame) RumbleSendOutputs();
}

int RumblePedalTrigger(float slip, float pedal, float dt, float& level) {
  if (RumbleTriggerMode == kTriggersOff || !(pedal > 0)) { level = 0; return 0; }
  float demand = RumbleClamp(slip);
  if (bRumbleTriggerPedalScaling) demand *= RumbleClamp(pedal);
  // A saturated tyre at partial pedal should not make the middle range harsh.
  // Soften combined demand gently, retaining full output at the limit.
  demand *= .75f + .25f*demand;
  float target = RumbleMixer::ToMotor(demand, fRumbleTriggerIntensity) / 65535.0f;
  // The native force solver can jump from grip to saturation in one tick.
  // Ease the pedal cue in; follow falling load faster and release immediately.
  float time = target > level ? .15f : .06f;
  level += (target-level) * (1.0f-expf(-dt/time));
  return (int)(RumbleClamp(level)*65535.0f);
}

void __cdecl RumbleFrame() {
  RumbleLastFrame = RumbleGetTime();
  float dt = *(float*)RUMBLE_SIM_DT_ADDR;
  if (!RumbleCanPlay() || !(dt > 0 && dt <= 0.1f)) { RumbleStopAllPads(); return; }
  for (int p = 0; p < RUMBLE_PORT_COUNT; ++p) {
    if (!g_Controllers[p].bConnected) {
      XINPUT_VIBRATION off = {};
      if (RumblePorts[p].sent[0] || RumblePorts[p].sent[1]) XInputSetState(p, &off);
      RumbleResetPort(p);
    }
  }
  bRumbleInFrame = true;
  ((void(__cdecl*)())RUMBLE_UPDATE_ADDR)();
  bRumbleInFrame = false;
  for (int p = 0; p < RUMBLE_PORT_COUNT; ++p) {
    RumblePort& state = RumblePorts[p];
    if (!g_Controllers[p].bConnected) { RumbleResetPort(p); continue; }
    float spin = 0, lockup = 0;
    // Proportional slip only, with the full trigger range available.
    // No shift, NOS, impact or synthesized trigger pulses.
    if (RumbleTriggerMode != kTriggersOff && !RumbleReadSlip(p, spin, lockup)) {
      spin = lockup = state.brakeLevel = state.gasLevel = 0;
    }
    float gas = RumblePedal(p, JOY_EVENT_THROTTLE, JOY_EVENT_THROTTLE_ANALOG, JOY_EVENT_THROTTLE_ANALOG_ALTERNATE);
    float brake = RumblePedal(p, JOY_EVENT_BRAKE, JOY_EVENT_BRAKE_ANALOG, JOY_EVENT_BRAKE_ANALOG_ALTERNATE);
    state.wanted[2] = RumblePedalTrigger(lockup, brake, dt, state.brakeLevel);
    state.wanted[3] = RumblePedalTrigger(spin, gas, dt, state.gasLevel);
    RumbleTelemetry("T,%.4f,%d,%.4f,%.4f,%.4f,%.4f\n", RumbleLastFrame, p, spin, lockup, gas, brake);
    RumbleTelemetry("F,%.4f,%d,%d,%d,%d,%d\n", RumbleLastFrame, p,
                    state.wanted[0], state.wanted[1], state.wanted[2], state.wanted[3]);
  }
  RumbleSendOutputs();
}
#endif

bool RumbleValidateHooks() {
  __try {
#ifdef GAME_UG
    const unsigned char event[] = {0x57,0x8B,0x3D,0x20,0xA9,0x77,0x00};
    const unsigned char update[] = {0xE8,0xAF,0xD4,0x12,0x00};
    const unsigned char setting[] = {0x0F,0x95,0xC3};
    const unsigned char collision[] = {0x8A,0x48,0x21,0x84,0xC9,0x74,0x15};
    const unsigned char auxiliary[] = {0x8A,0x46,0x21,0x84,0xC0,0x74,0x39};
    // Validate the loop that supplies the sampled levels read by our frame hook.
    const unsigned char channels[] = {
      0x8B,0x2D,0x38,0xAD,0x73,0x00,0xBF,0x58,0xB1,0x73,0x00,
      0xBE,0x02,0x00,0x00,0x00,0x55,0xE8,0x02,0xD3,0x12,0x00,
      0x83,0xC7,0x30,0x4E,0x75,0xF4,0x81,0xFF,0x18,0xB2,0x73,0x00,0x7C,0xE7
    };
    const unsigned char sampler[] = {0x8B,0x47,0x28,0x53,0x55,0x33,0xDB,0x56};
    const unsigned char admission[] = {0x83,0xEC,0x08,0x53,0x55,0x56,0x83,0xCE,0xFF};
    const unsigned char housekeeping[] = {0xA1,0xC8,0x45,0x73,0x00,0x85,0xC0,0x74,0x6D};
    if (memcmp((void*)RUMBLE_EVENT_ADDR,event,sizeof(event)) ||
        memcmp((void*)(RUMBLE_IMAGE_BASE+0x479D8),channels,sizeof(channels)) ||
        memcmp((void*)(RUMBLE_IMAGE_BASE+0x174CF0),sampler,sizeof(sampler)) ||
        memcmp((void*)(RUMBLE_IMAGE_BASE+0x174C20),admission,sizeof(admission)) ||
        memcmp((void*)RUMBLE_UPDATE_ADDR,housekeeping,sizeof(housekeeping)) ||
        memcmp((void*)RUMBLE_UPDATE_CALL,update,sizeof(update)) ||
        memcmp((void*)RUMBLE_SETTING_ADDR,setting,sizeof(setting)) ||
        memcmp((void*)(RUMBLE_COLLISION_SETTING_ADDR-5),collision,sizeof(collision)) ||
        memcmp((void*)(RUMBLE_AUX_SETTING_ADDR-5),auxiliary,sizeof(auxiliary))) return false;
#else
    const unsigned char event[] = {0x8B,0x44,0x24,0x04,0x56};
    const unsigned char update[] = {0xE8,0x5E,0x11,0xFF,0xFF};
    const unsigned char setting[] = {0x0F,0x95,0xC3};
    const unsigned char collisionSetting[] = {0xE8,0xFE,0x19,0x04,0x00,0x85,0xC0,0x74,0x13};
    const unsigned char package[] = {0xA1,0xD0,0x84,0x83,0x00};
    const unsigned char output[] = {0xC3,0x90,0x90,0x90,0x90};
    const unsigned char outputCall[] = {0xE8,0x4D,0xFC,0xE8,0xFF};
    const unsigned char stopCall[] = {0xE8,0xF6,0xFB,0xE8,0xFF};
    if (memcmp((void*)RUMBLE_OUTPUT_ADDR,output,sizeof(output)) ||
        memcmp((void*)RUMBLE_OUTPUT_CALL,outputCall,sizeof(outputCall)) ||
        memcmp((void*)RUMBLE_STOP_CALL,stopCall,sizeof(stopCall)) ||
        memcmp((void*)RUMBLE_EVENT_ADDR,event,sizeof(event)) ||
        memcmp((void*)RUMBLE_UPDATE_CALL,update,sizeof(update)) ||
        memcmp((void*)RUMBLE_SETTING_ADDR,setting,sizeof(setting)) ||
        memcmp((void*)(RUMBLE_COLLISION_SETTING_ADDR-7),collisionSetting,sizeof(collisionSetting)) ||
        memcmp((void*)RUMBLE_PACKAGE_ACTIVE_ADDR,package,sizeof(package))) return false;
#endif
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
  InputConfig ini("");
  RumbleLoadSettings([&](const char* key, float fallback) {
    return ini.ReadFloat("Rumble", const_cast<char*>(key), fallback);
  });
#ifdef GAME_UG
  // Ignore legacy trigger settings until UG1-specific tyre data is verified.
  RumbleTriggerMode = kTriggersOff;
#endif
  bRumbleTelemetry = ini.ReadInteger("Rumble", "Telemetry", ConfigDefaults::Rumble::Telemetry) != 0;
  if (!bRumbleEnabled) return;
  if (!RumbleValidateHooks()) {
    OutputDebugStringA("XtendedInput rumble: unsupported Underground executable or conflicting hook; rumble disabled.\n");
    RumbleTelemetry("Rumble disabled: unsupported executable or conflicting hooks.\n");
    return;
  }
#ifndef GAME_UG
  // 0x4022C0 is a shared empty function, also called without motor arguments
  // throughout the game. Redirect only the rumble frame and stop calls.
  injector::MakeCALL(RUMBLE_OUTPUT_CALL, RumbleNativeOutput, true);
  injector::MakeCALL(RUMBLE_STOP_CALL, RumbleNativeOutput, true);
#endif
  bRumbleNativeReady = true;
  for (int p = 0; p < RUMBLE_PORT_COUNT; ++p) RumbleResetPort(p);
  injector::MakeCALL(RUMBLE_UPDATE_CALL, RumbleFrame, true);
#ifndef GAME_UG
  // The UG2 INI owns the switch. Replace `setne bl` with `mov bl,1; nop` in the
  // car's feedback producer, without modifying the profile's vibration byte.
  injector::WriteMemory<unsigned short>(RUMBLE_SETTING_ADDR, 0x01B3, true);
  injector::WriteMemory<unsigned char>(RUMBLE_SETTING_ADDR+2, 0x90, true);
  // Collision production queries that same hidden byte through Player::GetFeedback.
  // Keep its call and other collision guards, bypass only the false-setting branch.
  injector::MakeNOP(RUMBLE_COLLISION_SETTING_ADDR, 2, true);
#endif
  bRumbleInstalled = true;
  RumbleTelemetry("Underground rumble initialized; impulse mode=%d\n", RumbleTriggerMode);
}


