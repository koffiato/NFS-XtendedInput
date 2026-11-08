# Configuration sources

`bindings.csv` consolidates the active default mappings from the upstream 1.22
and Underground 2.7 packages. Each row lists the games that use that mapping.
`provenance.json` records the original upstream file hashes. Duplicate entries,
commented-out experiments and keys absent from the runtime tables are omitted.

`settings.def` declares typed defaults, target applicability and comments once.
The runtime reads its defaults directly from these definitions. The native
`tools/configgen.cpp` build tool uses the same definitions and compiled event
tables to emit only the options used by the selected target. In particular, input-only
targets have no rumble section, Underground has no category gains, and UG1 has
no impulse-trigger options.

Build `MW` to inspect the output in `build/MW/config/`. The existing build scripts
and Visual Studio targets compile and run the native emitter before compiling
the ASI. Ordinary builds do not require Python.
Generated INIs and `EventReference.txt` are build artifacts, not source files.
Missing installed INIs are created from the exact generated text embedded in
the ASI; existing files, including their comments and custom bindings, survive.

Release packaging extracts those exact bytes from the ASI, without executing
the DLL or regenerating anything. An embedded manifest fingerprints the text
inputs listed in `inputs.txt`; changes require a rebuild before packaging.
Line endings are normalized for comparison across Windows and Linux checkouts.
