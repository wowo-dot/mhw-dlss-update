# Request-stamped jitter depth triplet v0.8.4

Prepared after [v0.8.2 run 42232](../research/depth-alignment-result.md), which
established the zero-jitter source/request association and correct DIRECT queue
order. This diagnostic joins the existing one-shot CPU jitter protocol with
three independent full-depth readback slots. No provider integration is added.

The first v0.8.3 run exposed a delayed draw-return marker and saved no pixels.
[The recorded failure and v0.8.4 correction](../research/depth-triplet-timing-result.md)
separate actual draw return from completion of the later CPU recovery sample.

## Action and scope

The accepted [scoped stimulus](projection-binding-stimulus.md) still observes
three eligible CPU projection calls: baseline, one injected sample, and recovery.
Only the middle sample is followed through publication, backend consumption,
actual root-CBV binding and one native draw. Camera-field restoration runs in
the existing `__finally` path before any sample becomes eligible for a copy.
The injection count remains one, and recovery-entry deferrals remain bounded
at 64. The capture window stays two seconds.

Each successfully restored CPU call now publishes its own immutable selection
record. A copy must match that source's original request frame, epoch, bank,
async mode, frontend context and thread. Missing baseline/recovery requests
cannot be replaced with later frames. Publication does not depend on copy-worker
global frame values.

Three separate READBACK buffers and private fences are allocated before arming.
Each slot admits at most one full-depth copy and one private Signal. All copies
retain the existing native source/canonical identity, device, extent, format,
successful Reset/Close generation and immediate pure COPY_SOURCE/COPY_DEST
barrier qualification. No game resource transitions, waits or color/depth writes
are added. The maximum private allocation is 3×64 MiB; at 2560×1440 the three
pixel payloads total 44,236,800 bytes. Issued private resources remain owned
until process exit, including failure paths.

The stimulus slot retains v0.8.2's stronger selected-draw checks before Signal:
writable DSV identity/lifetime, returned draw and ordered same-queue submissions.
The draw-return marker is published immediately after a validated native draw
return, even when CPU recovery is still pending. Overall core completion still
requires recovery; invalid or exceptional draws publish no return marker.
The baseline/recovery slots are explicitly **request controls**. They have
`site.request_control=true` and `site.marker=0`; they do not invent a draw marker.
Their private Signal follows the submitted native copy after its recording
checks. Before Map, both controls must match the stimulus's pinned depth resource
and canonical identity, and the writable view must still pass its live checks.

All slots require private fence completion, a later successful Reset of the
copied command list and no intervening resubmission of that old recording.
Observation stops only after all slots become terminal and the selected native
queue's completion observation is available, or on the existing deadline/fault.
Missing or failed slots leave the complete triplet rejected. This does not
establish full resource access coverage on other queues.

## Evidence and review

The header uses version `0.8.4`, experiment `projection-jitter-depth-triplet`,
`depth_frame_policy="selected-source-request"`, one injection, three CPU samples,
three GPU copies and three private Signals. Startup ready requires both
`same_frame_readback` and `triplet_readback`. Hooks and code gates are unchanged:
41 hooks, seven startup gates, 39 core gates, 45 distinct native byte spans.

The main `depth_readback` and `-depth.f32` companion belong to the stimulus.
Two `depth_control` records, stages 0 and 2, have companions
`-depth-control-0.f32` and `-depth-control-2.f32`. The frame-packet journal retains
the selected stimulus record plus two explicit control selections. Complete raw
journals are preserved; review never fabricates control draw bindings.

The independent reviewer requires the full CPU baseline/stimulus/recovery
contract and the existing stimulus draw/readback association. It then checks
each control's exact original request stamp, copy arguments, recording generation,
private fence, retirement, pixel hash and statistics. All three copies must use
the same depth and DIRECT queue, occur in baseline/stimulus/recovery submission
order without overlapping queue Execute intervals, use distinct buffers/fences,
and share one observed writable DSV lifetime. Repeated packet consumption,
descriptor reconstruction, changed frames and duplicated private command orders
reject the triplet.

Success reports `depth_triplet_request_stamps_verified` and
`depth_triplet_readbacks_completed`. It keeps
`depth_control_draw_bindings_verified`, `depth_pixel_producer_frame_verified`,
`final_scene_depth_verified` and `raster_jitter_verified` false. The controls
establish request/copy association, not their own selected draw bindings or all
pixel-producing draws.

The native matrix relation adds `(jx/width,jy/height)` in clip-normalized XY.
After the Direct3D viewport transform, the expected pixel shift is
`(jx/2,-jy/2)`. Each component is at most a quarter pixel for the admitted sample
range. The reviewer reports that prediction, not an observed displacement.
After a successful game capture, compare stable scene regions across all three
images; camera or animated-object motion must not substitute for jitter.

## Verification and operation

Executable version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
The prior Windows x64 calling conventions, source ownership assumptions and
native byte gates remain unchanged. Release builds use `/W4 /WX`.

All 241 native tests pass. Five new owned GPU cases exercise three distinct clear
values routed into the production capture slots, missing baseline, wrong recovery
frame, later view change and repeated consumption. Clear values test readback
routing, not raster jitter. The existing 504 Python checks plus 39 triplet
checks pass. Full-review composition uses independent owned CPU/core and GPU
fixtures and is explicitly synthetic, not a native end-to-end game capture.

Use the existing MO2 `barebonesdlss` profile and
`Monster Hunter: World - DLSS 1.2.110 test` entry, with DX12 On, DLSS Off and TAA On.
Load a save, keep the camera still and reply from the phone. Arm only the ready
process generation. Do not manually enable DLSS for this diagnostic.

Local preparation, deployment and prelaunch receipts are under
`local/projection-binding-20260923/v0.8.4/`. They determine installation state.
After capture, preserve all three pixel files and review with the exact archived
source. Exit normally before running `deploy.ps1 -Action Restore`, which restores
the previous DLL and sidecar state and checks all four configuration hashes.
The v0.8.3 failure remains archived and rejected. The subsequent
[v0.8.4 run 48276](../research/depth-triplet-result.md) passes the unchanged
reviewer and produces all three images. Its offline pixel measurement closely
matches the predicted shift on static depth interiors. The previous DLL and
sidecar state have been restored; see the local restored receipt.
