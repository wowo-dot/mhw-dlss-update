# Attachment and pipeline provenance after run 46324

Offline mapping completed 2026-09-23. This identifies the native paths to
observe next; it does **not** recover an attachment from the exited process or
extend [run 46324](projection-jitter-binding-result.md) into GPU evidence.

The next diagnostic is a read-only attachment/resource qualification on the
matched draw, with DLSS Off. Its [implementation plan and acceptance rules](../runtime/projection-attachment-plan.md)
are defined. This step subsequently passed as
[v0.3.1 run 11804](projection-attachment-result.md). The newer
[depth-view/state map](depth-view-state-map.md) addresses its remaining
descriptor-lifetime and earlier-barrier gaps. The preparation below is historical.

## Identity and verification

Host version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
Decoded image SHA-256
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
Both hashes are checked before instruction exploration. Addresses below are
RVAs for this host; they are not portable signatures.

Private reproducible evidence is in `local/projection-attachments-20260923/`:

- `verify_map.py` / `verified-map.json`: nine complete function extents,
  including chained unwind fragments; 93 selected instruction checks and nine
  SDK method-slot checks. The cache insertion function's final branch rejoins
  its common unlock/return block rather than ending in a second return.
- `sdk_layout.cpp`: x64 Windows SDK 10.0.26100.0 structure/enum assertions,
  compiled through CMake with C++20 and `/W4 /WX`. No GPU calls.
- `replay_descriptor.py` / `descriptor-replay.json`: a bounded Python
  assignment interpreter, using synthetic memory and the pinned instructions.
  All 64 shader-descriptor cases and 256 recipe cases pass. Cases cover all
  five-stage presence combinations, optional input layout, zero through seven
  render targets, a null color entry, rasterizer variants, absent depth and
  typeless-to-depth format conversions. Unknown instructions, unmapped reads
  and instruction-budget exhaustion reject replay. Native game code is never
  executed by these checks.

These checks validate the static interpretation, not live resource identity,
actual descriptor contents, PSO creation history or GPU execution. Full code
hashes, disassembly and replay outputs remain ignored local artifacts.

## Draw packet and output bindings

The accepted indexed-instanced native wrapper is `259D930`. Windows x64
arguments are RCX executor, RDX backend context, R8 draw packet. It calls:

1. `259BA50(executor, packet)` to accumulate packet barriers.
2. `259E470` for input assembly setup.
3. `259F9B0(executor, context, packet+10 pointer, packet+28 pointer)` for
   attachments, viewport and scissor.
4. `259E670(executor, context, packet+20 pointer, packet)` for PSO/root/shader
   bindings.
5. Any accumulated `ResourceBarrier` call, then `DrawIndexedInstanced`.

Here and below `+10`, `+28`, etc. are hexadecimal byte offsets, and a
"pointer" field means its stored pointer, not the address of the field.

Let A be the attachment block at `packet+10`, C the backend context, and S
the command state at `executor+8`.

| Native field | Meaning established by the pinned call path |
| --- | --- |
| `A+48 & 7` | Number of color entries on this path, 0–7 |
| `A+8*i` | Color view owner pointer; null entries are possible |
| `A+40` | Optional depth view owner pointer |
| `view+60` | Backend **view** object, distinct from a texture backing |
| color backend view `+10` | Ordinary CPU RTV handle |
| color backend view `+28` byte / `+2C` index / `+30+8*index` | Alternative rotating RTV handle branch |
| depth backend view `+10` | CPU DSV handle |
| backend view `+18` | Native resource candidate; color path also passes it to `DiscardResource` |
| `S+98..D7` / `S+D8` | Eight cached RTV handles / cached DSV handle |
| `S+140` | Depth backend view `+18`, copied only on the depth-present update path |
| `S+20` | D3D12 graphics command list |

Do not reuse the earlier texture-backing `+20` resource layout for a backend
**view**. The view path above uses `+18`. A runtime COM/interface/device check
is still required before treating the depth candidate as an ID3D12Resource.
When depth disappears, the observed update block does not clear `S+140`;
that field alone is therefore not reliable evidence of a current depth target.

There are two reasons no fresh output bind may appear inside a selected draw:

- If `A == C+10` and the scissor-enable bit agrees with `S+100`, the helper
  skips its attachment/viewport/scissor update section.
- Otherwise it rebuilds eight handles and compares them with `S+98`, plus
  depth with `S+D8`. Equal handles skip `OMSetRenderTargets` while allowing
  viewport/scissor updates. That comparison does not itself store/check a
  separate cached render-target count; preserve the packet's count explicitly.

The fresh bind at `259FBE5` is command-list vtable slot 46. Its arguments
are count `A+48 & 7`, an array of CPU RTV handles, `FALSE`, and an optional
pointer to the DSV handle. These are descriptor handles, not texture pointers.
The array interpretation matches Microsoft's
[OMSetRenderTargets contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-omsetrendertargets).

Viewport bounds start in `A+4C,+50,+54,+58`, with further clamping on the
color path. Viewport and scissor updates have their own caches at
`S+E0..EC` and `S+F0..FC`; an actual viewport event does not imply a fresh
attachment bind. `259BCC0` clears the command-state caches, including RTV/DSV,
PSO, root signature and depth resource candidate. A reset invalidates any
observer's cached association.

## Native PSO provenance

`259E670` first reads `packet+38`. A non-null value bypasses the native PSO
builder. Otherwise it calls `23ACD00(backend, packet, wait=true)`. The result
is compared with `S+108`; `SetPipelineState`, slot 25, is skipped if equal.

The native builder forms an 80-byte recipe and hashes it at `23A9500`.
The hardware branch applies CRC32 instructions to ten qwords; a software
branch also exists. The cache is reached through `backend+1360`: the group
index is `graphics+18`, each group occupies 640 bytes, and each of its 40
entries is a 16-byte PSO-pointer/key pair. A key hit can reuse a PSO or wait
for another thread to publish it. Cache insertion at `26246F0` is synchronized.
A diagnostic must not invoke the builder or its wait path just to inspect it.

Let G be `packet+20` and R be `packet+28`.

| Recipe offset | Source |
| --- | --- |
| `00` | `G+08`, shader-set handle |
| `08` | `G+20`, input-layout object |
| `10` | Topology type translated from bits in `G+00` |
| `14` | Rasterizer key selected through `R+18` and bit zero of `R+04` |
| `18` | `R+10`, depth/stencil-state owner; replaced by a default when DSV format is zero |
| `20` | `R+08`, blend-state owner |
| `28` | `A+48 & 7`, color count |
| `2C..48` | Eight color formats, active non-null entries from backend view `+20` |
| `4C` | Depth format derived from depth view owner `+74`, or zero |

Observed depth conversions are R16_TYPELESS→D16_UNORM (53→55),
R24G8_TYPELESS→D24_UNORM_S8_UINT (44→45), and R32_TYPELESS→D32_FLOAT
(39→40). These are recipe conversions, not proof of the actual DSV's mip,
array slice, plane or read-only flags.

On a cache miss, `23AC700` fills a zero-initialized 656-byte
`D3D12_GRAPHICS_PIPELINE_STATE_DESC`. The shader-set entries `+50/+58/+60/+68/+70`
feed VS/PS/GS/HS/DS bytecode descriptors. Blend state copies 328 bytes from
`*(blend owner+98)+10`; depth/stencil state copies 52 bytes from
`*(depth-state owner+60)+10`. The latter includes DepthEnable, DepthWriteMask,
DepthFunc, StencilEnable, masks and both stencil faces.

The builder then assigns the root signature and 44-byte rasterizer state.
A special blend adjustment is applied when IndependentBlendEnable is one,
at least two render targets exist, and RTV slot one is R32_UINT. It clears
IndependentBlendEnable and the first render target's write mask. Reconstructing
only the helper's output would miss this adjustment.

The creation API is device slot 10 at `23ACF92`. A successful descriptor
capture at that API, correlated with its returned object and later PSO bind,
would provide stronger evidence than native shader metadata. A cached object
is not retroactively certified by reconstructing a plausible descriptor.

One scope detail: PSO construction checks DS presence (`shader set+70`) for
the tessellation root variant, while command binding checks HS (`+68`). Both
are absent in run 46324. Do not generalize the verified VS/PS-only contract
to mixed tessellation-stage cases.

## Barriers are a separate path

`259BA50` walks the linked list at `packet+08`. A node's backing is at `+08`,
its native resource at backing `+20`, and its before/after engine states are
16-bit fields at node `+18/+1A`. These are **engine** encodings; for example,
engine `0x8` maps to D3D12 `0x80`. They cannot be logged as D3D12 states unchanged.

The helper appends 32-byte D3D12 barrier records to `S+160..168`, with a
capacity endpoint at `S+170`. Equal engine states follow a UAV-barrier branch;
ordinary transitions carry all-subresource index `FFFFFFFF`. Other barriers
can already be pending, and attachment handling can emit aliasing/discard work.
The draw wrapper flushes the pending records before its indexed API call.

Therefore the next observer should copy the actual bounded ResourceBarrier
API arguments in this draw's command-list scope and record its return. It
must distinguish transition, UAV and aliasing barriers. Absence of a matching
transition in this small window leaves the earlier state history unknown.
Neither an engine state nor a recorded barrier proves completed GPU state.

## What a later raster measurement should expect

The established matrix relation is `delta_clip_xy = clip_w * jitter / dimensions`.
Combining that with perspective division and the Direct3D viewport transform
predicts a screen displacement of `(jitter_x/2, -jitter_y/2)` when the observed
viewport dimensions match. This is a derivation, not an observation. Microsoft's
[viewport equations, section 15.6](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm)
specify the half-width/height scaling and flipped screen Y.

For run 46324 the ideal pre-rasterization displacement is approximately
`(+0.1328125, -0.2006173)` pixels, conditional on the candidate shader/PSO being
the executing path and ordinary viewport selection. Raster quantization,
coverage and depth comparison affect the resulting texels. The earlier
`1.4775390625` matrix-coefficient change is not a 1.48-pixel shift. Do not
double the native sample merely to make the effect visible.

## Decision

Implement the read-only attachment qualification first, using the already
working source-to-draw correlation. Capture the actual resource descriptions,
native depth/stencil/rasterizer metadata, attachment-cache evidence, any fresh
API binds and local barriers together. Keep actual PSO creation, view descriptor
provenance and GPU state/completion as separately reported evidence levels.

This will decide whether the matched draw is a useful depth-writing pass,
which subresource can be read, or whether selection must move to a different
pass. The no-color-output PS alone cannot answer those questions. Keep the
previous DLL restored and the game closed until that new recorder is built,
tested and deployed; no unchanged v0.2.1 rerun is needed.
