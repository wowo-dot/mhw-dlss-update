# Three jitter directions observed in an accepted depth sequence

Run `10232`, process creation FILETIME `134347625333711630`, used v0.9.1 on
2026-09-25. The unchanged archived reviewer accepts five independently stamped
2560×1440 depth readbacks: baseline, three scoped native jitter samples and zero
recovery. Separate pixel analysis measures all three predicted translations on
the same control-selected smooth depth interiors. This is bounded evidence
from one scene, not continuous jitter, full-renderer coverage or working DLAA.

The executable is version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
The loaded v0.9.1 DLL is SHA-256
`6C9EB26228CBD76DCADADF4D17F9EC633B46EC2E6EA02FB5D1A967B598FB941F`.
The barebones profile used DX12, TAA On and DLSS Off; the user reported a
loaded save and received instructions to keep the camera still.

## Capture and association

All five source/request stamps pass the archived review. Three distinct native
pairs were injected around the original projection helper and restored to zero
before source publication. There was one untouched primary-draw wait, no recovery
deferral, and no fault, drop or pending callback. Frame 14807 is outside the
admitted sample sequence; these are not claimed as five consecutive frames.

| Stage | Request frame | Private copy | DIRECT submission | Retirement Reset |
| --- | ---: | ---: | --- | --- |
| Baseline 0 | 14805 | 5095 | 5317–5320 | 5433–5434 |
| Stimulus 1 | 14806 | 5595 | 5825–5828 | 5945–5946 |
| Stimulus 2 | 14808 | 6618 | 6841–6844 | 6959–6960 |
| Stimulus 3 | 14809 | 7140 | 7350–7353 | 7468–7469 |
| Recovery 4 | 14810 | 7673 | 7858–7861 | 7976–7977 |

The selected draw marker is 5783, validated draw return is 5784, and its
submission is 5819. The first stimulus copy submission follows at 5825 on the
same DIRECT queue. All slots retain canonical depth identity `357249136`,
resource interface `357249184`, command list `357001712` and queue `208095632`.
Only stage 1 has the traced source/publication/consumption/CBV/draw chain. The
other slots are explicitly request-associated; no additional draw bindings are
inferred from their matching CPU frames.

Each private buffer saved 14,745,600 bytes after one successful private Signal,
fence value 1 and command-list retirement. Every image contains 3,686,400 finite
values, with no values outside [0,1]. The archived reviewer also accepts the
resource/DSV lifetime, recording, submission order and original-request match.
Its broad `raster_jitter_verified` flag remains false; pixel interpretation is
recorded independently below rather than changing that review result.

History used **5132 of 8192 entries**, confirming that this workload exceeds
v0.9.0's former 4096 bound. Other counts are 11,964 barriers, 242 observed copies,
355 submitted list references, 55 view-watch records and 88 packet
receipts/consumers. All 484 observer copy references released and hooks disabled
cleanly. Issued private readback resources retained their intentional process
lifetime ownership until the user exited normally.

## Separate pixel measurement

The five view matrices are bit-identical. Baseline and recovery view-projection
matrices are bit-identical, and 3,218,900 of 3,686,400 depth pixels are bit-exact
between those controls (**87.3183%**).

The same method and thresholds as [run 48276](depth-triplet-result.md) select
**1,527,643 pixels** using baseline and recovery only. The mask requires a
positive, bit-exact 5×5 control neighborhood, resolvable gradient exceeding
`max(16 float32 ULP, 1e-10)` and local affine-plane error at most
`max(8 ULP, 0.002 × gradient magnitude)`. All three stimuli use the identical
mask; stimulus residuals do not determine which pixels are selected.

For translation `(dx,dy)`, fit
`S(x,y) - B(x,y) = -dx * dB/dx - dy * dB/dy`, normalized by gradient magnitude.
Positive x points right and positive y points down in resource pixels. Native
projection samples predict `(jx/2, -jy/2)`. The fit uses ordinary least squares
and eight fixed robust reweighting iterations, with separate synthetic
non-collinear affine-slope checks of sign and scale.

| Stimulus | Predicted shift (pixels) | Robust measured shift (pixels) | Largest axis difference |
| --- | --- | --- | ---: |
| 1 | (-0.078125, -0.231482) | (-0.078189, -0.230663) | 0.000819 |
| 2 | (-0.140625, +0.077160) | (-0.140434, +0.076353) | 0.000808 |
| 3 | (+0.109375, -0.089506) | (+0.109427, -0.089462) | 0.000052 |

The fixed predicted model has median absolute normalized residuals of
0.000986, 0.000921 and 0.000333 pixels. The zero-shift model is approximately
234×, 83× and 269× worse by that measure. Opposite, double and each other
sample's predicted shift also fit substantially worse. These residuals measure
agreement with this affine-depth model; they are not statistical confidence
intervals or an accuracy guarantee for every pixel.

An 8×6 spatial partition yields 44 populated, well-conditioned tile fits for
each stimulus. Median tile shifts are (-0.078223, -0.231336),
(-0.140638, +0.077156) and (+0.109392, -0.089753). The global normal-matrix
condition number is 3.26. The inspected plot shows a coherent baseline depth
scene, a mask excluding the moving character and depth boundaries, and measured
changes close to the predicted diagonal for all three samples.

This supports the sign and scale of three scoped jitter directions reaching
static rasterized depth, extending the earlier one-sample result. It does not
establish all depth-producing draw attribution, moving-object behavior,
continuous-frame history, color/motion input alignment, camera cuts, renderer
recreation or successful modern-provider reconstruction.

## Closure and next step

The user exited normally. The previous slot DLL was restored exactly to
SHA-256 `689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`;
the temporary output sidecar was removed, and all four graphics/profile/mode
hashes remained unchanged. All 90 archived source files verify against the
preparation receipt. The build had passed Release `/W4 /WX`, 260 native tests
and 586 Python checks; archived accepted/rejected captures retained their
previous outcomes.

Private evidence is under `local/projection-binding-20260923/v0.9.1/`:
`review-10232.json`, the five raw `.f32` files, `pixel-overview-10232.json`,
`pixel-shift-10232.json`, inspected `pixel-shift-10232.png`, immutable source
archive, deployment/restoration receipts, `acceptance-10232.json` and the
hash manifest `archive-10232.json`. Raw game-derived pixels and shaders remain
ignored and are not distributable repository assets.

Next, associate the full-resolution color and motion-vector inputs with the
validated request/frame boundary, then validate motion contents and jitter
handling before a bounded DLAA reconstruction. No further unchanged depth
sequence rerun is needed for this result, and no next diagnostic is installed.
