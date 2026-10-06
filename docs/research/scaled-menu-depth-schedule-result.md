# v1.25.4: menu skips retain the scheduled depth writer/state interval

2026-09-29, process 34780 / creation 134351249069323154, DLL SHA-256
`6b56d3231565f6495e695ff51a9461fe30305d05374b2a1513533ab744eda79b`.
The exact build, executable and startup identities passed. See
`scaled-menu-depth-schedule.md` for capture scope and limits.

## Captured result

Quiet baseline: 679 DLSS frames, zero runtime faults, no missing depth leases,
one initial stream-readiness skip. The user opened the reproducing menu and
reported the skipping returned. That checkpoint had 1,216 DLSS frames and 832
missing-depth rejections, with zero SDK/runtime faults. The user then closed the
menu and reported normal recovery. The recovery checkpoint had 1,870 DLSS frames
and all 39 suspension periods resumed.

The diagnostic captured 39 observations over the run. Its four-slot retention
and five-second status collection preserved 11 unique observations: one baseline
and ten missing-depth samples. All eleven had stable owner/writer reads, two
equal full planner/position snapshots, a complete scene-depth span chain and
one selected writer preceding reconstruction. This does not establish the same
cause for every rejected frame, nor preserve all 39 sampled observations.

The same interval shape appeared in every retained observation:

- The selected kind-8 outer writer has exactly one schedule position.
- A depth use span points to that writer's nested packet. Its last-use ordinal
  equals the outer writer's schedule index. Nested packets are not separate
  schedule-array entries; their absent direct matches are expected in this
  observed shape and must be qualified through the parent/nested relationship.
- That span transitions native `0xC -> 0x100`. Reconstruction occurs after the
  writer and before the next depth transition.
- The next transition's packet is three schedule entries after reconstruction,
  with native `0x100 -> 0x8`. The tracker's final planned state is `0x8`.
- No depth-relevant API barrier was captured locally at reconstruction.

Example first missing sample: frame/epoch 20280, planner epoch 20281, selected
writer index 5672, reconstruction index 5724, next depth transition index 5727.
Both writer snapshots match the expected packet and destination. The complete
schedule contains 6,494 packets. The normal baseline has the same interval
shape at different schedule indices.

`analyze_schedule.py` checks retained observations without granting admission.
It revalidates both executable hashes through the offline reader and the full
495-byte native converter at RVA `0x259ba50`, SHA-256
`f560506e570fff4dd21df27e5d68db204420e82e3eed1bb5aae2cc3ad1441ee5`.
That converter maps native `0x100` to D3D12 RENDER_TARGET (`0x4`) and native `0x8`
to PIXEL_SHADER_RESOURCE (`0x80`). These native words cannot be cast to D3D flags.
The observation therefore supports a render-target interval at reconstruction,
not the pixel-read state described by the later final tracker field. GPU-state
admission remains unimplemented and explicitly unproven by the JSON reviewer.

## Shutdown and scope

Armed at tick 38138281; intentional stop requested at 38244656. Collector ended
normally at 38246593, 108,312 ms after arming. The menu evidence was saved before
the intentional stop, and the user was told that the stop was intentional.

Final totals: 2,341 successful DLSS replacements/expanded copies, 7,023 downstream
native effect draws, 3,544 route publications built. Dispatch reason 7 contains
1,203 unfiltered dispatches: 1,202 missing depth leases and one stream-readiness
failure. The broader skip counter is 1,204, including one non-dispatch skip;
the capture does not uniquely attribute that extra count. All 39 suspensions
recovered and there were zero faults.

All 2,342 primary GPU signals completed; all 2,341 route recordings completed
and released; all 3,545 jitter changes were restored. Full scale returned and
runtime hooks reported disabled. Read-only memory checks independently verified
all 35 full native code gates and 17 native hook heads restored. COM hook removal
is covered by runtime receipts, not those native byte checks. The full versioned
review passed runtime ownership and diagnostic consistency. It does not imply
artifact-free output, GPU depth admission, or complete transition coverage.

The user confirmed the scene was normal and exited normally. The latest DLL is
kept installed. Native AA/DLSS remain Off; graphics, profile and mode hashes are
unchanged. Evidence: `local/continuous-dlss-20260929/v1.25.4`.

## Next implementation

Implement a qualified consumer-point private depth copy in the scaled path,
using the native scheduled interval instead of requiring the writer's CPU
callback to finish while the temporal request is still queued. Qualify the
outer/nested writer relationship, complete same-epoch span chain and interval,
resource/backing identity, native state conversion and command execution
contract. Preserve identity rechecks and reject ambiguous/changed schedules.

Record the copy and balanced state transitions in reconstruction's own tracked
recording, retain its resources until that recording completes, and prevent the
old post-writer path from duplicating or overwriting it. Preserve admission for
SDK evaluation, recovery and cleanup. Test the pure interval decision and GPU
copy/state lifetime before the next menu reproduction. Do not solve this by
relaxing `live`, copying stale depth, using the final planned state, or waiting
for another CPU recording worker. This diagnostic deployed no such fix.
