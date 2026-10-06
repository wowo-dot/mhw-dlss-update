# Nine-frame live DLAA capture accepted — 2026-09-26

Run 14468 with v1.15.3 passes the complete frozen reviewer. Nine adjacent
frames, 60795–60803, produced private full-resolution DLAA outputs using one
feature. History resets on the first frame only. Seven interior projection
jitter samples match their SDK input receipts, and all seven camera mutations
were restored. Both packet-address reuse generations pass their lifetime checks.

## Recorder and SDK evidence

The new shutdown ordering resolves the journal overflow in this run. Dependent
callbacks drained at tick 41900703, journal admission closed at the same tick,
and the first hook-removal phase began afterward within that tick. Hook removal
and cleanup finished at 41901546, 843 ms later. The journal contains 1,335 events rather than
exhausting its unchanged 4,096-event budget. Its fault and pending counts are zero.
All 39 observed depth consumers have valid markers and marker failure zero.
Three additional requests were unconsumed after cancellation; they are counted
and do not replace any selected frame's qualified copy.

All eleven native hooks are disabled, with no pending callbacks. Selected
color/depth/motion copies have independently checked command submission,
completion and recording retirement. GPU conversion matches the independent
reference for 232,243,200 float values. All 99,532,800 SDK output RGB values were
checked. These checks establish transport and evaluation, not perceptual quality.

The SDK reused verified startup ownership with no second initialization. One
CreateFeature and nine EvaluateFeature calls succeeded, followed by feature
release and parameter destruction. Borrowed-device shutdown remains deferred
until process exit. Native bindings and the displayed native TAA path were
unchanged. GPU validation was used in the preparation fixtures, not enabled
inside the running game.

## Image inspection and next step

The camera view matrices are bit-identical across all nine frames. Fixed-scale
previews of the first, middle and final frames preserve the scene, and enlarged
rope, rail, armor and floor crops show smoother edges with some texture softening.
No obvious large displacement or doubled silhouette is visible in those crops.
This limited static inspection does not establish moving-camera ghosting or
long-term stability. The comparison is pre-TAA scene color against private DLAA,
not the game's native TAA output against DLAA.

All previews use the same nonnegative Reinhard mapping followed by sRGB and
nearest-neighbor enlargement for crops. No per-image normalization is applied.
Input RGB maxima range from 35.5 to 89, while output maxima range from roughly
4.0 to 4.54. Reconstruction substantially suppresses isolated high values;
highlight fidelity is not established by these previews. Analysis artifacts
are under `local/dlaa-sequence-analysis-20260926/v1/`. Its ten-file archive has
SHA-256 `c90680b896e78105db6c69c485a03503e77a2d2734ba459034da3b0ebb3f4ace`.

Next is a bounded presentation integration: convert the SDK output to the
native color format, establish GPU ordering before its downstream consumer,
and preserve native rendering on missing resources or failed invariants.
The present evaluator runs after capture and unhooking; it cannot simply be
switched on for continuous display. Persistent history, actual frame timing,
scene-change resets, camera motion and display/device recreation remain work
before usable continuous DLAA. Resolution-changing DLSS comes afterward.

## Identity and closure

Evidence: `local/dlaa-sequence-live-20260926/v1.15.3/`.
Process creation: 134348456500809665. Host v0.0.0.0 SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
DLL SHA-256:
`b79cdd0e9bc829cc6872bd05323cfed7f54d677e553f3cb8dc7930907b99fcf3`.
Capture SHA-256:
`7901b0062954b1c115c4a5f9efe3f5cdd611541973dd91a54bf5738f024d0c22`.
The 641-file prelaunch manifest has SHA-256
`c38483cade86a1f0bb4a2942dd5322c10c616b80c1736e79dd6d3cec4c2f03cd`.
All 75 runtime evidence files were preserved before termination.

The user was away from the computer and explicitly authorized terminating the
game. The exact process was terminated only after capture completion and hook
shutdown; this is not a normal-exit test. The previous diagnostic DLL was
restored at 00:44:22 +03, the output sidecar was removed, and all four
configuration hashes are unchanged. No next diagnostic is installed.
The final archive contains 722 files, SHA-256
`2b3f963a6a96f3691fb85c57ea1213bac55f2a45ad41a04083d5e63c1de70618`.
