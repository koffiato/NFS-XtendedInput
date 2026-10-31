# Underground 2 rumble research

Research for the local PC US 1.2 executable and PS2 USA/NTSC build, checked on
2026-10-31. The PC controller event engine is present and executable; its final
motor writer is an empty stub. All 12 initialized event records match PS2,
including curve points, thresholds, gains, priorities and enabled flags.

The lightweight native-output adapter is planned and has not been implemented.
The current plugin still uses its own playback and mixer.

## Current plugin configuration

These describe the existing adapters for UG/UG2/MW/PS, not the planned native
UG2 output adapter. Edit `[Rumble]` in the main INI beside the ASI.

| Setting | Default | Meaning |
| --- | --- | --- |
| `Enabled` | 1; supplied UG INI: 0 | Master switch |
| `Intensity` | 1.0 | Overall strength, 0-2 |
| `LeftMotorScale` / `RightMotorScale` | 1.0 | Heavy/light strength, 0-2 |
| `SwapMotors` | 0 | Swap main motors |
| `Impacts` / `Surface` / `Tyres` / `Car` | 1.0 | Category strength, 0-2 |
| `GearShift` | 1 | 0 off; positive values enable event playback; old preset values are accepted |
| `ImpulseTriggers` | 2 | 0 off, 1 broadcast, 2 automatic |
| `TriggerIntensity` | 1.0 | Trigger strength, 0-2 |
| `TriggerPedalScaling` | 1 | Scale trigger effects with pedal input |
| `Telemetry` | 0 | Diagnostic log beside the plugin |

UG/UG2 use native curve data with simplified playback and heavy-motor routing.
The custom presets, shift-quality cues, fatigue and ducking have been removed.
MW/PS retain live surface waves, console envelopes and temporary MW fallbacks.
These are not recovered ProStreet Xbox 360 curves.

Impulse triggers use Windows.Gaming.Input on compatible Xbox One/Series pads.
UG2 automatic mode additionally requires a single XInput pad on port 0;
broadcast mode targets all detected WGI pads. The current trigger mapping is
provisional and is not console behavior. Pause/focus/disconnect stops have
mocked coverage.

From an x86 Visual Studio developer prompt at the repository root:

```bat
build.cmd UG2
tests\runtime.cmd UG2
tests\input.cmd
tests\ug2.cmd "path\to\SPEED2.EXE"
```

The ASI is `build/UG2/NFSU_XtendedInput.asi`. The UG2 harness maps local US 1.2
signatures and data without launching the game; output and callbacks are mocked.
The private native-code research probe is separate from that harness. To package
an existing build, provide the full source commit SHA:

```bat
python scripts/release.py package --game UG2 --repository koffiato/NFS-XtendedInput --commit <40-character-commit-sha>
```

## PC controller path: effects exist, motor output is stubbed

The inspected PC executable contains this complete controller-side sequence:

| Stage | PC US 1.2 address | Verified behavior |
| --- | --- | --- |
| Event producers | `0x005F6350`; collision call at `0x005A9A31` | Feed scalar driving/collision events into the dispatcher, subject to settings/state guards |
| Event dispatcher | `0x005727B0` | Selects one of 12 records; road event 1 adjusts its first point |
| Generic dispatch | `0x005726D0` | Checks enable/state/input/suppression; computes gain and attempts admission |
| Slot admission | `0x00572390` | Stores descriptor, age, gain and priority in one of three slots |
| Curve sampler | `0x005722A0` | Interpolates authored points |
| Channel update | `0x00572470` | Samples effects, chooses/clamps the integer maximum, advances ages by `dt / gain` |
| Frame update | `0x00572610` | Updates two channels per port and calls the output seam when a level changes |
| Motor-output seam | `0x004022C0` | A single `RET`; performs no controller/device operation |
| Stop request | `0x005726A0` | Calls the same output seam with zeroes |
| Suppression update | `0x00572520` | Maintains the native sustained-event timers |

Channel 0 is at `0x008653B0 + port * 0x60`; channel 1 follows at `+0x30`.
The event table is at `0x007FB7D8`. The frame routine passes
`(port, channel0.output, channel0.output)` to the empty writer; it does not
supply separately mixed heavy/light values. The PC device-capability helper
at `0x00570A60` simply returns true. Neither stub creates XInput vibration or
DirectInput effects for this controller path.

The frame call at `0x005814AD` remains in the executable. Settings matter:
the producer reads a hidden profile vibration byte (`0x005F6388`, tested at
`0x005F6395`), and collision production queries it through `0x005EB420`.
The current plugin bypasses these two false-setting guards. A final-output
hook alone does not guarantee events will be produced with every profile.
Full event-producer parity with PS2 and stock gameplay activation remain
unverified.

### Native-code execution check

A private x86 probe mapped the supported PE into its own process, without
running the game entry point or injecting into a game. It called the original
PC dispatcher and frame routines with synthetic valid state:

- Event 7, input 1, created a native slot with the original descriptor, gain 3
  and age 0. The native frame produced level 255 and advanced age to 0.075 for
  `dt = 0.225`; another sample at zero delta produced level 60.
- A capture callback installed **only in that private image** at the output
  seam received `(port 1, 255, 255)`. Unchanged output did not resend; the native
  stop request delivered `(port 1, 0, 0)`.
- Global disable, disabled event 4 and the three-slot equal-gain rejection
  rules remained effective.

This executes the PC's actual feedback instructions rather than substituting
our plugin's scheduler. It proves the existing engine can create, advance and
forward effects under those conditions. It does not prove which events occur
in a real stock race, physical motor output, or complete pause/reconnect behavior.
No game was running for this check and no hardware output was requested.
The probe, logs and disassembly remain outside the repository; no game binary
is shipped.

### Steering-wheel path

PC also has a separate DirectInput force-feedback implementation. GUID
references and COM calls identify effect creation (`0x005C0200`, vtable
`+0x48`), parameter updates (`0x005C0390`, `+0x18`) and effect start
(`0x005BFEE0`, effect vtable `+0x1C`). Capture-only COM interfaces confirmed
those calls without opening a device. This is wheel support, outside the
controller-rumble work. Its existence neither repairs the empty controller
writer nor invalidates the preserved console-style controller data. Further
wheel investigation is out of scope.

## Inputs

| Input | Identification |
| --- | --- |
| PS2 boot executable | `SLUS_210.65;1`, boot serial SLUS-21065, USA/NTSC |
| PS2 executable length | 4,697,728 bytes |
| PS2 executable SHA-256 | `a666f3e8869e63ad2f5eff90fc30c4953fcce41166ff006aaf1ed327d180d8ff` |
| PC executable | US 1.2 `SPEED2.EXE`, 4,800,512 bytes |
| PC executable SHA-256 | `e17c8f060bae797beaf61395c6e2f1a23bcde8c4666a412c92274aa411066214` |

The PS2 executable was read directly from the ISO in memory. No emulator was
started, no game image or emulator setting was modified, and no game binary
was copied into the repository.

## PS2 table decoding

The PS2 event table at virtual address `0x00523070` is initialized by code;
reading its uninitialized bytes directly from the ELF gives zeroes.
The initialization block `[0x002F2EA8, 0x002F3230)` builds the curve descriptors
and 12 event records.

The block was evaluated in a private in-memory model of the registers and
byte stores. The ELF `.reginfo` supplies `$gp = 0x005737F0`. All instructions
in that straight-line block were decoded; none were skipped or treated as
unknown. This is static reconstruction, not a live gameplay capture.

Each event occupies 16 bytes:

- `+0x00`: 32-bit event ID.
- `+0x04`: priority byte; `+0x05`: enabled byte.
- `+0x08`: pointer to an eight-byte curve descriptor: point pointer, point count.
- `+0x0C`: parameter pointer.

Each curve point consists of two 32-bit floats: time and native level.
The four standard parameters are input minimum/maximum and amplitude gain
minimum/maximum. Road event 1 has two more floats for its initial-value range.

For this ELF's loadable segment, `file offset = virtual address - 0x000FF000`.
The ISO directory places the boot ELF at logical block 101028 (2048 bytes per
block); use the filesystem directory to locate it on other images rather than
assuming that ISO offset is universal.

Comparison used the PC event table at `0x007FB7D8` with its pointer targets
resolved through the PE section table.

## PS2 dispatch and admission

`0x002E89D8` selects an event by ID and calls `0x002E8888`. The latter:

- Rejects events while the global enable at `0x005721E4` is zero, or the event's
  enabled byte is zero.
- When the state at `0x005216F4` is 6, requires the state reached through
  `0x005738F0` to be 4 or 6. State 3 at `0x005216F4` also prevents scheduling.
  These are numeric guards; their complete menu/pause meanings are not traced.
- Rejects `input < inputMin`; equality is accepted. Caps input at `inputMax`
  and computes `gain = gainMin + (gainMax-gainMin) *
  (input-inputMin)/(inputMax-inputMin)`.
- Rejects an event/port whose suppression timer is negative (see below).
- Passes the curve descriptor, gain and signed priority byte to `0x002E8388`.
  Only a successful admission records the current timestamp for that event/port.

Road event 1 has an additional step **before** these checks: the dispatcher
selects a separate curve descriptor for port 0 (`0x005723D8`) or port 1
(`0x005723E0`) and writes its first point's level using the raw, unclamped
input: `25 + 60 * (input-0.001)/(0.025-0.001)`. An input of 0.05 therefore
writes approximately 147.5, while the generic gain is capped at 1.5. Active
slots hold the descriptor pointer, so later road calls can change the first
point of a curve already playing; the native scheduler does not copy points.

## PS2 three-slot scheduler

Two ports have state blocks at `0x005231C0 + port * 0x60`. Each block contains
two 0x30-byte channel states. Initialization at `0x002F3268..0x002F3310` clears
both channels. The normal event dispatcher submits to channel 0 only.

| Offset in a channel | Contents |
| --- | --- |
| `+0x00` | Three curve-descriptor pointers; null means free |
| `+0x0C` | Three floating-point curve ages |
| `+0x18` | Three gains; each also divides the age step |
| `+0x24` | Three signed priority bytes |
| `+0x28` | Current integer output |
| `+0x2C` | Previous integer output |

`0x002E8388` scans slots 0 through 2:

1. Skip a slot whose priority is greater than the incoming priority.
2. Take the first eligible free slot immediately.
3. An occupied slot is eligible if the incoming priority is greater, or if
   priorities are equal and the incoming gain is **strictly greater** than the
   stored gain.
4. Among eligible occupied slots, prefer the lowest priority; at equal priority,
   prefer the lowest current integer sample. A sample tie selects the later slot.
5. If no slot qualifies, reject the event. Otherwise store the descriptor,
   gain and priority, and reset that slot's age to zero.

There is no event-ID deduplication or special pulse/texture classification in
this scheduler. Repeated equal-gain calls can occupy all three free slots.
Once full, another equal-priority, equal-gain call is rejected even if an old
effect's instantaneous level has fallen. Priority controls replacement; it does
not silence lower-priority effects already admitted.

## PS2 sampling and timing

The curve helpers at `0x002E81B0`, `0x002E8210`, `0x002E8250` and `0x002E82A8`
select a segment, reject ages before the first point or at/after the final
point, and linearly interpolate the point values. `0x002E8330` multiplies the
sample by its slot gain and converts it to a signed integer with `CVT.W.S`.
For ordinary finite positive values this truncates the fraction: peak 255
with gain 0.3 becomes 76, not 77.

`0x002E8510` updates one channel as follows:

```text
previous = output
output = 0
for each occupied slot:
    if age is outside the curve: free slot; priority = -1
    else:
        output = max(output, integer(curve(age) * gain))
        age += dt / gain
output = clamp(output, 0, 255)
return output != previous
```

Sampling happens **before** age advancement. Gain changes duration as well as
amplitude; it is not just an intensity multiplier. For event 7 at input 1,
gain is 3: its point at age 0.075 occurs after approximately 0.225 seconds,
and the endpoint at age 0.2 after approximately 0.6 seconds. The corresponding
integer levels are initially 255 after clamping, then 60 at age 0.075, then 0
at the endpoint. Frame-step alignment can shift the observed transition tick.

The frame routine `0x002E8748` reads `dt` from `0x005725C0`. Its producer uses
3600-unit-per-second simulation ticks (`0x002F1918`, stored at `0x002F1A64`);
this is a game clock, not a separately measured wall-clock delta.

## PS2 motor command path

`0x002E8748` updates both channel states for each port. If either output
changed and controller type is `0x73` or `0x79` (`0x00333508`), it calls
`0x00333530`. The two arguments are both loaded from **channel 0's** `+0x28`;
channel 1's output is not forwarded by this path.

For channel-0 level `L`, the writer builds this six-byte actuator packet at
`0x0052BFE8 + port * 6` and marks the port pending at `0x00573360 + port * 4`:

```text
[L > 128 ? 1 : 0, L, 0, 0, 0, 0]
```

Thus 128 keeps the small motor off; 129 turns it on. The large motor receives
an eight-bit level. This is neither a weighted heavy/light split nor two
independently scaled copies. Controller setup at `0x003330E4..0x0033310C`
sets actuator alignment to `[0, 1, 255, 255, 255, 255]`; these slots mean the
binary small motor followed by the variable-speed large motor, as described
by [PS2SDK's libpad interface](https://github.com/ps2dev/ps2sdk/blob/master/ee/rpc/pad/include/libpad.h).

The pending-packet flush at `0x00332A98` calls the libpad actuator routine
`0x0042CCE8` with port, slot 0 and the packet, then clears the pending flag.
An explicit stop path at `0x002E8830` queues zeroes and invokes that flush
immediately. That stop routine does not clear the scheduler slots. Complete
pause, reconnect and packet-delivery behavior still needs runtime tracing.

## PS2 sustained-event suppression

`0x002E8628`, called after the channel updates, maintains a timer and last
accepted timestamp for each of 12 event IDs on both ports. Their arrays are
at `0x005236D8` and `0x00523130`, respectively; each event occupies eight bytes,
with the port selecting the first or second four-byte entry.

A nonnegative timer accumulates `dt` while its accepted timestamp is nonzero,
not `0x7FFFFFFF`, and less than 0.25 seconds old. Timestamp age uses
`(clock - timestamp) * 0.00025`, with the clock at `0x005725B0`. Reaching 10
seconds sets the timer to -4. While negative, new dispatches are rejected;
the timer recovers toward zero by adding `dt`, and its timestamp is cleared.
Otherwise a stale/missing timestamp clears and the timer decays toward zero
by subtracting `dt`. Exactly 0.25 seconds is stale.

This is a per-event admission lockout, not a gradual reduction in motor
strength. Already admitted slots continue through the normal sampler.

## Shared event records

PS2 event address is `0x00523070 + 16 * ID`. Semantic event names are omitted
because this check compares the tables; it does not independently trace every
PS2 producer.

| ID | Priority | Enabled | Curve points address | Parameters address | Input range | Gain range |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | 8 | 1 | `0x522f10` | `0x487190` | 2 to 11 | 0.3 to 2 |
| 1 | 6 | 1 | `0x522f68` | `0x487168` | 0.001 to 0.025 | 1.1 to 1.5 |
| 2 | 5 | 1 | `0x522f50` | `0x487148` | 9 to 40 | 0.95 to 1.5 |
| 3 | 9 | 1 | `0x522f10` | `0x487180` | 1 to 3 | 1 to 2 |
| 4 | 5 | 0 | `0x522fa8` | `0x4871a0` | 0 to 1 | 0.3 to 2 |
| 5 | 5 | 0 | `0x522fb8` | `0x4871a0` | 0 to 1 | 0.3 to 2 |
| 6 | 5 | 1 | `0x523018` | `0x4871d8` | 0 to 1 | 1.5 to 3 |
| 7 | 7 | 1 | `0x523000` | `0x4871c8` | 0 to 1 | 0.3 to 3 |
| 8 | 10 | 1 | `0x523038` | `0x4871f8` | 0 to 1 | 1 to 1.9 |
| 9 | 7 | 1 | `0x523028` | `0x4871e8` | 0.3 to 0.9 | 1.2 to 2.75 |
| 10 | 6 | 1 | `0x523048` | `0x487158` | 5 to 15 | 2 to 2.5 |
| 11 | 6 | 1 | `0x523058` | `0x487208` | 1 to 10 | 1.15 to 1.9 |

Road event 1's extra range is 25 to 85. Events 4 and 5 are disabled in both
builds; the presence of their curves does not mean they should be enabled.

## Shared authored curves

Times are authored curve-age coordinates, not fixed playback durations: the
native age advances by `dt / gain`. Levels are native values, not percentages. Decimal display is rounded, while the
comparison used the original 32-bit representations.

| Event ID | (time, level) points |
| --- | --- |
| 0 | (0, 255), (0.05, 200), (0.07, 80), (0.8, 0) |
| 1 | (0, 80), (0.05, 15), (0.1, 0) |
| 2 | (0, 70), (0.05, 15), (0.1, 0) |
| 3 | (0, 255), (0.05, 200), (0.07, 80), (0.8, 0) |
| 4 | (0, 140), (0.07, 0) |
| 5 | (0, 255), (0.05, 0), (0.07, 255), (0.15, 0) |
| 6 | (0, 45), (0.25, 35) |
| 7 | (0, 255), (0.075, 20), (0.2, 0) |
| 8 | (0, 70), (1, 1) |
| 9 | (0, 45), (0.01, 1) |
| 10 | (0, 45), (0.01, 0) |
| 11 | (0, 55), (0.01, 15), (0.05, 0) |

Events 0 and 3 share the same curve descriptor at `0x005723C0`.
The other descriptors are initialized at GP-relative addresses and point to
the arrays listed above.

## Verification and adapter differences

A bounded, private instruction model evaluated the initializer and the above
rumble routines directly from the local ELF, including branch delay slots and
branch-likely annulment. No instructions encountered in those checks were
skipped. Checks covered 2,000 deterministic slot-admission cases against an
independent selection model, 500 sampled-channel cases, both ports' motor
packing at levels 0/1/127/128/129/255, normal frame-to-packet routing, threshold
and mode guards, repeated events, road extrapolation and suppression/recovery.
These are synthetic-state checks, not gameplay observations or a complete EE
emulator. Host float32 arithmetic is not proof of bit-exact EE arithmetic for
all inputs.

EE-specific instruction decoding was checked against PCSX2's
[opcode tables](https://github.com/PCSX2/pcsx2/blob/1517c1f52029346441d6a20f4b8b8c1bfdc2347c/pcsx2/R5900OpcodeTables.cpp),
[signed packed maximum](https://github.com/PCSX2/pcsx2/blob/1517c1f52029346441d6a20f4b8b8c1bfdc2347c/pcsx2/MMI.cpp)
and [float-to-integer conversion](https://github.com/PCSX2/pcsx2/blob/1517c1f52029346441d6a20f4b8b8c1bfdc2347c/pcsx2/FPU.cpp).
Those sources establish instruction behavior, not UG2's game-specific logic.

The current [adapter](../src/rumble/Underground.h) replaces native dispatch,
plays one state per event ID and drives XInput/WGI itself. Calling the original
frame update does not make its own playback native. It shares the authored
data, but still differs from the PS2 path:

| Behavior | Current adapter | Decoded PS2 path |
| --- | --- | --- |
| Active effects | One state per event ID | Three shared slots in channel 0 |
| Admission/retriggering | Pulse/texture rules | Priority, stored gain and slot selection |
| Curve age | `age += dt` | `age += dt / gain` |
| Sample/output | Normalized float, percent-rounded device output | Integer sample, clamped to 0..255 |
| Main motors | Heavy only | Heavy level plus binary small motor above 128 |
| Sustained-event guard | No native lockout | Per-event 10-second accumulator / 4-second lockout |
| Road first point | Clamped normalized input; copied at start | Raw input before gain clamp; live descriptor |

Event 7 uses the matching authored envelope, but that alone does not make the
adapter's `GearShift` behavior authentic. Its PS2 producer calls event 7 with
input 1 at `0x0036B944` while predicate `0x0036CA20` is true. That predicate
checks object kind 1 and a positive float through `object+0x34 -> +0x4C ->
+0x5C`; its gameplay meaning has not been independently established. This
research does not prove an event on every successful gear change.

The three-slot budget is a game software choice, not a fundamental PS2
hardware limit. PS2 already drives both motors together; this game path derives
the two actuator commands from one scalar. The desired modern implementation
should preserve UG2's character without treating every old restriction as a
requirement. For the first lightweight baseline, native scheduling and its
eight-bit output are acceptable; more elaborate mixing is deferred. Trigger
rumble is an extension with no PS2 reference and needs separate restrained tuning.

The existing `tests/ug2.cmd` harness checks the current plugin's signatures,
routing and lifecycle with mocked callbacks/output. Its success is not evidence
that the proposed lightweight adapter is implemented or tested in gameplay.

A PCSX2 gameplay trace remains necessary to confirm real event sequences,
clock/pause transitions, controller initialization, submitted packets and
hardware behavior. No game image, emulator setting or implementation was
changed for this research. Results apply only to the identified USA PS2/PC
builds; other games and revisions remain outside this check.
