# Depth destination correction live result, v1.21.5

Closed and archived 2026-09-27. The narrower destination comparison accepted
all 66 attachment-only changes observed in this run, with zero actual depth
destination rejections. The user reported DLAA remained active and clean after
an area change through a loading screen followed by walking/panning.

The [frozen contract](native-aa-depth-destination.md) explains the comparison.
Its statement that live behavior was untested describes preparation time;
this document records the subsequent live result without changing that seal.

## Identity and verification

- PID 44500, creation FILETIME 134350008543287544; D3D12, Windowed 2560x1440,
  swap-chain format 87, adapter LUID [0, 98547], native AA/DLSS Off.
- Host version 0.0.0.0, SHA-256
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- Installed DLL SHA-256
  `9031276806f2c175fd52c6c8fdfffaa0a855f411f0df4e05ef8072a425e35f7a`.
- Root `local/native-aa-depth-destination-20260927/v1.21.5/`.
- 387-file prelaunch seal SHA-256
  `c7f416aa3ac2239cf6da32e256e44ab7a859e74b08089eb3443dcf251aedf22c`.
- 406-file final archive SHA-256
  `9f349ef3d86566f7df4ae0a4c9b4bfcdfab892cb9828641d0de563730194de0c`.
- Final log SHA-256
  `a9b18c8849fda6037bce5934016ab0119301fd7d10ef3bab9b56cf1e41e9eab3`.

Process generation, host, deployed module, modern runtime, startup capabilities,
saved configurations and all sealed files were verified before one arm request.
The prepared build had passed 42 native and 163 Python checks. All ten live
checkpoint channels passed, including the new destination identity review.
The frozen full-log reviewer accepted the final capture.

| Checkpoint | DLAA evaluations | Attachment-only admissions | Destination rejections | Missing-copy rejections |
| --- | ---: | ---: | ---: | ---: |
| Baseline | 2,018 | 5 | 0 | 0 |
| After area load | 5,368 | 46 | 0 | 87 |
| Intentional stop | 6,521 | 66 | 0 | 87 |

The arm-to-stop request interval was 84.219 seconds. There were zero runtime
faults, SDK failures, request collisions or actual state rejections. All four
native current-use state supplements accepted. All 30 reconstruction suspension
periods resumed; no waiting state or failure latch remained at stop.

## What the correction established

All 6,608 destination checks passed. The retained whole-scene diagnostic still
reports 66 mismatches, exclusively in attachment, attachment owner, attachment
resource and attachment readability. These are old-comparison misses, not actual
destination rejections. Every other field difference counter remained zero.

The first eight and latest retained samples show those attachment fields going
from unavailable to available between publication and consumption, with exact
container/full texture Owner/SRV/descriptor/readability agreement. The matching
old diagnostic samples have 24 reads and no read failures. The new retained
samples replay the allowed field mask, and both channels agree on identities.

This confirms the comparison change admitted the observed attachment changes
without weakening the remaining checks. A destination admission is not an
individual SDK evaluation receipt: later checks still apply, and bounded
samples do not establish that all 66 admissions reached Evaluate.

## Remaining missing-copy condition

All 87 unfiltered frames failed the unchanged lease check. Difference counters
are [87, 87, 87, 87, 0]: command list, generation, frame and available-depth
predicates failed; already-evaluated never failed. All nine retained actual
lease snapshots are [0, 0, 0, 0, 0]. Their first/latest times are approximately
41.735 and 57.500 seconds after arming.

These appeared between baseline and the area-load checkpoint; the count stayed
87 while a further 1,153 DLAA evaluations completed. This establishes recovery,
not exactly which game transition caused each missing copy. The earlier copy
production/reset path still needs to distinguish legitimately unavailable
stages from a tracking or ordering gap. Do not relax the lease gate or reuse
depth from another frame to hide this condition.

One unclaimed native resolve appears in the final status (zero at both earlier
checkpoints), with no tracked forwarded request. Its precise cause/timing is
not separately recorded; it must not be presented as zero native resolves or
proof of whole-renderer AA exclusivity. `exclusive_mode` remains false.

## Closure and scope

The assistant announced and requested intentional stop. All 6,522 SDK/depth
recordings and 87 unfiltered-copy recordings were submitted, completed, retired
and released. Both trackers had zero live references or pending submission,
reset or GPU work. All 7,018 jitter scopes restored; hooks disabled, pending
requests zero, no pending private recording. The user confirmed normal exit
and process absence was independently verified.

The DLL remains installed as requested. Its exact v1.21.4 predecessor and old
sidecar remain backed up; all four saved configurations stayed unchanged.
No original executable or game assets were changed. The finalizer reverified
334 frozen source identities, dependencies, deployment, logs and checkpoints
before sealing the archive.

This short run covers a reported area transition and camera/character motion.
It adds no pixel readback, controlled focus/resize or extended soak coverage.
Next, trace the earlier private depth-copy lifecycle for empty leases. CMFM/UI,
resolution-changing DLSS and frame generation remain outside this correction.
