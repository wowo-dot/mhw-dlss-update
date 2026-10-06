# Depth eligibility diagnostic, v1.21.4

Prepared 2026-09-27 after the [v1.21.3 admission result](native-aa-state-admission-result.md).
That run had 412 unfiltered frames at the grouped scene-depth check (reason 3)
and 31 at the depth-copy lease check (reason 7). Existing logs cannot identify
the individual failed predicates. This build adds bounded observations at those
two existing checks; it does not relax either check or repair their cause.

## Observations and hypotheses

`SceneDepth` default equality includes the destination texture's full `Owner`,
the scene container, SRV/CPU-descriptor identities, current source attachment,
its owner/resource and two readability flags. `scene_depth` reads the attachment
as a diagnostic observation separately from the destination texture. The actual
private depth copy is recorded earlier from the verified native CopyResource
destination and is retained by list generation/frame until resolve/submission.

A diagnostic attachment or descriptor changing could therefore reject an
otherwise unchanged depth destination. That is a hypothesis until live field
receipts show it. A real destination change, unavailable texture or stale copy
also remains possible. The old aggregate trace cannot choose among them.

## Runtime contract

The hook set is unchanged. The observer runs inside the existing
`ID3D12GraphicsCommandList::Dispatch` hook (`WINAPI`, Windows x64 ABI), reached
from the hash-gated native compute consumer at return RVA `0x259CFCA`. The same
tracked open list/generation, shader, packet, resource pins and runtime mutex
remain required. It invokes the same `scene_depth` routine once, through a
wrapper that delegates each read in the original order and observes only its
success/failure. It does not add game-memory reads, call game code or invoke COM.

The original decision is still `readable && depth == consumer.depth`. The lease
decision still requires exact list, generation, frame, available depth and not
already evaluated. Logging neither grants admission nor mutates native memory.
There are no new GPU commands, hook targets, resource pins, queue changes,
settings writes or SDK mode changes. Existing error/reconstruction and native
current-use admission policies are unchanged. Timing has small diagnostic
overhead; this is not a performance benchmark.

Native memory is read only within the existing consumer callback. Copied
addresses are diagnostic identities, never a cached authority or later
dereference. The original resource ownership is unchanged. Tested host is
version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
No signatures/offsets were newly derived, and all existing executable/code gates
remain required. Calling conventions and hook-removal contracts are unchanged.

## Schema

Header policy `depth_eligibility=depth-eligibility-observation-v1` requires the
tenth checkpoint channel, `depth_eligibility_verified`. The `depth_eligibility`
object is schema 1 with `diagnostic_only=true`.

Scene counters partition checks into passes, failed core reads/validation, and
readable-but-unequal snapshots. All 21 compared fields have individual change
counts. Only readable snapshots receive a difference mask; partial reads must
not be interpreted as actual field changes. Each rejected sample stores:

- Original frame/epoch/request sequence, packet, list/generation, view/context,
  bank/async mode and callback tick.
- Expected and observed scene fields in the order specified by `FIELDS` in
  `tools/continuous_depth.py` (six pointer identities, thirteen Owner members,
  then attachment/SRV readability).
- Read count, failed read count and first failed read's ordinal/address/size.
  Zero failed reads with a false result can indicate pointer/shape validation;
  it does not prove an OS read failed. Optional read failure can still return a
  readable scene and produce an equality mismatch through the affected fields.

Lease counters partition checks into passes and rejections. Five individual
predicate counts and the retained actual lease values distinguish list,
generation, frame, absent depth and already-evaluated state. An empty lease may
fail several predicates simultaneously. It does not alone explain why the
earlier copy callback did not produce a lease.

Each stage stores its first eight rejected samples and latest sample. Counters
continue beyond that; no per-frame file I/O or allocation is introduced. The
reviewer independently replays masks, partitions, bounded retention, read
receipt shape, stable first samples, monotonic counters and exact correspondence
to dispatch reasons 3/7. It does not independently reread the game's memory.

## Verification and next run

Release build passes /W4 /WX with no new project warning. 41 native and 151
Python checks pass. The new native fixture has 18 serialized cases: unchanged
depth; metadata versus texture mutations; optional/required failed reads; invalid
container; each lease predicate, empty/valid lease; and 1,000 failures verifying
bounded first/latest retention. All 21 SceneDepth fields are mapped distinctly.
The wrapper is compared with the original reader for equal returned values,
read count and rejection behavior. Python tests reject corrupt/missing receipts
and require this channel in the new-version full reviewer. Historical reviewers
remain compatible. These are offline tests, not live validation of v1.21.4.

Launch the same bare MO2 forced-DLL entry, native AA/DLSS Off, DX12, Windowed
2560x1440. After save load, arm once and capture a baseline with all ten channels.
Inspect field/predicate differences before requesting an area change through a
loading screen and walking/panning. If a suspension is observed, preserve its
actual counters; do not infer healthy DLAA from a visually clean scene alone.
Announce intentional stop, verify full drain, request normal exit and archive.

Evidence root: `local/native-aa-depth-eligibility-20260927/v1.21.4/`.
Back up the installed v1.21.3 DLL and output sidecar before deployment. Keep the
diagnostic installed after testing as requested; invoke `deploy.ps1 -Action
Restore` only if the user requests restoration with the game closed. All four
saved configurations remain unchanged. No new run is armed by preparation.
