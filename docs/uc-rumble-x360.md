# Undercover feedback research (Xbox 360)

PC and Xbox 360 share the core EA Black Box feedback architecture inherited from Most Wanted (2005),
Carbon (2006), and ProStreet (2007). On PC, retail Undercover v1.0.1.18 retains the concrete 21-slot
`IFeedback` vtable and the full Havok collision physics evaluator, but completely omits gamepad
vibration output (stubs its XInput actuator slot as `ret 4` and omits `XInputSetState` from the PE IAT).
In contrast, Xbox 360 implements the complete, native force feedback and dual-motor controller
vibration engine.

This document consolidates the recovered Xbox 360 rumble subsystem, its 21-slot feedback interface,
internal circular effect buffer, `ControllerVibrationDevice` mixer, Havok collision physics evaluator,
statically-linked `XFFB` driver, XAM/XInput hardware sink, and direct platform comparison with PC
retail (`nfs.exe`).

---

## Reference binaries

| Binary | Size (bytes) | SHA-256 |
| :--- | ---: | :--- |
| PC Retail Live `nfs.exe` (v1.0.1.18) | 10,584,064 | `d1ecded04cbba387c99299d2159b29b5c04b1d095293013caa4b404a43c7d2e9` |
| Retail `default.xex` | 6,909,952 | `97ac0c072253d46458dc6d57c6609922adbd3b9658216940282c361ad575e221` |
| XexTool expanded image `default.exe` | 25,034,752 | `befa136b20e47fc62b8129ccb32b97e6163ddb0bff52760d532788fc3e925c71` |

- **Xbox 360 Title ID:** `45410876` (EA-2166).
- **PE TimeDateStamp:** `0x48FD21F7` (Tue Oct 21 00:27:35 2008 UTC).
- **Entry Point RVA:** `0x00D5D960` (VA `0x82D5D960`).
- **Base Address:** `0x82000000`.

### Section memory layout & displacement skew

| Section | Virtual Address | Virtual Size | File Offset | Raw Size | Characteristics |
| :--- | :---: | :---: | :---: | :---: | :--- |
| `.rdata` | `0x82000400` | `0x00172280` | `0x00000400` | `0x00172400` | Read-only data & VTables |
| `.pdata` | `0x82172800` | `0x0005F9C8` | `0x00172800` | `0x0005FA00` | Exception directory & unwind |
| `.text` | `0x821E0000` | `0x00EA5E2C` | `0x001D2200` | `0x00EA6000` | PowerPC executable code |
| `.data` | `0x83090000` | `0x006D0538` | `0x01078200` | `0x00080600` | Globals, IAT & variables |

> [!NOTE]
> In XexTool's expanded PE image, `.text` begins at raw file offset `0x001D2200` with virtual base `0x821E0000`.
> The displacement between virtual addresses and raw offsets creates a constant displacement:
> $$\Delta = \text{VA}_{\text{text}} - \text{RawOffset}_{\text{text}} = 0x821E0000 - 0x821D2200 = 0xDE00$$
> Function pointers stored in `.rdata` virtual tables represent unskewed image addresses. Adding **`+0xDE00`**
> to any stored `.rdata` function pointer resolves directly to the exact runtime instruction prologue in `.text`.

---

## Feedback interface and producers

The abstract COM interface `IFeedback` is embedded at offset `+0x2C` inside the concrete device class
`GameDevice`:
- **PC:** The concrete `IFeedback` vtable is at `0x00C0D434` (21 slots). `InputDevice::GetInterfaces`
  is at Slot 9 (`+0x24`), returning the interface list at `InputDevice + 4`.
- **Xbox 360:** The primary `GameDevice` vtable is at `0x8212CC28` (runtime `0x8261EA28`). The concrete
  `IFeedback` vtable starts at `.rdata` `0x8212CC70` (runtime `0x8261E698` – `0x8261F5E0`, 21 slots).

### VTable slot layout and platform comparison

| Slot | Displacement | Method / Event | PC Address (`nfs.exe`) | Xbox 360 Address (`default.exe`) | Signature and Semantics |
| :---: | :---: | :--- | :---: | :---: | :--- |
| **0** | `+0x00` | `Destructor` | `0x0069A860` (thunk) | `0x82621DD8` (thunk) | `addi r3, r3, -0x2c`; branches to `GameDevice` dtor (`0x8261E540`). |
| **1** | `+0x04` | `PauseEffects` | `0x006778F0` | `0x8261E698` | Pauses effect playback across all active controller channels. |
| **2** | `+0x08` | `ResumeEffects` | `0x004C7740` (`ret`) | `0x8261E6E0` | Unpauses effects and resets channel timers. |
| **3** | `+0x0C` | `ResetEffects` | `0x00677960` | `0x8261E728` | Virtual forwarder to Slot 1 (`PauseEffects`). |
| **4** | `+0x10` | `BeginUpdate` | `0x004C7740` (`ret`) | `0x8261E848` | Resets per-frame scratch accumulators and effect states. |
| **5** | `+0x14` | `EndUpdate` | `0x004C7740` (`ret`) | `0x8261E8E8` | Empty return (`blr`). Mixing runs in `ControllerVibrationDevice::Update`. |
| **6** | `+0x18` | `UpdateRoadNoise` | `0x006B0620` (`ret 0xC`) | `0x8261E8F0` | Surface roughness and wheel bump feedback. |
| **7** | `+0x1C` | `UpdateTireSkid` | `0x0054E260` (`ret 0x14`)| `0x8261E9E8` | 5 parameters: surface attribute, skid scalar, load, grip, total load. |
| **8** | `+0x20` | `UpdateTireSlip` | `0x006B0620` (`ret 0xC`) | `0x8261EB30` | 3 parameters: surface attribute, longitudinal slip, lateral scrubbing. |
| **9** | `+0x24` | `UpdateRPM` | `0x006B0620` (`ret 0xC`) | `0x8261EC68` | Stub (`blr`). (PC retail uses Logitech DirectInput periodic effects). |
| **10** | `+0x28` | `UpdateShiftPotential` | `0x006B1200` (`ret 4`) | `0x8261EC70` | Evaluates potential levels (1, 2, 3) for tachometer vibration. |
| **11** | `+0x2C` | `UpdateNOS` | `0x0064C980` (`ret 8`) | `0x8261EEE8` | Reads/writes `[GameDevice + 0x748]`; engages nitrous vibration. |
| **12** | `+0x30` | `UpdateEngineBlown` | `0x006B1200` (`ret 4`) | `0x8261EE28` | Reads/writes `[GameDevice + 0x749]`; pulses engine blown flutter. |
| **13** | `+0x34` | `UpdateShifting` | `0x006B1200` (`ret 4`) | `0x8261EFA8` | Reads/writes `[GameDevice + 0x74A]`; pulses gear shift engagement click. |
| **14** | `+0x38` | `UpdateDriftSlip` | `0x006B0620` (`ret 0xC`) | `0x8261F090` | Lateral drift telemetry and oversteer slide feedback. |
| **15** | `+0x3C` | `Reserved15` | `0x006B0620` (`ret 0xC`) | `0x8261F138` | Internal device state synchronizer. |
| **16** | `+0x40` | `UpdateDriftSkid` | `0x008ECD50` (`ret 0x10`)| `0x8261F2A8` | High drift angle sustained tire skid vibration. |
| **17** | `+0x44` | `SteeringForce` | `0x006B1200` (`ret 4`) | `0x8261D4B8` | Wheel centering torque stub (`blr`) for gamepads. |
| **18** | `+0x48` | `UpdateRoadWithIndex` | `0x006B0620` (`ret 0xC`) | `0x8261F460` | Per-wheel road surface bump injection with wheel index routing (0..3). |
| **19** | `+0x4C` | `ReportCollision` | `0x00696420` | `0x8261F5E0` | **Active physics evaluator (>2.5 KB).** Havok rigid-body impact processor. |
| **20** | `+0x50` | `GetControllerType` | `0x006822A0` | `0x825D03B8` | Returns controller classification (`0x820FEB44`). |

### Producer calling sequence (`LocalPlayer::DoFFB`)

Feedback generation executes every frame in `LocalPlayer::DoFFB` (`0x827AF7D0` – `0x827AFE34`):
1. **Feedback Device Pointer:** `LocalPlayer` holds the active `IFeedback` pointer at `this + 0x50` (`lwz r11, 0x50(r28)`).
2. **Lifecycle Start:** Dispatches `ResumeEffects` (Slot 2) and `BeginUpdate` (Slot 4) via `0x827AF95C` / `0x827AF974`.
3. **Four-Wheel Sampling Loop:**
   - Evaluates each wheel (counter `r30` = 0..3).
   - Checks blown tyre state using universal attribute hash **`0xD929E923`** (`0x827AF9FC`).
   - Queries surface friction via `Attrib::Instance::GetAttributePointer` (`0x8227E850`):
     - `GetFFBRoll` (`0x825D00F8`, attribute `0xBA1297DA`)
     - `GetFFBSkid` (`0x825D0140`, attribute `0x0C149044`)
     - `GetFFBSlip` (`0x825D0188`, attribute `0x8FD11D27`)
   - Dispatches `UpdateRoadNoise` (Slot 6), `UpdateTireSlip` (Slot 8), and `UpdateTireSkid` (Slot 7).
4. **Vehicle State Dispatches:**
   - Calls **`UpdateRPM`** (Slot 9) at `0x827AFC38`.
   - Calls **`UpdateShiftPotential`** (Slot 10) at `0x827AFC68`.
   - Calls **`UpdateNOS`** (Slot 11) at `0x827AFCD4`.
   - If blown, calls **`UpdateEngineBlown`** (Slot 12) at `0x827AFD48`; otherwise calls **`UpdateShifting`** (Slot 13) at `0x827AFDA0`.
   - Calls **`UpdateDriftSlip`** (Slot 14) at `0x827AFDF0`.
5. **Lifecycle End:** Calls **`EndUpdate`** (Slot 5) at `0x827AFE1C`.

---

## Console effect state and actuator mixer

### `GameDevice` object layout
```
[GameDevice Object Layout]
+0x000: Primary GameDevice VTable (0x8212CC28 -> runtime 0x8261EA28)
+0x02C: Concrete IFeedback VTable (0x8212CC70 -> runtime 0x8261E698)
+0x034: ControllerVibrationDevice / tRumbler (0x8212CBF8 -> runtime 0x8261C368)
        +0x008: XFFB Device Handle
        +0x00C: Active/Connected Flag (uint8)
        +0x00D: Device Enabled Flag (uint8)
        +0x010: Effect Slots Buffer (8 channels * 116 bytes = 0x3A0 bytes)
        +0x434: Last Collision Type (uint32)
        +0x438: Last Collision Impulse Intensity (float 0.0f - 1.0f)
+0x748: mNOSActive (uint8)
+0x749: mEngineBlown (uint8)
+0x74A: mShifting (uint8)
```

### Circular effect buffer
Constructed in `0x8261C4C0`, the actuator mixer zeroes `0x3A0` bytes (928 bytes) starting at `this + 0x10`.
The buffer houses **8 effect channels**, each allocated **116 bytes (`0x74`)** containing envelope timings,
motor masks, intensity levels, and playback frequencies.

### `ControllerVibrationDevice` VTable (`0x8212CBF8`)
The mixer exposes 11 virtual slots:
- **Slot 1 (`0x8261C8B8`):** `Update` / Motor Mixer. Evaluates active channel envelopes, resolves motor duty
  cycles, and transmits packets to `XFFB`.
- **Slot 9 (`0x8261BFB0`):** `TriggerCollisionFeedback(int type, float intensity)`:
  ```asm
  0x8261BFB0: stw        r4, 0x434(r3)    ; writes collision type / severity
  0x8261BFB4: stfs       f1, 0x438(r3)    ; writes normalized intensity (0.0f - 1.0f)
  0x8261BFB8: blr
  ```

### Hardware output sink (`XFFB` & `XAM`)
In Slot 1 (`0x8261C8B8`), the mixer dispatches to Microsoft `XFFB v2.0.7645.0`:
- **`0x82D68538` (`XFFB_SetDeviceState`):** Prepares device output channels.
- **`0x82D68410` (`XFFB_UpdateMotors`):** Writes motor duty cycles to dispatch pointers `[0x83111994]` and
  `[0x8311198C]`, calling `xam.xex` Ordinal `0x192` (402: `XInputSetState`).

---

## Collision interception and Havok dynamics

### `LocalPlayer::OnCollision` (`0x827AFFF0`)
The collision listener is implemented at offset `+0x4C` of `LocalPlayer` (VTable `0x8214B1BC`) and
`RecordablePlayer` (VTable `0x8214B43C`):
1. **Simable Resolution:** Extracts `cinfo->objA` (`+0x3C`) and calls `GetSimable` (`0x822532e8`).
2. **Profile Gatekeeper Check (`0x827B0078`):**
   ```asm
   0x827B0078: lbz        r11, 9(r3)       ; PlayerSettings::mRumbleEnabled (+0x09)
   0x827B007C: cmplwi     cr6, r11, 0      ; is rumble enabled in user profile?
   0x827B0080: beq        cr6, 0x827b0180  ; IF ZERO, BYPASS REPORTCOLLISION!
   ```
   > [!IMPORTANT]
   > NOPing the 4-byte conditional branch at `0x827B0080` with `60 00 00 00` (`nop`) forces collision
   > delivery regardless of savegame profile states.
3. **Participant Identification (`iamA`):**
   Compares `simableA->mHandle` with `mySimable->mHandle` (`0x827B0144` – `0x827B0154`), setting `r5 = (handleA == myHandle) ? 1 : 0`.
4. **Dispatch:** Calls `IFeedback::ReportCollision` (Slot 19 / `0x4C(r11)`), passing `cinfo` in `r4`, `iamA` in `r5`,
   and the 12-byte velocity delta vector `vlive - vpre` in `r6`–`r7`.

### Havok physics evaluator (`ReportCollision` at `0x8261F5E0`)
1. **Orientation:** Resolves `mySimable` (`r31`) and `otherSimable` (`r30`) according to `iamA`.
2. **Closing Velocity Estimation:** Queries live Havok rigid body velocity (`ISimable::GetVelocity` at Slot 24 / `0x60(r11)`),
   computes velocity delta against pre-impact velocity, and enforces the Black Box **5.0 m/s closing velocity threshold**.
3. **Collision Domains:** Evaluates `cinfo->flags >> 29`:
   - **Type 1: Vehicle-to-Vehicle (`0x8261F99C`):** Computes angle of incidence, halves estimated impulse
     (`estimate *= 0.5f`), and queries vehicle scrape attributes **`0xAEF4FB7D`** and **`0x1C2333F7`**.
   - **Type 2: World Barrier Impact (`0x8261F830`):** Projects velocity against contact normal:
     $$\text{projection} = |\mathbf{v}_{\text{closing}} \cdot \mathbf{n}_{\text{contact}}|$$
     Compares against 5.0 m/s threshold (`-0x2358(r26)`). If below 5.0 m/s, queries barrier glance/scrape
     attribute **`0x96A192E8`**; if direct structural crash, queries **`0x7125C7EC`**.
   - **Type 3: World Prop / Object (`0x8261F7B8`):** Queries prop impact attribute **`0x751D40E5`**, scaling by mass.
4. **4-Wheel Force Ramp & Dispatch:** Computes force ramps across all 4 wheels against surface friction, divides
   total force by wheel count (`f12 / f11`), and invokes Actuator Slot 9 (`0x8261FDE8`) to play the impulse.

---

## Direct platform parity analysis

| Architectural Component | PC Retail v1.0.1.18 (`nfs.exe`) | Xbox 360 Retail (`default.exe`) | Parity Status |
| :--- | :--- | :--- | :--- |
| **`IFeedback` VTable** | `0x00C0D434` (21 slots, `+0x2C`) | `0x8212CC70` (21 slots, `+0x2C`) | **100% Identical:** Same 21 slots, same displacements, same parameters. |
| **`XInputSetState`** | Absent from PE IAT (`XINPUT1_3.dll`) | Imported (`xam.xex` Ordinal 402) | PC omitted hardware actuation; console uses native driver. |
| **Actuator Engine** | Stubbed as `ret 4` (`0x006B1200`) | Native `ControllerVibrationDevice` (`0x8261C2D0`)| Console maintains 8-slot, 928-byte circular mixer buffer. |
| **`ReportCollision`** | `0x00696420` (>2.5 KB evaluator) | `0x8261F5E0` (>2.5 KB evaluator) | **100% Identical:** Both binaries house full Havok collision evaluator. |
| **Gatekeeper Jump** | `0x007BAD13` (`je 0x007BAE26`) | `0x827B0080` (`beq 0x827b0180`) | **Identical:** Checks `PlayerSettings::mRumbleEnabled` (byte `+0x09`). |
| **Blown Tyre Hash** | `0xD929E923` (`0x007BC84C`) | `0xD929E923` (`0x827AF9FC`) | **100% Identical:** Universal Black Box engine flat tyre attribute. |
| **Closing Velocity Gate**| `5.0 m/s` (`0x00B2B284`) | `5.0 m/s` (`-0x2358(r26)`) | **100% Identical:** Same glance vs direct crash velocity threshold. |
| **Barrier Scrape Key** | `0x96A192E8` | `0x96A192E8` (`0x8261F8D8`) | **100% Identical:** Shared surface friction attribute key. |
| **Barrier Impact Key** | `0x7125C7EC` | `0x7125C7EC` (`0x8261F948`) | **100% Identical:** Shared structural crash attribute key. |
| **Vehicle Scrape Key** | `0xAEF4FB7D` | `0xAEF4FB7D` (`0x8261FAE0`) | **100% Identical:** Shared vehicle glancing scrape attribute key. |
| **Vehicle Crash Key** | `0x1C2333F7` | `0x1C2333F7` (`0x8261FB68`) | **100% Identical:** Shared vehicle T-bone impact attribute key. |
| **World Prop Key** | `0x751D40E5` | `0x751D40E5` (`0x8261F7D8`) | **100% Identical:** Shared destructible prop attribute key. |

---

## Integration blueprint for `NFS-XtendedInput`

1. **Leverage PC Havok Physics:** PC retail already contains the intact `ReportCollision` evaluator (`0x00696420`). XtendedInput does not need to re-implement impact physics or estimate velocity changes manually.
2. **Hook Concrete `IFeedback` (`0x00C0D434`):** Replace `GameDevice + 0x2C` with an XtendedInput `IFeedback` adapter, routing all 21 slots to our modern XInput motor mixer.
3. **Capture Slot 19 (`ReportCollision`):** Intercept the pre-calculated, normalized collision events, shaping them into tactile heavy/light motor pulses using the console envelope model (300 ms duration, 50 ms attack).
4. **Patch Profile Gatekeeper (`0x007BAD13`):** NOP the 6-byte gatekeeper jump on PC (`90 90 90 90 90 90` at `0x007BAD13`), ensuring collision delivery regardless of user savegame state.
5. **Console Modding / Emulator Parity:** On Xbox 360 title patches, NOP `0x827B0080` with `60 00 00 00` (`nop`) to force native rumble playback under all conditions.
