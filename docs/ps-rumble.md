# ProStreet feedback adapter (archived research)

These archived findings describe the ProStreet adapter. Console fidelity
remains unverified.

ProStreet shares rumble settings, mixing and XInput/Windows.Gaming.Input
output with Most Wanted, with separate callbacks. The supplied ProStreet INI
enables it. The native interface was checked in the running game. Gameplay
tests at `1b942a4` confirmed lockup, wheelspin and wall impacts with telemetry
enabled. Later unit and mixer changes have harness coverage only.

## Live data and temporary values

The game supplies road, skid, drift-skid, RPM, shift-potential, NOS,
engine-damage and collision callbacks. The adapter additionally reads live
player speed, velocity, pedals, wheel contact/slip/skid, the raw road signal and
transmission state.

Surface waves are read through ProStreet's own `Attrib::Instance` accessor
(`0x52B950`), for `FFB_ROLL`, `FFB_SKID` and `FFB_SLIP`. Missing,
unreadable, non-finite or invalid records are silent. Body tyre feedback uses
these native waves. Signed live slip distinguishes braking and throttle for
the optional proportional trigger extension.

Impacts and landings use the shared MW collision math and legacy envelopes.
NOS, shifts and engine damage reuse MW's car ramps. Scrape strength is a
linear contact-speed estimate. ProStreet Xbox 360 curves remain undecoded.

Compression-derived kerb/bumps and the damaged-tyre surface's separate impact
category remain pending. Ground impacts received through the native collision
callback still produce landing feedback.

## Native interface

Addresses below belong to the fixed-base PC image used by the upstream PS
input adapter. Registration and callback signatures are validated before
exposing rumble. A mismatch disables rumble and emits an OutputDebugString
diagnostic; it does not validate or change upstream input-hook compatibility.

| Item | Address/layout |
| --- | --- |
| Original GameDevice factory | `0x6C9520` |
| Input-device GetInterfaces | Slot 9 / `+0x24`; getter `0x6C7F20` |
| IFeedback in device | `+0x2C`, owner list at interface `+4` |
| IFeedback vtable | `0x98BE6C`, 21 slots |
| IFeedback registration handle | `0x4CD5D0` |
| IList Add / Find | `0x461420` / `0x4614B0` |
| Feedback producer | `0x4D0CF0` |
| Skid | Slot 7: surface reference and four floats |
| Slip | Slot 8: surface reference and two floats |
| Drift skid | Slot 16: surface reference and three floats |
| Steering force | Slot 17, consumed without a controller effect |
| Road | Slot 18: wheel index, native road scalar, surface reference |
| Collision | Slot 19: info reference, bool, 12-byte velocity vector by value |
| Controller type | Slot 20: native PC default |

PS reports road **before** slip/skid, with an explicit wheel index. MW reports
road last. The adapters preserve both orders and argument sizes. Slots whose
semantics have not been verified remain no-ops with the native stack size.

PS surface collection/layout pointers are at `+0/+4`, unlike MW's `+4/+8`.
The vehicle reader uses PS interface handles and accessors, not MW offsets.
Transmission state supplies shift feedback even when the native engine-damage
path omits UpdateShifting. The two local-player collision listener entries at
`0x9FA07C` and `0x9FA324` route through the adapter. Only registered feedback
instances receive these events. The adapter checks local ownership against
both participants before the stock participant-A lookup and velocity/profile
filters. Pause/focus stops still apply. Native steering-wheel feedback keeps
the original handler.

The input device also preserves PS's extra slot before StartVibration and the
native battery-query/noise-return signatures. Its custom remapping method
follows the native slots and remains called through the existing remapping hook.

## Diagnostics and validation

Set `[Rumble] Telemetry = 1` to write `scripts/NFS_XtendedInput_Rumble.log`.
`S` rows record native surface waves; `T` rows include live vehicle/wheel and
callback values; `P` rows record raw local-listener contact participants and
`C` rows record the contacts routed into rumble. PS tick columns identify slip
ratios, skid angles (radians) and the raw native road signal. Surface rows use
`PS-live` instead of MW's layout fields. The log supports simultaneous readers.
Disable telemetry after a short capture to limit log size.

From an x86 Visual Studio developer prompt:

```bat
build.cmd PS
tests\prostreet.cmd
```

The harness mocks controller output and native accessors. It exercises calls
through the actual compiled vtable, including stack cleanup for skid/drift/
collision, live curve sampling, player-list transitions, vehicle accessors,
interface registration/routing, pause, focus loss, missed ticks and malformed
data. An assembly replay follows the native road call's push/getter sequence,
including its native-scalar-before-surface ordering; telemetry is enabled and
written/read during that test. Collision fixtures include local car A/B,
unrelated contacts, pause and original-handler forwarding. Actions runs it
with runtime stack checks. Local validation also ran the signature checks
against a read-only snapshot of the installed running game.
Broader race/controller testing remains pending.

## Gameplay log review (2026-10-31)

The capture at `1b942a4` contains 30,265 complete tick rows
over 271.47 seconds, seven surface definitions, and 104 matched raw/delivered
collision records. One final tick row is incomplete at shutdown and was excluded
from the numeric review. Complete rows contain no faults or non-finite values.
Speed reached 69.43 m/s; pedals stayed within 0..1; NOS and gear changes were recorded.

Two non-sliding wall contacts produced impact strengths 1.00 and 0.05. The
strong hit recorded 51.27 m/s normal closing speed and a measured 50.50 m/s
velocity change. The other 102 contacts were sliding contacts, which feed the
scrape texture rather than starting impact pulses. Every captured contact had
the local car as A; participant-B handling remains covered by the harness.
Tick gaps alone do not prove that the physical motors stopped during pause.

The capture establishes the following input units and operating ranges:

- Live `FFB_SLIP` input spans .05..1 (ratio), and `FFB_SKID` spans .1..1
  (angle), unlike MW's slip/skid speed thresholds. PS uses these native
  ranges while retaining the shared mixer/envelopes.
- Skid angles reached pi at rest. A motion gate prevents stationary skid-wave
  and slide-trigger feedback; wheelspin from a standing burnout remains available.
- Slot `0x7C`/the road callback scalar ranged from 0..1.9. Feeding that raw
  value into `FFB_ROLL` stays below the captured 5..50 input range. The
  adapter samples road waves using the latest live body speed. The raw
  road scalar is logged separately.

Harness checks cover moderate spin/lockup and quiet parked tyres. Gameplay
validation of these settings remains pending.

The raw 6,981,620-byte log stays outside the repository. Capture SHA-256:
`6798d54683f9a27f8f1269783626c9529dfdd75bd65c98004cf89cb8528927be`.

Interface research references: [Xan's feedback investigation](https://github.com/xan1242/NFS-XtendedInput/issues/10),
[NFSPluginSDK ProStreet types](https://github.com/berkayylmao/NFSPluginSDK/tree/master/NFSPluginSDK/Game.ProStreet/Types).
