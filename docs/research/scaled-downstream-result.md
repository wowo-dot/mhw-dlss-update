# Native downstream scaling capture accepted

2026-09-28, v1.23.8, PID 43940 / creation 134350258564033909.
Frames 15458–15460 passed the frozen reviewer. The user reported a normal scene
and normal exit. Process exit, installed DLL, exact previous backup, unchanged
settings, frozen sources and prelaunch manifest were verified. The latest DLL
remains installed.

## Confirmed construction

The same three frames associate the native TAA final helper with the final
`System_Copy` producer. TAA's final copy again queues a 1280x720 rectangle from
each full-size ring output into one stable 2560x1440 R11G11B10_FLOAT destination.

The final draw has:

- Source rectangle `[0,0,1280,720]`, allocation dimensions `[2560,1440]`.
- Destination viewport `[0,0,2560,1440]`, triangle mode 1.
- UV offset `[-0.000195313,-0.000347222]`, approximately minus half a texel
  in each allocation dimension.
- One opcode-0 draw packet, stage 16, sort key `2147483648`.
- Source owner equal to the owner behind view+0x420; destination behind
  view+0x460. Both engine extents are 2560x1440, engine format 34.

The draw was recorded at native command index 59, before the later TAA copy
at indices 4243/4245/4244. Its stage sorts after TAA's stage 15. This resolves
the apparent reversal between CPU construction and intended GPU execution.

The assigned post-filter chain was identical in all three frames:

| Stage | Order | Unit vtable RVA | Role |
| ---: | ---: | --- | --- |
| 15 | 0 | 0x3538400 | Pre-TAA filter; static body includes exposure/tone mapping |
| 15 | 2 | 0x351bda8 | TAA input, ring output and final-copy destination |
| 15 | 100 | 0x3537a70 | Receives TAA's final-copy destination and writes the other intermediate |
| 15 | 134217713 | No unit | Assigned final endpoint from that intermediate into view+0x420 |
| 16 | 0 | System_Copy draw | Samples view+0x420 into view+0x460 |

The order-100 unit's execute method is RVA 0x24ca1f0 (complete span 792 bytes,
SHA-256 `ace3852211fb63c13cfca395d4992f922addc69db0841fa44e540819779e60aa`).
Its helpers reference `EvalTileMaxHorizontal`, `EvalTileMaxVertical`,
`EvalNeighborMax`, `mShutterSpeed` and `mBlurThreshold`: these identify a
motion-blur reconstruction path by inference from its implementation. The
capture itself records the vtable, order and owner associations, not a named
shader execution at that stage.

## Scope and next implementation

This verifies frame-matched **CPU construction and assigned ownership**. The
observer did not independently trace the later filter execution, final draw's
GPU consumption or presentation. New draw resource information is engine
metadata; the independent final-copy observer retains its COM/GetDesc checks.
No full-size output was injected and NGX was not evaluated.

The next integration must pass the reconstructed full-resolution image through
the order-100 filter and the final draw with matching rectangles and UVs.
Expanding only TAA's native copy would leave these later consumers using the
reduced region. Reuse the established native temporal route; do not restart
independent-AA pipeline work or parked beta polish.

## Input and shutdown verification

Nine raw and nine converted input images were retained. GPU submission,
completion and recording retirement passed. All 19,353,600 active float
comparisons had zero conversion error. Color states were proven by explicit
local transitions for all three frames; the supplemental native-current-use
proof was not needed in this run.

The projection viewport remained 2560x1440. One native jitter
`[0.390625,-0.3395061492919922]` was written and restored; its mapped render-pixel
SDK jitter is `[0.09765625,0.08487653732299805]`. Three frame associations passed.
Nine unowned projections and 33 pre-baseline depth requests were skipped.
Preview inspection shows a complete scene, qualitatively aligned depth and
motion concentrated around the animated character/weapon. It does not establish
motion-vector units/sign or subpixel raster jitter.

The test restored full scale after 408 overrides. All fault/pending counters
were zero and all 24 hooks were disabled. Pins released: input 24, native
consumer 3, depth 42, submission 297, final-copy 6. The new observer made zero
mutations and retained three of 38 observed primary-view draws. Successful
diagnostic readbacks remained allocated until verified process exit.

Evidence root: `local/scaled-temporal-20260928/v1.23.8/`.
Installed DLL SHA-256:
`78daa3778b39f1de9dde8eb988c3ed22d8203f85872c318ec89c742807a18356`.
Final log SHA-256:
`988d331b5c3d11b4b9f42b1bd7e86e3c3c4234e79453623e8d10b880a39b1345`.
Verified 568-file final archive SHA-256:
`e79d12c1cc5506f992263f1478c3a3600c853a87b40b29949329a4733df5de55`.
