# Read-only draw attachment qualification — v0.3.0

Design dated 2026-09-23, following the [offline map](../research/projection-attachment-map.md)
and accepted [nonzero draw-binding run 46324](../research/projection-jitter-binding-result.md).
Implemented 2026-09-24; the initial [v0.3.0 run 5208](../research/projection-attachment-first-run.md)
was partial, followed by accepted [v0.3.1 run 11804](../research/projection-attachment-result.md).
The preparation and
deployment receipts live under `local/projection-binding-20260923/v0.3.0/`
(the archive retains the date on which this work started). Consult
`deployment.json`, `prelaunch.json` and any later `restored.json` for the exact
installed state. All 39 v0.3.0 live gates passed, but its identity failure
rejected the complete capture. The corrected v0.3.1 cycle is closed/restored;
this original protocol is retained for reproducibility.

The Release build uses `/W4 /WX`. Preparation covers 63 native fixture tests,
34 existing binding reviewer/control tests, 25 stimulus tests and 43 attachment
reviewer/control tests. Native tests verify the failure reason and capture
stage, resource reference balance, unlocked COM queries, exception propagation
and hook removal. The first fault is preserved through later cleanup checks.
Both accepted game reviews (31912/46324) must replay identically; rejected
run 42212 must remain rejected before the preparation receipt can be written.

## Question and bounded scope

Identify the resource and native pipeline configuration behind the matched
projection-consuming draw. Preserve the existing source/publication/consumption/
root-CBV/draw correlation; use a single zero-jitter source. DLSS stays Off,
TAA and the bare profile stay unchanged. No camera write, provider call,
PSO recreation, draw replay, added GPU command or readback in this diagnostic.

Use the existing two-second/100,000-admission bounds and stop at one completed
selected native draw. Retain at most eight color slots and one depth slot,
deduplicating COM resource candidates; cap actual barrier copies at 64 records
for the whole draw and reject overflow. Preserve bounded shader capture and
the original API event limit. Do not silently truncate or select another pass
after a failed qualification. Changed identities, reset, contention or fault
produce partial evidence and normal hook shutdown.

## Capture points

1. At selected native draw entry, retain packet attachment/graphics/render-state
   pointers and incoming cached PSO (`packet+38`). Record the command state,
   command list, current native attachment block and cached handle arrays.
2. Extend scoped API observation with `OMSetRenderTargets` and `ResourceBarrier`.
   Copy pointer-valued argument payloads while their native call is active;
   record order/thread/list and original-call completion. Observe
   `CreateGraphicsPipelineState` only when naturally reached in this selected
   scope; a cache hit must not trigger recreation or cache flushing.
3. At the existing `259E670` entry, attachment setup has returned. Read the
   packet attachment block, resolve backend view handles/resource candidates,
   and compare with the command-state cache. Preserve expected handles,
   post-helper cached handles, color count, optional depth, and each association
   as distinct fields. A native cache match does not fabricate an API event.
4. In the same native draw lifetime, validate each distinct resource candidate
   through guarded COM identity, owning-device comparison and `GetDesc`.
   Capture dimension, format, width/height, depth/array size, mip levels,
   sample count/quality, layout and flags. Do this outside bookkeeping locks;
   the admission token must remain active. Recheck native anchors before and
   after, and pin accepted interfaces in a bounded owning array through capture
   finalization. Explicit release occurs outside hooks/locks only after drain;
   failure paths release partial query results. A drain timeout cannot release
   interfaces still in use and rejects the capture.
5. Copy only the bounded native inputs for the PSO recipe: shader containers,
   input-layout metadata, depth/stencil state, blend state, selected rasterizer
   state, RT/DSV formats, root selection and cached/actual PSO pointers. This
   implementation does not scan the PSO cache, acquire native locks or enter
   its wait loop. Native metadata is not a unique creation descriptor.
6. At the actual draw API and native return, require the original source,
   packet, attachment anchors, command-list identity, selected PSO/root and
   resource descriptions to remain consistent. Preserve the actual viewport
   and scissor evidence. A reset or an intervening incompatible bind rejects
   the association. Finish only after admitted calls drain.

For a resource with multiple mips, arrays or planes, retain that ambiguity.
`GetDesc` describes a resource, not a DSV/RTV. View format/subresource/read-only
flags require a matching creation-descriptor record or an independently
verified native view-construction path. The map does not yet establish the
creation path. Never infer these fields from matching texture dimensions.

## ABI and lifetime requirements

Pinned host version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
Use full live code gates for every newly interpreted native span. There are
39 gates: the old 36 plus packet barriers (`259BA50`, 495 bytes), attachment
setup (`259F9B0`, 1531 bytes) and command reset (`259BCC0`, 849 bytes).
PSO key generation/cache insertion are not interpreted by this implementation.
All 39 complete spans must match the pinned decoded image offline and the
loaded game image before hooks are installed. There are ten native and
fourteen API targets; all old kind indices are preserved.

Existing Windows x64 draw and state-setup hooks retain their ABI. New COM API
observations use runtime-discovered, verified System32 D3D12 method targets:

| Method | Slot | Windows x64 arguments |
| --- | ---: | --- |
| OMSetRenderTargets | command list 46 | RCX list; EDX count; R8 handles pointer; R9D contiguous flag; stack arg 5 optional DSV pointer |
| ResourceBarrier | command list 26 | RCX list; EDX count; R8 barrier array |
| CreateGraphicsPipelineState | device 10 | RCX device; RDX descriptor; R8 IID reference; R9 output-interface pointer; HRESULT in EAX |
| RSSetScissorRects | command list 22 | RCX list; EDX count; R8 rectangle-array pointer |

Preserve full argument/return forwarding, LastError, SEH cleanup, one original
call and no bookkeeping lock across the original. Game objects are borrowed
only while their native draw owns them; a scalar snapshot does not provide
ownership. Pin validated COM interfaces, release outside hooks/locks after
drain, and retain established trampoline lifetime rules. Do not hook the
driver globally for an unlimited creation history.

Actual PSO creation evidence, when available, copies the 656-byte descriptor,
at most 32 input elements and 63 printable bytes per semantic name. It compares
all five stage bytecodes with the retained native shader blobs, checks the
requested IID and successful HRESULT, then rechecks descriptor/layout/semantic
and bytecode contents after return. The returned object must match the selected
PSO and actual bind. A missing creation event stays explicitly unknown even
when native metadata is internally consistent. This observes natural creation;
the recorder never requests one itself.

Rotating view handles, split barriers, more than 64 total barriers and more
than one PSO creation fail closed. Whole-resource descriptions are rechecked
at the draw API and native return. Cached viewport/scissor state and fresh
API calls remain separate. The reviewer may identify a depth-write candidate,
but cannot prove its view subresource, recorded commands' GPU state or pixels.

## Offline acceptance before deployment

- Owned-fixture chains: depth-only, color+depth, null depth, null color slot,
  repeated handles with a changed attachment block, and fully cached bindings.
- Reject wrong resource interface/device, pointer change, inconsistent count,
  unsupported rotating-handle index, changed depth state, reset, mismatched
  PSO, barrier overflow, partial transition and missing original-call return.
- Verify depth absence cannot inherit stale `S+140`; prove cache-only records
  never set fresh-OM or actual-creation flags. Exercise naturally created and
  reused PSOs separately, including missing creation bytecode.
- Exercise zero-jitter read-only mode: no stimulus writes or provider calls,
  no new GPU work, hooks forward once and drain on every terminal path.
- Independent reviewer rejects malformed/incomplete records and reproduces
  existing accepted/rejected archives without upgrading their evidence.
- Release `/W4 /WX`; archive exact source, DLL hash, helper/reviewer versions,
  fixture results and any new code gates before installation.

Deploy only with the game closed, back up the exact MO2 slot/sidecar again,
and hash the four established configuration files. Use a new version/archive;
do not overwrite prior evidence. Load one save with DLSS Off and stay in a
normal scene for the bounded capture. After evidence is saved, exit normally,
archive, restore the exact previous slot/sidecar, and verify configurations.
Launch with the existing MO2 DLSS test entry and bare profile, leave DLSS Off,
load one save, and wait in the scene. Arm only after the loaded-save report and
the new process's `ready` record. The recorder is idle until explicitly armed,
with a 30-minute idle limit; capture admission lasts at most two seconds.

Rollback, while the game is closed:
`local/projection-binding-20260923/v0.3.0/deploy.ps1 -Action Restore`.
The helper checks the installed/backup/sidecar hashes, restores the exact prior
DLL and sidecar state, then records all four configuration comparisons.

## Acceptance levels and subsequent GPU test

The immediate success is **draw attachment/resource description correlated**.
Report native cache association, actual OM API bind, actual resource/device
identity, native PSO recipe, actual PSO creation, view-subresource provenance,
local recorded barriers and queue completion independently. None substitutes
for another. A useful candidate must have a validated depth resource and
compatible native depth-write settings; the no-color-output shader is only a
selection clue. All GPU-content/execution/raster flags remain false here.

A later readback experiment can proceed once resource/subresource and state
are qualified. It must have a known non-split state at the insertion point,
record the copy on the matched command list, restore the original state, and
observe that exact list generation's queue submission and fence completion.
Do not add a guessed transition to make an unknown resource appear readable.

For depth/stencil `CopyTextureRegion`, copy the full selected subresource,
with zero destination offsets and no source box; depth/stencil planes are
separate subresources. Allocate through `GetCopyableFootprints`, preserve
row pitch, and cap readback bytes before recording any work. Start with
single-sample resources. These constraints follow Microsoft's
[CopyTextureRegion contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-copytextureregion).

One valid readback proves accessible GPU output, not jitter displacement.
A later comparison also needs paired, equivalent geometry/transforms and a
controlled stimulus, or an isolated replay using captured inputs. Stationary
camera matrices alone do not freeze animation or establish matching geometry.
Use the derived subpixel displacement from the map as a prediction; never
report an image difference or CPU coefficient delta as the measured shift.
