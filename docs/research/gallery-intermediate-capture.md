# Gallery intermediate color pass: v1.25.33 diagnostic

Runtime follow-up: [capture and analysis result](gallery-intermediate-result.md).
Both recipes were captured; recovery and cleanup passed. The pass is DOF
composition with optional vignette. Rendering integration remains open.

2026-10-01. This revision prepares the next Gallery investigation after the
successful v1.25.32 wingdrake replacement test. It does not implement a Gallery
rendering fix. The new code records CPU evidence at an existing preparation
callback; reconstruction admission, shaders, geometry, state transitions and
resource ownership remain unchanged.

## Offline reviewer correction

`continuous_output_commands.py` and `continuous_output_diagnostics.py` now use
one qualification validator. Codes 12 and 13 are accepted only as whole-plan
dependency failures: reason 17, detail 0, stage zero, the matching input-binding
or output-order branch, zero key/address/raw/read/seen and absent packet index.
Existing geometry bounds remain. Unknown codes and mismatched receipts reject.
This validates recorded rejection evidence; it does not make a rejected frame
eligible for rendering.

Eighteen Python diagnostics tests include native-serialized discovery/order
receipts and malformed command/graph variants. Thirteen whole-run corruption
tests pass. Re-review of the v1.25.32 run and historical v1.25.25 long run passes;
quality and full-transition claims remain false. The previous sealed archive,
including its original failed review, is unchanged. Corrected reviews are saved
under `local/gallery-followup-20261001`.

## Capture boundary and selection

The earlier Gallery reader captured no stable recipe: it saw a changing primary
view or failed its one-draw requirement. v1.25.33 instead reuses the existing
bounded `OutputDependencies` recorder inside `build_plan`, after rejection and
before returning from the native post-filter preparation callback. No new hook
address or ABI is introduced. Existing post-filter hook/code gates and the
single native preparation callback lifetime apply. Tested executable SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.

A separate `intermediate_dependencies` channel selects reason 6/detail 4 only
when both endpoint reads are known, motion output differs from grading input,
and one descriptor connects those exact owners. The diagnostic type selector
is RVA `0x3526650`, observed in the prior Gallery graph. Stage/order bounds and
unique descriptor identity also apply. This type selects evidence only; it is
not an effect exception or permission to execute a private draw.

The recorder rechecks the native descriptor and snapshots all matching draw
packets, including multiple draws sharing a key. It retains shaders, input
layout, vertex bytes, shader-owner tables/ranges, texture identities/extents,
output attachments and the descriptor/source/destination connection. Two full
snapshots and the frame stamp must agree. Limits remain two samples, four draws
per sample, 16 attempts at least 60 frames apart, 16 KiB per shader, 64 owners,
128 binding words and a bounded native queue. It issues no GPU or COM work,
does not pin native resources and cannot prove GPU state or descriptor contents.

The optional `connection` receipt is also emitted by the existing dependency
channel. Historical schema-1 logs remain supported. For v1.25.33 the new channel
is mandatory, and its samples require nonzero distinct source/destination
owners. The old overlay channel retains its independent capacity and selector.

## Verification and next run

The DLL and native diagnostic fixtures build with warnings treated as errors.
Native tests cover multiple draws, eight unrelated/ambiguous selectors that
perform no reads, stale descriptors, changed frames, throttling and capacity.
Nine dependency-review tests pass, together with 25 collector tests. Synthetic
v1.25.33 full-log tests cover empty and populated captures and reject missing
channels, missing connection receipts and real runtime faults. Synthetic tests
are not evidence of successful capture in MHW. A Python test initially mutated
its own baseline; that fixture was corrected and its failed output retained.

Next: same solo/offline profile with Stracker's, DX12 windowed 2560x1440,
native AA/DLSS/MotionBlur Off. Verify startup and arm at the main menu; replay
the previously affected Gallery cutscene, then check recovery. Capture and
analyse this color pass before modifying its output route. Camp-entry skips
remain deferred. Keep the new DLL installed after normal exit. Exact prior DLL
and sidecar backups and restore instructions stay in the new evidence root.
