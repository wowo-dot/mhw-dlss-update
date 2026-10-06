# Windowed DLAA works; mixed focus/area test hits request-slot collision

Run 49168 on 2026-09-26 used the unchanged v1.18.2 DLL with the user's saved
Windowed mode at 2560x1440. The loaded-scene baseline and first Alt-Tab return
checkpoint both showed successful, active DLAA with zero faults. The later
interval ended with **fault 6: a TAA request slot was still live when a producer
tried to reuse it**. The overall run is a failure, despite a normal reported image.

The user subsequently clarified that they moved during the first focus interval
and switched areas through a loading screen **between the first and second
Alt-Tab reports**. This was therefore a mixed focus/area-transition run. The
evidence does not establish focus loss alone as the cause, or locate the fault
precisely before versus after the loading screen. The clean image after loading
may reflect fallback to the native renderer.

## Evidence and closure

| Checkpoint | Evaluations | Resets | Skips | Fault | Tracked resources |
| --- | ---: | ---: | ---: | ---: | ---: |
| Windowed baseline | 1,762 | 2 | 1 | 0 | 6 |
| First focus return | 5,409 | 70 | 17 | 0 | 6 |
| Second report / terminal status | 13,690 | 635 | 159 | 6 | 9 |

The first interval included 67 camera-displacement resets and one frame/epoch-gap
reset; movement was confirmed by the user. Later counters included frame gaps,
projection changes and displacement resets. Sparse snapshots do not map each
event to a user action. Reason counts overlap and are not independent reset totals.

The SDK itself reported 13,690 successful evaluations and an empty failure string.
There were 13,835 depth copies/private GPU signals, all completed: some private
recordings copied depth without completing an evaluation. All 14,377 scoped
jitter injections were restored. Pending callbacks and private recording were
zero, all hooks disabled, and the process stayed responsive. The recorder's
terminal health was false. The ordinary success reviewer rejected the run;
`failure-review-49168.json` records that rejection and independently checks closure.

The user confirmed normal exit. After observing process absence, exact rollback
completed at 14:49:25 +03. The prior DLL is restored and the output sidecar removed.
Windowed mode, resolution, DLSS Off/TAA On and all rendering options were preserved.
The graphics file differs only in `[Window] PosX`, changed from 0 to 1280. The
assistant made no configuration writes; the other three configuration hashes
are unchanged. No next diagnostic is installed.

## Confirmed code path and unresolved cause

`src/continuous_dlaa/runtime.cpp` has 32 TAA request entries indexed by frame
modulo 32. `taa_after` raises fault 6 when its selected entry is still live. A
matching `consumer_before` normally clears that flag. However, consumer handling
returns before lookup while any producer is publishing.

The final log does not retain the conflicting requests. It cannot distinguish
a second qualified producer in the same frame from an older unconsumed request
on wraparound, a delayed/omitted consumer during loading, or a consumer bypassed
by the publishing guard. These are hypotheses, not established causes. Increasing
the ring or overwriting live entries would hide the missing lifetime proof.

Next: add bounded first-collision metadata for old/new frame, epoch, packet,
publication sequence, view/context/layer and pending recording. Count consumer
bypasses during publishing and unmatched consumers, with bounded examples rather
than per-frame logging. Exercise same-frame collision, unconsumed wraparound and
producer/consumer overlap with owned CPU fixtures before choosing a retirement
rule. Keep failure closed and separate focus from area loading in later tests.

## Archive

Ignored root: `local/dlaa-windowed-focus-20260926/v1.18.2/`.
All 228 prelaunch files were reverified at archival. The 253-file final manifest
includes 205 frozen sources, checkpoints, user corrections, failure analysis and
the exact restoration/configuration comparison.

- Process: 49168 / creation 134348966026012165.
- Host version 0.0.0.0, SHA-256:
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- Test DLL: `f460559ffc15d0f8a6fdbb81fc292d18ba2c8713afa406f02a9c189097a0f159`.
- Startup log: `05849b534355f9602d1651c9cc67dd0b906519952304c17a7d4dd3228bab21a7`.
- Continuous log: `88e0242f92c75cabeaa8f903b75ace7cd02eb63bbb79a5d9f4e7285e8042d114`.
- Prelaunch manifest: `03d931665a0338ae4c2b77f9422b9b0e671323d1c803f6c64a0c1f02459f2d25`.
- Final manifest: `17b7ba4a6121c5a7af877c715df2425fbe3176b7fae53b4baa1e1475f4f62643`.

DX12, 2560x1440, swap-chain format 87, adapter [0,98931]. Windowed operation is
supported by the observed successful evaluations; broader transition reliability,
cutscenes, resizing and full device/swap-chain recreation remain unverified.
