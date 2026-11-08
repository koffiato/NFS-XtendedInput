// Live MW/ProStreet vehicle interfaces and guarded reads. MIT; see LICENSE.
#pragma once
#include <cstdint>
#include <cstring>
#include <float.h>
#ifdef GAME_PROSTREET
#include "ProStreet.h"
#endif

// --- player vehicle access ---

void*(__thiscall* UTL_Com_Object_IList_Find)(void* thethis, void* handle) = (void*(__thiscall*)(void*, void*))UTL_ILIST_FIND_ADDR;
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
  float fSlip[RUMBLE_MAX_WHEELS];         // MW: m/s; PS: slip ratio. + wheelspin, - braking
  float fSkid[RUMBLE_MAX_WHEELS];         // MW: lateral m/s; PS: slip angle, radians
  float fWheelAV[RUMBLE_MAX_WHEELS];      // MW: rad/s; unused for PS
#ifdef GAME_PROSTREET
  float fNativeRoad[RUMBLE_MAX_WHEELS];   // raw slot-0x7C road signal, not wheel angular velocity
#endif
  float fLinVel[3];                       // world m/s
  float fAngVel[3];                       // world rad/s
};

uintptr_t RumbleGameAddress(uintptr_t address) {
#ifdef GAME_PROSTREET
  return RumblePSAddress(address);
#else
  return address;
#endif
}

// the local player's ISimable, or NULL outside a race / while the player list is empty or being rebuilt
void* RumbleGetPlayerSimable() {
  if (*(int*)RumbleGameAddress(GAMEFLOWMANAGER_STATUS_ADDR) != 6) return NULL;
  void** players = *(void***)RumbleGameAddress(PLAYER_ILISTABLE_ADDR);
  if (!players || *(int*)RumbleGameAddress(PLAYER_ILISTABLE_COUNT_ADDR) <= 0 || !players[0]) return NULL;
  return RumbleVCall<void*>(players[0], 0x04);  // IPlayer::GetSimable
}

bool RumbleReadVehicleUnsafe(RumbleVehicleState& s) {
  void* simable = RumbleGetPlayerSimable();
  if (!simable) return false;

  void* susp  = RumbleFindInterface(simable, ISUSPENSION_IHANDLE_ADDR);
  void* body  = RumbleFindInterface(simable, IRIGIDBODY_IHANDLE_ADDR);
  void* input = RumbleFindInterface(simable, IINPUT_IHANDLE_ADDR);
#ifdef GAME_PROSTREET
  if (!susp || !body || !input) return false;
  s.fSpeed     = RumbleVCall<float>(body, 0x2C); // IRigidBody::GetSpeed
#else
  void* veh   = RumbleFindInterface(simable, IVEHICLE_IHANDLE_ADDR);
  if (!susp || !body || !veh || !input) return false;

  s.fSpeed           = RumbleVCall<float>(veh, IVEHICLE_GET_ABSOLUTE_SPEED_OFFSET);  // IVehicle::GetAbsoluteSpeed
#endif
#ifdef GAME_MW
  const char* ctrl   = RumbleVCall<const char*>(input, 0x08);  // IInput::GetControls
  s.fGas             = *(const float*)(ctrl + 0x14);
  s.fBrake           = *(const float*)(ctrl + 0x18);
  s.fHandBrake       = *(const float*)(ctrl + 0x1C);
#else
  s.fGas       = RumbleVCall<float>(input, 0x0C);
  s.fBrake     = RumbleVCall<float>(input, 0x08);
#ifdef GAME_CARBON
  s.fHandBrake = RumbleVCall<float>(input, 0x1C);  // 0x20 is GetControlClutch
#else
  s.fHandBrake = RumbleVCall<float>(input, 0x20);
#endif
#endif
  const float* lin   = RumbleVCall<const float*>(body, 0x24);  // IRigidBody::GetLinearVelocity
  const float* ang   = RumbleVCall<const float*>(body, 0x28);  // IRigidBody::GetAngularVelocity
  memcpy(s.fLinVel, lin, sizeof(s.fLinVel));
  memcpy(s.fAngVel, ang, sizeof(s.fAngVel));

#ifdef GAME_PROSTREET
  s.nWheels = RumbleVCall<int>(susp, 0x14); // same suspension interface used by PS DoFFB
#else
  s.nWheels = RumbleVCall<int>(susp, 0x08);
#endif
  if (s.nWheels < 0) return false;
  if (s.nWheels > RUMBLE_MAX_WHEELS) s.nWheels = RUMBLE_MAX_WHEELS;
  for (unsigned int i = 0; i < (unsigned int)s.nWheels; i++) {
#ifdef GAME_PROSTREET
    s.bOnGround[i] = RumbleVCall<bool>(susp, 0x34, i);
    s.fCompression[i] = 0.0f; // PS suspension layout differs; no guessed compression offset
    s.fSlip[i] = RumbleVCall<float>(susp, 0x0C, i);
    s.fSkid[i] = RumbleVCall<float>(susp, 0x08, i);
    s.fWheelAV[i] = 0.0f;
    s.fNativeRoad[i] = RumbleVCall<float>(susp, 0x7C, i);
#else
    s.bOnGround[i]    = RumbleVCall<bool>(susp, 0x24, i);
    s.fCompression[i] = RumbleVCall<float>(susp, 0x28, i);
    s.fSlip[i]        = RumbleVCall<float>(susp, 0x30, i);
    s.fSkid[i]        = RumbleVCall<float>(susp, 0x38, i);
    s.fWheelAV[i]     = RumbleVCall<float>(susp, 0x50, i);
#endif
  }
  return true;
}

// the vehicle path is reverse engineered, so a wrong guess must not take the game down
bool RumbleReadVehicle(RumbleVehicleState& s) {
  __try {
    if (!RumbleReadVehicleUnsafe(s)) return false;
    if (!_finite(s.fSpeed) || !_finite(s.fGas) || !_finite(s.fBrake) || !_finite(s.fHandBrake)) return false;
    for (int i = 0; i < s.nWheels; ++i) {
      if (!_finite(s.fSlip[i]) || !_finite(s.fSkid[i]) || !_finite(s.fWheelAV[i]) || !_finite(s.fCompression[i])) return false;
#ifdef GAME_PROSTREET
      if (!_finite(s.fNativeRoad[i])) return false;
#endif
    }
    return true;
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

#ifdef GAME_PROSTREET
bool RumblePSIsShifting() {
  __try {
    void* simable = RumbleGetPlayerSimable();
    void* transmission = simable ? RumbleFindInterface(simable, ITRANSMISSION_IHANDLE_ADDR) : NULL;
    return transmission && RumbleVCall<bool>(transmission, 0x14); // IsGearChanging
  } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
#endif

#ifdef HAS_CONSOLE_RUMBLE
// Resolve the contact participants, including traffic/props and participant B.
// ISimable::GetLinearVelocity fills a vector through vtable slot 26 on PC.
void*(__cdecl* RumbleMWResolveSimable)(void*) = (void*(__cdecl*)(void*))SIM_GETSIMABLE_ADDR;
void* RumbleMWCollisionBody(void* handle, float* velocity) {
  __try {
    void* simable = RumbleMWResolveSimable(handle);
    if (!simable) return NULL;
    RumbleVCall<void>(simable, 0x68, velocity);
    for (int i = 0; i < 3; ++i) if (!_finite(velocity[i])) return NULL;
    return simable;
  } __except (EXCEPTION_EXECUTE_HANDLER) { return NULL; }
}
bool RumbleMWCollisionVehicle(void* simable) {
  __try { return RumbleFindInterface(simable, IVEHICLE_IHANDLE_ADDR) != NULL; }
  __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
#ifdef GAME_CARBON
const float* RumbleSimTimePtr = (const float*)SIM_TIME_ADDR;
float RumbleSimTime() {
  __try { return RumbleSimTimePtr && _finite(*RumbleSimTimePtr) ? *RumbleSimTimePtr : 0.0f; }
  __except (EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
}
#endif
#endif

