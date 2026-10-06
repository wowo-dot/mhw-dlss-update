# Dependency binding boundary diagnostics

## Purpose and evidence

v1.25.31 diagnoses the v1.25.30 wingdrake failure recorded in
[radial-rgb-write-mask-result.md](radial-rgb-write-mask-result.md).
It adds observation only; it does not claim to fix travel or alter admission,
GPU resource ownership, frame selection, shaders, or native rendering.

The tested executable SHA-256 remains
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Offline inspection used the separately hashed decrypted image
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Private disassembly and verification receipts are under
`local/dependency-binding-check-20261001`; no proprietary code is distributed.

The code-gated native draw executor (RVA 0x259d670) calls graphics binding
setup at 0x259e670 before DrawInstanced. Its stage loops at 0x259f330 / 0x259f610
copy SRV descriptors using the graphics table's texture owner, owner+0x78
view, and view+0x10 CPU handle. Samplers are resolved separately; kind-zero
constant bindings use backend-context/global buffer metadata. The same whole
CPU binding snapshot therefore contains more than the texture identities.
Static inspection does not establish which value changed during the failed run.

## Observation points

No new hook or calling convention is introduced. Existing draw entry receives
RCX executor, RDX backend context, R8 packet; the existing DrawInstanced
observer supplies the later API boundary. The stream-route mutex protects
retained plans and counters. Each qualified copy/radial draw records:

1. The original plan's preparation snapshot, unchanged from v1.25.30.
2. A bounded comparison at native draw entry, before native setup runs.
3. The existing comparison at DrawInstanced, before any private draw is issued.

Entry observation cannot authorize or reject GPU work. The API boundary keeps
the exact prior requirement: readable bindings, the known binding contract,
and full field-wise equality with preparation. A failed API check still stops
the feature and preserves the native rendering path. Other effect types retain
their existing policy.

`dependency_binding_checks` schema 1 / `dependency-binding-boundaries-v1`
counts entry and API checks and rejections. Only the first three API failures
are saved. Each has tick, frame, epoch, stage, dependency kind, packet, graphics
table and shader-set identity, plus the entry and API comparison receipts.
Each comparison retains at most 16 differing scalar fields, a total difference
count and complete field-category mask. No raw memory or buffer contents are
logged; successful checks add counters only.

Field categories: 0 count; 1 IDs; 2 modes; 3 object addresses; 4 stage ranges;
5 opaque stage metadata; 6 binding words; 7 owner metadata; 8 views; 9 CPU
descriptor handles; 10 PS register indices. Owner metadata indices are
`13 * texture_register + field`, in order: id, kind, flags, width, height,
engine format, GPU-format metadata, object, type, primary, secondary, backing,
resource. The field-category mask still includes differences beyond the first
16 saved scalars.

Read progress phases: 0 complete; 1 header; 2 ID; 3 mode; 4 object;
5 stage range/metadata; 6 range bounds; 7 binding word; 8 word/register bounds;
9 texture owner; 10 view; 11 CPU descriptor; 12 expected texture-register mask.
The associated index selects the binding, stage or texture register. Phase 7
uses `128 * shader_stage + word_index`. If reading fails, later fields remain
zero-initialized: differences from those fields are incomplete evidence, not
proven mutations. `readable` and the progress receipt must be checked first.

These are CPU table checks, not independent GPU descriptor-content readback.
An entry pass followed by an API rejection narrows the interval to native
setup/recording, but does not by itself prove the changed field is harmless.
An entry rejection instead indicates change since preparation. Do not relax
owner or descriptor checks merely because a table changes.

## Verification and next run

Pure tests cover bounded difference storage, full masks after truncation,
immutable first-failure samples and rejection accounting. The production
reader fixture injects individual descriptor, owner-flag, constant-object and
mode changes, plus an unreadable modes table; it verifies the exact field or
read phase and preserves rejection. GPU fixtures exercise actual dependency
recording and restored native state. Python validation checks the emitted C++
receipts, malformed evidence, history, and synthetic v1.25.31 requirements.
Historical logs remain supported. A runtime failure must remain failed even
when its diagnostic evidence passes review.

Next: same solo/offline MO2 profile, DX12 windowed 2560x1440, native AA/DLSS and
MotionBlur Off, no resizing. Verify startup, arm at main menu, load a save,
then take one wingdrake trip. If the failure returns, inspect the new receipt,
verify cleanup and archive; no Gallery replay is needed for this diagnostic.
Keep the installed DLL afterward as requested. Exact backup and restoration
instructions accompany deployment. No settings or original game assets change.
