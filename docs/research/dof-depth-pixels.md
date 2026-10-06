# Gallery full-depth input capture — v1.25.40

The [v1.25.39 run](dof-stable-recipe-result.md) established all twelve
dispatch-state observations. This candidate captures the full native depth
allocation immediately before the first horizontal depth reduction in each of
the same two bounded Gallery samples. It does not admit the six-pass Gallery
pipeline for reconstructed rendering yet.

## Why this input matters

The active render view is 1280x720 inside a 2560x1440 allocation. Native depth
reduction covers the allocation, and the offline replay proved that changing
depth outside the active view can change the blur and final image. Two raw
R32F images preserve all those pixels. Their distributions can distinguish
clear values, finite depth and other contents, but cannot alone prove that all
pixels belong to the current frame. Actual descriptor contents remain unverified.

## Capture and ownership

No new native or API hook is installed. The existing Windows x64 compute
executor scope uses RCX executor, RDX backend context and R8 packet; native
Dispatch return RVA is `0x259CFCA`. Host hash remains
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
The [resource recorder's](dof-resource-capture.md) code gates, bounded recipe
rereads and native owner/device checks remain in force.

Additional admission checks require the captured first shader's SHA-256
`98c622bab8c0e113eeb4062dcc78fec129441854bcf67f61db6ed6239ca68e3c`,
groups 8x1440x1, a unique SRV t0, known state 0x40 or 0xC0, and known disabled
predication. The actual source must be Texture2D R32_FLOAT, 2560x1440, one
mip/array/sample/plane, with render-target flag 1. The GPU-description recorder
already checks the device and plane count. Unknown or differing contracts
reject the pixel capture without changing native dispatch arguments.

Two readback buffers are allocated before hook admission. Each selected
Dispatch records exactly: transition to COPY_SOURCE, complete subresource-zero
copy without a box, transition back to the observed state. Native depth is not
written. Root bindings, descriptors, viewport and native Dispatch are unchanged.
The injected copy/barriers bypass our own replacement and state observation;
the diagnostic never presents its temporary state as a native observation.

The continuous runtime includes these recording generations in its existing
validated submission/fence path, including batches with diagnostic work only.
Duplicate submission and Reset before submission are rejected. Completion and
successful native Reset are both required before mapping. Either may be
observed first. A worker saves row-padding-free files with CREATE_NEW, then
releases the source, command-list and readback references. Uncertain work stays
pinned; a device-removal fence value is never completion. There is no added
GPU wait or file write in the native Dispatch callback.

## Evidence and validation

`compute_resources` remains schema 3 and describes the existing resource/state
observer. Its `gpu_work_added=false` applies to that observer. The separate
`compute_depth_pixels` schema-1 channel identifies the two added GPU copies,
their source/list/generation/frame/packet, exact state, queue/fence, retirement,
save status and byte counts. Each image is 14,745,600 bytes, named
`inputs-PID-CREATION-gallery-depth-N.r32f` under the selected evidence root.

The new reviewer cross-checks each image receipt against the actual first
dispatch and its depth binding/state. Full v1.25.40 review requires the channel.
`review_compute_depth_pixels.py` verifies complete files, hashes them and reports
statistics for the active view and both outside regions without assuming that
outside pixels are disposable. Failed file writes remain explicit partial data.

The GPU fixture copies owned synthetic images through the production API hooks
and submission path, validates every pixel, verifies unchanged source content
and state, and exercises duplicate submission, missing identity/state,
completion-before-Reset and Reset-before-completion. WARP and NVIDIA run with
the D3D12 debug layer and GPU validation. The candidate's validation receipts
record actual results; these fixtures do not prove live Gallery pixels.

Evidence root: `local/dof-depth-pixels-20261001`. Preserve the exact installed
v1.25.39 DLL and sidecar before deploying. Next runtime: main menu, loaded save,
affected Gallery cutscene, verify both saved depth images, recovery, announced
stop and independent native-code cleanup. Keep native AA/DLSS Off, the existing
solo profile and window size unchanged. Keep the candidate installed afterward.
With the game closed, explicit rollback is `deploy.ps1 -Action Restore`; the
restored sidecar points at a sealed archive and needs a fresh output destination
before a launch. No game executable, original assets or save data are changed.
