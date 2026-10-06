> Update, 2026-09-30: the original reader incorrectly parsed three ranges
> per stage. Corrected schema 2 parses only the first count/start pair and
> retains the following eight bytes as opaque metadata. See
> [binding capture result](scaled-haze-binding-capture-result.md).

# Read-only haze recipe capture before extending the scaled route

The v1.25.7 capture proved that an optional native haze pass connects motion
output to a different grade input. DLAA at unchanged resolution could leave
that pass alone. The scaled route must also preserve haze while expanding its
output; accepting the changed owner link alone cannot do that.

The existing v1.25.7 DLL is retained. `tools/read_scaled_haze.py` observes a
running instance using only process query and VM_READ access. It does not add
hooks, suspend threads, change settings, call remote code or modify memory.
New evidence is written under `local/scaled-haze-20260930`; the installed output
sidecar points there to preserve the sealed previous run. Its exact old bytes
are backed up there. Restore only that sidecar if rollback is requested; no
DLL replacement is part of this preparation.

## Identity and scope

Before offline mapping, the host version 0.0.0.0 and SHA-256 were verified:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded comparison SHA-256:
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
The live reader matches the process creation time and mapped base against the
versioned input-log header, rehashes the host file, checks the mapped PE identity,
and checks these unmodified native code spans:

| RVA | Bytes | SHA-256 |
| --- | --- | --- |
| 0x24c7760 | 2256 | d1e1c91d7cd6ff65cf5fee677a998692fc4e9ead2bedcb7c0a79f88fbe752701 |
| 0x259e670 | 2554 | 088e67c77b7e96af64c83c8297f6fc9f6d355dbc170530dda125b41f4cb53727 |
| 0x259f330 | 1664 | ec12eca9df5bc5598eed515d9e882fad9d0afcf54c0488f9eaf98127ab046c32 |

The third span covers the per-stage graphics binding helper up to the next
function at 0x259f9b0. Its initial unwind entry is only 56 bytes and does not
cover the contiguous body. At 0x259f384 it loads packed words from the shader
set; type/index select owners from graphics-state+0x30. SRV/UAV branches select
owner+0x78/+0x80 and view+0x10. The caller supplies the pixel-stage range block
at shader-set+0x14 and vertex block at +8. Three count/start pairs are preserved
per stage; texture bindings are interpreted only from the first pair whose
loop is mapped here. CPU descriptor handles are not GPU descriptor contents.

The reader narrows by the primary view, stage 15/order 20000, haze type RVA
0x35372b0, descriptor unit+0x140, and native key 0x78004e20. It requires one
matching draw with the descriptor's output resource. It captures VS/PS bytecode,
input layout, native vertices, rectangle, blend/depth/raster descriptions,
bounded shader owner/range tables, named texture associations, and haze flags.
Names are matched through IDs read from the previously mapped haze callback's
parameter globals, not inferred from texture registers.

Engine texture metadata is labeled as such. COM GetDesc, GPU state, root-table
contents, shader sampling correctness and execution order are not established
by this external reader. Those still require integration validation.

## Bounds and verification

Limits: 16 filter descriptors, 16,384 queue records, 64 shader owners,
32 input elements, 128 words per binding range, 256 KiB per shader, and 16 MiB
total process reads per invocation. Only selected shader blobs and structured
recipe fields are saved; the scanned queue is represented by a hash and the
selected command. Attempts are capped at 32; default 16.

Each recipe requires a matching frame/epoch/view and queue identity at the end
of the read, followed by an identical second complete snapshot and shader bytes.
An observed mutation rejects that attempt. This cannot prove atomicity or rule
out an ABA change; a successful capture remains asynchronous CPU evidence.

Six offline tests cover a complete synthetic recipe, duplicate draw rejection,
eleven invalid structure/size cases, frame changes, same-frame packet changes,
and malformed/wrong-stage DXBC. No native runtime code changed, so no new DLL
build or GPU behavior test is claimed.

## Next live sequence

Launch through the existing MO2 test entry and load the save outside the sauna.
Leave native AA/DLSS Off and keep the window size unchanged. After arming and
checking the baseline, enter the sauna and stay there. Run the reader against
that process's input log, save the accepted shader/recipe, and inspect the shader
sampling and native bindings. Then confirm recovery outside the sauna and stop
the test intentionally with normal GPU/jitter/hook cleanup.

This is preparation for the haze correction, not a claim that it is fixed.
Output ordering and panning-history changes remain separate work. In particular,
early CPU recording must be handled before affected native draws are recorded;
removing the executed-mask guard is not a correct fix.
