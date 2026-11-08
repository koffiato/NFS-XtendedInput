# Undercover input and feedback

Undercover's force feedback subsystem, concrete `IFeedback` vtable, Logitech EZWheelWrapper
subsystem, collision formulas, and adapter hook points have been reverse engineered and
documented:

- **PC Retail (`nfs.exe`):** [uc-rumble.md](uc-rumble.md)
- **Xbox 360 Retail (`default.exe`):** [uc-rumble-x360.md](uc-rumble-x360.md)

The `UC` build supports controller rumble and optional impulse triggers on PC
v1.0.1.18. Driving and the Controls menu have been validated in local gameplay.

## Controls menu and the disabled native wheel manager

XtendedInput bypasses native DirectInput initialization at `0x0069A5B1`, which
leaves the Logitech wheel manager at `[0x00DA3BF0]` null. Two Controls menu
variants still call its connection query at `0x007577C0` without checking the
manager. A live debugger capture on 2026-11-08 confirmed an unhandled access
violation at `0x007577C0` (`mov eax, [ecx+0x70]`, ECX = 0), called from
`0x005A4C4F` with return address `0x005A4C54`.

Both menu call sites (`0x005A4A0E`, `0x005A4C4F`) now use
`UCWheelConnectedHook`. A null manager reports no wheel; a present manager
forwards to the original query with the same port and return value.
`tests/uc.cmd` verifies both paths through the native `__thiscall` ABI.
