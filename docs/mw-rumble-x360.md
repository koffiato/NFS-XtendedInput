# Most Wanted feedback research

PC, PS2 and Xbox 360 share feedback producers but use different output paths.
The MW adapter implements the recovered console waves, car/collision ramps
and pause/reset state rules. Body rumble has been tested and found accurate
to the Xbox 360 behavior in gameplay. Impulse triggers are an optional extension.

## Binaries and address mapping

| Input | Size | SHA-256 |
| --- | ---: | --- |
| PC Retail `speed.exe` | 6,029,312 | `80774c2e5d619b4f120b48d4462896fd504c263399d203a238769cffde1d253c` |
| PS2 Black Edition USA `SLUS_213.51;1` | 4,918,740 | `b8207db2506a8acd7228967819bfeae1169337901dc60efaecc662afd50e1208` |
| Retail `default.xex` | 3,936,256 | `aebdf3c14d1e4e4ed2c130d94bc1a0e97bd6d5bf39bdfdef33c3909c9cc4ba4c` |
| XexTool expanded image, `default.exe` | 13,434,880 | `e0002893223a1425e9cd760171921689007093b03ad3d597e156d4c15de09618` |

PC addresses use normal PE section mapping. The expanded Xbox image loads at
`0x82000000`, with `fileOffset = VA - 0x82000000`. Its `.text` starts at
dump offset `0xE0000`; the inherited raw pointer `0xDD800` is for the original
PE layout. Re-extracting the ISO XEX with `XexTool -b` produced the same image.
The XEX is at ISO offset `0x0FD90000 + 745014 * 2048`.

The PS2 USA Black Edition ELF is at ISO9660 sector 300. PS2 addresses are EE
virtual addresses after loading `PT_LOAD` segments. Names come from the MW
decompilation's `config/SLUS-21351/symbols.txt`; retail instructions establish
behavior and layout.

## Feedback interface and producers

Feedback tables: PC `0x008A7BD4`, PS2 `0x004EB038`, Xbox `0x820867C8`.

| Slot | Method | PC implementation | PS2 implementation | Xbox implementation |
| ---: | --- | --- | --- | --- |
| 0 | Destructor | `0x00634CF0` thunk | `0x00305DA0` | `0x823A52E8` |
| 1 | PauseEffects | `0x00628800` | `0x00305E38` | `0x823A4630` |
| 2 | ResumeEffects | `0x00628870` | `0x00305E88` | `0x823A46A8` |
| 3 | ResetEffects | `0x00628880` | `0x00305EC8` | `0x823A4720` |
| 4 | BeginUpdate | `0x00628890` | `0x00305F90` | `0x823A4808` |
| 5 | EndUpdate | `0x006288A0` | `0x00306028` | `0x823A4868` |
| 6 | UpdateRoadNoise | `0x006288B0` | `0x00306030` | `0x823A4870` |
| 7 | UpdateTireSkid | `0x006288C0` | `0x003061B0` | `0x823A4968` |
| 8 | UpdateTireSlip | `0x006288D0` | `0x00306350` | `0x823A4A78` |
| 9 | UpdateRPM | `0x006288E0` | `0x003064D0` | `0x823A4B70` |
| 10 | UpdateShiftPotential | `0x006288F0` | `0x003064F8` | `0x823A4B78` |
| 11 | UpdateNOS | `0x00628900` | `0x00306798` | `0x823A4DF8` |
| 12 | UpdateEngineBlown | `0x00628910` | `0x00306688` | `0x823A4D40` |
| 13 | UpdateShifting | `0x00628920` | `0x003068A8` | `0x823A4EB8` |
| 14 | ReportCollision | `0x00628930` | `0x003069A8` | `0x823A4F30` |

PC embeds `IFeedback` at device `+0x2C`, with its owner-list pointer at
interface `+4` (constructor `0x00634B50`). Its surface/car/collision methods
are empty returns. Pause calls native device operations; Reset forwards to
Pause. PS2 table entries contain signed `this` adjustments of `-0x40` and
use eight bytes per entry, versus four-byte function pointers on PC/Xbox.

`LocalPlayer::DoFFB` is PC `0x006EE490`, PS2 `0x003988B8`, Xbox
`0x8246E420`. The controller sequence is:

1. Resume and BeginUpdate.
2. For each wheel: absolute slip, absolute skid, then absolute vehicle speed.
3. RPM, shift potential, optional NOS, then engine damage **or** shifting.
4. EndUpdate.

Airborne wheels use the null surface. Grounded damaged tyres use surface
`0xD929E923`. A connected secondary steering interface takes an earlier
force-update branch. RPM writes no controller effect state; EndUpdate is empty.

PS2 `CanDoFFB` (`0x00398788`) checks profile Rumble, global enable, input
pause, human AI control and vehicle animation. PC `CanDoFFB` (`0x006EE400`)
checks the latter four conditions but omits the profile byte. PC steering
UpdateForces and collision production still check profile Rumble at settings
`+4`. The adapter bypasses the collision-setting jump at `0x006F23EA`.

## Console effect state

State arrays: PS2 `0x005AF930`, Xbox `0x82919810`; stride `0x1B0`.

| Offset | Contents |
| --- | --- |
| `+0x000..0x0FF` | Sixteen `{phase, amplitude, frequency, offset}` waves in four groups |
| `+0x100..0x19F` | Ten `{remaining, duration, amplitude, floor}` ramps, alternating left/right |
| `+0x1A0/+0x1A4` | Final motor volumes |
| `+0x1AC` | Enabled: PS2 word, Xbox byte |

Persistent event fields are outside this array:

| Event state | PS2 device offset | Xbox feedback-interface offset |
| --- | --- | --- |
| NOS engaged | `+0x688` | `+0x2E0` |
| Engine blown | `+0x68C` | `+0x2E1` |
| Shifting | `+0x690` | `+0x2E2` |
| Previous shift potential | `+0x694` | `+0x2E4` |

### Surface waves

Road, skid and slip use groups `+0x000`, `+0x040` and `+0x080`, with keys
`0xBA1297DA`, `0x0C149044` and `0x8FD11D27`. BeginUpdate clears their
amplitude/frequency/offset parameters, retaining phases, the fourth group
and all ramps.

Each callback queries four native attribute indices. An absent record uses
a zero default. Eight-float records contain frequency/amplitude/offset and
input-range endpoints. Interpolation requires `x >= xMin` and `xMin < xMax`,
then clamps the interpolation fraction to one. Each parameter independently
takes the maximum across wheels. Front skid input is multiplied by 0.25;
road and slip have no front/rear scaling.

PS2 attribute access is `0x00155D68`; Xbox interpolation is `0x823802A8`.
Sampling advances phase only when amplitude times frequency is positive,
subtracting one cycle if phase exceeds one. Signed channel pairs are summed,
effect groups take maxima, and final motors clamp to 0..1. PS2 Run is
`0x00304090`; Xbox Run/wave/ramp evaluators are `0x823A36C8`,
`0x823A3540` and `0x823A34D8`.

### Ramps and collisions

For positive remaining time and duration, sampling first subtracts elapsed
seconds, then evaluates
`max(0, floor + (amplitude-floor) / duration * remaining)`.

| Effect | State offset | Amplitude / floor | Duration | Routing / retrigger |
| --- | --- | --- | --- | --- |
| Collision | `+0x100` | strength / 0 | strength seconds | Left; active remaining time, duration and amplitude each take the maximum with incoming strength |
| Shift potential | `+0x120/+0x130` | 0.5, 0.75 or 1 / same | 3 seconds | Both; potential values 3, 4, 5 trigger on increases; nonzero previous potential clears both ramps on the next callback |
| NOS | `+0x140` | 1 / 0.5 | 2 seconds | Left; rising edge starts, falling edge clears |
| Engine blown | `+0x160/+0x170` | Left 1 / 0.6; right retains amplitude capped at 1 / 0.6 | 1 second | Both; rising edge starts, falling edge clears. Initially zero right amplitude rises toward 0.6 |
| Shifting | `+0x180/+0x190` | 0.4 / 0.4 | 0.5 seconds | Both; rising edge starts; falling edge leaves playback running |

Xbox initializers are in the interface table; collision ramp initialization
is at `0x823A5268`. The PS2 collision state-writing tail is
`0x00306EBC..0x00306F08`.

Collision handles are at `+0x3C/+0x4C`, stored velocities at `+0x50/+0x60`.
PC handle resolution is `0x0041AD40`, Xbox `0x8211DE40`. PC/PS2 type bits
are the low three bits; Xbox extracts the high three at `0x823A5094`.
Both console callbacks use these strength rules:

| Contact | Strength |
| --- | --- |
| World | `clamp(deltaSpeed / 30)` |
| Ground | `clamp((deltaSpeed-1) / 3)` |
| Object, delta at least 5 m/s | `clamp((deltaSpeed-5) / 20)` |

The low-delta object branch compares both live velocities with the selected
participant's stored velocity. Its rear-end estimate takes object A's stored
speed twice; it does not use the maximum of A/B speeds. Otherwise, moving
contacts require both computed speed values above 2 m/s and use
`clamp((liveSpeed-20) / 30)`. Sliding flags do not bypass this calculation.
Only the left collision ramp is written. PS2 rejects wheel devices in
surface/car/collision callbacks; Xbox's controller callbacks omit that query.

### Timing and lifecycle

Both console samplers accept milliseconds, convert to seconds and skip
advancement when disabled or delta is nonpositive. Their normal polling
paths Push the previous volume before sampling:

| Platform | Clock / update path | Push then Run |
| --- | --- | --- |
| PS2 | Port-zero PollDevice `0x00305840`, EE count register, `bGetTickerDifference` `0x0015B650`, updater `0x00305020` | `0x003051B0`, `0x003051BC` |
| Xbox | Hardware-clock delta at `0x823A4050`, updater `0x823A3D18` | `0x823A3E30`, `0x823A3E3C` |

Static code establishes this order, but not a fixed polling frequency.

| Call | State changes |
| --- | --- |
| Pause | Disable and Push; preserve waves, ramps, volumes and event fields |
| Resume | Enable and Push preserved volumes |
| Reset, output effect present | Clear volumes and left collision ramp, disable and Push |
| Reset, no output effect | Leave state unchanged |

PS2 Pause also calls `SteeringWheels_StopAllForces` at `0x00244550`.

### Motor output

PS2 Push (`0x00304D30`) converts `leftVolume * 255` with `cvt.w.s` and
sets the second effect word to zero. SetInfo (`0x004917F0`) copies their low
bytes to effect `+0x2D` and `+0x2C`. Start (`0x00491750`) sends the six-byte
packet at `+0x2C` through `scePadSetActDirect` (`0x00494CC0`); Stop
(`0x00491788`) sends zeros. Actuator alignment is `{0,1,255,255,255,255}`
at `0x005A7108`, installed at `0x004926F0`. This path drives the variable
large motor and leaves the binary small motor off; it discards rightVolume.

Xbox Push (`0x823A3A70`) converts both volumes to 16-bit requests. Constructor
`0x8261CA00` installs vibration table `0x820AFAAC`: destructor
`0x8261CA58`, active query `0x8261CC08`, send `0x8261CB38`, stop
`0x8261CB88`, setter `0x8261CBD8`. The setter accepts
`{left,right,immediate}`. Device fields are port `+4`, active byte `+8`,
motor words `+0xC/+0x10`, packed output `+0x14/+0x16`. Send/stop call
`0x8262D6C8`, which branches to `XamInputSetState` import `0x828A9C9C`
(ordinal `0x192`).

Legacy SmallRumble, BigRumble and SingleRoadBump arrays exist at Xbox
`0x828FD47C`, `0x828FD49C` and `0x828FD514`, but their controller routing
is unproven. PS2's direct `tEnvelope::GetValue` calls at `0x00318090` and
`0x00318180` belong to camera-shake code.

## PC DirectInput forces

PC has an active steering-force path alongside its empty controller callbacks.
GameDevice allocates a 32-byte secondary object through `0x006278F0`, stores
it at `+0x184C` and installs table `0x008A7A30`. Its connected query
(`0x00628E10`) uses manager `0x009203C4` and lookup `0x006CA110`.
DoFFB calls UpdateForces (`0x006430D0`) and returns when this interface is
connected (`0x006EE4DD..0x006EE50B`). The replacement InputDevice returns
zero from GetSecondaryDevice, so replacement ports use the plugin's controller
interface instead. Physical force rendering depends on the controller driver.

UpdateForces checks connection, profile Rumble, input pause, vehicle and
suspension, then produces:

| Producer range | Force input |
| --- | --- |
| `0x0064318F..0x00643206` | Speed-dependent centering spring via `0x006D8680` |
| `0x0064320B..0x006432E4` | Front-wheel skid sum, scaled/clamped to -255..255 while moving, converted to directional constant force via `0x006D8A60` |
| `0x006432E9..0x00643309` | Speed-dependent damper via `0x006D8AA0` |
| `0x0064330E..0x00643371` | Suspend when both front wheels are airborne; resume cached forces on contact |
| `0x00643371..0x00643425` | Signed speed change above 4 m/s, with a 0.15-second timing gate, feeds transient helper `0x006DC800` |
| `0x0064342C..0x006434C0` | Averaged grounded front-surface WheelEffectIntensity/Frequency, intensity scaled with speed, feeds `0x006DCDA0` |

The speed fraction is `min(abs(signedSpeed)*0.05,1)`. Spring saturation
scales with it; damper coefficients scale with its complement. Drag-race
predicate `0x004D6150` selects spring values of 7,764 and stops directional
constant force.

Surface layout fields `+0xF8/+0xFA` are WheelEffectFrequency (16-bit) and
WheelEffectIntensity (8-bit), keys `0x968C3422/0xB7E0AF1C`. These are
separate from RenderNoise and the controller's eight-float wave records.

### Descriptors

Descriptors are 56-byte DIEFFECTs: flags `0x12` (object offsets/Cartesian),
gain 10,000, sample period zero, no trigger, repeat zero. Axis offsets are
0 and optionally 4; directions start at `{1,1}`. Times below are microseconds.

| Helper / bank slot | Effect | Duration | Parameters |
| --- | --- | --- | --- |
| `0x006D8680`, `+0x4728`, slot 0 | Spring `0x13541C27` | Infinite | DICONDITION: offset/deadband 0, equal positive/negative coefficients and saturation. Second axis uses coefficients/saturation 8,000, deadband 500 |
| `0x006D8AA0`, same bank, slot 1 | Damper `0x13541C28` | Infinite | Offset/deadband 0, equal requested coefficients, saturation 10,000 |
| `0x006D88B0`, `+0x47A8`, slot 0 | Constant `0x13541C20` | Infinite | Signed magnitude; helper `0x006D8A60` applies the direction table |
| `0x006DC800`, `+0x4828`, slot 0 | Square `0x13541C22` | 150,000 | Requested magnitude, offset/phase 0, period 75,000; zero attack, fade level 0, fade time 19,999 |
| `0x006DCDA0`, same bank, slot 3 | Selected periodic waveform | Infinite | Requested magnitude, offset/phase 0, period `min(request,150)*1000`; zero envelope |

Surface waveform selectors 0..4 choose sine, square, triangle, sawtooth-up
and sawtooth-down GUIDs at `0x008C042C..0x008C046C`. The frequency-named
input becomes a period, not Hz. Zero period/magnitude can reach CreateEffect.

### Gates and replay

Offsets below are relative to the force manager, plus `port*4`:

| Field | Gate |
| --- | --- |
| `+0x4470` | DirectInput device pointer; required |
| `+0x4478` | Actuator-axis count; zero disables forces. Enumeration callback `0x006C9E30` tests DIDOI_FFACTUATOR and caps at two |
| `+0x2220` | Suspension flag; blocks updates until resume |
| `+0x4460` | Product GUID Data1 `0xC291046D` (VID 046D/PID C291); suppresses surface periodic effects and changes constant/transient replay |
| `+0x4468` | Product `0xC218046D` (VID 046D/PID C218); suppresses directional constant force |

Product checks are at `0x006DC678..0x006DC71B`, in the input-manager
subobject at outer manager `+0x2228`. The second-port C291 check reads device
info through the first device pointer.

| Condition | Operation |
| --- | --- |
| Effect absent | CreateEffect, then Start(1,0) |
| Playing continuous effect, unchanged parameters | No update or restart |
| Parameters changed | SetParameters(`0x140`: direction/type parameters); C291 constant force additionally calls Start twice |
| Stopped continuous effect, unchanged parameters | Start existing effect |
| Surface waveform changes while playing | Unload, recreate and Start |
| Transient request within 150 ms | Add magnitude to cached transient, update if changed, then Start again |
| Later transient request | Replace magnitude, update if changed, then Start again |
| Both front wheels airborne | `0x006E1560` records playing effects, Stops them and sets suspension |
| Contact returns | `0x006E16F0` clears suspension and restarts recorded effects |

`0x006CA5F0` queries GetEffectStatus for DIES_PLAYING; it does not query
effect support. Shared Start/Stop/Unload helpers are `0x006CA440`,
`0x006CA4D0` and `0x006CA560`. CreateEffect reaches device vtable `+0x48`
at `0x006CAC0D`; SetParameters reaches effect `+0x18` at `0x006CADAC`
and `0x006D9762`. Failed creation/updates do not advance parameter caches;
a failed surface update is retried on the next identical request.

## Retail surface data

All 47 simsurface collections have matching inherited FFB_ROLL/SKID/SLIP
records and WheelEffectFrequency/Intensity fields across PC, PS2 and Xbox.
There are 15 collections with road records, none with skid records, and 36
with slip records. All layouts are present. Names resolve through layout
`+0xDC` pointer fixups. Other physics/rendering fields were not compared.

| Retail attributes bank | Size | SHA-256 |
| --- | ---: | --- |
| PC `GLOBAL/attributes.bin` | 689,728 | `3d0362f2c052b2f93576852f504cb7806b14eefbf83b8531d45dc1d430ec6337` |
| PS2 `GLOBAL/ATTRIBUTES.BIN` | 703,296 | `16f3900df6232ef94c0239a0ed5476e4b75719afee3a1b4338bcb53b01efeebb` |
| Xbox `GLOBAL/ATTRIBUTES.BIN` | 697,856 | `9f4b04a221232e614b927a870e25da7db100477a7e425114b87082f46e8b99ad` |

All banks have little-endian VPAK headers. VLT data is little-endian on
PC/PS2 and big-endian on Xbox. Comparison resolves parents, pointer fixups,
eight-byte array headers and all eight floats per wave record.
The archive filename hash is `0x9775379A`:

| Platform | Archive | Local sector | Archive start sector |
| --- | --- | ---: | ---: |
| Xbox | ZZDATA4.BIN | 140194 | GDF sector 1526521 |
| PS2 | NFS/ZZDATA2.BIN | 310717 | ISO9660 sector 941549 |

Asphalt, asphalt_no_leaves, concrete, metal and cobble have one heavy-channel
slip record and no road/skid records. Grass adds two heavy road channels.
Null and unknown have no controller waves. The full comparison also covers
damaged tyres, dirt, gravel, sand, mud, railroad, wet pavement and slicks.

## Adapter implementation

The adapter receives the native PC DoFFB and collision callbacks. It reads
surface records through the game's attribute accessor and resolves collision
participants through `0x0041AD40`, then calls PC ISimable::GetLinearVelocity
at vtable `+0x68`. Vehicle checks use the native interface list.

| Area | Behavior |
| --- | --- |
| Collisions | Recovered strength calculation, left-only ramp and maximum retrigger rule, including the asymmetric rear-end branch |
| Sliding | Same collision calculation; no separate scrape motor |
| Zero measured delta | No contact-normal fallback; low-delta object contacts retain the console branches |
| Suspension | Ground collision callbacks only; no compression-derived impulse |
| Shifting | Native engine-damage-or-shifting producer branch; no extra transmission query |
| Lifecycle | Pause preserves state; reset clears volumes and the left collision ramp when an output device is available |
| Sampling | Full positive polling delta, previous-sample output order and 16-bit motor levels |
| PC DInput | Separate reference; its wheel-force effects are not mixed into controller output |

Reset's output-effect query maps to an available XInput or WGI pad. Missing
pads receive zero output without clearing logical effect state. Focus loss
and feedback older than 250 ms stop output and suspend sampling; polling
continues to update the clock. Device detach clears adapter state. These are
PC safety extensions, separate from gameplay Pause/Resume/Reset.

Intensity, motor/category gains, `GearShift` and impulse triggers are optional
extensions. Use unit gains, `GearShift = 1` and `ImpulseTriggers = 0` for the
console baseline. All collisions, including ground contacts, use `Impacts`.

## References

[Upstream issue 10](https://github.com/xan1242/NFS-XtendedInput/issues/10#issuecomment-1181213340)
documents interface embedding and PC stubs. The
[`rumble-test` branch](https://github.com/xan1242/NFS-XtendedInput/blob/rumble-test/dllmain.cpp)
adds structures and interface registration, but its effects are stubs and
UpdateNOS only prints diagnostics.

PS2 instruction decoding follows the
[PCSX2 opcode tables](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/R5900OpcodeTables.cpp).
