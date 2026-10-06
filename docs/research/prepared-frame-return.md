# Prepared frame return and fault shutdown

2026-09-30, v1.25.18. Local evidence: `local/frame-output-return-20260930`.
This corrects the failed v1.25.17 main-menu run documented in
[prepared-frame-output-first-run.md](prepared-frame-output-first-run.md).

## Entry identity survives native register changes

The native temporal executor uses the Windows x64 calling convention. Its R8
argument identifies the prepared packet on entry, but R8 is volatile across
the call. `mhw_binding_forward` intentionally records the native return
registers into the supplied frame. The old cleanup used the updated R8 as if
it still identified the entry packet. A changed value left the thread-local
resolve scope active and rejected the next frame with route fault 8.

`native_seh` now saves the entry packet in a separate local before forwarding
and uses that local for `finish_resolve`, including exceptional return cleanup.
The return register frame remains unchanged. Resource states, accepted effects,
GPU output selection, jitter and reconstruction inputs are not relaxed.

This is a confirmed ABI bug, reproduced by the real forwarding-wrapper fixture.
The live trace's first fault is consistent with it, but v1.25.18 still needs a
game run. The later `stream draw order or state` message in v1.25.17 does not
establish an independent inherited-state bug.

## Faulted work is retained, interception can stop

Previously, an incomplete rejected route prevented the stop worker from
disabling the remaining native hooks. On a route fault, the stop worker now
waits a bounded period for route callbacks to return, seals any undecided GPU
selector to its original native branch, and removes interception. It also
waits for route callbacks after removing the patches before stopping the D3D12
observer. Healthy routes still drain normally.

Abandonment preserves the first fault and all uncertain recording/resource
pins. It never invents resolution, submission, fence completion or retirement.
Published selectors stay immutable. Late route callbacks cannot acquire a
frame after this cutoff. A callback that cannot quiesce still prevents unsafe
cleanup; the module and trampolines stay pinned. Telemetry exposes `abandoned`;
the v1.25.18 reviewer explicitly rejects it on a purported successful run.

The native hook targets, ABI, executable hashes and ownership contracts remain
those in [prepared-frame-output.md](prepared-frame-output.md). No new native
hook is added. The stop audit still covers 38 gated functions and 18 native
hook heads. Configuration stays native AA/DLSS/MotionBlur Off, DX12, windowed
2560x1440, with the current Performance prototype rendering 1280x720.

## Verification scope

`scaled_return_fixture.hpp` exercises the actual runtime and assembly wrapper:
two consecutive native calls deliberately clobber R8, preserve RAX and return
R8 exactly, finish the correct entry packets, and clear the resolve scope.

Its fault case installs actual MinHook detours on fixture-owned code, creates
an incomplete retained recording, and verifies that active admission and an
in-flight callback prevent abandonment. A delayed callback return allows the
actual stop worker to restore exact native bytes and behavior for both hook
owners while retaining the uncertain recording and the original fault. It
checks that the selector stays native and late callbacks cannot claim it.

GPU regression cases exercise reordered CPU recording, stop during a prepared
frame, and alternating haze with aliased scratch targets on WARP and RTX 5090.
They use the actual production routing and submission callbacks and verify
pixels and recording lifetimes. These fixtures do not run the game or evaluate
NGX. The prior fixture-only auxiliary-texture validation discrepancy remains
documented in the parent design; no diagnostics are suppressed.

The preparation receipt records exact build/test counts, frozen source hashes,
prior archive verification and deployment identity. The last accepted live
run remains v1.25.16 until a later capture passes; v1.25.17 is a sealed failure.

## Next runtime check and removal

Start the same bare offline MO2 test entry at the main menu. Verify startup
identity, arm, and establish more than one successful frame before loading a
save. Then check panning, menus and an area transition. Retain all failure
telemetry and verify native hook bytes independently at stop.

The deployment preserves the exact destination DLL and output sidecar. With
the game closed, the local `deploy.ps1 -Action Restore` can restore them;
prepare a fresh log destination before another launch because its previous
sidecar points at sealed evidence. Keep the latest DLL installed after tests
unless the user requests otherwise. Game executable/assets and settings are
not changed.
