# First-Evaluate resource description diagnostic

Date: 2026-09-22. Bridge **v0.7.4** captured the first actual D3D12 resource
descriptions in **run 14392**. Resource descriptions, canonical device identity
and presentation creation format are accepted. Current resource states, image
contents, temporal conventions, rendering and full lifecycle remain unvalidated.
[Run 12196](depth-argument-retry.md) previously confirmed all 17 Evaluate
parameters and their correspondence to the corrected work item.

The first v0.7.4 attempt, PID 45940, reached the main menu after a black-screen
interval, but its five-minute window expired before DLSS was enabled. During
the reported black screen, read-only samples showed renderer frames
11671, 11897 and 12124 advancing over two seconds. The user reported a controller
disconnect and later confirmed recovery; no causal attribution is established.
There was no Create/Evaluate capture. The external recorder timed out and
detached with zero context changes. Its logs and review are preserved locally;
the subsequent fresh launch produced the accepted capture below.

## Accepted run 14392

With the external v0.2.0 recorder armed, the user enabled DLSS once and left
Options. Frame **17222** hit the known null-secondary depth argument fault at
host `+0x22927F2`. All ten code gates, 105 scalar reads and stable rechecks
passed. The recorder changed only RDX to that depth owner's primary backing
and retried the original instruction once. Evaluate sequence 69 then completed
all 17 getters and the resource inspection without a foreign-call fault.

Each of the four native pointers exactly matched its corrected work item's
primary backing. All four successfully queried as ID3D12Resource, returned
descriptions and shared the command list's canonical device identity. The
command list was DIRECT; its adapter LUID was `00000000:000184EE`.

| NGX parameter | DXGI format | Resource flags |
| --- | --- | --- |
| Color | R11G11B10_FLOAT (26) | ALLOW_RENDER_TARGET, ALLOW_UNORDERED_ACCESS (`0x5`) |
| MotionVectors | R32G32_FLOAT (16) | ALLOW_UNORDERED_ACCESS (`0x4`) |
| Depth | R32_FLOAT (41) | ALLOW_RENDER_TARGET (`0x1`) |
| Output | R11G11B10_FLOAT (26) | ALLOW_UNORDERED_ACCESS (`0x4`) |

All were TEXTURE2D, **2560x1440**, array/depth size 1, one mip, sample count 1,
sample quality 0, alignment 65536 and UNKNOWN layout. PreviousOutput was present
and null, so no interface inspection was attempted for it. All **26 COM calls**
completed; ten references were acquired and all ten releases completed, with
zero unconfirmed cleanup and zero added GPU commands. Format and flag names
were checked against the installed Windows SDK and Microsoft's
[DXGI format](https://learn.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
and [resource flag](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_flags)
definitions. Flags describe capabilities, not current states.

Exactly one presentation creation matched the adapter and a process-owned
window: **B8G8R8A8_UNORM (87), 2560x1440, three buffers, flags `0x842`**.
There was no observer fault, overflow or in-flight capture. This is creation-only
evidence; it does not establish the active Present target, later resize or HDR
colorspace. Neither the float intermediate textures nor NGX Hint.HDR establish
HDR presentation.

Frame **17223** encountered the same depth fault **31 ms** after the corrected
one. The exhausted one-use recorder passed it through and detached; the user
reported the resulting crash. The recorder made one context correction and
no remote memory writes. Of 64 debug exceptions, 61 other exceptions were
unclassified and passed through without payload collection. No Release or
Shutdown was observed. Evaluate still deliberately returned `BAD00012`; this
run does not demonstrate reconstruction or GPU completion.

The R32_FLOAT render-target description supports the primary-backing selection
for the kind-5 depth owner. It does not establish depth inversion, linearization,
normalization, motion units/direction, jitter or frame alignment. The next work
is validating the actual barrier/state path and those temporal conventions
before a lasting depth correction or SDK integration. **No repeat of this
resource-description capture is needed.**

Both logs and the exact post-crash settings were preserved under ignored
`local/resource-description-20260922/`. `review_capture.py` verifies the
correlation, descriptors, bounds, cleanup and hashes, producing `review-14392.json`.

| Evidence | SHA-256 |
| --- | --- |
| `retry-14392.jsonl` | `72CE746607F6880E5A6FF8CAF0E08BB7E2FF9ED631D92A58B22DF5BE89215D1F` |
| `bridge-14392.crash.log` | `18C50780539E1AA56CF5458E21B6ACCA43F5D9BC421DDFA1417890489D139091` |

After confirming the game had closed, only saved DLSS was reset from On to Off;
TAA, DX12 and fullscreen 2560x1440 remain selected. The original executable and
active bridge hashes were rechecked unchanged.

## Bounded observation and ownership

The existing first-Evaluate caller, handle, lifecycle, command-list and exact
host/NGX checks are unchanged. After a complete 17-getter snapshot, the observer
requires the four non-null resource inputs and a consistently reported
PreviousOutput, which may be null. Missing or contradictory inputs reject
before any additional COM calls. Subsequent Evaluate calls remain suppressed.

On that same callback thread, it obtains the command list's device, adapter
LUID, command-list type and canonical IUnknown identity. Only DIRECT or COMPUTE
lists qualify. For each non-null input it then:

1. Queries ID3D12Resource, acquiring a temporary reference.
2. Calls [GetDesc](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12resource-getdesc)
   and copies dimension, alignment, width, height, array/depth size, mip count,
   DXGI format, sample count/quality, texture layout and resource flags.
3. Calls [GetDevice](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12devicechild-getdevice)
   for the device's canonical IUnknown and requires identity with the command
   list's device. Matching adapter names alone do not pass this check.
4. Releases the acquired interfaces before leaving the callback.

The host owns the borrowed inputs for the admitted call. No resource pointer is
dereferenced later by the logging worker, and no acquired COM reference is
retained. Scalar descriptions and status are copied to the existing bounded
event queue. Four non-null resources require **26 COM calls**, including ten
releases; five require at most **31**, including twelve releases. No GPU
commands, copies, barriers, maps, feature creation or provider work are added.

An interface/device failure stops subsequent resources. Access violations and
in-page errors latch off foreign observation, while cleanup of already-owned
references is still attempted once. Release faults are reported without retry;
unconfirmed release completion is not reported as zero retained references.
Other exceptions are not swallowed. A foreign fault also poisons the existing
bridge boundary and global getter gate; independent cleanup remains separate.
An incomplete inspection aborts diagnostic admission and still returns the
existing intentional failure to the host.

Descriptions are immutable resource metadata; **GetDesc does not report current
resource states, image contents, depth inversion or motion/jitter conventions**.
Those remain separate validation requirements.

## Presentation metadata

After exact environment admission and resolver installation, two bounded
system-DXGI hooks observe successful `IDXGIFactory::CreateSwapChain` (slot 10)
and `IDXGIFactory2::CreateSwapChainForHwnd` (slot 15) calls. These are documented
Windows COM `STDMETHODCALLTYPE` methods using the Windows x64 ABI. A private
factory supplies the method targets; both must be in the committed image
mapping of `%SystemRoot%/System32/dxgi.dll`. The tested host hash is
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`;
tested system DXGI SHA-256 is
`B12AEBF0F077D6C1394B50E2EECDD5E16072C9A0C871F30887B5D170CBF2EAAE`.

Each detour calls its original first, preserves its result and LastError,
then copies the created chain's description and queue-device LUID. Temporary
queue/device references are released inside the callback; chain/window pointers
are borrowed only during it. At most eight creations are inspected. Faults or
capacity exhaustion stop observation. The process-pinned bridge and any enabled
trampolines stay alive until process exit, matching its existing hook lifetime.
No factory/chain pointer is retained and no Present or ResizeBuffers hook is added.

At Evaluate, presentation format is available only with exactly one fully
captured chain for that adapter and a window owned by this process. Multiple
candidates, overflow, in-flight capture, missing events or faults report unknown.
The event explicitly says **creation-only**: it does not establish the active
chain at Evaluate, later resizes, colorspace or HDR presentation. It supplies
format evidence without guessing from NGX's Hint.HDR or an intermediate texture.

## State conversion traced statically

After rechecking captured-host SHA-256
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`,
bounded IDA export of host `+0x259BA50` establishes how preparation records turn
into 32-byte D3D12 barriers. It reads a linked record's engine before/after words
at +24/+26 and the selected backing's native pointer at +32. Different states
produce a transition with flags zero and ALL_SUBRESOURCES. Equal states produce
a UAV barrier; the preceding preparation helper decides whether a record is
needed. The consumer `+0x259DAE0` flushes these records before `+0x2383CF0`.

| Preparation request | Engine code | Converted D3D12 state |
| --- | --- | --- |
| Color / motion / depth input | `0x4` | `0x40`, NON_PIXEL_SHADER_RESOURCE |
| Output | `0x400` | `0x8`, UNORDERED_ACCESS |

The low-state conversion is a bit mapping, not a direct cast. Special mappings
include `0x100` to RENDER_TARGET (`0x4`) and `0x200` to DEPTH_WRITE (`0x10`).
This supports retaining the original preparation/barrier path. It does **not**
prove any particular runtime barrier executed, its before-state was correct,
cross-queue synchronization completed, or GPU work finished. See Microsoft's
[resource-state guidance](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12).
The new runtime log therefore continues to mark resource state unobserved.

## Verification and deployment

Release builds with `/W4 /WX`. All **16 CTests pass**. The new test uses a real
WARP device, command list and four committed textures to verify descriptions,
same-device identity, optional null handling and the 26/31-call bounds. Fabricated
COM resources check interface rejection, GetDesc faults, canonical device
mismatch, release faults, reference balance and stopping before the next resource.
Hidden WARP swap chains exercise the actual system-DXGI hooks, real creation
format capture, adapter mismatch and ambiguous-target rejection. No GPU commands
are submitted by that test. Test hooks are removed while quiescent.

Binary audit: 393,216 bytes; 106 CFG targets, with all 15 provider wrappers and
callbacks distinct and in the CFG table. v0.7.4 SHA-256:
`FA743BE800A67E646FB4145C23EB8E90D53169D86394D5C13F514F45E54006C0`.

The prior v0.7.3 DLL was preserved at the same MO2 destination under
`mhw_dlss_bridge_probe.v0.7.3.dll.disabled`, hash
`1F2309021E4DF15D3645C0B7C4A264F0B40D02394438DE1B3F6C65EEE1D29552`.
At staging, settings were backed up and not changed: DLSS Off, TAA, DX12,
fullscreen 2560x1440. After run 14392, the saved On value was separately backed
up and reset to Off. MO2 PID 7112 retains the existing capture-one-evaluate mode.
The external v0.2.0 recorder and its **one-use** retry are unchanged.

Ignored artifacts: `local/resource-description-20260922/`, including CTest,
binary audit, staging manifest and bounded barrier decompilation/disassembly.
For removal, close the game and disable the bridge Force Load entry. For
rollback, preserve v0.7.4 under a disabled name, verify the v0.7.3 backup hash,
then restore it to the exact active DLL name. No original executable or asset
requires restoration.

The accepted attended run started Off at the main menu, waited for the external
tool's ARMED event, enabled DLSS once and left Options. Its next uncorrected
frame crashed as expected. Descriptor completeness, device identity, reference
cleanup, presentation selection and both capture logs were reviewed successfully.
