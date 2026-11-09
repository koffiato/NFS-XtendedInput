# Undercover mixer reference vectors

`uc-console-mixer.txt` contains 64 synthetic states executed through the local
retail PowerPC Run routine at `0x8260F6F0`, wave sampler `0x8260E0D0`, and ramp
sampler `0x8260E038`. The reference image SHA-256 is recorded in the file.
The research interpreter executes those instructions; only register-save
helpers and the CRT sine call are substituted with host operations.
These vectors contain no game assets or executable instructions.

Each row gives the LCG seed, elapsed milliseconds and two expected 16-bit
motor outputs. The LCG is `state = state*1664525 + 1013904223` modulo 2^32,
returning `(state >> 16) & 65535`. `CheckNativeMixerVectors` reconstructs the
same synthetic state from each seed. Wave parameters are exact binary
fractions; remaining float and sine differences allow two output units.

The corpus exercises signed channel sums, stereo routing, strongest-effect
selection, drift, collision waves, NOS, indexed bumps, and discarded car
ramps. The separate consecutive-frame test checks BeginUpdate phase resets
and Push-before-Run timing. The binary audit independently executes the
native BeginUpdate stores for three controller ports.
