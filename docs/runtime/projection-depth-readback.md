# One-shot depth readback v0.8.0

This diagnostic extends the [accepted v0.7.1 copy observation](../research/depth-copy-submission-result.md).
It adds at most one full-depth `CopyTextureRegion` and one private DIRECT-queue
Signal. It does not inject jitter, transition game resources, write into game
textures, add queue waits, integrate a provider, or change settings.

## Admission and frame scope

The executable remains version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
There are still 39 hooks and 44 distinct byte-gated native spans; their calling
conventions are unchanged from the [copy recorder](projection-copy-recorder.md).
The new action runs after a returned native `CopyResource` at RVA `0x259E100`.
Its additional API calls are excluded from the native observer using a scoped
thread-local guard and are logged separately.

The worker prepares an owned READBACK buffer and an unshared fence before
arming. Dimensions must match the discovered swap-chain extent, each at most
8192, with a 64 MiB footprint bound. Actual source descriptions must match:
R32_TYPELESS Texture2D, one mip/slice/sample, no multisample quality,
ALLOW_DEPTH_STENCIL, and the same device. The destination of the native copy
must be a matching R32_FLOAT render-target texture.

Admission also requires the selected draw's resource/canonical identity, a
still-matching writable D32_FLOAT view and recorded creation/lifetime, an actual
successful Reset, no overlapping calls on that list, and the immediately
preceding returned barrier call declaring both:

- Source: all subresources, `DEPTH_WRITE (0x10) → COPY_SOURCE (0x800)`.
- Native destination: all subresources, shader read `0xC0 → COPY_DEST (0x400)`.

Split or aliasing ambiguity, a bundle/Close in the current generation, changed
objects, incomplete relevant history or wrong formats/state prevent insertion.
Completed history is immutable; reservation headers are published separately
from returned payloads. Renderer threads never wait for another recorder entry.

The candidate must occur after the selected draw marker. This can be a later
frame because the game records lists in parallel. The capture identifies the
resource and command recording, **not the exact producing scene frame**. That
remaining alignment work must precede a jitter-displacement claim.

## Copy, completion and lifetime

The added copy reads original depth subresource zero into a placed footprint
in the owned buffer, with zero destination XYZ and a null source box. It adds
no source barrier. These full-subresource arguments follow the SDK's
[depth-copy requirements](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-copytextureregion).

After the native Execute returns, the recorder qualifies the exact Reset/Close
generation and DIRECT queue, then signals its private fence to one. The worker
polls without blocking a render thread. It also requires a later successful
Reset of the copied list, with no intervening resubmission of the captured
recording. This ensures that the list no longer contains a command that could
rewrite the buffer while the CPU reads it. Fence completion alone is insufficient
for that resubmission case.

Only after both completion and recording retirement may the worker Map the
buffer. [Map does not wait for GPU completion](https://learn.microsoft.com/en-us/windows/win32/direct3d12/readback-data-using-heaps).
The existing two-second capture bound and fixed observer capacities still apply.
Any post-insertion uncertainty leaves the buffer unmapped and all private owning
references pinned until process exit. Success also retains those bounded
references until exit. Source, list, allocator, queue, device, buffer and fence
are explicitly owned; this prototype is one-shot and the DLL is pinned.

The existing observer's own references still release after hook drain. Private
readback references have a separate, explicit lifetime and must not be reported
as released by the observer's zero-pin counters. Test-only teardown can release
them after the fixture's last GPU submission and list Reset.

## Evidence and verification

Header version `0.8.0` uses experiment `projection-depth-readback`, copy budget
one and Signal budget one; it cannot pass the older zero-GPU-command contract.
The native companion remains schema 4. A `depth_readback` record adds the private
footprint, object identities, native copy/barrier/generation selectors, signal
interval, completion value, later Reset, retention state and file SHA-256.

`binding-<pid>-<creation>-depth.f32` contains tightly packed little-endian
32-bit floats, top row first in resource coordinates, with row padding removed.
Width/height are in the record. The reviewer checks file size/hash, native
resource/view identity, submission and retirement chronology, private fence
policy, footprint arithmetic and independently recomputed pixel statistics.
It rejects nonfinite or out-of-range depth. Uniform valid data is reported
without claiming scene coverage. The new version also requires all three core
resource-query phases; prior versions keep their archived review contracts.

Verified preparation: Release `/W4 /WX`, 144 native checks and 433 Python checks.
Fourteen native readback checks cover padded rows, exact GPU clear values,
identity/format rejection, one-shot behavior, incomplete GPU work, signal/Map
failure, cancellation, wrong state/view, discarded recording and resubmission.
Thirty-four owned journals are retained, including four readback journals.
Full-review composition tests explicitly combine independent owned traces;
they are not a native end-to-end game capture.

Local receipts and exact DLL/source identities:
`local/projection-binding-20260923/v0.8.0/`. Preparation, deployment and prelaunch
receipts must agree before launching. They record preparation before any game
result. The subsequent [run 51816](../research/depth-readback-result.md) passed
the unchanged review, saved actual depth pixels and completed normal rollback.

## Runtime procedure and rollback

Use the existing MO2 bare-profile diagnostic entry with DX12 On, DLSS Off and
TAA On. Load a save and leave the camera still; reply from the phone. Arm only
the matching ready PID/creation/log. Save the native journal, readback record and
pixel file; review all three together. The user then exits normally with DLSS Off.

`deploy.ps1 -Action Restore` in the versioned local directory restores the exact
prior slot DLL and restores/removes its output sidecar, checking configuration
hashes. Never replace the slot while the game is running. Game assets and the
original executable remain untouched. Full scene validation, jitter, DLAA and
resolution-changing DLSS are later steps.
