# DLSS Performance + Reflex On live coexistence

Accepted 2026-10-02: combined probe v0.5.1, DLSS path v1.25.44 and Streamline
2.12.0 Reflex On on the same native D3D12 device. This is a game coexistence and
cleanup milestone. It does not measure latency improvement or establish all
rendering effects, presets, resize/recreation cases or combined DLAA stability.

Retail executable SHA256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Installed combined DLL SHA256:
`63bc422cef32bbf4775ac80b779e557ca1bc756259256e618e055e6e47c68637`.
Private evidence: `local/reflex-temporal-20261002/game-02-archive` and immutable
332-file `source-0.5.1`. The exact MO2 backups and removal instructions remain
in the live run folder; the user requested this DLL remain installed.

Main-menu activation, save loading, walking/panning, an area change through a
loading screen and a separate five-second Alt-Tab return all completed. The
user reported DLSS active and a clean scene at each checkpoint. Native AA and
native DLSS stayed Off, external Low Latency Mode was user-set Off, and the
window size was unchanged. Render/output extents were 1280x720 -> 2560x1440.
Pixel readback was not added to this combined game test.

Final joined review:

- 8,755 successful DLSS evaluations and 10,313 complete Reflex frames.
- One token and Sleep call per Reflex frame; 61,878 correctly ordered markers.
- 64 complete ordered driver reports aligned with the final integration IDs.
- Effective driver Off -> On -> Off; final Off observed in one query, 0 ms.
- Zero temporal, frame, presentation or request-collision faults.
- 8,759 temporal GPU signals completed; all private recording references
  retired/released, no pending callbacks, GPU work or retained route resources.
- All 9,534 jitter injections restored and full render resolution restored.
- Temporal drain preceded Reflex/presentation drain and Streamline shutdown;
  Streamline pins released, intentional process-lived NGX ownership preserved.

Counts need not match: Reflex covers complete engine frames while temporal
reconstruction qualifies rendered scene stages. Four skips were logged across
the whole capture: two by the main-menu checkpoint, three by save loading,
still three after area/focus checks, and four after intentional stop. Three
suspension periods resumed; no unrecoverable resolve or permanent shutdown
occurred during the observed checks. This is not a claim of zero unfiltered
transition frames or complete post-processing coverage.

Independent bounded live memory checks, before normal exit, verified 17 Reflex
API/presentation/frame heads, 20 temporal native heads and 46 full native
functions restored. The host swap chain remained native. They used 778 reads,
75,163 bytes and zero process-memory writes. Temporal binding-observer cleanup
is additionally checked by its telemetry; those COM observer heads are not
part of the independently compared 37-head list.

The user confirmed a normal scene after stop and normal exit. Retail executable,
original assets, runtime identities, profile/modlist and saved graphics settings
were preserved. Exact archive hashes and source manifest are in the result.

The rejected v0.5.0 attempt remains in `game-01-archive`: zero DLSS/Reflex frames,
ambiguous compound anchor failure and independently verified cleanup. v0.5.1
qualified its anchors on its first read (no retry needed); this successful run
does not prove the earlier failure was a counter race. The new diagnostic and
bounded setup retry contract remains strict on device, links and readable banks.

48 Python review tests and 11 native tests pass, with clean /W4 /WX builds.
Previously accepted 96-frame DLSS/240-frame DLAA owned coexistence results and
standalone Reflex/temporal milestones are preserved. Combined DLAA game testing,
broader effect/recreation tests and performance cleanup remain separate work.

Next: ordinary frame-generation integration before MFG. Establish real frame
IDs/constants, color/depth/motion/UI resource tags and their ownership/lifetimes,
then add SDK-generated presentation with the existing Reflex frame boundaries.
Keep diagnostic coverage during implementation; presets, UI and performance
cleanup remain deferred as agreed.
