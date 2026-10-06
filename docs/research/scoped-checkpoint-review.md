# Stopped-process checkpoint review

Prepared 2026-09-22: bridge **v0.8.1**, recorder **v0.7.1**.
[Run 17772](current-state.md) stopped on tracker state 0x8 before any correction
or Evaluate capture, so this checkpoint revision was not exercised. Game
acceptance remains pending; keep the game closed for offline backing-selection
work, rather than repeating the unchanged test. The preparation below addresses
[run 49944](multi-evaluate-first-result.md), where an
unrelated ResourceBarrier crossed the first Evaluate marker and prevented a
global completed prefix from being accepted.

## Evidence is reviewed after the call finishes

The in-process observer is unchanged. It still captures the original marker,
reserved count, consecutive completed prefix and `in_flight` result without
waiting. Each immutable Evaluate payload retains that original evidence. An
incomplete global snapshot is never rewritten as complete.

The mailbox now also carries a checksummed typed binding: command identity,
Evaluate thread/index and five resource labels. `inputs_verified` requires the
existing caller/handle/boundary, getter, resource-inspection, method and observer
health checks; it does not claim complete global barrier coverage. The original
`qualified` flag retains its stricter meaning. Producer **801** requires the new
reader; each mailbox is **131184 bytes** with the unchanged 131072-byte JSON
bound. The journal remains 753832 bytes and also uses producer 801. Mapping
names/schema labels remain unchanged; producer/size checks reject older layouts.

At a matching debugger exception, all target threads are stopped. The recorder
first saves and flushes the three capture channels and the journal. Its owned
typed copies of completed records allow `review_checkpoint` to reconstruct the
captured list's latest successful Reset generation before each Evaluate marker.
No pointer label is dereferenced, and no foreign call or GPU operation is added.

The review requires a valid, complete stopped journal with no pending/failed
records or capture flags. Call indices and CPU entry/return ordering are checked;
the mailbox index/command must match the checkpoint. Completed selected-list
calls are sorted by entry order. Concurrent selected-list recording, missing
Reset, failed Reset, Close or bundle use in that recording rejects the review.
Post-marker calls cannot supply missing pre-marker evidence.

A call crossing the marker is admitted only when it is:

- A ResourceBarrier on another command list whose complete batch contains only
  ordinary transitions or explicit UAV barriers for other non-null resources.
- A read-only GetCompletedValue query, recorded separately as a query overlap.

Selected-list overlaps, captured-resource overlaps, null/global barriers, split
barriers, aliasing barriers and unclassified operation overlaps remain rejected.
All aliasing overlap is rejected because heap relationships are outside this
capture contract. Microsoft documents that null resources in
[UAV barriers](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_resource_uav_barrier)
can apply broadly, and null endpoints in
[aliasing barriers](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_resource_aliasing_barrier)
do not restrict the barrier to a known resource pair.

## Resource-state limits remain explicit

This review authorizes another **bounded diagnostic observation**, not modern
DLSS rendering. It requires a declared shader-readable depth input, rejects
ambiguous/discontinuous declarations and rejects known incompatible states.
The existing 17 getter and bounded resource-description checks still apply.

Other inputs with no transition declaration remain unknown. In particular,
a UAV barrier is not proof of an output's state. The recorder logs
`declared_state_mask`, `expected_state_mask` and `unknown_state_mask`, using
resource-key bit positions Color=0, MotionVectors=1, PreviousOutput=2, Depth=3,
Output=4. Cross-list CPU recording order is not promoted to GPU execution order.
An accepted checkpoint does not certify resource contents, lifetime, frame
alignment or GPU completion. The full cross-list/fence journal remains available
for separate analysis.

The retry policy retains the same three-correction maximum, consecutive renderer
counters, stable identities, 14 fresh code gates, tracker states 0x800/0x4,
500 ms correction window, two-second journal and capacity/headroom checks.
Each preceding Evaluate must have its own accepted review and increasing marker
and reserved count; future slots must be idle. The one-correction CLI continues
to use its original strict channel gate. No modern vendor SDK is invoked.

## Verification

All **26 CTest tests** pass with warnings as errors. Added tests reject relevant
or unknown overlaps, corrupted typed bindings/qualification bits, incomplete
records, reordered calls, stale capture counts, missing depth declarations,
invalid states and broken recording generations. A test also keeps absent color
state explicitly unknown.

The local replay of the actual 412-call/839-description run 49944 accepts only
its unrelated crossing call 264, reconstructs Reset 215 and nine selected-list
calls, and reports expected-state mask **0xA** (motion/depth) and unknown-state
mask **0x11** (color/output). Changing one resource label in that same overlapping
batch to the captured depth label makes the replay reject it. This tests the
recorded failure directly; proprietary replay data remains under ignored
`local/scoped-checkpoint-20260922/`.

The owned debugger child publishes three captures with incomplete global
prefixes and late-completing unrelated barriers. The parent accepts the scoped
reviews, executes three actual corrected loads, saves all 13 journal calls and
three captures before the fourth fault, forwards that fault normally, detaches,
and retains the evidence after child exit. The one-correction fixture also
passes. WARP and RTX 5090 continuation tests pass; nine D3D12 hook targets are
unchanged. The bridge CFG audit verifies 120 targets and 15 distinct wrappers.

| Artifact | SHA-256 |
| --- | --- |
| Bridge v0.8.1, 685056 bytes | `4F1686B77C686B9C18207EFCDA944BC6ACE0BF92373C921883EC20B69E38EB27` |
| Recorder v0.7.1 | `FC5E52A035ADDD6F5D0CCB346895E4348A9FB79FCDD1FF82F34880377B095FE1` |

The pinned game SHA-256 remains
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`, version
0.0.0.0. System32 D3D12Core remains 10.0.26100.9278, SHA-256
`70710DE807F188A85A3F5AD3A729A41BFA7A92B7621CEEAF38059F5640399267`.
Hook ABI/ownership/target discovery is unchanged from
[the v0.8.0 preparation](multi-evaluate-fence-capture.md).

## Next offline test and rollback

Stage only with the game closed. Preserve the exact active v0.8.0 DLL as
`D:/MO2/MHW/mods/dlssTEST/mhw_dlss_bridge_probe.v0.8.0.dll.disabled` before
replacement. The existing `.mode` sidecar is unchanged: the already-open MO2
still selects `capture-three-evaluates`, with no restart needed. The staging
manifest and source archive are `local/scoped-checkpoint-20260922/staging-081.json`.
DLSS remains Off, with TAA/DX12 On/FullScreen/2560x1440 unchanged. No game binary,
asset or settings change is required for this preparation.

Launch the same MO2 entry and stay at the main menu with DLSS Off until the
**v0.7.1 recorder reports ARMED**. Enable DLSS once, close its popup and leave
Options. Stay at the main menu, report from the phone, and expect another crash
after the bounded test. Do not load a save or toggle twice.

To roll back with the game closed, restore the preserved v0.8.0 DLL to the active
`mhw_dlss_bridge_probe.dll` destination and use its matching archived v0.7.0
recorder. The unchanged sidecar works with both versions. Further rollback to
v0.7.9 is documented in the prior preparation and requires removing/restoring
the sidecar as specified there.
