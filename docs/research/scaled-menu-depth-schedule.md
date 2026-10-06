# v1.25.4: depth schedule at reconstruction

The v1.25.3 menu run found four selected depth writers whose callbacks arrived
after their temporal request had been consumed. The writer and request matched
on every selection predicate except `live`. CPU recording order did not prove
GPU execution order. See `scaled-menu-depth-result.md` for the observations and
limits; no correction is deployed by this revision.

This build observes the native resource schedule at reconstruction, alongside
the existing missing-depth-copy check. The purpose is to establish the selected
writer's scheduled position, the scene-depth state at reconstruction, and the
identity/lifetime contract required to record the private copy there. The final
state stored in a resource tracker can belong to a later effect and must never
be treated as the current state merely because it is readable.

## Scope and identity

Host version `0.0.0.0`, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decrypted analysis image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Preparation revalidates all 35 existing native code gates. Existing hook targets,
Windows x64 callback conventions, COM signatures and return handling remain as
documented in `scaled-menu-depth-admission.md` and continuous scaled output.
The hook count remains 48. No additional hook or GPU command is introduced.

Only the scaled build adds this observation. The native producer retains the
already-selected depth packet's object and value snapshot along with its
sequence. Selection predicates and ownership are unchanged. At the existing
dispatch lease check, under the runtime mutex, a sampled observation reads the
depth owner and selected writer before/after two planner captures. Packet
pointers are read through the existing guarded reader, never called or written.
The diagnostic does not pin native packets, retain new COM references, relax
admission, or authorize GPU work from captured data.

`DepthScheduleTrace` stores one successful-lease baseline, the first two misses,
and the latest miss at most once per second after the preceding missing sample.
The large snapshots and second-read scratch objects are in fixed global storage.
There are no per-frame allocations, GPU readbacks or new file writes inside the
callback. Bounded guarded reads still cost CPU time and can perturb callback
timing; a sampled success or absence of a failure is not itself proof of a fix.

## Diagnostic schema

`depth_schedule` schema 1, policy `depth-scheduled-consumer-observation-v1`,
`diagnostic_only: true`, capacity 4, interval 1000 ms. Counters `checks` and
`missing` exactly cover existing depth lease checks and rejections. `captured`
counts sampled observations, including replacements of the last slot.

Each sample has:

- `slot` (0 baseline, 1/2 first misses, 3 latest), monotonic `ordinal`, and
  `at = [tick, frame, epoch, temporal_sequence, selected_depth_sequence,
  command_list, recording_generation, temporal_packet, selected_writer]`.
- `owners`: `[object, backing, resource, width, height, gpu_format]` for
  **scene depth, color, temporal output**, in that order. These roles explicitly
  remap the shared planner reader's usual color/output/history inputs. Scene
  depth is the separately validated depth attachment, not the native TAA's
  possible 1x1 fallback binding.
- `expected_writer` and `actual_writer`: `[opcode, kind, source, destination,
  nested, rectangle0..5]`; `owner_stable` and `writer_stable` cover both reads.
- `plan`: the existing `NativeStateTrace` schema, with up to 32,768 scheduled
  packets, 16 state spans per role, and 32 preceding packets with at most 16
  relevant barrier nodes each. Renderer schedule fields are epoch `+0x1f208`,
  packet array `+0x7af18`, count `+0x7af28`; backing `+0x70` identifies its tracker.
- `positions`: `[match_count, first_index]` for the writer and all 3x16 tracker
  span packet addresses; zero matches use `UINT_MAX` for the index. Includes
  bounded scan completion and schedule-header recheck flags.
- `plan_equal` and `positions_equal` compare both full observations. They show
  repeatability, not an atomic snapshot or GPU completion.
- Existing captured API barrier count and relevant count, retaining at most 16
  depth transitions, depth/global alias barriers and depth/global UAV barriers
  as `[type, flags, subresource, before, after]`.

Incomplete, changed, late, ambiguous and unreadable observations remain useful
diagnostic records. The reviewer verifies schema bounds, counter coverage,
immutable baseline/first misses, latest-sample cadence and monotonicity. It may
report a stable selected writer preceding the temporal packet in the schedule;
it always reports `gpu_depth_admission_proven: false`. Establishing a usable
current-state interval and execution/lifetime contract requires analysis of the
captured schedule and native execution path, beyond this serialization review.

## Verification and next runtime check

Both DLL targets build with project warnings treated as errors. All 45 selected
native tests and 86 Python tests passed. Native schedule fixtures cover a
writer-before-consumer interval whose current state differs from a future final
state, duplicate/late/missing writers, changed owner/writer, unstable schedule,
oversized schedule and bounded sampling. Python checks consume the actual native
JSON output. Synthetic full reviews cover v1.25.0 through v1.25.4 and reject a
missing new channel. The preparation receipt also replays the sealed v1.25.3
19,779-frame log. Existing GPU route/shader evidence is inherited only for
byte-identical sources; no new GPU fixture run or live result is implied.

Use the same MO2 entry and bare profile; leave native AA/DLSS Off and window
size unchanged. Load a save and establish a quiet armed baseline. Open the menu
that reproduces the skips and leave it open long enough for a sampled missing
lease. Save and inspect the schedule checkpoint, close the menu to confirm
recovery, then stop intentionally. Verify GPU closure, full render scale and
native hook bytes before normal exit. Keep this DLL installed as requested.

Evidence: `local/continuous-dlss-20260929/v1.25.4`. Deployment preserves the exact
previous MO2 DLL and output sidecar there. With the game closed,
`deploy.ps1 -Action Restore` restores that slot and sidecar if requested. It does
not modify graphics settings, the game executable or original assets.
