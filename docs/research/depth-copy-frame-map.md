# Original depth-copy request mapping

Offline follow-up to [v0.8.0 run 51816](depth-readback-result.md), 2026-09-24.
Executable version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
Decoded-image SHA-256
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
The decoded image and disassembly remain private local artifacts.

## Confirmed observations

The selected draw's submission is order 5121. The readback list is submitted at
5615, index 1. Between them, DIRECT submission 5601, index 0 contains a matching
depth clear at 5406 in recording Reset 5229 / Close 5410. These are submitted
recording associations, not an assumption based on CPU recording order. Thus
the copied pixels cannot simply be assigned to selected frame 58792. This does
not identify every later depth draw or prove the pixels' complete frame origin.

Static examination connects the original copy packet to the native copy:

| Stage | RVA / data | Observation |
| --- | --- | --- |
| Copy producer | `0x23CD170`, length `0x6AF` | Builds opcode 10 packets and appends them to the frontend context queue. |
| Depth source | Fifth argument 8 | Reads source owner through `context+0x50`, then `view+0x58`. |
| Queue publication | `context+0x2B78`, count `+0x2B80` | Sixteen-byte entries hold the packet pointer at entry+8. The array can change. |
| Dispatcher | `0x22986B3`, opcode table `0x2298AB8` | Low five bits select opcode 10, a duplicate-copy coalescing path and executor slot 5. |
| DX12 executor | Vtable `0x354E4B0`, slot 5 | Dispatches to `0x259DF30`. |
| Copy consumer | `0x259DF30`, length 681 | Resolves source/destination backing and calls native CopyResource; return RVA `0x259E100`. |

Producer ABI is Windows x64: RCX frontend context, RDX pointer to four rectangle
u32 values, R8 pointer to two source-coordinate u32 values, R9 destination owner,
fifth stack argument source kind. Empty rectangles and self-copies return without
queue publication. The primary frontend context is renderer+`0x710`.

Consumer ABI is Windows x64: RCX executor, RDX backend context, R8 copy packet.
Executor+8 points to state; state+`0x20` holds the command list. Context+8 holds the
backend. Each owner resolves through nonzero `+0xF0`, otherwise `+0x60`; backing
`+0x20` is the native resource. The packet has source kind at `+0x10`, destination
owner at `+0x18`, source owner at `+0x20`, six rectangle/source-coordinate u32s at
`+0x28`, and an optional nested packet at `+0x40`. Header bookkeeping is mutable;
only its low-five-bit opcode is part of the receipt.

Producer span SHA-256:
`8103c87804f325a15bbeb3529f3b575cc14e3bae986156d53e1b796fbbec6d31`.
Consumer span SHA-256:
`e84ae8742c4a0ef334692595bb89c04c8be80306d4416c20304566cc1e5dd85a`.
These are full-body runtime byte gates on the pinned executable, not portable
signatures. Local `local/depth-frame-20260924/verified-frame-map.json` records 37
instruction assertions, input hashes and the prior capture's intervening clear.

## Next measurement and limits

[v0.8.1](../runtime/projection-depth-frame.md) will retain the renderer stamp
when the original producer builds a packet, then associate that packet's
generation with the DX12 consumer and admitted readback. A later global stamp
sampled on a worker thread is not evidence of the original request's frame.

The primary-context copy path is statically mapped; its exact live call frequency,
publication ordering and applicability to the readback candidate await the new
capture. Packet coalescing or a missing receipt may prevent readback, which is a
valid diagnostic outcome. Even a successful association establishes the copy
request's stamp, not every draw contributing pixels. Jitter placement and an
aligned baseline/jitter/restoration comparison remain subsequent work.
