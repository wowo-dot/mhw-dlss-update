# Full-size color with native render-size motion filtering

Implemented offline on 2026-09-28. This component is not installed in the game.
The v1.23.10 runtime recorder captures the remaining color-grading pass; it does
not evaluate DLSS or substitute this shader.

`scaled_temporal::make_filter_shader` accepts only the 10,008-byte pixel shader
observed in all three v1.23.9 frames, SHA-256
`3eed9b57bbbdf38d14d9a94596fc93a107aa99eab275af7118b95febbbc1ba18`.
The game shader is provided by the caller and qualified before parsing. No
native shader bytecode is embedded in the adapter or distributed with it.

The private variant keeps the original program, resource slots, buffer layout,
signatures, branches, blur weights and auxiliary reads. Three extra temporary
registers provide render-space raster position, full-size normalized color
coordinates, and the full-size integer color pixel. Only four t0 samples and
two t0 loads are redirected. Native t1 depth, t2 packed motion, t3 fur motion
and t4 neighbor motion remain in their original coordinate system. Reflection
is retained; obsolete instruction statistics are removed. A pinned, unmodified
Microsoft DXBC checksum implementation rebuilds the private container checksum.
Its license and file identities are under `third_party/dxc_hash/`.

The first contract supports static 2x output and a 1x control, top-left origin,
allocation equal to output, unchanged native UV0..1, unchanged render-space
constants/auxiliary textures, full-size t0, and a full-output viewport. The
caller must qualify these native bindings. Other ratios, shader variants,
subviews and dynamic resolution are rejected. The adapter never patches a
native shader, game executable, PSO, or command by itself.

The original normalized-coordinate edge clamps remain. At 2x, a native clamp
at the last render-pixel center maps to half a full-size pixel inward from the
last output center. The fixture explicitly verifies that behavior; this is not
a claim of ideal edge reconstruction or moving-scene quality.

## Verification

The standalone fixture takes a private captured shader path and an unused
output directory. No game process or SDK is involved. It uses D3D12 validation
and compares the adapted shader against the original at 1x for four controlled
motion conditions. The 2x cases check all pixels against a CPU bilinear oracle,
including native edge clamps, full-resolution integer alpha, constant-color
blur invariance, poisoned auxiliary padding, and untouched resource bindings.

Two additional varying-color blur cases use a separate texture-layout oracle:
disable the four color-coordinate expansions and put the same full-size image
in the top-left quarter of a double-size texture. Hardware normalized sampling
must produce the same texels as the adapted shader with a normal-size texture.
This tests both blur loops without translating the proprietary blur equations.

Final WARP and NVIDIA hardware results are retained in
`local/scaled-filter-20260928/{warp-complete,hardware-complete}/`. Each run has
16 draws, 229,376 float comparisons, 13 rejected shader/geometry inputs, exact
pixel results, and zero GPU warnings/errors. Resource reflection matches the
native shader. Every draw finishes and its recording is reset before release.
The current hardware adapter LUID is [0,93935]; the old session's [0,98547] no
longer resolves after restart. Device/vendor are recorded with each result.

Earlier fixture failures are retained: missing unused TexCoord1 linkage, an
unoptimized render-target clear warning, and the obsolete adapter LUID. They
were fixture issues, not game failures. The build passes /W4 /WX in project code.

This does not prove native GPU bindings, full-size color grading, final-screen
presentation, live DLSS, or performance. Those remain integration work.

Format reference: [Microsoft's tokenized program definitions](https://github.com/microsoft/DirectXShaderCompiler/blob/69c311af7091953427a6d67c16e811ce65313b93/include/dxc/Support/d3d12TokenizedProgramFormat.hpp).
Checksum source: [Microsoft DxilHash](https://github.com/microsoft/DirectXShaderCompiler/blob/69c311af7091953427a6d67c16e811ce65313b93/lib/DxilHash/DxilHash.cpp).
