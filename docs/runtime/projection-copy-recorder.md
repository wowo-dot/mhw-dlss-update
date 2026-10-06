# Native copy recorder v0.7.1

Completed game result: [run 6868](../research/depth-copy-submission-result.md).
The corrected recorder captured 52 depth copies with submitted generations,
across 13 repeated four-copy cycles. All 286 copy references released, hooks
disabled, game exited normally and the prior DLL was restored. The runtime
cycle is closed; no game readback commands have been implemented or deployed.

## v0.7.1 correction

The [first game run](../research/depth-copy-result.md) stopped at the shared
resource-initialization guard during warmup. v0.7.1 retains the same copy API
scope, 39 hooks and schema 4, but each call now owns two independent resource
references. After drain, the worker merges consistent metadata into the existing
128-resource registry and releases all retained references, even when merging
fails. It compares description fields explicitly, without comparing struct padding.
No wait, shared initialization or COM lock is added to renderer threads.

Readiness adds `copy_retention=per-call-until-drain` and `copy_pin_capacity=1024`;
v0.7.1 review and arming require that policy. The trace records acquired/released
reference counts; a clean run requires both to equal twice its copy count.
The earlier v0.7.0 schema remains readable. The new archive directory is
`local/projection-binding-20260923/v0.7.1/`, with its own preparation, deployment,
preflight and restoration receipts. The procedures below otherwise remain the same.

v0.7.1 verification: Release `/W4 /WX`, 131 native checks (17 copy-related and
114 remaining projection checks), 390 Python checks, 30 retained owned journals.
The 24 earlier v0.7.0 fixture reviews and six accepted game reviews replay unchanged;
failed game captures, including run 47748, remain rejected. These are preparation
results; the completed game result is linked above.

The earlier v0.7.0 implementation and verification are documented below and
preserved unchanged in that version's source archive.

Preparation for one bounded, read-only DX12 capture with the existing bare
profile, TAA On and DLSS Off. No settings menu, provider SDK, jitter injection,
game resource Map, or additional game GPU commands are included.

## Scope and evidence

Extends v0.6.0's DSV lifetime, draw binding, Reset/Close/submission and fence
journal. Startup now verifies six native code spans, including the entire
681-byte copy wrapper documented in [the offline map](../research/depth-copy-map.md).
The existing 39 core spans remain unchanged. Hash mismatch prevents arming.
Both the host hash and decoded hash are recorded in the map and preparation.

The two new Windows x64 hooks use system-discovered
`ID3D12GraphicsCommandList::CopyTextureRegion` (slot 16) and `CopyResource`
(slot 17). CopyTextureRegion has RCX list, RDX destination location, R8D X,
R9D Y, then stack Z, source location, source box. CopyResource has RCX list,
RDX destination resource, R8 source resource. The existing forwarding thunk
preserves registers, stack arguments, exception propagation and LastError.
Production admits only returns at `0x259E0E2`, `0x259E100`, `0x259E1A8` relative
to the verified image. It does not claim coverage of all renderer copies.

Schema 4 adds history kinds 15/16, a 512-entry copy array and 128 unique retained
resource records. Other limits and the two-second recording window are unchanged.
There are 39 hooks: 12 native and 27 API hooks. Every copy has entry/return order
in the same clock as the draw marker, barriers and submissions. The command
list is retained through the existing registry. Source/destination resources
must round-trip to the exact ID3D12Resource pointer, provide a canonical IUnknown,
return the exact device, and supply a nonempty GetDesc. First initialization
overlap, invalid reads, overflow, exceptions or drain failure reject the run.

Copy locations serialize only defined union fields: resource, type,
subresource/offset, footprint format/width/height/depth/row pitch. Type 2 denotes
the whole-resource CopyResource API. Box presence and six box words are explicit;
absent fields remain zero. Pointers to caller-owned locations/boxes are read only
while the call is active. No pixels or unrelated process memory are captured.
Retained resource references release only after admitted calls have drained and
hooks are disabled. The DLL and trampolines remain loaded until process exit.

The reviewer validates coverage, types, identities, devices, bounds and released
references. It reports each selected-depth copy's arguments, resource descriptions,
actual recording and submission, and any local COPY_SOURCE declaration. Missing,
repeated, overlapping or failed generations remain partial/rejected. Native call
sites are checked against the live-gated image. All GPU contents, jitter and
readback-readiness claims remain false.

## Verification and run procedure

Owned SDK fixtures submit whole-resource, texture-to-texture and texture-to-readback
copies. The readback fixture clears a 16×16 depth image to 0.375 and verifies every
pixel after its own fence completes, including a nonzero placed-footprint offset.
These are test-owned resources and commands; the production recorder does not
perform those operations. Separate admission-only cases exercise distinctive XYZ
and box decoding, bad pointers, null resources, capacity, exceptions and a pending
call during cleanup without forwarding invalid work to the driver.

Release `/W4 /WX`, 201 native tests and 381 Python review/control tests pass.
Twenty-four owned journals are retained. The current preparation and test receipts live under
`local/projection-binding-20260923/v0.7.0/`. Treat deployment as ready only when
`preparation.json`, `deployment.json` and `prelaunch.json` all exist and agree.
Preparation seals sources and the tested DLL; installation preserves the exact
previous DLL at `previous-slot.dll.disabled` and any pre-existing output sidecar.

Launch the existing MO2 bare-profile diagnostic entry, load a save with DLSS Off,
then arm the recorder using the matching PID/creation/log after it reports ready.
No camera movement or DLSS toggle is required for this copy observation. Archive
and review the two capture channels. After normal game exit, invoke
`deploy.ps1 -Action Restore` in this archive directory. It verifies identities,
restores the exact prior slot, removes/restores the sidecar, and records config
hashes. Never replace the DLL while the game is running.
