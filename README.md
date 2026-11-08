# NFS XtendedInput

This unofficial branch combines [NFS-XtendedInput](https://github.com/xan1242/NFS-XtendedInput)
and [NFSU-XtendedInput](https://github.com/xan1242/NFSU-XtendedInput) and adds
rumble support. It targets Windows 10/11 and 32-bit game executables.

[Underground 1](docs/ug-rumble.md) and [Underground 2](docs/ug2-rumble.md)
use their native feedback engines with a small output adapter. Underground 2 has optional trigger support.

[Most Wanted](docs/mw-rumble-x360.md) and [Carbon](docs/cb-rumble-x360.md) port
Xbox 360 controller feedback using native PC physics and surface data. Impulse
triggers are an optional extension.

Current support:

| Game | Build target | Existing feedback status |
| --- | --- | --- |
| Underground | `UG` | [Native feedback](docs/ug-rumble.md) |
| Underground 2 | `UG2` | [Native feedback and impulse triggers](docs/ug2-rumble.md) |
| Most Wanted (2005) | `MW` | [Xbox 360 feedback and optional impulse triggers](docs/mw-rumble-x360.md) |
| Carbon | `CB` | [Xbox 360 feedback and optional impulse triggers](docs/cb-rumble-x360.md) |
| Undercover | `UC` | Input only; [research](docs/uc-xinput.md) |
| ProStreet | `PS` | Experimental native feedback; [research](docs/ps-rumble-x360.md) |
| World | `W` | Input only; [research](docs/world-rumble.md) |

## Packages and configuration

Development ZIPs use the official release layout. Back up your ASI, INIs and
custom bindings, then extract the matching `Release-<game>-Pack.zip` into the
game directory.

* UG/UG2 settings and bindings reside in `scripts/NFSU_XtendedInput.ini`,
  with bindings in `[Events]` and `[EventsKB]` while rumble is under `[Rumble]`.
* Later games use `scripts/NFS_XtendedInput.ini` for general settings
  and `scripts/XtendedInputMaps/<profile>/NFS_XtendedInput.usermap.ini` for bindings.
* Impulse triggers are disabled by default. Set `ImpulseTriggers = 2`
to opt into automatic detection or `1` to broadcast to
all WGI controllers on supported games.
* See `EventReference.txt` for the action list.
* MW's in-game configuration is disabled for now.
* **When using dynamic icons in UG/UG2, disable Widescreen Fix's icon replacement.**

Canonical mappings come from the actual [1.22](https://github.com/xan1242/NFS-XtendedInput/releases/tag/1.22) and hashes. ,

## Build and checks

Clone with submodules, then use an x86 Visual Studio C++ developer prompt:

```bat
git clone --branch unified-feedback --recurse-submodules https://github.com/koffiato/NFS-XtendedInput.git
cd NFS-XtendedInput
build.cmd all
tests\input.cmd
```


Outputs are written under `build/<target>/`.

Builds generate the commented INIs and event reference for the selected game
from [shared C++ configuration definitions](config/README.md). Build
outputs live under `build/<target>/config/`; the same text is embedded in the ASI
for first-run creation and extracted during packaging. Unsupported settings and unused bindings are omitted.

Visual Studio users can open `src/NFS_XtendedInput.sln` and choose any of the
seven game configurations. Release builds use the same output directories as
`build.cmd`; debug builds go under `build/debug/<target>/`. Packaging is handled
by `scripts/release.py` for both build paths.

| Source | Purpose |
| --- | --- |
| `src/Unified.cpp`, `src/Main.hpp` | Shared build entry point and initialization |
| `src/input/` | Shared configuration, polling, bindings and frontend integration |
| `src/games/` | Underground-specific address/event/frontend definitions |
| `src/rumble/` | Shared output/settings and game feedback adapters |
| `config/`, `tools/configgen.cpp` | Shared defaults, binding rules and native config emission |
| `tests/` | Runtime, input, callback and packaging checks |

The [Actions workflow](.github/workflows/release.yml) builds all seven targets
on branch/PR/manual runs and produces a `release-packages` artifact. Tag pushes
can create a draft prerelease; published assets are not overwritten.
`scripts/release.py` verifies pinned official mod ZIP hashes and packages only
the required loader/assets with the current ASI/configurations. Python 3 is
required for packaging and its tests; ordinary builds use the C++ toolchain.
No games are downloaded. Run the packaging checks with:

```bat
python -m unittest discover -s tests -v

## Credits and license
`
-`` [xan1242](https://github.com/xan1242), Berkay Yigit: upstream XtendedInput.
- [Martin Penkava](https://github.com/shaneomac1337): original MW rumble work.
- [AeroWidescreen](https://github.com/AeroWidescreen): controller textures and mouse cursor.
- LINK/2012: injector; pulzed: mINI.
- [NFSU2-SplitScreen research](https://github.com/mihaits/NFSU2-SplitScreen/blob/main/docs/input.md): UG2 input reference.
- [Xan's feedback investigation](https://github.com/xan1242/NFS-XtendedInput/issues/10#issuecomment-1181213340): later-game interface and console-effect research.

Code and research include AI-assisted work. The project uses the
[MIT license](LICENSE); bundled dependencies retain their own licenses.
