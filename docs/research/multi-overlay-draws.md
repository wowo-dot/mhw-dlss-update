# Multiple native overlay draws: v1.25.27

2026-10-01. The wingdrake capture in `prepared-command-rejections-result.md`
rejected two draws sharing one descriptor key. This revision admits a bounded
sequence of distinct, independently qualified draws. It is a candidate for
live verification, not a claim that wingdrake or other reported skips are fixed.

## Native ordering contract

Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Offline image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.

The completed insertion queue is later finalized by RVA `0x23ca8b0`, which calls
`0x23d2370` and replaces the context's queue base with its returned array. The
sort compares unsigned 32-bit keys and preserves insertion order on equality.
Its pairwise stage and both merge paths take the right entry only on `JA`, at
RVAs `0x23d23ea`, `0x23d250e`, and `0x23d2673`. The upper four bytes of each
16-byte queue entry and its packet address do not break ties.

The isolated verifier checks both executable identities, the complete 1,047-byte
sort function and its comparisons, then replays the native sort in owned memory.
Its sole external call is redirected to local CRT memcpy; no game process is
opened. All 395 cases passed, including all-equal keys, conflicting padding and
packet values, descending and unsigned keys, both merge paths, and counts up to
65,534. Whole sorted entries and buffer canaries are checked. This establishes
ordering for this pinned function, not GPU execution or lifetime.

The runtime verifies these two complete native code spans before installation
of its hooks; neither function is hooked or modified:

| RVA | Bytes | SHA-256 |
| --- | --- | --- |
| `0x23d2370` | 1047 | `0cf82d2b8b6259fbb5cad0c7468a780bb7026d9a20f720e9bbd8b5cd501fffc3` |
| `0x23ca8b0` | 206 | `b27d637cadf610c779092c55dc94cda7812d4525fbb864fc26acad0b91620edc` |

## Implementation and bounds

Draw slots are assigned in completed-queue insertion order. The planner sorts
by unsigned key and then by that slot, preserving equal-key order without
sorting by pointer or draw arguments. Each draw retains its original packet,
vertex layout/buffer, shaders, bindings, draw arguments, blend state and target.
Only the already-qualified viewport/scissor coverage changes under the existing
immutable frame selector. State-list packets continue through the native path.

Up to eight total overlay draws remain allowed, irrespective of descriptor
grouping. A ninth refuses the frame. Duplicate packet identities, ambiguous
descriptors, unknown command opcodes, invalid geometry/shaders, and incompatible
inputs still refuse it. Every draw is qualified separately. All existing GPU
recording/submission order, resource-state and Reset/fence checks remain active.
No new hook or process-memory mutation point was added.

Diagnostic branch 5 retains its historical meaning for old captures. New branch
11 identifies a duplicate packet, separately from draw-capacity branch 6.
Additive `shared_key_prepared` and `shared_key_completed` counters record frames
with at least two draws under one key; the latter means all frame commands were
recorded, not that the GPU has completed them. GPU completion and release are
verified separately. The v1.25.27 reviewer requires these counters and checks
their bounds, monotonicity and final equality.

## Offline verification

Evidence: `local/multi-overlay-draws-20261001`.

- Warning-free Release build with `/W4 /WX` after correcting a test-only local
  name collision. Initial build failure is preserved.
- 56 focused native tests and 46 Python tests passed. Historical v1.25.26
  review still passes. A synthetic v1.25.27 schema check passes and rejects
  missing new telemetry; this is not live evidence. The initial synthetic
  check correctly rejected mismatched startup/header versions before correction.
- Native-graph fixtures reproduce `11,0,11,0` with count/first `4/12` then
  `4/0`, deliberately descending packet addresses, per-draw geometry failure,
  unsupported opcode, duplicate identity, eight valid draws and ninth refusal.
- 384 GPU fixture frames passed: WARP multiple-draw reconstructed and native
  branches, RTX 5090 multiple-draw reconstructed branch, and WARP single-draw
  regression. Two overlays use different native blending, padded vertex layouts,
  and rotating vertex resources. Final pixels match the in-order reference
  across all 24 CPU recording orders and split GPU submissions. GPU validation,
  copy extents, untouched native padding, predicate restoration and retirement
  checks pass. These fixtures use captured shaders but do not evaluate NGX.

## Next runtime check

Same main-menu launch, profile and window size; native AA/DLSS/MotionBlur Off.
After arming and a clean save-load baseline, repeat wingdrake travel. Check
shared-key counters, any new command/geometry rejection, image continuity,
recovery, and full cleanup. This removes only the one-draw count restriction:
the captured draws may expose a different unsupported contract.

Gallery's intermediate color/compute pass and the hunter silhouette remain
unresolved. No monster/effect-name whitelist was introduced. Do not interpret
this revision as general support for arbitrary post-processing shaders.

Deployment preserves the exact previous DLL and sidecar in the new run folder.
Keep the current DLL installed after the test. If explicitly requested later,
`deploy.ps1 -Action Restore` restores that exact backup with the game closed;
redirect its sidecar to fresh evidence before launching the restored build.
