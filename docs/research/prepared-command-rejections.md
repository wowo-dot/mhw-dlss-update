# Prepared command rejection capture: v1.25.26

2026-10-01. This is the diagnostic step toward general downstream handling.
Rendering admission, native shaders/draws, scale, temporal inputs and GPU
ownership are unchanged. It does not yet fix Gallery, wingdrake/combat skips
or the hunter silhouette. The broad v1.25.25 run remains sealed separately.

## Capture point and contract

Use the existing completed native post-queue preparation hook, before recording
workers execute the selected packets. No hook or native offset was added.
See `prepared-frame-output.md` for scheduling and ownership, and
`dlss-long-run-20261001.md` for the failures motivating this diagnostic.
Pinned executable SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.

Each overlay refusal now records an exact branch and its operands. The gate is
`[branch, key, packet, queue_index, raw_word, raw_read, prior_draws_for_key,
total_overlay_draws]`. A non-packet gate uses UINT32_MAX for queue_index.
Branch values are:

1. Descriptor flags could not be read.
2. More than eight candidate descriptors.
3. Multiple descriptors match the same command key.
4. Packet opcode word could not be read (preserves original graph reason 10).
5. More than one draw for a candidate key.
6. Total overlay draw capacity reached.
7. Unsupported command opcode; existing admission still accepts only 0 and 11.
8. Overlay output does not match the preceding admitted effect.
9. Geometry/shader qualification failed; existing qualification and vertex
   operands remain available.
10. Auxiliary input contract failed.

`diagnostics.commands` has policy `prepared-command-rejections-v1`. It retains
up to sixteen first distinct refusal shapes: graph reason/detail,
qualification, branch, key and opcode (32 means unread). Repetitions increment
counts without rereading memory. Overflow is explicitly counted. Samples are
immutable apart from occurrence counts, including unsuccessful captures; a
new run may be needed if the first capture of a shape was unreadable/unstable.

On a new shape, scan the bounded completed queue (1..65534 entries, blocks of
256) independently of the admission filter. Count every matched command opcode
for intermediate stage-15 filters, keep the first 32 command records, and
separately retain the rejected command even if it occurs later. A record has
the raw opcode and an optional bounded 72-byte native packet prefix; failed
reads are explicit and zeroed. No pointer in that prefix is dereferenced by
this diagnostic. No strings, shader binaries or general process dump are read.
Captures for graph reason 6/detail 4 also preserve intermediate color-chain
commands without accepting or interpreting unknown shaders.

Queue identity/count and frame/view/epoch are checked after capture. `stable`
describes those CPU checks, not resource lifetime, GPU execution, or permission
to render a captured command. Histograms count all matches even if saved packet
records truncate. Geometry admission still rejects duplicate/unknown commands;
the recorder cannot turn a refusal into acceptance.

## Verification and next runtime check

The pure capture fixture covers multiple draws, unsupported opcodes, a failure
after the 32-record boundary, unreadable packet/header/block, partial prefixes,
changing stamps, capacity and repetition without more reads. The existing
native graph fixture additionally verifies the real rejection branches and
that opcode-11 state commands remain admitted. Native and Python review tests
and GPU route fixtures are recorded under `local/command-rejections-20261001`.

Next: arm from the main menu, load a save, repeat wingdrake travel, and inspect
the retained exact branch/sequence. One Gallery reproduction is useful if the
run remains healthy. Diagnose generic command/coordinate contracts from that
evidence before expanding admission. No monster-name or effect-name whitelist.

The new test DLL and output sidecar are installed only after preserving the
previous exact destination and settings. `deploy.ps1 -Action Restore` in the
private run directory restores the saved DLL/sidecar when explicitly requested.
Original game files/settings are not written. Keep the current test installed
after the run unless the user asks otherwise.
