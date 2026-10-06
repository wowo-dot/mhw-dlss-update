# Depth-view lifetime and earlier state declarations

Offline follow-up to [accepted run 11804](projection-attachment-result.md),
2026-09-24. The depth-view creation/destruction path and an earlier native
clear/barrier path are now mapped and checked. This does not recover the
exited process's DSV descriptor or establish its GPU resource state.

Host version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
Decoded image SHA-256
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
Both are checked before analysis. All addresses and offsets below are
hexadecimal RVAs/byte offsets for this host, not portable signatures.

## Depth-view construction

The ordinary owner constructor `239B1C0` uses Windows x64 arguments:
RCX new owner, RDX texture owner, R8D flattened subresource index, R9B read-only
request. It stores the texture owner at `+58`, flattened index at `+6C` and
raw resource format at `+74`. Its backend view is published at owner `+60`.

It supplies a temporary native recipe to factory `2622FE0`:

| Recipe offset | Source |
| --- | --- |
| `00` | `(texture owner + 8C dword >> 27) & 15`, native dimension code |
| `04` | Flattened subresource index |
| `08` | Mip count from texture owner `+8D` byte |
| `0C` | Native format code from texture owner `+90` dword |
| `10` | Read-only request byte |

The format code at texture `+90` is not the DXGI format at depth owner `+74`.
The factory selects DX12 when the backend's device at `+18` is non-null,
allocates a `28`-byte object and tail-calls `24E6400`. Its alternative branch
is DX11 and must remain outside the DX12 observer.

Backend constructor `24E6400` takes RCX new backend view, RDX texture backing,
R8 recipe. It forms a zeroed 24-byte `D3D12_DEPTH_STENCIL_VIEW_DESC` on its
stack, obtains a CPU handle, and calls device slot 21 at `24E6510`:
RCX device, RDX backing `+20` resource, R8 descriptor pointer, R9 handle.
It stores the handle at backend view `+10`, the resource at `+18` and the
read-only request byte at `+20`. The full DSV descriptor is not retained in
this backend object. The API returns void; a returned call is not an HRESULT
success report. See Microsoft's [creation contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createdepthstencilview).

The instruction-level descriptor assignments are:

| Native input | Descriptor assignment |
| --- | --- |
| Format code 41 decimal | `D16_UNORM` (55) |
| Format code 43 decimal | `D24_UNORM_S8_UINT` (45) |
| Other format codes on this branch | `D32_FLOAT` (40) |
| Dimension code 1 / 2 | `TEXTURE1D` / `TEXTURE2D`; mip = input index |
| Dimension code 4 | `TEXTURE1DARRAY`; mip = index % mips, first slice = index / mips |
| Dimension codes 5, 6, 7 | `TEXTURE2DARRAY`; same division, array size = 1 |
| Read-only byte zero | Flags zero |
| Read-only byte nonzero, format code 43 | Read-only depth and stencil |
| Read-only byte nonzero, other formats | Read-only depth |

Unsupported dimension codes leave the view dimension zero. The dimension-4
branch also leaves array size zero. Replay preserves those assignments; it
does not certify every native input combination as a valid D3D12 descriptor.
The first runtime qualification should accept only the actually captured,
supported 2D descriptor matching the validated resource. Format, dimension,
flags and union interpretation follow the SDK's [DSV structure](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_depth_stencil_view_desc).

Run 11804 did not retain the new recipe fields or backend `+20` byte. Its
single-mip/single-slice resource cannot retrospectively establish read-only
flags or prove which creation call supplied its bound handle.

## Handle lifetime

`23A2880` allocates a handle from a synchronized free-index stack. Given native
backend B and its pool P at `B+1368`, it pops an index from the array ending
at `P+460B8`, then returns `B+1A8 + index * B+1DC` (base plus descriptor stride).

The backend-view destructor is `24E8E40`, pointed to by its vtable at `3539AF8`.
Before freeing the object, it passes view `+10` to `23AB3C0`. That helper
converts the handle back into an index using the same base/stride and pushes
it onto the synchronized free stack. Thus equal handle values can belong to
different view lifetimes. Raw handle equality across time is insufficient.

A new observer must correlate construction, the actual descriptor call,
publication, destruction/reuse and the handle used at output binding.
`CopyDescriptors` and `CopyDescriptorsSimple` can overwrite a tracked DSV
without this native constructor; observe or conservatively invalidate affected
destinations. Descriptor-copy type determines its stride; never inspect an
opaque descriptor as a guessed driver memory structure. See the
[descriptor-copy contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-copydescriptors).

## Barriers can precede the selected draw

Native clear handler `259C240` takes RCX executor, RDX context, R8 clear packet.
It first calls `259BA50` to accumulate the packet's barriers. Its attachment
block comes through `*(packet+30)+10`. The depth path selects attachment `+40`,
owner `+60`, then backend view `+10` as the DSV handle.

When packet `+10` has depth/stencil clear bits `100`/`200`, the path flushes
pending `D3D12_RESOURCE_BARRIER` records at `259C43B` (list slot 26), resets
the pending-array end, then calls `ClearDepthStencilView` at `259C47B` (slot 47).
The clear uses packet `+14` float depth, `+18` stencil byte, the corresponding
SDK clear flags, and zero rectangles/null rectangle pointer.

This provides a concrete possible location for earlier depth transitions.
It does not prove that this clear or a particular transition occurred before
run 11804's draw. The next observer must retain actual earlier API calls,
not treat this static route as the missing event.

The native barrier helper translates engine states before writing the SDK
record. Selected mappings, written here in hexadecimal:

| Engine state | D3D12 state |
| --- | --- |
| `4` | `40`, non-pixel shader resource |
| `8` | `80`, pixel shader resource |
| `C` | `C0`, both shader reads |
| `40` | `20`, depth read |
| `100` | `4`, render target |
| `200` | `10`, depth write |
| `400` | `8`, unordered access |
| `800` | `400`, copy destination |

Equal native before/after states use a UAV barrier; the ordinary transition
path writes all-subresources and no split flags. Other command paths may
produce different barriers. The reviewer should use actual API arguments and
reject unknown/unsupported cases instead of extending these mappings blindly.

## Native cache setup is not D3D12 Reset

Previously named `command_reset`, `259BCC0` clears engine command caches and
chooses a command list. It uses leaf `23A8710` to read `backend+118+8*index`,
or takes the fallback list from `backend+228`. It stores the result at command
state `+20`, queries interfaces and binds descriptor heaps. It does not call
`ID3D12GraphicsCommandList::Reset`. Native teardown `259C050` releases cached
interfaces; it does not call `Close`.

A recorder must track actual successful Reset calls (slot 10) and Close calls
(slot 9), preserving each list's recording generation. Native cache setup or
a frame counter is not a substitute. A reset can occur while prior GPU work
is executing, so its return is not queue-completion evidence. See
[Reset semantics](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-reset).

An earlier barrier can qualify a recorded declaration only within a complete,
unambiguous history. Missing history, unresolved split transitions, relevant
aliasing, bundles, overwritten descriptors or list reuse must remain explicit
unknowns. CPU ordering across different lists does not establish GPU ordering.
Microsoft's [resource-state rules](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12)
also distinguish explicit transitions, promotion/decay and split barriers.

## Verification and decision

Private artifacts under `local/depth-view-state-20260924/` contain:

- `verify_map.py` / `verified-map.json`: ten complete functions, including
  chained unwind fragments, two small return-terminated leaves, 90 instruction
  checks, ten SDK method-slot checks and the relevant vtable entries.
- `sdk_layout.cpp`: compiled x64 C++20 `/W4 /WX` structure/enum assertions for
  DSVs, barriers, clear flags and relevant states. No GPU calls.
- `replay.py` / `replay.json`: 1,080 descriptor and 781 barrier assignment cases
  against the pinned instructions in a bounded synthetic interpreter. The
  descriptor is compared byte for byte at the emulated API boundary and its
  stored handle/resource checked after volatile-register clobbers. Four
  negative checks reject division by zero, unmapped input, unexpected calls
  and instruction-budget exhaustion. No native game code is executed.
- `archive.json`: sources, evidence hashes, unchanged DLL and four unchanged
  configuration hashes. No runtime deployment occurred.

The next implementation is the [view-lifetime and earlier-barrier recorder](../runtime/depth-view-state-plan.md).
Its descriptor observation must begin before the selected view is created;
extending only the existing two-second draw window cannot recover an old
descriptor. Its short frame capture will retain actual Reset/barrier/clear
history through the matched projection draw. GPU copies remain a later step.
