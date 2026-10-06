# Larger native color/motion jitter result: run 44552

Capture accepted and closed 2026-09-25. The v1.6.1 selector skipped one weak
preview, admitted a baseline on its second attempt, and captured three verified
projection-associated color/motion pairs. One injection restored cleanly.
**Color again responds in the predicted direction, while selected static motion
excludes jitter.** The larger stimulus improves separation from zero/opposite
shift models, but exact color displacement remains sensitive to the estimator
and image region. This is not a DLAA output or a certification of all passes.

## Identity and capture

- Executable version `0.0.0.0`, SHA-256
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- Recorder v1.6.1 SHA-256
  `f5f8dd7cccd6efc58dbd8526e3a961bcdca024dc6af738789320eed82023d16f`.
- PID `44552`, creation FILETIME `134348132641816754`.
- Log SHA-256
  `fd013fd351219de2a6ef53d24dfa3c35b532a976d43aee72e748e3a983a16d39`.
- Unchanged TAA shader: 15,492 bytes, SHA-256
  `2d7b26c742c27db2c83546c6cf6a1dfda47d7cfd4336e69f8c2302de5802b0d1`.
- D3D12, adapter LUID `0:97816`, swap-chain format `87`, 2560×1440.
- Evidence/frozen source: `local/temporal-jitter-20260925/v1.6.1/`.

The [selection contract](../runtime/temporal-jitter-selection.md) documents
the unchanged hook ABI/ownership and new bounded lookahead. The build passed
86 native and 41 Python tests. The user confirmed a loaded save after the
stationary-camera request, with DLSS Off and TAA On. The test armed only after
deployment, sidecar, all four configuration files, prelaunch manifest, ready
event and process generation were checked.

Frames/epochs **15538, 15539, 15540** form baseline, stimulus and recovery.
The selected native pair `(0.390625, −0.3395061492919922)` predicts
**(+0.1953125, +0.1697530746459961) raster pixels**, right and down. Its magnitude
is about 0.259 pixels, compared with 0.080 in run 35156. Actual candidate bits
equal the preview. All view and unjittered view-projection matrices are
bit-identical; baseline/recovery view-projection matrices are bit-identical.
All three projection sources are valid, matched and acknowledged.

Six 2560×1440 files save three R11G11B10_FLOAT color images and three R32_UINT
motion images, each 14,745,600 bytes, 88,473,600 bytes total. All decoded values
are finite. Frozen review accepts source identities, copies, restored resource
states, submission, fence completion, retirement, file sizes and hashes.

| Frame | Reset | Dispatch | Motion copy | Color copy | Forward | Execute | Completion | Retirement |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 15538 | 1681 | 1705 | 1706–1707 | 1708–1709 | 1710 | 1718 | 1795 | 1764 |
| 15539 | 1764 | 1788 | 1789–1790 | 1791–1792 | 1793 | 1802 | 1955 | 1848 |
| 15540 | 1848 | 1872 | 1873–1874 | 1875–1876 | 1877 | 1885 | 1956 | 1931 |

There are 966 journal events: 390 Reset, 399 Close, 174 Execute and three
Dispatch. Final completion is observed 219 ms after admission begins, with
no later admitted events. All 233 lifecycle references and 21 producer pins
release. Issued copy references retain their documented lifetime until process
exit. Recorder/projection faults and pending counts are zero; hooks disabled,
one injection/restoration, stopped accepted and settled.

## Separate image analysis

The same twelve control-only masks and original robust first-order estimator
from [run 35156](ordinary-taa-jitter-result.md) are retained. Control words agree
at 673,823 pixels (18.28%), with 2,973 exact 5×5 neighborhoods before other gates.
The inherited script initially stopped at its more-than-2,000-pixel threshold.
The final script reports masks below that threshold as ineligible without
fitting them; it does not reduce the threshold. The 5×5 masks at sigma 2 and 3
have only 1,137 and 683 pixels. The failed console and final analysis are kept.

All **ten eligible global fits** have the predicted positive-X/positive-Y
direction. The exact-center masks yield the following representative results;
all other variants remain in the measurement file:

| Gaussian sigma | Selected pixels | Robust game fit X/Y (px) | Recovery X/Y (px) |
| --- | ---: | --- | --- |
| 1 | 151,406 | +0.141414 / +0.129501 | −0.000160 / +0.000009 |
| 2 | 86,654 | +0.150202 / +0.133801 | −0.000701 / −0.000170 |
| 3 | 62,606 | +0.159891 / +0.139352 | −0.000349 / −0.000228 |

All 43/38/30 qualified spatial tile fits also have the predicted direction.
For the three exact-center masks, the matrix-predicted model gives median
normalized residuals 0.1227/0.1224/0.1194 pixels, compared with zero-shift
0.1684/0.1742/0.1774 and opposite-shift 0.3261/0.3308/0.3353. Double shift also
fits worse. These are image-model residuals, not statistical confidence bounds.
The original fits still underestimate the matrix prediction.

Known bilinear translations of baseline, rounded to the captured RGB precision,
produce original-estimator fits (+0.16984, +0.14068), (+0.18433, +0.15322), and
(+0.18788, +0.15656). Negative and zero controls are retained too. These are
resampled-image controls, not game rerasterization; they cannot establish the
entire cause of the game-image disagreement or justify a correction factor.

After observing the disagreement, an explicitly **exploratory** comparison uses
ordinary least squares without per-pixel gradient normalization. It tests both
a two-parameter translation and translation plus nuisance brightness gain/offset.
It uses the same exact-center control masks and reports every qualified tile
in an 8×6 grid (at least 500 selected pixels, normalized design condition ≤30).
No tile is rejected by its stimulus fit or agreement with prediction.

| Sigma | Tile count | Median tile game X/Y with gain/offset | Median known-shift control X/Y |
| --- | ---: | --- | --- |
| 1 | 43 | +0.177103 / +0.144606 | +0.182946 / +0.152883 |
| 2 | 38 | +0.183463 / +0.150180 | +0.190765 / +0.163341 |
| 3 | 30 | +0.183742 / +0.160760 | +0.194033 / +0.165885 |

Those game medians are approximately 0.01–0.025 pixels per axis from prediction,
but region/method variation is material. At sigma 3 the game tile 10th–90th
percentiles span X 0.137–0.265 and Y 0.072–0.273 pixels. The global gain/offset
fit is (+0.15964, +0.19111), different from the median of the tiles. No estimator
is selected as ground truth. Photometric changes, spatial filtering and fit bias
remain unresolved; precise color-scale verification stays false. The known
matrix/depth-supported jitter scale should not be retuned to these color fits.

## Motion response and scope

The existing packed-half decoder produces history-pixel offsets
`(-dx * width / 2, +dy * height / 2)`, masking the unknown low bit before decoding
dy. If static motion included jitter, stimulus history motion would be
`(−0.1953125, −0.1697531)` and recovery the opposite. These signals are
representable; actual packing yields approximately `(−0.1953125, −0.1697731)`
and its opposite. Instead:

| Exact-center mask | Zero baseline vectors | Zero stimulus vectors | Zero recovery vectors |
| --- | ---: | ---: | ---: |
| Sigma 1 | 98.58% | 98.54% | 98.56% |
| Sigma 2 | 98.29% | 98.23% | 98.25% |
| Sigma 3 | 98.19% | 98.12% | 98.15% |

Both components have median zero in every eligible mask. Less restrictive masks
include more animation and exceed 93% zero; the eligible exact-5×5 mask has 100%.
This reproduces the scoped static-motion jitter-exclusion result with a larger
shift and a different vertical sign. It does not certify independently moving
characters, all materials, the shader fallback or continuous rendering. The user
offered a stationary pose for future tests; no pose is asserted for this capture.

`analysis-44552.json` retains the original measurements, control outcomes,
exploratory estimator comparison and narrowly scoped direction/exclusion flags.
Broad capture-review flags and `precise_color_jitter_scale_verified`, typed-view,
and `dlaa_ready` remain false. Final plot: `jitter-analysis-44552-v2.png`.

## Closure and next decision

The user confirmed normal exit. Previous slot DLL restoration completed at
`2026-09-25T15:39:58.8920773+03:00`, SHA-256
`689bef7cf00ff71e4518f452f0467edf6c3d877f2a6dd090553388d8b5f288cc`.
The temporary output sidecar is removed and all four configuration hashes are
unchanged. All 157 prelaunch files and 126 archived source files verify; closure
documents are frozen separately. No next test is installed or armed.

Next, audit the actual typed bindings and connect the separately validated real
scene depth to the same color/motion frame for bounded DLAA input assembly.
The native TAA named depth slot is still a 1×1 fallback and must not become the
SDK depth input. Preserve matrix-derived jitter and the scoped unjittered-motion
interpretation; carry the color photometric uncertainty into integration checks.
Do not claim that this run proves all input requirements or that SDK output is
ready. It provides CPU projection/TAA association and verified input copies, not
the complete earlier source-to-draw/CBV trace or a sustained reconstruction path.
