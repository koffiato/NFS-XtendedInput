# Carbon feedback research

PC and Xbox 360 share the `IFeedback` producer pipeline in `LocalPlayer::DoFFB`
and `LocalPlayer::OnCollision`, while PC controller effect methods are empty
stubs. The Carbon adapter shares the Most Wanted console feedback engine and
adds Carbon's 18-slot `IFeedback` layout, `SimSurface` collection offset,
drafting wake/slipstream effects, drift tyre wave and updated collision scales.
Impulse triggers are an optional extension.

Carbon's input-device table includes a capability query at slot 6, placing
`StartVibration` / `StopVibration` at slots 7 / 8 and `GetInterfaces` at slot 9
(`+0x24`). `LocalPlayer::SetControllerPort` calls this slot at `0x007651F2`,
then finds `IFeedback` through the returned interface's owner list at `+0x04`.
An ASI built with the older device layout returns zero from slot 9 and produces
no feedback at all. Rebuild the `CB` target and replace the installed
`scripts/NFS_XtendedInput.asi` after changing this layout. `tests\cb.cmd`
checks the device lookup, registration, native callbacks and polling through
to motor output, in addition to individual effect behavior.

## Reference binaries

| Binary | Size | SHA-256 |
| --- | ---: | --- |
| Local PC `NFSC.exe` | 7,217,152 | `baaaaec33873d4ba7e4dbae6a5212049ffdcf9679a35d865d7625b4189952fd3` |
| Xbox expanded image `default.exe` | 17,629,184 | `41167e1fc7401925b9612d69438b19583e5aa57b962df45386beee8f532becf6` |

PC addresses use normal PE section mapping. The expanded Xbox image loads at
`0x82000000`, with `fileOffset = VA - 0x82000000`. Its `.text` starts at dump
offset `0x110000`; the inherited raw pointer `0x105E00` describes the original
PE file layout.

## Feedback interface and producers

Feedback tables: PC `0x009E2188` (constructed at `0x00680600`), Xbox
`0x8208F9B8` (constructed at `0x8244A3F0`). Carbon expands MW's 15-slot table
to 18 slots by inserting `UpdateDrafting`, `UpdateDrift` and `UpdateSteering`
before `ReportCollision`:

| Slot | Offset | Method | PC implementation | Xbox implementation |
| ---: | --- | --- | --- | --- |
| 0 | `+0x00` | Destructor | `0x006807E0` | `0x8244A368` |
| 1 | `+0x04` | `PauseEffects` | `0x00679FA0` | `0x8244A498` |
| 2 | `+0x08` | `ResumeEffects` | `0x0067A010` (`ret`) | `0x8244A520` |
| 3 | `+0x0C` | `ResetEffects` | `0x0067A020` (calls slot 1) | `0x8244A5A8` |
| 4 | `+0x10` | `BeginUpdate` | `0x0067A030` (`ret`) | `0x8244A690` |
| 5 | `+0x14` | `EndUpdate` | `0x0067A040` (`ret`) | `0x8244A710` (`blr`) |
| 6 | `+0x18` | `UpdateRoadNoise(bool front, const SimSurface& surface, float speed)` | `0x0067A050` (`ret 0xC`) | `0x8244A718` |
| 7 | `+0x1C` | `UpdateTireSkid(bool front, const SimSurface& surface, float skid)` | `0x0067A060` (`ret 0xC`) | `0x8244A810` |
| 8 | `+0x20` | `UpdateTireSlip(bool front, const SimSurface& surface, float slip)` | `0x0067A070` (`ret 0xC`) | `0x8244A920` |
| 9 | `+0x24` | `UpdateRPM(float powerband, float overrev, float throttle)` | `0x0067A080` (`ret 0xC`) | `0x8244AA18` (`blr`) |
| 10 | `+0x28` | `UpdateShiftPotential(ShiftPotential potential)` | `0x0067A090` (`ret 4`) | `0x8244AA20` |
| 11 | `+0x2C` | `UpdateNOS(bool engaged, float nosLevel)` | `0x0067A0A0` (`ret 8`) | `0x8244AC80` |
| 12 | `+0x30` | `UpdateEngineBlown(bool blown)` | `0x0067A0B0` (`ret 4`) | `0x8244ABD8` |
| 13 | `+0x34` | `UpdateShifting(bool shifting)` | `0x0067A0C0` (`ret 4`) | `0x8244AD40` |
| 14 | `+0x38` | `UpdateDrafting(const void* draftInfo)` | `0x0067A0E0` (`ret 4`) | `0x8244AE18` |
| 15 | `+0x3C` | `UpdateDrift(float slipAngle)` | `0x006807C0` (`ret 4`) | `0x8244AED0` |
| 16 | `+0x40` | `UpdateSteering(float steering)` | `0x006807D0` (`ret 4`) | `0x824492A0` (`blr`) |
| 17 | `+0x44` | `ReportCollision(const COLLISION_INFO& cinfo, bool iamA)` | `0x0067A0D0` (`ret 8`) | `0x8244AFB8` |

PC embeds `IFeedback` at `InputDevice + 0x2C` with its owner-list pointer at
interface `+0x04` (registered via `IFeedback::IHandle` at `0x00679170` and
`UTL::COM::Object::_IList::Add` at `0x0060DCB0`).

`LocalPlayer::DoFFB` (`0x00762000` on PC) drives the interface each tick:

1. `ResumeEffects`, `BeginUpdate`, then `UpdateSteering` (`0x00762127`).
2. Checks `IVehicle::GetDriverStyle()` (`vtable[+0x34]`):
   - When `GetDriverStyle() == 2` (`STYLE_DRIFT`), calls `UpdateDrift` with
     `IVehicle::GetSlipAngle()` (`vtable[+0xC4]`) at `0x00762178` and skips the
     per-wheel surface loop (`jmp 0x007622E4`).
   - Otherwise, iterates each wheel (`0..GetNumWheels()-1`), reporting
     `fabsf(GetWheelSlip(i))`, `fabsf(GetWheelSkid(i))` and
     `fabsf(IVehicle::GetAbsoluteSpeed())` (`vtable[+0xAC]`). Grounded wheels
     with `GetWheelDamage(i) != 0` substitute `" tire_blown"` (`0xD929E923`);
     airborne wheels use the null surface (`0x00B4D920`).
3. Reports `UpdateRPM`, `UpdateShiftPotential`, optional `UpdateNOS`,
   `UpdateEngineBlown` **or** `UpdateShifting`, then `UpdateDrafting`
   (`0x00762432`) with `IVehicle::GetDraftingInfo()` (`vtable[+0xF4]`, returning
   `PVehicle + 0x1C0`), and finally `EndUpdate`.

`LocalPlayer::OnCollision` (`0x00765240` on PC) checks the profile's Rumble
setting byte before calling `ReportCollision` (`je 0x007652F5` at
`0x007652C8`). The adapter patches `0x007652C8` (`0x2B74` -> `0x9090`) so
INI `Enabled` controls collision rumble directly.

## Console effect state

Xbox state array: `0x82BABF68`; stride `0x234` per port (expanded from MW's
`0x1B0` to hold two additional four-wave groups and `draftScale`):

| Offset | Contents |
| --- | --- |
| `+0x000..0x03F` | Four `RoadNoise` waves (`0xBA1297DA`) |
| `+0x040..0x07F` | Four `TireSkid` waves (`0x0C149044`) |
| `+0x080..0x0BF` | Four `TireSlip` waves (`0x8FD11D27`) |
| `+0x0C0..0x0FF` | Four Group-4 waves (retained across `BeginUpdate`) |
| `+0x100..0x13F` | Four `Draft` waves (`+0x100` Left unused, `+0x110` Right active) |
| `+0x140..0x17F` | Four `Drift` waves (`+0x140` Left unused, `+0x150` Right active) |
| `+0x180..0x21F` | Ten `{remaining, duration, amplitude, floor}` ramps, alternating left/right |
| `+0x220/+0x224` | Final Left / Right motor volumes |
| `+0x228` | `draftScale` multiplier (reset to `1.0` in `BeginUpdate`) |
| `+0x230` | Enabled byte |

Persistent edge-detection fields live on the Xbox `FeedbackInterface` object:

| Event state | Xbox feedback-interface offset |
| --- | --- |
| NOS engaged | `+0x588` |
| Engine blown | `+0x589` |
| Shifting | `+0x58A` |
| Previous shift potential | `+0x58C` |

### Surface layout and waves

Unlike Most Wanted (where `SimSurface` stores `mCollection` at `+0x04`),
Carbon's `Attrib::Instance` stores `mCollection` at `+0x00`. Xbox
`UpdateRoadNoise` (`0x8244A718`), `UpdateTireSkid` (`0x8244A810`) and
`UpdateTireSlip` (`0x8244A920`) load `lwz r3, 0(r28)` (`*(const void**)surface`)
and query `Attrib::Collection::GetData(key, index)` (`0x8218D560`); PC
`Attrib::Instance::GetAttributePointer` (`0x00463A80`) likewise dereferences
`[ecx+0]` and jumps to `0x00463480`.

Wave interpolation (`0x82423390`), front-wheel `0.25` skid damping, and wave
sampling (`0x824488D8`) are identical to Most Wanted.

### Drafting (`UpdateDrafting`)

`UpdateDrafting` (`0x8244AE18` and helper `0x8244ADB8`) receives the
`0x20`-byte `DraftingInfo` struct from `PVehicle + 0x1C0`:

- `+0x00` (`uint8_t`): drafting active/enabled. If zero, returns immediately.
- `+0x01` (`uint8_t`): in slipstream (`inSlipstream`).
- `+0x1C` (`float`): time in leader wake (`wakeTime`, seconds).

When `wakeTime < 1.5f` (`0x8205E480`), `0x8244AE18` computes
`f = 1.0f - wakeTime * 0.666666687f` (`2.0f / 3.0f` at `0x820C80D4`) and calls
`0x8244ADB8`. If the port is enabled (`+0x230 != 0`), `0x8244ADB8` writes to
the Right draft wave (`+0x110`):

- `phase = Sim::mTime * 1000.0f` (`*(float*)0x82C1326C * 1000.0f`)
- `amplitude = f * 0.6f + 0.4f`
- `frequency = f * 60.0f`

When `wakeTime >= 1.5f` and `inSlipstream != 0` (`byte[+0x01] != 0`),
`0x8244AE18` sets `draftScale` (`+0x228`) to `0.5f` (without checking
`enabled`). In `Run` (`0x82449698..0x824496EC`), both Left and Right motor
maxima are multiplied by `draftScale` before clamping to `0.0f..1.0f`.
`BeginUpdate` (`0x8244A690`) resets `draftScale` to `1.0f` and clears
`amplitude`, `frequency` and `offset` of all `Draft` and `Drift` waves while
preserving `phase`.

### Drift (`UpdateDrift`)

`UpdateDrift` (`0x8244AED0`) runs instead of the per-wheel surface loop when
`IVehicle::GetDriverStyle() == 2`. It does not check the port's `enabled` byte:

1. Reads `simTimeMs = Sim::mTime * 1000.0f` and `a = fabsf(slipAngle)`.
2. If `a <= 0.01f` (`0x82062748`), returns.
3. Computes `t1 = min((a - 0.01f) * 8.16326523f, 1.0f)` (`0x820C86F4`) and
   writes to the Right drift wave (`+0x150`):
   - `phase = simTimeMs`
   - `amplitude = t1 * 0.6f + 0.4f`
   - `frequency = 60.0f`
4. If `a <= 0.015f` (`0x8206274C`), returns.
5. Computes `t2 = min((a - 0.015f) * 8.24742222f, 1.0f)` (`0x820C86F0`), copies
   the 16-byte Left drift wave (`+0x140`, which is never written and remains
   zero, clearing `offset` at `+0x15C` to `0.0f`) into `+0x150`, and overwrites
   `+0x150` with `phase = simTimeMs`, `amplitude = t2 * 0.6f + 0.4f`,
   `frequency = 60.0f`.

### Ramps and collisions

`UpdateShiftPotential` (`0x8244AA20`), `UpdateNOS` (`0x8244AC80`),
`UpdateEngineBlown` (`0x8244ABD8`), `UpdateShifting` (`0x8244AD40`),
`PauseEffects` (`0x8244A498`), `ResumeEffects` (`0x8244A520`) and
`ResetEffects` (`0x8244A5A8`) follow the exact same ramp parameters and state
rules as Most Wanted.

`ReportCollision` (`0x8244AFB8`) uses the same participant resolution and
branch structure as Most Wanted, with two updated scaling constants in the
low-delta object branch (`dv < 5.0f`):

- Rear-end estimate (`dot > 0.1f`): scales `Length(cinfo.objAVel)` by `0.25f`
  (`0x82063B34` at `0x8244B27C`, versus MW's `0.5f`) before testing `> 5.0f`
  and computing `clamp((rearDelta - 5.0f) / 20.0f)`.
- Moving contact (`liveSpeed > 2.0f` and `Length(otherDelta) > 2.0f`):
  multiplies `clamp((liveSpeed - 20.0f) / 30.0f)` by `0.75f` (`0x8205E444` at
  `0x8244B2BC`, versus MW's `1.0f`).

## PC adapter addresses (`NFSC.exe`)

| Symbol / constant | Address / value |
| --- | --- |
| `GAMEFLOWMANAGER_STATUS_ADDR` | `0x00A99BBC` |
| `PLAYER_ILISTABLE_ADDR` / `COUNT_ADDR` | `0x00A9FF2C` / `0x00A9FF34` |
| `UTL_ILIST_ADD_ADDR` / `FIND_ADDR` | `0x0060DCB0` / `0x0060CB50` |
| `IFEEDBACK_IHANDLE_ADDR` | `0x00679170` |
| `ISUSPENSION_IHANDLE_ADDR` | `0x00403CE0` |
| `IRIGIDBODY_IHANDLE_ADDR` | `0x00403750` |
| `IVEHICLE_IHANDLE_ADDR` | `0x00403D30` |
| `IINPUT_IHANDLE_ADDR` | `0x00403AA0` |
| `ATTRIB_GETATTRIBPTR_ADDR` | `0x00463A80` |
| `ATTRIB_FINDCOLLECTION_ADDR` | `0x00465930` |
| `SIM_GETSIMABLE_ADDR` | `0x0041EE60` |
| `SIM_TIME_ADDR` (`Sim::mTime`) | `0x00B4CFD8` |
| `PLAYER_COLLISION_RUMBLE_PATCH_ADDR` | `0x007652C8` |
| `IVEHICLE_GET_ABSOLUTE_SPEED_OFFSET` | `0xAC` |

The suspension constructor at `0x006DB9F0` registers handle `0x00403CE0`.
Handle `0x00403C80` belongs to transmission (constructor `0x006DB810`);
using it for wheel reads corrupts the stack. In `DoFFB`, the `push edi` at
`0x00762108` shifts the local stack offsets: the wheel loop uses the interface
queried by `0x0040F020`, not `0x0040F1F0`. Carbon's optional vehicle reader
also uses `IInput::GetControlHandBrake` at `+0x1C`; `+0x20` is clutch.
The Carbon harness exercises a full vehicle read with separate native interface
handles and pedal values, alongside the feedback-device lookup tests.
