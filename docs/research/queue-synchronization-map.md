# Queue synchronization map and adjacent-batch audit

2026-09-24, offline follow-up to [run 47376](projection-submission-result.md).
The retained capture already supports a **same-DIRECT-queue declaration** of
DEPTH_WRITE at the selected draw. This is narrower than a complete resource
state or cross-queue safety proof. The original runtime review is preserved.

## Adjacent batches

The immediate preceding DIRECT Execute at orders 4629/4630 contains a complete
recording generation, Reset 4291/4292 through Close 4611/4612. It records the
selected depth resource's `0x800 → 0x10` transition and matching clear. The
selected DIRECT Execute at 4633/4634 contains a complete Reset/Close generation
and places the selected draw in its first list, with no intervening relevant
barrier in that list before the draw. Replay of just these two generations
produces one transition, one clear and a final declared state of `0x10`.

Separate Execute calls on one queue impose workload ordering. This is stronger
than the ordering of whole lists within one Execute batch, where work may
overlap. [Microsoft's ExecuteCommandLists contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-executecommandlists).
The captured resource is a non-simultaneous-access depth texture; this audit
does not invent a COMMON-state decay between these DIRECT batches. The depth
texture and state-decay restrictions are documented in Microsoft's
[resource-barrier guide](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12).

The new field is `same_queue_depth_state_declared`, scoped to that preceding
batch and the selected batch prefix. It does not change v0.5.0's
`submitted_depth_state_declared=false` verdict, which had a stricter single-batch
scope and excluded other observed queues. No missing live synchronization
events are manufactured by this offline replay.

## Static routine correlated with actual submission callers

The executable is version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
The decoded image is
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
Both hashes are checked before instruction analysis. The full routine
`23A7930..23A7F46` hashes to
`0A5465C55BC22042112F78CF0448CDD8CD8482C06D50244F860D7F1EDD68740F`.
Thirty-one instruction assertions and nine SDK slot checks pass.

The observed return sites identify backend `+20` as this DIRECT queue,
`+208` as COMPUTE and `+1128` as COPY. The decoded branch contains:

| Order in the decoded branch | Call RVA | Observation |
| --- | --- | --- |
| DIRECT preceding batch | 23A7D72 | Return 23A7D75 is observed in run 47376 |
| DIRECT Signal | 23A7D91 | Fence from backend +230, incremented value from +238 |
| COMPUTE Wait | 23A7DAC | Same fence and value |
| COMPUTE Execute | 23A7DCA | Return 23A7DCD is observed |
| DIRECT selected batch | 23A7E0B | Return 23A7E0E is the selected submission |
| COMPUTE Signal | 23A7E2D | Same fence, next incremented value |
| DIRECT Wait | 23A7E45 | Same fence and next value |
| DIRECT remaining batch | 23A7EA5 | Observed in earlier captured cycles |
| DIRECT final Signal | 23A7EF4 | Fence from +168, counter from +1E0 |

The native routine therefore contains a bidirectional graphics/compute
handoff and a separate final graphics signal. That is static evidence, not
the missing actual fence pointers, values, HRESULTs or completion reads.
The selected DIRECT batch can lie alongside compute work between the two
handoffs; no assumption that all compute work precedes the draw is valid.
Four Execute callers lie outside the game image and are kept as unresolved
external call sites; they are not decoded as game RVAs.

## Next observation

[v0.6.0](../runtime/projection-sync-recorder.md) records actual queue Signal/Wait,
fence GetCompletedValue and CPU fence Signal, with pinned fence identity and
creation flags. It extends the existing bounded capture through a native
completion read of a signal following the selected submission. It adds no
signals, waits, transitions or copies to the game.

Exact-value Signal/Wait pairs will be reported as recorded API correlations.
They are not a complete dependency graph: higher or earlier unobserved writers
can matter. Completion observation requires a below-target baseline, an
unshared fence, successful monotonic signals from the selected queue, no
observed competing CPU/queue writer, and a later native read reaching the
target. Its scope is the recorded window. All-time fence history, complete
resource access coverage and GPU contents remain outside that claim.
See Microsoft's [multi-engine synchronization guidance](https://learn.microsoft.com/en-us/windows/win32/direct3d12/user-mode-heap-synchronization)
for the distinction between queue ordering and fence dependencies.

Artifacts and assertions are private under `local/queue-sync-20260924/`:
`audit.py`, `audit.json`, `queue-callers.json` and the routine disassembly.
The audit records the unchanged original review hash and all source hashes.
