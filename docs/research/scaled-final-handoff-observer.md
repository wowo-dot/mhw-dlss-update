# Scaled native final-copy observation

v1.23.7 extends the accepted v1.23.6 three-frame scaled projection capture with
one read-only native helper observer. This preparation is not live acceptance.
Modern NGX evaluation and output replacement remain disabled.

## Question and established evidence

The modern SDK already reconstructed saved half-size inputs to full-size output
offscreen. The previous live capture verified the active rectangle and measured
projection basis. We still need the native final output route: a full-sized
allocation does not prove that the final native copy covers its full extent.

Static review of the captured decrypted executable shows TAA Execute calling
RVA `0x23917a0`, returning at `0x23931b3`. Its native context rectangle is passed
to `0x23c9390`. When engine formats match and the rectangle fits the source,
that helper calls `0x23c9250`, emitting one opcode-13 copy packet. Otherwise it
uses the System_Copy graphics path. This probe qualifies only the copy branch;
an unexpected graphics path is retained as rejected diagnostic evidence.

## Identity and ABI

Tested host version: `0.0.0.0`, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Captured decrypted executable SHA-256:
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
These offsets are version-specific and every function below is gated by exact
live code bytes, in addition to the existing TAA Execute code gate.

| RVA | Bytes | SHA-256 |
| --- | ---: | --- |
| 0x23917a0 | 143 | b135188a6fd2cb924a70fae0ff9cfa5da3c524781aa175e9fbd4a00d023c405e |
| 0x23c9390 | 983 | 48a945e9db1187708d672eb8a5527d3c0cca27c761d8dc25c69751ed159b85a8 |
| 0x23c9250 | 313 | 48dc45a224fa9a3d25c884c46039221b34a7b8e21e880889250fd997f3ecb283 |

Windows x64 calling convention: RCX is recorded without assuming its meaning;
RDX is the native render context; R8 is the selected ring-output owner; R9 is
the destination attachment whose owner is at +0x58. No stack arguments. The
assembly wrapper preserves volatile integer/vector arguments and native return
registers; exceptions propagate through a finally-based callback retirement.
Only this helper is hooked; the other two functions are code-gated dependencies.

## Bounded association and ownership

Only a successfully recorded TAA dispatch publishes a selection. The helper
must match that frame, epoch, bank, async mode, context, primary view, ring
output, and thread. Unselected startup/other-layer calls forward normally and
consume no records. Exactly three selected callbacks are allowed.

Both resources are queried and pinned independently to the expected D3D12
device. Native owner metadata is checked against the real texture description;
source identity must equal the captured rwOutput. The destination must be
different. Native lifetime timestamps at owner+0x40 are intentionally excluded
from immutable owner comparison. The recorder does not own native owner or
attachment allocations: bounded reads before/after the forwarded call recheck
their identity while COM references retain the GPU resources.

The queue count at context+0x2b80 and array at +0x2b78 bracket the original call.
Only the single appended slot is interpreted. The opcode-13 packet must contain
the exact source/destination owners, source rectangle, zero destination offset,
zero subresources/mode, and null optional payload. Packet and queue are reread
for stability. Frame, owner and rectangle identities must survive the call.
No native arguments, rectangles, packets, GPU commands or resources are changed
by this observer. The existing scale/projection diagnostic still performs its
bounded temporary changes and restoration.

The parent waits for all three helper returns before accepting completion.
Admission closes before callback draining and hook disable; pins are released
after drain. The DLL and trampoline remain allocated for forwarded returns.
The parent records observer fault 19 for handoff failure. Local faults: 1
contention, 2 publication sequence, 3 entry/resource validation, 4 post-call
validation or unsupported branch, 5 native exception, 6 hook enable, 7 drain,
9 release failure. A failed diagnostic leaves native rendering intact.

## Verification and limits

Owned native fixtures exercise the actual assembly entry and original-call ABI,
three publications, startup forwarding, SEH, nested calls, cancellation, wrong
frame/source, alias, changed owner/attachment, invalid description, malformed
queue/packet and unexpected copy branch. Real WARP resources verify COM pin
retirement. Two combined fixtures exercise publication from the actual TAA
recorder together with projection, depth, submission and GPU readback capture.
The Python reviewer cross-checks all three copy receipts against their actual
TAA records and rejects tampered association, resource, geometry and cleanup.

Even an accepted live capture proves **CPU copy construction and resource
identity**, not GPU execution of the final copy, later spatial scaling, correct
DLSS output presentation, motion convention, or temporal image quality. Those
remain integration work. The next decision is based on the observed final-copy
rectangle and destination, rather than inferred allocation dimensions.

Deployment uses the existing preserved MO2 test slot. The game must be closed
for DLL replacement; its exact previous DLL and output sidecar are backed up.
Do not restore after testing unless requested. The versioned deployment script
supports explicit `-Action Restore` and verifies both the installed and backup
hashes first. No original game executable/assets or saved settings are patched.
