# Haze enqueue recipe captured successfully

2026-09-30, v1.25.9, PID 48080 / creation 134351952741962897.
Evidence: `local/scaled-haze-bindings-20260930`.

## Confirmed result

The corrected recorder accepted three same-frame recipes in three attempts,
with no capture rejections. Renderer frames 15795, 15855 and 15915 are separated
by the configured 60-frame minimum. Each records exactly one appended native
draw with the expected haze key, shader identities, pipeline and named texture
owner agreement. Repeated reads within each callback were equal; the recipes
also remained immutable throughout the subsequent log. The first sample's
source/destination agree with the same-frame graph's missing connection:
motion output -> haze -> color-grade input.

All three use helper and draw rectangle [0,0,1280,720], allocation dimensions
[2560,1440], zero coordinate offset, vertex count 3 and first vertex 12. The
16-byte layout is POSITION float2 at offset 0 and TEXCOORD float2 at offset 8.
The triangle's position/UV tuples are (-1,1,0,0), (-1,-3,0,1), (3,1,1,0).
The hash-verified native VS passes position and UV through, so these coordinates
interpolate UV 0..0.5 across the viewport. This supports the adapter's planned
full-output viewport with only final color sampling coordinates scaled by two;
it does not itself prove an integrated GPU result.

VS (712 bytes): c7c73b6ac661514c34c53681e217dc3fe7014e21a99a41adb0fcb7befc1f5de6.
PS (14596 bytes): 4f890ffa89ea59a218db7e191d8aab48e9d5fe4be200c50a606b22594bb57f69.
The corrected blocks contain zero VS bindings and nine PS bindings: two sampler,
four SRV and three constant-buffer entries. The metadata matches the sentinel
values that broke v1.25.8. PS t0..t3 resolve to owner-table indices 2..5 and match
the named native context slots. Engine metadata dimensions are:

| Register | Input | Extent |
| --- | --- | --- |
| t0 | Color from motion filtering | 2560 x 1440 |
| t1 | Depth | 2560 x 1440 |
| t2 | Volume input | 32 x 32 |
| t3 | Default filter input (custom absent) | 1 x 1 |

All source CPU descriptor handles are nonzero. Color source/destination owners,
layout, shader identities, geometry, pipeline state fields, packed bindings,
and all four texture metadata records are equal across the three samples.
Haze flags are [0,1,1,0]. Strength ramps from about 0.0037002 to 0.075, then stays
0.075. Constant-buffer contents were not copied.

These are same-frame CPU enqueue recipes. GPU descriptor contents, GPU resource
descriptions, execution-time bindings and resource states remain unverified by
this channel. The capture adds no COM references, GPU work or rendering changes.

## Run and cleanup

1,897 successful DLSS evaluations; zero runtime faults or CPU recording-order
rejections. The 1,670 unfiltered frames comprise 1,646 unbuilt haze plans,
22 consumer-depth admission rejections, one startup warmup and one intentional
stop. The depth rejections report reason 5 (unsupported expected depth-use
span/representation); this bounded trace does not identify the precise branch.
Both suspension periods recovered. The user reported normal recovery outside
the sauna, normal scene after intentional stop, and normal exit.

All 3,545 reconstruction/depth GPU recordings and 1,670 unfiltered recordings
completed and released. All 3,568 jitter changes were restored. Full resolution
returned. The final JSONL status (not the collector's earlier latest-status
snapshot) confirms cleanup; independent read-only checks matched 38 native
function spans and 17 hook heads. There were 120 camera-displacement history
resets, retained as a separate panning issue rather than treated as haze faults.

The DLL and game/profile settings remain unchanged after this run. No new
rendering build was installed. The archive retains the exact v1.25.9 sources,
reviewers, deployment receipt, three captures, checkpoints and cleanup proof.

## Next integration

Add the optional haze draw between motion filtering and color grading. Validate
and retain its actual execution-time resources, descriptors and pipeline before
replacement. Extend plan ordering/lifetime handling to four effects while
retaining the existing three-effect route when haze is absent. Apply the tested
haze PS adapter to the full-size color only; preserve depth/noise/filter sampling
and native constants. Check the actual native VS/triangle in the GPU fixture
and exercise both paths before another runtime sauna test. Do not admit the
five-descriptor graph merely by relaxing its current link checks.

Keep v1.25.9 installed. Its current output directory becomes sealed when
archived; redirect the output sidecar into a new evidence directory before the
next launch. Panning history, early CPU recording and performance cleanup remain
separate work. The sauna rendering issue is not fixed by this capture alone.
