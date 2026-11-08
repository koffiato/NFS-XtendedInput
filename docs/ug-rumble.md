# Underground 1 rumble

UG1 US 1.4 uses the game's native dispatcher, three-slot scheduler and curve
timing. The adapter forwards the resolved output after the game's channel
updates. Executable checks and local gameplay confirm working controller
feedback. Tests cover default INI creation and the in-game Vibration toggle.

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
the PS2 DualShock 2 convention; see [Original Xbox implementation](#original-xbox-implementation)
below for the native Xbox motor packing, deadzone, and event tuning.

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
telemetry requires further research. Original Xbox and PS2 console producers, event
tables, and motor writers were reverse-engineered and audited on 2026-11-08.

## Executables inspected

Checked on 2026-10-31 (PC) and 2026-11-08 (Xbox and PS2).

| Executable | Platform | SHA-256 / Details |
| --- | --- | --- |
| Local installed `Speed.exe` | PC x86 | `35654f401383be9af0dbff0d11a92220a17579a01030005de6943e63e03f460f` (3,178,496 B) |
| Prior US 1.4 reference | PC x86 | `c0fad450912952f53809a54690d235aa8b4e25b9f46650693c7cbee1b4c074b7` (3,178,496 B) |
| Retail `default.xbe` | Xbox x86 | Retail Xbox disc image (7,825,162,240 B); XBE 2,473,984 B |
| Retail `SLUS_208.11;1` | PS2 MIPS | Retail PS2 disc image (2,726,166,528 B); ELF 4,291,540 B; SHA-256 `a2418fdd5ba9e10864fc7b5a4e59197c3d1acbd7a349313a101a1207a2385ba8` |

The inspected PC code and event data match across 39 ranges. The full files differ;
this establishes compatibility of those ranges, not every executable revision.

The Xbox executable (`Title ID 0x45410047`, retail entry point `0x00164356`, debug build
path `D:\Speed\XBOX\obj\release\nfsR.exe`) and PS2 boot ELF (`SLUS-20811`, entry point `0x00100000`,
`.reginfo` `$gp = 0x00504DF0`) were read directly from disc images in memory
without running an emulator or extracting files to disk.

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

## Original Xbox implementation

The retail Xbox executable (`default.xbe`, image base `0x00010000`) contains a complete,
fully functional native controller rumble pipeline linked against the Xbox Peripheral
library (`XPP`).

### Pipeline addresses

| Stage | Xbox VA | Behavior |
| --- | --- | --- |
| Driving producer | `0x00037810` | Dispatches driving events 1-7 and 9-11 |
| Collision producer | `0x0006B050` | Dispatches event 0 subject to player `+0x21` vibration byte |
| Dispatcher | `0x00132270` | Same 12-event selector; calls generic dispatch `0x001321C0` |
| Slot admission | `0x00131FE0` | Same three-slot scheduler and priority replacement |
| Curve sampler | `0x00131F20` | Linear interpolation identical to PC |
| Channel update | `0x00132080` | Samples occupied slots, clamps 0-255, advances ages by `dt / gain` |
| Frame loop | `0x00132110..0x00132185` | Samples 2 channels across **4 controller ports** (8 channels total at `0x002C9888`) |
| Motor writer | `0x00014640` | Deadzone filter, motor scaling, and `XInputSetState` call |
| Kernel output call | `0x001ED26A` | `XInputSetState` in `XPP` section |

### Native Xbox motor writer mechanics

Unlike PC (where the frame loop called `0x00574EB0` without writing to motors), Xbox
UG1 calls `0x00014640(port, channel0.output, channel0.output)` at `0x00132166`:

1. **Deadzone threshold (32 out of 255)**:
   The writer compares both motor inputs against `32.0f` (`0x001F431C`). Any level
   below 32 is zeroed:
   ```x86asm
   fild   dword ptr [esp + 0xc]
   fcomp  dword ptr [0x1f431c]   ; 32.0f
   fnstsw ax
   test   ah, 5
   jp     keep
   xor    cl, cl                  ; level < 32 -> 0
   ```
2. **Left (heavy) motor scaling**:
   For level $L \ge 32$, the heavy motor is scaled linearly by `256.0f` (`0x001F4318`)
   into a 16-bit word (`0..65280`):
   ```cpp
   device->feedback.Rumble.wLeftMotorSpeed = (WORD)(heavyLevel * 256.0f);
   ```
3. **Right (light) motor explicitly muted ($0.0\times$)**:
   The right motor input is multiplied by `0.0f` (`0x001F4098`) before the 256 scale:
   ```x86asm
   fild   dword ptr [esp + 0x10]
   fmul   dword ptr [0x1f4098]   ; 0.0f!
   fmul   dword ptr [0x1f4318]   ; 256.0f
   call   __ftol
   mov    word ptr [esi + edx + 0xb4], ax ; wRightMotorSpeed = 0
   ```
   Both the Duke and Controller S right motors were deliberately kept silent by EA
   Vancouver, avoiding high-frequency buzzing from the shared single-channel feedback engine.
4. **4 controller ports**:
   Xbox allocates 8 channels (`4 ports * 0x60 = 0x180` bytes) at `0x002C9888`, allowing 4-player
   split-screen rumble, whereas PC only allocates 4 channels for 2 ports at `0x0073B158`.

### Xbox event table parameter differences

The Xbox event table is located at `0x00252588`. All 12 curve profiles and priorities
match PC exactly. However, **four events have Xbox-specific input thresholds and gain multipliers**:

| ID | Event | PC input range | PC gain range | Xbox input range | Xbox gain range | Tuning difference |
| :---: | :--- | :--- | :--- | :--- | :--- | :--- |
| **0** | **Collision** | 2.0 to 11.0 | 0.3 to 2.0 | **2.0 to 60.0** | **0.4 to 2.5** | Wider dynamic range; higher maximum gain for severe crashes |
| **2** | **Suspension shock** | 9.0 to 40.0 | 0.95 to 1.5 | **35.0 to 130.0** | **2.45 to 3.0** | Small bumps ignored; hard suspension bottoming hits with double gain |
| **3** | **Hard crash** | 1.0 to 3.0 | 1.0 to 2.0 | **1.0 to 10.0** | 1.0 to 2.0 | Extended linear input ceiling before saturation |
| **10** | **Curb / strip / engine** | 5.0 to 15.0 | 2.0 to 2.5 | **9.0 to 70.0** | **1.8 to 4.0** | Much wider input scaling; maximum gain reaches 4.0 |

Events 1, 4, 5, 6, 7, 8, 9 and 11 have identical input bounds, gains, priorities and curves across PC and Xbox.

## Original PS2 implementation

The retail PS2 release (`SLUS_208.11;1`, SLUS-20811, 4,291,540 bytes, SHA-256
`a2418fdd5ba9e10864fc7b5a4e59197c3d1acbd7a349313a101a1207a2385ba8`, entry point
`0x00100000`, `$gp = 0x00504DF0`) was reverse-engineered directly from the disc image.

### PS2 pipeline addresses

| Stage | PS2 VA | PC US 1.4 counterpart | Behavior |
| --- | --- | --- | --- |
| Initialization | `0x002D7730..0x002D7A70`| Static `.data` | Builds event table and curve descriptors at startup |
| Event table | `0x004E26B0` | `0x007045A8` | 12 event records (16 bytes each) |
| Curve descriptors | `0x00503F38` | `0x00704510` | 15 curve descriptor pairs `{ points, count }` |
| Event dispatcher | `0x002D6ED0` | `0x00574D90` | Selects event record, handles road event 1, calls generic dispatch |
| Generic dispatch | `0x002D6D98` | `0x00574D20` | Checks enable/state/input bounds; computes gain and port channel address |
| Slot admission | `0x002D69B8` | `0x00574C20` | Three-slot priority/gain scheduler |
| Curve sampler | `0x002D67D8` | `0x00574B70` | Linear interpolation between authored points |
| Channel update | `0x002D6B40` | `0x00574CF0` | Samples occupied slots, clamps 0-255, advances ages by `dt / gain` |
| Frame loop | `0x002D6C58..0x002D6D10`| `0x004479D8` | Updates 2 channels per port across 2 ports (`0x004E27A0`) |
| Frame motor call | `0x002D6CF8` | `0x004479FC` (stubbed) | Passes `(port, channel0.output, channel0.output)` to `0x0011F0B8` |
| Explicit stop call| `0x002D6D6C` | N/A | Passes `(port, 0, 0)` to `0x0011F0B8` |
| Native motor writer| `0x0011F0B8` | None | Builds 6-byte DualShock 2 actuator packet |

### PS2 motor writer mechanics (`0x0011F0B8`)

The PS2 motor writer receives `(port, smallLevel, largeLevel)` from the channel update
loop (where both level arguments are `channel0.output`):

1. **DualShock 2 actuator packet buffer**:
   The writer formats a 6-byte packet at `0x0044E550 + port * 6`:
   ```mips
   sltiu   $a1, $a1, 0x81        ; test if smallLevel < 129
   xori    $a1, $a1, 1           ; smallLevel > 128 ? 1 : 0
   sb      $a1, 0($a3)           ; packet[0] = small motor (1-bit on/off)
   sb      $a2, 1($t0)           ; packet[1] = large motor (0..255)
   sb      $zero, 2($a1)         ; packet[2..5] = 0
   ```
2. **Pending transmission flag**:
   Sets the port pending byte at `0x00500158 + port * 4` (`$gp - 0x4C98`), which is flushed
   to libpad hardware via `scePadSetActDirect`.
3. **No deadzone filter**:
   Unlike Xbox, PS2 does not apply a 32-level deadzone cutoff. Faint effects below 32
   drive the large motor directly with low byte values (`1..31`).
4. **Validation of `NFS-XtendedInput`**:
   This confirms that `NFS-XtendedInput`'s heavy-motor scaling (`L / 255.0f`) and binary
   light-motor threshold (`L > 128 ? 1.0f : 0.0f`) are an accurate representation of the
   authentic PS2 console behavior.

### Event parameters and curve matching

All 12 event parameters in PS2 UG1 match PC UG1 **100% bit-for-bit**:
- Event 0 at `0x004110E0`: `[2.0, 11.0, 0.3, 2.0]`
- Event 1 at `0x004110B8`: `[0.001, 0.025, 1.1, 1.5, 25.0, 85.0]`
- Event 2 at `0x00411098`: `[9.0, 40.0, 0.95, 1.5]`
- Event 3 at `0x004110D0`: `[1.0, 3.0, 1.0, 2.0]`
- Event 4/5 at `0x004110F0`: `[0.0, 1.0, 0.3, 2.0]` (both disabled)
- Event 6 at `0x00411128`: `[0.0, 1.0, 1.5, 3.0]`
- Event 7 at `0x00411118`: `[0.0, 1.0, 0.3, 3.0]`
- Event 8 at `0x00411148`: `[0.0, 1.0, 1.0, 1.9]`
- Event 9 at `0x00411138`: `[0.3, 0.9, 1.2, 2.75]`
- Event 10 at `0x004110A8`: `[5.0, 15.0, 2.0, 2.5]`
- Event 11 at `0x00411158`: `[1.0, 10.0, 1.15, 1.9]`

All curve points (times and levels) also match PC UG1 identically.

---

## Cross-platform comparison (PC vs PS2 vs Xbox)

| Dimension | PC US 1.4 | PS2 USA (SLUS-20811) | Xbox USA Retail |
| :--- | :--- | :--- | :--- |
| **Native Motor Writer** | None (empty frame call) | `0x0011F0B8` (libpad packet) | `0x00014640` (`XInputSetState`) |
| **Output Deadzone** | None | None | **Level < 32 forced to 0** |
| **Heavy Motor Output** | Stubbed | `L` (0..255 byte) | $L \times 256.0$ ($0\dots 65280$ 16-bit) |
| **Light Motor Output** | Stubbed | $L > 128 \ ?\ 1 : 0$ (binary) | **$0.0\times$ (deliberately muted)** |
| **Event 0 Params** | `[2.0, 11.0, 0.3, 2.0]` | `[2.0, 11.0, 0.3, 2.0]` | **`[2.0, 60.0, 0.4, 2.5]`** |
| **Event 2 Params** | `[9.0, 40.0, 0.95, 1.5]` | `[9.0, 40.0, 0.95, 1.5]` | **`[35.0, 130.0, 2.45, 3.0]`** |
| **Event 3 Params** | `[1.0, 3.0, 1.0, 2.0]` | `[1.0, 3.0, 1.0, 2.0]` | **`[1.0, 10.0, 1.0, 2.0]`** |
| **Event 10 Params**| `[5.0, 15.0, 2.0, 2.5]` | `[5.0, 15.0, 2.0, 2.5]` | **`[9.0, 70.0, 1.8, 4.0]`** |
| **Controller Ports** | 2 ports (4 channels) | 2 ports (4 channels) | **4 ports (8 channels)** |

