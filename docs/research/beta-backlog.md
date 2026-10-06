# Deferred beta investigations

## Severe slowdown in the first silhouette correction run

Deferred at the user's request on 2026-10-02. In v1.25.44 the user confirmed that
the hidden-hunter silhouette matches the hunter, but reported "seconds per
frame". The run completed 1,565 DLSS evaluations and recorded 4,909 corrected
indexed alternatives with zero runtime faults and verified cleanup. This is a
functional milestone, not acceptable production performance.

The timed diagnostic snapshots are preserved in the sealed
`local/hunter-silhouette-output-20261002` archive. They are not Present timings
or a CPU/GPU profile; logging, repeated contract validation, pipeline creation
and extra draws are possible costs, not established causes. During performance
work, compare the unchanged diagnostic build with measured reductions of
instrumentation, and measure the correction itself. Keep resource, recording,
submission and state-restoration guards intact. See
[the live silhouette result](hunter-silhouette-route.md).

## Camp-entry reconstruction suspensions after wingdrake

Deferred at the user's request on 2026-10-01. In the v1.25.32 test, wingdrake
travel stayed active and clean, including the reproduced auxiliary texture
replacement. The user associated later brief skips with trying to enter camp
and asked not to focus on that now. Retained samples include dependency
discovery rejection after the successful refresh; the specific dependency
subgate and its causal connection to camp entry remain unproved.

The run recovered, completed 9,246 DLSS evaluations with zero runtime faults,
and stopped intentionally with verified cleanup. Preserve the exact guards
and evidence; no additional camp test is requested now. See
[auxiliary-binding-refresh-result.md](auxiliary-binding-refresh-result.md),
including the separate offline review schema mismatch. This entry does not
mark the previously reported Gallery, attack-effect or hunter-overlay issues
as resolved.

## Future side project: native effect customization

Parked at the user's request on 2026-09-30. The
[native post-processing map](future-native-effects.md) records the identified
motion, haze, grading and final-screen passes, shader identities, evidence,
reusable fixtures and possible customization experiments. This is a separate
future project, not a beta blocker or a request to begin CMFM/settings work.
Continue the shared DLSS output integration first.

## Intermittent depth-copy absence

Status: parked by user decision on 2026-09-27. This is not a prerequisite for
render-resolution control, DLSS upscaling or subsequent frame-generation work.
Preserve the diagnostics and current guards; do not keep preparing new tests
for this item unless new evidence makes it an implementation blocker.

Baseline: [v1.21.5 archived result](native-aa-depth-destination-result.md).
The attachment-comparison defect was fixed and live-tested. The separate
remaining condition produced 87 unfiltered frames across 30 suspension periods;
all periods resumed, zero runtime/SDK faults, and the user reported clean
rendering after area load and walking/panning. The first eight and latest
retained failed lease snapshots were empty. Their upstream cause is unproved.

Evidence remains sealed under
`local/native-aa-depth-destination-20260927/v1.21.5/`, with the tested DLL left
installed. Archive SHA-256:
`9f349ef3d86566f7df4ae0a4c9b4bfcdfab892cb9828641d0de563730194de0c`.

For beta, correlate missing-copy events with recorded loading, menu, cutscene,
camera and gameplay activity. Determine whether unavailable stages explain
them or valid copies are being lost. Retain exact frame/list-generation/state
checks; stale-depth reuse is not a solution. Reopen sooner if the condition
causes persistent deactivation, visible artifacts, crashes, unsafe resource
use, or prevents a new feature from functioning.

## Broader AA coverage and performance

Carry forward extended transition coverage, occasional unclaimed native
resolves, AA exclusivity across all paths, and diagnostic overhead measurement.
The final v1.21.5 checkpoint contains one unclaimed native resolve with no
tracked forwarded request; its exact cause/timing is not recorded. Do not
claim universal AA exclusion or production readiness from the focused tests.

This deferral does not change runtime recovery policy: unavailable inputs
suspend reconstruction; a real failure remains reported and latched. Shared
components changed for DLSS or frame generation still receive focused checks.
The integrated suite then receives broader beta testing and optimization.
