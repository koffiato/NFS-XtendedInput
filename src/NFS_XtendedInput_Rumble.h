/*
// clang-format off
//
//    MIT License
//    Need for Speed (Black Box, MW & newer) - Xtended Input plugin
//    Bringing native XInput to NFS
//
//    Rumble: Copyright (c) 2026 Martin Penkava
//
// clang-format on
*/

// Shared MW / Carbon / ProStreet / Undercover feedback implementation.
//
// The game asks InputDevice::GetInterfaces for IFeedback, then supplies native
// feedback events and collisions (MW DoFFB @ 0x6EE490; CB @ 0x762000; PS @ 0x4D0CF0).
// PC controller effect methods are stubs. This implementation drives XInput
// motors using recovered console controller rules. Physical ProStreet parity
// still requires controller comparison; its surface records come from PC.
//
// The game never stops effects on focus loss and keeps a raw pointer to the interface, so:
// - the object is embedded in InputDevice (same place as the game's own device, +0x2C) and is never deleted
// - motors are zeroed on pause, focus loss, disconnect, missed ticks (watchdog) and DLL unload

#pragma once

#include "rumble/Settings.h"
#include "rumble/MostWanted.h"
#ifdef GAME_PROSTREET
#include "rumble/ProStreet.h"
#endif

// pad that last produced input (used in omni mode, where both game ports read every pad)
int  RumbleLastActivePad = 0;
bool bRumbleAppActive    = true;
// bumped whenever all pads are force-stopped, so every feedback object resyncs its motor state
unsigned int RumbleStopGeneration = 0;

void(__thiscall* UTL_Com_Object_IList_Add)(void* thethis, void* handle, void* iface) = (void(__thiscall*)(void*, void*, void*))UTL_ILIST_ADD_ADDR;

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
#ifdef GAME_UC
  // Bypass collision gatekeeper jump: cmp byte ptr [eax+9], 0; je 0x007BAE26
  if (*(uint16_t*)PLAYER_COLLISION_RUMBLE_PATCH_ADDR == 0x840F)
    injector::MakeNOP(PLAYER_COLLISION_RUMBLE_PATCH_ADDR, 6, true);
#elif defined(GAME_PROSTREET)
  if (!RumbleValidateProStreet()) {
    bRumbleEnabled = false;
    OutputDebugStringA("[XtendedInput] ProStreet rumble disabled: unsupported feedback interface.\n");
    return;
  }
  // The stock producer drops contacts before calling IFeedback (including
  // contacts where the player is B). Route both local-player listeners through
  // the adapter; foreign feedback interfaces still call the original producer.
  injector::WriteMemory<uintptr_t>(0x009FA07C, (uintptr_t)&RumblePSCollisionHook, true);
  injector::WriteMemory<uintptr_t>(0x009FA324, (uintptr_t)&RumblePSCollisionHook, true);
#else
  // LocalPlayer::OnCollision skips ReportCollision when the save's hidden Rumble byte is 0 (`je` at PLAYER_COLLISION_RUMBLE_PATCH_ADDR).
  // The ini is the only switch, so drop that jump; the null-settings check before it stays.
  if (*(uint16_t*)PLAYER_COLLISION_RUMBLE_PATCH_ADDR == 0x2B74)
    injector::WriteMemory<uint16_t>(PLAYER_COLLISION_RUMBLE_PATCH_ADDR, 0x9090, true);
#endif
}

#include "rumble/Vehicle.h"
#include "rumble/Surface.h"
#include "rumble/Telemetry.h"

#ifdef GAME_PROSTREET
// Trigger normalization estimates from PC records/logs, not recovered console curves.
// PS uses ratios/angles, not MW's m/s:
// FFB_SLIP spans .05..1 and FFB_SKID spans .1..1 on the captured tarmac surface.
const float RUMBLE_LOCKUP_MIN_SLIP = 0.05f;
const float RUMBLE_LOCKUP_FULL_SLIP = 1.0f;
const float RUMBLE_SPIN_MIN_SLIP = 0.05f;
const float RUMBLE_SPIN_FULL_SLIP = 1.0f;
const float RUMBLE_SLIDE_MIN_SKID = 0.1f;
const float RUMBLE_SLIDE_FULL_SKID = 1.0f;
#else
const float RUMBLE_LOCKUP_MIN_SLIP    = 3.0f;   // m/s of braking slip
const float RUMBLE_LOCKUP_FULL_SLIP   = 15.0f;
const float RUMBLE_SPIN_MIN_SLIP      = 3.0f;   // m/s of wheelspin
const float RUMBLE_SPIN_FULL_SLIP     = 20.0f;
const float RUMBLE_SLIDE_MIN_SKID     = 2.0f;   // m/s of lateral tyre speed
const float RUMBLE_SLIDE_FULL_SKID    = 20.0f;
#endif

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

#ifdef GAME_UC
// OnCollision builds this vector on the stack at 0x7BADF7 before pushing
// iamA and CollisionInfo*. Native slot 19 returns with ret 0x14 (0x696E2F).
struct RumbleUCVector { float x, y, z; };
static_assert(sizeof(RumbleUCVector) == 12, "UC collision argument size");
#endif

// Native IFeedback: MW has 15 slots (0x8A7BD4); CB has 18 (0x9E2188);
// PS and UC have 21 (0x98BE6C / 0xC0D434). Empty virtuals preserve these ABIs.
// packed to 4 so the double member doesn't push the object off +0x2C inside InputDevice
#pragma pack(push, 4)
#pragma warning(push)
#pragma warning(disable : 4121)  // intentional, see above
class IFeedback;
#ifdef GAME_PROSTREET
IFeedback* RumblePSDevices[MAX_CONTROLLERS] = {};
#endif
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
  bool         mOnWGI;       // last output went through Windows.Gaming.Input (else XInput)
  RumbleMixer  mMixer;
  MWEffects mMW;

  // Callback values for telemetry; MW event edges live in mMW.
  bool           mShifting;
  bool           mEngineBlown;
  ShiftPotential mShiftPotential;

  // surface and tyres
  RumbleVehicleState mVehicle;
  bool               mHasVehicle;
  RumbleWave         mRoadWave;
  RumbleWave         mSkidWave;
  RumbleWave         mSlipWave;
  float              mWheelSkid[RUMBLE_MAX_WHEELS];  // from DoFFB, |lateral m/s|
  int                mWheelArg;                      // wheel DoFFB is reporting
  // Optional trigger extension: proportional braking/throttle slip and car ramps.
  float              mLockupLevel;  // 0-1, this tick
  float              mSpinLevel;
  float              mSlideLevel;

  // impulse triggers (0-65535 like the motors), last sent values
  int mLastTriggerLeft;
  int mLastTriggerRight;

  void Init(void* ownerList, int deviceIndex) {
#ifdef GAME_PROSTREET
    if (deviceIndex >= 0 && deviceIndex < MAX_CONTROLLERS) RumblePSDevices[deviceIndex] = this;
#endif
    mOwnerList      = ownerList;
    mDeviceIndex    = deviceIndex;
    mPad            = deviceIndex;
    mLastLeft       = 0;
    mLastRight      = 0;
    mLastTriggerLeft  = 0;
    mLastTriggerRight = 0;
    mPaused         = true;
    mNOSEngaged     = false;
    mLastUpdateTime = RumbleGetTime();
    mStopGeneration = RumbleStopGeneration;
    mOnWGI          = false;
    memset(&mMixer, 0, sizeof(RumbleMixer));
    mVehicle = {};
    mRoadWave = {};
    mSkidWave = {};
    mSlipWave = {};
    ClearEffects();
  }

  void Detach() {
    if (bRumbleEnabled) Stop();
#ifdef GAME_PROSTREET
    if (mDeviceIndex >= 0 && mDeviceIndex < MAX_CONTROLLERS && RumblePSDevices[mDeviceIndex] == this)
      RumblePSDevices[mDeviceIndex] = NULL;
#endif
  }

  void ClearEffects() {
    mMW = {};
    mLockupLevel           = 0.0f;
    mSpinLevel             = 0.0f;
    mSlideLevel            = 0.0f;
    mHasVehicle            = false;
    mWheelArg = 0;
    memset(mWheelSkid, 0, sizeof(mWheelSkid));
    mRoadWave.Begin();
    mSkidWave.Begin();
    mSlipWave.Begin();
    mNOSEngaged         = false;
    mShifting           = false;
    mEngineBlown        = false;
    mShiftPotential     = SHIFT_POTENTIAL_NONE;
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

    if (useWGI) {
      triggerLeft  = RumbleRoundToPercent(triggerLeft);
      triggerRight = RumbleRoundToPercent(triggerRight);
      if (left == mLastLeft && right == mLastRight && triggerLeft == mLastTriggerLeft && triggerRight == mLastTriggerRight) return;
      RumbleWGI::Send(left / 65535.0, right / 65535.0, triggerLeft / 65535.0, triggerRight / 65535.0);
      RumbleTelemetryOutput(-1, left, right, triggerLeft, triggerRight, "WGI", -1);
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
    DWORD result = XInputSetState(mPad, &vib);
    RumbleTelemetryOutput(mPad, left, right, 0, 0, "XInput", (long)result);
    if (result == ERROR_SUCCESS) {
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

  // never deleted: it's embedded in InputDevice
  virtual void* ScalarDtor(unsigned int /*flags*/) { return this; }

  bool HasOutput() {
    int pad = bXInputOmniMode ? RumbleLastActivePad : mDeviceIndex;
    return RumbleWGI::ShouldUse() || (pad >= 0 && pad < MAX_CONTROLLERS && g_Controllers[pad].bConnected);
  }

  void PushMW(int leftTrigger = 0, int rightTrigger = 0) {
    if (mPaused || !bRumbleAppActive || !bRumbleEnabled || !HasOutput()) Send(0, 0);
    else Send(mMW.outputLeft, mMW.outputRight, leftTrigger, rightTrigger);
  }

  virtual void PauseEffects() {
    mPaused = true;
    PushMW();
  }
  virtual void ResumeEffects() {
    mPaused = false;
    PushMW(mLastTriggerLeft, mLastTriggerRight);
  }
  virtual void ResetEffects() {
    if (!HasOutput()) return;
    mPaused = true;
    mMW.collision = {};
    mMW.outputLeft = mMW.outputRight = 0;
    PushMW();
  }

  virtual void BeginUpdate() {
#ifdef GAME_PROSTREET
    ++RumbleTelemetry.nFeedbackFrame;
    mNOSEngaged     = false;
    mShifting       = false;
    mEngineBlown    = false;
    mShiftPotential = SHIFT_POTENTIAL_NONE;
#endif
#if defined(GAME_CARBON) || defined(GAME_UC)
    mMW.Begin();
#endif
    mWheelArg       = 0;
    for (int i = 0; i < RUMBLE_MAX_WHEELS; i++) mWheelSkid[i] = 0.0f;
    mRoadWave.Begin();
    mSkidWave.Begin();
    mSlipWave.Begin();
    RumbleTelemetry.nWheel = 0;
  }

  // Native tyre waves for body rumble; signed slip estimates for optional triggers.
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

    // Use the game's FFB_SLIP / FFB_SKID waves for body rumble. The signed
    // live slip above only distinguishes brake/throttle effects on triggers.
    float slipLeft, slipRight, skidLeft, skidRight;
    mSlipWave.Sample(dt, slipLeft, slipRight);
    mSkidWave.Sample(dt, skidLeft, skidRight);
    float skid = 0;
    for (int i = 0; i < RUMBLE_MAX_WHEELS; ++i)
      skid = (std::max)(skid, mWheelSkid[i] * (i < 2 ? 0.25f : 1.0f));
#ifdef GAME_PROSTREET
    // PS slip angles can be pi at rest; this is not a moving slide.
    if (!mHasVehicle || mVehicle.fSpeed < 0.5f) skid = 0;
#endif
    float slide = RumbleRampValue(skid, RUMBLE_SLIDE_MIN_SKID, RUMBLE_SLIDE_FULL_SKID);
    mMixer.Add(kRumbleTyres, (std::max)(slipLeft, skidLeft), (std::max)(slipRight, skidRight));

    mLockupLevel = lockup;
    mSpinLevel   = spin;
    mSlideLevel  = slide;
  }

  // Trigger motors are an extension, not a recovered console effect. Use
  // proportional live signals and the body car ramps without added oscillators.
  static float PedalGain(float pedal) {
    pedal = RumbleClamp(pedal);
    return bRumbleTriggerPedalScaling ? pedal : (pedal > 0 ? 1.0f : 0.0f);
  }

  void MixTriggers(float& leftTrigger, float& rightTrigger) {
    float gas = mHasVehicle ? PedalGain(mVehicle.fGas) : 0;
    float brake = mHasVehicle ? PedalGain(mVehicle.fBrake) : 0;
    float tyres = fRumbleCategoryScale[kRumbleTyres];
    float car = fRumbleCategoryScale[kRumbleCar];
    leftTrigger = mLockupLevel * brake * tyres;
    rightTrigger = (std::max)(mSpinLevel, mSlideLevel) * gas * tyres;
    float nos = mMW.nos.remaining > 0 ? mMW.nos.floor + (mMW.nos.amplitude-mMW.nos.floor)*mMW.nos.remaining/mMW.nos.duration : 0;
    rightTrigger = (std::max)(rightTrigger, nos * gas * car);
    rightTrigger = (std::max)(rightTrigger, (mMW.shiftLeft.remaining > 0 ? 0.4f : 0) * car);
    leftTrigger *= fRumbleTriggerIntensity;
    rightTrigger *= fRumbleTriggerIntensity;
  }

  virtual void EndUpdate() {
#ifndef GAME_PROSTREET
    mMW.feedbackTime = RumbleGetTime();
#endif
  }
  void PollEffects() { MixFrame(); }

  void MixFrame() {
    if (!bRumbleEnabled) return;  // GetInterfaces hides the object when disabled

    double now = RumbleGetTime();
    float dt = (float)(now - mLastUpdateTime);
    mLastUpdateTime = now;
    // Keep the poll clock moving while disabled, without consuming effect time.
    if (mPaused || !bRumbleAppActive) {
      Send(0, 0);
      return;
    }
#ifndef GAME_PROSTREET
    if (now - mMW.feedbackTime > 0.25) {
      Send(0, 0);
      return;
    }
#endif
    // Console Push precedes Run; a nonpositive delta does not sample state.
    PushMW(mLastTriggerLeft, mLastTriggerRight);
    if (!(dt > 0)) return;
    mHasVehicle = (bRumbleTelemetry || mOnWGI) && RumbleReadVehicle(mVehicle);
    mMixer.Begin();
    mMW.Mix(dt, mMixer);
    float roadLeft, roadRight;
    mRoadWave.Sample(dt, roadLeft, roadRight);
#ifndef GAME_PROSTREET
    mMixer.Add(kRumbleSurface, roadLeft, roadRight);
#endif
    MixTyres(dt);
#ifdef GAME_CARBON
    mMixer.fMotor[0] *= mMW.draftScale;
    mMixer.fMotor[1] *= mMW.draftScale;
#endif
    int left, right;
    mMixer.Resolve(left, right);
#ifdef GAME_PROSTREET
    if (bRumbleTelemetry) {
      if (!RumbleTelemetry.fStartTime) RumbleTelemetry.fStartTime = now;
      RumbleTelemetryPrintf("M,%.6f,%d,%u,%.9g,%.9g,%.9g,%.9g,%.9g\n",now-RumbleTelemetry.fStartTime,
        mDeviceIndex,RumbleTelemetry.nFeedbackFrame,dt,roadLeft,roadRight,mMixer.fMotor[0],mMixer.fMotor[1]);
    }
#endif
    if (bRumbleTelemetry)
      RumbleTelemetryWriteTick(now, mHasVehicle ? &mVehicle : NULL, mMW.nosOn, mMW.shifting, (ShiftPotential)mMW.potential);
    // Trigger output is an optional extension; body output remains one poll late.
    if (mOnWGI) {
      float leftTrigger = 0, rightTrigger = 0;
      MixTriggers(leftTrigger, rightTrigger);
      PushMW(RumbleMixer::ToMotor(leftTrigger, 1), RumbleMixer::ToMotor(rightTrigger, 1));
    }
    mMW.outputLeft = left;
    mMW.outputRight = right;
  }

  // Slot 6 reports road feedback in MW/CB/UC; ProStreet uses slot 18.
#ifndef GAME_PROSTREET
  virtual void UpdateRoadNoise(bool /*front*/, const SimSurface& surface, float speed) {
    if (!mPaused) mRoadWave.Add(&surface, ATTRIB_KEY_FFB_ROLL, speed);
    if (bRumbleTelemetry && RumbleTelemetry.nWheel < RUMBLE_MAX_WHEELS)
      RumbleTelemetry.iSurface[RumbleTelemetry.nWheel] = RumbleTelemetrySurfaceId(&surface);
    mWheelArg++;
    RumbleTelemetry.nWheel++;
  }
#else
  virtual void UpdateRoadRoll(bool front, const SimSurface& surface, float speed) {
    if (!mPaused) mRoadWave.Add(&surface, ATTRIB_KEY_FFB_ROLL, speed);
    RumbleTelemetryPSCallback(mDeviceIndex,6,mWheelArg,&surface,speed,(float)front,0,0,speed);
  }
#endif
#ifdef GAME_PROSTREET
  virtual void UpdateTireSkid(const SimSurface& surface, float skid, float range, float threshold, float load) {
    if (mPaused || !_finite(skid) || !_finite(range) || !_finite(threshold) || !_finite(load)) return;
    float input = RumblePSSkid(skid,range,threshold,load);
    mSkidWave.Add(&surface, ATTRIB_KEY_FFB_SKID, input);
    RecordSkid(skid);
    RumbleTelemetryPSCallback(mDeviceIndex,7,mWheelArg,&surface,skid,range,threshold,load,input);
  }
  virtual void UpdateTireSlip(const SimSurface& surface, float slip, float grip) {
    if (mPaused || !_finite(slip) || !_finite(grip)) return;
    float input = RumblePSSlip(slip,grip);
    mSlipWave.Add(&surface, ATTRIB_KEY_FFB_SLIP, input);
    RecordSlip(slip);
    RumbleTelemetryPSCallback(mDeviceIndex,8,mWheelArg,&surface,slip,grip,0,0,input);
  }
#elif defined(GAME_UC)
  virtual void UpdateTireSkid(const SimSurface& surface, float skid, float /*load*/, float /*grip*/, float /*totalLoad*/) {
    AddSkid(mWheelArg < 2, surface, fabsf(skid));
  }
  virtual void UpdateTireSlip(const SimSurface& surface, float slip, float /*grip*/) {
    AddSlip(surface, slip);
  }
#else
  virtual void UpdateTireSkid(bool front, const SimSurface& surface, float skid) {
    AddSkid(front, surface, skid);
  }
  virtual void UpdateTireSlip(bool /*front*/, const SimSurface& surface, float slip) {
    AddSlip(surface, slip);
  }
#endif

  void AddSlip(const SimSurface& surface, float slip) {
    if (!_finite(slip)) return;
    if (mPaused) return;
    mSlipWave.Add(&surface, ATTRIB_KEY_FFB_SLIP, slip);
    RecordSlip(slip);
  }

  void RecordSlip(float slip) {
    if (mWheelArg >= 0 && mWheelArg < RUMBLE_MAX_WHEELS) RumbleTelemetry.fSlip[mWheelArg] = slip;
  }

  void AddSkid(bool front, const SimSurface& surface, float skid) {
    if (!_finite(skid)) return;
    if (mPaused) return;
    mSkidWave.Add(&surface, ATTRIB_KEY_FFB_SKID, front ? skid * 0.25f : skid);  // Xbox MW damps the front wheels
    RecordSkid(skid);
  }

  void RecordSkid(float skid) {
    if (mWheelArg >= 0 && mWheelArg < RUMBLE_MAX_WHEELS) {
      mWheelSkid[mWheelArg] = skid;
      RumbleTelemetry.fSkid[mWheelArg] = skid;
    }
  }
#ifdef GAME_UC
  virtual void UpdateRPM(float throttle, float powerband, float /*rpm*/) {
    RumbleTelemetry.fThrottle  = throttle;
    RumbleTelemetry.fPowerband = powerband;
  }
#else
  virtual void UpdateRPM(float powerband, float /*overrev*/, float throttle) {
    RumbleTelemetry.fPowerband = powerband;
    RumbleTelemetry.fThrottle  = throttle;
  }
#endif
  virtual void UpdateShiftPotential(ShiftPotential potential) {
    mShiftPotential = potential;
    if (!mPaused) mMW.Potential((int)potential);
  }
  virtual void UpdateNOS(bool engaged, float /*nosLevel*/) {
    mNOSEngaged = engaged;
    if (!mPaused) mMW.NOS(engaged);
  }
  virtual void UpdateEngineBlown(bool blown) {
    mEngineBlown = blown;
    if (!mPaused) mMW.Engine(blown);
  }
  virtual void UpdateShifting(bool shifting) {
    mShifting = shifting;
    if (!mPaused) mMW.Shift(shifting && bRumbleGearShift);
  }

#ifdef GAME_CARBON
  virtual void UpdateDrafting(const void* draftInfo) {
    __try { mMW.Draft(draftInfo, RumbleSimTime(), mPaused); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
  }
  virtual void UpdateDrift(float slipAngle) {
    mMW.Drift(slipAngle, RumbleSimTime());
  }
  virtual void UpdateSteering(float /*steering*/) {}
#endif

#ifdef GAME_UC
  virtual void UpdateDriftSlip(bool active, float slipAngle, float /*lateralSlip*/) {
    if (active) mMW.Drift(slipAngle, RumbleSimTime());
  }
  virtual void ReservedUC15(uintptr_t, uintptr_t, uintptr_t) {}
  virtual void UpdateDriftSkid(const SimSurface& surface, float skid, float /*load*/, float /*grip*/) {
    AddSkid(mWheelArg < 2, surface, fabsf(skid));
  }
  virtual void UpdateSteering(float /*steering*/) {}
  virtual void UpdateRoadWithIndex(unsigned int wheel, float /*bumpScalar*/, const SimSurface& surface) {
    mWheelArg = wheel < RUMBLE_MAX_WHEELS ? (int)wheel : RUMBLE_MAX_WHEELS;
    if (mWheelArg == RUMBLE_MAX_WHEELS) return;
    if (mHasVehicle) mRoadWave.Add(&surface, ATTRIB_KEY_FFB_ROLL, mVehicle.fSpeed);
    if (bRumbleTelemetry) RumbleTelemetry.iSurface[wheel] = RumbleTelemetrySurfaceId(&surface);
    if (RumbleTelemetry.nWheel < (int)wheel+1) RumbleTelemetry.nWheel = wheel+1;
  }
#endif

#ifdef GAME_PROSTREET
  virtual void ReservedPS14(uintptr_t) {}
  virtual void UpdateDriftSlip(const SimSurface& surface, float slip, float grip) {
    if (mPaused || !_finite(slip) || !_finite(grip)) return;
    float input = RumblePSDriftSlip(slip,grip);
    mSlipWave.Add(&surface, ATTRIB_KEY_FFB_SLIP, input);
    RecordSlip(slip);
    RumbleTelemetryPSCallback(mDeviceIndex,15,mWheelArg,&surface,slip,grip,0,0,input);
  }
  virtual void UpdateDriftSkid(const SimSurface& surface, float skid, float range, float threshold) {
    if (mPaused || !_finite(skid) || !_finite(range) || !_finite(threshold)) return;
    float input = RumblePSDriftSkid(skid,range,threshold);
    mSkidWave.Add(&surface, ATTRIB_KEY_FFB_DRIFT_SKID, input);
    RecordSkid(skid);
    RumbleTelemetryPSCallback(mDeviceIndex,16,mWheelArg,&surface,skid,range,threshold,0,input);
  }
  virtual void UpdateSteering(float /*steering*/) {} // wheel force, not controller rumble
  virtual void UpdateRoadNoise(unsigned int wheel, float nativeRoad, const SimSurface& surface) {
    mWheelArg = wheel < RUMBLE_MAX_WHEELS ? (int)wheel : RUMBLE_MAX_WHEELS;
    if (mWheelArg == RUMBLE_MAX_WHEELS) return;
    float threshold, gain, amplitude = 0;
    // Xbox 0x825AAA18: strict threshold, then surface gain * road signal.
    // A later wheel on the same side replaces the ramp, even if weaker.
    if (!mPaused && _finite(nativeRoad) &&
        RumbleReadSurfaceFloat(&surface, ATTRIB_KEY_FFB_BUMP_THRESHOLD, threshold) && nativeRoad > threshold &&
        RumbleReadSurfaceFloat(&surface, ATTRIB_KEY_FFB_BUMP_GAIN, gain)) {
      MWRamp& bump = (wheel & 1) ? mMW.bumpRight : mMW.bumpLeft;
      amplitude = RumbleClamp(gain * nativeRoad);
      bump.Start(amplitude, 0.1f, 0.3f);
    }
    RumbleTelemetryPSCallback(mDeviceIndex,18,mWheelArg,&surface,nativeRoad,0,0,0,amplitude);
    if (bRumbleTelemetry) RumbleTelemetry.iSurface[wheel] = RumbleTelemetrySurfaceId(&surface);
    if (RumbleTelemetry.nWheel < (int)wheel+1) RumbleTelemetry.nWheel = wheel+1;
  }
#endif

  static float Length(const float* v) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
  static float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

#if defined(GAME_PROSTREET)
  virtual void ReportCollision(const COLLISION_INFO& cinfo, bool iamA, RumblePSVector /*velocityChange*/) {
#elif defined(GAME_UC)
  virtual void ReportCollision(const COLLISION_INFO& cinfo, bool iamA, RumbleUCVector /*velocityChange*/) {
#else
  virtual void ReportCollision(const COLLISION_INFO& cinfo, bool iamA) {
#endif
    if (!bRumbleEnabled || mPaused) return;
    float ownVelocity[3], otherVelocity[3];
    void* own = RumbleMWCollisionBody(iamA ? cinfo.objA : cinfo.objB, ownVelocity);
    if (!own) return;
    void* other = RumbleMWCollisionBody(iamA ? cinfo.objB : cinfo.objA, otherVelocity);
    const float* before = iamA ? cinfo.objAVel : cinfo.objBVel;
    float delta[3] = {ownVelocity[0]-before[0], ownVelocity[1]-before[1], ownVelocity[2]-before[2]};
    float dv = Length(delta), strength = 0;
    switch (cinfo.flags & 7) {
      case 2: strength = Clamp01(dv / 30); break;
      case 3: strength = Clamp01((dv - 1) / 3); break;
      case 1:
        if (dv >= 5) strength = Clamp01((dv - 5) / 20);
        else {
          const float* otherBefore = iamA ? cinfo.objBVel : cinfo.objAVel;
          float ownSpeed = Length(before), otherSpeed = Length(otherBefore);
          bool rearEnd = false;
          if (other && RumbleMWCollisionVehicle(other) && ownSpeed > 0 && otherSpeed > 0) {
            float dot = (before[0]*otherBefore[0] + before[1]*otherBefore[1] + before[2]*otherBefore[2]) / (ownSpeed*otherSpeed);
            rearEnd = dot > 0.1f;
          }
          // Retail code reads A's stored speed twice, including when we are B.
          float rearDelta = Length(cinfo.objAVel) * RUMBLE_REAR_END_VELOCITY_SCALE;
          if (rearEnd && rearDelta > 5) strength = Clamp01((rearDelta - 5) / 20);
          else if (other) {
            float otherDelta[3] = {otherVelocity[0]-before[0], otherVelocity[1]-before[1], otherVelocity[2]-before[2]};
            float liveSpeed = Length(ownVelocity);
            if (liveSpeed > 2 && Length(otherDelta) > 2) strength = Clamp01((liveSpeed - 20) / 30) * RUMBLE_CONTACT_SPEED_SCALE;
          }
        }
        break;
    }
    if (bRumbleTelemetry) {
      const float* n = cinfo.normal;
      const float* cv = cinfo.closingVel;
      RumbleTelemetryPrintf("C,%.4f,%d,0x%02X,%.2f,%.2f,%.2f,%.2f,%.2f,%d\n", RumbleGetTime() - RumbleTelemetry.fStartTime,
        cinfo.flags & 7, cinfo.flags & 0xFF, Length(cv), fabsf(cv[0]*n[0] + cv[1]*n[1] + cv[2]*n[2]), Length(cinfo.slidingVel), dv, strength, 1);
    }
    mMW.Collision(strength);
  }
#ifdef GAME_PROSTREET
  virtual int GetControllerType() { return 5; } // native PC default; no force-feedback wheel
#elif defined(GAME_UC)
  virtual int GetControllerType() { return 0; }
#endif
};
#pragma warning(pop)
#pragma pack(pop)
static_assert(sizeof(void*) == 4, "32-bit only");

#ifdef GAME_PROSTREET
// Native listener this is LocalPlayer +0x4C (feedback at this+4). Its context
// interface at this-0x28 supplies GetSimable in slot 3, as the original producer
// does at 0x4D1466. Compare handles for BOTH contact participants before routing.
void(__thiscall* RumblePSOriginalCollision)(void*, const COLLISION_INFO&) =
    (void(__thiscall*)(void*, const COLLISION_INFO&))0x004D13F0;

bool RumblePSCollisionTarget(void* listener, const COLLISION_INFO& hit, IFeedback*& feedback, bool& iamA) {
  __try {
    feedback = *(IFeedback**)((char*)listener + 4);
    bool ours = false;
    for (int i = 0; i < MAX_CONTROLLERS; ++i)
      if (feedback && feedback == RumblePSDevices[i]) ours = true;
    if (!ours) return false;
    void* simable = RumbleVCall<void*>((char*)listener - 0x28, 0x0C);
    if (!simable) { feedback = NULL; return true; }
    void* handle = *(void**)((char*)simable + 8); // ISimable::_mHandle
    if (bRumbleTelemetry)
      RumbleTelemetryPrintf("P,0x%02X,%p,%p,%p\n", hit.flags & 0xFF, hit.objA, hit.objB, handle);
    if (!handle || (handle != hit.objA && handle != hit.objB)) { feedback = NULL; return true; }
    iamA = handle == hit.objA;
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) { feedback = NULL; return false; }
}

void __fastcall RumblePSCollisionHook(void* listener, void*, const COLLISION_INFO& hit) {
  IFeedback* feedback = NULL;
  bool iamA = false;
  if (!RumblePSCollisionTarget(listener, hit, feedback, iamA)) {
    RumblePSOriginalCollision(listener, hit);
    return;
  }
  // A local listener can survive a vehicle transition while old contacts drain.
  if (!feedback || !bRumbleEnabled || !bRumbleAppActive || feedback->mPaused) return;
  feedback->ReportCollision(hit, iamA, RumblePSVector{});
}
#endif
