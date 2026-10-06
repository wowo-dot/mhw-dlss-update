# First same-frame DLAA handoff accepted — v1.17.0

Run 37072 passes the full frozen reviewer. On frame 22879, DLAA ran on the
native TAA command list and replaced the actual native output texture. All
3,686,400 packed pixels match the SDK image. The observed compute/graphics
bindings returned to their pre-injection values. The game process remained
responsive after capture. The user was away during capture, then reported a
normal scene and normal exit. Exact rollback completed at 04:02:05 +03; the
output sidecar was removed and all four configurations remain unchanged.

This establishes one actual native-output replacement. It does not establish
continuous DLAA, retained SDK history, final swap-chain image quality or full
lifecycle stability. Native TAA forwarded first, and the other eight captured
frames used its normal path. The replaced texture may also feed native history.

## Evidence

- Evidence: `local/dlaa-inline-live-20260926/v1.17.0/`.
- Process generation: 37072 / 134348576856444925.
- Host 0.0.0.0 SHA-256:
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- DLL SHA-256:
  `cecc736d0a7a7cf17b5a283345e97b93baf3e315dd9a2c40c81074a7c8d074c2`.
- Prelaunch manifest, 472 verified files:
  `a36cb9723eadee4e858fef0b5c45b0c7f04c0a537954eb4f3a5431c4946dbb73`.
- Capture SHA-256:
  `5ea5739c58166e1f2b4ca59b1a5d6f010713076fe35e93cb2e6dc3233267c88c`.
- Startup log SHA-256:
  `a319f0d7710bd20f289a24e2c4e06c59017e2be611b5ab9e4cb95d467897d2d3`.
- Sealed 545-file archive SHA-256:
  `3f97e1381186b1e1e5e45cbaaed8e6e84d22642abf1c1aa049dcfc69b709c7e8`.

The frozen reviewer accepts frames 22875–22883, 2560×1440, DX12 swap-chain
format 87, adapter LUID [0,97816]. All nine native binding snapshots and raw
input/independent conversion checks pass. Capture shutdown reports zero faults
and pending callbacks; all 29 hooks are disabled. The journal contains 1,518
events and 43 valid depth markers. Admission closed before 875 ms of unhooking.
All 68 run files were preserved before requesting normal exit.

## Selected frame

Receipt 4 uses Reset generation 2560. In the shared journal order, depth copy
ends at 2588, the TAA consumer begins at 2592, motion/color copies finish at
2594/2596, original Dispatch forwards at 2597, and insertion is bracketed by
2598/2599. Dispatch exits at 2600, Close occurs at 2602, and Execute at 2608.
The exact private fence completes at value 1, and subsequent Reset retirement
is observed before collection. Its extra reference survives journal cleanup.

The SDK receives the actual frame's pixel jitter [0.1953125,0.169753075], reset
history, unit motion scale, unit exposure and fixed diagnostic frame time.
Create, Evaluate, ReleaseFeature and DestroyParameters all return success.
Private images/readbacks total 365,690,880 bytes. Converted color, depth and
motion match the independently verified captured inputs bit-for-bit.

All 11,059,200 SDK RGB values are finite and written; 11,059,151 differ from the
input. The R11G11B10 native readback matches independent nearest-even packing
for every pixel. Observed root signatures, root arguments, descriptor heaps and
PSO equal the pre-injection snapshot after restoration.

The frozen aggregate report retains older offscreen review placeholders such
as `sdk_evaluation_verified=false` and `native_state_restoration_verified=false`.
The new path is specifically certified by `inline_dlaa_verified=true` and
`native_binding_restore_verified=true`; it does not claim blanket restoration
of arbitrary SDK state or retained temporal history.

## Next step

Normal exit and exact rollback are complete. Extend the bounded same-frame path across
consecutive frames using one retained SDK history. Queue ordering, per-frame
resource reuse and history resets must be explicit before increasing duration.
Keep the game settings at DLSS Off/TAA On and the bare profile unchanged.
See the [implementation contract](../runtime/inline-dlaa.md).
