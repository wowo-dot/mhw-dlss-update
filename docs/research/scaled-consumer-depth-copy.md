# v1.25.5: private depth copy at the DLSS consumer

Prepared 2026-09-29 after v1.25.4 reproduced menu skips and recovery. This
revision removes the scaled path's dependency on the depth writer finishing
CPU recording while the temporal request is still queued. It records the
private depth copy in reconstruction's own recording. Live menu verification
is pending; the GPU fixtures do not establish the game's live behavior.

## Native contract and limits

Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded analysis image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
The existing hook targets, Windows x64 adapters, COM signatures and ownership
rules in `continuous-scaled-output.md` remain. Hook count remains 48. The
additional full-function gate is the 495-byte converter at RVA 0x259ba50,
SHA-256 `f560506e570fff4dd21df27e5d68db204420e82e3eed1bb5aae2cc3ad1441ee5`.
There are now 36 code gates. No new hook is installed.

The native planner at 0x229cee0 builds the resource-use spans and scheduled
packet array. The kind-8 writer's scaled rectangle is smaller than its backing
allocation, selecting the nested draw path at 0x259df30/0x259d670. The destination
is scene depth, R32_FLOAT with ALLOW_RENDER_TARGET. Its selected span references
the nested packet; its last-use ordinal identifies the outer writer in the
schedule. Native state 0x100 converts to D3D12 RENDER_TARGET (4), and native
state 8 converts to PIXEL_SHADER_RESOURCE (128).

The planner optimization at 0x229eb22 can move a later transition's node list
onto an earlier packet. It requires the preceding span's last-use ordinal to
precede the hoist target. Consequently the following depth 0x100-to-8 transition
may already have executed before reconstruction even though its span's packet
occurs afterward. The old diagnostic's preceding-32 window cannot exclude this.
The new admission scans every packet from the selected writer through the
consumer, including the nested state lists of opcode-10 packets.

Like `native-aa-state-admission.md`, this admission relies on the pinned game's
native planning/execution contract: the game orders its own scheduled resource
uses, including work recorded by different CPU workers. It does not independently
measure native cross-queue GPU execution or introduce an ordering model for native
work. CPU callback timing and the tracker's final state grant no admission.
The inserted copy restores the native resource's expected state before the native
sequence continues. Private work retains the existing DIRECT-queue fence ordering.

## Admission and ownership

`depth_consumer_admission.hpp` performs two fresh, equal captures under the
runtime mutex. Each checks:

- The selected writer snapshot, current scene-depth owner/backing/resource,
  backing dimensions, scaled rectangle and nested viewport.
- Same planner/tracker epoch, a complete linked span chain, native writer
  transition 12-to-0x100 and next transition 0x100-to-8.
- Exactly one outer writer, consumer and next packet in the schedule. The
  nested writer is not a separate schedule entry. Writer precedes consumer;
  consumer precedes the next use. Writer last-use ordinal equals writer index.
- All relevant state nodes in the interval. Exactly one matching writer
  transition is required. Zero later depth transitions selects state 4;
  exactly one matching hoisted transition selects state 128. Unexpected,
  duplicated or conflicting transitions are rejected.
- Fresh headers and identities; local API barriers must not contradict the
  derived state. Relevant/global alias or UAV barriers, split transitions
  and per-subresource transitions are rejected.

Bounds: 32,768 schedule entries, 16 complete depth spans, 256 entries between
writer and consumer, 128 between consumer and next use, 128 state nodes per
packet chain, and 16 relevant depth nodes. Fixed global scratch storage avoids
worker-stack growth and per-frame allocation. These bounded reads still cost
CPU time; this revision makes no performance claim.

The current temporal request already retains color, motion, output and scene
depth. The copy additionally requires the pinned depth COM resource and private
buffer descriptions to match the validated full allocation. It retains all four
inputs against the consumer's exact command-list generation, then records
native-depth state -> COPY_SOURCE, CopyResource to private depth, and
COPY_SOURCE -> original state. Private depth enters/leaves COPY_DEST.

The shared `record_consumer_depth` helper validates all fallible preconditions
before issuing commands. SDK conversion, evaluation and output use the same
recording and existing lease. Reset plus completed-fence retirement remains
mandatory. A qualified copy can still precede a later history/route rejection;
that copy-only recording retains and retires its inputs normally.

Scaled post-draw and post-CopyResource private depth injection are disabled.
The native calls and bounded draw observations still run. The DLAA build retains
its previous writer-copy path. No waiting on a second recording worker or stale
request reuse is introduced. Missing or ambiguous proof uses the existing
unfiltered suspension/recovery path. It cannot borrow an older matching-looking
lease: the depth-eligibility channel's depth flag means a copy qualified for this
consumer, not merely the raw previous lease flag.

## Telemetry and offline checks

`consumer_depth` schema 1, policy `scheduled-consumer-depth-copy-v1`, reports
`writer_copy_disabled: true`, attempts, accepted/rejected, copies, hoisted-state
count, and 12 reason counters. It retains the first eight receipts plus latest.
Each receipt records ordinal, tick/frame/epoch/request/list/generation, reason,
writer/consumer/next indices, derived D3D state, relevant node count and equality.
Reason 0 is success; 1 shape; 2 writer/owner/viewport; 3 schedule; 4 tracker;
5 interval; 6 state nodes; 7 changed reread; 8 conflicting API barriers;
9 occupied private lease/missing selected depth sequence; 10 descriptions;
11 reserved. Existing schedule and depth channels remain for diagnosis.

Both DLLs build with warnings treated as errors. All 46 selected native CTest
cases and 94 Python tests passed. Pure cases exercise valid RT/hoisted states,
hoisting outside the old 32-packet window, changing identities, duplicate and
missing packets, invalid epochs/ordinals/chains, wrong or cyclic state nodes,
API conflicts, unavailable leases and bounded receipts. Python tests consume
actual native JSON and check counter/receipt consistency. Explicitly synthetic
complete traces cover versions 1.25.0 through 1.25.5 and reject missing channels.

WARP and NVIDIA fixtures each passed 64 frames and 524,288 exact float-depth
pixels, with 32 RT and 32 hoisted-PS cases. They record the consumer first on
the CPU, then the writer, and submit writer before consumer. Seven fence-ordered
DIRECT-queue switches and changing source resources are included. All 64
recordings complete and retire; debug/GPU validation reports zero warnings or
errors. Invalid self-copy, extent and state cases issue no commands. An initial
fixture run reported missing optimized-clear warnings; that fixture omission
was corrected and the failed attempt is retained.

The GPU fixtures prove the shared copy/state/retention mechanics under explicit
submission order, not the native planner or menu behavior. Unchanged scaled
route/SDK GPU evidence is inherited separately from v1.25.2 after source-hash
checks. The sealed v1.25.4 live trace is replayed by the updated reviewer.

## Next live test and removal

Use the same bare MO2 entry, 2560x1440 window, native AA/DLSS/MotionBlur Off.
Load a save and wait for arming. Check a quiet baseline, then open the menu
that reproduces skips, save the new proof counters, and close it to inspect
recovery. Do not broaden travel coverage before this reproduction. Stop
intentionally and verify full scale, all GPU work, jitter and native hook bytes.

Evidence/deployment: `local/continuous-dlss-20260929/v1.25.5`. The exact v1.25.4
MO2 DLL and sidecar are preserved there. With the game closed,
`deploy.ps1 -Action Restore` restores those two files if explicitly requested.
Keep the latest installed by default. No game executable, original asset or
graphics/profile setting is modified by deployment.
