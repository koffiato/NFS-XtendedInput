# Need for Speed: Undercover feedback research

PC and console versions of Undercover inherit the core Black Box feedback architecture from Most
Wanted (2005), Carbon (2006), and ProStreet (2007). On PC, retail Undercover ships with a complete
concrete 21-slot `IFeedback` vtable, full MSVC RTTI, an active physical collision evaluator
(`0x00696420`), and an embedded Logitech Gaming SDK (`LogitechEZWheelWrapper`) supporting 10 force
feedback effect classes. However, PC retail completely omits gamepad vibration output: its XInput
controller wrapper stubs the actuator method as `ret 4`, and `XInputSetState` is absent from the
executable import table.

This document provides a comprehensive technical analysis of Undercover's input, force feedback, and
collision subsystems on PC (`nfs.exe`), mapping its concrete vtables, calling producers, collision
mechanics, hardware wrappers, and the exact integration points required for the XtendedInput rumble
adapter.

---

## Binaries and address mapping

| Input | Size | SHA-256 |
| :--- | ---: | :--- |
| PC Retail Live `nfs.exe` | 10,584,064 bytes | `d1ecded04cbba387c99299d2159b29b5c04b1d095293013caa4b404a43c7d2e9` |

- **Architecture:** PE32 x86 (standard Win32 executable).
- **TimeDateStamp:** `0x49372C65` (Thu Dec 4 01:05:41 2008, PC v1.0.1.18 retail patch).
- **Image Base:** `0x00400000`.

### PE section virtual memory layout

| Section | Virtual Address Range | File Offset | Raw Size | Characteristics |
| :--- | :---: | :---: | :---: | :--- |
| `.text` | `0x00401000 - 0x00BD5E5B` | `0x00001000` | 8,212,480 bytes (`0x7D5000`) | Executable code |
| `.rdata` | `0x00BD6000 - 0x00D1E5EC` | `0x007D6000` | 1,347,584 bytes (`0x149000`) | Read-only data, vtables, RTTI COLs |
| `.data` | `0x00D1F000 - 0x013721D4` | `0x0091F000` | 409,600 bytes (`0x064000`) | Globals, RTTI TypeDescriptors |
| `.reloc` | `0x01376000 - 0x01407876` | `0x00986000` | 598,016 bytes (`0x092000`) | Base relocation table |

---

## RTTI hierarchy and device architecture

Undercover PC retains complete MSVC Run-Time Type Information (2,271 symbols). The feedback and
input subsystem is structured across several interacting object hierarchies:

| Class Name | TypeDescriptor | Object Offset | VTable Address | Description |
| :--- | :---: | :---: | :---: | :--- |
| `.?AVIFeedback@@` | `0x00D3B768` | `+0x00` | `0x00C0CFC4` | Abstract base interface: 21 slots (dtor + 20 purecalls) |
| `.?AVGameDevice@@` (Primary) | `0x00D3B9CC` | `+0x00` | `0x00C0D3DC` | Primary device object; Slot 9 queries interface list |
| `.?AVGameDevice@@` (`IFeedback`) | `0x00D3B9CC` | `+0x2C` | `0x00C0D434` | **Concrete `IFeedback` vtable** embedded at offset `+0x2C` |
| `.?AVInputDevice@@` | `0x00D3B954` | `+0x00` | `0x00C0D324` | Abstract base input device class |
| `.?AVSteeringWheelDevice@@` | `0x00D3B9A8` | `+0x00` | `0x00C0D378` | DirectInput wheel device wrapper |
| `.?AVLogiGameControllerXInput@@` | `0x00D518B0` | `+0x00` | `0x00C2087C` | Logitech XInput wrapper; polls `XInputGetState` |
| `.?AVLogiGameControllerDI@@` | `0x00D51874` | `+0x00` | `0x00C2084C` | Logitech DirectInput controller wrapper |
| `.?AVForceFeedbackSettings@@` | `0x00D2A28C` | `+0x00` | `0x00BE67B0` | Global force feedback configuration manager |
| `.?AVPlayerSettings@@` | `0x00D2A214` | `+0x00` | `0x00BE6790` | Player profile settings container |
| `.?AVLocalPlayer@@` (Primary) | `0x00D54DC0` | `+0x00` | `0x00C24AF8` | Local human player representation |
| `.?AVLocalPlayer@@` (`DoFFB`) | `0x00D54DC0` | `+0x04` | `0x00C24AF0` | Single-slot interface: `DoFFB(ISimable*, dt)` |
| `.?AVLocalPlayer@@` (Collision) | `0x00D54DC0` | `+0x4C` | `0x00C24A14` | `Sim::Collision::IListener` collision hook point |
| `.?AVRecordablePlayer@@` | `0x00D54E18` | `+0x4C` | `0x00C24C00` | Replay/recordable collision listener |
| `.?AVOnlineLocalPlayer@@` | `0x00D54E5C` | `+0x4C` | `0x00C24E24` | Online local player collision listener |

### Interface registration and identification handles

- **`IFeedback::_IHandle` (`0x00676D00`):**
  ```asm
  00676D00: B8 10 CE C0 00   mov eax, 00C0CE10h   ; ASCII "IFeedback"
  00676D05: C3               ret
  ```
- **`ISteeringWheel::_IHandle` (`0x00666E20`):**
  ```asm
  00666E20: B8 E0 B5 C0 00   mov eax, 00C0B5E0h   ; ASCII "ISteeringWheel"
  00666E25: C3               ret
  ```

Device lookup is handled by `LocalPlayer::SetFeedbackDevice(int port)` at `0x007BC700`:
1. Resolves device manager via `call 0x00682120`.
2. Queries interfaces via `InputDevice::GetInterfaces` (Slot 9, offset `+0x24`).
3. Finds `IFeedback` interface using handle `0x00676D00` via `IList::Find` (`0x005F6700`), storing pointer at `player + 0x0C`.
4. Finds `ISteeringWheel` interface using handle `0x00666E20`, storing pointer at `player + 0x10`.

---

## Concrete `IFeedback` vtable layout (`0x00C0D434`)

The concrete `IFeedback` vtable embedded inside `GameDevice` at offset `+0x2C` has 21 slots. Stack
cleanup conventions and argument counts match ProStreet's 21-slot layout slot-for-slot:

| Slot | Displacement | Method / Event | PC Address | Stack Cleanup | Disassembly / Functional Description |
| :---: | :---: | :--- | :---: | :---: | :--- |
| **0** | `+0x00` | Destructor | `0x0069A860` | 0 | `sub ecx, 2Ch; jmp 0x0069C6A0;` adjusts `this` to `GameDevice` dtor |
| **1** | `+0x04` | PauseEffects | `0x006778F0` | 0 | Loops 4 wheel ports (`0..3`), calls `LogitechEZWheelWrapper::Wheel` to stop road, dirt, bumpy, and collision effects; resets spring centering to 75% (`0x4B`) |
| **2** | `+0x08` | ResumeEffects | `0x004C7740` | 0 | `ret` (stub on PC; console sets `Enabled = 1`) |
| **3** | `+0x0C` | ResetEffects | `0x00677960` | 0 | `mov eax, [ecx]; jmp [eax+4];` virtual forwarder to Slot 1 (Pause) |
| **4** | `+0x10` | BeginUpdate | `0x004C7740` | 0 | `ret` (stub on PC; console resets master scale and zero-clears wave group inputs) |
| **5** | `+0x14` | EndUpdate | `0x004C7740` | 0 | `ret` |
| **6** | `+0x18` | UpdateRoadNoise | `0x006B0620` | `ret 0xC` (12) | Stub (3 params: `wheelIdx`, `roadScalar`, `surface`) |
| **7** | `+0x1C` | UpdateTireSkid | `0x0054E260` | `ret 0x14` (20) | Stub (5 params: `surface`, `skidAngle`, `slipVel`, etc.) |
| **8** | `+0x20` | UpdateTireSlip | `0x006B0620` | `ret 0xC` (12) | Stub (3 params: `surface`, `longSlip`, `latSlip`) |
| **9** | `+0x24` | UpdateRPM | `0x006B0620` | `ret 0xC` (12) | Stub (3 float params: `rpm`, `redline`, `idle`) |
| **10** | `+0x28` | UpdateShiftPotential | `0x006B1200` | `ret 4` (4) | Stub (1 param: integer potential level `3..5`) |
| **11** | `+0x2C` | UpdateNOS | `0x0064C980` | `ret 8` (8) | Stub (2 params: `engaged`, `boostLevel`) |
| **12** | `+0x30` | UpdateEngineBlown | `0x006B1200` | `ret 4` (4) | Stub (1 param: boolean `blown`) |
| **13** | `+0x34` | UpdateShifting | `0x006B1200` | `ret 4` (4) | Stub (1 param: boolean `shifting`) |
| **14** | `+0x38` | UpdateDriftSlip | `0x006B0620` | `ret 0xC` (12) | Stub (3 params: 2 float coordinates + flag) |
| **15** | `+0x3C` | Reserved Slot | `0x006B0620` | `ret 0xC` (12) | Stub (3 params) |
| **16** | `+0x40` | UpdateDriftSkid | `0x008ECD50` | `ret 0x10` (16) | Stub (4 params: drift angle, velocity) |
| **17** | `+0x44` | SteeringForce | `0x006B1200` | `ret 4` (4) | Stub (1 param: float steering torque) |
| **18** | `+0x48` | UpdateRoadWithIndex | `0x006B0620` | `ret 0xC` (12) | Stub (3 params: `wheelIdx`, `bumpScalar`, `surface`) |
| **19** | `+0x4C` | ReportCollision | `0x00696420` | 20 | **Fully implemented (>2.5 KB)!** Contact physics evaluation, closing delta velocity calculation, Havok simable lookup, 5.0 m/s threshold check, dispatches to `LogitechEZWheelWrapper::Wheel` |
| **20** | `+0x50` | GetControllerType | `0x006822A0` | 0 | Calls `0x006821F0`, classifies device (`0x4128808B` gamepad, `0x12345678` wheel, `0x17300A97` keyboard) |
| **21** | `+0x54` | SecondaryQuery | `0x00646150` | 4 | Extended float/force scaling query |

---

## Producer calling pipeline (`LocalPlayer::DoFFB`)

Feedback production is driven per frame by `LocalPlayer::DoFFB`:
- **VTable binding:** `LocalPlayer` offset `+0x04` (`0x00C24AF0` Slot 0).
- **Entry point:** `0x007BC6D0` (for `LocalPlayer`) and `0x007BC7D0` (for `RecordablePlayer`).

```asm
007BC6D0: mov eax, [esp+4]       ; vehicle pointer
007BC6D4: cmp eax, [ecx+68h]     ; verify vehicle belongs to this local player
007BC6D7: jne 007BC6EE           ; exit false if mismatched
007BC6D9: fld dword ptr [esp+8]  ; dt (elapsed seconds)
007BC6DD: push ecx
007BC6DE: add ecx, -4            ; adjust this pointer from interface (+4) to base (+0)
007BC6E1: fstp dword ptr [esp]   ; pass dt
007BC6E4: call 007BC470          ; invoke LocalPlayer::ProduceFeedback
007BC6E9: mov al, 1
007BC6EB: ret 8
```

### Producer execution sequence (`0x007BC470` $\rightarrow$ `0x007BA770`)

1. **Gatekeeper Validation (`CanDoFFB` `0x007BA2F0` / `0x007BA3E0`):**
   - Checks `[0x0127988C]` (game pause / gameflow manager inactive).
   - Checks `[0x0127995C]` (demo, replay, or frontend state inactive).
   - Checks vehicle dynamics: speed threshold, airborne/crash state (`call [eax+0ECh]`), staged start (`call [eax+24h]`), and reset state (`call [eax+88h]`).
   - If any condition is violated, feedback loop aborts immediately.
2. **Lifecycle Start:**
   - Calls **Slot 2 (`ResumeEffects`)** (`0x007BA89B`).
   - Calls **Slot 4 (`BeginUpdate`)** (`0x007BA8A5`).
3. **Four-Wheel Sampling Loop (`ebp = 0..3`):**
   - Resolves surface pointer. Damaged tyre replaces surface reference with blown tyre surface key **`0xD929E923`** (`0x007BA951`), identical to ProStreet.
   - Calls **Slot 6 (`UpdateRoadNoise`)** passing wheel index, road bump scalar, and surface reference (`0x007BA9F0`).
4. **Car & Engine Status Updates:**
   - Calls **Slot 9 (`UpdateRPM`)** passing 3 float parameters (`0x007BAADA`).
   - Calls **Slot 10 (`UpdateShiftPotential`)** passing gear potential level (`0x007BAAF6`).
   - Calls **Slot 11 (`UpdateNOS`)** passing nitro engaged flag and boost level (`0x007BAB2A`).
   - Evaluates engine blown vs shifting:
     ```c
     if (pVehicle->IsEngineBlown()) {
         pFeedback->UpdateEngineBlown(true);  // Slot 12 (0x007BAB5A / 0x007BAB65)
     } else {
         pFeedback->UpdateShifting(pVehicle->IsShifting()); // Slot 13 (0x007BABAC)
     }
     ```
   - Calls **Slot 14 (`UpdateDriftSlip`)** passing lateral drift telemetry (`0x007BABF1`).
5. **Lifecycle End:**
   - Calls **Slot 5 (`EndUpdate`)** (`0x007BAC46`).

---

## Collision physics and reporting pipeline

Unlike ProStreet PC where `ReportCollision` was stubbed as `ret 0x14`, Undercover PC retains a
complete physical collision detection and force calculation pipeline.

### Collision listener hook points

Collision events are dispatched via the `Sim::Collision::IListener` interface embedded at offset
`+0x4C` in player objects:

| Class | Object Offset | VTable Address | Method / Hook Point |
| :--- | :---: | :---: | :--- |
| `LocalPlayer` | `+0x4C` | `0x00C24A14` | Slot 0: `0x007BAC70` (`LocalPlayer::OnCollision`) |
| `RecordablePlayer` | `+0x4C` | `0x00C24C00` | Slot 0: `0x007BAC70` |
| `OnlineLocalPlayer` | `+0x4C` | `0x00C24E24` | Slot 0: `0x007BAC70` |

### Collision dispatch (`LocalPlayer::OnCollision` `0x007BAC70`)

When a contact occurs:
1. Resolves participants from `CollisionInfo`: `mSimableA` (`+0x3C`) and `mSimableB` (`+0x4C`).
2. Identifies contact category: `hit->mFlags & 7`.
3. Queries player profile settings: `mov eax, [edx+14h]; lea ecx, [edi-8]; call eax;`.
4. **The Hidden Rumble Enable Check:**
   ```asm
   007BAD0F: 80 78 09 00     cmp byte ptr [eax+9], 0  ; PlayerSettings::mRumbleEnabled
   007BAD13: 0F 84 0D 01 00  je  007BAE26             ; if 0, skip ReportCollision!
   ```
   > [!IMPORTANT]
   > In Undercover, the player's vibration flag is located at `PlayerSettings + 0x09` (compared to
   > `+0x1C` in ProStreet). If this byte is 0, the game branches around `ReportCollision`. The
   > 6-byte jump at `0x007BAD13` must be NOP'd to guarantee collision delivery.
5. Formats the closing velocity delta vector on stack (`12 bytes`).
6. Evaluates local player ownership (`0x00401810`), producing `bool iamA`.
7. Calls **Slot 19 (`ReportCollision`)** on `IFeedback` (`[ebx+0x4C]`):
   ```asm
   007BAE15: 8B 53 4C        mov edx, dword ptr [ebx+4Ch] ; Slot 19
   007BAE18: 83 C4 08        add esp, 8
   007BAE1B: 50              push eax                     ; push iamA
   007BAE1C: 51              push ecx                     ; push CollisionInfo*
   007BAE1D: 8B 4F 04        mov ecx, dword ptr [edi+4]   ; ecx = IFeedback*
   007BAE20: FF D2           call edx                     ; IFeedback::ReportCollision
   ```

### Concrete collision calculation (`0x00696420`)

The concrete `ReportCollision` function in `GameDevice` implements identical mathematical formulas
and constants to Most Wanted, Carbon, and ProStreet:

1. **Contact Category Bitfield Extraction:**
   Extracted by shifting right arithmetic by 29 bits:
   ```asm
   006965ED: 8B 45 1C        mov eax, dword ptr [ebp+1Ch] ; hit->mFlags
   006965F0: C1 E0 1D        shl eax, 1Dh
   006965F3: C1 F8 1D        sar eax, 1Dh                 ; arithmetic shift 29 bits
   ```
   - `type == 1`: Dynamic Object contact (vehicles, track props, traffic)
   - `type == 2`: World barrier / scenery contact
   - `type == 3`: Ground contact (road / terrain landing)
2. **Physical Thresholds:**
   - **High-delta threshold:** `5.0f` (`0x00C0B560`). If closing velocity exceeds 5.0 m/s, evaluates full vehicle mass and momentum response.
   - **Low-speed vehicle threshold:** `2.0f` (`0x00BDE7C8`).
   - **Ground contact threshold:** `1.0f` (`0x00BD97B0`).
3. **Debounce Gate:**
   - Compares timestamp against `timeGetTime` (`0x0075A510`).
   - Minimum retrigger interval is **150 milliseconds** (`0x96` ms). Contacts occurring within 150 ms are accumulated or rate-limited.
4. **Magnitude Clamping:**
   - Force magnitude is clamped to a maximum of 100% (`0x64`).
5. **Direct Dispatch to Wheel Driver:**
   - Dispatches directly to `LogitechEZWheelWrapper::Wheel::PlayFrontalCollisionEffect` (`0x0075A4C0`) and `PlaySideCollisionEffect` (`0x00759FC0`).

---

## Logitech EZWheelWrapper subsystem

Undercover PC embeds the complete Logitech Gaming SDK (`LogitechEZWheelWrapper`) to provide force
feedback on steering wheels:
- **Wheel Instance:** `0x00DA3BF0` (`LogitechEZWheelWrapper::Wheel`, 124 bytes).
- **Controller Instance:** `0x00DA3BEC` (`LogitechEZWheelWrapper::Controller`, 28 bytes).

### Supported force feedback effects

The executable contains 10 dedicated force effect classes inheriting from base `Force` (`0x00C20804`):

| Class Name | VTable | Role and Driver Function |
| :--- | :---: | :--- |
| `LogiFrontalCollisionEffect` | `0x00C20810` | Frontal collision impulse rendering (`0x0075A4C0`) |
| `LogiSideCollisionEffect` | `0x00C208AC` | Lateral side impact collision rendering (`0x00759FC0`) |
| `LogiBumpyRoadEffect` | `0x00C20930` | High-frequency rough road surface vibrations |
| `LogiDirtRoadEffect` | `0x00C20954` | Off-road / dirt road rumble profiles |
| `LogiSlipperyRoadEffect` | `0x00C208B8` | Reduced traction / wet road centering relaxation |
| `LogiSurfaceEffect` | `0x00C208DC` | Continuous road surface texture feedback |
| `LogiConstantForce` | `0x00C2093C` | Sustained directional load forces |
| `LogiDamperForce` | `0x00C20948` | Velocity-dependent steering damping |
| `LogiSpringForce` | `0x00C208D0` | Centering spring force (reset to 75% on pause: `0x0075A060`) |
| `LogiSoftstopForce` | `0x00C208C4` | Steering rack end-stop resistance |

---

## Gamepad vibration omission on PC

Undercover PC completely lacks native gamepad vibration output:
1. **Actuator Stub:**
   In `LogiGameControllerXInput` (`0x00C2087C`), the rumble actuator slot (Slot 7) points to
   `0x006B1200`, which is literally `ret 4`:
   ```asm
   006B1200: C2 04 00   ret 4
   ```
2. **Missing Import:**
   The `XINPUT1_3.dll` import table contains `XInputGetCapabilities` (`0x00BD66A0`), `XInputEnable`
   (`0x00BD66A4`), and `XInputGetState` (`0x00BD66A8`), but **`XInputSetState` is completely absent**.
3. **Architectural Separation:**
   Black Box designed physical force generation exclusively for DirectInput steering wheels via
   `LogitechEZWheelWrapper`. Gamepads received input polling through XInput, but output rumble was
   never implemented for the PC release.

---

## XtendedInput rumble adapter blueprint

Because Undercover PC contains the complete Black Box `IFeedback` architecture and producer loops,
restoring console-grade dual-motor rumble requires bridging the game's events into the unified
rumble engine.

### Integration patch points

1. **Collision Listener VTable Hooks:**
   Replace Slot 0 of the three collision listener vtables with the XtendedInput collision hook:
   - `0x00C24A14` (`LocalPlayer`)
   - `0x00C24C00` (`RecordablePlayer`)
   - `0x00C24E24` (`OnlineLocalPlayer`)
2. **Hidden Rumble Byte Patch:**
   Bypass the profile rumble check at `0x007BAD13` by writing 6 NOPs (`0x90`):
   ```cpp
   // LocalPlayer::OnCollision skips ReportCollision when PlayerSettings + 9 == 0
   // Original bytes: 0F 84 0D 01 00 00 (je +0x10D)
   injector::MakeNOP(0x007BAD13, 6, true);
   ```
3. **Feedback Device Installation:**
   Intercept `LocalPlayer::SetFeedbackDevice` (`0x007BC700`) or install the XtendedInput
   concrete `IFeedback` adapter into `GameDevice + 0x2C` (`0x00C0D434`).
4. **Motor Output Pipeline:**
   Connect the unified wave and ramp pipeline to dynamic `XInputSetState`:
   - **Left Motor (Low Frequency):** Heavy collisions (`Slot 19`), high shift potential (`Slot 10`), NOS boost (`Slot 11`), blown engine shake (`Slot 12`), road bumps (`Slot 18`, wheels 0 & 2).
   - **Right Motor (High Frequency):** Tire slip (`Slot 8`), tire skid (`Slot 7`), gear shifts (`Slot 13`), blown engine secondary ramp (`Slot 12`), road bumps (`Slot 18`, wheels 1 & 3).

---

## Lineage and architectural comparison

| Feature | Most Wanted (2005) | Carbon (2006) | ProStreet (2007) | Undercover (2008) |
| :--- | :---: | :---: | :---: | :---: |
| **`IFeedback` VTable Slots** | 15 slots | 18 slots | 21 slots (PC) / 24 (X360) | **21 slots (PC)** |
| **Interface Handle** | `0x00621E10` | `0x0064C460` | `0x004CD5D0` | `0x00676D00` |
| **Concrete VTable Address** | `0x008A7BD4` | `0x009E2188` | `0x0098BE6C` | `0x00C0D434` |
| **Offset in `GameDevice`** | `+0x2C` | `+0x2C` | `+0x2C` | `+0x2C` |
| **`DoFFB` Entry Point** | `0x006EE490` | `0x006EDBF0` | `0x004D12D0` | `0x007BC6D0` |
| **`OnCollision` Entry Point**| `0x006EE7A0` | `0x006EE0D0` | `0x004D13F0` | `0x007BAC70` |
| **Rumble Profile Byte** | `Settings + 0x1C` | `Settings + 0x1C` | `Settings + 0x1C` | `Settings + 0x09` |
| **Blown Tyre Surface Key** | N/A | N/A | `0xD929E923` | `0xD929E923` |
| **PC `ReportCollision`** | Stubbed | Stubbed | Stubbed (`ret 0x14`) | **Active (`0x00696420`)** |
| **High-Delta Threshold** | `5.0f` | `5.0f` | `5.0f` | `5.0f` (`0x00C0B560`) |
| **Low-Speed Threshold** | `2.0f` | `2.0f` | `2.0f` | `2.0f` (`0x00BDE7C8`) |
| **Ground Threshold** | `1.0f` | `1.0f` | `1.0f` | `1.0f` (`0x00BD97B0`) |
| **Debounce Timing Gate** | 150 ms | 150 ms | 150 ms | 150 ms (`0x96` ms) |
| **Steering Wheel SDK** | DirectInput base | DirectInput base | DirectInput base | **Logitech EZWheelWrapper (10 effects)** |
| **PC Native Gamepad Rumble**| Stubbed | Stubbed | Stubbed | Stubbed (No `XInputSetState`) |
