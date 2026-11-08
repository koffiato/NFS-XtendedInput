# ProStreet feedback research

PC and Xbox 360 share the Black Box feedback interface architecture, but PC ships
with stubbed controller effects while Xbox 360 implements the complete native force
feedback and dual-motor vibration engine. This document consolidates the recovered
Xbox 360 rumble subsystem, its 24-slot feedback interface, effect state array,
wave/ramp evaluators, collision calculation formulas, motor output pipeline, PC
native hook points, adapter implementation rules, and its direct lineage from Most
Wanted (2005) and Carbon (2006).

## Binaries and address mapping

| Input | Size | SHA-256 |
| --- | ---: | --- |
| PC Retail Live `prostreet-live.exe` | 24,657,920 | `36d7d61702cbda6e00a88f0a317a56c7a73d83eee50b892dc0e16a64ea9d1a1d` |
| Retail `default.xex` | 4,812,800 | `4f8b623a2a1ed06555b57da4e70411574ac962bce4b81bb100629ecc518275c2` |
| XexTool expanded image `default.exe` | 18,087,936 | `49692cddb8a8f666c4f1acec3f8c7df933298d538acb480f2a1f9d877b6d37bd` |

PC addresses follow standard Win32 PE image mapping based at `0x00400000`.

The Xbox 360 expanded image (`default.exe`) was extracted using `XexTool -e d default.xex default.exe`.
It loads at base address `0x82000000`, with memory mapping rule `fileOffset = VA - 0x82000000`.
Its `.text` section spans `0x82140000 - 0x82ADCED4`, with entry point at `0x82A679C8`.
Original PE timestamp: `0x47266B4C` (Tue Oct 30 02:22:52 2007). Title ID: `45410822` (EA-2082).

## Feedback interface and producers

The abstract interface `IFeedback` is embedded at offset `+0x2C` inside the game's concrete
device class `GameDevice` (owner list at interface `+4`).
- **PC:** Original `GameDevice` factory is at `0x006C9520`. Interface query `InputDevice::GetInterfaces`
  is at Slot 9 (`+0x24`), getter `0x006C7F20`. Interface registration handle / type descriptor at
  `0x004CD5D0` returns string `"IFeedback"` at `0x009F97A4`. Registration into the device interface
  list uses `IList::Add` (`0x00461420`) and lookup uses `IList::Find` (`0x004614B0`).
  The concrete PC vtable is at `0x0098BE6C` (21 slots).
- **Xbox 360:** Interface type descriptor at `0x822A7230` returns string `"IFeedback"` at `0x820867C4`.
  The outer `GameDevice` vtable is at `0x820B33A8`. The concrete `IFeedback` vtable starts at `0x820B33F0`
  (24 slots: 21 core slots + 3 secondary/extension methods). The inactive/stub vtable is at `0x820B3450`.

### Vtable slot layout and platform comparison

| Slot | Method / Event | PC Implementation | Xbox 360 Implementation | Signature and Behavior |
| ---: | --- | --- | --- | --- |
| 0 | Destructor | `0x006C7F60` (thunk) | `0x825B2FF8` (thunk) | `this` adjustment `-0x2C`, jumps to `GameDevice` destructor (`0x825A9C68` on X360) |
| 1 | PauseEffects | `0x006A5A40` | `0x825A9DC8` | Clears `Enabled` byte (`+0x250`), calls `Push` (`0x825A90F0`) |
| 2 | ResumeEffects | `0x00402CC0` (`ret`) | `0x825A9E10` | Sets `Enabled` byte (`+0x250`) to 1, calls `Push` (`0x825A90F0`) |
| 3 | ResetEffects | `0x00890020` | `0x825A9E58` | If output device present: zeroes motor volumes `+0x240/+0x244`, zeroes collision ramp `+0x180`, clears `Enabled` byte, calls `Push`. Otherwise no-op |
| 4 | BeginUpdate | `0x00402CC0` (`ret`) | `0x825A9F18` | Resets master scale `+0x248` to 1.0f; zeroes wave parameters (amp/freq/offset) for 5 wave groups, preserving phases and all 12 ramps |
| 5 | EndUpdate | `0x00402CC0` (`ret`) | `0x825A9F98` | Empty return (`blr`) |
| 6 | UpdateRoadNoise | `0x0044AC10` (`ret 0xC`) | `0x825A9FA0` | Key `0xBA1297DA` (`FFB_ROLL`), 4 attribute indices, interpolates via `0x825B68F0`, updates wave Group 0 (`+0x000`) |
| 7 | UpdateTireSkid | `0x0082D870` (`ret 0x14`)| `0x825AA098` | Key `0x0C149044` (`FFB_SKID`), 4 attribute indices, applies front skid angle scaling, updates wave Group 1 (`+0x040`) |
| 8 | UpdateTireSlip | `0x0044AC10` (`ret 0xC`) | `0x825AA1E0` | Key `0x8FD11D27` (`FFB_SLIP`), 4 attribute indices, updates wave Group 2 (`+0x080`) |
| 9 | UpdateRPM | `0x0044AC10` (`ret 0xC`) | `0x825AA308` | Empty return (`blr`); RPM produces no controller vibration |
| 10 | UpdateShiftPotential | `0x007C71A0` (`ret 4`) | `0x825AA310` | Evaluates potential values 3, 4, 5 (amps 0.5, 0.75, 1.0); duration 3.0s; drives ramps `+0x1A0/+0x1B0`; cached state at `GameDevice + 0x73C` |
| 11 | UpdateNOS | `0x007B55E0` (`ret 8`) | `0x825AA568` | Rising edge starts ramp `+0x1C0` (Amp: 1.0, Floor: 0.5, Duration: 2.0s); falling edge clears ramp; engaged state at `GameDevice + 0x738` |
| 12 | UpdateEngineBlown | `0x007C71A0` (`ret 4`) | `0x825AA4B0` | Drives Left ramp `+0x1E0` (1.0 / 0.6) and Right ramp `+0x1F0` (rises to 0.6); duration 1.0s; state at `GameDevice + 0x739` |
| 13 | UpdateShifting | `0x007C71A0` (`ret 4`) | `0x825AA620` | Drives Left ramp `+0x200` & Right ramp `+0x210` (Amp: 0.4, Floor: 0.4, Duration: 0.5s); state at `GameDevice + 0x73A` |
| 14 | Reserved / Unused | `0x007C71A0` (`ret 4`) | `0x825AA708` | Empty return (`blr`) |
| 15 | UpdateDriftSlip | `0x0044AC10` (`ret 0xC`) | `0x825AA710` | Key `0x8FD11D27` (`FFB_SLIP`), evaluates drift slip telemetry, updates wave Group 4 (`+0x100`) |
| 16 | UpdateDriftSkid | `0x0087E0A0` (`ret 0x10`)| `0x825AA870` | Evaluates drift skid angle and velocity, updates wave Group 5 (`+0x140`) |
| 17 | SteeringForce | `0x007C71A0` (`ret 4`) | `0x825A8CA8` | Empty return (`blr`) on console controller; wheel forces handled separately |
| 18 | UpdateRoadWithIndex | `0x0044AC10` (`ret 0xC`) | `0x825AAA18` | Road bump with wheel index (0..3); queries surface attribute `0xE288A6B0`; drives ramps `+0x220` (Left: wheels 0,2) and `+0x230` (Right: wheels 1,3) |
| 19 | ReportCollision | `0x0082D870` (`ret 0x14`)| `0x825AAB88` | Collision calculations, contact type bitfield extraction, drives Left ramp `+0x180` |
| 20 | GetControllerType | `0x006AD170` | `0x82573910` | Device type query (returns controller classification) |
| 21 | SecondaryDeviceQuery | N/A | `0x82573930` | Xbox 360 device extension query |
| 22 | SecondaryDeviceType | N/A | `0x82573940` | Xbox 360 device interface query |
| 23 | StubEntry | N/A | `0x825A93D0` | Null handler / return |

### Producer calling sequence

Feedback production is driven by `LocalPlayer::DoFFB`:
- **PC:** `0x004D12D0` (vtable `0x009F9FB4`), delegating to producer function `0x004D0CF0`.
- **Xbox 360:** `0x822AE9F8` (vtable `0x82087884`), delegating to producer function `0x822AE230`.

The producer sequence executes as follows:
1. **CanDoFFB Validation:** `0x822AE020` on X360 (`0x004D1200` on PC). Checks player profile rumble byte at `settings + 0x1C` (`0xBFB400 + 0x1C` on PC), pause state, demo/replay state, and human AI control. If any check fails, returns immediately.
2. **Lifecycle Start:** Calls `ResumeEffects` (Slot 2) and `BeginUpdate` (Slot 4).
3. **Four-Wheel Sampling Loop (wheels 0 to 3):**
   - Resolves surface pointer. Damaged tyre replaces surface with `0xD929E923`. Airborne wheels use null surface.
   - Calls **Slot 18 (`UpdateRoadWithIndex`)** passing wheel index (0..3), native road scalar, and surface reference. Note that ProStreet calls road noise **first**, with explicit wheel index routing, whereas Most Wanted called it last.
   - Calls **Slot 8 (`UpdateTireSlip`)** passing surface, longitudinal slip, and lateral utilization.
   - Calls **Slot 7 (`UpdateTireSkid`)** passing surface, skid angle, and slip velocity.
   - If drift physics are active, calls **Slot 16 (`UpdateDriftSkid`)**.
4. **Car Status Updates:**
   - Calls **Slot 9 (`UpdateRPM`)**.
   - Calls **Slot 10 (`UpdateShiftPotential`)**.
   - Calls **Slot 11 (`UpdateNOS`)**.
5. **Engine Blown vs Shifting Mutually Exclusive Branch:**
   ```c
   if (pVehicle->IsEngineBlown()) {
       pFeedback->UpdateEngineBlown(true); // Slot 12
   } else {
       pFeedback->UpdateShifting(pVehicle->IsShifting()); // Slot 13
   }
   ```
6. **Lifecycle End:** Calls `EndUpdate` (Slot 5).

## Console effect state

Xbox 360 allocates the concrete effect state array at `0x82B97D00`.
- Stride: `0x254` bytes (596 bytes) per controller port.
- Capacity: 8 ports (`0x12A0` bytes total).

| Offset Range | Stride / Count | Data Structure | Contents |
| --- | ---: | --- | --- |
| `+0x000 .. +0x03F` | `0x10` × 4 | `{float phase, amplitude, freq, offset}` | Wave Group 0: Road Roll Noise (`FFB_ROLL`) |
| `+0x040 .. +0x07F` | `0x10` × 4 | `{float phase, amplitude, freq, offset}` | Wave Group 1: Tire Skid (`FFB_SKID`) |
| `+0x080 .. +0x0BF` | `0x10` × 4 | `{float phase, amplitude, freq, offset}` | Wave Group 2: Tire Slip (`FFB_SLIP`) |
| `+0x0C0 .. +0x0FF` | `0x10` × 4 | `{float phase, amplitude, freq, offset}` | Wave Group 3: Reserved / Secondary surface waves |
| `+0x100 .. +0x13F` | `0x10` × 4 | `{float phase, amplitude, freq, offset}` | Wave Group 4: Drift Slip (`FFB_SLIP`) |
| `+0x140 .. +0x17F` | `0x10` × 4 | `{float phase, amplitude, freq, offset}` | Wave Group 5: Drift Skid (`FFB_SKID`) |
| `+0x180 .. +0x18F` | `0x10` | `{float remaining, duration, amp, floor}` | Ramp 0 (Left): Collision Impact |
| `+0x190 .. +0x19F` | `0x10` | `{float remaining, duration, amp, floor}` | Ramp 1 (Right): Collision (Unused / zeroed) |
| `+0x1A0 .. +0x1AF` | `0x10` | `{float remaining, duration, amp, floor}` | Ramp 2 (Left): Shift Potential |
| `+0x1B0 .. +0x1BF` | `0x10` | `{float remaining, duration, amp, floor}` | Ramp 3 (Right): Shift Potential |
| `+0x1C0 .. +0x1CF` | `0x10` | `{float remaining, duration, amp, floor}` | Ramp 4 (Left): NOS |
| `+0x1D0 .. +0x1DF` | `0x10` | `{float remaining, duration, amp, floor}` | Ramp 5 (Right): NOS (Unused) |
| `+0x1E0 .. +0x1EF` | `0x10` | `{float remaining, duration, amp, floor}` | Ramp 6 (Left): Engine Blown |
| `+0x1F0 .. +0x1FF` | `0x10` | `{float remaining, duration, amp, floor}` | Ramp 7 (Right): Engine Blown |
| `+0x200 .. +0x20F` | `0x10` | `{float remaining, duration, amp, floor}` | Ramp 8 (Left): Shifting |
| `+0x210 .. +0x21F` | `0x10` | `{float remaining, duration, amp, floor}` | Ramp 9 (Right): Shifting |
| `+0x220 .. +0x22F` | `0x10` | `{float remaining, duration, amp, floor}` | Ramp 10 (Left): Road Bump (wheels 0 and 2) |
| `+0x230 .. +0x23F` | `0x10` | `{float remaining, duration, amp, floor}` | Ramp 11 (Right): Road Bump (wheels 1 and 3) |
| `+0x240` | 4 bytes | `float` | Left Motor Final Volume (0.0 .. 1.0) |
| `+0x244` | 4 bytes | `float` | Right Motor Final Volume (0.0 .. 1.0) |
| `+0x248` | 4 bytes | `float` | Master Feedback Gain Scale (default 1.0) |
| `+0x24C` | 4 bytes | `uint32_t` | Reserved / alignment padding |
| `+0x250` | 1 byte | `uint8_t` | Enabled flag (1 = active, 0 = paused/disabled) |

### Persistent event state fields

Unlike the per-tick wave array, persistent trigger tracking fields are located in `GameDevice`:

| Field | `GameDevice` Offset | Data Type | Function |
| --- | ---: | --- | --- |
| `mNOSEngaged` | `+0x738` | `uint8_t` | Tracks active NOS state for rising/falling edge detection |
| `mEngineBlown` | `+0x739` | `uint8_t` | Tracks blown engine state for rising/falling edge detection |
| `mShifting` | `+0x73A` | `uint8_t` | Tracks manual/automatic gearshift animation state |
| `mShiftPotential` | `+0x73C` | `uint32_t` | Previous shift potential level (clears ramps when returning to 0) |

## Surface waves and evaluators

### Attribute interpolator (`0x825B68F0`)

Surface wave records are stored as 8-float attribute packets in `GLOBAL/attributes.bin`:
`{float xMin, float y0, float dy, float xMax, ...}`.
The interpolation helper evaluates:
```c
float InterpolateAttribute(const float* record, float input) {
    float xMin = record[0];
    float y0   = record[1];
    float dy   = record[2];
    float xMax = record[3];
    if (input >= xMin && xMin < xMax) {
        float fraction = (input - xMin) / (xMax - xMin);
        if (fraction > 1.0f) fraction = 1.0f;
        return y0 + dy * fraction;
    }
    return 0.0f;
}
```

### Surface keys and parameters

1. **Road Noise (`FFB_ROLL`, `0xBA1297DA`):**
   Queried in Slot 6. Reads 4 native attribute indices across wheels. Independent maximum taken across wheels.
2. **Tire Skid (`FFB_SKID`, `0x0C149044`):**
   Queried in Slot 7. Front-wheel skid angle input is scaled by 0.25; rear wheels take 1.0. Updates wave Group 1.
3. **Tire Slip (`FFB_SLIP`, `0x8FD11D27`):**
   Queried in Slot 8. Evaluates longitudinal slip and lateral utilization. Updates wave Group 2.
4. **Drift Slip and Skid (Slots 15 & 16):**
   Evaluates drift telemetry using keys `0x8FD11D27` and `0x0C149044`, updating wave Groups 4 (`+0x100`) and 5 (`+0x140`).
5. **Road Bump (`FFB_BUMP`, `0xE288A6B0`):**
   Queried in Slot 18 with explicit wheel index. Wheels 0 and 2 drive Left ramp `+0x220`; wheels 1 and 3 drive Right ramp `+0x230`.

### Native wave evaluator (`0x825A7640`)

Phase advances only when `amplitude * frequency > 0`. Phase wraps modulo 1.0. Sine is evaluated via lookup `0x82A21BE8`:
```c
void EvaluateWave(WaveState* wave, float dt) {
    if (wave->amplitude * wave->frequency > 0.0f) {
        wave->phase += wave->frequency * dt;
        if (wave->phase > 1.0f) {
            wave->phase -= 1.0f;
        }
        wave->output = wave->offset + wave->amplitude * sinf(wave->phase * 6.2831853f);
    } else {
        wave->output = 0.0f;
    }
}
```

## Ramps and collisions

### Native ramp evaluator (`0x825A75D8`)

Ramps advance by subtracting elapsed seconds `dt` from `remaining`. If `remaining > 0` and `duration > 0`:
$$\text{output} = \max\left(0.0,\; \text{floor} + \frac{\text{amplitude} - \text{floor}}{\text{duration}} \times \text{remaining}\right)$$

| Event | Ramp Offset | Amplitude / Floor | Duration | Motor Routing and Retrigger Rules |
| --- | --- | --- | --- | --- |
| **Collision** | Left: `+0x180` | `strength` / 0.0 | `strength` sec | Left motor only. `remaining`, `duration`, and `amplitude` each take $\max(\text{existing}, \text{strength})$. Floor is 0.0. Right ramp `+0x190` remains 0. |
| **Shift Potential** | Left: `+0x1A0`<br>Right: `+0x1B0` | Level 3: 0.5 / 0.5<br>Level 4: 0.75 / 0.75<br>Level 5: 1.0 / 1.0 | 3.0 sec | Both motors. Triggered when potential reaches 3, 4, or 5. A potential drop clears both ramps immediately on the next tick. |
| **NOS** | Left: `+0x1C0` | 1.0 / 0.5 | 2.0 sec | Left motor only. Rising edge (`engaged == 1 && !mNOSEngaged`) triggers ramp. Falling edge clears ramp (`remaining = 0`). |
| **Engine Blown** | Left: `+0x1E0`<br>Right: `+0x1F0` | Left: 1.0 / 0.6<br>Right: rises to 0.6 | 1.0 sec | Both motors. Rising edge triggers ramp. Left motor starts at 1.0 decaying to 0.6; Right motor starts at 0.0 and ramps up toward 0.6. |
| **Shifting** | Left: `+0x200`<br>Right: `+0x210` | 0.4 / 0.4 | 0.5 sec | Both motors. Rising edge triggers flat pulse (Amp: 0.4, Floor: 0.4). Falling edge allows pulse to finish playing out. |
| **Road Bump** | Left: `+0x220`<br>Right: `+0x230` | `bumpAmp` / 0.0 | 0.2 sec | Spatial stereo: Left ramp driven by wheels 0 and 2; Right ramp driven by wheels 1 and 3. |

### Collision calculation (`ReportCollision` `0x825AAB88`)

Collision arguments: `this`, `CollisionInfo* info`, `bool isLocal`, `Vector3 velocityVector` (12 bytes by value).

1. **Contact Type Extraction:**
   Contact flags word is located at `CollisionInfo + 0x1C`. The contact category is extracted using PowerPC arithmetic right shift by 29 bits:
   ```asm
   lwz   r11, 0x1C(r4)
   srawi r11, r11, 29
   ```
   - `type == 1`: Dynamic Object contact (vehicles, track props, traffic)
   - `type == 2`: World barrier / scenery contact
   - `type == 3`: Ground contact (road / terrain landing)

2. **Mathematical Strength Formulas:**
   - **World Contact (`type == 2`):**
     $$\text{strength} = \text{clamp}\left(\frac{\Delta\text{Speed}}{30.0},\; 0.0,\; 1.0\right)$$
   - **Ground Contact (`type == 3`):**
     $$\text{strength} = \text{clamp}\left(\frac{\Delta\text{Speed} - 1.0}{3.0},\; 0.0,\; 1.0\right)$$
   - **Object Contact (`type == 1`), High Speed Delta ($\Delta\text{Speed} \ge 5.0\text{ m/s}$):**
     $$\text{strength} = \text{clamp}\left(\frac{\Delta\text{Speed} - 5.0}{20.0},\; 0.0,\; 1.0\right)$$
   - **Object Contact (`type == 1`), Low Speed Delta ($\Delta\text{Speed} < 5.0\text{ m/s}$):**
     Queries participant B vehicle interface. If both vehicles have live speed $> 2.0\text{ m/s}$, evaluates closing velocity:
     $$\text{strength} = \text{clamp}\left(\frac{\text{liveSpeed} - 20.0}{30.0},\; 0.0,\; 1.0\right) \times 0.75$$

3. **Collision Ramp Injection:**
   Strength feeds directly into Left ramp `+0x180`:
   ```c
   ramp->remaining = fmaxf(ramp->remaining, strength);
   ramp->duration  = fmaxf(ramp->duration,  strength);
   ramp->amplitude = fmaxf(ramp->amplitude, strength);
   ramp->floor     = 0.0f;
   ```

## Timing, lifecycle, and motor output

### Polling loop execution order

The console device updater at `0x825A93E0` maintains a strict **Push-then-Run** execution sequence:
1. `0x825A93F8`: Calls `Push` (`0x825A90F0`), transmitting the volumes calculated in the **previous** tick to hardware.
2. `0x825A9404`: Calls `Run` (`0x825A8CE0`), advancing delta time, sampling waves/ramps, and calculating the volumes for the **next** tick.

This order eliminates frame-boundary jitter and mirrors the architecture of Most Wanted and Carbon.

### Master mixer (`Run` `0x825A8CE0`)

1. Evaluates all wave groups (0 to 5) using `0x825A7640`.
2. Evaluates all 12 ramps using `0x825A75D8`.
3. Combines channels:
   $$\text{LeftSum} = \left(\sum \text{Wave}_{\text{left}} + \max(\text{Ramps}_{\text{even}})\right) \times \text{MasterScale}$$
   $$\text{RightSum} = \left(\sum \text{Wave}_{\text{right}} + \max(\text{Ramps}_{\text{odd}})\right) \times \text{MasterScale}$$
4. Clamps final volumes to $[0.0,\; 1.0]$ and writes to `+0x240` (Left) and `+0x244` (Right).

### Hardware output (`Push` `0x825A90F0`)

1. If `Enabled` (`+0x250`) is 0, motor levels are forced to 0.
2. Converts float volumes to 16-bit unsigned integers:
   $$\text{u16Left} = (\text{uint16\_t})\text{clamp}(\text{LeftVolume} \times 65535.0,\; 0,\; 65535)$$
   $$\text{u16Right} = (\text{uint16\_t})\text{clamp}(\text{RightVolume} \times 65535.0,\; 0,\; 65535)$$
3. Calls XInput dispatcher `0x82A649F0`, which invokes `XamInputSetState` import `0x82ADBFB4` (ordinal `0x192`).

### Lifecycle transitions

| Operation | State Transitions |
| --- | --- |
| **PauseEffects** (`0x825A9DC8`) | Sets `Enabled` byte `+0x250` = 0; calls `Push` (zeroes physical motors immediately). Preserves all active waves, ramps, and volumes. |
| **ResumeEffects** (`0x825A9E10`) | Sets `Enabled` byte `+0x250` = 1; calls `Push` (restores active motor volumes). |
| **ResetEffects** (`0x825A9E58`) | Zeroes final volumes `+0x240/+0x244`; zeroes Collision ramp `+0x180`; sets `Enabled` = 0; calls `Push`. Preserves car ramps. |

## PC interface and adapter implementation

### Native PC hooks and accessors

The PC adapter hooks into the game's native feedback dispatch chain without modifying
gameplay physics or input processing:
- **GameDevice factory:** `0x006C9520`.
- **Interface getter:** `0x006C7F20` (`InputDevice::GetInterfaces`, slot 9 / `+0x24`).
- **Interface list registration:** `IList::Add` (`0x00461420`) and `IList::Find` (`0x004614B0`).
- **Collision listeners:** The two local-player collision listener entries in `LocalPlayer` vtable
  at `0x009FA07C` and `0x009FA324` route to handler `0x004D13F0`. The adapter inspects both collision
  participants, confirms local player car ownership, and resolves participant velocities.
- **Surface collection/layout offsets:** Located at `+0/+4` (unlike MW's `+4/+8`).
- **Native attribute accessor:** ProStreet's `Attrib::Instance` accessor is at `0x0052B950`.

### Adapter runtime behavior

| Area | Adapter Implementation |
| --- | --- |
| **Collisions** | Evaluates native contact type bits (`srawi 29`), applies recovered World (`/30`), Ground (`-1/3`), and Object (`-5/20`) formulas into Left ramp `+0x180`. |
| **Road noise** | Passes explicit wheel index (0..3) from Slot 18; drives Left ramp `+0x220` (wheels 0, 2) and Right ramp `+0x230` (wheels 1, 3). |
| **Tire telemetry** | Samples native `Attrib::Instance` (`0x0052B950`) records for `FFB_ROLL`, `FFB_SKID`, and `FFB_SLIP`. Operates on native ranges (slip: 0.05..1.0 ratio, skid: 0.1..1.0 rad). Motion gate suppresses stationary tyre squeal. |
| **Car ramps** | Shift potential, NOS, engine damage, and gearshifts drive recovered linear ramps matching the console timing. |
| **Lifecycle safety** | Window focus loss and missing ticks suspend motor output without resetting internal ramp ages; device detachment clears state. |
| **Steering wheels** | Native DirectInput force feedback retains the original game handlers; gamepad rumble routes through XInput/WGI. |

## Diagnostics and validation

Enable telemetry by setting `[Rumble] Telemetry = 1` in `scripts/NFS_XtendedInput.ini`. Telemetry records are written to `scripts/NFS_XtendedInput_Rumble.log`:
- `S` rows: Native surface wave definitions queried through `Attrib::Instance`.
- `T` rows: Live telemetry per tick (slip ratios, skid angles in radians, native road scalar, vehicle speed, pedals).
- `P` rows: Raw collision listener contact participants.
- `C` rows: Collisions routed into the rumble engine with calculated strengths.

### Automated test harness

From an x86 Visual Studio developer command prompt:
```bat
build.cmd PS
tests\prostreet.cmd
```
The test harness exercises compiled vtable dispatch, stack cleanup for skid/drift/collision callbacks,
interface registration, pause/resume transitions, focus loss, and malformed telemetry handling.

## Comparison with Most Wanted and Carbon

| Architectural Feature | Most Wanted (2005) | Carbon (2006) | ProStreet (2007) |
| --- | --- | --- | --- |
| **`IFeedback` Vtable Size** | 15 slots (`0x008A7BD4` / `0x820867C8`) | 18 slots (`0x009E2188`) | 21 slots PC (`0x0098BE6C`) / 24 slots X360 (`0x820B33F0`) |
| **Effect State Stride** | `0x1B0` bytes (432 bytes) | Pending verification | `0x254` bytes (596 bytes) |
| **Supported Ports** | 4 ports (`0x6C0` total) | 4 ports | 8 ports (`0x12A0` total) |
| **Wave Groups** | 4 groups (Road, Skid, Slip, Unused) | 4 groups | 6 groups (Road, Skid, Slip, Reserved, Drift Slip, Drift Skid) |
| **Ramp Slots** | 10 ramps (5 pairs) | 10 ramps (candidate) | 12 ramps (6 pairs: Collision, Potential, NOS, Blown, Shift, Road Bump) |
| **Road Noise Routing** | Slot 6; called last; single monaural wave | Slot 6 | Slot 18; called **first**; wheel index 0..3 routes to spatial stereo ramps (`+0x220/+0x230`) |
| **Drift Mechanics** | Not present | Primitive | Dedicated slots (15 & 16) and wave groups (4 & 5) |
| **Contact Bit Extraction** | High 3 bits (`srawi r11, r11, 29`) | High 3 bits | High 3 bits (`srawi r11, r11, 29`) |
| **Collision Formulas** | World: `/30`<br>Ground: `-1/3`<br>Object: `-5/20` | World: `/30`<br>Ground: `-1/3`<br>Object: `-5/20` | World: `/30`<br>Ground: `-1/3`<br>Object: `-5/20` (Identical!) |
| **Update Order** | Push then Run (`0x823A3E30..3C`) | Push then Run | Push then Run (`0x825A93F8..04`) |
| **Low-Delta Collision** | Closing speed $> 2.0\text{ m/s}$, $(-20)/30 \times 0.75$ | Same lineage | Identical formula |

## References

- Retail executable images: PC `prostreet-live.exe` and Xbox 360 `default.exe` / `default.xex` (Title ID `45410822`).
- Most Wanted feedback research: [`mw-rumble-x360.md`](file:///C:/Stuff/Rumble/NFS-XtendedInput-unified/docs/mw-rumble-x360.md).
- Carbon feedback research: [`cb-rumble-x360.md`](file:///C:/Stuff/Rumble/NFS-XtendedInput-unified/docs/cb-rumble-x360.md).
- PC rumble adapter implementation: [`ProStreet.h`](file:///C:/Stuff/Rumble/NFS-XtendedInput-unified/src/rumble/ProStreet.h).
- Upstream XtendedInput feedback investigation: [Issue 10](https://github.com/xan1242/NFS-XtendedInput/issues/10).
