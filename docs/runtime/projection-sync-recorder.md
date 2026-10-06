# Queue synchronization recorder v0.6.0

Prepared 2026-09-24 from the [queue synchronization audit](../research/queue-synchronization-map.md).
This extends [v0.5.0](projection-submission-recorder.md), whose accepted game
capture is preserved. The completed [run 21900](../research/queue-synchronization-result.md)
records a bounded fence-completion observation; its original and corrected
offline reviews are both archived. The prior DLL has been restored.
The pinned executable hash and existing five startup/39 core gates are unchanged.

## New API hooks and ownership

| Target | SDK slot | Windows x64 captured arguments and return |
| --- | --- | --- |
| Queue Signal | 14 | RCX queue, RDX fence, R8 64-bit value; EAX HRESULT |
| Queue Wait | 15 | RCX queue, RDX fence, R8 64-bit value; EAX HRESULT |
| Fence GetCompletedValue | 8 | RCX fence; RAX 64-bit value |
| Fence Signal | 10 | RCX fence, RDX 64-bit value; EAX HRESULT |

Each wrapper forwards the original once, preserving registers, LastError and
exception propagation. All targets come from owned discovery objects on the
actual device and pass system-method/prefix checks. Device slot 36 creates the
owned fence; discovery performs no submission or signal. Total hooks: 37,
including 12 native targets and 25 APIs. Reset/ResourceBarrier still share
the existing core hooks.

Fences join the bounded COM registry. Exact ID3D12Fence, canonical IUnknown,
owning device and ID3D12Fence1 creation flags are captured. Fence1's canonical
identity must match the base fence. Only supported system methods are called;
partial query outputs are released. The same registry rejects concurrent
first-use initialization rather than waiting across COM. All admitted calls
must drain before list, queue, fence, resource, heap or device pins are released.

The event kind additions are 11 queue Signal, 12 queue Wait, 13 completed-value
read and 14 CPU fence Signal. Queue events store the fence interface in
`resource`, its canonical identity in `backing`, and the 64-bit value in `handle`.
Fence events use `object`/`identity` for the fence, and `handle` for the argument
or returned value. New scalar completion selectors refer to exact event-entry
orders. UINT64_MAX from GetCompletedValue ends qualification as device removal.

## Bounded capture through completion observation

The first selected Execute remains fixed. The journal continues after it,
selects a successful same-queue Signal entering after that Execute returns,
and stops after a native GetCompletedValue return on that fence reaches its
value. Later command-list reuse is recorded without changing the selected
generation; replay distinguishes a new Reset from resubmission of the same
recording. A stop candidate is not an acceptance verdict: failed/ambiguous
signals and competing writers still fail offline qualification.

The original two-second history deadline, 50-ms prefix, 4,096 history calls,
2,048 view events, 65,536 barriers, 4,096 submitted-list entries, 128 lists per
batch, 128 pinned lists, eight queues and 256 resource pins remain. At most 32
fences are retained. Fixed journal arrays occupy 6,793,728 bytes. Capacity or
deadline failure preserves partial evidence and never silently discards older
events. Worker polling enforces the deadline even if no more API calls occur.
The previous game capture used 2,260 history calls before fence observation;
the remaining capacity is a measured starting allowance, not a completion
guarantee for this extension.

The main log uses version 0.6.0 / `projection-sync-readonly`; readiness and
companion use schema 3. Older schema-1/2 captures retain their existing results.

## Evidence fields

- `same_queue_depth_state_declared` replays the immediate previous DIRECT
  Execute and the selected batch prefix, with complete generations, resource
  pin lifetime, non-decaying depth-texture checks and transition continuity.
  Other queues are explicitly outside this declaration's scope.
- `recorded_exact_value_fence_pairs` reports successful Signal/Wait calls with
  matching fence/value and nonoverlapping CPU intervals. Missing/ambiguous
  pairs are reported; this is not a certified dependency graph.
- `selected_queue_fence_completion_observed` requires a native below-target
  baseline, an unshared fence, a successful following signal on the selected
  queue, monotonic nonoverlapping observed writers from that queue, no observed
  CPU/competing writer, and a later native read reaching the value. It covers
  the recorded window, not the fence's entire creation-to-destruction history.

`gpu_completion_verified`, `resource_state_at_gpu_execution_verified`,
`cross_queue_resource_safety_verified`, GPU contents and raster displacement
remain false. These distinctions keep an observed fence threshold from being
used as a blanket safety certificate for resource readback or vendor calls.

## Verification and deployment

Release builds use `/W4 /WX`. Owned-device tests submit their own lists and
exercise graphics/compute Signal/Wait, 64-bit completion values, CPU writers,
shared fences, incomplete traces, later Reset generations and resubmission.
Only fixtures add GPU work or wait for their own fences. Reference balances
and stop/drain are checked. Python replay tests include damaged traces and
synthetic composition with the independent core fixture; this is not a native
end-to-end game capture. Versioned preparation records final test counts.

Archive: `local/projection-binding-20260923/v0.6.0/`. `preparation.json` seals
the tested sources, SDK/map checks and binaries. `deploy.ps1` preserves the
exact existing MO2 DLL and sidecar before installation; `preflight.ps1` verifies
the installed slot, source snapshot and unchanged configuration. `-Action Restore`
verifies the deployed/backup hashes before restoring the DLL and previous
sidecar state. `deployment.json`, `prelaunch.json` and any later `restored.json`
are authoritative for the runtime cycle.

Use the existing `barebonesdlss` MO2 test entry, DX12 On, DLSS Off, TAA On.
Load one save and report readiness. Verify PID/creation, build hash and all
startup gates before arming once. Keep the scene open while the capture saves;
exit normally when asked. No game assets or profile changes are required.

## Offline follow-up after run 21900

The build-time reviewer required all selected-queue operations between the
selected Execute and Signal to have disjoint CPU intervals. The follow-up
permits a successful Wait strictly contained in a later Execute on the same
queue/thread, with both calls strictly after the selected Execute returns and
before the qualifying Signal enters. It reports accepted nested intervals in
`completion_later_execute_nested_waits`. It does not infer order inside the
later batch. Crossing intervals, other nested calls, concurrent threads,
selected-Execute/Signal overlap and all existing fence-writer failures still
reject completion. This changes only offline review, with 83 synchronization
tests passing; the deployed v0.6.0 binary/source snapshot is preserved.
