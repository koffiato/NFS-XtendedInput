# NFS Undercover PC XInput & Force Feedback Reverse Engineering Report

**Target Executable:** `C:\Stuff\Rumble\tools\research\uc\nfs.exe` (10,584,064 bytes, PE32 x86)\
**Date:** 2026-11-01\
**Investigation Goal:** Determine if the PC port of Need for Speed: Undercover implements native XInput rumble/FFB that can be used to recreate Xbox 360 vibration on PC.

---

## 1. Executive Summary

A deep binary analysis of `nfs.exe` (NFS Undercover PC) reveals:
1. **XInput Input Polling Exists:** The PC port does import `XINPUT1_3.dll`, but **only for controller buttons and analog sticks** (`XInputGetState`, `XInputGetCapabilities`, and `XInputEnable`).
2. **XInput Vibration is Completely Missing:** `XInputSetState` (Ordinal 3) is **never imported or called anywhere in the executable**.
3. **The XInput Wrapper is a Logitech Library:** XInput handling is delegated to a bundled helper class: `LogitechControllerInput::LogiGameControllerXInput`. Its vibration method (`vtable[7]`) is an empty stub (`ret 4` at `0x006B1200`).
4. **`IFeedback` Remains Abstract on PC:** Undercover uses the identical `IFeedback` COM interface found in Most Wanted, Carbon, and ProStreet. However, its PC vtable (`0x00C0CFC4`) points all 20 feedback methods directly to `MSVCR80.dll _purecall`.

**Verdict:** NFS Undercover PC **cannot be used to extract native PC rumble routines**, because EA Vancouver left controller vibration completely unimplemented on PC in Undercover as well. Vanilla Undercover PC has zero controller rumble out of the box. The definitive reference for Xbox 360 vibration mechanics remains the Xbox 360 executable (`default.xex`) itself.

---

## 2. Detailed Binary Analysis of `nfs.exe`

### 2.1 PE Import Table Audit (`XINPUT1_3.dll`)
Inspection of the PE Import Directory in `nfs.exe` at RVA `0x91C174` (IAT RVA `0x7D66A0`):

```
DLL: XINPUT1_3.dll
  Ordinal 4 -> XInputGetCapabilities (Thunk: 0xBD66A0, Stub: 0x008AE17C)
  Ordinal 5 -> XInputEnable         (Thunk: 0xBD66A4, Stub: 0x008AE182)
  Ordinal 2 -> XInputGetState       (Thunk: 0xBD66A8, Stub: 0x008AE176)
```

**Key Finding:** `Ordinal 3` (`XInputSetState`) is **absent** from the import table. There are no dynamic calls (`GetProcAddress`) to `XInputSetState` in the binary.

### 2.2 XInput Implementation Class: `LogiGameControllerXInput`
References to the XInput thunks lead to the class `LogitechControllerInput::LogiGameControllerXInput` (RTTI TypeDescriptor at `0x00D518B0`, Vtable at `0x00C2087C`):

```
vtable[ 0]: 0x007573E0 -> Update() calls XInputGetState(port, &this->state)
vtable[ 1]: 0x00758960 -> returns 0
vtable[ 2]: 0x00757340 -> returns pointer to XINPUT_STATE (this + 0x1238)
vtable[ 3]: 0x00757420 -> IsButtonDown(wMask)
vtable[ 4]: 0x00757430 -> IsButtonPressed(wMask) (just pressed this frame)
vtable[ 5]: 0x00757460 -> IsButtonReleased(wMask) (just released this frame)
vtable[ 6]: 0x00757490 -> GetCapabilities() calls XInputGetCapabilities()
vtable[ 7]: 0x006B1200 -> STUB (ret 4) [Vibration / Actuator slot]
vtable[ 8]: 0x007574F0 -> SetPort(dwUserIndex)
vtable[ 9]: 0x00757500 -> GetPort()
```

The game only reads buttons, triggers, and thumbsticks. The motor actuator method is an empty stub that immediately returns.

### 2.3 The `IFeedback` Pipeline in Undercover
NFS Undercover retains the exact Black Box COM feedback architecture:
- Interface identifier: `IFeedback::_IHandle` at `0x00676D00` (returns string `"IFeedback"` at `0x00C0CE10`).
- Interface registration: Registered into `InputDevice` at offset `+0x2C` via `UTL::COM::Object::IList_Add` (`0x005F6A60`).
- Steering wheel interface: `ISteeringWheel::_IHandle` at `0x00666E20`.

In `InputDevice`, the PC implementation vtable is assigned to `0x00C0CFC4`:
```
IFeedback[ 0]: 0x007B8560 -> Destructor
IFeedback[ 1..20]: 0x008AE240 -> jmp dword ptr [MSVCR80.dll _purecall]
```

Every single force feedback method (`BeginUpdate`, `EndUpdate`, `UpdateRoadNoise`, `UpdateTireSkid`, `UpdateTireSlip`, `UpdateRPM`, `ReportCollision`, etc.) resolves to a pure virtual crash handler (`_purecall`) on PC.

---

## 3. Comparison Across the Black Box Engine Family

| Game | Release Year | Input System | PC Controller Rumble | `IFeedback` Status on PC | X360 Controller Rumble |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **NFS Underground 1** | 2003 | DirectInput | Stubbed | Native 3-Slot Channel Scheduler | N/A (Original Xbox / PS2) |
| **NFS Underground 2** | 2004 | DirectInput | Stubbed (`0x004022C0`) | Native 3-Slot Channel Scheduler | N/A (Original Xbox / PS2) |
| **NFS Most Wanted** | 2005 | DirectInput | Missing | Abstract (`_purecall` / empty) | **Native PFD 5.1 / XamInputSetState** |
| **NFS Carbon** | 2006 | DirectInput | Missing | Abstract (`_purecall` / empty) | **Native PFD 5.1 / XamInputSetState** |
| **NFS ProStreet** | 2007 | DirectInput | Missing | Abstract (`_purecall` / empty) | **Native PFD 5.1 / XamInputSetState** |
| **NFS Undercover** | 2008 | Hybrid (Logitech XInput + DInput) | **Missing (`XInputSetState` omitted)** | Abstract (`_purecall` / empty) | **Native PFD 5.1 / XamInputSetState** |

---

## 4. Conclusion & Implications for X360 Rumble on PC

1. **Why Undercover Cannot Help:** EA Vancouver never finished the PC rumble pipeline for Undercover. They adopted Logitech's XInput wrapper solely to map Xbox 360 controller buttons and analog sticks without going through DirectInput, but they completely left out motor vibration.
2. **Where the Solution Lies:**
   - The physics side (`DoFFB`) was never broken on PC—it produces the full suite of RPM, slip, skid, surface noise, and collision telemetry.
   - The missing piece on PC has always been the **concrete implementation of `IFeedback`** that receives these events, manages envelope decay and spline interpolation, and dispatches them to `XInputSetState`.
   - `NFS-XtendedInput`'s implementation in `src/NFS_XtendedInput_Rumble.h` already fills this exact void by instantiating the missing `IFeedback` device in `InputDevice + 0x2C` and driving XInput motors directly.
   - For 1:1 recreation of Xbox 360 curves, our disassembly of MW X360 (`0x8261C9B8` in `default.xex`) remains the authoritative reference.
