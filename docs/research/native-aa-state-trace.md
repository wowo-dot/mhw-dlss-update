# Remaining native state rejects — v1.21.1 diagnostic

Prepared 2026-09-27 after the [v1.21.0 recovery result](native-aa-unfiltered-result.md).
This diagnostic preserves all rendering/admission behavior and adds bounded
evidence at state rejection. No state is accepted merely because native rendering
used it, because a UAV barrier exists, or because the scene looked correct.

## Confirmed offline observations

The previous run's four native forwards failed the color/output state gate.
Its counters cannot distinguish missing transitions, aliasing, flags/split
transitions, subresource transitions, a discontinuous chain or a wrong final
state. The raw color and output checks also share that counter. No causal
conclusion about those four events can be recovered from the old counter alone.

Pinned executable version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Offline decoded image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Both identities were verified before the new instruction exports.

Native compute consumer RVA `259CC20` calls the packet-state converter at
`259BA50` before binding and flushing its deferred ResourceBarrier array. The
converter traverses `packet+8`: linked nodes have next at +0, backing at +8,
native before/after words at +18/+1A, and D3D resource at backing+20. Equal
native before/after words take the branch at `259BAB3` that constructs a type-2
UAV barrier. Unequal words are translated into an all-subresource transition.
The final array is flushed at `259CF98`, returning `259CF9E`, before the native
Dispatch at `259CFC7`/return `259CFCA`.

Thus a same-state entry can produce no transition in the observed consumer
scope. The current `state_of` intentionally does not infer a resource state from
a UAV barrier alone. **Hypothesis:** the four misses may be such entries. This
needs live resource/packet matching; static branch existence is not proof that
the rejected calls took it. Native metadata will be diagnostic observations,
not new admission authority or an independently proved GPU state.

Complete exported spans (unwind entries alone truncate the two helpers):

| RVA | Bytes | SHA-256 |
| --- | ---: | --- |
| 259CC20 | 1283 | `6921c7cbfc7b277f4b076a68de8165bd54b3f6571ca574fdb16d5c35cbe154da` |
| 259BA50 | 495 | `f560506e570fff4dd21df27e5d68db204420e82e3eed1bb5aae2cc3ad1441ee5` |
| 259F610 | 924 | `936ac61d6575029672cd657bc8956e63f00d8ac50df2ede414d8c93dcf886bb5` |

Exports remain ignored/local under `local/native-aa-state-20260927/static/`.
The converter/descriptor helpers are not newly hooked or called by the mod.

## Trace contract

Existing hooks and calling conventions remain those in
[native-aa-postqueue.md](native-aa-postqueue.md) and
[native-aa-unfiltered.md](native-aa-unfiltered.md). No additional game/D3D hooks,
GPU commands, resource references, settings writes or shader changes were added.
The original `state_of` implementation remains byte-for-byte identical as source.
The new explanation function mirrors it solely for diagnostics.

At an output or temporal-motion state reject, under the runtime mutex, retain
the first eight samples and the latest sample beyond those eight. Each sample
contains tick/frame/epoch/sequence, list/generation/packet identities, existing
owner/backing/resource metadata for color/output/motion, the classification,
and up to sixteen relevant barriers already observed in this consumer. The
optional native packet-chain read visits at most 128 nodes and retains sixteen
matching entries. Bad reads/cycles/capacity stop that diagnostic read and mark
it incomplete; they never change render admission or raise a renderer fault.
These bounded reads are not an atomic snapshot of unrelated engine writers.

`state_trace` schema 1 policy is `consumer-local-state-v1`. Its output/temporal
rejection counts sum to `dispatch_reasons[6]`. Samples contain:

- `resources`: color, output, motion. Their proof array is
  `[result, last_valid_state, transition_count, last_or_failing_barrier_index]`.
- Proof result: 0 match, 1 missing transition, 2 aliasing, 3 flags/split,
  4 non-full subresource, 5 chain mismatch, 6 unexpected final state.
- `barriers`: `[index, type, flags, subresource, before, after, mask, other_mask]`.
  Role bits are color=1, output=2, motion=4; null pointer=8, unrelated=0.
  Aliasing uses both masks. Other barrier types use only the first mask.
- `native`: `[node_index, role_mask, native_before_word, native_after_word]`.
  These words are native enum values, not already-converted D3D12 states.
- Total/relevant counts disclose truncation. The Python reviewer replays the
  complete relevant API traces independently; it does not invent missing rows
  or claim replay verification for a truncated sample.

There are no per-frame allocations or unbounded histories in the game. Trace
capture happens only on rejection. The original SDK, history, activation,
recovery, submission and resource-retention paths remain as in v1.21.0.
This is still `exclusive_mode=false` and does not fix the rejected path yet.

## Verification and next run

Release compiled with project `/W4 /WX`. All 38 native and 119 Python checks
pass. The native fixture compares diagnostic classification with the original
state gate across named edge cases and 4,096 generated traces. It exercises
same-state UAV-only evidence, missing/valid/wrong states, aliases, flags,
subresources, conflicting chains, bounded samples and malformed native lists.
Python independently replays native fixture traces, rejects altered evidence,
checks truncation/first-sample immutability, and checks complete version parsing
over clearly synthetic trace metadata. An initial snapshot-immutability test
failed because the reviewer returned nested references; it now deep-copies the
result. The fixed test passes; the runtime was unaffected by that parser fix.

Run the same bare MO2 entry with AA/DLSS Off and Windowed 2560x1440. Arm after
save load, inspect the first rejects, then request one area load if needed to
capture recurrence. A full repeated stability sequence is unnecessary for this
evidence-only change. Preserve the state rows before ending the test. Runtime
install/rollback uses the exact baseline DLL/sidecar backup with the game closed.
Evidence root: `local/native-aa-state-20260927/v1.21.1/`.
