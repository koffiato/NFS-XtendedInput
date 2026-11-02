// Optional MW/ProStreet diagnostic records. MIT; see LICENSE.
#pragma once
#include "Vehicle.h"
#include "Surface.h"
#include "Log.h"
#include <cstdarg>

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
    RumbleTelemetryFile = RumbleOpenLog("scripts\\NFS_XtendedInput_Rumble.log");
    if (!RumbleTelemetryFile) {
      bRumbleTelemetryDead = true;
      return;
    }
#ifdef GAME_PROSTREET
    fprintf(RumbleTelemetryFile, "# PS native PC data; effect envelopes/tuning temporarily shared with MW; Xbox 360 curves not decoded\n");
    fprintf(RumbleTelemetryFile, "# S,id,collection,PS-live,FFB_ROLL/SKID/SLIP[4 x 8]; '-' means no native attribute\n");
    fprintf(RumbleTelemetryFile, "# P,flags,objectA,objectB,localHandle (raw local-listener contact, before ownership filtering)\n");
#else
    fprintf(RumbleTelemetryFile, "# S,id,collection,name,renderNoise[freq amp minSpeed maxSpeed],wheelEffect[freq intensity],friction[6],FFB_ROLL/SKID/SLIP[4 x 8]\n");
#endif
#ifdef GAME_PROSTREET
    fprintf(RumbleTelemetryFile, "# T,time,speed,gas,brake,handbrake,powerband,throttle,nos,shifting,shiftPotential,{surf,ground,comp,slipRatio,skidRadians,nativeRoad,fbSlipRatio,fbSkidRadians}x4,linVel[3],angVel[3]\n");
#else
    fprintf(RumbleTelemetryFile, "# T,time,speed,gas,brake,handbrake,powerband,throttle,nos,shifting,shiftPotential,{surf,ground,comp,slip,skid,av,fbSlip,fbSkid}x4,linVel[3],angVel[3]\n");
#endif
    fprintf(RumbleTelemetryFile, "# C,time,type,flags,closingVel,normalSpeed,slidingSpeed,speedChange,strength,measuredSpeedChange\n");
    fprintf(RumbleTelemetryFile, "# F,time,pad,left,right,leftTrigger,rightTrigger,backend,result (XInput error code; WGI -1 means status unavailable)\n");
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
    const float* w = (const float*)RumbleGetAttributePointer(surface, key, i);
    if (w)
      RumbleTelemetryPrintf(",%g %g %g %g %g %g %g %g", w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
    else
      RumbleTelemetryPrintf(",-");
  }
}

void RumbleTelemetryDumpSurfaceUnsafe(int id, const void* surface) {
#ifdef GAME_PROSTREET
  // PS collection/layout are at +0/+4; MW's layout field offsets do not apply.
  RumbleTelemetryPrintf("S,%d,%p,PS-live", id, RumbleSurfaces[id]);
#else
  const char* layout = *(const char* const*)((const char*)surface + 8);
  const char* name   = layout ? *(const char* const*)(layout + 0xDC) : NULL;
  RumbleTelemetryPrintf("S,%d,%p,%s", id, RumbleSurfaces[id], name ? name : "?");
  if (layout) {
    const float* rn = (const float*)(layout + 0xCC);
    const float* fr = (const float*)(layout + 0xE0);
    RumbleTelemetryPrintf(",%g %g %g %g,%u %u,%g %g %g %g %g %g", rn[0], rn[1], rn[2], rn[3], *(const unsigned short*)(layout + 0xF8),
                          *(const unsigned char*)(layout + 0xFA), fr[0], fr[1], fr[2], fr[3], fr[4], fr[5]);
  }
#endif
  RumbleTelemetryDumpFFBWave(surface, 0xBA1297DA);  // FFB_ROLL
  RumbleTelemetryDumpFFBWave(surface, 0x0C149044);  // FFB_SKID
  RumbleTelemetryDumpFFBWave(surface, 0x8FD11D27);  // FFB_SLIP
}

// returns the surface's id in the log, dumping its data the first time it is seen
int RumbleTelemetrySurfaceId(const void* surface) {
  const void* collection = NULL;
  __try { collection = surface ? RumbleSurfaceCollection(surface) : NULL; }
  __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
  if (!collection) return -1;
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

void RumbleTelemetryOutput(int pad, int left, int right, int leftTrigger, int rightTrigger, const char* backend, long result) {
  if (!bRumbleTelemetry || !RumbleTelemetry.fStartTime) return;
  RumbleTelemetryPrintf("F,%.4f,%d,%d,%d,%d,%d,%s,%ld\n", RumbleGetTime() - RumbleTelemetry.fStartTime,
                        pad, left, right, leftTrigger, rightTrigger, backend, result);
}

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
                          v.fSlip[i], v.fSkid[i],
#ifdef GAME_PROSTREET
                          v.fNativeRoad[i],
#else
                          v.fWheelAV[i],
#endif
                          wheel ? RumbleTelemetry.fSlip[i] : 0.0f, wheel ? RumbleTelemetry.fSkid[i] : 0.0f);
  }
  RumbleTelemetryPrintf(",%.2f %.2f %.2f,%.3f %.3f %.3f%s\n", v.fLinVel[0], v.fLinVel[1], v.fLinVel[2], v.fAngVel[0], v.fAngVel[1], v.fAngVel[2],
                        hasVehicle ? "" : ",novehicle");

  if (now - RumbleTelemetry.fLastFlushTime > 1.0) {
    RumbleTelemetry.fLastFlushTime = now;
    RumbleTelemetryFlush();
  }
}
