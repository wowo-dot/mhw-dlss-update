# DLAA resumes after menu/title/reload — unchanged v1.18.2

Run 51752 on 2026-09-26 completed one normal gameplay-menu and title/reload round
trip with the same DLL that passed the [panning test](continuous-camera-history-result.md).
The user reported normal images after each phase. After reloading, DLAA reset
history once for the scene discontinuity, then retained it for 3,266 evaluations
through the final clean stop. No new renderer or history-policy code was needed.

## Phase evidence

| Checkpoint | Evaluations | Resets | Skips | User report |
| --- | ---: | ---: | ---: | --- |
| Loaded-scene baseline | 1,317 | 1 | 1 | Save loaded |
| Gameplay menu closed | 6,426 | 28 | 84 | Looks fine afterwards |
| Title screen | 8,128 | 59 | 2,708 | Looks normal |
| Same save reloaded | 11,361 | 60 | 6,505 | Save loaded looks good |
| Final stop | 11,395 | 60 | 6,505 | Normal exit confirmed afterward |

The menu interval added nine resets with frame/epoch gaps and eighteen with
camera displacement. A saved displacement example moved 28.836 world units in
10.11 ms; the final menu-interval example moved 20.422 units. These are recovered
camera displacements, unlike the previous erroneous view-translation test.
They satisfy the implemented threshold, but the sparse record does not prove
that every such reset was visually necessary or exactly when a menu action occurred.

The title interval added projection changes. The title checkpoint showed a
missing matching depth/list-generation lease (`last_skip=7`), with zero faults
and all submitted private work complete. This does not establish title-screen
DLAA coverage. The renderer remained responsive and the user reported a normal
title screen. Individual causes of all 6,505 skipped attempts are not logged.

The one reset after the title checkpoint covered a 7,258-frame/epoch gap,
64.573429 seconds between successful evaluations, changed view and depth
identities, a nontranslation matrix jump and 2,726.731 world units of camera
displacement. Its reason mask was 1846. The following 3,266 evaluations retained
history. Multiple reasons can belong to one reset; reason totals must not be
summed as separate resets.

## Verification and shutdown

Independent history review passed for all four preserved raw checkpoints. The
complete stopped-log reviewer accepted SDK mode/formats, counter agreement,
history examples, graphics identity and final closure. All 11,395 private GPU
signals completed, all 18,737 scoped jitter injections restored, zero faults or
pending callbacks, no pending private recording, all hooks disabled. There were
32 tracked command lists and 19 pinned resources, within their 128-entry bounds.

The user confirmed normal exit. Process absence was independently checked before
rollback at 14:23:28 +03. The exact prior DLL was restored, the output sidecar
removed, and all four configurations remained unchanged. The game is closed;
no next diagnostic is installed. Original game assets were untouched.

The exact accepted DLL and unchanged source reused the earlier 68 targeted checks
and saved matrix replay. All 240 files in the preceding archive were verified
before preparation, and all 226 new prelaunch files were reverified at archival.
No duplicate build or GPU test was needed for this unchanged rendering code.

## Limits and next step

This is one successful round trip with user visual reports and bounded telemetry,
not objective live pixel-quality measurement or repeated-transition stress testing.
Cutscenes, focus/alt-tab, resolution changes and device/swap-chain recreation
remain untested. Full resource recreation is not implemented. Native TAA still
runs, and this test provides no performance attribution.

Next: focus changes and a cutscene or abrupt camera transition with the same
phase checkpoints, then address remaining lifecycle behavior before profiling.

## Evidence

Ignored root: `local/dlaa-transitions-20260926/v1.18.2/`.
The final manifest covers 250 files, including 204 frozen sources, phase snapshots,
user reports, startup/final reviews, loaded-module identities and rollback receipt.

- Process: 51752 / creation 134348950486889994.
- Host version 0.0.0.0, SHA-256:
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- DLL: `f460559ffc15d0f8a6fdbb81fc292d18ba2c8713afa406f02a9c189097a0f159`.
- Startup log: `ad89bbc2bcd0d40568d1ebb24c1753935084df427b6097ced3039527099c0a85`.
- Continuous log: `512b0391e7ae62b521aa365f406b880538eed4d0b790095fd08705ecfa82251e`.
- Prelaunch manifest: `df293af5fe83f1c901607760fd03261299962e1673d7b267fd3dfe684e99bfe5`.
- Final manifest: `ff7528af35812292f858bc846c419ca8e7822bc98032f5fde1987f15edfc1dea`.

DX12, 2560x1440, swap-chain format 87, adapter LUID [0,98931].
Arm-to-stop requests span 190.922 seconds; this interval is not a benchmark.
