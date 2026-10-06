# Gallery compute bundle captured (v1.25.35)

Run on 2026-10-01, PID 23128 / creation 134353170129823683. Evidence:
`local/dof-compute-capture-20261001`. DLL SHA-256:
`837fa604b0f2310b18aea1b4eaa2f7091579734b663849b3bc7b5a4a516af78a`.
The [capture contract](dof-compute-bundle.md) describes its bounded CPU scope.

## Result

Both complete bundles were captured at frames 17155 and 17215, with two
attempts and zero capture errors. Both preserve `[11,6,6,6,6,6,6,0]`, the same
six compute shader hashes and the composition shader/geometry. Eight unique
DXBC files (six CS, one VS, one PS) were extracted privately; all six CS files
were disassembled through the Windows compiler runtime. The reviewer checks
typed packet coverage, order, shader stages, and resource/descriptor identity.

Gallery still suspends reconstruction, as expected for this diagnostic build.
No compute route was admitted and no DOF composition replacement ran. After
leaving Gallery, the user reported a clean image; counters confirmed recovery
with zero runtime faults. This is a successful capture, not a Gallery fix.

## Confirmed dependency chain

Names below come from captured shader reflection. Dimensions are native CPU
texture metadata, not independently verified D3D12 resource descriptions.

| Pass | Inputs -> outputs | Group dimensions | Group size | CS SHA prefix |
|---|---|---|---|---|
| Horizontal depth/blur-radius reduction | depth 2560x1440 -> TileMaxH 128x1440 | 8x1440x1 | 320x1x1 | `98c622bab8c0e113` |
| Vertical reduction | TileMaxH -> TileMax 128x72 | 128x5x1 | 1x320x1 | `d2c5eca59598bd0e` |
| Neighborhood reduction | TileMax -> Neighbor 128x72 | 16x9x1 | 8x8x1 | `4f54cf139cc8222b` |
| Blur preparation | depth + HDR scene + Neighbor -> CoC and Image, both 1280x720 | 80x45x1 | 16x16x1 | `b7afd17a818940e9` |
| DOF filtering | CoC + Image + Neighbor -> packed DOF image and DOFAImage, both 1280x720 | 80x45x1 | 16x16x1 | `c4c5caf1ae7f1cdf` |
| Unpack | packed DOF image -> Image 1280x720 | 80x45x1 | 16x16x1 | `61b739ad7861bffa` |

The composition draw samples CoC, the **latest** Image writer (unpack), the
external HDR scene, Neighbor, and DOFAImage. The extractor resolves eleven
earlier-writer-to-reader identity edges per bundle. Its four external reads
are depth in passes 1/4 and scene color in pass 4/composition. The scene owner
in pass 4's t1 and composition's t2 equals the descriptor's source owner.
Image is deliberately reused: preparation writes it, filtering reads it,
unpack overwrites it, and composition reads that later version. This ordering
must survive integration; a one-producer-per-resource map would be wrong.

Disassembly shows that pass 4 doubles thread coordinates, clamps them using
`CBScreen.iViewSize`, gathers depth using `fScreenInverseSize`, and performs
four integer `Load`s of HDRImage for a 2x2 block. Pass 1 also uses integer depth
loads and `fScreenSize`. Thus the external scene/depth sampling contract needs
explicit review; it is not solely a final-draw viewport issue.

## Limits and next implementation work

Actual CBScreen/CBNewDOFFilter bytes were not part of this diagnostic. Their
owners/modes and binding words were captured, but old command-arena pointers
must not be dereferenced later. Full allocation size does not prove the valid
rendered footprint. GPU resource states, descriptors and execution of these
compute stages have not been independently validated. In particular, we have
not yet proved whether native constants describe render size or display size
at each pass in the reduced-resolution path.

Next extend the offline GPU fixture to the entire six-compute-plus-composition
pipeline, using these private shader inputs. Establish the native constants,
depth/color footprint and chronological producer contracts before changing
admission. Preserve the artistic DOF parameters. Isolate necessary input
coordinate adaptations from internal auxiliary passes, retain resources through
submission/fences, and test the complete output against an independent native
reference. Do not simply permit opcode 6.

## Lifecycle

Main-menu baseline: 776 DLSS frames, two startup skipped dispatches. Loaded
scene: 1483 frames, no additional skips. Gallery: reconstruction waiting,
with no runtime fault. Recovery checkpoint: 2399 frames, waiting false.

Intentional stop: 3264 DLSS frames, 2249 skipped dispatches (not a displayed
frame count), zero faults. All 5513 main GPU signals completed; all 5512 main
and 2248 unfiltered recordings released, and all 3292 output recordings
released. All 12379 jitter edits and full render scale restored. Frozen full
review and all eleven checkpoint component reviews pass. Independent live
checks verified forty full native code spans and eighteen hook heads restored.

The collector finished at native termination: 33 timed snapshots/events,
7,089,714 bytes, no read retries or fault observation. Timeline SHA-256:
`ec9f5c8051e9f379071ff26536bef8263899dcd96ed58191266318bdb5f8cabd`.
The user confirmed a normal scene and normal exit. Keep this DLL installed.
Before another launch, use a fresh output directory so the sealed run cannot
be overwritten. Camp remains deferred; monster effects and silhouette follow
the Gallery integration.
