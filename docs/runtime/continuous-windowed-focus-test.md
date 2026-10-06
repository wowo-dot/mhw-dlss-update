# Windowed DLAA and focus recovery — unchanged v1.18.2

The user set ScreenMode=Windowed and closed the game after the accepted
[fullscreen menu/title/reload test](../research/continuous-transition-result.md).
Preserve that choice. Current saved resolution is 2560x1440, with native DLSS Off,
TAA On and DX12 On. No renderer, policy or SDK change is needed to test this mode;
display-mode independence is a hypothesis until the live run passes.

Run 49168 showed active windowed DLAA and a healthy first focus-return checkpoint,
but the overall run failed with a live request-slot collision. The user clarified
that area loading also occurred between the two focus reports, so it is not an
isolated focus test. Closure and exact rollback completed; see the
[failure result](../research/windowed-area-transition-failure.md). Keep windowed
mode and investigate request lifetime before repeating transition validation.

## Sequence and evidence

1. Launch the usual MO2 Portable test entry in the bare offline profile, load
   the save, and remain in the scene. Do not resize/maximize the window or change
   resolution during this comparison.
2. Verify startup, exact loaded DLL/provider, extent/format and process generation.
   Arm only after the save is loaded. Save a growing-evaluation windowed baseline
   and obtain the user's image report.
3. Ask the user to Alt-Tab to another application for about five seconds, return
   to the game, then wait in the scene for about five seconds. Save a checkpoint
   and inspect SDK progress, resets/retention, resource identities and faults.
4. If healthy, repeat the focus-away/back cycle once to check repeatability.
   A history reset is expected only if the recorded history guards require it;
   continued background rendering can preserve frame continuity.
5. Stop, verify callback/GPU/jitter closure, request normal exit, then restore the
   previous diagnostic DLL and prior sidecar state. Keep the user's windowed mode.

If admission faults, preserve the stopped log and investigate before further
actions. Full device/swap-chain recreation is unimplemented; this tests focus
changes at a fixed extent, not resizing or switching display mode while armed.
Cutscenes and abrupt in-scene camera transitions remain separate coverage.

Evidence root: `local/dlaa-windowed-focus-20260926/v1.18.2/` (ignored). Reuse the
exact accepted v1.18.2 DLL and inherited checks; verify the prior archive and
unchanged source/dependencies instead of rebuilding identical code. Checkpoint
phase labels and user confirmations are separate records. Clean images alone
do not establish correct SDK progress or full temporal quality. No performance
claim follows from this test, and the native TAA dispatch still runs.

The root's `deploy.ps1 -Action Install` backs up the destination and all current
configuration identities. `-Action Restore`, with the game closed, restores only
the test DLL/sidecar and checks configurations; it does not revert windowed mode.
