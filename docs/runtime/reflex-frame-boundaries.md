# Reflex frame-boundary trace

This short solo/offline test uses the separate `mhw_reflex_boundary_probe`
diagnostic DLL. It does not run DLAA/DLSS, change render resolution, call Reflex
sleep or load Streamline. Its purpose is to establish update/submission/present
alignment before integrating Reflex into the preserved DLSS renderer.

1. Close the game. Preserve the DLL at the exact MO2 `dlssTEST` destination and
   its `.output-dir` sidecar in a fresh repository capture folder. Record both
   hashes before replacing them. Never overwrite the game executable/assets.
2. Install the new probe in that same test slot and select the fresh absolute
   output directory with the sidecar. Use the usual MO2 launch/profile. Keep
   native AA/DLSS Off and the window size unchanged.
3. Reach the main menu, then arm through `tools/reflex_boundaries.py`. The DLL
   validates the retail executable and full native code spans before installing
   any hooks. Read the `armed` record before treating capture as active.
4. Stay at the main menu for the 15-second trace. The worker stops and restores
   the native code automatically, then writes the bounded event log.
5. Review complete pairs, threads, counters, API calls and restoration. If a
   gameplay trace is needed, use another launch and arm after the save is loaded.
   Do not infer a simulation frame ID from a global renderer counter alone.

Control event names are scoped to PID and process creation time:
`Local\MhwReflexBoundaries-v1-<pid>-<creation>-1` arms and suffix `-0` stops.
The tool verifies process creation identity before signaling.

For removal after the game closes, replace the installed test DLL and sidecar
with the exact preserved files recorded in that run's `deployment.json`.
Keep the test DLL installed between diagnostic runs when requested by the user.
The successful v1.25.44 DLSS baseline is separately preserved, not discarded.

See [the implementation and SDK evidence](../research/reflex-foundation.md).
