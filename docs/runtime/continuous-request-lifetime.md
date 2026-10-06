# Continuous request-lifetime diagnosis (v1.18.3)

The mixed windowed focus/area run ended with fault 6: publication found an already
live TAA entry in the 32-slot frame-modulo ring. The existing trace cannot say
whether this was another producer for the same frame or an older request whose
consumer was delayed, omitted or bypassed. The [failure result](../research/windowed-area-transition-failure.md)
remains a failure. This build observes those alternatives before selecting a
retirement policy; it does not enlarge the ring or discard live records.

The [continuous hook contract](continuous-dlaa.md) and
[camera-history policy](continuous-camera-history.md) still apply. Host version
0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
The native x64 ABI wrappers, hook targets, GPU passes, request ownership and
process-lifetime resource retention are unchanged. Native TAA still forwards
once. Its future bypass is a separate implementation step.

## Bounded evidence

Every continuous status adds `requests` schema 1, policy
`observe-existing-request-lifetime`:

- Publication attempts, successful publications, collisions, and the first
  collision's old/new typed identities: frame, epoch, sequence, projection time,
  packet/state, view/context/layer, depth, bank and async mode. Also record slot,
  live count, active publisher/callback counts and current private GPU lease.
- TAA/depth consumer counts partitioned into publication-blocked, unmatched,
  matched and reentrant calls. Changed packet and multiple-match counts are
  subsets of matched calls, not additional calls.
- First/latest blocked, unmatched and multiple-match examples. These describe
  the packet and caller, live/matching record counts, and newest matching record.
- Each live TAA entry counts matching consumers bypassed by the publishing guard
  and retains the latest such tick, caller and publisher count. A later collision
  carries this evidence with the old record.

Metadata updates use the existing mutex. Blocked consumers now briefly acquire
it to observe typed records, then still return without admission or retirement.
The publisher count is sampled at the guard; the record snapshot is taken after
locking and can be later. Do not infer that they are an atomic view of the engine.
The added locking/scanning can affect timing. No raw packet read is added to the
blocked branch, and addresses are recorded only as identities.

Request telemetry is below 4 KiB, with four additional bounded fields in each
existing TAA record. Status publication retains the existing sparse durable log
and five-second latest-status write. No per-frame disk write, image readback,
new SDK work or unbounded queue is introduced. Fault 6 preserves the occupied
entry and stops admission exactly as before. Changed packets still retire the
selected entry before the existing validation rejects them.

The independent Python reviewer checks counter partitions, sample identity and
ordering, slot arithmetic, bypass evidence and collision/fault agreement. It
classifies same-frame publication, forward slot reuse and backward frame values.
A consistent diagnostic record is not a passing render test: the full reviewer
continues to reject any fault or unclean stop.

## Offline verification

Release /W4 /WX build passed. Nine owned native fixtures exercise the actual
publication helper and consumer callback: retirement/reuse, same-frame conflict,
old-slot wrap, 10,000 publication-blocked calls on another thread, unmatched
packets, multiple generations sharing a packet, changed packets, reentrancy and
depth observations. They do not reproduce the full native producer snapshot or
prove which case happened in the game.

Nineteen Python request checks validate those native records and reject corrupt
records, plus exercise v1.18.3 full-reviewer integration using explicitly synthetic
request metadata over a prior accepted log. Ten existing continuous tests,
17 history reviewer checks, 13 prior full-log checks and 15 startup checks also
passed: 83 targeted checks total. Existing native coverage includes 1,000 scoped
projection callbacks, 1,000 WARP submissions and 100,000 camera poses.

The preparation script verifies unchanged source for the 18 SDK/GPU/binding
files. The earlier 240/600-evaluation GPU evidence is inherited for that path;
it is not a new live validation of this diagnostic build.

## Isolated area-loading test and rollback

Evidence root: `local/dlaa-request-lifetime-20260926/v1.18.3/` (ignored).

1. With the game closed, verify the prior archive, executable/runtime/dependencies
   and 55 decoded native/startup code spans. Preserve the exact destination DLL
   and sidecar, freeze tested sources, deploy and seal the preparation.
2. Launch the usual MO2 Portable test entry in the bare offline profile. Preserve
   Windowed 2560x1440, native DLSS Off/TAA On and DX12 On. Load a save, stay in the
   scene, and reply from the phone. Keep focus and window dimensions unchanged.
3. Verify the startup/provider/process generation, arm, and save an active
   baseline. Only then cue an area change with a loading screen. Save the next
   checkpoint before asking for another action. User reports record the action
   separately from checkpoint labels.
4. If healthy, repeat an area transition with another checkpoint. If admission
   faults, save terminal evidence and review the collision and closure. A normal
   image after a fault can be native fallback and does not prove active DLAA.
5. Stop if still active, verify completed GPU work, restored jitter, zero pending
   callbacks/private lease and disabled hooks. Close normally and run this root's
   `deploy.ps1 -Action Restore`. Restore the exact DLL/sidecar, preserving the
   user's graphics/profile choices, then archive the result.

Focus loss, resizing and display/device recreation are separate tests. This run
does not implement the lifetime fix, native TAA bypass or a performance change.
