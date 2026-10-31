# Xbox 360 Need for Speed: Most Wanted — Force Feedback & Rumble Architecture

## 1. Executive Summary

This document presents the reverse engineering findings on how the Xbox 360 release of **Need for Speed: Most Wanted (2005)** handles force feedback and controller vibration (rumble).

The research was performed strictly read-only against the original retail ISO image:
- **Source Image**: `C:\Users\Koffiato\Desktop\NFS\X360\Need for Speed - Most Wanted (USA).iso` (Read-only access, zero modifications to the ISO file).
- **Executable**: `default.xex` (Title ID: `454107D9`, retail encrypted AES-128 + LZX compressed).
- **Decompressed Binary**: `default.exe` (13,434,880 bytes, base `0x82000000`, entry point `0x8262E768`).

On PC, EA stripped out the force feedback motor output stage, leaving `IFeedback` as 15 empty virtual stub methods. On Xbox 360, the game contains a fully functional, dual-tier vibration architecture consisting of:
1. **Low-Level Actuator Hardware Layer**: Direct driver interfacing with the Xbox 360 controller via `XamInputSetState` / `XInputSetState`.
2. **EA PFD (Platform Force Direct / Personal Force Device v5.1)**: A proprietary haptic middleware system that loads pre-authored vibration banks (`PFDx`), evaluates multi-knot spline curves at 50 Hz, and mixes up to 4 simultaneous controller channels.
3. **Vehicle Simulation Telemetry (`LocalPlayer::DoFFB`)**: A 1:1 match with the PC simulation code, dispatching 4-wheel slip, skid, road noise, suspension travel, engine RPM, shift potential, nitrous, and collisions.

---

## 2. ISO & Binary Extraction Architecture

### 2.1 ISO Partition & GDFX File System
The disc image uses the Microsoft **XGD2** (Xbox Game Disc 2) partition layout:
- **Base Partition Offset**: `0x0FD90000` (sector `0x1FBA0`).
- **Volume Descriptor**: Located at sector 32 (`0x0FDA0000` from disc start), magic string `MICROSOFT*XBOX*MEDIA`.
- **Root Directory**: Sector `745012` (`0x0FD90000 + 745012 * 2048`), size 2048 bytes.
- **Directory Structure**:
  - `Movies/`: Pre-rendered FMVs (`.wmv` container format).
  - `NFS/`: Core game assets (`ZDIR.BIN` master directory, `ZZDATA0.BIN` .. `ZZDATA9.BIN` big archives, and padding streams `PADA1.BIN`, `PADB1.BIN`).
  - `default.xex`: Sector `745014`, length `3,936,256` bytes.

### 2.2 XEX Header & Binary Layout
Using a locally compiled build of `XexTool`, `default.xex` was decrypted and uncompressed:
- **Image Base**: `0x82000000`
- **Entry Point**: `0x8262E768`
- **Section Layout**:
  - `.rdata`: `0x82000600 .. 0x820B106C` (Constants, C++ vtables, RTTI)
  - `.pdata`: `0x820B1200 .. 0x820DD6B8` (PPC exception unwinding tables)
  - `.text`: `0x820E0000 .. 0x828AB01C` (Main executable PPC instructions)
  - `.data`: `0x828D0000 .. 0x82C8CE8C` (Globals and mutable engine state)

---

## 3. Low-Level Actuator Hardware Layer

### 3.1 Kernel Import & Wrapper
The game communicates with the hardware controllers through `xam.xex`:
- **Import Ordinal**: `0x192` (`XamInputSetState`)
- **Import Pointer**: `0x820006A4`
- **Import Thunk**: `0x828A9C9C`
- **XDK Wrapper Function (`XInputSetState`)**: `0x8262D6C8`

```ppc
// 0x8262D6C8: XDK XInputSetState wrapper
0x8262D6C8: mr       r5, r4       // r5 = pVibration
0x8262D6CC: li       r8, 0
0x8262D6D0: li       r7, 0
0x8262D6D4: li       r6, 0
0x8262D6D8: li       r4, 0        // r4 = 0 (sub-device)
0x8262D6DC: b        0x828A9C9C   // tail call to XamInputSetState
```

### 3.2 Native Vibration Device Class (`ControllerVibrationDevice`)
Located at `0x8261C9B8 .. 0x8261CC80` with virtual method table at `0x820AFA98`.
The class instance size is `0x28` (40 bytes) with the following memory layout:

| Offset | Type | Member Name | Description |
|---|---|---|---|
| `+0x00` | `void*` | `m_vtable` | Pointer to vtable at `0x820AFA98` |
| `+0x04` | `uint32_t` | `m_portIndex` | Controller index (0 .. 3) |
| `+0x08` | `uint8_t` | `m_bActive` | 1 if motors are engaged, 0 if stopped |
| `+0x09..0x0B` | `uint8_t[3]` | _pad | Alignment padding |
| `+0x0C` | `uint32_t` | `m_leftMotorSpeed` | Heavy motor speed (`0 .. 65535`) |
| `+0x10` | `uint32_t` | `m_rightMotorSpeed` | Light motor speed (`0 .. 65535`) |
| `+0x14` | `uint16_t` | `m_vib.wLeftMotorSpeed` | `XINPUT_VIBRATION` left motor speed |
| `+0x16` | `uint16_t` | `m_vib.wRightMotorSpeed`| `XINPUT_VIBRATION` right motor speed |
| `+0x18..0x27` | `uint8_t[16]`| _reserved | Internal synchronization and state |

#### Virtual Method Table (`0x820AFA98`):
- `vtable[0] (0x8261C9B8)`: Destructor
- `vtable[6] (0x8261CC08)`: `bool IsActive()`
- `vtable[7] (0x8261CB38)`: `void SetVibration()`
  - Copies `m_leftMotorSpeed` and `m_rightMotorSpeed` into `m_vib`.
  - Invokes `XInputSetState(m_portIndex, &m_vib)`.
  - Sets `m_bActive = 1`.
- `vtable[8] (0x8261CB88)`: `void StopVibration()`
  - Sets `m_vib.wLeftMotorSpeed = 0` and `m_vib.wRightMotorSpeed = 0`.
  - Invokes `XInputSetState(m_portIndex, &m_vib)`.
  - Sets `m_bActive = 0`.
- `vtable[9] (0x8261CBD8)`: `void SetMotorSpeeds(uint32_t left, uint32_t right, bool bImmediate)`
  - Stores `left` into `+0x0C` and `right` into `+0x10`.
  - If `bImmediate == 1`, calls `vtable[7]->SetVibration()`.

### 3.3 Device Management & Instantiation
- **Global Device Manager Instance**: Located at `*(void**)0x82A2D9EC`.
- **`HasVibrationDevice(port)`** (`0x8261CCC8`): Checks if a physical vibration device is attached to port `0 .. 3`.
- **`GetVibrationDevice(port)`** (`0x8261CDC8`): Allocates or retrieves the `ControllerVibrationDevice` pointer for the given port.

---

## 4. EA PFD (Platform Force Direct) Haptic Middleware

Unlike the PC build which relies on game code to calculate motor levels directly, Xbox 360 uses EA Vancouver / Black Box's **PFD (Platform Force Direct)** subsystem (version 5.1).

### 4.1 PFD File & Bank Format (`PFDx`)
Force feedback curves and envelopes are packaged in pre-compiled binary banks identified by the 4CC header `PFDx` (`0x50464478`):
- **Magic Identifier**: `PFDx` (`0x825BF5D4`)
- **Version**: Major `5`, Minor `1` (checked at `0x825BF5EC` and `0x825BF5F8`).
- **Bank Loader**: `0x825BF518` (`PFD_LoadBank(void* bankData, int bankSize)`).
- **Bank Lookup**: `0x825BF8F8` (`PFD_GetBankByName(const char* name)`). Known banks identified in memory include:
  - `PursuitID`: Pursuit and siren/chopper rumble events.
  - `rapsheet`: Bounty, milestone, and arrest force events.

### 4.2 Multi-Channel Dispatch & Update Loop
The PFD subsystem maintains an array of four channel contexts at `0x82A2D92C[0 .. 3]` corresponding to players 1 through 4:
- **Update Frequency**: Fixed 50 Hz tick rate (`0x825BF708: li r9, 0x32` [50], `li r8, 0xa` [10 ms]).
- **Output Dispatcher (`0x825C0028`)**: Synchronized motor speed dispatch function:
  - Accepts `channel` (`r3`), `leftMotorSpeed` (`r4`), and `rightMotorSpeed` (`r5`).
  - Acquires the PFD critical section at `*(0x82A2D988)` via `0x8261ADE8`.
  - Dispatches values to the target player slot in `0x82A2D92C`.
  - Releases the critical section via `0x828A9E2C`.

### 4.3 Spline / Envelope Interpolation
Authored force curves in the `PFDx` banks are evaluated by the spline interpolator at `0x825BFD60 .. 0x825BFE60`:
- Knot points are stored as 8-byte structures (`(float time, float amplitude)`).
- The interpolator calculates segment duration `dx = x1 - x0` and amplitude change `dy = y1 - y0`.
- Segment slopes (`dy / dx`) are computed via single-precision PowerPC floating point (`fdivs`, `fmuls`).
- Rounding constant `0.5` is added (`fadd f7, f8, f0`) before converting to 16-bit unsigned integers via `fctidz` and `stfiwx`.

### 4.4 Effect Manager Class (`0x825C0820`)
Virtual method table located at `0x820AF240`:
- **`CreateEffect(int port, uint32_t* pHandle, void* pDevice)`** (`0x825C0820`):
  - If `pDevice == NULL`, resolves the device via `GetVibrationDevice(port)`.
  - Allocates an internal haptic slot.
  - Sentinel invalid handle: `0xBEEFDEAF` (`0x825C0908`).
- **`PlayEffect(uint32_t handle, ...)`** (`0x825C0928`): Validates handle against `0xBEEFDEAF` and begins waveform playback.
- **`StopEffect(uint32_t handle)`** (`0x825C0AC8`): Silences the active effect.

---

## 5. Vehicle Simulation Telemetry (`LocalPlayer::DoFFB`)

The vehicle physics engine feeds telemetry into the feedback system inside `LocalPlayer::DoFFB` at `0x8246E420 .. 0x8246EA54`. This function is byte-for-byte conceptually identical to the PC implementation at `0x006EE490`.

### 5.1 Wheel Loop Processing
`DoFFB` iterates over all 4 wheels (`r28 = 0 .. 3`):
1. **Lateral Tyre Slip**:
   - Queries raw lateral velocity from vehicle physics via `0x8246E718`.
   - Normalizes amplitude: `fabsf(slip)` via `fcmpu / fneg`.
   - Dispatches via `IFeedback::UpdateTireSlip(bool front, const SimSurface& surface, float slip)` at `0x8246E780` (vtable slot 7).
2. **Longitudinal Tyre Skid**:
   - Queries wheel skid velocity from physics via `0x8246E6F0`.
   - Normalizes amplitude: `fabsf(skid)` via `fcmpu / fneg`.
   - Dispatches via `IFeedback::UpdateTireSkid(bool front, const SimSurface& surface, float skid)` at `0x8246E760` (vtable slot 8).
3. **Axle Weighting**:
   - Compares wheel index: `front = (wheel < 2)` at `0x8246E738`.
4. **Road Surface & Rolling Noise**:
   - Resolves surface attribute key via `0x8246E5A0`.
   - Queries `SIMSURFACE_KEY_BLOWN_TIRE` (`0xD929E923`) at `0x8246E5CC`:
     - If the tyre is punctured/blown, reroutes road noise to the spiked blown-tire rumble waveform.
   - Dispatches via `IFeedback::UpdateRoadNoise(bool front, const SimSurface& surface, float speed)` at `0x8246E7B0` (vtable slot 6).

### 5.2 Powertrain & Gameplay State Dispatches
Following the wheel loop, `DoFFB` evaluates overall vehicle powertrain dynamics:
- **Engine RPM & Throttle**:
  - Calculates powerband ratio and overrev.
  - Calls `IFeedback::UpdateRPM(float powerband, float overrev, float throttle)` at `0x8246E8C4` (vtable slot 9).
- **Gear Shift Potential**:
  - Evaluates engine torque curve vs transmission gear ratios.
  - Calls `IFeedback::UpdateShiftPotential(ShiftPotential potential)` at `0x8246E8F4` (vtable slot 10).
- **Nitrous Oxide (NOS)**:
  - Checks if nitrous is actively discharging.
  - Calls `IFeedback::UpdateNOS(bool engaged, float nosLevel)` at `0x8246E964` (vtable slot 11).
- **Blown Engine**:
  - Checks `IEngineDamage` component.
  - Calls `IFeedback::UpdateEngineBlown(bool blown)` at `0x8246E9D8` (vtable slot 12).
- **Transmission Shifting**:
  - Checks gear transition timer.
  - Calls `IFeedback::UpdateShifting(bool shifting)` at `0x8246EA34` (vtable slot 13).
- **Suspension Travel / Frame End**:
  - Calls `IFeedback::UpdateSuspension()` at `0x8246EA48` (vtable slot 5).

### 5.3 Collision & Impact Handling
Vehicle impacts with world objects, barriers, and traffic vehicles are caught by the physics collision listener (`CollisionInfo`) and routed through `IFeedback::ReportCollision(const COLLISION_INFO& cinfo, bool iamA)`.

---

## 6. Architectural Comparison: Xbox 360 vs. PC

| Component | PC Retail (`speed.exe`) | Xbox 360 (`default.exe`) | XtendedInput Unified |
|---|---|---|---|
| **Motor Output Binding** | Stubbed out; no `XInputSetState` or DirectInput effect calls | Native `XamInputSetState` / `XInputSetState` at `0x8262D6C8` | Live XInput / WGI actuator bridge |
| **Haptic Middleware** | None (code omitted) | EA PFD 5.1 (`PFDx` banks, 50 Hz tick loop) | Synthesized `RumbleMixer` & wave generators |
| **`LocalPlayer::DoFFB`** | Active at `0x006EE490` | Active at `0x8246E420` (identical logic) | Hooked / queried via live vehicle state |
| **`IFeedback` Interface** | 15 empty stubs at vtable `0x008A7BD4` | Fully implemented / routed to PFD & controllers | Fully implemented class in plugin |
| **Tyre Slip & Skid** | Evaluated in physics; discarded at stub | Evaluated in physics; evaluated by PFD spline | Dual-band slip/skid curves (`mSlipWave`, `mSkidWave`) |
| **Surface Puncture** | Hash `0xD929E923` present in code | Hash `0xD929E923` present in code (`0x8246E5CC`) | Handled via `mBlownTireWave` |
| **Motor Mapping** | N/A | Heavy: Left (Low freq), Light: Right (High freq) | Heavy: Left (Low freq), Light: Right (High freq) |
| **Collision Impulses** | Discarded | Evaluated from contact normal & velocity | Impulse envelope applied to `mPulseLeft`/`mPulseRight` |

---

## 7. Conclusions

1. **Strict Read-Only Verification**: All reverse-engineering work was performed without modifying any sectors or bytes of `Need for Speed - Most Wanted (USA).iso`.
2. **True Source of MW Vibration**: On Xbox 360, controller rumble is powered by EA's proprietary `PFDx` 5.1 system working directly with `ControllerVibrationDevice` and `XInputSetState`.
3. **Validation of XtendedInput Design**: Martin Penkava's force-feedback reconstruction in `NFS_XtendedInput_Rumble.h` correctly guessed the role of `LocalPlayer::DoFFB`, the 15-method `IFeedback` vtable layout, the blown-tyre hash `0xD929E923`, and the separation of low-frequency engine/collision pulses from high-frequency road/slip vibrations.
