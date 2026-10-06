# Bounded depth interval rejection diagnostics

2026-09-30, v1.25.19. Local root: `local/depth-interval-detail-20260930`.
Follow-up to the smithy report in
[prepared-frame-return-result.md](prepared-frame-return-result.md).

This version adds diagnostic detail, not a smithy rendering correction. The
depth eligibility algorithm, resource-state requirements, hook targets, GPU
output path and temporal inputs remain unchanged. No effect name is used to
grant admission. Existing renderer/ABI/hash contracts are in
[prepared-frame-output.md](prepared-frame-output.md).

`DepthConsumerAdmission` retains the first eight failed attempts and the latest
failure after that, independently of later successful attempts. Each receipt
includes the existing frame/epoch/list/generation, rejection reason and derived
transitions, plus 14 bounded diagnostic fields. They are populated from values
already read by the admission check; no extra process-memory reads are added.
The detail vector is `[site, schedule_index, opcode, packet, head, node, next,
backing, resource, span, before, after, visited_or_replay_index, relevant_nodes]`.
Unused or not-yet-read fields are zero. Values before a failed field read are
partial observations, not a validated packet. Replay sites use opcode zero.

| Site | Failed operation or condition |
|---|---|
| 0 | Rejected outside instrumented interval inspection |
| 10 | Schedule packet or opcode read |
| 20 | Packet state-list head read |
| 21-26 | Node next, backing, resource, span, before, or after read, respectively |
| 27 | Resource/backing disagreement or relevant-node capacity |
| 28 | State-node traversal capacity/cycle |
| 30 | Opcode-10 nested packet identity read |
| 31 | Relevant transition on an unqualified packet opcode |
| 32 | Writer/nested/consumer interval identity |
| 40 | Writer transition identity |
| 41 | Transition without a preceding writer or following tracker span |
| 42 | Read-state/span/order comparison |
| 43 | Missing required transition |
| 50 | Final identity revalidation |

Site 11 marks a packet inspection in progress and is reserved; normal failures
replace it with a specific site. A second-read failure preserves its detail
even though the public rejection is the existing unstable-snapshot reason 7.
The failure channel has schema 1 and explicit capacity/count. The reviewer
checks rejection totals, retained ordering, immutable first samples, site codes
and consistency with receipts also present in the ordinary sample channel.

Preparation and dispatch now have independent bounded shader caches and
accepted-validation counters. Both retain exact-byte/hash checking. The existing
`shader_admission` again accounts solely for dispatch; the new
`preparation_shader_admission` accounts for preparing output candidates. A
successful preparation validation can only follow a published temporal request,
and a prepared-route publication requires that validation. Reviewers check those
bounds separately. The original v1.25.18 log and failed frozen review remain
unchanged; synthetic validator tests are not a reclassification of that run.

Verification includes the full depth scheduling fixture, unreadable-node and
unqualified-opcode failures, retention after twelve repeated failures, JSON
corruption rejection, shader accounting corruption rejection, the existing
temporal runtime regressions and real forwarding/fault-shutdown fixtures.
This does not establish a smithy fix or game acceptance.

The next run starts at the main menu with native AA/DLSS/MotionBlur Off and the
same window size. After baseline verification, load the save and enter the
smithy; stay at the affected spot without menus when the image changes. Save
that rejection before leaving to compare recovery. Preserve the current DLL
after the test. The local deployment receipt identifies the exact saved DLL
and sidecar; `deploy.ps1 -Action Restore` is available with the game closed,
but the prior output directory is sealed and must not be reused for a launch.
