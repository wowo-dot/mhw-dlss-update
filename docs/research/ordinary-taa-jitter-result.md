# Bounded color/motion jitter result: run 35156

Capture accepted and closed 2026-09-25. Three consecutive projection-associated
color/motion pairs pass frozen v1.6.0 review. **Color responds in the predicted
jitter direction, and the selected static majority retains zero motion during
both stimulus and recovery.** This supports jitter exclusion from that motion
field in this scene. Precise color displacement remains unresolved: the tiny
stimulus produces underestimated image fits, including in known-shift estimator
controls. No SDK evaluation or DLAA output is performed.

## Identity and bounded capture

- Executable version `0.0.0.0`, SHA-256
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- Recorder v1.6.0 SHA-256
  `75ea0ea664f04e38948c8ffbb5453a84f6f8175cb2add65976a2bde06e0067ae`.
- PID `35156`, creation FILETIME `134348110441703979`.
- Log SHA-256
  `8d93679813b7eac01b00a58dc41513042c34bc06ee3343367d01c837a67398c1`.
- Shader SHA-256
  `2d7b26c742c27db2c83546c6cf6a1dfda47d7cfd4336e69f8c2302de5802b0d1`,
  15,492 bytes; unchanged from the earlier TAA captures.
- D3D12, adapter LUID `0:97816`, swap-chain format `87`, 2560×1440.
- Local evidence: `local/temporal-jitter-20260925/v1.6.0/`.

The [v1.6.0 contract](../runtime/temporal-jitter-pairs.md) documents the eight
hook targets, calling conventions, scope and ownership. The build passed 76
native and 38 Python tests before deployment. This run kept the bare profile,
DLSS Off and TAA On. The user reported a loaded save after the stationary-camera
request. Saved view matrices independently establish an unchanged camera.

Frames/epochs **26684, 26685, 26686** match projection source, TAA producer and
paired readback, with banks 0/1/0 and async mode 1. One native-table injection
and one exact restoration are recorded. The middle frame's native pair is
`(0.09375, 0.12962967157363892)`, predicting raster displacement
**(+0.046875, −0.06481483578681946) pixels**. Baseline and recovery have zero
jitter. All view and unjittered view-projection matrices are bit-identical;
baseline and recovery jittered view-projection matrices are also bit-identical.
All three projection samples are valid, matched and acknowledged.

Each frame saves one R11G11B10_FLOAT color image and one R32_UINT packed motion
image, each 14,745,600 bytes: 88,473,600 bytes total. All decoded values are
finite. Source identity, state restoration, private allocation, footprint,
file length/hash, submission, fence completion and recording retirement pass
the frozen reviewer. Three copies per channel add 18 command-list operations
and three private Signals, with no observer Execute.

| Frame | Reset | Dispatch | Motion copy | Color copy | Forward | Execute | Completion | Retirement |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 26684 | 1737 | 1761 | 1762–1763 | 1764–1765 | 1766 | 1774 | 1851 | 1820 |
| 26685 | 1820 | 1844 | 1845–1846 | 1847–1848 | 1849 | 1858 | 2038 | 1904 |
| 26686 | 1904 | 1928 | 1929–1930 | 1931–1932 | 1933 | 1941 | 2039 | 1987 |

The journal contains 1,008 events: 409 Reset, 410 Close, 186 Execute and three
Dispatch, tracking 21 lists, 44 allocators and one queue. Final completion was
observed 219 ms after admission began; no journal events follow it. All 241
lifecycle references and 21 producer pins release. Issued copy references stay
retained until process exit. Recorder and projection faults/pending counts are
zero; hooks are disabled before final review.

## Color response and estimator limits

Raw baseline/recovery color words agree at 1,235,650 pixels (33.52%). Requiring
every word in a 5×5 neighborhood to agree leaves only 9,565 pixels before other
gates; the initial analysis stopped at an arbitrary 10,000-pixel minimum. This
was a mask-selection rejection, not a capture failure. The retained control
selection report compares coverage without consulting stimulus or motion data.

The final analysis reports all twelve combinations of Gaussian sigma 1/2/3
and four control-only masks: exact 5×5 neighborhood, exact center, no exact-word
requirement, and exact center with a looser drift bound. All require a 20-pixel
image border and control-reference gradient magnitude above 1e-4. Blurred
baseline/recovery drift must be at most 0.025 times gradient magnitude, or
0.05 for the looser variant. The minimum count is now 2,000, with actual coverage
reported. Stimulus and motion values never select pixels.

The reference is the mean of blurred baseline and recovery. Eight Huber
iterations fit `stimulus − reference = −shift · gradient`, normalized by
gradient magnitude; gradients use a fourth-order central difference. RGB
luminance weights are an analysis choice, not renderer color-space validation.
Four continuous analytic-texture controls recover positive, negative, zero
and another nonzero shift within 0.001 pixels per axis.

The exact-center mask is shown as a representative analysis, with all variants
retained rather than selecting the best agreement with prediction:

| Gaussian sigma | Pixels | Measured raster X/Y (px) | Recovery X/Y (px) |
| --- | ---: | --- | --- |
| 1 | 393,357 | +0.031742 / −0.047536 | −0.000304 / −0.000018 |
| 2 | 235,692 | +0.033162 / −0.051140 | −0.000355 / −0.000392 |
| 3 | 166,564 | +0.036386 / −0.054224 | −0.000407 / −0.000262 |

All twelve global fits have the predicted positive-X/negative-Y direction.
The exact-center masks also yield 45/36/33 spatial tile fits; all have that
direction. Tiles qualify only by at least 500 selected pixels and normal-matrix
condition below 20. The predicted shift has smaller median normalized residual
than zero or opposite shift for all three exact-center masks. The color response
is therefore supported across the image, but the fitted amount remains below
the matrix prediction by roughly 0.010–0.017 pixels per axis in these fits.

To check measurement bias, an additional control bilinearly translates the
captured baseline by the known predicted shift and rounds the RGB values to
the same unsigned 6/6/5-mantissa precision. Original representable values round
back exactly. This synthetic input is fitted using the original control masks:

| Sigma | Known-shift control fit X/Y (px) |
| --- | --- |
| 1 | +0.031501 / −0.046165 |
| 2 | +0.037961 / −0.053733 |
| 3 | +0.040212 / −0.055750 |

Negative translations also underestimate; zero translations fit exactly zero.
Thus this estimator underestimates a known shift on the captured texture at
this precision. These controls are resampled images, **not a model of game
rerasterization**, so no correction factor is applied and they do not establish
the cause of every difference in the game images. Filtering, color quantization,
changing shading and nearby animation limit this first-order measurement.
Precise color-scale agreement remains unverified. There is no basis here for
changing the already matrix/depth-supported jitter scale to match these fits.

## Motion response

Use the previously shader-derived history-pixel decoding:

```text
dx = half(packed >> 16)
dy = half(packed & 0xfffe)
history_pixel_delta = (-dx * width / 2, +dy * height / 2)
```

If static motion included the injected displacement, the stimulus would carry
approximately `(−0.046875, +0.064815)` history pixels and recovery the opposite
pair. Packing/unpacking those predictions produces nonzero
`(−0.0468445, +0.0648022)` and its opposite, well above packing resolution.
Instead, both components have median exactly zero for every control-only mask:

| Exact-center mask | Zero baseline vectors | Zero stimulus vectors | Zero recovery vectors |
| --- | ---: | ---: | ---: |
| Sigma 1 | 99.03% | 98.99% | 98.98% |
| Sigma 2 | 98.77% | 98.72% | 98.70% |
| Sigma 3 | 98.63% | 98.56% | 98.54% |

Every mask variant exceeds 97% zero vectors in all three frames. This supports
**jitter exclusion in the selected static majority**. Earlier right-pan evidence
showed that the field does respond to actual camera motion; zero values here
are not being used alone to certify the motion path. The remaining nonzero
vectors, moving characters, all materials and the shader's depth/matrix fallback
are outside this conclusion. Low-bit meaning remains unknown.

The separate `analysis-35156.json` records
`scoped_color_jitter_direction_supported=true` and
`scoped_static_motion_jitter_exclusion_supported=true`. Broad capture-review
flags, precise color-scale, typed-view and `dlaa_ready` flags remain false.
The script collection, masks, complete measurements, estimator controls and
`jitter-analysis-35156.png` remain local with the raw captures.

## Startup, closure and next step

Before arming, the user reported roughly two minutes of black screen, then
reported that loading completed without intervention. Read-only live checks
found a ready/unarmed recorder, no installed diagnostic hooks, and unchanged
projection/TAA code. CPU, memory and read I/O increased during startup; the
later ten-second sample had a fixed renderer frame. There was no debugger
attachment, suspension or process-memory write. The exact delay cause remains
unknown; absence of active hooks is not proof that DLL loading had no effect.
The startup records are preserved alongside this accepted capture.

The user confirmed normal exit. At `2026-09-25T15:04:14.7800583+03:00`, the
previous DLL was restored, SHA-256
`689bef7cf00ff71e4518f452f0467edf6c3d877f2a6dd090553388d8b5f288cc`.
The temporary output sidecar is absent and all four configuration hashes are
unchanged. All 152 prelaunch files and 124 frozen source files verify. Closure
documents are copied separately without altering the tested source snapshot.
No next test is installed or armed.

Next: select a deliberately larger bounded native-table sample to tighten the
color-displacement evidence, retaining baseline/recovery and motion controls.
Do not subtract jitter from the selected static motion merely because projection
is jittered. Same-frame scene depth, actual typed input/output views, provider
contract and a bounded native-resolution SDK evaluation remain DLAA integration
requirements. This run associates CPU projection and TAA requests; it does not
repeat the earlier full projection-to-draw/CBV chain or test sustained rendering.
