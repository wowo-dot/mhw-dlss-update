# Paired scene-depth pixels, run 25300

Captured and closed 2026-09-25 with recorder **1.9.0**. The frozen reviewer
accepts all three depth/color/motion sets for frames **11884–11886**. Depth
pixels now come from the separately identified scene-depth candidate, rather
than ordinary TAA's named 1x1 fallback. This verifies readback and request/frame
association; the content analysis below has a narrower spatial conclusion.

## Copy and lifetime evidence

The selected view+628/container+A0 candidate is 2560x1440 R32_FLOAT with
ALLOW_RENDER_TARGET, one mip/slice/sample. Its matched native CopyResource
source is R32_TYPELESS with ALLOW_DEPTH_STENCIL. Each selected frame again has
three native copies, all with complete Reset/Close/Execute/retirement witnesses.
The observer selects the copy on TAA's own command-list generation, with source
COPY_SOURCE (0x800) and destination COPY_DEST (0x400).

Immediately after that native copy returns, the observer transitions the
destination to COPY_SOURCE, copies it to a private readback buffer, then restores
COPY_DEST. The shared journal orders this before the same frame's TAA Dispatch
and existing color/motion readbacks. Exact request stamp, candidate identity,
TAA packet, command list, allocator and Reset generation match. All three
private completion fences and later recording-generation retirements verify.

Nine distinct readback allocations produce nine hash-verified files, each
14,745,600 bytes, totaling **132,710,400 bytes**. Each depth footprint has 2560
columns, 1440 rows and 10,240-byte pitch. No descriptor, projection or shader
changes and no SDK calls were made. Bounds remain ten hooks, 27 observer GPU
commands, three private Signals and zero observer Execute calls.

The candidate still has no independently witnessed state at the later TAA
consumer itself. The proven state interval is the selected native-copy callback.
This distinction matters for any later SDK injection point. Complete writer
coverage and a final-writer guarantee remain unproven; same-batch list ordering
is not treated as whole-list serialization.

## Pixel contents and spatial comparison

All decoded depth, color and motion values are finite. Depth is non-flat and
within [0, 0.068810001]; per-frame median values are approximately
0.01354925, 0.01354783 and 0.01354605. Only 5,104 / 5,101 / 5,114 of the
3,686,400 pixels are exactly zero. No pixel equals one. About 93.04% of motion
pixels are zero under the stationary-camera request. The range and visible
near/far ordering are consistent with inverted depth; near/far distances and
projection interpretation are not derived from the display mapping.

All three color/depth pairs were inspected side by side. Character, equipment,
floor and room geometry have corresponding silhouettes. A fixed log mapping
is used for viewing depth, shared across all frames; this is not linearized
distance or an SDK preprocessing transform.

An additional offline edge test selects the strongest 0.5% of log-depth
gradients in five fixed regions: the inner frame and its left, right, upper and
lower halves. It measures absolute color-luminance gradient along each depth
edge normal, searching integer offsets from -6 to +6 pixels in both axes.
Selection uses depth only. An owned synthetic translated rectangle checks the
search direction. Four 32-pixel shifts provide deliberately displaced controls.

All **15 frame/region combinations peak at (0, 0)**. Whole-frame zero-offset
scores exceed the strongest displaced control by **3.99x, 3.96x and 3.99x**;
the half-frame ratios range from 1.97x to 6.30x. This supports pixel-grid spatial
agreement in this scene. The regions overlap and are not independent samples.
Texture and lighting edges differ from depth edges. This stationary capture
cannot discriminate a stale background frame, quantify subpixel accuracy or
establish temporal alignment during camera motion. Broad content-alignment and
DLAA-ready flags remain false in the untouched frozen review and analysis.

## Capture closure

The journal contains 1,145 events: 461 Reset, 461 Close, 205 Execute, 15 native
copy markers and three TAA markers. Final selected completion is 219 ms after
capture start; no journal event follows that completion. All faults and pending
counts are zero, with hooks disabled. Released metadata references are 24
producer, three TAA consumer, 45 depth route and 261 lifecycle references.
Issued readback references retain the existing process-lifetime policy.

There are 17 requests and 15 matched native copies: 12 qualified copies and
three explicitly unqualified tail copies in frame 11888, whose Execute and
retirement fall outside the recording. Two requests have no captured consumer.
The 113 unmatched consumer calls do not establish exhaustive producer coverage.
The frozen v1.9 reviewer accepts its pretested selected-frame scope while
preserving these tail limits; the previous v1.8 rejection is not rewritten.

User confirmed normal exit. The exact previous DLL was restored at
**17:51:41 +03**; the temporary output sidecar was removed and all four saved
configurations are unchanged. No next test is installed or armed. The original
game executable/assets were not changed.

Private evidence: `local/temporal-depth-pixels-20260925/v1.9.0/`, including
`review-25300.json`, `analysis-25300.json`, `spatial-25300.json`, all nine raw
images, full-resolution previews and `triplets-preview-25300.png`. Prelaunch
contains 174 sealed files, including 132 frozen source files. The preparation
suite passed 115 native checks and 55 Python tests (nine jitter-only skips).
Log SHA-256:
`3dcd4ee4ffc213bac7e5ff6b9e48aef6493dac12ef65c919224c51f394e62bc5`.
Host version 0.0.0.0 and SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.

## Next decision

Reuse the same bounded recorder for a controlled camera pan, then compare
depth/color spatial agreement and current-versus-adjacent motion prediction.
The purpose is to distinguish current depth from a stale frame; it does not
require another recorder implementation. Preserve these accepted stationary
images as the control. Actual typed views and the state/lifetime contract at
the proposed evaluation point still need validation before bounded SDK input
assembly and the first native-resolution DLAA evaluation. Sustained rendering
and replacing/presenting TAA output remain later integration work.
