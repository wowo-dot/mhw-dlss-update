# Depth timing during camera motion, run 31044

Captured and closed 2026-09-25 using the **unchanged v1.9.0** DLL from
[stationary run 25300](ordinary-taa-depth-pixels-result.md). The frozen reviewer
accepts all three depth/color/motion sets for frames **22592–22594**.
The moving-scene content comparison supports **current depth/color association
in the tested static scenery**, rather than only agreement on a still image.
It does not certify SDK input views or produce DLAA output.

## Stimulus and capture

The user reported panning and subsequently clarified the motion was right and
up rather than purely horizontal. This is a valid stimulus; no rerun is needed
for that reason. DLSS remained Off, TAA On, DX12 On, fullscreen 2560x1440.
All decoded color, depth and motion values are finite. Every motion pixel is
nonzero. Whole-image median history offsets are approximately (5.74, 0.05),
(21.15, 0.25) and (20.98, 0.26) pixels, with spatially varying vertical motion;
the maximum absolute component is 62.97 pixels. These are image history offsets,
not direct measurements of controller input or camera angular velocity.

The three depth files are non-flat, collectively span approximately
[0.01354724, 0.06913050], and contain no zeros or ones. They and the color/motion
files total **132,710,400 bytes**. Depth is the matched R32_FLOAT native-copy
destination, not ordinary TAA's distinct 1x1 RGBA8 named depth fallback.
Copy-point state restoration, exact TAA packet/list/allocator/Reset association,
actual submission, private fence completion and recording retirement verify.
No projection/descriptor changes or SDK calls were added.

## Content timing measurement

Five fixed analysis regions were chosen from the color preview. The initial
measurement is preserved. Inspection of its mask preview showed water and a
weapon tip touching analysis regions, so a second measurement conservatively
expands actor exclusions and masks the water. It does not change raw captures,
the frozen reviewer or initial results. The final measurement is
`depth-timing-31044-v2.json`; its interpretation is
`timing-analysis-31044-v2.json`.

In each remaining region, select the strongest 0.5% of log-depth gradients,
requiring magnitude at least 0.002 and 64 edge pixels. Measure absolute color
luminance gradient along depth-edge normals. Search same-frame integer offsets
from -6 to +6 in each axis, compare all three color frames at zero offset, and
retain four displaced controls at +/-32 pixels. An owned translated rectangle
checks indexing and sign. Depth alone selects the edges; agreement with color
does not select or reject individual samples.

The bench region retains only 11 selected pixels after exclusion and is
unqualified in every frame. All **12 qualified frame/region combinations**
peak at **(0, 0)** and prefer the matching color frame over both alternatives.

| Qualified region | Edge pixels per frame | Matching / best other color, 22592 | 22593 | 22594 |
| --- | ---: | ---: | ---: | ---: |
| Inner frame excluding actors/water | 6,597 | 4.04x | 1.94x | 2.98x |
| Upper room | 1,214 | 3.23x | 7.06x | 3.66x |
| Right pillar | 624 | 3.34x | 3.25x | 3.45x |
| Left floor | 2,233 | 1.42x | 1.53x | 1.70x |

Matching-frame scores also exceed the strongest displaced control by
1.38x–3.90x. These are edge-score ratios, not confidence probabilities. The
regions overlap and are coverage checks rather than independent samples.
The result supports spatial agreement on the pixel grid and rejects the tested
adjacent-color-frame alternatives for this moving scene. It does not establish
subpixel accuracy, physical depth linearization, independently moving-object
behavior or every possible common-latency interpretation.

## Motion control and limits

Warping the previous color gradient using current history vectors improves the
edge score over zero and reversed motion in all eight qualified comparisons.
However, frames 22593 and 22594 have similar motion fields and exchange best
edge scores. This metric does not independently distinguish their exact vector
frame. Preserve the earlier [run 43820 independent color-image matching
calibration](ordinary-taa-paired-result.md); do not claim this run repeats its
quantitative displacement verification. No raw-depth equality after a 2D warp
is assumed, since camera motion can change projected depth values.

The final analysis records scoped depth/color spatial and frame-association
support. Broad frozen-review content-alignment and DLAA-ready flags remain
false. Complete final-writer coverage, typed views and the state/lifetime
contract at the intended SDK evaluation point remain outstanding.

## Closure and provenance

The capture has 1,340 journal events: 541 Reset, 541 Close, 237 Execute,
18 native-copy markers and three TAA markers. Final selected completion occurs
218 ms after capture start, with no later journal event. All faults and pending
counts are zero; hooks are disabled. Metadata reference releases: 24 producer,
three TAA consumer, 54 depth-route and 303 lifecycle. Issued readback references
retain their existing process-lifetime policy.

Of 21 requests and 18 matched copies, 15 copies have complete witnesses. Three
unselected tail copies in frame 22597 have no observed Execute or retirement;
three requests have no captured consumer. The 134 unmatched consumer calls do
not establish exhaustive producer coverage. All selected-frame checks pass the
unchanged reviewer and its pretested bounded-tail policy.

User confirmed normal exit. The previous DLL was restored at
**18:09:05 +03**, temporary output sidecar removed, and all four configuration
hashes remain unchanged. No next test is installed or armed. The 147 prelaunch
files and 132 frozen sources verify. No new binary build or native test suite
was needed: this run uses the exact artifact with 115 native / 55 Python checks
(nine jitter-only skips) already recorded before the stationary capture.

Private evidence: `local/temporal-depth-pan-20260925/v1.9.0/`. It includes all
nine raw images, previews, original and revised measurements, frozen source,
deployment/restore receipts and closure/archive manifests. Log is 319,592 bytes,
SHA-256 `c203c45b8f956f0e3c81251bbaf4bf57b4462b4db9af784703e2b65c70592457`.
Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
DLL SHA-256
`61c643d62ce6cde8d7e76e7dd70b53a4ca240e7bad82b42af6d2a4c7cbdbae24`.

## Next step

Proceed to actual typed input views and the resource state/lifetime contract
at the proposed native-resolution evaluation point. The evidence does not
require another depth-timing run. The native-copy state interval is verified;
do not assume the same candidate is shader-readable at the later TAA callback.
Resolve that contract, including whether a private owned depth copy should
carry the validated contents to evaluation, before bounded SDK input assembly.
