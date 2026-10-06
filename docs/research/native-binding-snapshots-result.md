# Native binding snapshots accepted — v1.16.0

Run 36704 on 2026-09-26 passes the full frozen reviewer: nine native binding
snapshots and nine retained-history offscreen DLAA outputs. The user confirmed
normal exit. The exact previous diagnostic DLL was restored at 02:57:18 +03;
the output sidecar was removed and all four configurations remain unchanged.
No next diagnostic is installed.

## Evidence

- Process generation: 36704 / 134348538801908752.
- Host 0.0.0.0 SHA-256:
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- v1.16.0 DLL SHA-256:
  `d1602441860b9a56a2b06598a0c540692f3c0e350fcb37aa47286b7f5a879754`.
- Capture SHA-256:
  `ad8f610badf19997c2c3f49fd12cb732b8cdb8c724638fb0bee4c187cc68bcb2`.
- Evidence directory: `local/dlaa-bindings-live-20260926/v1.16.0/`.
- 486-file archive SHA-256:
  `4d6ac60d792ef702732746a40c0c8d5203eeb1e004fa7f9d0cc18f40c429f735`.

The pre-arm check verified 404 sealed preparation files, dependencies, loaded
host/provider/DLL identity, startup capabilities and unchanged settings.
The capture spans frames 23122–23130 at 2560×1440 on adapter LUID [0,97816].
All 29 hooks shut down with zero faults/pending callbacks. Journal admission
closed before 922 ms of hook removal: 1,337 events, all 41 depth markers valid,
and one verified native packet-address reuse.

The earlier launch, PID 28908, reached startup readiness while the chat was
unavailable. No arm request was sent and its log contains no temporal capture.
It is preserved separately within this archive; its exit cause is unknown.

## Observed native bindings

All nine snapshots match the independent native root/PSO caches and the exact
frame, command list and submission-journal Reset generation. The observer
tracked 21 lists; the selected TAA work uses one list with nine generations.
Compute root, graphics root and PSO identities remain stable across the sequence.

| Stage | Descriptor tables | Root CBVs |
| --- | --- | --- |
| Compute | 0, 1 | 2, 3, 5, 6 |
| Graphics | 0, 1, 16 | 5 |

There are two bound heaps per snapshot. The first slot cycles across three
identities, while the second remains stable. The dynamic-resource/stable-sampler
interpretation follows the native binding layout; this recorder did not log
heap `GetDesc` results. Graphics table 16 has the same GPU handle as compute
table zero in every snapshot. The restoration path must preserve graphics
tables as well as compute bindings when it temporarily changes heaps.

Every selected depth copy is in the same command-list generation as its paired
motion/color copies and native TAA dispatch. In all nine frames the recorded
order is depth copy, motion copy, color copy, then native Dispatch forwarding.
For frame 23122, Reset is 1907, depth ends at 1937, motion at 1941, color at
1943, and Dispatch forwarding is 1944. `run-36704.json` records all nine ordered
witnesses. These observations support same-frame insertion at this consumer;
they do not establish that an inserted SDK pass has executed there yet.

## SDK and limits

One private SDK feature completes nine evaluations, resetting history only
on the first frame. Seven scoped jitter injections are restored and associated
with their exact SDK frames. All 232,243,200 converted input floats match the
independent reference; 99,532,800 DLAA RGB values pass output checks. The camera
view matrices are identical throughout the sequence.

This run leaves native TAA on screen. It validates the live binding observer,
not in-game binding restoration or SDK insertion. The existing output-conversion
and restoration primitive has separate WARP/RTX fixture evidence described in
the [handoff contract](../runtime/dlaa-output-handoff.md). No new temporal-quality
or highlight-fidelity claim follows from the capture review.

Next: convert the matching inputs and evaluate DLAA within the selected native
frame, restore every binding affected by the injected work, and replace the
native output with a bounded native-rendering fallback. The fixture should
exercise the observed root-CBV and graphics-table layout before a live write.
Continuous history, real frame timing and lifecycle/visual tests follow.
