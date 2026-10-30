# ProStreet feedback adapter

ProStreet now uses the same feedback engine, settings, mixer, envelopes and
XInput/Windows.Gaming.Input output as Most Wanted. The supplied ProStreet INI
enables it. The native interface was checked against the user's installed
running game. User testing confirmed lockup/wheelspin feedback, then confirmed
that the corrected road argument order and collision routing allow driving
with telemetry enabled and wall-impact rumble. This remains experimental:
broader race/controller coverage and original console fidelity are pending.

## Live data and temporary values

The game supplies road, skid, drift-skid, RPM, shift-potential, NOS,
engine-damage and collision callbacks. The adapter additionally reads live
player speed, velocity, pedals, wheel contact/slip/skid/angular velocity and
transmission state. It does not infer feedback from controller button presses.

Surface waves are read through ProStreet's own `Attrib::Instance` accessor
(`0x52B950`), for `FFB_ROLL`, `FFB_SKID` and telemetry's `FFB_SLIP`. Missing,
unreadable, non-finite or invalid records are silent. A valid native PC curve
takes priority; an absent curve is not presented as recovered console data.
The shared MW tyre textures still provide slip/lockup/slide feedback.

Impacts, landings, NOS, shifts, shift cues and tyre texture tuning temporarily
reuse the existing MW implementation. These are **not ProStreet Xbox 360
curves**. Xbox 360 decoding remains pending. No game files were downloaded,
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
| Road | Slot 18: wheel index, angular speed, surface reference |
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
`C` rows record the contacts routed into rumble. PS surface rows use `PS-live`
instead of interpreting MW's surface-name/layout fields. Disable telemetry
after capturing a short race to avoid an unnecessarily large log.

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
including its angular-speed-before-surface ordering; telemetry is enabled and
written/read during that test. Collision fixtures include local car A/B,
unrelated contacts, pause and original-handler forwarding. Actions runs it with runtime stack checks. Local validation also ran the
signature checks against a read-only snapshot of the installed running game.
These checks do not establish in-game stability or original console fidelity.

Interface research references: [Xan's feedback investigation](https://github.com/xan1242/NFS-XtendedInput/issues/10),
[NFSPluginSDK ProStreet types](https://github.com/berkayylmao/NFSPluginSDK/tree/master/NFSPluginSDK/Game.ProStreet/Types).
The bindings were cross-checked with the installed game's live code.
