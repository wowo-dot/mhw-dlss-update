# Independent AA-Off continuous DLAA: first live result

2026-09-26, v0.8.0. **Partial result; continuous acceptance rejected.**
The independent route displayed 4,196 DLAA frames before its post-queue
admission check latched the feature off with fault 8. The user reported a clean
still scene and clean panning, then noticed DLAA had stopped. They clarified
that they had only walked and panned: no menu/map or area transition preceded
the stop. This run does not validate loading transitions.

## Identity and evidence

- Host version 0.0.0.0, SHA-256
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- PID 56660, creation 134349288952564621; armed tick 39578000.
- Native AA Off, native DLSS Off, DX12, Windowed 2560x1440, bare offline profile.
- Test DLL SHA-256
  `a75d57fc07f03ae33b53b54a214a4e5573e291f1f66d28b8dddcc3167172709b`.
- Session `local/independent-aa-stream-20260926/v0.8.0/`.
- Final log and `capture-56660.jsonl`: 67,875 bytes, SHA-256
  `cbb230120268eb1792eaff1bbb8aa86aef892355eef747c599f63253ddf0c9a4`.
- Frozen strict `review-56660.json` remains rejected (`healthy stop`).
  `observations-56660.json` records narrower, checked observations without
  converting that rejection into a pass.

## Confirmed

Source frames/epochs 21088-25283 produced 4,196 evaluations, native output
handoffs, marker publications/consumptions and completed GPU signals. SDK
Create/Evaluate returned success and the pass has no failure string. There
were 4,197 scoped jitter injections and 4,197 restorations: the rejected next
frame had already passed projection. No native AA option writes occurred.

The final durable record, after hook removal, shows no pending callbacks,
private recording, GPU work, Reset retirement or submission. All runtime and
binding hooks are disabled and owned descriptors detached. Two completed marker
records and one projected record remained retained after fault because
collection freezes on failure. Private SDK objects and list references also
remained pinned until process exit; this was not a clean full-resource release.
The five-second status file predates final hook removal; use the durable log
for closure claims.

History retained on 4,042 evaluations and reset 154 times: one initial reset
and 153 camera-distance resets. No frame/epoch gap, camera identity, view,
depth, viewport, projection or frame-interval reset was recorded. The inherited
20-unit per-frame camera-distance threshold fired during ordinary movement.
That policy needs a separate quality review; it did not cause fault 8.

## Failure localization and remaining uncertainty

Fault 8 is raised only when `owned::recycle(0)` or `owned::insert(0,q)` rejects
in `schedule_before`. Projection association had already passed. Recycle checks
descriptor ownership and prior removal; insert checks the exact four-node
queue, its input owner, a repeated queue snapshot, and writable attachment.
The v0.8.0 log omitted the failed queue and owned-pass subreason. It therefore
cannot distinguish a changed queue layout from owner/revalidation/attachment
failure or a descriptor lifecycle rejection. Ordinary walking is sufficient
to expose this path; blaming an area load is contradicted by the user report.

Next: retain a bounded first-rejection snapshot including the recycle/insert
stage, exact owned-pass subreason and observed queue, with prior successful
context. Preserve admission checks until that evidence or an offline
reproduction identifies the invalid assumption. Do not blindly bypass the
check, retry after a fault, or activate native TAA. Separately review movement
history resets against camera motion evidence.

## Closure

The user exited normally. Process absence was checked before the sealed helper
restored the exact previous DLL (`689bef7c...`) at
2026-09-26T23:49:09.965709+03:00 and removed the matching output sidecar.
All four configuration hashes remained unchanged. No source, tests or tools
changed during capture or closure. The archive records this as a partial run.
