# Native overlay state inheritance

## v1.25.22 live result, 2026-09-30

The run in `local/copy-recording-20260930` used the verified executable
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`,
DX12, RTX 5090 (LUID 0:93877), 1280x720 scene rendering and 2560x1440 output.
The barebonesdlss profile retained Stracker's Loader. Native AA and DLSS stayed
Off. Process 17396, creation 134352731393638593, loaded DLL
`1e4204f95fd592a2ecacae414b7fd4363f152b639c870bedd92d891c9c0a75f8`.

Save loading and the first quest-abandon return passed. The previous copy-stage
fault did not recur. This is one successful exercise, not universal coverage.
The user saw no rectangle during the cutscene, but telemetry showed overlay
admission rejection and paused DLSS evaluation. That visual report is therefore
not proof of a fixed cutscene. DLSS recovered afterward, with 3,071 completed
overlay draws across the run and 2,924 rejected overlay plans.

The run stopped itself after 12,965 DLSS frames. The collector first observed
runtime fault 45 / stream fault 6 at 23:31:14 local time. The native draw failure
was earlier at tick 2529765, frame/epoch 48934, effect stage 4. Its mask was 2:
the plan had no known local output state. Output resource, RTV, PSO and root
signature identities all matched. The user reported standing still in Castle
Schrade, without a nova, movement or menu transition. No intentional stop was
requested and the one-hour limit was not reached.

All 15,967 private GPU signals and recordings completed and released; all
17,485 jitter changes were restored. Full scale was restored. Read-only live
checks verified 38 complete native function spans and 18 hook heads. Two
downstream recordings remained retained until process exit. The full reviewer
correctly rejects this run as an unsuccessful stop. Game exit and final archive
sealing are tracked separately in the local evidence; do not infer them here.

The old periodic status file remains active after this fault. The last native
`continuous` row followed by the `stopped` event is authoritative.

## Candidate correction

Private replacement effects still require an observed local render-target
state. A qualified in-place overlay preserves the native draw, target,
resources, shaders, bindings and native barriers. Only viewport and scissor
coverage change within the same full-size target. Its original state can
therefore remain inherited from an earlier command list. This does not mark
that state as locally known or infer it from the RTV binding.

v1.25.23 permits that unchanged native overlay contract when the local state
is unknown. It still rejects a known contradictory state, mismatched resource
or RTV, changed PSO/root signature, invalid geometry, input aliasing, broken
frame/submission order, and retention failure. The relaxation applies only to
the existing qualified overlay path. There are no additional hooks, inferred
cross-list state values, or new barriers.

An offline reproduction places motion and its overlay on different command
lists, with the former leaving the target in render-target state and the latter
having no redundant local transition. The old code rejects it with the same
draw-order/state failure. The candidate passes 96 frames across all 24 CPU
recording orders and both single and batched GPU submissions. Native-choice
and reconstructed-choice variants are tested. Contradictory state and RTV
negative cases reject before recording. This demonstrates the rejected valid
case; the exact preceding GPU state history of the live failure was not logged.

Evidence is in `local/overlay-inherited-20260930`. The candidate was prepared
while v1.25.22 remained installed. The first v1.25.23 live result is below.

## Cutscene qualification diagnostics

The existing bounded graph samples now optionally include `qualification`.
High 16 bits identify the effect stage; low 16 bits identify the failed check:

| Code | Check |
| --- | --- |
| 1 | Draw packet and vertex arguments |
| 2 | Native target attachment, format and ownership |
| 3 | Vertex stride, offset and CPU arena |
| 4 | Bounded vertex read |
| 5 | Vertex coverage/expected geometry |
| 6 | Rectangle, pipeline, shaders and input layout |
| 7 | Unexpected additional shader stages |
| 8 | Overlay vertex-shader identity and pixel-shader eligibility |
| 9 | Recipe signature |
| 10 | Overlay predecessor/output relationship |
| 11 | Overlay input bindings or reconstructed-color alias |

Zero means no finer classification is available. These assignments do not
change admission or perform additional process reads. Graph sampling remains
bounded to the first four rejections and the latest one. Parser tests reject
invalid phase/stage combinations and nonzero accepted qualifications.

The live read-only helper saved one stable GodRays recipe after a cutscene
rejection interval. It is not a captured rejected vignette recipe. An earlier
helper accepted empty command sets; that incomplete evidence is explicitly
marked unusable. The last reader began after the terminal fault and obtained
no valid overlay recipe. No nova checkpoint was completed before the fault.

## v1.25.23 first live result, 2026-10-01 local time

Process 40428, creation 134352781643765871, loaded the candidate DLL
`91d6113e634cd99a6b078e055f3e2a65f0bfae88ca3b14c100b772879797cc82`.
The executable, adapter, render/output extents and settings matched the
preceding run. Stracker's Loader remained enabled. Save loading and Castle
Schrade idle/walking passed the user's visual checks. The previous fatal
overlay-state rejection did not recur during this run; broader coverage remains
unproven.

The user ended the test. The normal stop event completed with 9,384 DLSS
evaluations, zero runtime/stream faults, and 5,855 completed native overlay
draws. All 12,596 private GPU signals and recordings completed and released;
all 9,962 downstream recording references were released. All 13,277 jitter
changes and full resolution were restored. Read-only live checks verified
38 complete native function spans and 18 hook heads before game exit. The
frozen full reviewer accepted telemetry accounting and cleanup. The game was
subsequently observed absent; its exact exit method was not separately confirmed.
The current DLL remains installed, with all four configuration hashes unchanged.

Cutscene rendering is still incomplete. Both replays suspended DLSS at overlay
qualification 327683: effect stage 5, check 3 (vertex stride, offset, or CPU
arena). There were 3,203 rejected overlay plans, with successful DLSS recovery
after the cutscenes. The telemetry acceptance does not establish a cutscene fix.
The cumulative skip counter also includes events outside matched resolve
fallbacks; it must not be presented as a count of visibly unfiltered frames.

Three bounded read-only capture attempts produced zero stable double snapshots.
The first two usually missed the required primary view or completed draw queue;
the final queue-focused reader ran after the overlay ended. No vertex layout was
captured, and the exact failed operand is not known. The next diagnostic should
record the three already-read operands at check 3, or obtain a stable bounded
queue capture. Do not broaden geometry admission based only on this check code.

The local final result, reviewer, collector timeline, native restoration checks,
and configuration verification are preserved under the evidence directory.
