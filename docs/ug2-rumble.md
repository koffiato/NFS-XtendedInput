# Underground 2 rumble research

The shared adapter reads the native PC event table and applies its own mixer,
including impact ducking, fatigue and trigger effects. UG1 uses different hooks
and wheel data. The PS2 comparison below covers UG2 only.

The UG2 harness in `tests/ug2.cmd` validates hook signatures, curves, routing
and lifecycle with mocked controller output. Gameplay and hardware behavior
still require separate validation.

Verified on 2026-10-31 against the user's local USA disc image.

All 12 initialized PS2 feedback event records match the installed PC US 1.2
records: event IDs, priority bytes, enabled bytes, curve point counts and
contents, input thresholds and amplitude gains. Event 1's two additional
road initial-value parameters also match. Floating-point contents were
compared as bytes; the tables below show readable decimal values.

This establishes PS2 provenance for the native UG2 tables already read by
the adapter. It does not establish identical final motor output.

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

## Decoding

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

## Native dispatcher observations

The PS2 dispatcher at `0x002E89D8` indexes the 16-byte event table and passes
the selected record to `0x002E8888`. That routine checks the enabled byte and
the minimum input, caps the input at the maximum, interpolates the gain and
passes the curve and priority to the scheduler at `0x002E8388`. The curve
reader at `0x002E8210` treats times before the first point and at or after
the final point as outside the curve. These observations are from static
disassembly; scheduler arbitration and actual motor commands remain unverified.

## Event records

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

## Authored curves

Times are the curve time coordinates used by the adapter in seconds; levels
are native values, not percentages. Decimal display is rounded, while the
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

## What this does not verify

The current [adapter](../src/rumble/Underground.h) adapts those native values
through shared category mixing, fatigue, ducking and motor scales. Its
`GearShift` preset replaces the authored event 7 envelope. Xbox impulse-trigger
effects are additional adapter behavior. Identical source tables therefore
do not imply a bit-for-bit reproduction of PS2 controller commands.

A PCSX2 gameplay trace is still needed to verify native priority arbitration,
event retriggering/overlap, time advancement and final per-motor quantization,
including stop behavior. No motor-command trace or hardware comparison was
performed for these findings.

These results apply to the identified USA PS2 and PC executables. They do not
verify other UG2 regions/revisions, Underground 1, or ProStreet's Xbox 360 data.

