# Ordinary TAA motion readback v1.4.0

Runtime result: [run 48724](../research/ordinary-taa-motion-result.md) passed
all three readbacks and is closed. The exact prior DLL is restored and settings
are unchanged. Motion semantics still require a controlled camera-pan test.

Prepared 2026-09-25. This extends the
accepted [submission/completion run 14268](../research/ordinary-taa-submission-result.md)
with three bounded copies of the actual `tGBuffer4` texture immediately before
its matched native TAA Dispatch. The purpose is to examine exact packed pixels
using the [shader-derived decode](../research/ordinary-taa-shader-result.md).
This does not enable DLAA or validate the motion producer's coverage.

## Commands, gates and ownership

The seven hooks and Win64 calling conventions are unchanged from the
[submission observer](temporal-submission-observer.md) and
[native consumer observer](temporal-consumer-observer.md). No additional game
offsets or hook targets are introduced. Host version `0.0.0.0`, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
the same 48 executable code gates apply. System `CopyTextureRegion` slot 16
is validated before use. Each admitted original Dispatch still forwards once.

Before producer admission, allocate three READBACK buffers using validated
`GetCopyableFootprints` results. Each source must be the producer-pinned
canonical resource identity, owned by the discovered device, full-resolution
`R32_UINT`, one mip/slice/sample, quality zero, unknown texture layout, and
exactly `ALLOW_RENDER_TARGET`. Extents are at most 8192 on each axis; both
tightly packed pixel bytes and padded allocation bytes are at most 16 MiB
per sample. At 2560×1440 each image is 14,745,600 bytes.

The matched native consumer must establish a conflict-free, full-resource
`0xC0` shader-read state before Dispatch. Inside the submission marker:

1. Transition `0xC0 -> COPY_SOURCE (0x800)` through the original barrier trampoline.
2. Copy the entire subresource zero into that receipt's separate readback buffer.
3. Restore `COPY_SOURCE -> 0xC0` before forwarding the original Dispatch.

These add at most nine command-list operations (six barriers, three copies).
The existing maximum of three private queue Signals is separate. The observer
does not submit command lists, change descriptor bindings, alter shader data,
write jitter, replace output, or call a vendor SDK. Private transitions bypass
the native-barrier observer; their own outcomes are logged separately.

The submission journal includes copy begin/end and original-Dispatch-forward
orders. Readback requires the exact source/packet/frame/list/allocator
association, successful Close and unique Execute, completed private fence,
and later successful Reset retiring the recording. Admission closes before
hook removal to avoid accumulating irrelevant events while removal pauses
threads. Admitted callbacks still drain; all bounds remain fail-closed.

Source, list, allocator, device and readback allocations are **retained until
process exit once the first transition is attempted**, even after successful
completion and saving. This protects references if an uncertain recording is
later submitted or resubmitted. Unissued allocations release on rejection.
If CopyTextureRegion faults after the first transition returned, restoration
is attempted by an SEH finally block. Any uncertain operation quarantines the
sample and prevents mapping; a potentially partial GPU command cannot be undone.

Mapping and disk writes occur on the worker only after hooks are disabled,
callbacks drained, and all three completion/retirement proofs accepted.
Files use exclusive creation and strip row padding without converting pixels:
`inputs-<pid>-<creation>.motion-{0,1,2}.u32`, little-endian row-major words.
Partial files survive a failed write for diagnosis; aggregate acceptance is false.

## Independent review and tests

`review_temporal_inputs.py` accepts version 1.4 and calls `temporal_motion.py`
after shader, consumer and lifecycle review. It checks copy-order uniqueness,
source identity/description, native state, restoration, footprint arithmetic,
exact length and SHA-256 of all three artifacts. `motion_readback_verified`
and `motion_pixels_saved` can become true. Broad input eligibility, motion
semantics, typed views and DLAA readiness remain false. The legacy
`readback_eligible` flag remains false; use the narrowly scoped motion-readback
flag to identify this result.

Pure decoding follows the captured shader: `dx=half(p>>16)`,
`dy=half(p&0xfffe)`, history UV delta `(-dx/2,+dy/2)`. The low bit's meaning
and the actual constant-buffer mode remain unknown. Decoding does not prove
camera/object coverage or jitter inclusion.

Release `/W4 /WX` build and all 49 native tests pass. Six new WARP cases cover
exact pixel copies, actual lifecycle detours, incorrect state/format, missing
retirement and Map failure. The 17×13 fixture checks all 663 words across three
different images, with 256-byte row pitch reduced to 68-byte rows. Extent,
padded allocation, mip/sample/flag and lifetime guards are exercised. The first
test pass exposed a fixture cleanup exception (deleting an open input file);
closing the file before deletion fixed it. This was not a GPU-copy failure.
Twenty-six Python tests cover the fixture and corrupted artifacts/orders,
along with known half-float values, sign, low-bit masking and nonfinite values.
Existing contract tests retain 620 unreadable-byte and 319 shader-byte cases.

## Local run and rollback

Fresh preparation/deployment evidence lives in
`local/temporal-motion-20260925/v1.4.0/`; the deployment receipt determines
the exact installed DLL and archived prior slot. Keep old closed captures sealed.
Deploy only while the game is closed. The local `deploy.ps1 -Action Restore`
verifies identities, restores the prior DLL and restores/removes the output
sidecar according to its original state. No game assets or configuration edits
are part of this experiment.

Use the existing barebones MO2 profile and test entry, DLSS Off/TAA On,
2560×1440 DX12, offline/private. Load a save and keep the camera still. Arm once
after readiness with `tools/arm_temporal_inputs.py`, then preserve and review
the log, shader and all pixel files. Exit normally before exact rollback.
After the stationary capture is accepted, inspect decoded contents and decide
which controlled camera/object movement will discriminate the remaining
motion hypotheses. Do not infer those results from this synthetic fixture.
