# Gallery DOF constants captured (v1.25.36)

Run on 2026-10-01, PID 23216 / creation 134353192342693257.
Evidence: `local/dof-pipeline-20261001`. Installed DLL SHA-256:
`354128a6bdaa91ac488cb3c5919f7f0a404948010990f038ac8abd05d61d56b7`.
See the [capture and offline fixture contract](dof-pipeline-constants.md).

## Result

Two complete paired bundles passed at frames 11342 and 11402, in two attempts
with zero capture errors. Both preserve `[11,6,6,6,6,6,6,0]`, the six compute
shader hashes and the VS/PS from v1.25.35. Each includes eleven typed constant
bindings. Extraction produced eight private shaders and five unique bounded
constant files, with every file hash verified. CPU dependency analysis again
finds eleven internal read-after-write edges and four external reads per bundle.

The final draw's CBScreen allocation has a different address from the compute
passes, but **all 112 bytes are identical**, in both captured frames. Address
differences did not imply different screen values. The captured contract is:

| Field | Value |
|---|---|
| fScreenSize | 2560 x 1440 |
| fScreenInverseSize | 1/2560, 1/1440 (float precision) |
| iViewOffset / fViewOffset | 0, 0 |
| iViewSize / fViewSize | 1280 x 720 |
| fViewInverseSize | 1/1280, 1/720 (float precision) |
| Checkerboard | 0 |

CBScreen SHA-256:
`b1cdc0a1dfb548b720b5aa23646508365e6e9c15938f9be0d852089097572522`.
Its content-scale fields are `[0.5,0.5,1,0.5,2,1,2,1]` in reflected offset order.
Each frame's four CBNewDOFFilter bindings are byte-identical to one another;
the two projection bindings also agree within each frame.

The artistic values change between frames. CoC bias/scale change from
approximately `0.000576643/-0.010814094` to `0.001490637/-0.017350690`.
The vignette switches from disabled to enabled and its power/color also change.
The single-pixel radius is 0.0078125, aspect is 0.5625, and the CoC threshold
is approximately 1/720. The Z-to-linear parameters change with the projection.
These are per-frame inputs to preserve, not constants to bake into an adapter.

## Implications and limits

The shaders explicitly distinguish allocation size from view size. In pass 4,
doubled dispatch coordinates are clamped to `iViewSize - 2`, depth gathers use
`fScreenInverseSize`, and scene color is read with integer 2x2 loads. Meanwhile
passes 1/2 use fScreenSize bounds, and the captured dispatches still cover the
display-sized reductions and 1280x720 half-allocation intermediates. Replaying
all passes with one indiscriminate scale change would not establish correctness.

The new evidence establishes CPU constant bytes and their command association.
It does **not** prove actual texture contents/valid pixel footprints, resource
states, or native GPU format equivalence. No captured live pixels were fed to
the synthetic fixture. Gallery still suspends reconstruction at the compute
admission gate; no live DOF replacement draw ran. This was a successful
diagnostic, not the Gallery rendering fix.

Next use these captured constants in the complete offline six-compute fixture
with matching allocation/view dimensions. Compare external depth and scene
sampling against a reference with explicit valid footprints, verify native
formats/states before live execution, and preserve all internal producer order
(especially the Image overwrite by unpack). Adapt the coordinate contract
without modifying the animated artistic parameters. Do not merely allow opcode 6.

## Runtime and cleanup

Main-menu checkpoint: 1225 DLSS frames, two startup skipped dispatches, zero
faults. Loaded save: 2285 frames and no additional skips. Gallery suspended as
expected while both captures completed; the user confirmed clean recovery.
Recovery checkpoint: 3095 DLSS frames, waiting false, two suspensions and two
resumptions. Intentional stop: 3424 DLSS frames, 2327 skipped dispatches, zero
runtime faults. Skipped dispatches are not a displayed-frame count.

All 5752 main GPU signals completed and all 5752 recordings were released.
All 2327 unfiltered recordings completed/released; all 3447 output recordings
were released. All 13044 jitter edits were restored, full render scale returned,
and all hooks were disabled. The frozen full reviewer and all eleven component
reviews pass. Independent live reads verified forty full native code spans
and eighteen hook heads restored, without process-memory writes.

The collector exited at native termination: 38 events/snapshots, 7,623,994 bytes,
zero read retries, no faults. Timeline SHA-256:
`dfa9eb4972e3e80474a033a2d1421ad43e3ca1e71cb6872f1071bfbd4ac1592c`.
The user confirmed a normal scene and normal exit. The closure verifier checks
both processes are absent, all four configurations unchanged, the installed
DLL/backup/sidecar hashes, and all prelaunch evidence before sealing the archive.
Keep this DLL installed; choose a fresh output root before another launch.
Camp remains deferred; monster effects and silhouette follow Gallery.
