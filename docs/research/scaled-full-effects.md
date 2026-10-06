# Full-resolution native effects

Implemented and verified offline on 2026-09-28. The scaled SDK resolve now
feeds a GPU integration test of the game's motion filter, color grading and
final screen at 2560x1440. The production effect component is implemented;
the live draw/copy routing is not connected. No new runtime DLL was installed.

## Implementation and provenance

`src/scaled_temporal/effects.*` builds private graphics pipelines and immutable
vertices from a qualified native recipe. Its bytecode inputs come from the
accepted [v1.23.12 draw capture](scaled-draw-execution-result.md), not embedded
game shaders. The five VS/PS identities, layout, blend, depth and raster recipe
are checked before pipeline creation. The tested host image is SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.

The motion-filter pipeline uses the already-verified
[color-coordinate adapter](scaled-filter-adapter.md). Its native UV and
constants remain in render space; only color reads and output coverage expand.
Depth, motion, fur and neighborhood inputs retain their original coordinates.
Color grading retains its native pixel shader, tone mapping, LUT and HDR
controls, with full-output UVs. Final screen retains its native shaders and
source-mip constant, with the observed half-texel sampling convention expanded
to the complete source image. The admitted geometry is top-left, same-aspect
1x or 2x, within the existing filter's allocation limits. This is not arbitrary
resolution-ratio support.

`Effect::record` replaces one qualified DrawInstanced. It changes PSO, vertex
buffer slot zero, viewport and scissor, then restores those four bindings.
It leaves root signatures/arguments, descriptor heaps, topology, render targets
and resource states untouched. Before its first command it checks the device,
resource geometry/format, original vertex-buffer range, native draw counts,
topology, viewport and scissor. Unqualified calls return false without commands.
The caller must forward those calls and must prove actual API bindings and
render-target state; cached native state alone does not authorize replacement.

References are bounded and retained across recording. Retirement requires a
same-device completed fence and proof that every containing recording retired.
Unproved recorded uses retain their references rather than releasing objects
still referenced by GPU commands. This component does not establish the game's
queue/recording lifetime proof on behalf of its caller.

## API binding observation

The existing state observer has an optional raster extension: 22 hooks instead
of its default 18. The four added Windows x64 COM entry points are
IASetPrimitiveTopology (slot20), RSSetViewports (21), RSSetScissorRects (22), and
IASetVertexBuffers (44). Calls forward with their original arguments and record
successful updates. No absolute D3D12 implementation addresses are stored.

The owner continues to provide successful DIRECT-list Reset generations and
pin each admitted list. ClearState clears raster knowledge without changing
the Reset generation. Multiple viewports/scissors, an unbound slot zero, or
unknown bundle mutations cannot authorize a draw. Updating a different vertex
slot preserves the known slot zero. Reset invalidates all old raster state.
These hooks were tested on WARP and the NVIDIA adapter; they have not yet been
enabled in the installed game recorder. The original 18-hook mode remains the
default and passes its existing GPU regression.

## Verification

Final builds passed /W4 /WX. Three focused CTest cases passed: effect geometry,
binding logic and the original WARP output/binding-restoration fixture. Geometry
checks cover 24 supported configurations and 22 rejected configurations,
including odd dimensions and the minimum-size boundary.

The effect fixture passed on WARP and NVIDIA LUID [0,93935], vendor4318,
device11141. Each run covers:

- 66 draws, with 147,456 exact packed-pixel comparisons across 1x/2x geometry,
  all three effects, tone-curve/LUT/HDR cases, and the three-stage output chain.
- 381 rejected recipe, binding, and retirement cases before unsafe commands
  or releases. Original vertices start at a nonzero offset, exposing a wrong
  offset restoration in the following native draw.
- 15 API state admission/recovery checks, including ClearState, multiple
  viewports/scissors, unbinding, another vertex slot, bundles and Reset.
- Poisoned output/padding, a subsequent native draw without rebinding the four
  restored states, complete fence/recording retirement, and zero GPU validation
  warnings/errors.

The existing inline SDK fixture also passed three combined hardware cases:
normal Performance DLSS, spatial-only output, and spatial recovery after an
injected failure following SDK handoff. Each runs three resolve frames, then
passes the final packed GPU resource directly through all three native effects
on the same device. Intermediate transfers stay on the GPU; readback is used
only for comparison. Each chain compares 11,059,200 full-resolution packed
pixels exactly against separately recorded reference draws: 33,177,600 in total.
Bindings are restored and all fixture GPU work/recordings retire. GPU validation
reports zero warnings/errors in all three cases.

These fixtures use captured shaders/images with synthetic effect constants,
descriptors and auxiliary inputs. They prove implementation, transport,
coverage, state restoration and selected effect branches. They do not prove
the live descriptor/root contract, a moving scene, native downstream queue
retirement, or game performance. The reference motion filter shares the adapter
already independently tested in the earlier filter fixture; its equivalence
is not newly established by comparing two instances of that adapter.

Initial rejected fixture output is preserved: an optimized-clear mismatch
produced debug warnings despite exact pixels; matching the resource's clear
value fixed it. The initial geometry test also mistakenly classified 17x9 as
supported despite the 32-pixel minimum; it is now a rejected case, with a
supported odd-size case tested separately. Neither fix relaxed production
admission or the exact comparison requirements.

Local evidence: `local/scaled-effects-20260928/`, including `warp-final/`,
`hardware-final/`, `sdk-final-upscale-resolve-{success,off,after-handoff}/`,
logs, frozen source/binaries, verification report and `archive-final.json`.
Private captured shader/runtime inputs remain in their existing ignored local
locations and must not be redistributed.

## Remaining live integration

The next runtime change must connect the existing frame/packet identities to
the SDK resolve, full-sized native final copy, and three replacement draws as
one consistent route. Prepare all required effect pipelines before committing
full-sized output; qualify API state at each selected draw; forward unrelated
draws unchanged. Enlarge only the matching final-copy rectangle. Preserve
native transitions and prove completion/retirement of the downstream lists,
not merely completion of the earlier input-copy list.

One bounded static mapping is prepared for that integration: IA helper RVA
0x259e470, full 512-byte extent through the next function, SHA-256
`7db0a032642249004ebda64a7e8442407851cec8f25208e8966a63990ddade49` in decrypted
image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Its packet has four vertex-owner pointers at offsets0/8/0x10/0x18, stride bytes
at0x20 and byte offsets at0x24. Owner+0x60 points to the backend; backend+0x40
is its GPU base, backend+0x10 the byte size. Native cached views are state+0x48.
The original buffer's COM-resource ownership mapping is still unqualified.
Do not infer it from unrelated texture or projection-buffer layouts. No new
hook or live offset use was added based on this static mapping.
