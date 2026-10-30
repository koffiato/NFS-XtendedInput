# Underground input adapter

The UG/UG2 input engine lives in [`src/ug`](../src/ug) and uses the root
build and release workflow. Start with the [main README](../README.md) for
current game support, installation, settings and build instructions.

Both games now use [one shared feedback adapter](../src/rumble/Underground.h).
UG targets US 1.4 and is experimental, with rumble disabled in the supplied
configuration. UG2 targets US 1.2 and passes the executable integration
harness; in-game controller validation remains pending.

## Configuration

The canonical release-based configurations and action references are:

- [Underground](../data/Config-UG)
- [Underground 2](../data/Config-UG2)

UG/UG2 store settings and bindings in `NFSU_XtendedInput.ini`.
One controller button can be assigned per event in the INI.
Disable the Widescreen Fix's button-icon replacement to use XtendedInput's
dynamic icons.

## Underground in-game remapping

UG's controller settings menu can edit the actions it exposes; frontend
events still require INI edits. Duplicate bindings are permitted.

Only primary keyboard mappings are supported by this menu. For controllers,
steering, throttle and brake use analog primary bindings and digital secondary
bindings; other actions expose primary bindings only.

Press F5 while rebinding to unbind an action. Bind F5 itself through the INI.
Changes made in the menu are saved to the configuration file.

## Build and test

From the repository root in an x86 Visual Studio developer command prompt:

```bat
build.cmd UG
build.cmd UG2
tests\ug2.cmd "path\to\SPEED2.EXE"
```

The root `build.cmd` is the build entry point for every target. Windows 7
variants are not part of the unified build.

See [UG2 rumble notes](ug2-rumble.md) for UG2's effect behavior, hook provenance and test
coverage. The input engine originates from
[xan1242/NFSU-XtendedInput](https://github.com/xan1242/NFSU-XtendedInput)
under the [MIT license](../LICENSE).
