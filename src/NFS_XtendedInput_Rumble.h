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

enum RumbleCategory {
  kRumbleImpacts,  // hits, scrapes, spike strips
  kRumbleSurface,  // kerbs, bumps, landing, road baseline
  kRumbleTyres,    // lockup, wheelspin, slides, cornering
  kRumbleCar,      // NOS, shifts, engine
  kRumbleCategoryCount,
};

bool  bRumbleEnabled                             = true;
bool  bRumbleSwapMotors                          = false;
float fRumbleIntensity                           = 1.0f;
float fRumbleLeftScale                           = 1.0f;  // low frequency (heavy) motor
float fRumbleRightScale                          = 1.0f;  // high frequency (light) motor
float fRumbleCategoryScale[kRumbleCategoryCount] = {1.0f, 1.0f, 1.0f, 1.0f};
float fRumbleMinImpactVel                        = 2.0f;   // m/s, closing speeds below this don't rumble
float fRumbleMaxImpactVel                        = 25.0f;  // m/s, closing speed for full strength

// ducking: textures drop to this level while an impact pulse is playing
const float RUMBLE_DUCK_LEVEL     = 0.3f;
const float RUMBLE_DUCK_THRESHOLD = 0.05f;
// fatigue fade: a texture running longer than FATIGUE_START seconds fades to FATIGUE_FLOOR over FATIGUE_RAMP seconds,
// and recovers once it has been quiet for FATIGUE_RESET seconds
const float RUMBLE_FATIGUE_START = 3.0f;
const float RUMBLE_FATIGUE_RAMP  = 5.0f;
const float RUMBLE_FATIGUE_FLOOR = 0.6f;
const float RUMBLE_FATIGUE_RESET = 1.0f;

// pad that last produced input (used in omni mode, where both game ports read every pad)
int  RumbleLastActivePad = 0;
bool bRumbleAppActive    = true;
// bumped whenever all pads are force-stopped, so every feedback object resyncs its motor state
unsigned int RumbleStopGeneration = 0;

void(__thiscall* UTL_Com_Object_IList_Add)(void* thethis, void* handle, void* iface) = (void(__thiscall*)(void*, void*, void*))UTL_ILIST_ADD_ADDR;

void RumbleStopAllPads() {
  XINPUT_VIBRATION vib = {0, 0};
  for (DWORD i = 0; i < MAX_CONTROLLERS; i++) XInputSetState(i, &vib);
  RumbleStopGeneration++;
}

double RumbleGetTime() {
  static LARGE_INTEGER freq = {0};
  if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return (double)now.QuadPart / (double)freq.QuadPart;
}

// envelopes from the console Rumble.cpp (time in seconds, value 0-255)
struct RumbleEnvelopePoint {
  float fTime;
  float fValue;
};

const RumbleEnvelopePoint SmallRumblePoints[] = {{0.0f, 255.0f}, {0.05f, 200.0f}, {0.07f, 80.0f}, {0.8f, 0.0f}};
const RumbleEnvelopePoint BigRumblePoints[]   = {{0.0f, 255.0f}, {0.15f, 255.0f}, {0.2f, 120.0f}, {1.2f, 0.0f}};

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
    if (v <= 0.0f) return 0;
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
  RumblePulse  mPulseLeft;
  RumblePulse  mPulseRight;
  RumbleTextureState mNOSTexture;
  RumbleMixer  mMixer;

  void Init(void* ownerList, int deviceIndex) {
    mOwnerList      = ownerList;
    mDeviceIndex    = deviceIndex;
    mPad            = deviceIndex;
    mLastLeft       = 0;
    mLastRight      = 0;
    mPaused         = true;
    mNOSEngaged     = false;
    mLastUpdateTime = 0.0;
    mStopGeneration = RumbleStopGeneration;
    memset(&mPulseLeft, 0, sizeof(RumblePulse));
    memset(&mPulseRight, 0, sizeof(RumblePulse));
    memset(&mNOSTexture, 0, sizeof(RumbleTextureState));
    memset(&mMixer, 0, sizeof(RumbleMixer));
  }

  void Send(int left, int right) {
    if (mStopGeneration != RumbleStopGeneration) {
      // all pads got zeroed behind our back
      mStopGeneration = RumbleStopGeneration;
      mLastLeft       = 0;
      mLastRight      = 0;
    }

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
    mPulseLeft.pPoints  = NULL;
    mPulseRight.pPoints = NULL;
    mNOSEngaged         = false;
    Send(0, 0);
  }

  // called every frame from PollDevice: stop the motors if the game stopped ticking us without pausing
  void Watchdog() {
    if (mPaused || (mLastLeft <= 0 && mLastRight <= 0)) return;
    if (!bRumbleAppActive || RumbleGetTime() - mLastUpdateTime > 0.25) {
      mPaused = true;
      Stop();
    }
  }

  // never delete -- embedded in InputDevice
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

  virtual void BeginUpdate() { mNOSEngaged = false; }

  virtual void EndUpdate() {
    double now = RumbleGetTime();
    float  dt  = (float)(now - mLastUpdateTime);
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;
    mLastUpdateTime = now;

    mPulseLeft.Advance(dt);
    mPulseRight.Advance(dt);

    if (!bRumbleEnabled || !bRumbleAppActive || mPaused) {
      Send(0, 0);
      return;
    }

    mMixer.Begin(dt);
    mMixer.AddPulse(kRumbleImpacts, mPulseLeft.GetValue(), mPulseRight.GetValue());
    mMixer.AddTexture(kRumbleCar, mNOSTexture, mNOSEngaged ? 0.15f : 0.0f, mNOSEngaged ? 0.30f : 0.0f);

    int left, right;
    mMixer.Resolve(left, right);
    Send(left, right);
  }

  virtual void UpdateRoadNoise(bool /*front*/, const SimSurface& /*surface*/, float /*speed*/) {}
  virtual void UpdateTireSkid(bool /*front*/, const SimSurface& /*surface*/, float /*skid*/) {}
  virtual void UpdateTireSlip(bool /*front*/, const SimSurface& /*surface*/, float /*slip*/) {}
  virtual void UpdateRPM(float /*powerband*/, float /*overrev*/, float /*throttle*/) {}
  virtual void UpdateShiftPotential(ShiftPotential /*potential*/) {}
  virtual void UpdateNOS(bool engaged, float /*nosLevel*/) { mNOSEngaged = engaged; }
  virtual void UpdateEngineBlown(bool /*blown*/) {}
  virtual void UpdateShifting(bool /*shifting*/) {}

  virtual void ReportCollision(const COLLISION_INFO& cinfo, bool /*iamA*/) {
    if (!bRumbleEnabled || mPaused) return;

    const float* v     = cinfo.closingVel;
    float        speed = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (!(speed > fRumbleMinImpactVel)) return;  // also rejects NaN

    float strength = (speed - fRumbleMinImpactVel) / (fRumbleMaxImpactVel - fRumbleMinImpactVel);
    if (strength > 1.0f) strength = 1.0f;

    mPulseRight.Start(SmallRumblePoints, _countof(SmallRumblePoints), strength);
    // the heavy motor only kicks in for proper hits
    if (strength > 0.4f) mPulseLeft.Start(BigRumblePoints, _countof(BigRumblePoints), strength);

#ifdef _DEBUG
    char buf[128];
    sprintf_s(buf, "[XtendedInput] collision: closingVel %.2f m/s, strength %.2f\n", speed, strength);
    OutputDebugStringA(buf);
#endif
  }
};
#pragma warning(pop)
#pragma pack(pop)
static_assert(sizeof(void*) == 4, "32-bit only");
