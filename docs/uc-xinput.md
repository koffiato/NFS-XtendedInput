# Undercover PC XInput and feedback research

Reference: `nfs.exe`, 10,584,064 bytes, PE32 x86.
SHA-256: `d1ecded04cbba387c99299d2159b29b5c04b1d095293013caa4b404a43c7d2e9`.
Addresses below apply to this local binary; no retail file is included here.

## Observed PC paths

The recorded `XINPUT1_3.dll` import table contains:

| Ordinal | Function | IAT address | Stub |
| ---: | --- | --- | --- |
| 4 | XInputGetCapabilities | `0x00BD66A0` | `0x008AE17C` |
| 5 | XInputEnable | `0x00BD66A4` | `0x008AE182` |
| 2 | XInputGetState | `0x00BD66A8` | `0x008AE176` |

There is no static XInputSetState import in that table. Import absence alone
does not prove that no dynamic or alternative output path can exist.

The Logitech wrapper table at `0x00C2087C` includes polling at
`0x007573E0`, capabilities at `0x00757490`, and a candidate actuator slot
7 at `0x006B1200` that is exactly `ret 4`. Slot 8 also points to
`0x00758960`; port-set/get methods follow at slots 9 and 10
(`0x007574F0` and `0x00757500`).

The feedback table at `0x00C0CFC4` contains destructor `0x007B8560`, then 20
entries pointing to `0x008AE240`. That thunk jumps through IAT `0x00BD6260`,
the `_purecall` import.
This is an abstract interface table, not a recovered concrete rumble engine.
It does not prove that a live device dispatches through those entries.

Interface-registration candidates are `IFeedback::_IHandle` at
`0x00676D00`, registration at device `+0x2C`, list-add at `0x005F6A60`,
and steering-wheel handle `0x00666E20`. Trace the concrete device and its
producer before treating those candidates as adapter bindings.

## Implications

These inspected paths provide no native PC rumble implementation to reuse.
They do not establish the Xbox 360 effect model or an ABI identical to MW,
Carbon or ProStreet. Those games have different feedback layouts and need
separate investigation.

Xbox curves, motor routing, timing and lifecycle behavior remain unverified.
