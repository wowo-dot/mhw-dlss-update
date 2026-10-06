# Independent-AA projection/bindings v0.6.1 result

2026-09-26, PID 45344 / creation 134349242098501078. **Capture accepted:**
eight projection/request/GPU associations and native binding snapshots, all eight
GPU completions and recording retirements, nine input images, and six restored
jitter scopes. Completion was collected before hook removal. Native AA and DLSS
remained Off throughout. This is an independent input/projection/binding result;
no SDK evaluation or DLAA output handoff ran in this test.

The user exited normally. Exact previous diagnostic DLL restored at
22:28:29 +03; matching output-directory sidecar removed. All four configuration
hashes unchanged. Game closed; no next build installed or armed.

## Confirmed observations

Source frames 11895-11902, Windowed 2560x1440, DX12, device 353637680,
adapter LUID 98931:0, swap-chain format 87. The sampled native TAA and FXAA flags
were both zero. Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Test DLL SHA-256
`dbb5632e1850f80d39544b234dad359c6a5e18e4830349d3883d2bf9b71d9f31`.
Hook targets, ABI, ownership and code gates remain documented in the
[runtime contract](../runtime/independent-aa-projection-closure.md) and its
linked v0.6.0 contract.

- All eight owned requests matched their projection and GPU receipt by immutable
  source identity. Matrix products and view-projection relations passed the
  frozen reviewer. Six distinct nonzero stimuli were restored to zero; first
  and last receipts were zero-jitter controls. No projection fault or pending
  callback remained.
- All eight native binding snapshots were valid on the matching Reset generation.
  Pipeline 1914590912 and compute root 356190976 were stable; both compute and
  graphics arguments were recorded. Input copies left the bindings unchanged.
  Actual binding restoration after private shader work remains untested here.
- Each input read point followed the qualified paired native transition. Color,
  depth and motion were all in state 128 (pixel-shader resource). Receipts 0, 3
  and 7 each copied all three inputs through state 2048 (copy source) and restored
  state 128. All nine saved image hashes and footprints passed review.
- All eight private fence values reached 1; all selected command-list recordings
  retired through observed successful Reset calls. Readback resources and watched
  input references were released. The submission journal released all 437 pins.
- All 25 hooks were disabled and drained; owned descriptors detached. Outer,
  projection, GPU and submission fault/pending counts were zero.

## Recorder closure

The journal started at tick 34847859. All eight completed fence values were
sampled at tick 34848281, before collection closure was requested. Collection
closed at the same millisecond, with 2,831 events; that count remained fixed.
First outer hook removal was recorded at tick 34848281 and first GPU hook
removal at 34848562, 281ms later. Millisecond timestamps allow equality; source
ordering and the reviewer require completion/closure before physical removal.

The frozen reviewer accepted `completion_before_hook_removal_verified`, including
retirement before cutoff and draining admitted events. The unchanged 4,096-event
cap did not overflow. This verifies the v0.6.1 closure correction in the game;
the prior v0.6.0 partial capture remains rejected and unchanged.

## Image inspection and limits

Nine 2560x1440 images, 14,745,600 bytes each (132,710,400 total), cover source
frames 11895, 11898 and 11902. Formats are R11G11B10_FLOAT color, R32_FLOAT
depth and R32_UINT packed motion. Independent vectorized decoding was checked
against the existing scalar decoders at 303 pixels per color/motion role.

All decoded color, depth and motion values were finite. Depth ranged from zero
to approximately 0.05739. The preview shows a coherent matching scene across
color and depth, with localized character/object motion and a largely stationary
background. Median and 90th-percentile motion magnitude were zero; approximately
5.4-5.6% of pixels exceeded 0.01 pixels of motion. This is a visual/numeric sanity
check, not an independent measurement of raster jitter or temporal alignment.

CPU projection and GPU receipt association is verified; actual color-jitter
response, cross-queue producer freshness and DLAA history/output correctness are
not certified by this capture. The reviewer preserves those explicit limitations.

Next, connect the existing private conversion and DLAA evaluation to this
qualified AA-Off read point, using its source-frame projection and saved native
bindings. Respect the observed PS-SRV input state; the old TAA-entry output
handoff assumes a UAV state and must not be reused unchanged. Verify private
shader binding restoration and output before enabling continuous handoff.
Other-AA suppression, mode switching and production failure latching remain
integration work; the historical TAA-dependent route is not this implementation.

## Evidence

Ignored session: `local/independent-aa-projection-closure-20260926/v0.6.1/`.
352 frozen sources and 391 prelaunch files, including test binaries and 13 real
WARP fixture traces/reviews. Offline verification: 48 native and 43 Python checks.
Prelaunch SHA-256
`164467154ea584bb027ddd7f8751196837c9c48c6ecae35553430a619ea81630`.

Capture: 764,381 bytes, SHA-256
`9dd2d5efe123178d55d104f82d56113830677961e415b2cf19e56a7257592093`.
`review-45344.json` records full scoped acceptance; `pixel-inspection-45344.json`
and `input-triplets-preview.png` retain the additional image inspection.
`restored.json` records exact rollback and unchanged configuration hashes.
Restored DLL SHA-256
`689bef7cf00ff71e4518f452f0467edf6c3d877f2a6dd090553388d8b5f288cc`.
No native source, tests or tools changed during this capture or closure.
