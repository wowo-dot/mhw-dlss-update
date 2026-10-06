# Gallery DOF coordinate replay with captured constants

2026-10-01. Offline continuation of the
[v1.25.36 constant capture](dof-pipeline-constants-result.md). No runtime code,
installed DLL, game settings, or compute admission policy changed.

## Result

Both captured frames, 11342 and 11402, pass on NVIDIA hardware and WARP with
GPU validation enabled. The experimental preparation adapter and the existing
composition adapter reproduce the independent native-shader reference with
zero maximum pixel delta. All seven retained intermediate images agree;
R8 alpha is compared as bytes, not as floating-point values. This establishes
the tested 2x coordinate transformation with synthetic pixels, not a live
Gallery fix or equivalence to the game's resource formats.

| Captured frame | Device | Final maximum delta | Wrong color coordinates | Missing Image overwrite | Changed outside-view depth |
|---|---|---:|---:|---:|---:|
| 11342 | NVIDIA | 0 | 0.046875 | 0.0344849 | 0.00200339 |
| 11342 | WARP | 0 | 0.046875 | 0.0344849 | 0.00200340 |
| 11402 | NVIDIA | 0 | 0.0520881 | 0.0363770 | 0.00290553 |
| 11402 | WARP | 0 | 0.0520881 | 0.0363770 | 0.00290552 |

Deltas compare each case with its reference on the **same device**. They are
not WARP-versus-NVIDIA equivalence measurements. The existing three synthetic
pipeline cases also pass on both devices. The targeted C++20 build passes
`/W4 /WX`; malformed and altered shader inputs are rejected.

## Experiment and independent reference

`tools/dof_pipeline_fixture.cpp` accepts an optional private 1792-byte constant
pack. It contains the captured projection, screen and blur buffers at offsets
0, 1280 and 1536. Extraction verifies each raw buffer's hash and agreement
between its bindings within the captured frame. Both frames keep their own
projection, animated blur and vignette values. Screen allocation is 2560x1440,
active view 1280x720, with zero offsets. Other geometries are rejected.

`tools/dof_prepare_experiment.hpp` is an offline-only exact-hash DXBC adapter.
Each of the preparation shader's four integer HDR reads becomes a 2x2 box
average of reconstructed display color. It preserves the original shader's
depth reads, clamp, CoC calculation, sample selection, weights, artistic
constants and binding layout. There is no added sampler and no live caller.
Shader reflection and native/adapted PSO creation are checked.

The reference uses the original six compute shaders and a separately
CPU-downsampled render-space color proxy. Thus its preparation needs no
coordinate adapter. Its final draw uses the original composition shader,
an independently transformed screen buffer and a padded scene texture. The
candidate uses the unchanged captured constants, adapted preparation and the
existing production composition adapter. Both preserve the complete native
order, including Image preparation, packed blur and Image overwrite by unpack.

Independent CPU oracles additionally check horizontal and vertical depth
reductions and preparation color. The high-frequency color pattern and its
2x2/4x4 averages are exactly representable; the depth pattern is constant
inside 20-pixel tiles and varies across tiles. Omitting the coordinate
correction or the Image overwrite changes the final result, as shown above.
No captured live scene pixels were used.

The initial hardware attempt stopped on a fixture comparison error: four
packed R8 alpha bytes may form a NaN if interpreted as a float. The readback
storage was valid; numeric vector equality was the wrong comparison. Changing
only that check to compare representations allowed the reference comparison
to finish. The failed attempt is retained alongside the successful runs.

## Valid-footprint finding

The input depth allocation is display-sized, while the active view is half
size in each dimension. The native reduction dispatch covers the full depth
allocation. Replacing depth outside the view from 0.99 to 0.001 changes the
reduction/neighbor chain, alpha, unpacked blur and final pixels. In contrast,
color preparation remains unchanged when its outside-view region is poisoned:
the original view clamp bounds those reads.

This proves sensitivity in the fixture, not that the game supplies invalid
depth. Do not silently zero, replicate, discard or reinterpret the unused
part of a native texture. Its live contents and ownership still need evidence.
Preserving only the visible 1280x720 depth pixels would not reproduce the
complete input contract exercised here.

Float intermediates use RGBA32F, depth R32F, packed output R32_UINT, alpha
R8_UNORM. Actual native GPU descriptors, formats, subresources, resource states
and valid pixel footprints remain unverified. The fixture owns all resources,
explicitly transitions them, and retains each case's resources and command
storage until completion. That does not prove the equivalent live lifetimes.

## Reproduce and continue

Build target `dof_pipeline_fixture`. Run each frame/device as a separate process
with a new output directory (existing directories are rejected):

```text
dof_pipeline_fixture.exe hardware PRIVATE_SHADER_DIRECTORY NEW_OUTPUT_DIRECTORY FRAME_CONSTANT_PACK
dof_pipeline_fixture.exe warp PRIVATE_SHADER_DIRECTORY NEW_OUTPUT_DIRECTORY FRAME_CONSTANT_PACK
```

Omit the pack for the original three synthetic cases. Private evidence is in
`local/dof-coordinate-replay-20261001/`, including constant-pack provenance,
source snapshots, build logs, adapter disassembly, per-case reports and outputs.
The captured inputs remain in the sealed `local/dof-pipeline-20261001/` archive.

Next establish the native GPU resource contract for the known six-pass bundle,
then integrate its chronological dependencies and owned recording. Do not
admit arbitrary opcode-6 commands or remove the current rejection merely
because these offline checks pass. Gallery still suspends in v1.25.36.

Keep v1.25.36 installed (SHA-256
`354128a6bdaa91ac488cb3c5919f7f0a404948010990f038ac8abd05d61d56b7`).
No new candidate is prepared. The old output root is sealed; a fresh root and
sidecar are required before the next game launch. Camp remains deferred.
