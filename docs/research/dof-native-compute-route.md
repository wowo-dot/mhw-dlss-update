# Gallery native compute integration — v1.25.41

The candidate admits the captured six-compute Gallery sequence into the existing
full-resolution output plan. It preserves the game's dispatch scheduling, resource
allocations, SRV/UAV views, constants, samplers and barriers. Only two compute PSOs
have private adapted alternatives. Live Gallery behavior remains unverified.

This replaces the proposed second set of owned blur buffers. Preserving the
already-bound native views avoids reconstructing descriptors whose contents have
not been independently captured. It also keeps the full native depth allocation,
including the outer pixels shown to affect the blur in the
[captured-depth replay](dof-live-depth-replay-result.md).

## Rendering contract

The completed queue must contain six adjacent compute packets followed immediately
by the same-key DOF composition draw. All six shader hashes, technique IDs, binding
words, owner modes, texture identities, constants, dispatch groups and composition
inputs must match the captured recipe. Nine distinct resources must have the
expected dimensions, formats, device, mip count, plane count and access flags.
Admission is limited to the validated 2560x1440 output / 1280x720 render case.

The passes remain in native order:

1. Horizontal depth reduction: adapted branch adds the group barrier validated
   offline; native branch retains the original shader.
2. Vertical reduction: unchanged native dispatch.
3. Neighborhood reduction: unchanged native dispatch.
4. Preparation: adapted branch averages each 2x2 reconstructed-color footprint;
   depth and the remaining calculations keep their native inputs and constants.
5. Blur: unchanged native dispatch.
6. Image rewrite: unchanged native dispatch, including its chronological overwrite.

The existing full-size composition adapter consumes these native auxiliary
textures. The six compute nodes participate in the same frame handoff as the
temporal resolve, copy and subsequent effects. For passes 0 and 3, native and
adapted Dispatch commands use the existing immutable frame selector; one branch
executes image work. Other passes execute once. There are no new resource barriers,
descriptor tables or descriptor substitutions in this route.

Private PSOs warm on native frames. Root signature and shader-set identity must
remain stable. At dispatch, the route validates the native packet, caller,
command-list generation, bindings and disabled native predication, then restores
the prior PSO and predication. Local tracked state contradictions reject the
frame; inherited native state is not replaced with an invented transition.

## Hooks and lifetime

No hook target was added. The existing Windows x64 compute executor at RVA
`0x259cc20` supplies the executor, backend context and packet, and the existing
`ID3D12GraphicsCommandList::Dispatch` hook supplies the list and three group counts.
The qualified API caller is RVA `0x259cfca`. Native entry/return and observer
contracts remain those documented in the existing compute diagnostics.

Supported retail executable SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Research executable SHA-256:
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
These identities and the existing native code gates are retained in preparation
and runtime receipts. Original executables and game assets are unchanged.

Every participating resource, descriptor heap, root signature and PSO is retained
for the recording. Fence completion alone does not release those references:
the command-list generation must also retire through Reset. The shared selector
cannot change after submission. Unsupported preparation keeps the native route;
an unexpected recording invariant uses the existing controlled stop and cleanup.

## Verification

`dof_native_route_fixture` executes the production native packet parser,
qualification, dispatch callbacks, binding observer, submission validation and
lifetime retirement. Both captured full-depth images and their paired constants
pass on NVIDIA and WARP with GPU validation enabled. All seven intermediate
images and final pixels exactly match the independent reference, which also
contains the proven horizontal synchronization correction. Color is synthetic.

The fixture checks seven malformed recipes, a changing packet state-list pointer,
late full-resolution selection, all six dispatch receipts, both PSO restorations,
and actual GPU fence plus Reset retirement. It preserves all 3,686,400 depth values.
Sample 1 retains sensitive wrong-color, stale-writer and cleared-outer-depth
controls. Opaque native handle fields in this fixture are synthetic; actual GPU
descriptors are supplied separately by the fixture. This is not a claim that the
real game's descriptor contents have been decoded.

The complete native queue discovery at the live 2560x1440 configuration still
needs the game test. Existing plan regression verifies unsupported/incomplete
compute sequences remain rejected. The continuous route regression passes 96
frames, 288 native effects and 786,432 exact output pixels. Sixty targeted CTests
and 105 Python tests pass, as do historical v40 and synthetic v41 full-log review.
The latter rejects a missing new telemetry channel. Final builds are warning-free.

Initial fixture build/setup failures and one missing test-command argument were
corrected; their logs remain alongside passing results. Evidence is under
`local/dof-native-compute-route-20261001`; private shader and image inputs stay
untracked. New `dof_native_sequence` schema 1 reports qualification, warmup,
per-pass dispatches, adaptation, restoration, completed sequences and rejection
receipts. Existing capture channels remain available.

## Deployment and next test

The existing MO2 test slot is the destination. `deployment.json` records whether
installation completed and identifies the exact prior DLL and output-sidecar
backups. `deploy.ps1 -Action Restore`, with the game closed, restores those files;
the previous output root is sealed, so a new output directory is required before
launching a restored version. Settings and profile contents are not changed.

Next: main-menu baseline, loaded save, affected Gallery cutscene, gameplay recovery,
then announced stop and cleanup verification. Check actual six-pass admission,
adapted dispatches, composition and visual quality together. Keep the candidate
installed after normal exit as requested. Gallery remains ahead of monster effects
and the hunter silhouette; camp-entry behavior remains deferred.
