# Most Wanted PC and Xbox 360 feedback research

The existing PC adapter uses the game's feedback interface and surface data.
Several surface calculations match the Xbox implementation, but collision
effects, waveform clipping and update order differ.

The recovered path is vehicle telemetry and collision listeners -> concrete
`IFeedback` state -> periodic sampling/mixing -> vibration device ->
`XamInputSetState`. A telemetry callback's existence does not establish
motor output. In particular, the Xbox RPM callback is empty.

## Binary provenance and address mapping

| Input | Size | SHA-256 |
| --- | ---: | --- |
| PC Retail `speed.exe` | 6,029,312 | `80774c2e5d619b4f120b48d4462896fd504c263399d203a238769cffde1d253c` |
| Retail `default.xex` | 3,936,256 | `aebdf3c14d1e4e4ed2c130d94bc1a0e97bd6d5bf39bdfdef33c3909c9cc4ba4c` |
| XexTool expanded image, `tools/mw_x360/default.exe` | 13,434,880 | `e0002893223a1425e9cd760171921689007093b03ad3d597e156d4c15de09618` |

The ISO XEX at `0x0FD90000 + 745014 * 2048` matches the existing extracted
XEX byte for byte. A fresh `XexTool -b` extraction from that XEX also
matches the existing expanded image byte for byte. Load this output as a
binary at `0x82000000`.

For this Xbox dump, `fileOffset = VA - 0x82000000`. Its inherited PE
headers list `.text` RVA `0xE0000`; that section starts at dump offset
`0xE0000`. The inherited raw pointer `0xDD800` describes the original PE
file layout. The PC executable uses ordinary PE section raw pointers.

Xbox `0x8261CB38` is at dump offset `0x61CB38` and begins with
`7D8802A6` (`mflr r12`).

## Verified interface and output path

PC feedback vtable: `0x008A7BD4`. Xbox feedback vtable: `0x820867C8`. The
plugin's MW virtual declaration order matches both tables:

| Slot | Method | Xbox implementation |
| ---: | --- | --- |
| 0 | Destructor | `0x823A52E8` |
| 1 | PauseEffects | `0x823A4630` |
| 2 | ResumeEffects | `0x823A46A8` |
| 3 | ResetEffects | `0x823A4720` |
| 4 | BeginUpdate | `0x823A4808` |
| 5 | EndUpdate | `0x823A4868` |
| 6 | UpdateRoadNoise | `0x823A4870` |
| 7 | UpdateTireSkid | `0x823A4968` |
| 8 | UpdateTireSlip | `0x823A4A78` |
| 9 | UpdateRPM | `0x823A4B70` |
| 10 | UpdateShiftPotential | `0x823A4B78` |
| 11 | UpdateNOS | `0x823A4DF8` |
| 12 | UpdateEngineBlown | `0x823A4D40` |
| 13 | UpdateShifting | `0x823A4EB8` |
| 14 | ReportCollision | `0x823A4F30` |

`DoFFB` is at PC `0x006EE490`, Xbox `0x8246E420`. Slip is called first,
through **slot 8**, then skid through **slot 7**, then road through slot 6.
Slot 5 is `EndUpdate`. Both Xbox `EndUpdate` and `UpdateRPM` are single `blr`
instructions. An RPM telemetry call does not establish engine rumble.

The PC destructor adjusts `this` by `-0x2C`, PauseEffects calls existing
device operations, and ResetEffects forwards to PauseEffects. Slots 6..14 are
empty returns with the expected argument cleanup. PC constructor `0x634B50`
registers the feedback interface at device `+0x2C`, with owner-list pointer at
interface `+4`, matching the plugin's embedding.

Xbox constructor `0x8261CA00` installs concrete vibration vtable
**`0x820AFAAC`**, with five entries: destructor `0x8261CA58`, active query
`0x8261CC08`, send `0x8261CB38`, stop `0x8261CB88`, setter `0x8261CBD8`.
The setter takes a pointer to three words `{left,right,immediate}`.
Port is at `+4`, active byte at `+8`, motor words at `+0xC/+0x10`, and the
packed output at `+0x14/+0x16`.
The allocation size has not been established.

Send calls wrapper `0x8262D6C8` at `0x8261CB64`; stop calls it at
`0x8261CBB4`. The wrapper tail-branches to the import thunk described by
the XEX-generated IDC as `XamInputSetState`, ordinal `0x192`, at
`0x828A9C9C`. Import thunk instructions need runtime/import setup; empty
bytes in an expanded image are not evidence that the call is absent.

## What matches, and what still differs

| Area | Direct evidence | Adapter assessment |
| --- | --- | --- |
| Surface records | Xbox `0x823802A8` interpolates three float parameters between offsets `0/4/8` and `0x10/14/18`, using range `0xC..0x1C` | Matches the adapter's eight-float record and range clamp |
| Surface keys | Road `0xBA1297DA`, skid `0x0C149044`, slip `0x8FD11D27` in their concrete callbacks | Matches |
| Wheel aggregation | Four channels; independent maxima for each interpolated parameter | Matches `RumbleWave::Add` |
| Front skid | `0x823A49AC..B4` multiplies input by float at `0x820624E8` = `0.25` | Matches the adapter's front-wheel weighting |
| RPM | Concrete Xbox slot 9 is empty | Plugin's telemetry-only callback is consistent |
| Surface sampling | `0x823A3540` keeps signed wave samples; `0x823A36C8` sums channel pairs, takes maxima across effects, then clamps final motors | Plugin clamps each channel and each pair earlier; negative cancellation can differ |
| Collision state | `0x823A4F30..52E0` writes ramp state; `0x823A34D8` evaluates remaining-time ramps | Current Small/Big envelope playback is not this recovered path |
| Collision flags | Xbox type extraction at `0x823A5094` shifts right 29 bits; PC masks low three bits | Raw collision records cannot be copied between platforms |
| Timing/order | Xbox wave/ramp evaluators multiply supplied delta by `0.001`; periodic caller `0x823A3D18` sends before advancing samples at `0x823A3E2C..3C` | Xbox converts milliseconds to seconds; plugin samples and sends in EndUpdate. Update cadence remains unmeasured |
| Shifting | Stock Xbox and PC DoFFB select engine-damage callback **or** shifting callback | Plugin's shifting branch patch is an intentional behavior extension |

The SmallRumble curve's numeric sequence is present in the Xbox image at
`0x828FD47C`. Its callers and role in GameDevice collision feedback remain
to be established.

The recovered surface/collision path uses game feedback state at
`0x82919810`, stride `0x1B0`, and output bridge `0x823A3A70`.

## Research boundaries and next checks

The adapter's suspension fallback, impulse triggers, lifecycle handling and
shifting branch patch are extensions. Its collision envelopes are not yet a
port of the recovered Xbox ramp state. Do not change effect behavior until
the following questions have been resolved from the concrete implementations:

- Recover all ramp initializers, duration units, retrigger rules and motor routing.
- Confirm waveform phase advancement, signed summation and final clamping.
- Trace the periodic sampler's caller and measure update cadence and ordering.
- Translate collision fields at the PC boundary; the Xbox flag layout differs.

No game launch, controller-output test or independent console trace was made
for this research. Compilation and mocked tests do not establish authenticity.

To reproduce the binary mapping, use `XexTool -b <output> <default.xex>` and
load the resulting image as big-endian PowerPC at `0x82000000`. Inspect
the interface table, its concrete callbacks and their callers together.
The workspace's read-only helper is `tools/mw_audit.py` (Python + capstone);
`--x360` selects the expanded-memory layout. It is research tooling outside
this repository. No retail executable or bank is required by the build or CI.
