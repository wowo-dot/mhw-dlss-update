# Camera panning retains DLAA history — v1.18.2

Run 52584 on 2026-09-26 verifies the correction to the false translation-cut
detector identified in [v1.18.1](continuous-history-reset-result.md). Across
10,002 live DLAA evaluations, only the initial evaluation reset history. The
remaining 10,001 retained it, including 3,473 evaluations where the old
view-translation check would have reset it. The user confirmed panning and
reported: **"Clean scene; game closed normally"**.

The [new policy](../runtime/continuous-camera-history.md) recovers camera position
from validated view matrices and compares world-space displacement. Other reset
guards and the entire GPU rendering path remain unchanged. The independent
reviewer recomputes positions and reason masks, verifies saved avoided-reset
examples, and reconciles history counters with SDK evaluations.

## Captures and shutdown

| Checkpoint | DLAA evaluations | Resets | Legacy translation resets avoided |
| --- | ---: | ---: | ---: |
| Requested stationary phase | 3,185 | 1 | 0 |
| User-confirmed panning checkpoint | 9,774 | 1 | 3,269 |
| Final stopped log | 10,002 | 1 | 3,473 |

The checkpoint interval includes time before the user's panning confirmation;
it is not a precisely timed motion-only performance sample. Arm-to-stop requests
span 107.875 seconds. There was one skipped attempt. Every noninitial reset
reason count remained zero, and the longest retained streak was 10,001.

All 10,002 private GPU signals completed. All 10,003 scoped jitter injections
were restored, all hooks disabled, and faults/pending callbacks were zero.
The process remained responsive after the stop. After the user confirmed normal
exit and the process was independently observed absent, exact rollback completed
at 14:08:04 +03. The prior DLL is restored, the output sidecar removed, and all
four configurations are unchanged. The game is closed; no next diagnostic is
installed. Native DLSS remains Off and TAA On in the bare profile.

## Verification and remaining work

The Release /W4 /WX build and 68 targeted offline checks passed before deployment,
including 100,000 randomized poses, invalid-matrix rejection, real translation
jumps, rotation cuts, combined reset reasons and boundary cases. Three saved
matrix pairs from the previous live run also retained history through the
compiled new policy. All 224 prelaunch files were reverified during archival.

This run establishes continuous history retention during the tested stationary
and panning phases, with a clean user-reported scene. It does not establish
objective temporal image quality or validate genuine camera cuts, cutscenes,
menu transitions, alt-tab, resolution changes or resource recreation. The native
TAA dispatch still runs; no new performance measurement or attribution was made.

Next: exercise genuine camera cuts and menu transitions with reset-reason
telemetry, followed by lifecycle behavior. Keep performance profiling separate.

## Evidence

Ignored root: `local/dlaa-camera-history-20260926/v1.18.2/`.
The final manifest covers 240 files, including 203 frozen sources, startup and
continuous logs, checkpoints, independent reviews, loaded-module identities,
the exact DLL backup and restoration receipt.

- Process: 52584 / creation 134348940520110774.
- Host version 0.0.0.0, SHA-256:
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- Test DLL:
  `f460559ffc15d0f8a6fdbb81fc292d18ba2c8713afa406f02a9c189097a0f159`.
- Startup log:
  `62b3070d0c4d7ac2a8a05ef975dabb8befcf161ddb8ae51d55ac8e868851b7b3`.
- Continuous log:
  `389e34903d91dd2277240bb018acf25b978186fc4967f901fc7295635df4acdc`.
- Prelaunch manifest:
  `34ae97d14268c64a7be9f5e2022c1aa4c9185d375bb283987004074d91e6182a`.
- Final archive manifest:
  `992707510eb87c6398d783ce2a29db582b11393bdd3cab986d9a4be2198d766b`.

DX12, 2560x1440, swap-chain format 87, adapter LUID [0,98931].
