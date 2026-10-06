# Ordinary TAA compute shader and binding recipe

Offline map, 2026-09-25. Executable file/product version `0.0.0.0`, host SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
The existing private decoded image has SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Offsets below are RVAs or object-relative fields, not portable signatures.
All five additional code spans are checked against the live image before arming.

## Established static paths

The existing ordinary-TAA producer hook is unchanged. See
[its input map](ordinary-taa-input-map.md) for the ABI, caller admission and
resource lifetimes. No compute consumer or descriptor creator is hooked.

`23D0830` builds the state referenced by context `+1C8` and packet `+10`.
State `+8` is a shader handle; handle `+0` is the shader set. Set `+88` is
the resource-owner count and `+80` points to DWORD global binding IDs.
State `+30 + 8*i` stores the corresponding owner copied from the context's
global owner table. State `+10` points to one mode byte per owner.
State `+18` is the technique index. The state can be reused when unchanged.

`23ACC10` selects the compute shader through set `+78`, then entry `+10`.
The descriptor has a DWORD byte count at `+0` and bytecode pointer at `+8`.
On a cache miss it passes these to `23A3BB0`, which constructs the native
compute PSO using backend `+2A0` as its root signature.

`259CC20` is a direct compute-packet consumer. It uses the same state and
shader set, passes set `+44` to `259F610`, and eventually calls Dispatch.
The binder reads a WORD instruction count at set `+44`, a WORD starting
index at `+46`, and DWORD instructions from the table pointed to by set `+0`.

| Instruction bits | Observed meaning |
| --- | --- |
| 0–3 | Kind: 0 constant buffer, 1 sampler, 2 SRV, 3 UAV |
| 4–11 | State owner index for sampler/SRV/UAV |
| 12–23 | Global metadata index used in the CB and UAV-counter paths |
| 24–31 | Descriptor-table ordinal; requires shader/root-layout interpretation |

For SRVs, the binder reads owner `+78`, then view wrapper `+10` as the
source CPU descriptor handle. UAVs use owner `+80` and wrapper `+10`.
These handles are copied to the working descriptor table. A handle alone
does not reveal the typed view format, mip range, resource state or GPU use.
Names are associated using captured global binding IDs and matching owners,
not inferred from register numbers. Unused named inputs need not occur in
a compiled variant's resource-owner list.

## Live gates added in v1.1.0

| RVA | Bytes | SHA-256 |
| --- | ---: | --- |
| `23D0830` | 518 | `652891ad67f47d632a5cfbc9dc5d0a4356075d22f875bf4ebf872f16c8bd1c97` |
| `23ACC10` | 228 | `a44070c5139f8781f6142f1ae2b4710efe8f284fd3b52656c6543e1bf9f6c734` |
| `23A3BB0` | 107 | `ea53238830e5665c24a082f0a70e543147c44698daffb1bff576d8c3c13fe041` |
| `259CC20` | 1283 | `6921c7cbfc7b277f4b076a68de8165bd54b3f6571ca574fdb16d5c35cbe154da` |
| `259F610` | 924 | `936ac61d6575029672cd657bc8956e63f00d8ac50df2ede414d8c93dcf886bb5` |

Complete chained unwind regions were included; a single unwind fragment
is not assumed to be the complete function. Together with the existing
39 discovery and four TAA gates, the new observer checks 48 spans.

## Limits and next evidence

v1.1.0 captures at most one 256 KiB SM5.0 compute shader, 64 owners and 128
binding instructions for each of three originating requests. It compares
all metadata and shader bytes around the original producer. It saves an
owned byte copy only after callbacks drain and the hook disables. Resources
retain the same COM pin/release contract as v1.0.0; registry/state pointers
are only read during the admitted callback, with no ownership assumed.

The captured bytecode will let us inspect actual texture loads and decoding
math. Runtime shader capture, typed-view validation, motion-vector units/sign,
camera/object coverage, GPU states and submission alignment remain pending.
No modern reconstruction is implemented by this observer.

A search for indirect calls at vtable offset `90` found `26274E0`, but its
objects and call arguments do not establish ID3D12Device SRV creation. It is
not used as an SRV creator or a runtime gate. GBuffer1's engine height flag
and typed SRV interpretation remain unresolved.
