# Unified implementation notes

The [README](README.md) is the main guide for game support, installation,
configuration, builds and validation. This document describes the shared
implementation and where its configuration data came from.

## Source layout

| Path | Responsibility |
| --- | --- |
| `src/Unified.cpp` | Build entry point for all seven targets |
| `src/Main.hpp` | MW and newer input engine |
| `underground/dllmain.cpp` | UG/UG2 input engine |
| `src/rumble/Core.h` | Categories, gains, envelopes, ramps, shift presets, fatigue, ducking and mixing |
| `src/rumble/Output.h` | Timing, motor quantization and Windows.Gaming.Input backend |
| `src/rumble/Settings.h` | Shared bounded settings reader |
| `src/rumble/Underground.h` | UG/UG2 native events, game-specific hooks, routing and lifecycle |
| `src/NFS_XtendedInput_Rumble.h` | MW feedback interface and vehicle/surface access |
| `underground/NFSU_XtendedInput_Rumble.h` | Compatibility include for the shared Underground adapter |

The two input engines remain separate because Underground's event scanners and
the later games' ActionID interfaces differ. The existing feedback adapters
share the core, settings and WGI backend. Carbon, Undercover, ProStreet and
World do not yet have feedback adapters.

## Configuration provenance

The canonical `data/Config-*` files come from the shipped non-Win7 packages of
[NFS-XtendedInput 1.22](https://github.com/xan1242/NFS-XtendedInput/releases/tag/1.22)
and [NFSU-XtendedInput 2.7](https://github.com/xan1242/NFSU-XtendedInput/releases/tag/2.7).

Control mappings are preserved. A `[Rumble]` section was appended, the supplied
UG configuration disables experimental rumble, and the stray
`<<<<<<< HEAD` line in the UG2 release INI was removed.
[data/config-provenance.json](data/config-provenance.json) records SHA-256
hashes of the original files. The legacy `underground/Config-*` directories
mirror their canonical configurations.

The upstream ZIPs contain input mappings and controller-icon assets, not
authored feedback profiles. UG/UG2 read their native event curves from the
running game. MW reads native surface attributes and retains the fork's
console-derived effects. Console profiles have not been recovered or verified
for all seven games.

## Hook and interface differences

UG and UG2 share an event-table format but use different dispatcher calling
conventions, addresses and some curve values. Each profile validates its added
hook signatures before installing rumble patches.

Later games also differ: Carbon's feedback interface has additional slots
compared with MW, and Undercover changes callback argument sizes. Reusing the
MW interface layout unchanged would therefore be unsafe. These adapters remain
unfinished.

Xan's [IFeedback investigation](https://github.com/xan1242/NFS-XtendedInput/issues/10#issuecomment-1181213340)
explains the PC feedback stubs, interface registration and hidden vibration
setting. [UG2 research and validation notes](underground/RUMBLE.md) document
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
