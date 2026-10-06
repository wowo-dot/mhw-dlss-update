# Agreed implementation milestones

## Active sequence, revised 2026-10-02

The user approved proceeding with the current DLSS Performance baseline:
**Reflex, then ordinary frame generation, then MFG**. Other quality ratios and
broader polish/optimization are deferred. Player-facing settings still wait for
CMFM. Keep the diagnostic tools useful during implementation.

Gallery blur, radial effects and the hunter silhouette have passed their focused
tests; v1.25.44 is the preserved renderer baseline. The signed Streamline 2.12.0
Reflex/PCL foundation passes its owned DX12 fixture. A separate bounded native
frame trace is prepared to establish real update/submission/present alignment;
Reflex is not yet active in game. See [Reflex foundation](reflex-foundation.md).

This sequence supersedes the older quality-ratio-before-Reflex ordering below.
New crashes, broken output and unsafe lifetime behavior remain immediate defects.

## Active sequence, confirmed 2026-10-01

The user approved this order after the targeted wingdrake fix:

1. Correct the offline command-qualification reviewer and test it against saved
   evidence. Preserve original archived results.
2. Complete remaining post-processing integration: investigate the Gallery
   intermediate color pass, then reported attack effects and hunter-silhouette
   scaling. Use resource dependencies and coordinate contracts for fixes;
   do not infer that all reported effects share the same failure.
3. Establish supported DLAA/DLSS dimensions and quality ratios, including resource
   and history recreation; current 1x/2x adapter coverage is not arbitrary-ratio
   support. Reuse shared lifetime/recovery components. CMFM/settings UI waits.
4. Run a broader regression of those changes, then proceed to Reflex and FG/MFG.
5. Keep diagnostics during integration; broader beta work and measured performance
   cleanup follow. Camp-entry skips remain explicitly deferred for now.

Step 1 passed. Step 2 captured the Gallery intermediate pass twice and identified
its depth-of-field composition and optional vignette. Recovery and cleanup passed;
the Gallery rendering fix remains open. The v1.25.34 DOF composition candidate
passes offline shader, dependency and regression tests. Live testing found six
preceding compute commands absent from the plan fixture; the route still
suspends before composition. v1.25.35 adds the complete sequence as a regression
case and a bounded compute/draw recorder. Capture the missing producer shaders
and bindings before implementing their coordinate/resource contracts; see
[the compute bundle plan](dof-compute-bundle.md). The v1.25.35 live capture
succeeded twice: all six CS shaders, composition and ordered SRV/UAV edges are
saved. Gallery still suspends. Next establish constants and external depth/color
coordinates in a complete GPU fixture; see [the result](dof-compute-bundle-result.md).
See [gallery-intermediate-result.md](gallery-intermediate-result.md) and
[dof-output-route-result.md](dof-output-route-result.md).

## Active sequence, revised 2026-09-27

User decision after the v1.21.5 live test: park intermittent recovered DLAA
skips and extended polish for beta; proceed with render-resolution control,
then modern DLSS upscaling, then Reflex, then frame generation/MFG. Retain diagnostics and
the archived working DLAA baseline. Do not resume depth-copy investigation as
a prerequisite unless it causes a concrete blocker in the new implementation.

1. Control actual scene render resolution while retaining display-resolution
   output. Separate texture allocation, active rectangle and output dimensions.
2. Connect modern DLSS using qualified lower-resolution inputs, correct motion
   and jitter coordinates, and a full-resolution downstream handoff.
3. Integrate Reflex at the real simulation/render/present boundaries and verify
   frame-ID alignment and pacing before enabling frame generation.
4. Add frame generation/MFG as its own integration milestone.
5. Beta-test the integrated suite with longer sessions, transition/compatibility
   coverage and recordings; then address deferred polish and performance work.

Crashes, broken output and unsafe resource handling introduced or exposed by
each implementation step remain immediate defects. Focused verification of new
code continues. Player-facing settings integration still waits for CMFM.

Reflex was made explicit after the user's follow-up on 2026-09-27. NVIDIA's
[Streamline DLSS-G guide, section 8](https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuideDLSS_G.md#80-integrate-sl-reflex)
requires Streamline Reflex and matching present-marker/common-constant frame
indices. A standalone non-Streamline Reflex integration does not satisfy that
DLSS-G route. This dependency does not require replacing the working NGX DLAA
implementation now; select and pin the integration stack when that milestone
starts.

See [render-resolution starting map](dlss-render-resolution-control.md) and
[parked beta investigations](beta-backlog.md). v1.21.5 stays installed; its
6,521-frame run is archived and the game is closed.

The historical prerequisites below are superseded where they conflict with
this sequence. They are not instructions to complete polish before DLSS.

## Earlier checkpoint, 2026-09-27

The current native-stage prototype is v1.19.0: qualified TAA resolves are replaced
by DLAA, with per-invocation unfiltered recovery and no native retry after a claimed
failure. It passes offline checks but is not deployed or live-tested. This is a
new implementation, distinct from the never-deployed fallback draft mentioned in
the historical decision below. See the
[implementation result](native-temporal-resolve-implementation.md).

Before moving the main implementation effort to DLSS: verify the native-stage
bypass in game; complete renderer-side AA exclusion and failure/suspension handling
across unqualified frames, with one jitter owner and correct sharpening/output
routing; then check sauna, loading and focus transitions. A scoped diagnostic may
temporarily select TAA for preparation; that is not the final independent AA mode.
Retain diagnostics and defer broad optimization/release work.

DLSS then requires separate render/output dimensions: actual lower-resolution
scene rendering, qualified color/depth/motion inputs at their resulting extents,
correct jitter/motion scaling, full-resolution output/downstream routing and
history/resource recreation when dimensions or mode change. Current inline SDK
creation and handoff still assume equal input/output dimensions.

All player-facing settings, warning-popup and AA-control-lockout work waits for
the user's CMFM framework. It is not a gate for these rendering diagnostics.

## Revised AA ownership decision, 2026-09-26

The user superseded the native-TAA fallback proposal below. The next milestone
is **independent AA selection and AA-Off recovery**:

- The mod chooses Native / Off / DLAA from its own configuration; later DLSS is
  another exclusive mode. DLAA must not require TAA selected in game Options.
- Owned DLAA/DLSS modes suppress native TAA and FXAA. Native mode restores the
  user's normal selection. Engine preparation may be reused without running its
  AA resolve. Do not silently change the saved selection to TAA as the solution.
- Transient input absence (such as loading) suspends reconstruction. Resume only
  with qualified resources and fresh temporal history.
- Actual failure latches the feature disabled, restores scoped changes, and
  returns to unfiltered rendering at a safe renderer boundary. Report the cause;
  do not silently substitute TAA, automatically retry every frame, or throw away
  already-recorded GPU work. Explicit retry/mode change or restart can re-enable.
- Keep existing diagnostics; DLSS upscaling and then FG/MFG follow this milestone.

See [implementation foundation and pending live control](independent-aa-foundation.md).
The original runtime remains v1.18.4; the new controller/GPU handoff are not yet
connected to it. The unfinished v1.19.0 TAA-fallback edits were preserved locally
and removed from the runtime after this decision. They were never deployed.

## Earlier proposal (superseded where it conflicts with the above)

User decision, 2026-09-26: retain diagnostic hooks and telemetry while implementing
the DLSS feature suite, then do broader cleanup, optimization and release work.
Add native TAA bypass before considering the experimental DLAA milestone complete.
No runtime implementation or deployment is changed by this plan.

1. Diagnose and fix the pending TAA request lifetime failure from the mixed
   windowed/focus/area-transition run. Validate the specific fix with callback
   fixtures and repeated, separately observed area transitions.
2. Bypass the native TAA resolve on frames successfully handled by DLAA, using a
   routing contract that can also serve DLSS upscaling. Retain native rendering
   as the fallback and keep diagnostic instrumentation.
3. Mark DLAA functionally implemented and experimental after focused correctness
   checks. Automatic configuration/UI polish, broad compatibility testing and
   major performance optimization are not gates for beginning DLSS work.
4. Implement DLSS upscaling, then frame generation/MFG as separate milestones.
   Continue testing each changed shared component and relevant transitions.
5. Clean up and optimize the integrated feature suite, add normal user controls,
   and perform broader quality, stability and release validation.

## Native TAA bypass contract to implement

Current `dispatch_call` always forwards the original TAA Dispatch, then attempts
DLAA. The selected current-color and motion inputs are copied before that native
dispatch. Successful DLAA overwrites the selected native output, so the redundant
native dispatch is not evidence that DLAA is consuming an already TAA-resolved
image. Removing it is useful, but its share of the measured cost is unknown.

Keep the engine's TAA setup enabled where it provides required inputs, bindings
and dispatch placement. Bypass only the qualified resolve dispatch after proving
that the replacement path owns a complete, matching frame. Do not globally switch
the in-game TAA setting Off as a substitute for this integration.

Required cases:

- Successful replacement: DLAA produces the required native output, native TAA
  dispatch count is zero for that selected invocation, and bindings/resource
  states remain correct for downstream game rendering.
- Disabled, unsupported, missing-input or preflight-rejected frame: native TAA
  runs exactly once and there is no incomplete replacement output.
- Failure after command recording begins: prove the native state/output and
  fallback ordering are recoverable. D3D12 command recording cannot be assumed
  to roll back. Preserve or explicitly reset/reseed native temporal history when
  returning to fallback after bypassed frames.
- Keep unrelated dispatches unchanged. Retain per-frame identity, GPU lifetime,
  scoped jitter, resource validation and history-reset checks.

Add bounded counters distinguishing replacement, bypass and fallback. Validate
routing counts with owned callback fixtures; extend GPU fixtures for successful
output/binding equivalence and meaningful forced-failure paths. Then perform a
focused live comparison. Keep the existing diagnostic capabilities available;
do not fold broad optimization into this step or promise an FPS improvement
without measurement.


On 2026-10-01, v1.25.36 completed the live constant capture. Both paired bundles passed: compute and composition CBScreen bytes are identical despite different addresses, with 2560x1440 allocation and 1280x720 view. Animated blur/vignette parameters are preserved per frame. Recovery/cleanup and normal exit passed. Gallery admission remains unchanged; next replay the complete pipeline with the captured constants and explicit valid input footprints. See [DOF constants result](dof-pipeline-constants-result.md).
