# NFS XtendedInput

Adds xinput controls and rumble support for BlackBox era Need for Speed games. This unofficial branch combines
[NFS-XtendedInput](https://github.com/xan1242/NFS-XtendedInput) and
[NFSU-XtendedInput](https://github.com/xan1242/NFSU-XtendedInput).

## Supported games

| Game | Build target | Rumble |
| --- | --- | --- |
| Underground | `UG` | [Native feedback](docs/ug-rumble.md) |
| Underground 2 | `UG2` | [Native feedback](docs/ug2-rumble.md) |
| Most Wanted (2005) | `MW` | [Xbox 360 feedback](docs/mw-rumble-x360.md) |
| Carbon | `CB` | [Xbox 360 feedback](docs/cb-rumble-x360.md) |
| Undercover | `UC` | [Xbox 360 feedback](docs/uc-rumble.md) |
| ProStreet | `PS` | [Xbox 360 feedback](docs/ps-rumble-x360.md) |
| World | `W` | Experimental, [input only](docs/world-rumble.md) |

## Install and configure

Back up your ASI, INIs and custom bindings, then extract the matching
`Release-<game>-Pack.zip` into the game directory.

- UG/UG2: `scripts/NFSU_XtendedInput.ini` contains settings and bindings.
- Later games: settings are in `scripts/NFS_XtendedInput.ini`; bindings are in
  `scripts/XtendedInputMaps/<profile>/NFS_XtendedInput.usermap.ini`.
- `EventReference.txt` lists the available actions.
- Impulse triggers are optional on UG2, MW, Carbon, Undercover and ProStreet.
  In `[Rumble]`, set `ImpulseTriggers = 2` for automatic detection or `1` to
  broadcast to all WGI controllers. The default is `0` (off).
- MW's in-game control configuration is disabled for now.
- **When using dynamic icons, disable Widescreen Fix's icon replacement.**

## Build and checks

Clone with `--recurse-submodules`, then use an x86 Visual Studio C++ developer prompt:

```bat
build.cmd all
tests\runtime.cmd all
tests\input.cmd
tests\feedback.cmd all
python -m unittest discover -s tests -v
```

Replace `all` with a build target from the table to build one game. Outputs go
under `build/<target>/`. Visual Studio users can open
[src/NFS_XtendedInput.sln](src/NFS_XtendedInput.sln).

Native Underground checks also require a local game executable:

```bat
tests\ug.cmd "path\to\Speed.exe"
tests\ug2.cmd "path\to\SPEED2.EXE"
```

Python 3 is required for packaging and Python tests. See
[configuration sources](config/README.md) and [release packaging](scripts/release.py)
for development details.

## Credits and license

- [xan1242](https://github.com/xan1242), Berkay Yigit: upstream XtendedInput.
- [Martin Penkava](https://github.com/shaneomac1337): original MW rumble work.
- [AeroWidescreen](https://github.com/AeroWidescreen): controller textures and mouse cursor.
- LINK/2012: injector; pulzed: mINI.
- [NFSU2-SplitScreen research](https://github.com/mihaits/NFSU2-SplitScreen/blob/main/docs/input.md): UG2 input reference.
- [Xan's feedback investigation](https://github.com/xan1242/NFS-XtendedInput/issues/10#issuecomment-1181213340): later-game interface and console-effect research.

AI tools were used in this project.

[MIT license](LICENSE). Bundled dependencies retain their own licenses.
