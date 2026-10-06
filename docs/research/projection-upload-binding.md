# Projection constants: allocation, publication and D3D12 binding

Updated 2026-09-23. This offline map follows the CPU proof from
[run 17072](scoped-camera-jitter.md#run-17072-cpu-stimulus-and-recovery-accepted)
to native constant publication, D3D12 binding, draw entry points and shader
bytecode storage. It selects the next read-only diagnostic. It does not accept
a GPU result and does not install or modify a runtime probe.

The actual executable is version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
The decoded reference is
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
Both hashes are asserted before analysis. Addresses below are RVAs in that
reference, not distributable absolute addresses or verified live signatures.
The existing executable and game assets were not modified.

Private evidence remains under `local/projection-upload-20260923/`:
`verify_map.py` replays 85 selected instruction checks across 21 explicit spans
and 13 D3D12 ABI slot checks. `verified-map.json` carries each span's hash and
scope. Twenty spans cover complete functions (one ends in a verified imported
`LeaveCriticalSection` tail jump); one is an inline scene-publication block.
Unwind table entries can cover only fragments: the verified exports explicitly
join the relevant fragments. Exploratory windows, including one starting
inside an instruction at `0x23D0860`, are not verification or hook evidence.

## Confirmed static route

```text
228F3B0 -> 23D0E70 -> 23CCC50
    projection matrices -> context slot (buffer_id & 0xFFF)
23C3790 scene scope -> 23D2A80
    command kind 11 contains (buffer_id, same CPU pointer)
259E1E0
    backend recording context + 0x1860 + 8*slot = CPU pointer
259E670 -> 259F070 or 259F330
    CPU pointer + backend conversion offset -> root CBV or CBV descriptor
259D670 / 259D740 / 259D820 / 259D930 / 259DA10
    draw, indexed draw or indirect draw commands
```

This joins code paths and data layouts. The matching objects, selected mode,
command ordering and branch execution still need a contemporaneous capture.
In particular, frontend context `renderer+0x710` and the backend recording
context consumed by `0x259E1E0` are different objects. Their slot arrays also
have different offsets. They must not be equated by name or sample time.

### Allocation and transfer

The projection allocator `0x23CCC50` selects metadata through
`context+0x2B18`, uses metadata `+0x10` as a count of four-byte words and
stores the allocated pointer at `context+0x710+8*slot`. It marks the frontend
state dirty. Run 17072 observed slot 11, 268 words (1,072 bytes), with `fProj`
at word 32 and `fNoJitterProj` at word 200. These are observations for that run,
not constants to substitute for future metadata discovery.

The selectors `0x24E9620` and `0x24E9630` read the same byte at `0x51CC080`:
the first returns whether it is zero; the second returns its raw value.
The nonzero branch reaches the mapped D3D12 initialization/binding path.
Record this selector and actual loaded API at runtime; saved DX12 settings
alone do not establish branch execution.

For this branch, `0x23C3160` rounds each allocation to 256 bytes. It suballocates
from context cursors `+0x2B38/+0x2B40`; a refill requests a multiple of 16 KiB
from `0x23A2AB0`, using `renderer+0x78` as the backend. That leaf allocator
atomically advances one of three arena cursors. It does not itself verify the
arena capacity, so a diagnostic must validate bounds independently.

Backend fields established by allocation, initialization and rollover:

| Offset | Meaning in the mapped path |
|---|---|
| `+0xFF0` | Constant transfer mode |
| `+0x1000` | Per-arena byte stride used by the copy path |
| `+0x1018 + 8*i` | CPU allocation base for arena `i` |
| `+0x1030 + 4*i` | Allocated byte cursor for arena `i` |
| `+0x103C` | Current allocation arena index |
| `+0x1040 + 8*i` | Separately mapped destination base, when mode 1 uses it |
| `+0x1058` | CPU-side constant resource |
| `+0x1060 + 8*i` | CPU resource GPU virtual address plus arena offset |
| `+0x1078` | Separate destination resource, when allocated |
| `+0x1080 + 8*i` | GPU address used for each arena's constants |
| `+0x1098` | Conversion offset for the arena being consumed |

`0x23A48E0` obtains GPU addresses with `ID3D12Resource::GetGPUVirtualAddress`
and obtains CPU pointers with `Map`. It makes three equally spaced arena bases.
At rollover `0x23A2B50`, the allocation index advances modulo three and the next
cursor clears, while the conversion offset uses the **previous** arena:

```text
delta = gpu_base[consumed_arena] - cpu_base[consumed_arena]
binding_gpu_va = published_cpu_pointer + delta
```

This arena index is separate from the renderer's two-bank command counter.
Derive the consumed arena by range membership and conversion consistency;
never use the then-current allocation index or frame parity as a substitute.

The same rollover distinguishes these transfer paths:

| Mode | Mapped behavior |
|---|---|
| 0 | `CopyBufferRegion` from CPU resource to separate destination, with arena offset and allocated byte count; concurrent queue/fence coordination follows |
| 1 | CPU copy from the allocation arena into the separately mapped destination |
| 2 | Initialization assigns the CPU resource's GPU address as the binding base; no separate transfer in this rollover branch |

No live mode was captured in run 17072. A CPU pointer cannot be used as a GPU
address, and recording the calculated address is not a GPU-content readback.
The aligned extent for that run's 1,072-byte buffer would be 1,280 bytes, if
the mapped D3D12 allocation branch and matching metadata are confirmed live.

### Queued publication and binding

After `0x228F3B0` returns, camera iteration enters `0x23C3790` for the scene.
Its block at `0x23C39FB..0x23C3A36` resolves `CBViewProjection` by name and
calls `0x23D2A80`. That function appends to or creates a bounded command-kind-11
packet. Packet count is at `+0x10`; up to 16 entries contain the CPU pointer at
`+0x18+16*i` and buffer ID at `+0x20+16*i`. The native packet stores the pointer,
not a private copy of all constant bytes.

The D3D12 consumer `0x259E1E0` takes the packet in R8 and the backend recording
context in RDX. It stores each pointer at `context+0x1860+8*(id&0xFFF)`. A
separate nearby implementation `0x2599AD0` issues a different interface call;
it is not interchangeable with this D3D12 consumer.

Graphics state setup `0x259E670` uses a packed binding table and chooses:

- `0x259F070`: constants with binding kind zero read a 12-bit buffer slot from
  packed-word bits 12..23 and obtain the published pointer from the backend
  slot array. The root parameter is an argument's base plus 2 plus the packed
  word's high byte. If the cached address differs, callsite `0x259F288` issues
  `SetGraphicsRootConstantBufferView` and updates the cache. A skipped call
  requires evidence of the already bound state; absence of a fresh API event
  is not evidence of absence of binding.
- `0x259F330`: the corresponding slot and converted address form a
  `D3D12_CONSTANT_BUFFER_VIEW_DESC`. Byte size comes from registry record
  `+0x5C`, rounded to 256. Callsite `0x259F572` calls `CreateConstantBufferView`.
  The parent later binds descriptor tables. Descriptor creation alone is not
  proof of an active table or draw.

Both paths have a default-buffer fallback if the published slot is null. A
future correlation must require the exact sampled pointer, not accept that
fallback. Do not assume a fixed root parameter or equate the packed high byte
to an HLSL register without the root signature/shader mapping.

The draw functions above call `0x259E670` before issuing native D3D12 draw
commands. For example, `0x259D740` calls it at `0x259D79A`, then issues
`DrawIndexedInstanced` at `0x259D802`. This gives a bounded native draw scope
for attribution, but none of those calls has yet been tied to run 17072.

### Shader identity and transform remain live questions

`0x23AC700` builds the D3D12 graphics pipeline descriptor. From the graphics
state packet supplied as R8 to `0x259E670`, follow `packet+8` to a handle, then
dereference that handle to the shader set. Stage entries are:

| Shader set offset | Pipeline stage |
|---|---|
| `+0x50` | VS |
| `+0x58` | PS |
| `+0x60` | GS |
| `+0x68` | HS |
| `+0x70` | DS |

Each nonnull stage entry has a blob descriptor pointer at `+0x10`; the
descriptor contains byte count at `+0` (32 bits) and bytecode pointer at `+8`.
The builder copies these into the matching pipeline descriptor fields, and
`0x23ACD00` passes that descriptor to `CreateGraphicsPipelineState`.
This permits bounded shader extraction even when a PSO was created before
arming. It does not identify which shader actually draws scene geometry.

The next observation must retain shader hashes and local-only bytecode from
the matched draw. Inspect its constant accesses and clip-position computation:
it may consume `fViewProj`, `fProj`, a prior-frame matrix, or another transform.
Do not assume the jitter-tested `fProj` member is itself read by the vertex
shader. Tessellation/geometry stages and actual viewport are also relevant.

## Next diagnostic contract

The next run should be **read-only, DLSS Off/TAA, no injected jitter**. Capture
one selected loaded-scene producer and its publication, binding and draw. This
reuses the verified camera/type/provider eligibility, but does not repeat the
unchanged baseline/stimulus/recovery experiment.

Candidate native observation points, all x64 register ABI:

| Target | Inputs to retain | Intended evidence |
|---|---|---|
| `0x228F3B0` | RCX embedded entry, RDX frontend context, original bool result | Source frame/epoch, IDs, bytes, pointer, viewport, arena metadata |
| `0x23D2A80` | RCX frontend context, EDX buffer ID | Same pointer published into exact command packet |
| `0x259E1E0` | RCX executor, RDX backend context, R8 packet | Same packet/ID/pointer consumed into backend slot |
| `0x259E670` within a recognized draw scope | RCX executor, RDX backend context, R8 graphics packet, R9 draw packet | Shader set, PSO/root signature, binding mode, command list |

These are candidates, not installed hooks or tested prototypes. The three
non-projection functions are modeled as side-effect operations; their ABI,
exception forwarding and any caller-observed return registers must be checked
before replacing them. Complete target bytes must be live-gated before arming.
Every hook must call its original once and preserve original outcomes.

All foreign objects are borrowed: read only within the intercepted native call,
with bounded validated reads and before/after identity checks. Copy owned
scalars and bounded bytecode immediately; do not retain a raw pointer to read
afterward, free a game allocation, write to its constants, or use an object
after command reset/recreation. Shader file output belongs on a worker using
owned captured bytes, never in the render callback. Preserve preexisting DLL
and sidecar state if a follow-up observer is eventually deployed.

Bound the proposed observation to one selected source frame, at most 16
matching draw records and five distinct shader blobs (256 KiB per blob,
1.25 MiB total), with a two-second armed deadline and explicit drop/rejection
counts. Ineligible or oversized data must reject qualification, not silently
truncate to a successful result. Identical bytes/pointers without the ordered
producer/packet/consumer chain are insufficient because arenas are reused.
Match frame provenance through the retained packet; backend submission may
lag the frontend's current global frame.

Require actual API binding evidence (including inherited state when cached),
PSO/root-signature identity, descriptor/table provenance where applicable,
viewport and a draw using the matching constants. Native cache snapshots or
the calculated GPU address alone get a lower evidence level. GPU readback,
completion and raster displacement remain subsequent qualifications even if
the command-recording and shader correlation passes.

Implementation and owned-fixture validation of this observer remain next.
No new launch is needed for the completed offline map. The restored v0.15
bridge remains in the test slot; no follow-up recorder is installed.

API references: Microsoft's
[root CBV binding contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-setgraphicsrootconstantbufferview),
[CBV descriptor](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_constant_buffer_view_desc),
and [graphics pipeline descriptor](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_graphics_pipeline_state_desc).
The local SDK header's hash and enumerated method slots are retained in
`sdk-vtable-slots.json`; neither that file nor static slot agreement proves a
live object's interface identity.
