# v1.25.34 Gallery result: compute bundle blocks composition

2026-10-01. Main-menu and save-loading checks passed. During the affected Gallery
cutscene the user saw DLSS Off, matching a recorded suspension. DLSS recovered
after leaving Gallery. The test was stopped intentionally with zero runtime
faults and independently verified native cleanup. The user confirmed a normal
scene and normal game exit; the closure receipt verifies absence and provenance.

## Exact refusal

The source/destination chain reaches the DOF descriptor, but command selection
rejects before shader/binding/resource qualification. The captured signature is
reason 17, detail 0, qualification 0, branch 7 (unsupported opcode), key
`0x78006595`, low-five-bit opcode 6. The stable, completely readable queue sample
contains eight commands under that key, in this order:

1. One opcode-11 state packet.
2. Six opcode-6 compute packets.
3. One opcode-0 composition draw.

Their direct dispatch group counts are `(8,1440,1)`, `(128,5,1)`, `(16,9,1)`
and three instances of `(80,45,1)`. These dimensions do not by themselves identify
the shaders or prove which textures they produce. Raw upper opcode/mode bytes
must not be interpreted as additional arguments; the documented low bits/byte
define those fields.

The new `dof_draws` counter and dependency binding-check counts remain zero.
Thus this run does not exercise the DOF shader adapter, nor establish that its
live resources would pass admission. It is a planner refusal, not a failed SDK
evaluation or a visual validation of the candidate.

The prior v1.25.26 notes already recorded this eight-command shape. The v34
offline plan fixture modeled the composition draw but omitted the six compute
commands. That fixture gap let the candidate pass offline checks without
covering the actual command bundle. The native shader/pixel tests remain valid
for the isolated adapter; they do not substitute for bundle integration.

## Next correction

Extend the fixture to reproduce the complete native bundle and its current
refusal. Capture/map the six compute shader recipes, SRV/UAV dependencies,
extents, coordinate constants and producer order before changing admission.
Determine whether they consume a scene texture that earlier replacements have
expanded. If so, they need an explicit coordinate/input contract as well;
blindly accepting opcode 6 or merely leaving its original shader running is
not sufficient proof. Preserve native order and native/reconstructed selection.

The existing bounded intermediate recorder only triggers reason 6/detail 4,
so it correctly produced no new sample for this reason-17 rejection. Any next
capture must deliberately select this command signature and preserve the
compute states while they are valid. Saved packet addresses belong to reused
frame arenas and must not be assumed to still refer to the captured dispatch.

## Recovery and cleanup

At recovery, 4,010 DLSS evaluations were recorded and the suspension count matched
the resumption count (three each). The user reported a clean normal scene.
Final intentional-stop telemetry and frozen v34 review:

- 4,471 DLSS evaluations, 2,199 skipped dispatch attempts, zero runtime faults.
- All 6,670 main and 2,198 unfiltered GPU recordings completed and released.
- All 4,557 output recordings released; no pending output work.
- All 13,873 jitter changes restored; full render scale restored.
- Forty complete native code spans and eighteen hook heads verified restored
  by read-only live checks. All hooks reported disabled.

Skipped attempts are not a count of displayed bad frames. Full review passing
means lifecycle/telemetry checks passed; the Gallery rendering fix failed this
test. The current DLL stays installed as requested. SDK shutdown remains deferred
under the existing pinned-runtime policy.

Evidence: ignored `local/dof-output-route-20261001`, PID 49484, process creation
134353150299535872. See `gallery-rejection-49484.json`, the Gallery/recovery/stop
checkpoints, `final-review-49484.json`, `stop-native-code-49484.json` and
`session-49484/`. Executable identity and adapter contract are documented in
[dof-output-route.md](dof-output-route.md). Camp investigation remains deferred;
monster effects and the undersized hunter silhouette remain pending.
