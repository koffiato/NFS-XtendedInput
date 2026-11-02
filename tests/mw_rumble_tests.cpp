// Recovered MW controller behavior; native interfaces, clock and output are mocked.
#define NOMINMAX
#define GAME_MW
#include <Windows.h>
#include <XInput.h>
#include <cassert>
#include <cstdio>
double fixedTime = 10;
XINPUT_VIBRATION sent = {};
DWORD outputResult = ERROR_SUCCESS;
BOOL WINAPI TestQPC(LARGE_INTEGER* p) { p->QuadPart = (LONGLONG)(fixedTime*1000000 + 0.5); return TRUE; }
BOOL WINAPI TestQPF(LARGE_INTEGER* p) { p->QuadPart = 1000000; return TRUE; }
DWORD WINAPI TestSetState(DWORD, XINPUT_VIBRATION* value) { sent = *value; return outputResult; }
#define QueryPerformanceCounter TestQPC
#define QueryPerformanceFrequency TestQPF
#define XInputSetState TestSetState
#include "../src/Main.hpp"

bool Near(float a, float b) { return fabsf(a-b) < 0.00001f; }
struct Body {
  void** table;
  void* interfaces;
  float velocity[3];
  bool vehicle;
} bodies[2];
void* bodyTable[27] = {};
void __fastcall Velocity(Body* body, void*, float* out) { memcpy(out, body->velocity, 12); }
void* __cdecl Resolve(void* handle) {
  return handle == (void*)1 ? &bodies[0] : handle == (void*)2 ? &bodies[1] : NULL;
}
void* __fastcall Find(void* list, void*, void* handle) {
  return handle == (void*)IVEHICLE_IHANDLE_ADDR && ((Body*)list)->vehicle ? list : NULL;
}
RumbleWaveRecord record = {2, 0.6f, 0.3f, 0, 2, 0.6f, 0.3f, 20};
int attributeQueries = 0;
const void* __fastcall Attribute(const void*, void*, unsigned int, unsigned int index) {
  ++attributeQueries;
  return index == 0 ? &record : NULL;
}
void Reset(IFeedback& f) {
  fixedTime = 10;
  bRumbleAppActive = bRumbleEnabled = true;
  bRumbleTelemetry = false;
  RumbleTriggerMode = kTriggersOff;
  outputResult = ERROR_SUCCESS;
  g_Controllers[0].bConnected = true;
  bodies[0] = {bodyTable, &bodies[0], {}, true};
  bodies[1] = {bodyTable, &bodies[1], {}, true};
  f.Init(NULL, 0);
  f.ResumeEffects();
  sent = {};
}
void Tick(IFeedback& f, double dt = 0.01) {
  fixedTime += dt;
  f.EndUpdate();
  f.PollEffects();
}
COLLISION_INFO Contact(int flags = 2) {
  COLLISION_INFO hit = {};
  hit.flags = flags;
  hit.objA = (void*)1;
  hit.objB = (void*)2;
  hit.normal[0] = 1;
  hit.closingVel[0] = 15;
  return hit;
}
int main() {
  bodyTable[26] = (void*)Velocity;
  RumbleMWResolveSimable = Resolve;
  UTL_Com_Object_IList_Find = (decltype(UTL_Com_Object_IList_Find))Find;
  Attrib_Instance_GetAttributePointer = (decltype(Attrib_Instance_GetAttributePointer))Attribute;
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
  wave.fAmplitude[0] = wave.fFrequency[0] = 1;
  float left, right;
  wave.Sample(0.75f, left, right);
  assert(Near(left, -0.7f));
  wave.Begin();
  wave.Sample(0.1f, left, right);
  assert(Near(wave.fPhase[0], 0.75f));
  RumbleMixer mixer = {};
  mixer.Add(kRumbleSurface, -0.7f, 0);
  mixer.Add(kRumbleImpacts, 0.4f, 2);
  int l, r;
  mixer.Resolve(l, r);
  assert(l == (int)(0.4f*65535) && r == 65535);

  IFeedback feedback;
  Reset(feedback);
  feedback.UpdateNOS(true, 1);
  Tick(feedback);
  assert(sent.wLeftMotorSpeed == 0 && feedback.mMW.outputLeft > 0);
  Tick(feedback);
  assert(sent.wLeftMotorSpeed > 0 && sent.wRightMotorSpeed == 0);
  // No secondary shift query: only the native callback starts this ramp.
  feedback.UpdateEngineBlown(false);
  Tick(feedback);
  assert(feedback.mMW.shiftLeft.remaining == 0);
  feedback.UpdateShifting(true);
  assert(Near(feedback.mMW.shiftLeft.remaining, 0.5f));
  assert(Near(feedback.mMW.shiftRight.remaining, 0.5f));

  Reset(feedback);
  COLLISION_INFO hit = Contact();
  bodies[0].velocity[0] = 15;
  feedback.ReportCollision(hit, true);
  assert(Near(feedback.mMW.collision.remaining, 0.5f));
  Tick(feedback);
  Tick(feedback);
  // Retail fixed-stream output, before the second 10 ms sample.
  assert(abs((int)sent.wLeftMotorSpeed - 32112) <= 1 && sent.wRightMotorSpeed == 0);
  for (int i = 2; i < 51; ++i) Tick(feedback);
  assert(sent.wLeftMotorSpeed == 0 && sent.wRightMotorSpeed == 0);
  // Weak retriggers preserve amplitude/duration but extend remaining time.
  Reset(feedback);
  feedback.mMW.Collision(0.5f);
  feedback.mMW.collision.Sample(0.4f);
  feedback.mMW.Collision(0.2f);
  assert(Near(feedback.mMW.collision.remaining, 0.2f));
  assert(Near(feedback.mMW.collision.duration, 0.5f));
  assert(Near(feedback.mMW.collision.amplitude, 0.5f));
  feedback.mMW.Collision(0.8f);
  assert(Near(feedback.mMW.collision.remaining, 0.8f));
  assert(Near(feedback.mMW.collision.duration, 0.8f));

  // Sliding flags do not suppress the native collision or add a scrape motor.
  Reset(feedback);
  hit = Contact(0x82); hit.slidingVel[0] = 5;
  bodies[0].velocity[0] = 15;
  feedback.ReportCollision(hit, true);
  Tick(feedback); Tick(feedback);
  assert(abs((int)sent.wLeftMotorSpeed - 32112) <= 1 && sent.wRightMotorSpeed == 0);
  // Closing speed alone cannot invent a hit when measured delta is zero.
  Reset(feedback);
  feedback.ReportCollision(hit, true);
  assert(feedback.mMW.collision.remaining == 0);
  hit = Contact(3); bodies[0].velocity[0] = 2.5f;
  feedback.ReportCollision(hit, true);
  assert(Near(feedback.mMW.collision.amplitude, 0.5f));

  // Rear-end strength uses A's stored speed for either ownership choice.
  for (int owner = 0; owner < 2; ++owner) {
    Reset(feedback);
    hit = Contact(1);
    hit.objAVel[0] = bodies[0].velocity[0] = 20;
    hit.objBVel[0] = bodies[1].velocity[0] = 40;
    feedback.ReportCollision(hit, owner == 0);
    assert(Near(feedback.mMW.collision.amplitude, 0.25f));
  }
  Reset(feedback);
  hit = Contact(1);
  hit.objAVel[0] = bodies[0].velocity[0] = 40;
  hit.objBVel[0] = bodies[1].velocity[0] = -20;
  bodies[1].vehicle = false;
  feedback.ReportCollision(hit, true);
  assert(Near(feedback.mMW.collision.amplitude, 2.0f/3));
  Reset(feedback);
  hit = Contact(); hit.objA = NULL;
  feedback.ReportCollision(hit, true);
  assert(feedback.mMW.collision.remaining == 0);
  bodies[0].velocity[0] = 15;
  hit.objA = (void*)1; hit.objB = NULL;
  feedback.ReportCollision(hit, true);
  assert(Near(feedback.mMW.collision.amplitude, 0.5f));

  Reset(feedback);
  feedback.UpdateNOS(true, 1);
  feedback.mMW.Collision(0.5f);
  feedback.mRoadWave.fPhase[0] = 0.25f;
  feedback.mRoadWave.fAmplitude[0] = 1;
  Tick(feedback);
  MWEffects before = feedback.mMW;
  RumbleWave beforeRoad = feedback.mRoadWave;
  feedback.PauseEffects();
  assert(sent.wLeftMotorSpeed == 0 && sent.wRightMotorSpeed == 0);
  feedback.UpdateNOS(false, 0);
  feedback.UpdateShifting(true);
  feedback.UpdateShiftPotential((ShiftPotential)5);
  feedback.UpdateEngineBlown(true);
  feedback.ReportCollision(Contact(), true);
  char surface[16] = {};
  feedback.UpdateRoadNoise(true, *(SimSurface*)surface, 10);
  feedback.UpdateTireSlip(true, *(SimSurface*)surface, 10);
  feedback.UpdateTireSkid(true, *(SimSurface*)surface, 10);
  fixedTime += 0.1;
  feedback.PollEffects();
  assert(!memcmp(&before, &feedback.mMW, sizeof(before)));
  assert(!memcmp(&beforeRoad, &feedback.mRoadWave, sizeof(beforeRoad)));
  feedback.ResumeEffects();
  assert(sent.wLeftMotorSpeed == before.outputLeft);
  Tick(feedback);
  assert(Near(feedback.mMW.nos.remaining, before.nos.remaining - 0.01f));
  // Reset clears only volumes and collision; paused event flags/ramp survive.
  before = feedback.mMW;
  feedback.ResetEffects();
  assert(feedback.mPaused && feedback.mMW.collision.remaining == 0);
  assert(feedback.mMW.outputLeft == 0 && feedback.mMW.outputRight == 0);
  assert(feedback.mMW.nosOn && Near(feedback.mMW.nos.remaining, before.nos.remaining));
  assert(feedback.mRoadWave.fAmplitude[0] == 1);
  feedback.ResumeEffects();
  feedback.UpdateNOS(true, 1);
  assert(Near(feedback.mMW.nos.remaining, before.nos.remaining));
  before = feedback.mMW;
  g_Controllers[0].bConnected = false;
  feedback.ResetEffects();
  assert(!feedback.mPaused && !memcmp(&before, &feedback.mMW, sizeof(before)));
  g_Controllers[0].bConnected = true;

  // BeginUpdate clears wave parameters even when disabled, retaining phase/ramps.
  feedback.PauseEffects();
  feedback.BeginUpdate();
  assert(feedback.mRoadWave.fAmplitude[0] == 0);
  assert(feedback.mRoadWave.fPhase[0] == beforeRoad.fPhase[0]);
  assert(Near(feedback.mMW.nos.remaining, before.nos.remaining));
  feedback.ResumeEffects();
  attributeQueries = 0;
  feedback.UpdateRoadNoise(true, *(SimSurface*)surface, 10);
  assert(attributeQueries == 4);
  // The sampler consumes full positive deltas; stale producers stop output.
  Reset(feedback);
  feedback.UpdateNOS(true, 1);
  Tick(feedback, 0.15);
  assert(Near(feedback.mMW.nos.remaining, 1.85f));
  before = feedback.mMW;
  feedback.PollEffects();
  assert(!memcmp(&before, &feedback.mMW, sizeof(before)));
  fixedTime += 0.3;
  feedback.PollEffects();
  assert(sent.wLeftMotorSpeed == 0 && Near(feedback.mMW.nos.remaining, 1.85f));
  feedback.EndUpdate();
  bRumbleAppActive = false;
  fixedTime += 0.01;
  feedback.PollEffects();
  assert(sent.wLeftMotorSpeed == 0 && Near(feedback.mMW.nos.remaining, 1.85f));
  bRumbleAppActive = true;
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
  return 0;
}
