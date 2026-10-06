# Continuous DLAA transition test — unchanged v1.18.2

This test extends the [accepted stationary/panning run](../research/continuous-camera-history-result.md)
using the exact same v1.18.2 DLL and `camera-displacement-v1` history policy.
No renderer or policy change is introduced. Its purpose is to observe history
and rendering continuity during menus and a loaded-save/title/reload round trip.

Run 51752 completed this sequence on 2026-09-26: normal user-reported scenes,
DLAA resumed after reload with one reset and 3,266 subsequent retained-history
evaluations, zero faults and clean stop/rollback. See the
[result](../research/continuous-transition-result.md). Title-screen DLAA coverage
and broader transition stability are not established by this test.

## Sequence

Use the usual MO2 Portable test entry in the bare offline profile. Leave native
DLSS Off, TAA On, DX12 On and resolution unchanged. Load the save and wait for
startup/module verification and the arm signal before exercising transitions.

1. Save an active baseline in the loaded scene.
2. Open and close a normal gameplay menu, then hold in the scene. Save a second
   checkpoint and obtain the visual report. This is an overlay/menu test; it
   does not by itself establish a camera cut.
3. Return to the title screen and wait there. Save a checkpoint before reloading.
4. Load the same save again and remain in the scene. Verify evaluation progress,
   reset reasons and retained history after the transition, then save a checkpoint.
5. Stop the generation-specific recorder, verify callback/GPU/jitter closure,
   request normal exit, and restore the exact prior diagnostic DLL.

A hard camera discontinuity is only claimed when captured matrix/identity
evidence supports it. A title/reload round trip can establish a scene transition
without proving every cutscene camera path. If evaluation stops or faults, save
the status and terminal log immediately and investigate that boundary before
requesting another transition. The original renderer remains the fallback.

## Evidence and acceptance

Ignored evidence root: `local/dlaa-transitions-20260926/v1.18.2/`.
Each checkpoint preserves the exact raw status, its SHA-256, the requested phase,
process generation, independent history review, and counter/reason deltas from
the preceding checkpoint. User action and visual reports are recorded separately;
phase labels alone do not prove a transition occurred.

Resets during a discontinuity are expected. Check their recorded reasons rather
than imposing an arbitrary exact number. Successful recovery requires growing
evaluation counts and retained-history frames afterward, correct SDK parameters,
matching jitter restoration, zero faults, bounded registries, and completed
private submissions at final stop. A clean image alone does not pass telemetry
verification. No live pixel-quality or performance measurement is added here.

Full device/swap-chain recreation is not implemented. Resolution changes and
alt-tab are separate tests; avoid combining them with this round trip. The DLL
and native resource pins remain alive until process exit, with no hot unload.

## Deployment and rollback

The root's `deploy.ps1 -Action Install` requires the game to be closed, verifies
the exact previously accepted DLL, preserves the current destination and optional
sidecar, and checks all four configurations unchanged. All original game assets
remain untouched. With the game closed, `deploy.ps1 -Action Restore` restores
the exact prior DLL and sidecar state. Do not overwrite the prior sealed run.
