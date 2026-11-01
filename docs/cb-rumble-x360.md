# Carbon feedback research

## Reference binaries

| Binary | Size | SHA-256 |
| --- | ---: | --- |
| Local PC `NFSC.exe` | 7,217,152 | `baaaaec33873d4ba7e4dbae6a5212049ffdcf9679a35d865d7625b4189952fd3` |
| Xbox expanded image `default.exe` | 17,629,184 | `41167e1fc7401925b9612d69438b19583e5aa57b962df45386beee8f532becf6` |

The expanded Xbox image uses memory layout: address `VA` is at offset
`VA - 0x82000000`. Its `.text` starts at dump offset `0x110000`. The
inherited raw pointer `0x105E00` describes the
original PE file layout.

## PC interface observations

The table at `0x009E2188` has the following 18 recorded entries. It is
not the MW 15-slot layout. Names below the common prefix still require
verification at Carbon's call sites.

| Slot | Address | Observation / interpretation |
| ---: | --- | --- |
| 0 | `0x006807E0` | Destructor |
| 1 | `0x00679FA0` | Nonempty device operation; not an empty return |
| 2 | `0x0067A010` | Empty return |
| 3 | `0x0067A020` | Virtual forwarder through slot 1 |
| 4 | `0x0067A030` | Empty return; common BeginUpdate position |
| 5 | `0x0067A040` | Empty return; common EndUpdate position |
| 6 | `0x0067A050` | `ret 0xC`; common road callback position |
| 7 | `0x0067A060` | `ret 0xC`; common skid callback position |
| 8 | `0x0067A070` | `ret 0xC`; common slip callback position |
| 9 | `0x0067A080` | `ret 0xC`; common RPM callback position |
| 10 | `0x0067A090` | `ret 4`; common shift-potential position |
| 11 | `0x0067A0A0` | `ret 8`; common NOS position |
| 12 | `0x0067A0B0` | `ret 4`; common engine-damage position |
| 13 | `0x0067A0C0` | `ret 4`; common shifting position |
| 14 | `0x0067A0E0` | `ret 4`; additional slot, not MW's collision signature |
| 15 | `0x006807C0` | Additional method; meaning not established here |
| 16 | `0x006807D0` | Additional method; meaning not established here |
| 17 | `0x0067A0D0` | `ret 8`; collision callback candidate |

The table includes concrete device operations, a forwarder and empty
callbacks. Argument layouts and interface ownership still need to be
recovered at Carbon's call sites.

## Xbox findings that need recovery

The Xbox DoFFB entry point and concrete feedback table remain to be
identified by tracing the device constructor and feedback callers.

Import/library candidates are XAM output ordinal `0x192`, import slot
`0x82000838`, thunk `0x82AACBE4`, and an `XFFB` library version
`2.0.4025.2`. These are leads for import/library verification, not proof of
particular motor effects, wheel forces or gameplay routing.

Carbon motor effects and console-output behavior remain unvalidated.
