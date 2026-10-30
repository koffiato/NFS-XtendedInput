# NFS XtendedInput - unified feedback branch

XInput controller support for seven Black Box Need for Speed games, with shared
rumble code for Underground, Underground 2 and Most Wanted.

This unofficial fork builds on [NFS-XtendedInput](https://github.com/xan1242/NFS-XtendedInput)
and [NFSU-XtendedInput](https://github.com/xan1242/NFSU-XtendedInput).
**`unified-feedback` is a development branch. Rumble is not implemented for all seven games.**

## Game support

All seven input targets compile. The feedback status below is separate from
input support; a passing build does not establish in-game compatibility.

| Game | Build target | Configuration | Rumble status |
| --- | --- | --- | --- |
| Underground | `UG` | [Config-UG](data/Config-UG) | Experimental US 1.4 adapter; disabled in the supplied INI |
| Underground 2 | `UG2` | [Config-UG2](data/Config-UG2) | US 1.2 adapter; executable integration tests pass |
| Most Wanted (2005) | `MW` | [Config-MW](data/Config-MW) | PC 1.3 adapter retained from the rumble fork |
| Carbon | `CB` | [Config-Carbon](data/Config-Carbon) | Not implemented |
| Undercover | `UC` | [Config-UC](data/Config-UC) | Not implemented |
| ProStreet | `PS` | [Config-ProStreet](data/Config-ProStreet) | Not implemented |
| World | `W` | [Config-World](data/Config-World) | Not implemented; upstream input support remains experimental |

No new in-game or controller hardware validation has been completed for this
branch. UG/UG2 check their added rumble hook signatures before patching; those
checks do not validate the upstream input hooks.

This branch targets **Windows 10/11 and 32-bit game executables**.
The unified build does not produce Windows 7 variants.

## Installation

1. Back up your existing XtendedInput ASI, INI and custom mappings.
2. Install the matching upstream package to obtain its ASI loader, button
   textures and cursor assets:
   - [Version 2.7](https://github.com/xan1242/NFSU-XtendedInput/releases/tag/2.7)
     for UG/UG2.
   - [Version 1.22](https://github.com/xan1242/NFS-XtendedInput/releases/tag/1.22)
     for MW/Carbon/Undercover/ProStreet/World.
3. Replace the XtendedInput ASI in the game's `scripts` folder with the build
   for that game:
   - UG/UG2: `build/<target>/NFSU_XtendedInput.asi`
   - Other games: `build/<target>/NFS_XtendedInput.asi`
4. For a fresh configuration, copy the INIs from the corresponding
   `data/Config-*` directory into `scripts`. To preserve existing bindings,
   merge the settings you need instead of replacing your INIs.

Use one XtendedInput ASI per game. If using Underground's dynamic button icons,
disable the Widescreen Fix's button-icon replacement. The x86 Visual C++
runtime is required by these builds.

The existing [UG2 rumble release](https://github.com/koffiato/NFS-XtendedInput/releases/tag/u2-rumble-v0.1.0)
predates this branch; it is not a unified seven-game release.

## Controls and configuration

The shipped control mappings come from the actual upstream 1.22 and 2.7
release packages. Each game's configuration directory also contains
`EventReference.txt` for its available actions.

Common defaults are right trigger for throttle, left trigger for brake,
left stick for steering, A for handbrake, B for NOS and Start/Menu for pause.
Camera controls and game-specific actions vary; use your game's INI as the
reference.

| Games | General settings | Controller and keyboard bindings |
| --- | --- | --- |
| UG/UG2 | `scripts/NFSU_XtendedInput.ini` | `[Events]` and `[EventsKB]` in the same file |
| MW and newer | `scripts/NFS_XtendedInput.ini` | `scripts/XtendedInputMaps/<profile>/NFS_XtendedInput.usermap.ini` |

For MW and newer, `NFS_XtendedInput.default.ini` supplies mappings when a
profile is created or its controls are reset. Secondary bindings use
`[Events_Secondary]` and `[EventsKB_Secondary]`.

ProStreet supports in-game remapping. MW's Controls menu remains disabled
because of an upstream crash. See the [Underground notes](underground/README.md)
for UG's remapping limits.

## Rumble settings

Edit `[Rumble]` in the main INI beside the ASI. These settings currently
affect **UG, UG2 and MW only**. The supplied UG INI sets `Enabled = 0`
because its adapter is experimental.

| Setting | Default | Meaning |
| --- | --- | --- |
| `Enabled` | 1; supplied UG config: 0 | Master switch |
| `Intensity` | 1.0 | Overall strength, 0-2 |
| `LeftMotorScale` / `RightMotorScale` | 1.0 | Heavy/light motor strength, 0-2 |
| `SwapMotors` | 0 | Swap the main motors |
| `Impacts` / `Surface` / `Tyres` / `Car` | 1.0 | Category strength, 0-2; zero mutes a category |
| `GearShift` | 2 | Shift knock: 0 off, 1 low, 2 medium, 3 strong, 4 maximum |
| `ImpulseTriggers` | 2 | Trigger rumble: 0 off, 1 broadcast, 2 automatic |
| `TriggerIntensity` | 1.0 | Trigger strength, 0-2 |
| `TriggerPedalScaling` | 1 | Scale trigger effects with pedal input |
| `Telemetry` | 0 | Write diagnostic feedback logs beside the plugin |

UG/UG2 use their own native event curves for impacts, road texture, suspension,
drift, wheel slip, NOS, shifts and drag RPM. UG2 additionally reads wheel data
for wheelspin and brake lockup. MW retains the fork's surface, collision,
suspension, tyre and car effects, including shift cues and blown tyres.

Impulse triggers require compatible Xbox One/Series hardware and
Windows.Gaming.Input. Automatic mode uses conservative controller detection;
UG2 additionally requires a single XInput controller on port 0. When automatic
mode is unavailable, ordinary XInput rumble remains available.
Broadcast mode sends combined output to all detected WGI pads.

Continuous effects fade during sustained use, and impacts reduce background
textures. The adapters include stops for pause, focus loss, disconnect and
shutdown. See [UG2 feedback details](underground/RUMBLE.md) for its lifecycle
checks and diagnostics.

## Build

Install Visual Studio C++ desktop tools with an x86 compiler and a Windows SDK.
Clone the branch with its pinned submodule:

```bat
git clone --branch unified-feedback --recurse-submodules https://github.com/koffiato/NFS-XtendedInput.git
cd NFS-XtendedInput
```

From an **x86 Visual Studio developer command prompt**, run:

```bat
build.cmd all
```

To build one game, use its target from the support table, for example:

```bat
build.cmd UG2
build.cmd MW
```

Outputs go to `build/<target>/`. The root build script is the supported build
entry point for this branch; the older Visual Studio projects remain in the
repository.

## Validation and development

All seven input builds and the UG2 executable integration harness passed at
the initial unified checkpoint. To run the harness against your local US 1.2
executable:

```bat
underground\tests\run.cmd "path\to\SPEED2.EXE"
```

The harness checks hook signatures, native curves, routing, lifecycle stops,
pedal mappings, wheel fixtures, mixer behavior and shared settings. It mocks
controller output and game callbacks; it does not launch the game.

Carbon, Undercover, ProStreet and World still need feedback adapters.
UG needs gameplay validation, and the shared refactor needs controller testing.
See [UNIFIED.md](UNIFIED.md) for the code layout and data provenance.

Report issues with this fork in
[this repository](https://github.com/koffiato/NFS-XtendedInput/issues).

## Credits and license

- [xan1242](https://github.com/xan1242) and Berkay Yigit - upstream XtendedInput projects.
- [Martin Penkava](https://github.com/shaneomac1337) - the Most Wanted rumble implementation.
- [AeroWidescreen](https://github.com/AeroWidescreen) - controller button icons.
- LINK/2012 - injector.
- [NFSU2-SplitScreen research](https://github.com/mihaits/NFSU2-SplitScreen/blob/main/docs/input.md)
  - reference material for the UG2 adapter.
- [Xan's feedback research](https://github.com/xan1242/NFS-XtendedInput/issues/10#issuecomment-1181213340)
  - interface and console-effect investigation.

The rumble work and unified refactor include AI-assisted development.
The project is distributed under the [MIT license](LICENSE); bundled
dependencies retain their own licenses.
