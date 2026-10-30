# Unified branch — work in progress

All seven input targets build from `src/Unified.cpp` with `build.cmd all` in
an x86 Visual Studio developer prompt. Windows 10/11 only; no Win7 packages.
Individual targets are UG, UG2, MW, CB (Carbon), UC, PS and W (World).
This is a development branch, not a seven-game rumble release.

## Feedback status

| Game | Input build | Feedback |
| --- | --- | --- |
| Underground | Compiles | Experimental US 1.4 native-event adapter; gameplay untested |
| Underground 2 | Compiles | Existing US 1.2 adapter; executable integration harness passes |
| Most Wanted | Compiles | Existing PC 1.3 IFeedback adapter preserved |
| Carbon | Compiles | Adapter pending; stock MW feedback layout is incompatible |
| Undercover | Compiles | Adapter pending; callbacks have different argument sizes |
| ProStreet | Compiles | Adapter pending; installed executable could not be inspected after launch |
| World | Compiles | Adapter pending |

`[Rumble]` currently takes effect only in UG, UG2 and MW. No new hardware or
in-game validation has been completed for this branch. The installed local
copies available for further testing are UG2, MW and ProStreet. Completing
and verifying the remaining adapters requires suitable local executables.
No downloaded game executables or research binaries are included in the repo.

## Shared implementation

`src/rumble/Core.h` owns categories, gains, fatigue, ducking, envelopes, ramps,
shift presets and the mixer. `Output.h` owns timing, motor quantization and
the dynamically loaded Windows.Gaming.Input trigger backend. `Settings.h`
defines one bounded configuration reader for both input engines.
`Underground.h` shares UG/UG2 event processing, routing, output, lifecycle and
native-curve playback; each game has its own validated hook addresses.
The existing MW adapter uses the same core, output and settings headers.

The two upstream input engines remain separate adapters because their native
event scanners and ActionID interfaces differ. They share the entry point,
build, feedback implementation and configuration layout in this repository.

## Configuration provenance

The files under `data/Config-*` come from the actual shipped non-Win7 ZIPs:

- [NFS-XtendedInput 1.22](https://github.com/xan1242/NFS-XtendedInput/releases/tag/1.22)
- [NFSU-XtendedInput 2.7](https://github.com/xan1242/NFSU-XtendedInput/releases/tag/2.7)

Control mappings are preserved. Only a `[Rumble]` section was appended and
the stray `<<<<<<< HEAD` line in the UG2 release INI was removed.
`data/config-provenance.json` records the SHA-256 of each original config.
The legacy `underground/Config-*` paths mirror the canonical data files.

Those release packages contain input mappings, not authored feedback curves.
UG/UG2 read the curves from the running game; the existing MW adapter reads
its native surface attributes and retains the fork's console-derived effects.
These are not newly recovered, console-verified profiles for all seven games.

Xan's [IFeedback research](https://github.com/xan1242/NFS-XtendedInput/issues/10#issuecomment-1181213340)
documents why the PC stubs need replacing and why the hidden feedback setting
must be considered. Its PS3/PS2/Xbox controller-icon packages are not feedback data.

## Validation

Run `underground\tests\run.cmd "path\to\SPEED2.EXE"` from an x86 developer
prompt. This harness maps only feedback data and hook signatures, mocks device
output and native frame calls, and exercises native curves, ABI, routing,
lifecycle stops, pedal remaps, wheel fixtures, mixer and shared settings.
It does not launch the game or establish that the effects feel correct.
