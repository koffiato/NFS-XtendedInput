# Unified implementation notes

The [README](README.md) is the main guide for game support, installation,
configuration, builds and validation. This document describes the shared
implementation and where its configuration data came from.

## Source layout

| Path | Responsibility |
| --- | --- |
| `src/Unified.cpp` | Build entry point for all seven targets |
| `src/Main.hpp` | MW and newer input engine |
| `src/ug/Input.cpp` | UG/UG2 input engine |
| `src/input/StringHash.h` | Runtime and compile-time NFS string hashes |
| `src/input/Keyboard.h` | Windows key-name conversion |
| `src/input/Bindings.h` | Controller binding names, values and menu-label lookup |
| `src/input/Analog.h` | Normalized trigger/stick reads and remapping activation order |
| `src/rumble/Core.h` | Categories, gains, envelopes, ramps, shift presets, fatigue, ducking and mixing |
| `src/rumble/Output.h` | Timing, motor quantization and Windows.Gaming.Input backend |
| `src/rumble/Settings.h` | Shared bounded settings reader |
| `src/rumble/Underground.h` | UG/UG2 native events, game-specific hooks, routing and lifecycle |
| `src/NFS_XtendedInput_Rumble.h` | MW/PS feedback interface, effect mixing and lifecycle |
| `src/rumble/Vehicle.h` | Guarded MW/PS vehicle and interface readers |
| `src/rumble/Surface.h` | Native surface attributes, wave validation and sampling |
| `src/rumble/Telemetry.h` | MW/PS diagnostic surface, vehicle and collision records |
| `src/rumble/Log.h` | Diagnostic log opening with concurrent reader access |
| `src/rumble/ProStreet.h` | ProStreet native interface signatures and collision argument layout |

The two input engines remain separate because Underground's event scanners and
the later games' ActionID interfaces differ. The existing feedback adapters
share the core, settings and WGI backend. Carbon, Undercover and World do not yet have feedback adapters.

Input names, menu lookup and analog sampling use the same helpers in both
engines. Their small wrappers preserve the existing contracts: Underground
matches INI names exactly, supports the combined D-pad binding and writes `0`
for an unbound controller name. Later games match names without case and write
`-1`. Each engine retains its menu text and icon modes. Analog sampling keeps
the original 32767 divisor, including the slightly larger negative endpoint,
and the original priority when more than one control is fully pressed.

Add controller bindings in `src/input/Bindings.h`; its table owns the canonical
INI name, value and label index. Keep labels in the game-facing configuration
header. Add keyboard names in `src/input/Keyboard.h`. Changes to normalization
belong in `src/input/Analog.h`, not in either engine's polling wrapper.

`src/Unified.cpp` builds one translation unit per target. The native rumble
headers contain state and definitions and are included through that entry
point; they are not separate compilation units. Keep native calling conventions,
interface slot order and embedded-object layout in the adapters. In particular,
PS slip ratios/angles and MW slip speeds use different units even though their
mixing code is shared. Changes to these interfaces require the callback harness
as well as a build.

Run `tests\input.cmd` to check both input contracts without a game executable.
The cleanup was also compared against the previous implementations across all
65,536 binding values, all icon modes, all named keys/bindings in upper and lower
case, every 16-bit stick value and every 8-bit trigger value. The before/after
results matched. The CI tests cover aliases, case handling, unknown/combined
bindings, labels, analog endpoints, remapping priority and invalid remap indices.

## Configuration provenance

The canonical `data/Config-*` files come from the shipped non-Win7 packages of
[NFS-XtendedInput 1.22](https://github.com/xan1242/NFS-XtendedInput/releases/tag/1.22)
and [NFSU-XtendedInput 2.7](https://github.com/xan1242/NFSU-XtendedInput/releases/tag/2.7).

Control mappings are preserved. A `[Rumble]` section was appended, the supplied
UG configuration disables experimental rumble, and the stray
`<<<<<<< HEAD` line in the UG2 release INI was removed.
[data/config-provenance.json](data/config-provenance.json) records SHA-256
hashes of the original files. All targets use the canonical `data/Config-*`
directories; duplicated legacy configurations and projects have been removed.

The upstream ZIPs contain input mappings and controller-icon assets, not
authored feedback profiles. UG/UG2 read their native event curves from the
running game. MW reads native surface attributes and retains the fork's
console-derived effects. ProStreet reads live PC surface and vehicle data,
using MW effect tuning temporarily; see [ProStreet notes](docs/prostreet-rumble.md).
Console profiles have not been recovered or verified
for all seven games.

## Hook and interface differences

UG and UG2 share an event-table format but use different dispatcher calling
conventions, addresses and some curve values. Each profile validates its added
hook signatures before installing rumble patches.

Later games also differ: Carbon's feedback interface has additional slots
compared with MW, and Undercover changes callback argument sizes. Reusing the
MW interface layout unchanged would therefore be unsafe. The PS adapter
implements its 21-slot interface, five-argument skid/collision callbacks and
separate road callback. Carbon and Undercover adapters remain unfinished.

Xan's [IFeedback investigation](https://github.com/xan1242/NFS-XtendedInput/issues/10#issuecomment-1181213340)
explains the PC feedback stubs, interface registration and hidden vibration
setting. [UG2 research and validation notes](docs/ug2-rumble.md) document
the existing adapter in more detail.

## Release packaging

[`scripts/release.py`](scripts/release.py) defines the seven official archive
names and their resource layouts. It verifies pinned upstream ZIP checksums,
copies only the required loader/texture/cursor files, and adds the current ASI,
canonical configuration, action reference, license and installation notes.
Packaging rejects invalid or non-x86 DLLs and verifies the output archive.

The [Actions workflow](.github/workflows/release.yml) tests the packager, builds
each game on Windows, and collects all seven archives before generating checksums.
Only a tag push can create a draft prerelease; branch/PR/manual runs produce
downloadable artifacts. Packaging tests use synthetic fixtures and require no
game executable or network access:

```bat
python -m unittest discover -s tests -v
```

To package an existing local build, pass the full source commit SHA:

```bat
python scripts/release.py package --game UG2 --repository koffiato/NFS-XtendedInput --commit <40-character-commit-sha>
```

The script caches official mod packs in `.cache/upstream` and writes packages
to `dist`. After packaging all seven targets, run
`python scripts/release.py checksums` to create `dist/SHA256SUMS.txt`.
