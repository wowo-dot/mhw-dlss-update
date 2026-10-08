# Shader census

2026-10-08, game-43 (v0.13.4 alpha loaded, title screen). Private outputs:
`local/shader-census-20261008` (census.py, classify.py, census-2/,
classified.json). No shader bytecode is part of this document.

## How

A read-only external reader (`OpenProcess` query/VM_READ, `ReadProcessMemory`;
no writes, suspension, remote threads or COM calls) walks the shader system's
technique table. It first checks three code spans against the pinned decrypted
image (`23D7600` loader, `23D7DE0` handle lookup, `23D0830` state builder).

| Location | Meaning |
| --- | --- |
| image `0x51C73D0` | Shader system |
| system `+0x38` | Loaded shader package |
| package `+0xF4` | Technique count |
| package `+0xF8` | Program count |
| package `+0xB8` | 32-byte technique entries (`+0` name) |
| system `+0x2458` | Per-technique shader sets (`23D7DE0` hands these out as handle `+0`) |
| set `+0x50/58/60/68/70/78` | VS/PS/GS/HS/DS/CS entries; entry `+0x10` descriptor {u32 size, u64 data} |
| set `+0x80/+0x88` | Global binding IDs and owner count; `+0`, `+8`, `+0x14`, `+0x44` binding words and ranges |

**Result:** 45,887 named techniques and 10,425 unique programs, equal to the
package's program count (228 MB), all loaded at the title screen. The read took
64 s; the reader re-reads no blob once its data pointer has been seen.

Global binding IDs are shared across techniques:

| ID | Binding |
| --- | --- |
| 11 | CBViewProjection |
| 12 | CBScreen |
| 69 | tDepthMap |
| 162 | SSFilter |
| 193 | tBaseMap |

## Techniques already handled

| Stage | Technique |
| --- | --- |
| motion | `MotionBlurStandard` |
| tone map / grade | `ToneMappingApply_ColorGrading` |
| final / copy | `System_Copy` |
| haze | `Haze` |
| radial | `RadialBlur_Default` |
| DOF composite | `DepthOfField_Blend` |
| depth colour | `DepthColor` |

## Post-process families (classify.py)

Screen-addressing features per pixel or compute shader: UV samples, loads at
the raster position (`SV_Position`), CBScreen fields used, bound textures.

| Family | Techniques | Shape | Notes |
| --- | --- | --- | --- |
| ColorCorrectCube (`uColorCorrectCube`) | `ColorCorrect`, `ColorCorrectCube{Simple,Fixed,Table,Sophia}` | Filter VS; scene UV sample; LUTs; depth or raster loads; 17–33 instructions | Same shape as depth colour |
| Motion blur variants | `MotionBlurWeak`, `*CBR`, `*Capture`, `*ForPublication`, `MotionBlur` | Same structure as Standard | Only Standard's hash is admitted |
| Blink (`uBlinkFilter`) | `Blink` | Copy VS; no textures; 21 instructions | Constant full-screen draw |
| ImagePlane (`uImagePlaneFilter`) | `ImagePlane`, `ImagePlane4KCBR` | Own VS; tBaseMap, cube map, temp map | Needs its own analysis |
| Circular DOF (`uCircularDOFFilter`) | 48 `PS_CircularDOF_*` | Multi-pass convolutions, half and full size | Likely cutscenes |
| Radial occlusion | `RadialBlur_Occlusion` | Copy VS; reads depth; writes the mask `RadialBlur_Default` samples | Probably the Safi/Alatreon custom mask |
| God rays | 8 `GodRays*` | Copy VS; multi-pass | The composite is already admitted as an order-1024 overlay |
| TAA3, FXAA, combined | `CSTemporalAA3_*`, `FXAA`, `CombinedFilter_*` | Compute / PS | Other AA settings only |
| Bloom, tone map helpers | `Bloom*`, `FFTBloom_*`, `ToneMapping*` | PS / CS | Not filter-chain stages in observed graphs |

## Next

1. **A generated adapter for single-pass screen filters (filter VS family).**
   It applies three rules:
   - every raster-position read uses the render lattice;
   - every scene (t0) UV sample expands to the output;
   - a t0 load is admitted only at the raster position.

   Each generated shader stays hash-gated and gets the depth-colour fixture's
   native oracle. Regenerating `DepthColor` must reproduce the hand-written
   adapter.
2. **Generalise dependency kind 4 into "approved screen filter" at any chain
   position:** before motion, or between motion/haze and grade.
3. **Then the multi-pass families:** circular DOF, then radial occlusion.
