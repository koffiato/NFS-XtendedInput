# Undercover Xbox 360 controller feedback

The UC adapter ports the retail console controller mixer using PC producer
callbacks and PC attributes. This is a code and binary comparison; physical
controller or emulator trace equivalence has not been established.

Verified findings updated on 2026-11-09. This document supersedes the earlier
Undercover analysis that applied a `+0xDE00` address adjustment, treated the
wheel actuator as the controller mixer, or preserved wave phase between frames.

## Binary and address mapping

Reference `default.exe`: 25,034,752 bytes, SHA-256
`befa136b20e47fc62b8129ccb32b97e6163ddb0bff52760d532788fc3e925c71`.
Title ID: `45410876`. The extracted XEX hash is
`97ac0c072253d46458dc6d57c6609922adbd3b9658216940282c361ad575e221`.

**This XexTool output is a memory-layout image.** Read instructions at
`fileOffset = VA - 0x82000000`, including `.text`. The original PE header's
raw pointer `0x1D2200` is stale: `.text` bytes actually begin at offset
`0x1E0000`. For example, vtable slot 4 points to `0x82610A48`, whose code
begins at file offset `0x610A48`. Adding `0xDE00` to vtable entries produces
incorrect runtime addresses. Older research used that erroneous displacement.

## Interface and actual producers

PC `IFeedback` is at `0x00C0D434`, Xbox at `0x8212CC70`. Both have 21 slots.
The PC collision callback passes its 12-byte vector by value and must retain
`ret 0x14` cleanup. Participant velocity is slot 24 (`+0x60`), unlike MW/CB.
See [PC interface notes](uc-rumble.md).

| Slot | Callback | Xbox VA | Controller behavior |
| ---: | --- | --- | --- |
| 4 | BeginUpdate | `0x82610A48` | Clear complete wave records, including phase |
| 5 | EndUpdate | `0x82610AE8` | Stub; device polling runs the mixer |
| 6 | Road | `0x82610AF0` | Four `FFB_ROLL` records, speed input |
| 7 | Tyre skid | `0x82610BE8` | Load-shaped curve into `FFB_SKID` |
| 8 | Tyre slip | `0x82610D30` | Signed slip curve into `FFB_SLIP` |
| 9 | RPM | `0x82610E68` | Stub; no independent engine buzz |
| 10 | Shift potential | `0x82610E70` | Update ramps; Run discards samples |
| 11 | NOS | `0x826110E8` | Left ramp, 2 s, 1.0 to 0.5 |
| 12 | Engine blown | `0x82611028` | Update ramps; Run discards samples |
| 13 | Shifting | `0x826111A8` | Update ramps; Run discards samples |
| 14 | Drift | `0x82611290` | Two waves, one per body motor |
| 15 | Drift tyre slip | `0x82611338` | Piecewise curve into normal slip group |
| 16 | Drift tyre skid | `0x826114A8` | Angle curve using `FFB_DRIFT_SKID` |
| 17 | Steering | `0x8260F6B8` | Wheel-only stub |
| 18 | Indexed bump | `0x82611660` | Threshold/gain into side-specific ramps |
| 19 | Collision | `0x826117E0` | Select authored collision wave records |

The ordinary wheel loops in BOTH PC `0x007BA910..0x007BAA15` and Xbox
`0x827A1C0C..0x827A1D80` dispatch slot 6. They query two chassis values and
discard them. Contrary to older notes, that loop does not dispatch slots 7/8.
The adapter preserves the other native callbacks for their separate callers;
it does not invent extra tyre events or replace the game's producer loop.

## Curves and mixing

Drift is not Carbon's absolute-angle threshold curve. When active, slot 14
writes the same parameters to the left and right waves:

- Phase = simulation time in milliseconds.
- Amplitude = first float * 0.5 + 0.2.
- Frequency = second float * 30 + 60 Hz.
- Inactive clears both waves. Paused callbacks leave state unchanged.

Undercover BeginUpdate clears phase, amplitude, frequency and offset in all
six wave groups. It preserves ramp timers. Reusing MW/Carbon phase continuity
caused the previous adapter to develop a different texture over successive
frames. The reset map is now checked by executing the retail BeginUpdate
instructions for ports 0, 1 and 3 in the binary audit.

Surface evaluation (`0x82617910`) interpolates frequency, amplitude and offset
between each record's two endpoints, then keeps the maximum of each component
across wheels. The wave sampler (`0x8260E0D0`) retains signed samples and the
console's single-subtraction phase wrapping.

Normal slip below -0.75 gives 1.0; positive slip above grip maps to 0..0.5.
Skid maps up/down around its threshold, raises that level to
`(1-load)*2`, then multiplies by load. Drift tyre slip uses the separate
0.3 breakpoint; drift skid uses its own angle curve and attribute key.
No MW front-wheel quarter-strength damping is applied to UC skid input.

Indexed bumps use strict `signal > FFB_BUMP_THRESHOLD` and amplitude
`clamp(FFB_BUMP_GAIN * signal)`. Wheels 0/2 select left and 1/3 right.
A later wheel replaces the side's ramp even if weaker. Duration is 0.3 s,
floor 0.1. Slot 18 does not synthesize another road-roll wave.

Controller Run (`0x8260F6F0`) advances all ramps, but shift-potential,
engine-blown and shifting samples are discarded. Body output takes maxima of
road, skid, slip, collision, drift, NOS and indexed bumps. NOS is left only.
Push (`0x8260FBC0`) precedes Run, retaining the one-poll body-output delay.
Impulse triggers remain an optional extension.

## Collisions

The controller branch (`0x82611FF8`) evaluates four collision wave records;
it does not use the separate wheel actuator's 300 ms/50 ms envelope or MW's
scalar collision ramp. BeginUpdate clears collision wave inputs each frame.

The adapter opens the same PC feedback collection as native ReportCollision:
class `0xD93FC53A`, collection `0xEEC2271A`, using `0x00464A20`,
constructing an Attrib::Instance with `0x00462C80`, and releasing it through
`0x00468820`. Missing collections/attributes remain silent.

| Contact | Attribute key | Wave input |
| --- | --- | --- |
| Type 3 | `0x751D40E5` | Contact force |
| Barrier glance | `0x96A192E8` | Live own speed * 2.23699 |
| Barrier direct | `0x7125C7EC` | Own velocity change * 2.23699 |
| Vehicle rear contact | `0xAEF4FB7D` | max(stored A speed, stored B speed) * 0.25 * 2.23699 |
| Moving vehicle contact | `0x1C2333F7` | Live own speed * 2.23699 |
| Vehicle impact (delta >= 5 m/s) | `0x3C714126` | Own velocity change * 2.23699 |

The barrier classifier uses absolute normalized stored velocity projected on
the contact normal, with a 0.1 threshold. Low-delta vehicle rear contacts
require another vehicle, normalized velocity dot > 0.1, and estimated delta
> 5. Other moving contacts require own live speed > 2 and other live velocity
relative to own stored velocity > 2. Both player A/B orientations are supported.
PC stores the contact category in the low three bits; Xbox uses bits 29..31.
The by-value velocity-change argument is retained for ABI compatibility;
the console evaluator itself recomputes participant velocity changes.

## Validation and limits

`tests\uc.cmd` checks literal vtable calls with runtime stack validation,
collection reference balancing, curve boundary vectors, stereo drift, indexed
bumps, collision keys/inputs, pause behavior and captured XInput output.
It also checks 100 consecutive producer/poll frames at changing intervals,
including phase resets and the one-poll delay. Another 64 synthetic state
vectors were evaluated through the retail PowerPC mixer and compared against
the adapter within two motor units out of 65535. This is a scalar reference
comparison; its sine call uses host math rather than emulating the Xbox CRT.
`python tools/uc_console_audit.py <path-to-default.exe>` verifies the reference
hash, vtable mapping, key constants and representative instructions directly
from the console image without requiring a disassembler.

PC attribute records remain the source of authored wave data. A direct audit
of the local `GLOBAL/ATTRIBUTES.BIN` banks resolves inheritance and compares
119 wave records across 82 surface/feedback collections: all match after
endian conversion. Run it with:

```bat
python tools\uc_attribute_audit.py <PC-ATTRIBUTES.BIN> <X360-ATTRIBUTES.BIN>
```

PC bank SHA-256: `adeb6c25fbdf290dad6c117a021fdce043b7faaafafdbcb2858767451c5f3980`.
Xbox bank SHA-256: `ab11b15bccc764c003b2beb793a2010ec369feb05ffb8e47a7c69c48513d1ddc`.
Other banks or modified game data have not been compared.

The Xbox clock is also elapsed real time, computed during port 0 polling
(`0x826101BC..0x82610200`), rather than a fixed 30 Hz feedback scheduler.
The adapter therefore retains its elapsed polling clock; it does not add an
invented fixed cadence. Frame/poll scheduling can still differ by platform.
Physical controller trace equivalence has not been verified.

Validation for this revision passed all seven build targets (`UG`, `UG2`,
`MW`, `CB`, `UC`, `PS`, `W`), the UC/MW/Carbon/ProStreet feedback suites,
and shared runtime checks for those four feedback targets. Reproduce the
UC-specific checks from an x86 developer prompt at the repository root:

```bat
build.cmd UC
tests\uc.cmd
tests\runtime.cmd UC
```

The normal PC producer, profile-gate bypass, output watchdog and optional
trigger routing remain in use.
