# First ordinary TAA motion-pixel readback

Accepted and closed 2026-09-25: **v1.4.0, PID 48724**, creation
`134348067919451595`. All three full-resolution packed images passed the
frozen capture reviewer. Decoding produces finite, changing vectors with
spatial structure; controlled camera-motion and producer-coverage validation
remain outstanding. This is a successful motion-buffer readback, not DLAA.

## Capture identity and acceptance

Local evidence: `local/temporal-motion-20260925/v1.4.0/`.
Host version `0.0.0.0`, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
DLL SHA-256 `dc0c450cc30e529fd0a2316222e4b83b94f0a1c918c4f3c27cc7a2a9faf0fbac`.
Log SHA-256 `8030c1470d1ca9529707147673127a0a2de622dc96a738e8d5ca8dba0628af43`.
The 15,492-byte CS 5.0 shader is byte-identical to run 4376, SHA-256
`2d7b26c742c27db2c83546c6cf6a1dfda47d7cfd4336e69f8c2302de5802b0d1`.

Requested conditions were a loaded save, stationary camera, DLSS Off/TAA On,
DX12 at 2560×1440 in the bare profile. The user confirmed the save was loaded
and later confirmed normal exit. Camera movement itself was not instrumented;
world and character animation were not suspended.

Frames/epochs **12257–12259**, banks 1/0/1, each match the producer packet and
original native Dispatch (160×90×1). Producer thread 47288 and consumer thread
47576 are distinct. All three use one pinned GBuffer4 resource/canonical
identity and one DIRECT command list/queue, with three distinct Reset and
allocator generations. Every selected Execute batch contains two lists and
the selected list occurs once.

Each source is 2560×1440 `R32_UINT`, one mip/slice/sample, render-target flag.
The native consumer records a complete `0x4 -> 0xC0` transition, without state
conflict. The observer then records `0xC0 -> 0x800`, copies subresource zero,
and restores `0x800 -> 0xC0` before forwarding Dispatch. Each readback has
10,240-byte rows and exactly **14,745,600 bytes** of packed pixel data.

| Frame | Reset | Dispatch marker | Copy begin/end | Original forward | Execute | Signal | Completed | Retirement Reset |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 12257 | 1799 | 1823 | 1824 / 1825 | 1826 | 1834 | 1836 | 1909 | 1880 |
| 12258 | 1880 | 1904 | 1905 / 1906 | 1907 | 1916 | 1918 | 2061 | 1962 |
| 12259 | 1962 | 1986 | 1987 / 1988 | 1989 | 1997 | 1999 | 2062 | 2037 |

These are journal orders, not times. Each private fence was signaled once
from zero to one and polled complete; successful Reset retirement is also
present. Retirement can occur before the worker observes completion.
The frozen reviewer checks all returned Close/Execute/Reset intervals, source
associations, unique copy orders and artifact hashes. All 133 prelaunch
files verify, including the 111 archived source files.

The journal used 1022/4096 events: 417 Reset, 417 Close, 185 Execute and three
Dispatch markers; 21 tracked lists, 44 allocators and one queue. Final fence
completion and the last event are both within 203 ms of capture start.
There are **zero events admitted after the final completion**, compared with
1904 in the previous run. Closing admission before hook removal worked here.

Zero recorder faults/pending callbacks; all seven hooks disabled. The 21
ordinary resource pins, three consumer-list pins and 243 cumulative lifecycle
pins were released. Copy-owned source/list/allocator/device/readback references
remained retained until normal process exit, as designed. All three copies
reached saved phase with restoration, successful Map, exact length and hash.

## Pixel observations, separate from capture acceptance

`analyze_48724.py` applies the shader-derived decode:

```text
dx = binary16(p >> 16)
dy = binary16(p & 0xfffe)
history_pixel_delta = (-dx * width / 2, +dy * height / 2)
```

All **11,059,200 decoded vectors** are finite. The archived scalar decoder
agrees with 101 deterministic sample positions per image from the vectorized
analysis. Signed zero is treated as zero; raw nonzero words are not sufficient
evidence of motion. Common words include `0x80000000`, `0x00008000` and
`0x80008000`, which decode to signed-zero components.

| Frame | Nonzero vectors | Nonzero pixels | 99th percentile magnitude, all pixels | Maximum magnitude |
| --- | --- | --- | --- | --- |
| 12257 | 253,377 | 6.8733% | 0.11894 px | 1.97601 px |
| 12258 | 254,580 | 6.9059% | 0.31445 px | 1.41365 px |
| 12259 | 252,826 | 6.8583% | 0.33353 px | 0.88034 px |

Between the first/second frames, 255,071 vectors change; between the
second/third, 254,318 change. The local `motion-48724.png` plots magnitude
and signed X/Y at fixed scales for all frames. Nonzero regions form a clear
central character/equipment-shaped silhouette plus smaller background regions.
That is consistent with animation during a stationary-camera request. There
is no paired color image or controlled object motion proving correspondence.

The low packed bit is set on 14,664 / 14,675 / 14,672 pixels. It is excluded
from Y decoding exactly as in the shader; its semantic meaning remains unknown.
No new validity-mask interpretation is assigned.

## Closure and next discrimination

Normal exit confirmed. At 13:48:21 +03 the exact prior slot DLL was restored
(`689bef7cf00ff71e4518f452f0467edf6c3d877f2a6dd090553388d8b5f288cc`),
the temporary output sidecar removed, and all four configuration hashes
remained unchanged. No diagnostic is installed or armed for another run.
Full restoration and final file/source manifests are retained locally.

Next: reuse the tested capture implementation in a fresh reversible deployment
and record a deliberate, steady horizontal camera pan. Check whether the
previously static background acquires coherent motion and whether its direction
agrees with the shader's history displacement. A second direction or paired
color/depth/matrix observation may be needed to establish scale and alignment.
Do not infer camera coverage from the stationary run. Constant-buffer mode,
jitter inclusion, disocclusions/transparent coverage, typed descriptor views,
and sustained renderer integration remain unverified. No vendor SDK evaluated
these images; `motion_contents_verified` and `dlaa_ready` remain false.
