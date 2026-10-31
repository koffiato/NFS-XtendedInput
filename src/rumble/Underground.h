// Controller rumble port from koffiato/NFS-XtendedInput (MIT).
// Original rumble: Copyright (c) 2026 Martin Penkava.
// U2 adapter uses native feedback events and wheel data; no MW addresses.
#pragma once
#include <cstdarg>
#include "Settings.h"
#include "Log.h"

// Underground 2 US 1.2. These sites are validated together before any rumble write.
// Event dispatcher: cdecl(type, port, amount). The original only queues console
// motor curves; steering-wheel forces have a separate path and remain intact.
// The test harness supplies its own image buffer; the plugin keeps U2's fixed base.
#ifndef RUMBLE_IMAGE_BASE
#define RUMBLE_IMAGE_BASE 0x00400000u
#endif
#ifdef GAME_UG
// Underground US 1.4. Its dispatcher uses EDX=event, ESI=port,
// [ESP+4]=amount; the adapter below preserves that native convention.
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
static_assert(sizeof(RumbleNativeEvent) == 16, "U2 feedback table layout");

bool bRumbleInstalled = false;
bool bRumbleTelemetry = false;
bool bRumbleOnWGI = false;
double RumbleLastFrame = 0.0;
float RumbleSimClock = 1.0f;
FILE* RumbleLog = NULL;
bool bRumbleLogTried = false;

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
#ifdef GAME_UG
  // The simulation delta is zero while paused. Its producer additionally
  // guards loading/cinematic states before it reaches our event hook.
  return *(float*)RUMBLE_SIM_DT_ADDR > 0.0f;
#else
  typedef bool(__cdecl* PackageActive)(const char*);
  PackageActive active = (PackageActive)RUMBLE_PACKAGE_ACTIVE_ADDR;
  return !active("UI_Pause.fng") && !active("UI_PauseOptions.fng") &&
         !active("UI_PauseOptionsMain.fng") && !active("UI_PC_Customize_Options.fng");
#endif
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
  #ifdef GAME_UG
  // UG retains native slip/yaw events; no U2 wheel offsets are used.
  return false;
  #else
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
#endif
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

#ifdef GAME_UG
void __declspec(naked) RumbleEventUG() {
  __asm {
    pushad
    push dword ptr [esp+36]
    push esi
    push dword ptr [edx]
    call RumbleEvent
    add esp,12
    popad
    ret
  }
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
    if (memcmp((void*)RUMBLE_EVENT_ADDR,event,sizeof(event)) ||
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
    if (memcmp((void*)RUMBLE_EVENT_ADDR,event,sizeof(event)) ||
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
  bRumbleTelemetry = ini.ReadInteger("Rumble", "Telemetry", 0) != 0;
  if (!bRumbleEnabled) return;
  if (!RumbleValidateHooks()) {
    OutputDebugStringA("XtendedInput rumble: unsupported U2 executable or conflicting hook; rumble disabled.\n");
    RumbleTelemetry("Rumble disabled: unsupported executable or conflicting hooks.\n");
    return;
  }
  for (int p = 0; p < RUMBLE_PORT_COUNT; ++p) RumbleResetPort(p);
#ifdef GAME_UG
  injector::MakeJMP(RUMBLE_EVENT_ADDR, RumbleEventUG, true);
  injector::MakeNOP(RUMBLE_AUX_SETTING_ADDR, 2, true);
#else
  injector::MakeJMP(RUMBLE_EVENT_ADDR, RumbleEvent, true);
#endif
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


