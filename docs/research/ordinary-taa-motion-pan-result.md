# Ordinary TAA motion during a rightward camera pan

Accepted and closed 2026-09-25: **v1.4.0, PID 34276**, creation
`134348079721948472`. Three full-resolution motion images pass the frozen
reviewer. Unlike the [stationary run](ordinary-taa-motion-result.md), every
pixel decodes to a finite nonzero vector. The broad scenery field and majority
positive history-X displacement support camera-motion coverage and the
predicted horizontal direction in this tested view. Quantitative scale,
frame correspondence, jitter handling and DLAA remain unverified.

## Provenance and capture proof

Evidence: `local/temporal-motion-20260925/v1.4.0-pan-right/`.
The DLL and reviewer are unchanged from run 48724; the original 49 native
and 26 Python tests are reused. The sealed stationary archive's 151 files
and this deployment's 123 prelaunch files verify; 112 sources are archived.
Host version `0.0.0.0`, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
DLL SHA-256 `dc0c450cc30e529fd0a2316222e4b83b94f0a1c918c4f3c27cc7a2a9faf0fbac`.
Log SHA-256 `32a54ae97d01308cc425187e9b3eeac6b1d3d1a78d5944c680eb5fe1989d119c`.
The 15,492-byte shader remains identical to run 4376, SHA-256
`2d7b26c742c27db2c83546c6cf6a1dfda47d7cfd4336e69f8c2302de5802b0d1`.

The user was instructed to keep the character stationary and gently hold the
camera stick right, replying from the phone while the pan continued. They
reported "Panning" before arming. No camera angle, angular velocity or renderer
frame timestep was captured. DLSS Off/TAA On, DX12, 2560×1440 and the bare
profile were verified unchanged.

Frames/epochs **17696–17698**, banks 0/1/0, match the actual native Dispatch
calls at 160×90×1. Producer thread 32380 and consumer thread 9920 differ.
One pinned R32_UINT source, DIRECT list and queue are shared across three
distinct Reset/allocator generations. Each source has one mip/slice/sample;
each 14,745,600-byte image has verified format, dimensions, length and hash.
Each native full-resource transition is `0x4 -> 0xC0`; private copies restore
`0xC0` before the original Dispatch. Every selected Execute contains two lists.

| Frame | Reset | Dispatch marker | Copy begin/end | Original forward | Execute | Signal | Completed | Retirement Reset |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 17696 | 2325 | 2349 | 2350 / 2351 | 2352 | 2360 | 2362 | 2435 | 2406 |
| 17697 | 2406 | 2430 | 2431 / 2432 | 2433 | 2442 | 2444 | 2614 | 2488 |
| 17698 | 2488 | 2512 | 2513 / 2514 | 2515 | 2523 | 2525 | 2615 | 2563 |

These are journal orders. All three private fences completed at one and the
selected list generations retired successfully without intervening submission.
The journal used 1299/4096 events: 529 Reset, 531 Close, 236 Execute and three
Dispatch markers. It tracked 21 lists, 44 allocators and one queue, with 502
submitted pointers and maximum batch six. Final completion and last event were
204 ms after capture start; zero events entered after final completion.
Producer request timestamps are 15 and 16 ms apart, but those timestamps are
not a measurement of the renderer's camera timestep.

Zero faults/pending callbacks; all hooks disabled. Ordinary resource pins
21/21, consumer-list pins 3/3, lifecycle cumulative pins 292/292 released.
Copy-owned references remained retained until the subsequent normal exit.

## Scoped numerical and spatial observations

The archived shader decode is unchanged:
`dx=half(p>>16)`, `dy=half(p&0xfffe)`;
history pixel displacement is `(-dx*1280,+dy*720)` at this resolution.
All **11,059,200 vectors are finite and nonzero**, compared with 6.86–6.91%
nonzero pixels in the stationary run. The two runs are not pixel-aligned;
the comparison is of coverage/statistics, not a pointwise subtraction.

| Frame | Positive history X | Median history X | Median history Y | Maximum vector magnitude |
| --- | --- | --- | --- | --- |
| 17696 | 94.2818% | +9.44336 px | +0.00017 px | 20.48113 px |
| 17697 | 94.3038% | +33.22266 px | −0.01090 px | 74.02572 px |
| 17698 | 94.3902% | +33.51563 px | −0.01322 px | 73.36718 px |

Positive history X was predicted before the test: with ordinary rightward
camera yaw, static scenery scrolls left, so its previous screen coordinate is
to the right of its current coordinate. The observed majority sign agrees.
About 5.6–5.7% have negative X; a third-person camera orbit and foreground
animation do not imply a uniform sign at every pixel. This run does not assign
the negative region to a particular cause.

In a fixed geometric perimeter (excluding the center half of width and height),
94.90–94.94% of X values are positive. This is not a semantic background mask.
The local `motion-34276.png` shows broad spatially coherent scenery motion,
recognizable geometric boundaries, and smaller character-region motion.
The visualization uses the same scale for the three pan images; its scale
differs from the stationary plot and must not be compared by color alone.

The magnitude changes substantially between the first and second frames.
Without camera transforms, renderer timestep or paired color displacement,
that change cannot be attributed to input acceleration, frame timing or a
scale error. Reported pixel units are shader-derived, not independently
calibrated. The low packed bit is set on 21,291 / 21,139 / 21,118 pixels;
its meaning remains unknown and it is masked exactly as in the shader.

## Closure and next step

The user confirmed normal exit. At 14:08:54 +03 the exact prior diagnostic DLL
was restored (`689bef7cf00ff71e4518f452f0467edf6c3d877f2a6dd090553388d8b5f288cc`),
the temporary output sidecar removed, and all four configuration hashes remained
unchanged. The final archive preserves this run and its source/analysis chain.
No new runtime test is installed or armed.

Next useful diagnostic: matched current-color and motion readbacks over the
same recorded frames during controlled camera movement. Compare actual static
image displacement against the decoded vectors to establish scale and frame
alignment; account for disocclusions and independently moving objects.
Jitter inclusion still needs its own controlled evidence before choosing SDK
input conventions. Typed descriptor views, constant-buffer mode, sustained
input lifetime and the output replacement path remain separate integration
work. Broad `motion_contents_verified` and `dlaa_ready` remain false; the
scoped result is camera-motion coverage/direction support in this tested view.
