# ProStreet feedback adapter (archived research)

Development is currently limited to UG2. These earlier ProStreet findings are
retained for reference; no new PS work or console-fidelity claim is implied.

ProStreet now uses the same feedback engine, settings, mixer, envelopes and
XInput/Windows.Gaming.Input output as Most Wanted. The supplied ProStreet INI
enables it. The native interface was checked against the user's installed
running game. User testing at revision `1b942a4` confirmed lockup/wheelspin
feedback and driving with telemetry enabled and wall-impact rumble after the
road/collision fixes. That test predates subsequent unit and mixer changes.
This remains experimental:
broader race/controller coverage and original console fidelity are pending.

## Live data and temporary values

The game supplies road, skid, drift-skid, RPM, shift-potential, NOS,
engine-damage and collision callbacks. The adapter additionally reads live
player speed, velocity, pedals, wheel contact/slip/skid, the raw road signal and
transmission state. It does not infer feedback from controller button presses.

Surface waves are read through ProStreet's own `Attrib::Instance` accessor
(`0x52B950`), for `FFB_ROLL`, `FFB_SKID` and `FFB_SLIP`. Missing,
unreadable, non-finite or invalid records are silent. A valid native PC curve
takes priority; an absent curve is not presented as recovered console data.
Body tyre feedback uses these native waves without synthesized lockup pulses,
wheelspin oscillation or an added skid hiss. Signed live slip distinguishes
braking and throttle for the optional proportional trigger extension.

Impacts and landings use the shared MW collision math and console envelopes.
NOS, shifts and engine damage reuse the MW port's baseline car ramps; the
shortened NOS kick, custom shift presets, perfect/missed-shift cues, fatigue
and impact ducking are removed. Scrape strength remains a linear contact-speed
estimate. These fallbacks are **not ProStreet Xbox 360 curves**. Xbox 360
decoding remains pending. No game files were downloaded,
and no executable, memory snapshot or game data is shipped in this repository.

Compression-derived kerb/bumps and the damaged-tyre surface's separate impact
category remain pending. Ground impacts received through the native collision
callback still produce landing feedback. The adapter routes native local-player
collision listeners before the stock
producer's participant-A lookup and velocity/profile filters. It checks the
local simable handle against both A and B and uses the existing MW impact
math. Other feedback devices keep the original collision producer.

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
instances receive these events; pause/focus stops and contact ownership checks
still apply. Native steering-wheel feedback keeps the original handler.

The input device also preserves PS's extra slot before StartVibration and the
native battery-query/noise-return signatures. Its custom remapping method
follows the native slots and remains called through the existing remapping hook.

## Diagnostics and validation

Set `[Rumble] Telemetry = 1` to write `scripts/NFS_XtendedInput_Rumble.log`.
`S` rows record native surface waves; `T` rows include live vehicle/wheel and
callback values; `P` rows record raw local-listener contact participants and
`C` rows record the contacts routed into rumble. PS tick columns identify slip
ratios, skid angles (radians) and the raw native road signal; that signal was
incorrectly labelled `av` in the first logs. PS surface rows use `PS-live`
instead of interpreting MW's surface-name/layout fields. Disable telemetry
after capturing a short race to avoid an unnecessarily large log. The log now
permits simultaneous readers, so future captures do not require closing the game.

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
unrelated contacts, pause and original-handler forwarding. Actions runs it with runtime stack checks. Local validation also ran the
signature checks against a read-only snapshot of the installed running game.
These checks do not establish in-game stability or original console fidelity.

## Gameplay log review (2026-10-31)

The user confirmed the telemetry crash was gone and wall impacts worked with
code revision `1b942a4`. The local capture contains 30,265 complete tick rows
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

The capture also exposed unit errors in the temporary MW adaptation:

- Live `FFB_SLIP` input spans .05..1 (ratio), and `FFB_SKID` spans .1..1
  (angle), unlike MW's slip/skid speed thresholds. PS now uses these native
  ranges while retaining the shared mixer/envelopes.
- Skid angles reached pi at rest. A motion gate prevents stationary skid-wave
  and slide-trigger feedback; wheelspin from a standing burnout remains available.
- Slot `0x7C`/the road callback scalar ranged from 0..1.9. Feeding that raw
  value into `FFB_ROLL` never reached the captured 5..50 input range. The
  adapter now samples road waves using the latest live body speed. The raw
  scalar remains logged without calling it wheel angular velocity.

A replay of the capture found 139 parked ticks that could falsely qualify as
slides under the previous mapping, and zero of 8,371 applicable wheel samples
reached a road curve's minimum with the old scalar. The updated unit/motion
handling has harness coverage, including moderate spin/lockup and quiet parked
tyres. Its revised feel still needs an in-game retest; the successful user
retest above predates these unit corrections.

The raw 6,981,620-byte log stays outside the repository. Capture SHA-256:
`6798d54683f9a27f8f1269783626c9529dfdd75bd65c98004cf89cb8528927be`.

Interface research references: [Xan's feedback investigation](https://github.com/xan1242/NFS-XtendedInput/issues/10),
[NFSPluginSDK ProStreet types](https://github.com/berkayylmao/NFSPluginSDK/tree/master/NFSPluginSDK/Game.ProStreet/Types).
The bindings were cross-checked with the installed game's live code.
