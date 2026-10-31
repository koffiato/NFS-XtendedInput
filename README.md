# NFS XtendedInput

This unofficial branch combines [NFS-XtendedInput](https://github.com/xan1242/NFS-XtendedInput)
and [NFSU-XtendedInput](https://github.com/xan1242/NFSU-XtendedInput) and adds
rumble support. It targets Windows 10/11 and 32-bit game executables.

**Current development is focused on [Underground 2](docs/ug2-rumble.md).**
The goal is console-grounded controller feedback through the game's existing
event engine, a small output adapter and conservative impulse triggers.

Other completed work is retained, but is not being expanded:

| Game | Target / configuration | Existing feedback status |
| --- | --- | --- |
| Underground | `UG` / [Config-UG](data/Config-UG) | Experimental US 1.4; disabled by default |
| Underground 2 | `UG2` / [Config-UG2](data/Config-UG2) | US 1.2 adapter; executable integration checks pass |
| Most Wanted (2005) | `MW` / [Config-MW](data/Config-MW) | PC 1.3 adapter retained from the rumble fork |
| Carbon | `CB` / [Config-Carbon](data/Config-Carbon) | Input only |
| Undercover | `UC` / [Config-UC](data/Config-UC) | Input only |
| ProStreet | `PS` / [Config-ProStreet](data/Config-ProStreet) | Experimental; [archived findings](docs/prostreet-rumble.md) |
| World | `W` / [Config-World](data/Config-World) | Experimental input only |

All seven input targets compile. This does not establish gameplay compatibility.

## Packages and configuration

Development ZIPs use the official release layout. Back up your ASI, INIs and
custom bindings, then extract the matching `Release-<game>-Pack.zip` into the
game directory.

* UG/UG2 settings and bindings reside in `scripts/NFSU_XtendedInput.ini`,
  with bindings in `[Events]` and `[EventsKB]`.
* Later games use `scripts/NFS_XtendedInput.ini` for general settings
  and `scripts/XtendedInputMaps/<profile>/NFS_XtendedInput.usermap.ini` for bindings.
* See `EventReference.txt` for the action list.
* MW's in-game configuration is disabled for now.
* **When using dynamic icons in UG/UG2, disable Widescreen Fix's icon replacement.**

Canonical mappings come from the actual
[1.22](https://github.com/xan1242/NFS-XtendedInput/releases/tag/1.22) and
[2.7](https://github.com/xan1242/NFSU-XtendedInput/releases/tag/2.7) packages.
[data/config-provenance.json](data/config-provenance.json) records original
hashes. Control mappings are preserved; the branch adds `[Rumble]`, disables
experimental UG rumble by default and removes a stray conflict marker from
the upstream UG2 INI.

## Feedback configuration

Edit `[Rumble]` in the main INI beside the plugin. See the
[UG2 documentation](docs/ug2-rumble.md) for settings and validation limits.
The existing adapters for MW and ProStreet retain their earlier feedback work;
[ProStreet research](docs/prostreet-rumble.md) is archived.

## Build and checks

Clone with submodules, then use an x86 Visual Studio C++ developer prompt:

```bat
git clone --branch unified-feedback --recurse-submodules https://github.com/koffiato/NFS-XtendedInput.git
cd NFS-XtendedInput
build.cmd all
tests\input.cmd
```

Outputs are written under `build/<target>/`. See the
[UG2 documentation](docs/ug2-rumble.md) for its build and executable checks.
No game binaries are stored in the repository or required by CI.

| Source | Purpose |
| --- | --- |
| `src/Unified.cpp`, `src/Main.hpp` | Shared build entry point and initialization |
| `src/input/` | Shared configuration, polling, bindings and frontend integration |
| `src/games/` | Underground-specific address/event/frontend definitions |
| `src/rumble/` | Shared output/settings and game feedback adapters |
| `tests/` | Runtime, input, callback and packaging checks |

The [Actions workflow](.github/workflows/release.yml) builds all seven targets
on branch/PR/manual runs and produces a `release-packages` artifact. Tag pushes
can create a draft prerelease; published assets are not overwritten.
`scripts/release.py` verifies pinned official mod ZIP hashes and packages only
the required loader/assets with the current ASI/configurations. No games are
downloaded. Run the packaging checks with:

```bat
python -m unittest discover -s tests -v
```

Report problems in [this fork](https://github.com/koffiato/NFS-XtendedInput/issues).

## Credits and license

- [xan1242](https://github.com/xan1242), Berkay Yigit: upstream XtendedInput.
- [Martin Penkava](https://github.com/shaneomac1337): original MW rumble work.
- [AeroWidescreen](https://github.com/AeroWidescreen): controller textures and mouse cursor.
- LINK/2012: injector; pulzed: mINI.
- [NFSU2-SplitScreen research](https://github.com/mihaits/NFSU2-SplitScreen/blob/main/docs/input.md): UG2 input reference.
- [Xan's feedback investigation](https://github.com/xan1242/NFS-XtendedInput/issues/10#issuecomment-1181213340): later-game interface and console-effect research.

Code and research include AI-assisted work. The project uses the
[MIT license](LICENSE); bundled dependencies retain their own licenses.
