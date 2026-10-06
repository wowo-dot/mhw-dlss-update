# Paired current-color/motion result: run 43820

Accepted and closed 2026-09-25. Three consecutive current-color and packed-motion
images pass frozen v1.5.0 review. **Independent image matching supports the
shader-derived motion direction, pixel scale and same-frame association in the
tested scenery.** Median endpoint disagreement is 0.118 and 0.122 pixels across
the two adjacent image pairs. This is a scoped input validation; DLAA is not
enabled and jitter inclusion remains unverified.

## Identity and capture

- Executable version `0.0.0.0`, SHA-256
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- Recorder v1.5.0 SHA-256
  `a5e72c6f9eca152edffad294545aa4a78c42be622df2064fa554004a7d9f0ff7`.
- PID `43820`, creation FILETIME `134348094598536299`.
- Log SHA-256
  `394af7c409d8f70caf5b1d81bfb664f4f2352b7cb8873572867942e93f351fc6`.
- Shader: 15,492 bytes, SHA-256
  `2d7b26c742c27db2c83546c6cf6a1dfda47d7cfd4336e69f8c2302de5802b0d1`,
  identical to run 4376 and both v1.4 captures.
- D3D12, adapter LUID `0:97816`, swap-chain format `87`, 2560×1440.
- Evidence and frozen source: `local/temporal-pairs-20260925/v1.5.0/`.

The user confirmed panning after the requested gentle rightward camera turn,
with the character stationary and phone replies. The bare profile retained
DLSS Off, TAA On and DX12. Camera angles and angular speed were not instrumented.
No jitter was injected. The [paired recorder contract](../runtime/temporal-paired-readback.md)
documents hook ABIs, hash gates, ownership, copy budgets and fail-closed behavior.
The build passed 58 native and 32 Python tests before deployment.

Frames 18042–18044 each save a 14,745,600-byte R32_UINT motion image and an
equally sized R11G11B10_FLOAT current-color image: **88,473,600 bytes total**.
All decoded components are finite. Color has HDR values up to 12.25, without
tone mapping in the captured data. All six copies have distinct private
allocations and verified source identities, footprint, length and SHA-256.
The copy transitions restore motion to 0xC0 and color to 0x40 before forwarding
the original Dispatch. The frozen reviewer verifies completion and retirement
before readback. Three actual submissions share one native command list and
queue, with distinct Reset generations and private fences.

| Frame | Reset | Dispatch | Motion copy | Color copy | Forward | Execute | Completion | Retirement |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 18042 | 2461 | 2485 | 2486–2487 | 2488–2489 | 2490 | 2498 | 2575 | 2544 |
| 18043 | 2544 | 2568 | 2569–2570 | 2571–2572 | 2573 | 2582 | 2762 | 2628 |
| 18044 | 2628 | 2652 | 2653–2654 | 2655–2656 | 2657 | 2665 | 2763 | 2705 |

The journal contains 1,370 events: 558 Reset, 560 Close, 249 Execute and three
Dispatch events; 21 lists, 44 allocators and one queue. Final completion is
observed 203 ms after admission starts. No events are admitted after final
completion; faults and pending counts are zero and hooks are disabled.
All 307 cumulative lifecycle references and 21 producer pins are released.
Issued copy references remain retained until normal process exit as designed.

## Image comparison

The decoded history offset is:

```text
dx = half(packed >> 16)
dy = half(packed & 0xfffe)
history_pixel_delta = (-dx * width / 2, +dy * height / 2)
previous_color_coordinate = current_pixel_coordinate + history_pixel_delta
```

Six static-scene boxes were selected from the color preview before fitting:
stone above the door, the upper door, two pillars, the right pillar base and
the lower-left floor. They exclude the player and flame sources. The analysis
evaluates 217 patches per image pair. Each is 25×25 pixels sampled every two
pixels (13×13 samples); log1p of fixed RGB-weighted luminance limits highlights.
Normalized correlation removes local gain/offset sensitivity. The luminance
weights are an analysis choice, not a certification of the renderer color space.

Two separate measurements are preserved:

1. Warp the earlier image using each captured motion field, scanning signed
   scale multipliers from −1.5 to +1.5 in 0.025 steps. Median correlation across
   all 217 textured patches peaks at **+1.0 for the current frame's vectors**.
2. Estimate translation from images alone, with no vector seed: exhaustive
   ±80-pixel horizontal/±24-pixel vertical search at two-pixel steps, followed
   by ±2-pixel refinement at quarter-pixel steps. Qualification depends only
   on texture, correlation >0.95, peak uniqueness beyond six pixels, and an
   interior search optimum. No match is selected by agreement with motion.

| Measurement | 18043 → 18042 | 18044 → 18043 |
| --- | ---: | ---: |
| Image-qualified patches | 109 / 217 | 122 / 217 |
| Median vector/image endpoint disagreement | 0.118 px | 0.122 px |
| 90th-percentile disagreement | 0.264 px | 0.237 px |
| Median measured/predicted horizontal scale | 1.00082 | 1.00056 |
| Qualified matches below 0.5 px disagreement | 109 / 109 | 121 / 122 |
| Current-vector median correlation, all 217 patches | 0.971 | 0.973 |
| Zero-motion median correlation | 0.037 | 0.052 |
| Other adjacent vector field at native scale | 0.709 | 0.710 |

Other-frame vectors produce approximately 1.28/1.26 pixels median endpoint
disagreement; frame 18042's smaller vectors produce about 19.83/18.92 pixels.
These controls compare fields at the same image coordinates. They are an
association test, not an assertion of exact correspondence between pixels of
the two different vector images. Rescaling the neighboring field can also fit
fairly well because this is a coherent pan; the native-scale current field is
the best tested interpretation, not a proof against every alternative model.

Reversing both signs gives correlation 0.043/0.063; reversing vertical sign
alone gives 0.496/0.435; swapping pixel axes gives −0.016/−0.011. Half and double
scale give only 0.067–0.124. Qualified vertical motion spans approximately
−15.2 to +9.1 pixels, so the evidence tests vertical sign as well as horizontal.
Three independent analytic-texture controls recover known positive, negative
and zero translations at the quarter-pixel search resolution.

The matcher approximates each patch with translation, while the actual field
varies within a patch. Its quarter-pixel search, interpolation, shading changes
and disocclusions limit precision. The ~0.12-pixel figures are measured
disagreement, **not absolute metrological accuracy**. There is no claim about
independently moving characters, all rendering passes, camera cuts or other scenes.

Scripts `paired_data.py`, `measure_43820.py`, `verify_alignment_43820.py` and
`plot_alignment_43820.py`, all raw files, full patch results and plots are retained
under the local evidence directory. `analysis-43820.json` records narrowly
scoped supported conclusions. The frozen review's broad
`color_motion_alignment_verified`, `motion_contents_verified`, typed-view
and `dlaa_ready` flags remain false; those flags are not rewritten to claim
more than the recorder proves. Final plot: `alignment-43820-v2.png`.

## Closure and next step

The user confirmed normal exit. At `2026-09-25T14:33:33.0956426+03:00`, the exact
previous slot DLL was restored (SHA-256
`689bef7cf00ff71e4518f452f0467edf6c3d877f2a6dd090553388d8b5f288cc`),
and the temporary output-directory sidecar was removed. Graphics settings,
profile settings, mod list and mode sidecar hashes are unchanged. The 142-file
prelaunch manifest and 115 archived source files verify. No next test is installed.

Next: combine this validated color/motion capture with the previously verified
bounded projection stimulus. With the camera stationary, compare baseline,
known injected jitter and recovery images to establish how jitter reaches color
and whether motion includes that jitter displacement. Preserve same-frame depth
association and typed input/output view requirements when assembling the first
bounded native-resolution DLAA evaluation. Sustained rendering and presentation
remain later integration work; this run does not evaluate an SDK or replace TAA.
