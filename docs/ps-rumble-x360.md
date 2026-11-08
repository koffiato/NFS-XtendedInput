# ProStreet controller feedback

The adapter uses the recovered retail Xbox 360 callback transforms, linear ramps,
and motor selection rules with native PC surface attributes. Physical controller
comparison and equivalence of PC/Xbox surface databases remain unverified.

## Reference images

| Local input | SHA-256 |
| --- | --- |
| PC mapped retail `prostreet-live.exe` | `36d7d61702cbda6e00a88f0a317a56c7a73d83eee50b892dc0e16a64ea9d1a1d` |
| Xbox 360 expanded `default.exe`, title `45410822` | `49692cddb8a8f666c4f1acec3f8c7df933298d538acb480f2a1f9d877b6d37bd` |

Xbox virtual addresses map to file offsets by subtracting `0x82000000`.
Executable images and extracted game assets are not distributed with this project.

## Native interface and producer

PC `IFeedback` has 21 slots and is embedded at `InputDevice + 0x2C`; the native
vtable is `0x0098BE6C`. The device getter is slot 9 (`+0x24`, `0x006C7F20`).
The active Xbox feedback table at `0x820B33F0` contains the shared slots 0-19.
PC adds a controller-type query at slot 20 (`0x006AD170`). The Xbox words at
`0x820B3440..0x820B3448` belong to an adjacent object table, not feedback slots
20-22. Constructor `0x825738F8` installs `0x820B3440` as a separate object's table;
its entries `0x82573910`, `0x82573930`, and `0x82573940` are not extra rumble events.
The adapter preserves all 21 entries in the PC interface.

| Slot | Callback | PC argument layout | Xbox address |
| ---: | --- | --- | --- |
| 0 | Destructor | flags | `0x825B2FF8` |
| 1/2/3 | Pause / Resume / Reset | none | `0x825A9DC8` / `0x825A9E10` / `0x825A9E58` |
| 4/5 | Begin / End | none | `0x825A9F18` / `0x825A9F98` |
| 6 | Road roll | front, surface, speed | `0x825A9FA0` |
| 7 | Tyre skid | surface, skid, range, threshold, load | `0x825AA098` |
| 8 | Tyre slip | surface, slip, grip | `0x825AA1E0` |
| 9 | RPM (controller no-op) | three floats | `0x825AA308` |
| 10 | Shift potential | enum | `0x825AA310` |
| 11 | NOS | engaged, level | `0x825AA568` |
| 12 | Engine blown | bool | `0x825AA4B0` |
| 13 | Shifting | bool | `0x825AA620` |
| 14 | Reserved | one word | `0x825AA708` |
| 15 | Drift slip | surface, slip, grip | `0x825AA710` |
| 16 | Drift skid | surface, skid, range, threshold | `0x825AA870` |
| 17 | Steering (controller no-op) | float | `0x825A8CA8` |
| 18 | Indexed bump | wheel, road signal, surface | `0x825AAA18` |
| 19 | Collision | info reference, iamA, 12-byte vector by value | `0x825AAB88` |
| 20 | PC controller-type query | none | No Xbox feedback equivalent |

PC `LocalPlayer::DoFFB` delegates to `0x004D0CF0`; Xbox delegates to
`0x822AE230`. Indexed bump is called first per wheel, followed by slip/skid in
normal physics. Drift physics calls drift skid for rear wheels after the bump.
The adapter preserves the x86 stack layouts, including the 20-byte collision
argument list and the producer's surface-first push sequence for slot 18.

## Slot coverage audit

All PC slots are declared in order; allocation of extra state is not evidence of
additional virtual callbacks or motor contributions.

| Slots | Coverage / native behavior |
| --- | --- |
| 0 | Embedded, non-deleting interface destructor adapter |
| 1-5 | Lifecycle/ABI implemented; live poll cadence has not been matched against a console trace |
| 6 | Road roll implemented; primary DoFFB producer uses slot 18 instead |
| 7-8 | All four skid scalars and both slip scalars preserved and transformed |
| 9 | Native controller no-op; adapter records RPM telemetry only |
| 10-13 | Potential, NOS, blown engine and shifting event/ramp state accounted for |
| 14 | Native no-op retained |
| 15 | Drift slip implemented; no call from the inspected primary DoFFB loop |
| 16 | Rear-wheel drift skid implemented with the native drift key |
| 17 | Native controller steering no-op retained |
| 18 | Indexed bumps, threshold, gain, side routing and replacement accounted for |
| 19 | Collision adapter and by-value argument layout accounted for |
| 20 | PC controller classification query; returns generic controller type 5 |

Xbox allocates 24 wave channels (six groups of four) and twelve ramps. The
adapter represents the three wave groups and ten ramps needed by the recovered
active callbacks. The other two right-hand collision/NOS ramps have no writer in
those callbacks. The final native mixer samples allocated dormant state but
omits its result; enabling it would change the reference behavior.

The separate device-level StartVibration/StopVibration path uses a 500 ms full
motor override outside IFeedback. The PC StartVibration method currently remains
empty. This path is not invoked by the inspected racing feedback producer and
is not a replacement for kerb, grass or sliding feedback. The Xbox wheel-force
backend is gated by device capability/subtype and is separate from ordinary
controller motor output.

The coverage audit has not identified an omitted gameplay callback explaining
the remaining kerb/grass/sliding discrepancy. Isolated callback/mixer comparisons
do not establish end-to-end driving parity. In particular, the earlier T rows
omit the range, threshold, normalized load and grip arguments of the tyre calls.

## Callback transforms

Define `map(x, lo, hi)` as `clamp((x-lo)/(hi-lo), 0, 1)` when
`hi-lo > 0.000001`, otherwise zero (Xbox helper `0x82141608`).

Ordinary slip (`0x825AA1E0`) evaluates the `FFB_SLIP` records at:

- `1` when slip is strictly below `-0.75`;
- `0.5 * map(slip, grip, 2)` when slip is above grip;
- `0` otherwise.

Ordinary skid (`0x825AA098`) computes:

```
level = skid < threshold
    ? map(skid, range, threshold)
    : 1 - map(skid, threshold, range + threshold)
input = pow(level, (1 - load) * 2) * load
```

It evaluates `FFB_SKID` at this input. No MW front-wheel damping or absolute-value
substitution is applied. Drift slip and skid use their separately recovered
piecewise transforms in `src/rumble/ProStreet.h`. Drift skid queries key
`0x35A8EADA`. Both drift callbacks write the normal slip/skid groups at state
`+0x80/+0x40`; they do not write the allocated groups at `+0x100/+0x140`.

Surface wave packets are eight floats:
`{frequencyMin, amplitudeMin, offsetMin, xMin, frequencyMax, amplitudeMax, offsetMax, xMax}`.
The interpolator is `0x825B68F0`. All four attribute indices are queried; absent
or malformed PC records stay silent. Frequency, amplitude, and offset each take
the maximum across wheel callbacks. Channels 0/1 sum for the heavy motor;
channels 2/3 sum for the light motor. Signed samples are retained until the final
maximum selection and motor clamp. The phase advances only when amplitude times
frequency is positive; the console subtracts one on phase wrap.

## Indexed bumps

Slot 18 (`0x825AAA18`) reads **threshold key `0xE289A6B0`** and **gain key
`0x2D1022A0`**. The old `0xE288A6B0` spelling was incorrect.

When `roadSignal > threshold`, it restarts the wheel's side ramp with:

- amplitude `clamp(gain * roadSignal, 0, 1)`;
- floor `0.1`;
- duration and remaining time `0.3` seconds.

Wheels 0/2 write `+0x220` (heavy), wheels 1/3 write `+0x230` (light). Later wheels
replace the same-side ramp even when weaker; there is no strongest-wheel merge.
Vehicle speed and `FFB_ROLL` are not substitutes for these attributes.

## Collision and event state

Xbox collision handling resolves both participants and measures the change from
the local participant's stored pre-contact velocity to its live velocity.
The passed vector is retained for ABI compatibility and is not used for the
controller strength. PC bitfield category is `flags & 7`; the Xbox compiler's
corresponding bitfield uses an arithmetic shift by 29.

| Contact | Strength |
| --- | --- |
| World | `clamp(deltaSpeed / 30)` |
| Ground | `clamp((deltaSpeed - 1) / 3)` |
| Object, delta at least 5 | `clamp((deltaSpeed - 5) / 20)` |
| Aligned vehicle contact with small delta | same object formula, substituting `length(info.objAVel) * 0.25` when above 5 |
| Other small-delta object contact | `clamp((liveSpeed - 20) / 30) * 0.75`, if liveSpeed and the other-live-minus-own-before magnitude both exceed 2 |

Positive strength writes the heavy collision ramp at `+0x180`, with zero floor
and amplitude/duration equal to strength. Active ramps merge each field by max.
There is no contact-normal estimate, separate right scrape texture, or synthetic
landing envelope. Both PC local-listener hooks route participant A and B contacts;
foreign feedback devices retain the native listener.

NOS is a heavy-only ramp from 1 toward 0.5 over two seconds, started on a rising
edge and cleared on a falling edge. Potential, shifting, and engine callbacks
maintain their native event/ramp state. **The retail final mixer samples these
ramps but discards their values.** Their existence does not mean they produce
body rumble. The optional impulse-trigger extension can still use shift state.

## Final mixer and lifecycle

`Run` at `0x825A8CE0` samples six four-channel wave groups and twelve ramps.
Only normal skid/slip channel pairs, collision, NOS, and the two indexed bump
ramps reach the motor maxima. Roll, the other allocated wave groups, shift
potential, engine, and shifting results are discarded in this retail build.

The device update at `0x825A93E0` calls Push before Run, so motors receive the
previous poll's computed output. EndUpdate marks feedback freshness; device
polling advances effects. Begin clears wave inputs while preserving phases and
ramps. Pause silences output and freezes sampling. The PC adapter also stops
output on focus loss, disconnect, missing producer ticks, and DLL unload.

### Recovered durations versus adapter scheduling

The effect durations are read from Xbox constants: indexed bumps 0.3 seconds,
NOS 2 seconds, engine 1 second, shifting 0.5 seconds and shift potential 3 seconds.
Collision duration is the computed strength in seconds. The native sampler accepts
milliseconds and multiplies by 0.001; the adapter supplies seconds directly.
The inspected mixer discards engine, shifting and potential body contributions.

The **250 ms stale-producer watchdog is PC adapter policy**, not a recovered Xbox
effect duration. Focus/disconnect gates also belong to the port's output safety
handling. During a stale/focus gate the adapter sends zero and freezes sampling.
Those cases can differ from the original device lifecycle.

The adapter obtains elapsed wall time with QueryPerformanceCounter and samples
from PC InputDevice::PollDevice. The console obtains its delta in its own device
updater. Push-before-Run order is recovered, but its previous-poll latency in
milliseconds and the schedule relative to feedback production have not been
validated against a live console trace. No fixed 30/60 Hz emulation rate, extra
smoothing or artificial texture delay has been added. Diagnostic logging adds
execution overhead and can perturb the measured live cadence.

The numeric reference comparisons supply explicit inputs and deltas. They verify
effect calculations for those values; they do not verify that PC gameplay
supplies the same event stream or update intervals as Xbox gameplay. Complete
timing parity is therefore not established.

## Validation and remaining limits

```
build.cmd PS
tests\prostreet.cmd [optional-local-PC-mapped-image]
```

The x86 harness checks native producer stack order, all changed callback layouts,
both collision participants, measured collision branches, stereo bump routing and
replacement, strict thresholds, malformed attributes, car-ramp output selection,
Push-before-Run timing, pause/focus loss, and stale feedback.

An optional independent check requires Python with Capstone and a local expanded
Xbox image. It executes the original callback and mixer instructions using a
bounded interpreter, then compares numeric vectors to the compiled adapter:

```
python tests/prostreet_console_reference.py path/to/default.exe
```

The audit passed **2,500 cases**: 1,000 road/regular/drift tyre callback cases,
800 potential/NOS/engine/shifting state cases (including paused calls), 200 indexed
bumps, and 500 complete mixer states. Mixer comparisons allow two units of
16-bit motor rounding. Sine/power use host math; this is not Xbox hardware
emulation or physical controller validation. Surface attributes remain native PC
data; missing channels are not invented, and Xbox asset equality is not claimed.
