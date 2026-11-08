# Need for Speed: World feedback research

NFS World inherits the Black Box feedback and physics pipeline directly from the Carbon/Most Wanted engine branch. On PC, the final release (`nfsw.exe`, build date May 13, 2015) ships with an intact 18-slot `IFeedback` vtable layout, active collision evaluator and reporter, and a full simulation producer pipeline in `LocalPlayer::DoFFB`. However, as in Carbon and Most Wanted, PC retail stubs all effect methods in `IFeedback` with empty `ret` / `ret N` instructions.

This document provides a technical analysis of NFS World's input, force feedback, collision, and player device subsystems, mapping exact virtual addresses, concrete vtables, calling producers, and integration requirements for the XtendedInput rumble adapter.

---

## Binaries and address mapping

| Input | Size | SHA-256 |
| :--- | ---: | :--- |
| PC Live `nfsw.exe` | 11,452,160 bytes | `b2c616c641d3512b4d1edec9aab750f3308281752628abc845142d1c434ec819` |

- **Architecture:** PE32 x86 (standard Win32 executable).
- **TimeDateStamp:** `0x55529D64` (Wed May 13 00:40:04 2015 UTC).
- **Preferred Image Base:** `0x00400000`.
- **Compiler / CRT:** Microsoft Visual C++ 9.0 (2008), importing `MSVCR90.dll` and `MSVCP90.dll`.
- **ASLR / DEP:** `DYNAMIC_BASE: True` (`0x8140`), `NX_COMPAT: True`. **All absolute addresses require runtime relocation:**
  ```cpp
  MainBase = (uintptr_t)GetModuleHandleA(NULL) - 0x00400000;
  ```

### PE section virtual memory layout

| Section | Virtual Address Range | File Offset | Raw Size | Characteristics |
| :--- | :---: | :---: | :---: | :--- |
| `.text` | `0x00401000 - 0x00B1C26C` | `0x00000400` | 7,451,648 bytes (`0x71B400`) | Executable code |
| `.rdata` | `0x00B1D000 - 0x00C5A4B2` | `0x0071B800` | 1,299,968 bytes (`0x13D600`) | Read-only data, vtables |
| `.data` | `0x00C5B000 - 0x00E109FC` | `0x00858E00` | 553,472 bytes (`0x087200`) | Globals, writable static data |
| `.tls` | `0x00E11000 - 0x00E11011` | `0x008E0000` | 512 bytes (`0x000200`) | Thread-local storage |
| `.rsrc` | `0x00E12000 - 0x00F90108` | `0x008E0200` | 1,565,184 bytes (`0x17E200`) | Resources |
| `.reloc` | `0x00F91000 - 0x0101D0BA` | `0x00A5E400` | 573,952 bytes (`0x08C200`) | Relocation table |

---

## UTL / COM interface handles

Interfaces in NFS World are identified via standard Black Box UTL COM handles (`mov eax, <str_va>; ret`):

| Interface | Handle Function | String Address | ASCII Name |
| :--- | :---: | :---: | :--- |
| `IFeedback` | `0x00756910` | `0x00BCBB2C` | `"IFeedback"` |
| `ISteeringWheel` | `0x00756860` | `0x00BCBB1C` | `"ISteeringWheel"` |
| `IVehicle` | `0x0064F4E0` | `0x00BB5B64` | `"IVehicle"` |
| `IRigidBody` | `0x00687220` | `0x00BB8D38` | `"IRigidBody"` |
| `ISuspension` | `0x00684110` | `0x00BB8B64` | `"ISuspension"` |
| `IInput` | `0x0065D2C0` | `0x00BB6BD8` | `"IInput"` |
| `ITransmission` | `0x0065D2D0` | `0x00BB6BE0` | `"ITransmission"` |
| `ISimable` | `0x00684170` | `0x00BB8B9C` | `"ISimable"` |
| `IPlayer` | `0x006840F0` | `0x00BB8B4C` | `"IPlayer"` |
| `IEntity` | `0x006F6D50` | `0x00BC3014` | `"IEntity"` |
| `IAttachable` | `0x00684160` | `0x00BB8B90` | `"IAttachable"` |
| `IStateManager` | `0x00753C10` | `0x00BCB5AC` | `"IStateManager"` |

---

## Concrete `IFeedback` vtable layout (`0x00BCBD98`)

NFS World inherits Carbon's **18-slot** feedback interface table (unlike ProStreet / Undercover which expand to 21 slots). In stock `nfsw.exe`, the concrete vtable is at `0x00BCBD98` (embedded inside `GameDevice` at offset `+0x1820`):

| Slot | Disp | Method / Event Signature | PC Stock Address | Stock Behavior |
| :---: | :---: | :--- | :---: | :--- |
| **0** | `+0x00` | Destructor | `0x0075D340` | `sub ecx, 1820h; jmp GameDevice::dtor` (`0x0075DE40`) |
| **1** | `+0x04` | `PauseEffects()` | `0x00756AE0` | Calls embedded `LogitechEZWheelWrapper` to stop wheel effects |
| **2** | `+0x08` | `ResumeEffects()` | `0x007D96B0` | `ret` |
| **3** | `+0x0C` | `ResetEffects()` | `0x00756B50` | `mov eax, [ecx]; jmp [eax+4]` (calls Slot 1) |
| **4** | `+0x10` | `BeginUpdate()` | `0x007D96B0` | `ret` |
| **5** | `+0x14` | `EndUpdate()` | `0x007D96B0` | `ret` |
| **6** | `+0x18` | `UpdateRoadNoise(bool front, const SimSurface& s, float speed)` | `0x0051E110` | `ret 0xC` |
| **7** | `+0x1C` | `UpdateTireSkid(bool front, const SimSurface& s, float skid)` | `0x0051E110` | `ret 0xC` |
| **8** | `+0x20` | `UpdateTireSlip(bool front, const SimSurface& s, float slip)` | `0x0051E110` | `ret 0xC` |
| **9** | `+0x24` | `UpdateRPM(float powerband, float overrev, float throttle)` | `0x0051E110` | `ret 0xC` |
| **10** | `+0x28` | `UpdateShiftPotential(ShiftPotential potential)` | `0x0081A5D0` | `ret 4` |
| **11** | `+0x2C` | `UpdateNOS(bool engaged, float boostLevel)` | `0x00A6D2D0` | `ret 8` |
| **12** | `+0x30` | `UpdateEngineBlown(bool blown)` | `0x0081A5D0` | `ret 4` |
| **13** | `+0x34` | `UpdateShifting(bool shifting)` | `0x0081A5D0` | `ret 4` |
| **14** | `+0x38` | `UpdateDrafting(const void* draftInfo)` | `0x0081A5D0` | `ret 4` |
| **15** | `+0x3C` | `UpdateDrift(float slipAngle)` | `0x0081A5D0` | `ret 4` |
| **16** | `+0x40` | `UpdateSteering(float steering)` | `0x0081A5D0` | `ret 4` |
| **17** | `+0x44` | `ReportCollision(const COLLISION_INFO& cinfo, bool iamA)` | `0x00A6D2D0` | `ret 8` |

---

## Device architecture & critical vtable discrepancy

In previous titles (MW, Carbon, Undercover), `InputDevice` placed `IFeedback` at offset `+0x2C`. In Carbon, an extra capability query slot placed `GetInterfaces` at slot 9 (`+0x24`).

### Stock `GameDevice` concrete memory layout (`0x184C` = 6,220 bytes):

In NFS World (`nfsw.exe`), EA Black Box overhauled `GameDevice`, replacing the dynamic heap-allocated scalar pointer (`DeviceScalar*` at `+0x18` in Carbon/MW) with a massive contiguous object embedding 256 scalars, previous values, current values, and interfaces inline.

Constructor at `0x0075D210` (factory at `0x0075DE00`, allocating `0x184C` bytes via `0x0061C2A0`):

| Offset Range | Size | Field | Description |
| :---: | :---: | :--- | :--- |
| `+0x00 - +0x03` | 4 bytes | `void* vptr` | Stock device vtable (`0x00BCBD50`) |
| `+0x04 - +0x17` | 20 bytes | `UTL::COM::Object::_IList` | COM interface list (`Padding1`, handles, `Padding2`) |
| **`+0x18 - +0x1017`** | **4,096 bytes** | **`DeviceScalar mDeviceScalars[256]`** | **Inline array of 256 16-byte scalar structures** |
| `+0x1018 - +0x1417` | 1,024 bytes | `float mPrevValues[256]` | Inline array of previous frame values |
| `+0x1418 - +0x1817` | 1,024 bytes | `float mCurrentValues[256]` | Inline array of current frame values |
| `+0x1818 - +0x181B` | 4 bytes | `int fDeviceIndex` | Controller / device index (`0x0075D198`) |
| `+0x181C - +0x181F` | 4 bytes | `float fControllerCurve` | Controller curve multiplier (`1.0f`) |
| `+0x1820 - +0x182B` | 12 bytes | `IFeedback mFFB` | Concrete 18-slot feedback object (vtable `0x00BCBD98`) |
| `+0x182C - +0x182F` | 4 bytes | `int fNumDeviceScalars` | Scalar count (initialized to 143 via `0x00756A52`) |
| `+0x1830 - +0x183F` | 16 bytes | Padding / Reserved | Internal alignment |
| `+0x1840 - +0x184B` | 12 bytes | `ISteeringWheel mWheel` | Concrete steering wheel interface |

#### `DeviceScalar` internal structure (16 bytes):
- `+0x00`: `DeviceScalarType fType` (`kJoyAxis = 0`, `kAnalogButton = 1`, `kDigitalButton = 2`)
- `+0x04`: `uint32_t fName_CRC32` (action hash)
- `+0x08`: `float* fPrevValue` (pointer pointing to `device + 0x1018 + i*4`)
- **`+0x0C`**: **`float* fCurrentValue`** (pointer pointing to `device + 0x1418 + i*4`)

Because the array begins at `device + 0x18`, the pointer `fCurrentValue` for scalar index `i` is located at:
$$\text{Offset}(i) = 0\text{x}18 + 0\text{x}0\text{C} + i \times 16 = 0\text{x}24 + i \times 16$$

### Stock `GameDevice` vtable (`0x00BCBD50`):
- `+0x00`: Destructor (`0x0075DE40`)
- `+0x04`: `IsConnected()` (`0x004A9BE0` -> `mov al, 1; ret`)
- `+0x10`: `Update()` / Poll (`0x0075C250`)
- `+0x14`: `GetNumDeviceScalar()` (`0x00756A90` -> `mov eax, [ecx + 0x182c]; ret`)
- `+0x18`: `StartVibration()` (`0x007D96B0` -> `ret`)
- `+0x1C`: `StopVibration()` (`0x007D96B0` -> `ret`)
- **`+0x20` (Slot 8): `GetInterfaces()` (`0x0075D300`):**
  ```asm
  0075D300: 85 C9       test ecx, ecx
  0075D302: 74 07       je   0075D30B
  0075D304: 8D 81 20 18 00 00  lea eax, [ecx + 1820h] ; returns embedded IFeedback object
  0075D30A: C3          ret
  0075D30B: 33 C0       xor  eax, eax
  0075D30D: C3          ret
  ```
- **`+0x24` (Slot 9): `GetWheelInterfaces()` (`0x0075D330`):**
  ```asm
  0075D330: 8B 81 40 18 00 00  mov eax, [ecx + 1840h] ; returns steering wheel object
  0075D336: C3          ret
  ```

> [!IMPORTANT]
> **Virtual slot layout difference from Carbon:**
> In Carbon, `LocalPlayer::SetControllerPort` called Slot 9 (`+0x24`) to query interfaces.
> In NFS World, `LocalPlayer::SetControllerPort` (`0x007783F0`) calls **Slot 8 (`+0x20`)** for `IFeedback` and **Slot 9 (`+0x24`)** for `ISteeringWheel`.
> If `InputDevice` includes the dummy `PSControllerCapability()` slot (as Carbon/ProStreet did), `GetInterfaces` shifts to Slot 9 (`+0x24`), causing `LocalPlayer::SetControllerPort` to call `StopVibration` at `+0x20` and completely miss `IFeedback`.

---

## Mod compatibility investigation: `NRZMods.asi` crash (`SBRCrashDump_CL0_2026.11.08_08.20.06`)

### Crash signature
```
Unhandled exception at 0x6AF494D0 in NRZMods.asi (+0xa994d0): 0xC0000005: Access violation reading location 0x0000001F.
    Register dump:
        EAX: 0x0000001F  EBX: 0x00000000  ECX: 0x70516F14  EDX: 0x00000000  
        EDI: 0x6AF9C2B0  ESI: 0xFFFFFFC5  EBP: 0x001AFE40  EIP: 0x6AF494D0  
        ESP: 0x001AF770  EFL: 0x00210202
```

### Callstack and context
- **Return address:** `0x00A2E50F` in `nfsw.exe (+0x35E50F)` (image base `0x006D0000`, preferred VA `0x0075E50F`).
- **Hook site:** `0x0075E50A` (`call 0x0075E1C0` in `ActionMap::Update`). `NRZMods.asi` intercepts this call with its own input processor at `NRZMods.asi (+0xA994D0)`.
- **Calling parameters:**
  - `ecx`: `ActionMap*`
  - `[ebp + 8]` (`push ebx` at `0x0075E4FC`): `GameDevice*` (`devManager->devices[deviceIndex]`).

### Disassembly of the faulting routine in `NRZMods.asi`:
```asm
0x6af49481: mov  ecx, dword ptr [ebp + 8]    ; ecx = device pointer
0x6af49484: add  ecx, 0x24                   ; ecx = device + 0x24 (&scalar[0].fCurrentValue)
0x6af4948c: mov  esi, 0xffffff9b             ; loop counter: -101 (101 iterations)
...
0x6af494ca: mov  eax, dword ptr [ecx]        ; load fCurrentValue pointer from device + 0x24 + i*16
0x6af494cc: test eax, eax
0x6af494ce: je   0x6af494e0
0x6af494d0: movss xmm0, dword ptr [eax]      ; DEREFERENCE POINTER -> CRASH (EAX = 0x0000001F)
...
0x6af494c4: add  ecx, 0x10                   ; advance to next DeviceScalar (+16 bytes)
0x6af494c7: inc  esi                         ; increment counter
0x6af494c8: je   0x6af49529                  ; break when esi == 0
```

### Failure mechanism
1. `NFS_XtendedInput.asi` replaces the `"GameDevice"` factory at `0x00B10F9D + MainBase` with `InputDeviceFactory`.
2. `InputDevice` in `NFS-XtendedInput` uses the legacy Carbon layout:
   - Allocated size is only **~80 bytes**.
   - `+0x18`: `DeviceScalar* fDeviceScalar` (heap pointer, NOT an inline array).
   - `+0x24`: `int fDeviceIndex` (integer 0, NOT a pointer).
   - To prevent vanilla `nfsw.exe` from crashing, XtendedInput hooked `0x0075E1C0` with `UnscrewWorldDeviceScalars` to dereference `[ebx + 0x18]` as a pointer.
3. However, `NRZMods.asi` hooks `0x0075E50A` / `0x0075E1C0` directly and bypasses XtendedInput's hook. It expects native `GameDevice` memory layout:
   - At iteration 42 (`ESI = -59`, signed `-101 + 42 = -59`), `ecx` reaches:
     $$\text{ECX} = \text{device} + 0\text{x}24 + 42 \times 16 = \text{device} + 0\text{x}2\text{C}4 \quad (708 \text{ bytes into the object})$$
   - Because `InputDevice` is only ~80 bytes, offset `0x2C4` reads raw heap memory of whatever object was allocated after `InputDevice`.
   - The memory at that offset contained `0x0000001F`.
   - `NRZMods.asi` executes `movss xmm0, dword ptr [eax]` on address `0x0000001F`, triggering `STATUS_ACCESS_VIOLATION` (`0xC0000005`).

---

## `LocalPlayer` object layout & controller port wiring

`LocalPlayer` constructor at `0x0077C450` initializes multiple interfaces:

| Offset | VTable | Role |
| :---: | :---: | :--- |
| `+0x00` | `0x00BCF258` | Primary `LocalPlayer` representation |
| `+0x04` | `0x00BCF254` | `DoFFB` interface: Slot 0 is `DoFFB(ISimable*, dt)` (`0x00778350`) |
| `+0x24` | `0x00BCF22C` | `IEntity` subobject: Slot 3 (`+0x0C`) is `GetSimable()` |
| `+0x30` | `0x00BCF210` | `IAttachable` subobject |
| `+0x44` | `0x00BCF1B8` | **Device integration subobject** |
| `+0x4C` | `0x00BCF3D4` | **`Sim::Collision::IListener` collision hook point** |

### Internal offsets within `LocalPlayer` (`esi = LocalPlayer`):
- `esi + 0x50` (`(esi + 0x44) + 0x0C`): Active `IFeedback*` pointer.
- `esi + 0x54` (`(esi + 0x44) + 0x10`): Active `ISteeringWheel*` pointer.
- `esi + 0x5C` (`(esi + 0x44) + 0x18`): Active controller port (`int`).

### Device binding sequence (`LocalPlayer::SetFeedbackDevice` / `SetControllerPort`, `0x007783F0`):
1. Checks if target port equals current port `[player + 0x5C]`.
2. If old `[player + 0x50]` (`IFeedback*`) is non-null, calls Slot 3 (`ResetEffects`) and clears pointer to null.
3. If new port >= 0:
   - Resolves device manager via `call 0x00759C90`.
   - Fetches device pointer `esi = devManager->devices[port]`.
   - Calls **Slot 8 (`+0x20`)** on device (`mov edx, [eax + 20h]; call edx`).
   - Queries `IFeedback` handle `0x00756910` via `UTL::COM::Object::_IList::Find` (`0x007536B0`).
   - Stores `IFeedback*` at `player + 0x50`.
   - Calls **Slot 9 (`+0x24`)** on device (`mov edx, [eax + 24h]; call edx`).
   - Queries `ISteeringWheel` handle `0x00756860` via `_IList::Find`.
   - Stores `ISteeringWheel*` at `player + 0x54`.
4. Calls `IFeedback::ResetEffects()` (Slot 3) on the newly bound device.

---

## Producer Calling Pipeline (`LocalPlayer::DoFFB`)

Feedback production is driven per frame by `LocalPlayer::DoFFB` (`0x00778350`):

1. **Gatekeepers:**
   - Checks `0x00776990` (`CanDoFFB`):
     - Pause state `*(uint32_t*)0x00D77A94 == 0`
     - Valid vehicle with speed above threshold `0x00BCECCC`
     - Replay/demo state (`IVehicle::vtable[+0x7C] == 0`)
     - AI control (`IVehicle::vtable[+0x28] == 0`)
     - Vehicle animation (`IVehicle::vtable[+0xF4] == 0`)
   - Checks `0x00776C10`:
     - Global flags `*(uint32_t*)0x00D10C34 != 0` and `*(uint32_t*)0x00C850C0 != 0`
     - `IStateManager` pause state
2. **Producer Loop (`0x00776CC0`):**
   - Resolves `IFeedback*` from `player + 0x50`.
   - `0x00776DA3`: Calls Slot 2 (`ResumeEffects`).
   - `0x00776DAD`: Calls Slot 4 (`BeginUpdate`).
   - `0x00776DC9`: Calls Slot 16 (`UpdateSteering(steering)`).
   - **Surface & Wheel Loop (wheels `0..3`):**
     - Blown tire detection: grounded wheel with damage substitutes `" tire_blown"` (`0xD929E923`).
     - Airborne wheel substitutes null surface (`0x00D77B08`).
     - Surface collection queried via `Attrib::Instance` at offset `+0x00`.
     - `0x00776ECE`: Calls Slot 8 (`UpdateTireSlip(front, surface, slip)`).
     - `0x00776EE7`: Calls Slot 7 (`UpdateTireSkid(front, surface, skid)`).
     - `0x00776F15`: Calls Slot 6 (`UpdateRoadNoise(front, surface, speed)`).
   - **Powertrain & Effects:**
     - `0x00776FC4`: Calls Slot 9 (`UpdateRPM(powerband, overrev, throttle)`).
     - `0x00776FDE`: Calls Slot 10 (`UpdateShiftPotential(potential)`).
     - `0x00777014`: If NOS active, calls Slot 11 (`UpdateNOS(engaged, boostLevel)`).
     - `0x0077704E`: Calls Slot 13 (`UpdateShifting(shifting)`).
     - `0x0077706A`: If drafting/slipstream, calls Slot 14 (`UpdateDrafting(draftInfo)`).
   - **Finalize:**
     - `0x00777074`: Calls Slot 5 (`EndUpdate`).

---

## Collision Evaluation (`LocalPlayer::OnCollision`)

Collisions are routed through `LocalPlayer::OnCollision` (`0x00777080`, Slot 0 of `0x00BCF3D4`):

1. Resolves `objA` via `Sim::FindSimable` (`0x00753700` with list `0x00C8E934`).
2. Resolves `objB` if present (`edi + 0x4C`).
3. Gatekeeper checks at `0x00777106` and `0x0077710F` (`0x00D10C34` and `0x00C850C0`).
4. **Collision dispatch at `0x00777140`:**
   ```asm
   00777132: 0F B6 C8          movzx ecx, al          ; bool iamA
   00777135: 8B 47 44          mov   eax, [edi + 44h] ; Slot 17 (+0x44) = ReportCollision
   00777138: 83 C4 08          add   esp, 8
   0077713B: 51                push  ecx              ; iamA
   0077713C: 8B 4E 04          mov   ecx, [esi + 4]   ; IFeedback* (player + 0x50)
   0077713F: 52                push  edx              ; const COLLISION_INFO&
   00777140: FF D0             call  eax              ; IFeedback::ReportCollision
   ```

---

## Core Simulation Addresses (Relative to `MainBase`)

| Symbol | Address (Base `0x00400000`) | Description |
| :--- | :---: | :--- |
| `Sim::GetSimable` | `0x00691DA0` | Resolves `ISimable*` from handle |
| `Sim::FindSimableTable` | `0x00C8E934` | Simable lookup list / table head |
| `GameFlowManager::Status` | `0x00CFC730` | Flow status (racing = 6) |
| `PlayerList` | `0x00CA11A0` | Player list container (`+0x104` array, `+0x10C` count) |
| `PlayerCount` | `0x00D77B30` | Active player count |
| `SurfaceBlownTireHash` | `0xD929E923` | `" tire_blown"` bin key |
| `SurfaceNullPointer` | `0x00D77B08` | Null/airborne surface instance |
| `LocalPlayer::DoFFB` | `0x00778350` | Producer entry point (`ISimable*, dt`) |
| `LocalPlayer::OnCollision` | `0x00777080` | Collision listener entry point |
| `LocalPlayer::SetControllerPort` | `0x007783F0` | Resolves and stores `IFeedback*` at `player + 0x50` |
| `IFeedback::vtable` | `0x00BCBD98` | Stock 18-slot concrete feedback vtable |
| `GameDevice::vtable` | `0x00BCBD50` | Stock device vtable (Slot 8 = `GetInterfaces`) |

---

## Adaptation Plan for `NFS-XtendedInput`

1. **Native `GameDevice` Concrete Memory Alignment:**
   - Redefine `InputDevice` for `GAME_WORLD` to match the exact `0x184C`-byte memory layout of `GameDevice`:
     - Embed `DeviceScalar mDeviceScalars[256]` inline from `+0x18` to `+0x1017`.
     - Embed `float mPrevValues[256]` inline from `+0x1018` to `+0x1417`.
     - Embed `float mCurrentValues[256]` inline from `+0x1418` to `+0x1817`.
     - Place `int fDeviceIndex` at `+0x1818` and `float fControllerCurve` at `+0x181C`.
     - Embed `IFeedback mFFB` at `+0x1820` (Slot 8 in vtable).
     - Place `int fNumDeviceScalars` at `+0x182C`.
   - In initialization, set each `mDeviceScalars[i].fCurrentValue = &mCurrentValues[i]` and `fPrevValue = &mPrevValues[i]`.
   - **Remove `UnscrewWorldDeviceScalars` hooks:** Once `InputDevice` matches native memory layout, `nfsw.exe`'s native scalar calculation at `0x0075E1C0` and third-party mod scanners like `NRZMods.asi` work out-of-the-box without buffer overruns.
2. **Device VTable Alignment:**
   - For `GAME_WORLD`, ensure `InputDevice::GetInterfaces()` sits at **Slot 8 (`+0x20`)**, NOT Slot 9. Remove `PSControllerCapability()` for target `W`.
3. **`IFeedback` Implementation:**
   - NFS World uses the exact **18-slot** Carbon layout. The existing Carbon console effect engine (`MWEffects`, `MWRamp`, `MWWave`, `draft`, `drift`, road noise, slip/skid curves) can be shared directly with NFS World.
4. **ASLR Address Handling:**
   - Wrap all game addresses with `MainBase` offsets, as `nfsw.exe` relocates dynamically.
5. **Collision Gatekeeper Bypass:**
   - If required, patch `0x00777106` (`je 0x00777142`) so controller rumble is active regardless of stock profile settings.
