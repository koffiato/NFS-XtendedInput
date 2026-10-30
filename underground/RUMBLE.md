# Underground 2 rumble

The port uses U2's console feedback event dispatcher and authored envelopes,
adapted to the MW fork's category mixer, impact ducking, fatigue fade, motor
scaling and Windows.Gaming.Input impulse trigger output. It replaces the U2
console motor queue and keeps the original feedback update/steering-wheel path.

Supported effects are collisions, road texture, suspension shocks, drift/yaw,
wheel slip, NOS, gear shifts and the game's drag RPM effect. Wheel data adds
wheelspin and brake lockup outside drift events. LT carries braking slip; RT
carries wheelspin, NOS and shifts. MW's Attrib surface waves, spike strips,
engine-damage effects and perfect/missed-shift cues are not part of this port.
Underground 1 rumble is deferred.

## Install

Use an ASI loader and the **U2** build of `NFSU_XtendedInput.asi`. Back up your
existing plugin, then replace that ASI in `scripts`. Keep your current INI and
bindings: missing `[Rumble]` keys use the defaults below. The supplied U2 INI is
for fresh installations; replacing an existing INI resets custom bindings.

The first version targets **U2 US 1.2**, the same executable layout as upstream.
The added hook signatures and curve table were checked against the user's
installed `SPEED2.EXE`, SHA-256
`e17c8f060bae797beaf61395c6e2f1a23bcde8c4666a412c92274aa411066214`.
Other variants require verification. A mismatched or already hooked rumble site
disables the added rumble patches and emits a debugger message. `Telemetry=1`
also records the reason in `NFSU_XtendedInput_Rumble.log` beside the plugin.
This validation covers the added rumble hooks; upstream's input patches retain
their existing executable requirements.

## Settings

Add `[Rumble]` to `NFSU_XtendedInput.ini` beside the ASI:

| Key | Default | Meaning |
|---|---|---|
| Enabled | 1 | Master switch. When off, no added patches or rumble output. |
| Intensity | 1.0 | Overall gain, 0–2. |
| LeftMotorScale / RightMotorScale | 1.0 | Heavy/light motor gains, 0–2. |
| SwapMotors | 0 | Swap the two main motors. |
| Impacts / Surface / Tyres / Car | 1.0 | Category gains, 0–2; zero mutes a category. |
| GearShift | 2 | Shift knock: 0 off, 1 low, 2 medium, 3 strong, 4 max. |
| ImpulseTriggers | 2 | 0 off; 1 broadcast; 2 automatic. |
| TriggerIntensity | 1.0 | Trigger gain, 0–2, also affected by Intensity. |
| TriggerPedalScaling | 1 | Scale trigger effects by the configured pedal input. |
| Telemetry | 0 | Write event and output samples to the adjacent log. |

Automatic impulse triggers require one WGI pad, one XInput controller, port 0
connected, and no detected Steam virtual pad. Otherwise output uses each player's
own XInput slot. WGI cannot map its enumeration order to XInput slots, so mode 1
intentionally broadcasts the combined effects to all WGI pads. Trigger motors
require compatible hardware and Windows 10+. WGI is loaded dynamically; failure
falls back to ordinary XInput rumble. The Windows 7 build uses XInput 9.1.0.

The INI is the master switch; the profile's hidden feedback byte is not modified.
Continuous effects fade to 60% after sustained use, recover after quiet, and duck
during impacts. Pulses keep their strength. Motors stop on pause menus, focus
loss, non-race states, invalid/frozen simulation steps, missed frame updates,
disconnect and process exit. Pedal scaling follows scanner mappings, including
alternate analog and keyboard bindings. Unsupported/missing wheel data produces
no extra slip effect.

## Build and verify

Open the solution in Visual Studio with C++ desktop tools and a Windows SDK.
Build `Release-UG2|Win32`. The legacy project configuration uses v140;
`build.cmd UG2-Win7` can also build against an installed modern compiler with
`XINPUT_OLD` for the XInput 9.1.0 path. Windows 7 runtime behavior still needs testing.

From an **x86** Visual Studio developer command prompt:

```bat
build.cmd UG2
build.cmd UG2-Win7
tests\run.cmd "D:\Games\NFS Underground 2\SPEED2.EXE"
```

The test harness maps a user-supplied executable into a separate test process,
validates the actual hook signatures and event data, and mocks controller output
and game callbacks. It checks patch refusal on mismatch, disabled configuration,
event routing/envelope expiry, pause/focus/menu/frozen-frame handling, remapped
pedals, wheel telemetry including air/reverse/invalid pointers, and mixer gains,
clamping, fatigue and impact priority. It does not launch or modify the game.

Game/hardware acceptance checks: race with collisions, kerbs, drifting, wheelspin,
hard braking, NOS and automatic/manual shifts; pause/resume, Alt-Tab, restart and
quit while an effect plays; unplug/reconnect a pad; connect a second pad and check
automatic WGI fallback. Verify normal body rumble with `ImpulseTriggers=0` and
impulse triggers in auto mode on an Xbox One/Series controller. These live checks
are required before calling the build fully validated.

## Provenance

- Target: xan1242/NFSU-XtendedInput, commit
  `0b88532a99a06334e2385a5f251ea502dd9d7555`.
- Mixer, envelopes, fatigue and WGI backend: koffiato/NFS-XtendedInput, commit
  `b5aa64ca241c43ad2fbab316896f2ccf43efea96`,
  `src/NFS_XtendedInput_Rumble.h` (MIT; Martin Penkava).
- U2 feedback entry points and wheel layout were cross-checked with the
  [NFSU2-SplitScreen research notes](https://github.com/mihaits/NFSU2-SplitScreen/blob/main/docs/input.md)
  and independently inspected in the installed executable. That project's runtime
  implementation is not included.
