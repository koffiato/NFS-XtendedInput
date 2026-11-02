// Recovered MW feedback behavior; output and attribute access are mocked.
#define NOMINMAX
#define GAME_MW
#include <Windows.h>
#include <XInput.h>
#include <cassert>
#include <cstdio>
XINPUT_VIBRATION sent = {};
DWORD outputResult = ERROR_SUCCESS;
DWORD WINAPI TestSetState(DWORD, XINPUT_VIBRATION* value) { sent = *value; return outputResult; }
#define XInputSetState TestSetState
#include "../src/Main.hpp"
bool Near(float a, float b) { return fabsf(a-b) < 0.00001f; }
int main() {
  MWRamp ramp = {};
  ramp.Start(1, 0.5f, 2);
  assert(Near(ramp.Sample(0.5f), 0.875f));
  MWEffects effects = {};
  effects.NOS(true);
  effects.nos.Sample(0.5f);
  effects.NOS(true);
  assert(Near(effects.nos.remaining, 1.5f));
  effects.NOS(false);
  assert(effects.nos.Sample(0) == 0);
  effects.Engine(true);
  assert(Near(effects.engineLeft.Sample(0.5f), 0.8f));
  assert(Near(effects.engineRight.Sample(0.5f), 0.3f));
  effects.Shift(true);
  assert(Near(effects.shiftRight.Sample(0.1f), 0.4f));
  effects.Shift(false);
  assert(Near(effects.shiftRight.Sample(0.1f), 0.4f));
  effects.Potential(3);
  assert(Near(effects.potentialLeft.Sample(0.1f), 0.5f));
  effects.Potential(3);
  assert(effects.potentialLeft.Sample(0) == 0);
  effects.Potential(4);
  assert(Near(effects.potentialRight.Sample(0), 0.75f));
  RumbleWave wave = {};
  wave.fOffset[0] = 0.2f;
  wave.fOffset[1] = 0.1f;
  wave.fAmplitude[0] = 1;
  wave.fFrequency[0] = 1;
  float left, right;
  wave.Sample(0.75f, left, right);
  assert(Near(left, -0.7f)); // signed cancellation survives to the motor mixer
  wave.Begin();
  wave.Sample(0.1f, left, right);
  assert(Near(wave.fPhase[0], 0.75f)); // inactive waves retain phase
  RumbleMixer mixer = {};
  mixer.Add(kRumbleSurface, -0.7f, 0);
  mixer.Add(kRumbleImpacts, 0.4f, 2);
  int l, r;
  mixer.Resolve(l, r);
  assert(l == (int)(0.4f*65535) && r == 65535);
  IFeedback feedback;
  feedback.Init(NULL, 0);
  feedback.ResumeEffects();
  feedback.UpdateNOS(true, 1);
  assert(Near(feedback.mMW.nos.remaining, 2));
  feedback.EndUpdate();
  assert(feedback.mMW.outputLeft == 0);
  feedback.mLastUpdateTime = RumbleGetTime() - 0.01;
  feedback.PollEffects();
  assert(sent.wLeftMotorSpeed == 0 && feedback.mMW.outputLeft > 0);
  feedback.PollEffects();
  assert(sent.wLeftMotorSpeed > 0 && sent.wRightMotorSpeed == 0);
  // Strong contacts reach both motors; sliding contacts do not restart hits.
  feedback.Stop(); feedback.ResumeEffects();
  COLLISION_INFO hit = {};
  hit.flags = 2; hit.normal[0] = 1; hit.closingVel[0] = 15;
  feedback.ReportCollision(hit, true);
  assert(Near(feedback.mPulseLeft.GetValue(), 0.5f));
  assert(Near(feedback.mPulseRight.GetValue(), 0.5f));
  feedback.EndUpdate(); feedback.PollEffects(); feedback.PollEffects();
  assert(sent.wLeftMotorSpeed > 0 && sent.wRightMotorSpeed > 0);
  feedback.Stop(); feedback.ResumeEffects();
  hit.flags = 0x82;
  hit.slidingVel[0] = 5;
  feedback.ReportCollision(hit, true);
  assert(feedback.mPulseLeft.GetValue() == 0 && feedback.mPulseRight.GetValue() == 0);
  feedback.EndUpdate(); feedback.PollEffects(); feedback.PollEffects();
  // Sliding speed is refreshed by contacts, not held indefinitely.
  assert(feedback.mScrapeSpeed == 0 && feedback.mPulseRight.GetValue() == 0 && sent.wRightMotorSpeed > 0);
  feedback.Stop(); feedback.ResumeEffects();
  hit.flags = 3; hit.closingVel[0] = 2.5f;
  feedback.ReportCollision(hit, true);
  assert(Near(feedback.mLandingRight.GetValue(), 0.5f));
  feedback.Stop(); feedback.ResumeEffects();
  // Native suspension samples can add a kerb impulse independently of waves.
  feedback.mHasVehicle = true;
  feedback.mVehicle.nWheels = 1; feedback.mVehicle.fSpeed = 10;
  feedback.mVehicle.bOnGround[0] = true;
  feedback.mVehicle.fCompression[0] = 0;
  feedback.UpdateBumps(0);
  feedback.mVehicle.fCompression[0] = 0.1f;
  feedback.UpdateBumps(0.01f);
  assert(feedback.mBumpLeft.GetValue() > 0);
  feedback.PauseEffects();
  assert(sent.wLeftMotorSpeed == 0 && sent.wRightMotorSpeed == 0);
  assert(feedback.mMW.nos.remaining == 0);
  feedback.Send(1, 2);
  assert(sent.wLeftMotorSpeed == 1 && sent.wRightMotorSpeed == 2);
  // Record actual motor commands and backend failures, rather than
  // inferring delivery from telemetry callbacks or collision magnitudes.
  RumbleTelemetryFile = fopen("build/tests/mw-output.log", "w+");
  assert(RumbleTelemetryFile);
  bRumbleTelemetry = true;
  RumbleTelemetry.fStartTime = RumbleGetTime();
  feedback.Send(32767, 16383);
  outputResult = ERROR_DEVICE_NOT_CONNECTED;
  feedback.Send(65535, 0);
  assert(feedback.mLastLeft == -1);
  fflush(RumbleTelemetryFile);
  rewind(RumbleTelemetryFile);
  char line[256];
  assert(fgets(line, sizeof(line), RumbleTelemetryFile));
  int pad, leftMotor, rightMotor, lt, rt;
  double time;
  long result;
  assert(sscanf(line,"F,%lf,%d,%d,%d,%d,%d,XInput,%ld",&time,&pad,&leftMotor,&rightMotor,&lt,&rt,&result)==7);
  assert(leftMotor == 32767 && rightMotor == 16383 && result == ERROR_SUCCESS);
  assert(fgets(line, sizeof(line), RumbleTelemetryFile));
  assert(strstr(line, ",65535,0,0,0,XInput,1167"));
  fclose(RumbleTelemetryFile);
  RumbleTelemetryFile = NULL;
  bRumbleTelemetry = false;
  puts("MW feedback and restored effect coverage checks passed");
}
