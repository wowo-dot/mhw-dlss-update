# Packet generations in the nine-frame recorder — v1.15.1

This corrects the [v1.15.0 packet reuse rejection](../research/dlaa-packet-reuse-result.md).
The nine-frame sampling, resource limits, startup ownership, eleven native
hooks and SDK configuration remain the [v1.15.0 contract](dlaa-history-sequence.md).
Live v1.15.1 produced nine outputs and verified two packet-address reuses.
The full reviewer rejected two supplemental depth consumers without markers;
the ten individual component checks passed. See the
[result and remaining recorder issue](../research/dlaa-nine-frame-result.md).
Normal exit, exact previous-DLL restoration and final archiving are complete.
Evidence/deployment lives under
`local/dlaa-sequence-live-20260925/v1.15.1/`.

## Identity and lifetime

A packet address identifies CPU queue storage, not a permanent request. Each
observed producer/consumer pair retains a separate receipt. The recorder now
logs a producer-begin order before calling the native producer, a publication
order after it returns, and the previous receipt and generation for that
address. These orders share the consumer's atomic order counter.

Reuse is accepted only after the earlier consumer publishes its completed
state with release/acquire synchronization. That receipt must be valid and
returned, with exactly one returned dispatch. Its exit must precede the new
producer-begin order, and the new native frame and epoch must both be later.
The most recent matching publication is consumed once. A second call in that
generation still fails; an unconsumed or active prior use fails, as does an
overlapping producer even when the earlier consumer finishes before publication.
Receipts are never overwritten and every recorded address generation is linked.

This CPU lifetime does not imply GPU completion. The existing per-frame
Reset/Close/Execute records, private fences, allocator retirement, raw copies
and exact typed-conversion checks remain mandatory before SDK evaluation.
No hook target, calling convention, engine layout or native argument changes.
The tested host remains v0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.

The v1.15.1 header explicitly enables `packet_generations`. Its reviewer binds
the consumer's producer-begin value to the CPU receipt and independently
checks the predecessor chain, distinct event orders and completion-before-reuse.
Historical versions keep their original prohibition on repeated packet addresses.
The SDK-free three-frame recorder also retains the original runtime policy.

## Verification and next run

Release /W4 /WX build and 78 focused native tests pass. A WARP fixture recycles
two alternating queue banks through all nine frames, including both depth-copy
and TAA packet storage. All nine independent GPU input sets convert exactly;
GPU validation is clean apart from the fixture's nine explicitly counted
clear-value performance warnings. Separate cases reject duplicate consumption,
unconsumed/active reuse, producer overlap and stale frame/epoch values.

Four groups of ten combined reviewer tests cover current unique/recycled
packets and historical three/nine-frame records, with inapplicable cases skipped.
The 61 historical input tests, 15 SDK receipt tests and seven startup-link tests
pass. The previously accepted live v1.14 capture still passes the current reviewer.
Full SDK artifact review under a synthetic v1.15.1 header checks 99,532,800 RGB
values and rejects a rehashed NaN. The SDK evaluator and isolated fixture are
unchanged; preparation verifies their source/binary hashes and reuses the five
sealed v1.15.0 RTX fixture results rather than claiming a fresh hardware run.

Launch the same barebones entry, DX12 On, DLSS Off, TAA On. Load a save, keep
the camera still and preferably use a stationary pose; reply from the phone.
The agent verifies startup and the exact process, then arms the capture. No
DLSS toggle is needed. Outputs remain private/offscreen. After evidence is
saved, exit normally; the versioned deployment script restores the exact prior
DLL and sidecar, with configuration hashes checked before final archiving.
