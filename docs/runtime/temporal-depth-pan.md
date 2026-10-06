# Camera-pan depth/color/motion timing check

Captured and closed 2026-09-25:
[run 31044](../research/ordinary-taa-depth-pan-result.md). The frozen reviewer
accepts all three sets; revised static-scene analysis supports current
depth/color timing in 12 qualified frame/region comparisons. Previous DLL
restored, sidecar removed, configurations unchanged. No next test installed
or armed. Typed views and evaluation-point state/lifetime are next.

The procedure below reuses the **unchanged v1.9.0** recorder accepted in
[stationary run 25300](../research/ordinary-taa-depth-pixels-result.md).
This is a new stimulus/run, not a new recorder version. The tested DLL SHA-256 is
`61c643d62ce6cde8d7e76e7dd70b53a4ca240e7bad82b42af6d2a4c7cbdbae24`.
The [existing hook, state, ownership and bound contract](temporal-depth-pixels.md)
applies unchanged: three depth/color/motion sets, ten hooks, 27 observer GPU
commands, three private Signals, no jitter/descriptor writes or SDK calls.

## Preparation and procedure

Fresh evidence directory: `local/temporal-depth-pan-20260925/v1.9.0/`.
Verify all 207 files in the closed stationary archive and its archive hash,
the current host executable hash, exact restored destination DLL, sidecar
absence and four configuration hashes. Use the archived DLL and its frozen
source/reviewer snapshot. The 115 native / 55 Python prelaunch checks (nine
jitter-only skips) are inherited from that exact artifact; they are not newly
executed or represented as a new test run. Preserve current planning documents
separately from that historical source snapshot.

Install with the game closed, preserving the exact destination and sidecar.
The deployment and prelaunch receipts establish whether installation finished.
MO2 entry: **Monster Hunter: World - DLSS 1.2.110 test**, same bare profile.
Keep DLSS Off, TAA On, DX12 On and 2560x1440. Use an offline/private save.

Load the same save, then continuously rotate the camera at a moderate pace in
one direction, with some slight vertical movement if comfortable. Keep the
character standing in place. Avoid camera resets, menus and direction reversals
during the short capture. Reply from the phone with **Panning** while continuing
the movement; the recorder is armed only after current-process identity,
readiness, installed DLL and the prelaunch/configuration hashes are verified.
No need to match a particular angular speed or pose. Stop panning after the
capture confirmation, then leave DLSS Off and exit normally when asked.

## Analysis question and controls

The stationary run verifies native-copy and exact TAA-generation association,
finite depth and zero-offset integer-grid depth/color edge agreement. A still
background cannot discriminate an earlier frame. This run adds camera motion
to test that distinction using saved pixels rather than metadata alone.

1. Apply the unchanged frozen reviewer, including restored copy-point state,
   exact TAA generation, completion, retirement and artifact hashes.
2. Decode the same packed color/motion and R32_FLOAT depth formats. Inspect
   all three pairs and repeat fixed-region depth/color edge-offset measurements.
3. Compare same-frame depth/color spatial agreement against adjacent-frame
   alternatives. Use static scene geometry, exclude moving actors, transparencies
   and disocclusions, and retain rejected/ambiguous regions.
4. Use current-to-previous motion coordinates
   `(-decoded_x * width / 2, +decoded_y * height / 2)` as the tested prediction.
   Compare native-scale current vectors against zero motion, wrong sign and
   adjacent-frame vectors. Depth values can change during a pan; do not assume
   identical raw depth values after a 2D warp or infer metric distance from them.
5. Treat low movement, poor textured overlap, nearly indistinguishable adjacent
   fields or inconsistent depth/color results as inconclusive. Do not promote
   broad content-alignment, final-writer, typed-view or DLAA-ready flags based
   solely on a visual match or capture acceptance.

If timing is supported, proceed to typed input views and the state/lifetime
contract at the proposed evaluation point. No SDK integration is added here.

## Rollback

After normal exit, run this evidence directory's `deploy.ps1 -Action Restore`.
It verifies the installed DLL, preserved backup and output sidecar before
restoring the exact previous DLL and sidecar state. Verify all four saved
configuration hashes; archive this run separately. Do not modify the closed
stationary archive or rerun its preparation/deployment scripts.
