# Underground 1 rumble

UG1 US 1.4 uses the game's native dispatcher, three-slot scheduler and curve
timing. The adapter forwards the resolved output after the game's channel
updates. Executable checks pass, and the user reported working controller
feedback in local gameplay. Tests cover default INI creation and the in-game
Vibration toggle.

## Configuration

Edit `[Rumble]` in `scripts/NFSU_XtendedInput.ini`.
On first startup, the plugin creates this file beside its ASI if it is missing,
using the bundled UG1 defaults. Existing INIs are preserved.

| Setting | Default | Meaning |
| --- | --- | --- |
| `Enabled` | 1 | Master switch; the in-game Vibration option must also be on |
| `Intensity` | 1.0 | Overall strength, 0-2 |
| `LeftMotorScale` | 1.0 | Heavy-motor strength, 0-2 |
| `RightMotorScale` | 1.0 | Light-motor strength, 0-2 |
| `SwapMotors` | 0 | Swap the two motors |
| `Telemetry` | 0 | Log requested output beside the plugin |

Native level L drives heavy output `L / 255` and light output on when `L > 128`.
Intensity, motor scales and swapping apply afterward. The light threshold follows
UG2; UG1 console motor packing has not been verified.

The in-game Vibration option controls each player's feedback. Switching it off
stops active motors and clears queued effects; switching it on allows new effects.

Impulse triggers are disabled, including with older INIs. Legacy category gains,
`GearShift` and trigger settings have no effect on this adapter.

Focus loss, non-race state, disconnect, zero/invalid frame delta, delta above
0.1 seconds, or more than 0.25 seconds without an update stop output and clear
queued effects. Pause detection relies on the simulation delta becoming zero.

## Build and verification

From an x86 Visual Studio developer prompt at the project root:

```bat
build.cmd UG
tests\runtime.cmd UG
tests\ug.cmd "path\to\Speed.exe"
```

The ASI is `build/UG/NFSU_XtendedInput.asi`. The
[executable harness](../tests/ug_rumble_tests.cpp) runs the original dispatcher,
admission and channel-update instructions in a private mapped image, with mocked
controller output. It does not launch or modify the installed game.

Passing checks cover both ports, native enable/state/input guards, gain-scaled
timing, endpoint expiry, slot replacement, 2,000 admission comparisons, motor
thresholds, scaling/swapping, signature refusal and lifecycle stops. UG1/UG2
builds, shared runtime/input checks and UG2's rumble regression checks also pass.

Further validation: complete gameplay event activation and overlap, pause/resume,
Alt-Tab, restart/quit and unplug/reconnect on a physical controller. UG1 tyre
telemetry and console producer/motor behavior require further research.

## Executables inspected

Checked on 2026-10-31. Both PC files are 3,178,496-byte x86 executables with
image base `0x00400000`.

| Executable | SHA-256 |
| --- | --- |
| Local installed `Speed.exe` | `35654f401383be9af0dbff0d11a92220a17579a01030005de6943e63e03f460f` |
| Prior US 1.4 reference | `c0fad450912952f53809a54690d235aa8b4e25b9f46650693c7cbee1b4c074b7` |

The inspected code and event data match across 39 ranges. The full files differ;
this establishes compatibility of those ranges, not every executable revision.
No UG1 console executable was inspected.

## Native PC pipeline

| Stage | Address | Behavior |
| --- | --- | --- |
| Driving producer | `0x00429270` | References events 1-7 and 9-11 |
| Collision dispatch | `0x00465C5F` | Event 0 |
| Auxiliary dispatch | `0x00431C8A` | Event 8, input 1 |
| Dispatcher | `0x00574D90` | EDX = record pointer, ESI = port, stack argument = float input |
| Slot admission | `0x00574C20` | Three priority/gain-controlled slots |
| Curve sampler | `0x00574B70` | Linear interpolation |
| Channel update | `0x00574CF0` | Maximum integer sample; age advances by dt/gain |
| Frame loop | `0x004479D8..0x004479FC` | Updates two channels on each of two ports |
| Adapter hook | `0x004479FC` | Reads output after sampling; retains original call to `0x00574EB0` |

The inspected frame loop contains no motor writer. `0x00574EB0` updates separate
feedback state; it does not sample the controller channels. The adapter calls it
once and reads channel 0 without advancing curve ages again.

Channel state starts at `0x0073B158 + port * 0x60`; channel 1 follows at `+0x30`.
Each channel contains three descriptor pointers at `+0x00`, ages at `+0x0C`,
gains at `+0x18`, signed priority bytes at `+0x24`, and current/previous integer
outputs at `+0x28`/`+0x2C`. Normal dispatch targets channel 0.

When gameflow at `0x0077A920` is 6, dispatch requires the state through
`0x007361F0` to be 1, 4, 7 or 11. Gameflow 3 rejects events. Dispatch also
requires nonzero global enable at `0x006F1D44` and an enabled event record.
Input below the minimum is rejected; input above the maximum is capped. Gain
is interpolated between the event's bounds. This dispatcher has no UG2-style
sustained-event suppression check.

Admission takes the first eligible free slot. Occupied slots require greater
incoming priority, or equal priority with strictly greater gain. Replacement
prefers the lowest priority, then the lowest current integer sample; sample ties
choose the later slot. Equal-gain repeated calls can fill three free slots, then
are rejected. There is no event-ID deduplication.

Sampling excludes ages before the first point or at/after the final point.
Samples are multiplied by gain and truncated to integers. The channel takes the
maximum, clamps it to 0-255, then advances ages by `dt / gain` using simulation
delta `0x0073AD38`. Gain affects amplitude and duration. For event 7 at input 1,
gain 3 stretches the 0.2-age curve to approximately 0.6 simulation seconds.

The [adapter](../src/rumble/Underground.h) validates hook signatures, sampler-loop
bytes and table structure before patching. It preserves native dispatch and
sampling, including the vibration guards at `0x004292A0`, `0x00465C50` and
`0x00431C7B`. The options handler at `0x004BF0E2..0x004BF0F7` toggles the player
byte reached through `0x007361B4 + port * 4`, at player offset `+0x21`, and
stores it in the profile. The adapter
also checks this byte during frame output and input polling, so disabling
vibration stops effects already playing. Native toggle/profile-store instructions
are exercised by the executable harness for both ports.

## Authored events

Records occupy 16 bytes at `0x007045A8 + ID * 16`: ID, priority byte, enabled
byte, padding, descriptor pointer and parameter pointer. A descriptor contains
a point pointer and count. Values are float32; decimals below are rounded.
Times are curve ages. Gameplay names are omitted where producer semantics
have not been established.

| ID | Priority | Enabled | Input range | Gain range | (age, level) points |
| --- | --- | --- | --- | --- | --- |
| 0 | 8 | 1 | 2 to 11 | 0.3 to 2 | (0, 255), (0.05, 200), (0.07, 80), (0.8, 0) |
| 1 | 6 | 1 | 0.001 to 0.025 | 1.1 to 1.5 | (0, 80), (0.05, 15), (0.1, 0) |
| 2 | 5 | 1 | 9 to 40 | 0.95 to 1.5 | (0, 55), (0.05, 15), (0.1, 0) |
| 3 | 9 | 1 | 1 to 3 | 1 to 2 | (0, 255), (0.05, 200), (0.07, 80), (0.8, 0) |
| 4 | 5 | 0 | 0 to 1 | 0.3 to 2 | (0, 140), (0.07, 0) |
| 5 | 5 | 0 | 0 to 1 | 0.3 to 2 | (0, 255), (0.05, 0), (0.07, 255), (0.15, 0) |
| 6 | 5 | 1 | 0 to 1 | 1.5 to 3 | (0, 45), (0.25, 35) |
| 7 | 7 | 1 | 0 to 1 | 0.3 to 3 | (0, 255), (0.075, 20), (0.2, 0) |
| 8 | 10 | 1 | 0 to 1 | 1 to 1.9 | (0, 70), (1, 1) |
| 9 | 7 | 1 | 0.3 to 0.9 | 1.2 to 2.75 | (0, 45), (0.01, 1) |
| 10 | 6 | 1 | 5 to 15 | 2 to 2.5 | (0, 45), (0.01, 0) |
| 11 | 6 | 1 | 1 to 10 | 1.15 to 1.9 | (0, 55), (0.01, 15), (0.05, 0) |

Events 0 and 3 share a descriptor. Events 4 and 5 are disabled. UG1 event 2
starts at 55; the inspected UG2 event starts at 70. Matching other table values
does not establish matching gameplay producers.

Event 1's producer at `0x004293B1..0x004293EB` selects descriptor `0x00704548`
or `0x00704550` by port and writes its first level as
`25 + 60 * (rawInput - 0.001) / (0.025 - 0.001)` before dispatch clamps the input.
Active slots reference the descriptor, so later calls can change an active curve.
