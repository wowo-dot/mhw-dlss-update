# First jittered depth triplet: v0.8.4 run 48276

Captured 2026-09-25 in a loaded save, DX12 On, DLSS Off and TAA On. The
unchanged archived reviewer accepts three independent 2560×1440 depth readbacks
for baseline frame 15630, scoped stimulus 15631 and recovery 15633. One recovery
admission was deferred. One camera-field injection was restored; capture stopped
with zero faults, drops or pending calls and all hooks disabled.

Offline measurements on static, locally affine depth surfaces closely match the
predicted subpixel translation. This is evidence that this scoped projection
stimulus reaches rasterized scene depth. It is a one-sample, one-scene result,
not qualification of sustained jitter, every contributing draw or working DLAA.

Executable version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
DLL SHA-256
`3DE6A194D9A8BF3356A2111F56AFF12E0CB14412BB662DD26AC39D7881469FC9`.
PID 48276, creation FILETIME 134347594784406213.

## Capture and ordering

The [v0.8.3 timing failure](depth-triplet-timing-result.md) is corrected:
the validated draw return is published at provenance order 4532, immediately
after draw marker 4531 and before selected submission 4567 and stimulus copy
submission 4573. CPU recovery still precedes overall core completion.

| Slot | CPU/request frame | Native copy order | Copy submission | Private Signal | Retiring Reset |
| --- | ---: | ---: | ---: | ---: | ---: |
| Baseline | 15630 | 3954 | 4135–4138 | 4139–4140 | 4253–4254 |
| Stimulus | 15631 | 4377 | 4573–4576 | 4577–4578 | 4691–4692 |
| Recovery | 15633 | 5277 | 5438–5441 | 5442–5443 | 5556–5557 |

Each slot matches its original native copy-request stamp, uses a separate private
buffer/fence and completes one copy and one Signal. All three share the pinned
depth resource/canonical identity and the same DIRECT queue. Writable view
lifetime, recording retirement and pixel hashes pass the archived reviewer.
Baseline/recovery remain request controls; only the stimulus has the traced
selected-draw binding. No fabricated control draw markers are used.

Each image contains 3,686,400 finite depth values; total pixel payload is
44,236,800 bytes. Raw image hashes are recorded in the capture and acceptance
receipts. The depth preview shows character/weapon, architecture, floor and
hanging cloth. Its logarithmic colors are not linear world distances.

## Pixel measurement

All three recorded view matrices are bit-identical, as are baseline/recovery
view-projection matrices. Baseline and recovery have 2,998,078 identical depth
pixels (81.33%); their visible differences concentrate around animated objects.
The stimulus differs at almost every pixel.

To avoid selecting pixels based on the desired response, the measurement mask
uses only baseline/recovery: exact equality over each 5×5 neighborhood, positive
depth, a gradient exceeding 16 float32 ULP, and local affine-plane residual at
most `max(8 ULP, 0.002 × gradient magnitude)`. This selects 1,449,100 pixels.
Edges, unresolved gradients and moving objects are excluded.

For a scene translation `(dx,dy)`, an affine depth interior obeys
`S(x,y)-B(x,y) = -dx*dB/dx - dy*dB/dy`. The fit normalizes each equation by its
gradient magnitude. The native matrix/viewport contract independently predicts
`(jx/2,-jy/2)` pixels, with positive X right and positive Y down.

| Measurement | X, pixels right | Y, pixels down |
| --- | ---: | ---: |
| Predicted from recorded jitter | 0.234375 | 0.0216049 |
| Ordinary least-squares estimate | 0.234460 | 0.0232770 |
| Robust estimate | 0.234162 | 0.0225428 |
| Median of 47 sufficiently populated/conditioned tile estimates | 0.234255 | 0.0216084 |

The fixed prediction has median absolute normalized residual 0.0006836 pixels,
versus 0.20018 for zero shift, 0.40105 for the opposite shift and 0.20036 for
double amplitude. This is approximately a 293-fold reduction from the zero-shift
model. Normalized residuals are model discrepancies, not confidence intervals.
The global fit has condition number 1.38. The 8×6 tile grid was fixed in advance;
47 tiles meet the count/conditioning requirements. No tile was selected by its
agreement with the prediction.

The measurement and figures are separate from the unchanged capture reviewer.
Its broad `raster_jitter_verified`, complete pixel-producer-frame and final-scene
flags remain false. Static smooth interiors provide strong evidence of the
expected response in this captured depth image, but do not establish coverage
of color, motion, transparent geometry, every pass or a continuing sequence.

## Cleanup and next decision

The journal has 55 watch events, 2,943 history calls, 6,591 barriers and 165
native copies. All 330 observer copy references release after drain. Issued
private resources remain retained until process exit. The user closed normally;
the exact prior v0.15 DLL was restored, the temporary output sidecar removed
and all four graphics/profile/modlist/mode hashes remained unchanged.

The build passed 241 native and 543 Python checks. All 83 archived source
identities verify. Local evidence is under
`local/projection-binding-20260923/v0.8.4/`: raw images/journals, unchanged review,
pixel overview and shift measurements, figures, acceptance/archive and rollback
receipts. Proprietary captures and shaders remain untracked.

Next: carry this pixel-level check into a bounded sequence with different
jitter directions and recovery, then associate jittered color and motion inputs
before sustained DLAA integration. An unchanged zero-jitter alignment rerun is
unnecessary. No modern provider was evaluated by this diagnostic.
