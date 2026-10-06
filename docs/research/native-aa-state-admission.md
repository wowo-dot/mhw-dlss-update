# Current-use state admission, v1.21.3

Prepared 2026-09-27 after the [v1.21.2 capture](native-aa-state-provenance-result.md).
This changes admission for the two captured missing-transition patterns. It is
not yet live-verified and does not claim whole-renderer AA exclusivity.

## State contract

The existing API-local `state_of` path remains first and unchanged. If color or
output fails that check, the new path may supplement a missing transition. It
never overrides a conflicting transition, split barrier, subresource transition
or aliasing barrier. Motion admission remains the existing API-local check.

The supplement relies on the pinned game's native planning/execution contract,
not an independent GPU-state query. Native compute consumer `259CC20` consumes
its planned state nodes via `259BA50`, flushes the translated barriers before
Dispatch, then dispatches the exact verified TAA shader. We replace that same
dispatch using its required resource states and restore those states afterward.
The game remains responsible for ordering its scheduled commands. The mod does
not introduce a new queue ordering model or infer ordering from CPU wall time.

At that dispatch, the supplement requires:

- Exact current packet, binding and owned resource identities, and a tracked,
  open command-list generation matching the binding observer. These existing
  checks still precede the SDK path.
- A unique current packet in the bounded schedule, renderer/tracker epoch equal
  to the request epoch +1 (the captured convention), complete predecessor data,
  complete doubly linked use spans and consistent before/after chains.
- Output's current span is exactly `0x400 -> 0x400`, linked to this packet, with
  one matching native node and one observed resource-specific API UAV barrier.
  Its state comes from the qualified native use contract; the UAV barrier alone
  grants no state authority.
- Either output's initial planned state is `0x400` with this as its first use,
  or an earlier compute packet two schedule entries before the consumer has a
  linked `0 -> 0x400` initialization. Intervening output state nodes reject.
- If color has no local transition, that earlier compute packet must contain
  the linked `0x100 -> 4` transition, with its read span extending exactly to
  this consumer. Intervening color state nodes reject.
- A second complete snapshot equal to the first, reread native node/backing
  identities (including the output node's current-use span), and unchanged
  color/output owners. The current packet is checked again after admission.

Native words above are not D3D12 flags. Final tracker state is deliberately
ignored: it describes later work. The two-copy comparison is observational
consistency, not a lock on game memory. The parent consumer owns the packet
while we inspect it; arena and tracker addresses are read only during that
callback. No native pointer becomes a cached state authority. Resource COM pins
retain the D3D textures, not the game's tracker objects. Changed/unreadable or
unrecognized records leave the original rendering path in place.

Existing pinned executable SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Version 0.0.0.0. Existing consumer, converter, scheduler and constructor code
hash gates remain required. There are no additional hooks or game function
calls. Calling conventions, request retirement, scope cleanup, SDK error policy,
GPU pin retirement and state restoration are unchanged.

## Diagnostic receipts

Header policy `state_admission=native-current-use-v1` identifies this path.
`state_admission` reports attempts, accepted/rejected proofs and the first eight
plus latest local-state evidence samples. Results 1–6 describe the replayable
local/shape/schedule/tracker/output/color rejection; 7 means fresh identity or
stability checks failed. Zero requires both a valid shape and the fresh checks.

The Python reviewer independently replays the retained shape decisions and
checks partitions and immutable first receipts. Later live rereads are reported
runtime receipts, not independently reconstructible from the first snapshot.
Proof acceptance does not promise SDK evaluation: later depth, shader and
history eligibility checks still apply. Unmatched native work remains visible;
`exclusive_mode` stays false. The existing rejection trace counts actual
rejections, separately from these supplement attempts.

## Fence snapshot correction

`UnfilteredLifetime::write` samples the fence once, collects against that value
when healthy, and uses the same value for all reported pin counters. On fault it
preserves every pin. The GPU can progress during serialization without mixing
two different completion observations into one receipt.

The WARP regression holds GPU execution behind a gate, resets one recording,
and releases the gate exactly between serialization of the completion value
and the recording counters. Both still describe the earlier completion sample;
a later collection releases only the reset/completed recording. A fault-mode
snapshot confirms completed pins remain retained.

## Verification and runtime plan

Release build passes /W4 /WX. 40 native tests and 138 Python checks pass. The
native admission fixture exercises actual bounded readers against owned mock
renderer memory, including both accepted shapes, ignored future final state,
aliases, flagged/split/subresource barriers, wrong states, duplicate packets,
stale epochs, cyclic/broken spans, ended color use, wrong span links, replacement
backings, metadata changing on reread and unreadable memory. Independent replay
accepts all four saved v1.21.2 samples. These are CPU admission tests and WARP
copy/lifetime tests, not live DLAA validation of the new supplement.

Run the same bare MO2 entry with native AA/DLSS Off, DX12, Windowed 2560x1440.
After save load, inspect all nine checkpoint channels, native admission receipts
and actual SDK evaluations. Repeat one area load to exercise the second pair.
If the new proof rejects, preserve its reason rather than loosening the check
in a running game. Announce the intentional stop, verify complete drain and
normal exit, and leave the installed diagnostic in place at the user's request.

The v1.21.2 recovered depth-lease suspension remains an unresolved observation;
this change does not claim to repair its trigger. CMFM/UI, resolution-changing
DLSS and frame generation remain deferred.

Evidence root: `local/native-aa-state-admission-20260927/v1.21.3/`.
The previous slot and sidecar are backed up before installation. If explicitly
requested, `deploy.ps1 -Action Restore` restores that exact v1.21.2 DLL/sidecar
while the game is closed. Do not invoke it automatically after testing.
