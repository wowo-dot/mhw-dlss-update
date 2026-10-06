# Prepared color-chain capture: live result

2026-09-30, v1.25.16, PID 2596 / creation 134352074187384157. DLL SHA-256:
`82260448154c3798347af46bb5f159d189d5edac7d6ae1ea5d2aaf1db5437d77`.
Same pinned host and bare profile, DX12 windowed 2560x1440, active 1280x720;
native AA, DLSS and MotionBlur Off. See the
[recorder contract](prepared-color-chain.md) and
[shared output investigation](shared-renderer-boundary.md).

The user reported the main menu ready, then a clean loaded save. After the
intentional stop they confirmed a normal scene and normal game exit. This was
a short structural capture, not a broad visual compatibility test.

## Depth correction now exercised live

The saved main-menu checkpoint has 650 DLSS frames, zero faults, 651 accepted
depth copies and no depth rejection. Retained receipts prove D3D12 combined
shader-read state 192, previously rejected by the pixel-only rule. This scene
used native temporal shader technique 23086; the alternative 29842 shader was
not exercised, so this does not validate that separate variant fix.

Across the run, 3,255 of 3,256 depth attempts were accepted. The retained
checkpoint/log receipts include render-target state 4 and combined read state
192; sixteen distinct retained receipts put reconstruction inside an already
begun read span. The reviewer validates their full recorded transition chains.
Thus both new read-state and begun-read admission were exercised in this run.
This does not prove every state combination or reproduce a particular weather
condition. One depth rejection, reason 5, was counted but its detailed receipt
was not retained by the bounded first/latest samples. Its specific cause is
unresolved; do not infer it from neighboring successes.

## Native preparation evidence

All 26 attempted queue captures succeeded. Twenty-five distinct snapshots
remain in the combined log/checkpoint/timeline evidence; one intermediate
latest sample was replaced before the periodic status recorded it.

Two descriptor arrangements were retained. The menu checkpoint includes an
additional phase-15/order-1 unit, vtable RVA `0x353e048`, before temporal
reconstruction. Gameplay snapshots omit that entry. Its effect name is not
established by this capture. Both arrangements have connected descriptor
adjacency from the temporal output through the native queue tail.

The allocation metadata remains 2560x1440 while prepared motion/grading draw
rectangles are 1280x720. The final-screen draw rectangle is 2560x1440. The extra
pre-temporal unit's retained draw prefix includes 640x360 work. The two scratch
owners change roles with the descriptor arrangement; a fixed texture pointer
cannot stand in for the semantic color stage. These are CPU preparation
observations, not a substitute for actual GPU binding/state proofs.

The runtime also completed 57 haze routes. The periodically sampled new queue
channel did not retain that brief arrangement. Do not claim this diagnostic
enumerated all encountered effects or that it replaced their specialized code.

## Remaining output-order issue reproduced

Thirty-two unfiltered frames comprise 29 output-readiness reason-7 rejections,
two pipeline warmups, and the one depth rejection. All nine suspension periods
resumed; final reconstruction state is not waiting and no failure is latched.

The retained reason-7 receipts include executed mask 6 (grading and final draw
already recorded) and mask 4 (final only). Their submission receipts put the
temporal list at index 1 and the affected downstream list at index 2 of the same
queue/batch. GPU ordering is suitable in these examples, but the existing
late claim cannot alter downstream commands already recorded on another CPU
worker. Keep that rejection: deleting it would not make those commands correct.

The next implementation must establish dimensions, admission and ownership
before participating workers record their commands, while preserving native
auxiliary-coordinate behavior and resource retirement. No new per-effect
adapter or output-migration implementation was added during this run.

## Stop and evidence

The frozen complete reviewer accepted 3,224 DLSS frames with zero runtime
faults. All 3,255 GPU recordings completed and released; all 3,643 jitter changes
were restored. All 3,224 output routes completed and released, zero live. Full
resolution returned, pending callbacks drained, and hook groups reported off.
Independent read-only comparison verified 38 complete native code spans and
17 hook heads restored. The collector ran for 114.016 seconds, retaining 22
snapshots/events; seven status-read retries caused no collector fault.

Private evidence: `local/prepared-color-chain-20260930/`, including separate
menu/gameplay checkpoints, `queue-analysis-2596.json`, the timed collector,
frozen source/tools, review and native-code restoration receipts. The newest
DLL stays installed as requested. After archival, prepare a fresh output root
before another launch; the installed sidecar still names this completed run.
