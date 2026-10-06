# Native depth-copy association, run 45404

Captured and closed 2026-09-25 with recorder **1.8.0**. The recorder completed
cleanly. The frozen full-capture reviewer remains **rejected** with
`depth copy reset boundaries`. A separate, tested post-capture audit verifies
the narrower association for all three selected frames; it does not replace
that rejection or certify the incomplete tail.

## Confirmed observations

Frames **12665, 12666, 12667** each have three published copy requests and three
matching native CopyResource calls. All nine selected copies have complete
Reset/Close/Execute/next-Reset witnesses. The original request stamp, packet
generation, source/destination owners, native resource descriptions and local
barrier states agree with the selected primary view and TAA color/motion frame.
The renderer's later frontend frame counter is not substituted at consumption.

The separate view+628/container+A0 candidate is now connected to a real native
copy destination, rather than inferred from format alone. Source is full-size
2560x1440 R32_TYPELESS with ALLOW_DEPTH_STENCIL; destination is R32_FLOAT with
ALLOW_RENDER_TARGET. Both have one mip, slice and sample. The candidate's opaque
SRV address is stable, but the typed descriptor is still unverified. Ordinary
TAA continues to bind the distinct 1x1 RGBA8 fallback depth texture.

Each selected frame has this observed submission arrangement:

| Copy stage | Source state at CopyResource | Destination state | Relation to TAA |
| --- | --- | --- | --- |
| Earlier copy | COPY_SOURCE + NON_PIXEL_SHADER_RESOURCE (0x840) | COPY_DEST (0x400) | Earlier Execute call, list index 1 |
| Later batch, list 0 | 0x840 | 0x400 | Earlier list-array position in the same Execute batch |
| Later batch, list 1 | COPY_SOURCE (0x800) | 0x400 | Same command-list generation, before TAA Dispatch |

The third stage gives a concrete location for the next bounded depth-pixel
capture. Array position alone does not establish whole-list serialization.
This trace does not prove exhaustive writer coverage or certify a final writer
for SDK use. State at CopyResource also does not establish a shader-readable
state at a later injection point; an observer readback must explicitly preserve
the native state contract.

## Why the full reviewer rejected

The recorder stops once the three paired captures complete and their recording
generations retire. In this run it also observes the next frame's three native
copies (12668), but stops before their Execute/retirement events. Three more
requests are published for 12669 without a matched consumer before shutdown.
The frozen reviewer demanded complete generations for every recorded copy,
including the unselected tail, so it rejects the full capture.

The supplemental audit keeps the original log and rejection unchanged. It
permits an explicitly unqualified tail only after the last selected frame and
Dispatch, with an observed initial Reset, no Execute or later Reset, at most one
valid Close and no overlapping operations. Selected-frame boundaries remain
mandatory. Fifty Python tests pass (nine jitter-only skips), including rejection
of incomplete selected frames, missing initial Reset, overlapping/repeated Close
and execution without retirement. The prelaunch suite was 49 Python/108 native
checks; no runtime DLL changed after that suite or after this capture.

There are 113 unmatched copy-consumer calls. The counter includes warmup and
nonmatching destinations; it does not distinguish individual causes. All nine
recorded requests belonging to the selected frames match a consumer, but this
does not turn the counter into evidence of exhaustive producer coverage.

## Capture and closure

Six verified color/motion files total **88,473,600 bytes**. All decoded color and
motion values are finite. A tone-mapped color preview was visually inspected:
the loaded gameplay scene is coherent. This is artifact QA, not depth/color
alignment proof. Roughly 92.8% of motion pixels are zero under the stationary
camera request; no new motion-calibration claim is made.

The journal has 1,108 events: 447 Reset, 447 Close, 199 Execute, 12 native-copy
markers and three TAA Dispatch markers. Final selected-frame completion is
203 ms after capture start, with no journal event after that completion.
All capture faults and pending counts are zero; hooks are disabled. Released
metadata references: 24 producer, three TAA consumer, 36 depth-route and 255
lifecycle references. Issued readback references retain the existing process
lifetime policy.

User confirmed normal exit. The exact previous DLL was restored at
17:26:32 +03; temporary output sidecar removed; all four saved configurations
remain unchanged. No next test is installed or armed.

Private evidence: `local/temporal-depth-routes-20260925/v1.8.0/`, including
the original `review-45404.json`, supplemental `analysis-45404.json`, its
separately preserved reviewer/tests, preview, frozen prelaunch sources and
closure/archive manifests. Log SHA-256:
`e9953f301442c159d1702615ae19aaf66d766b421afc737c8d4ef2ff80b17d5f`.

## Next step and limits

Prepare a bounded depth readback at the request-matched native copy on TAA's
command list, associate it with the same selected color/motion frame, and
validate resource-state restoration plus submission/completion before saving
pixels. Then compare scene depth against the paired imagery. Depth pixels and
content alignment, exhaustive final-writer proof, typed input views, SDK
evaluation and sustained DLAA remain outstanding.
