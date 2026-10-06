# Native copy execution and output diagnostics

2026-09-30, v1.25.20. Local evidence root:
`local/copy-branch-output-20260930`. This follows the failed
[broad run](depth-interval-broad-run-result.md).

## Depth correction

The depth interval reader now follows the native opcode-10 execution branch
before traversing a nested draw's state nodes. No location, effect name, weather
or shader identity selects this behavior. The parent state list remains part
of every interval proof. The selected depth writer must still execute its
qualified child draw; copying a complete resource does not satisfy that writer
contract.

The existing native code gate pins the Windows x64 copy executor at RVA
`0x259df30`, 681 bytes, SHA-256
`e84ae8742c4a0ef334692595bb89c04c8be80306d4416c20304566cc1e5dd85a`.
Its arguments remain RCX executor, RDX context, R8 packet. The target executable
SHA-256 remains
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
decoded-image SHA-256 remains
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
No new hook or calling convention is introduced.

`read_copy_execution` selects owner `+f0`, falling back to `+60`, as native
execution does. For kind 8, matching source/destination extents and a full,
zero-origin child viewport take the direct-copy branch; other extents call the
child draw. Other kinds skip the child for destination kind 6, use a direct
region copy for matching backing copy formats, and call the child otherwise.
Direct-copy/skip paths do not interpret the unused child opcode or state head.
The viewport and branch operands are still read and checked. An executing
child must have the qualified draw layout, and its state nodes remain fully
validated.

All branch operands join the existing double snapshot. Unreadable pointers,
changing operands, invalid parent nodes, invalid executing-child nodes,
missing transitions, write hazards and unknown states still reject admission.
No pointer or state is reused from a prior frame. Lifetimes remain native
queue-preparation ownership plus existing command-list generation/fence pins.
Unscaled DLAA's runtime depth-copy path is unchanged.

The receipt retains the existing interval schema and adds
`copy_execution_policy: native-opcode10-branches-v1`. Each receipt's five
`copy_paths` counters mean unknown, whole copy, region copy, skipped, child
draw. Accepted receipts require at least the writer child. Detail site 33
identifies unreadable/unqualified branch operands. Old logs retain their old
interpretation; v1.25.20 review requires the new policy.

Offline cases cover unused poisoned child headers, parent transitions on direct
copies, required child execution for subrectangles/offsets/formats, alternate
backing precedence, executing-child corruption and changing branch identity.
This establishes the code correction against the pinned executor. The next
game run must still demonstrate recovery of the actual smithy/rain cases.

## Output-state failure and effect-size evidence

The broad run's `stream draw order or state` guard is preserved, with independent
mask bits for missing recording (1), unknown output state (2), incorrect output
state (4), output resource mismatch (8), unavailable raster snapshot (16),
unknown RTV (32), mismatched RTV (64), PSO mismatch (128), root mismatch (256).
First/latest bounded receipts retain the frame, packet, generation and compared
values. They grant no rendering permission. GPU fixtures provoke unknown state,
wrong state and mismatched RTV, verify distinct receipts, then confirm that
valid draws still render correctly.

Prepared-queue samples now include bounded native packet bindings, draw
arguments, attachment target and pixel-shader descriptor metadata for their
existing first-two-draw prefix. Reads do not invoke COM or save shader bytes.
Unreadable optional metadata remains explicitly unknown and does not change
feature admission. Shader sizes are bounded to 262,144 bytes. This applies to
all captured post-filter entries, including unknown effects.

The retained nova samples include an additional overlay targeting a 2560x1440
color resource with a 1280x720 draw rectangle. Static inspection also shows that
its helper reads the context viewport, but this does not establish which
technique produced the reported rectangle or prove how to adapt its sampling.
Neither the nova/cutscene rectangle nor the terminal output-state fault is
claimed fixed by this version. Do not resize arbitrary effects based only on
their names or allocation extents.

## Runtime plan

Keep the user's Stracker-enabled profile and native AA/DLSS/MotionBlur Off.
Arm from the main menu, check baseline, then reproduce smithy/rain and continue
through attacks/effects/cutscenes as the user requested. Do not stop after an
individual checkpoint. The existing fault, 60-minute and 128-MiB collector
bounds remain; announce any intentional stop. Preserve Steam recording and
timestamp reports. Preserve the installed DLL afterward. The deployment script
backs up the exact previous DLL and sidecar; no original game asset is edited.
