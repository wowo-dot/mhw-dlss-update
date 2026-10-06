# Queued-work identity across renderer counter changes

2026-09-23, offline follow-up to [v0.11.0 run 17204](frame-packet-probe.md#run-17204-producerpreparation-counter-mismatch).
The run rejected a stable producer snapshot at counter 24291 followed by a
stable preparation snapshot at 24292. Static analysis now establishes a
double-buffered command path and a direct producer-to-descriptor identity link.
This explains why global-counter equality is not a general queued-work identity
test. It does not retrospectively establish the run's image or jitter alignment.

## Verified code and local replay

The original executable is version 0.0.0.0, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
The offline image remains SHA-256
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
Both were verified before analysis. The ignored local verifier checks eight
complete byte contracts and 43 specific decoded instructions. Its preparation
contract includes a trailing jump table, excluded from instruction decoding.

| Function / region | RVA | Bytes | SHA-256 |
| --- | --- | --- | --- |
| Optional producer | `0x23913F0` | 737 | `0FA009DC0FF13AF4F39D5305A9A3392E5274B637230D7A531006A13DCFACB060` |
| Enqueue leaf | `0x23CABF0` | 99 | `5C63FD6C587C250E0FB09F02C2CF776FB44ED90944E90D1B98C80A700036D433` |
| Counter/bank update | `0x2293280` | 489 | `576813BEA02244E8970C592CF2F2A10E74335CB893C1402F560CF3EB4FA9AD64` |
| Record and publish | `0x229A080` | 1428 | `961CF9969C28ABC6B5B63BA9FB476978555EB188FDC173940D7ED81FC566A4E0` |
| Entry sort | `0x23D2370` | 1047 | `0CF82D2B8B6259FBB5CAD0C7468A780BB7026D9A20F720E9BBD8B5CD501FFFC3` |
| Queue finalization | `0x23CA8B0` | 206 | `B27D637CADF610C779092C55DC94CDA7812D4525FBB864FC26ACAD0B91620EDC` |
| Preparation | `0x229CEE0` | 7756 | `11F7CC4D48598967F439659C87142839C4AD3FB900CFAFB7FAB3631655691AF3` |
| Consumer dispatch | `0x259DAE0` | 918 | `A0BA3E63DF0026B40BD545D9A7668B899FBCEF7FD83D5E0898CD9A31BD12338D` |

The 99-byte enqueue leaf has no calls or branches before its return. It was
replayed in a private executable allocation against owned context, entry-array
and descriptor buffers. **96 cases pass** across four insertion indices, eight
flag pairs and three pre-existing entry keys. Each checks the returned address,
the entire context/entry arrays against expected writes, and an unchanged
descriptor. No game process was opened or written. This proves the tested
leaf's ABI/result and write behavior, not the lifetime of actual game objects.

Evidence and the rerunnable verifier are under ignored
`local/queued-work-20260923/`, principally `verified-contract.json` and
`verify_queue_contract.py`. It uses the existing local PE reader and Capstone;
all executable bytes remain local and are not included in repository sources.

## Producer to descriptor to payload

The existing Windows x64 producer receives its argument block in RCX. On the
non-null-wrapper branch it obtains the recording context from `**(args+16)`.
The record/publish function uses `renderer+0x710` as that context; run 17204's
two saved addresses differ by exactly `0x710`.

The producer constructs a 64-byte payload with wrapper at +0, color/motion/
output/depth owners at +8/+16/+24/+32, reset-related float at +40, rectangle
float bits at +44 through +56 and a zero dword at +60. **No frame counter or
jitter vector is stored in that payload.** It reserves payload and descriptor
space separately, allowing an arena refill at either allocation. Therefore,
the fact that the observed descriptor was payload+64 is not an invariant.

The descriptor stores opcode 23 at +0, subtype 3 at +16 and the payload pointer
at +32. At `+0x23916AA` the producer calls the enqueue leaf with RCX=context
and RDX=descriptor. The leaf chooses `context.entry_base + 16*count`, increments
the dword count at context+0x2B80, writes the descriptor pointer at entry+8,
and returns that entry address in RAX. The base is at context+0x2B78.

The producer's remaining epilogue does not write RAX, so **its successful
non-null-wrapper branch returns the inserted entry address**. This is not a
contract for the early null-wrapper branch. The existing game caller ignores
that return. An updated version of the existing producer hook can observe it
after the original call without adding a fourth game hook.

The entry key combines flags from context+0x154 and +0x15C. It is not a frame
number or a unique work-generation identifier. Queue finalization invokes an
entry sort and may replace context+0x2B78 with a different array. Entries are
copied/reordered as 16-byte records. **The original queue-entry address/index
must not be retained as the later preparation identity.** Its descriptor and
payload pointers are candidate identity labels, to be captured immediately
and revalidated in the later callback that owns them.

Preparation walks entries, reads entry+8 into R15, then descriptor+32 into RBX
for the four tracking-helper calls. The consumer dispatcher independently
reads descriptor+32 and passes that payload to the consumer in RCX. Preparation
also writes descriptor+8 as bookkeeping, so a checksum of the entire descriptor
would introduce another invalid immutability assumption. The opcode, subtype,
payload link and immutable payload/resource fields are the relevant checks.

## Counters and command banks are different identities

| Renderer offset | Observed use |
| --- | --- |
| `+0x1F200` | Current command-bank index, toggled with XOR 1 |
| `+0x1F208` | Counter copied into preparation's tracking-context epoch |
| `+0x1F210` | Counter used by temporal history and the jitter accessors |
| `+0x1F3B0 + 8*bank` | Published entry-array pointer |
| `+0x1F3C0 + 4*bank` | Published entry count |
| `+0x93` | Branch selecting current versus opposite bank during preparation |

The counter-update function increments +0x1F208, conditionally increments
+0x1F210, and toggles +0x1F200. The two counters can therefore diverge; the
flags controlling the conditional increment are not assigned product meanings
by this analysis.

The record/publish function finalizes the context's queue and stores its array
and count in the current bank. Preparation selects `current_bank XOR 1` when
the +0x93 branch is active, otherwise the current bank. It reads +0x1F208 into
a local and puts a pointer to that local at tracking-context+16. Thus the
tracking epoch is a preparation-side snapshot of a separate counter, not a
frame stamp read from the queued payload.

Run 17204 is consistent with this buffered execution path. It did not capture
the producer's bank/epoch, returned entry or queue publication, so we cannot
claim which bank transition occurred for that item or impose a universal +1
counter relationship. Steam offline status does not change these code findings.

## Concrete next diagnostic

Keep the one-item budget, native TAA fallback, original helper bookkeeping and
all resource/caller guards. Before changing admission, add a publication receipt
inside the existing producer hook:

1. Preserve the producer's original temporal counter, history indices, jitter
   bits, rectangle/reset and resource identities. Also record the separate
   counter, bank and mode; never overwrite them from a later global sample.
2. Immediately after the original producer returns, validate the returned
   entry against the live context's array/count, then capture and recheck its
   descriptor and payload links with bounded reads. Verify opcode/subtype,
   the complete payload contract and the original resources. Preserve the
   original return and post-call last-error value.
3. Require that receipt before preparation. Match the exact descriptor and
   payload from it, while keeping context/epoch agreement across preparation
   and resource/Reset checks through consumer/Evaluate. A callback arriving
   before receipt publication must reject without waiting; no race grants
   permission and no second item replenishes the budget.
4. Record later renderer counters as observations. Retire the cross-stage
   equality test only together with verified source-to-item binding. Such
   binding establishes CPU provenance, not temporal image alignment; keep
   shader-jitter/content and GPU qualification false.

Offline tests must cover reordered/copied queue entries, independent payload/
descriptor allocations, mismatched links/opcode/subtype/resources/scalars,
failed or unstable reads, unrelated/reused work, preparation before receipt,
separate counters and the single-use lifetime. The new runtime path then needs
its own live code gates, transport/version checks and one controlled test.

At the completion of this offline trace, the deployed v0.11.0 DLL, admission
policy and Off/TAA settings remained unchanged. The subsequent
[v0.12.0 implementation and run 4768](publication-receipt-probe.md) integrate
this receipt, pass offline tests and bind one item through Evaluate in game
despite a counter/bank advance. Temporal image alignment remains unverified.
