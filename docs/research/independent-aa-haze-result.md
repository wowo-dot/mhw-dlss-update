# Independent AA-Off DLAA: haze graph passes; sauna exposes barrier check

2026-09-27, v0.8.2. **Partial run: native haze queue insertion, assigned
endpoints and restoration verified; continuous rendering stopped at fault 19.**
The user identified entering the sauna as a reproducible trigger and reports
that the earlier TAA-dependent implementation did not fail there. This is a
useful route comparison, not a captured A/B test or proof of the failed barrier
subcondition. No area-load or haze-exit pass is claimed.

## Evidence

PID 52576, creation 134349313504396003. Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
DLL SHA-256
`00acf00cbde7e28913be1d5412e77a214d41976faa99bc09ca89f97d1ee25279`.
Native AA Off, native DLSS Off, Windowed 2560x1440, DX12, bare offline profile.
Session `local/independent-aa-queue-layout-20260927/v0.8.2/`.
All 414 prelaunch files were verified before the arm request (tick 42128156).
Startup and installation passed. Sources/tests/tools remained frozen through
capture and closure.

`capture-52576.jsonl`: 59,885 bytes, SHA-256
`840c9f746b2da8f2ac2047ffdcb111cb0cd0b4ac1e9ace7c640adbdc78cb1c22`.
Frozen strict `review-52576.json` rejects full acceptance (`healthy stop`).
Separate `observations-52576.json` verifies only the partial findings. It calls
the frozen independent graph reviewer on the first complete haze snapshot.

There were 2,883 base-layout evaluations and 23 haze-layout evaluations, totaling
2,906 successful SDK evaluations and native handoffs. All 2,907 admitted native
queues returned and all 2,907 markers were consumed. The first haze frame is
44277; the last successful SDK frame is 44299. Haze entered once and never
returned to the base layout before failure.

The first haze snapshot passes independently reconstructed endpoint checks at
callback, return and restoration, including exact original descriptor ordering.
The final native copy correctly reads scratch 0 with haze present. There is no
queue rejection and no failed native return. This verifies the observed haze
layout, not full continuous operation or its disappearance transition.

## Remaining failure

Fault 19 is in `stream_runtime.cpp::barrier_after`, after marker consumption
and before SDK evaluation. It requires the first relevant barrier batch to
contain exactly two relevant transitions: color and motion, both whole-resource
RENDER_TARGET to PIXEL_SHADER_RESOURCE with no flags. All three input states
must also be known, issue-free and exactly PIXEL_SHADER_RESOURCE afterward.

This capture does not record which predicate failed. Possible differences in
batch composition, transition timing, or tracked state remain hypotheses. Do
not relax the state check, claim an SDK failure, or blame haze output ownership
without capturing the actual barriers and input states. The next investigation
should compare the independent insertion point with the earlier TAA route and
preserve a bounded first-failure barrier/state snapshot for the sauna trigger.

## Closure

All 2,906 issued private GPU signals completed; all 2,907 scoped jitter changes
were restored. All hooks are disabled, descriptors detached, pending callbacks
zero and no private recording remains pending. Two slots stayed pinned after
the fault; one consumed marker had not reached evaluation/submission. The
awaiting GPU/Reset/submission counters each report one for that unsubmitted
record. These counters do not mean an issued fence failed to complete, and
this is not a full retirement pass. SDK/list references remained pinned until
process exit. No SDK/pass error string was recorded.

History resets: 92 (one initial, 91 camera-distance). History policy was unchanged
and remains a separate quality-review item.

The user exited normally. The sealed helper verified process absence, restored
the exact previous DLL (`689bef7c...`) at 2026-09-27T00:27:52.199580+03:00,
removed the matching sidecar and confirmed all four configurations unchanged.

