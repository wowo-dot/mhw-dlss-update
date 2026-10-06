# Temporal path cache and deferral observation

Date: 2026-09-08. Version **0.7.2** adds read-only observations to narrow the
missing Evaluate after [run 43380](renderer-gate-observation.md#run-43380-ready-gates-through-a-longer-lifetime).
All 14 tests pass; live acceptance is pending. This build does not hook host
code, traverse render queues, submit GPU work or implement modern DLSS.

## Static findings

The captured host identifies a 432-byte `uTemporalAAFilter2` object, registered
at `+0x212BA0` and constructed at `+0x2391210`, with vtable `+0x351BDA8`.
Its execute member is `+0x2392150`. The earlier vtable export started at
`+0x351BDD0`, inside that full table, rather than at the object's vtable base.
No runtime layer-object pointer is assumed from the registration metadata.

Before its four DLSS selector checks, execute can return for a special-mode
condition, missing context, excluded context flags, or deferred main-view work.
The apparent context-lookup call in the decompiler, `+0x24F2320`, actually
takes **no arguments** and returns singleton `host+0x51CC238`. The apparent
arguments in earlier pseudocode were unused register values.

When singleton bytes +56 and +57 are both set and the passed render context is
the renderer's main view, execute calls `+0x24C9240` with codes 15 and 2 and
returns early. That function prepares the layer's record at +320 and passes
it to `+0x24F2350`. This queues the record only if its override byte is set or
its owner has flag `0x10`, incrementing the manager's count at +60 and storing
the pointer in its array at +64. The new observer reads only the manager's
two flags and count; it does not call these functions or inspect queue entries.
The count includes other work and does not identify a queued temporal pass.

Four lazily populated shader identifiers provide additional historical clues:

| Mask bit | Host RVA | Identifier and direct writer |
| --- | --- | --- |
| 1 | `0x4BE60D0` | `CBScreen`, execute `+0x2392150` after the early returns |
| 2 | `0x4BE6110` | `CB_DL`, execute after selecting a nonnull DLSS wrapper |
| 4 | `0x4BE6174` | `TemporalAA2_ResolveDenseVelocity`, helper `+0x23913F0` |
| 8 | `0x4BE6178` | `rwOutput`, the same helper before its work-item enqueue |

All four have captured initial DWORD `0xFFFFFFFF`. For each, IDA finds exactly
three direct references in the named function: a read, an atomic compare/
exchange write and a readback. This is bounded direct-reference evidence,
not proof excluding every aliased write or reset. The helper's cache writes
occur before the type-3 enqueue, so initialized identifiers do not prove the
enqueue completed, that the consumer ran, or that Evaluate was called.

## Identity and live verification

Executable versions and hashes were recorded again before this analysis:

| Artifact | Version and SHA-256 |
| --- | --- |
| MHW, Steam build 15539686 | version fields `0.0.0.0`; `C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA` |
| Captured host | IDA input MD5 `A137D3636F22A730F71522C193B0472E`; SHA-256 `D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C` |
| Modern NGX core | 32.0.16.1047; `B3C82638C60F29AEFF68052FFE7147246A73C80CBEA5883B35A9B0D1ED57DDE7` |

All previous provider/host identity gates remain required. Before a new
temporal sample, the observer verifies the five renderer instruction sequences
from v0.7.1 and 15 additional exact sequences (198 bytes): all 12 cache
reference instructions, the singleton getter, the mode/deferral branch and
the queue insertion routine. The guarded code reader rejects inaccessible or
guarded memory. A mismatch stops the temporal observer, leaving the existing
provider behavior unchanged. The extracted instructions, IDA exports and
source-to-contract audit remain ignored under
`local/temporal-activation-20260908/`.

## Sampling and interpretation

In `capture-one-evaluate` mode, the worker observes a live Init lifetime and a
complete route. It takes at most 8 samples before a successful Create timestamp
is recorded and 64 afterward, at least 2 seconds apart. The existing renderer
observer still takes its separate delayed samples only while a token is live.
The new observer can also sample after Release while Init remains alive.

Each new snapshot uses at most 9 protected scalar reads: four cache DWORDs,
the manager pointer, its two boolean bytes and queue-count DWORD, and a pointer
recheck. With no manager, it uses 6 reads and reports absence. The per-process
snapshot ceiling is 648 scalar reads, separately from bounded code validation.
No borrowed pointer is retained in published state. Address overflow, read
failure, guard pages and invalid boolean values stop observation. A changed
manager pointer discards that snapshot's interpretation and permits a later
attempt. The shared bridge lifetime lock prevents Init retirement during reads;
it does not own the game's manager or make the snapshot atomic.

A changed Create phase or a wrapper active at the end discards interpretation
of that attempt. Before/after labels refer to the recorded successful Create
timestamp, not a frame boundary. Polling gaps can span Create, loading and scene
changes. A cache transition between two samples cannot be assigned to a precise
frame or to the exact moment of feature creation.

`bridge.temporal-snapshot` logs raw cache values, a non-sentinel mask, manager
presence/modes/count and read status. Only changes, the first sample, and the
first change of Create phase publish events. Queue-count fluctuations alone
are suppressed. `bridge.temporal-summary` records before/after sample counts,
valid reads, first/last masks, observed sentinel-to-non-sentinel changes,
discarded attempts and stop/budget status. Logs explicitly label the evidence
`historical-cache-state` and say `execution-count=not-instrumented`.

Interpret the observations conservatively:

- A non-sentinel cache indicates stored lookup output, not necessarily a valid
  shader ID, current execution or successful rendering. Even `0x0000FFFF` is
  retained distinctly from the full-width `0xFFFFFFFF` sentinel.
- An unchanged sentinel can mean an unreached path **or** a lookup that kept
  returning the sentinel. It does not by itself prove the pass was disabled.
- A pre-Create baseline can distinguish pre-existing cached values from later
  observed changes. It does not date changes within a polling gap.
- Manager flags are a main-view deferral candidate. The actual context passed
  to execute remains unobserved, and queue count is not temporal-specific.
- The first generic cache, DLSS-branch cache and helper caches can guide which
  boundary to inspect next. They are not per-frame pass or enqueue counters.

At the time of the v0.7.2 implementation, the special-mode exit, actual layer
enable state, passed context, work-item consumption and frame-resource states
were unresolved. [The 2026-09-22 live investigation](taa-activation-depth-crash.md)
subsequently confirmed the AA-dependent permit and layer enable state, then
reached a pre-Evaluate depth preparation crash with TAA enabled. Resource
layout, lifetime and frame alignment still require validation. Do not change
sizing or patch renderer flags based on an assumed cause.

## Validation and runtime test

Release builds with `/W4 /WX`; all **14 CTest tests pass**. The new test checks
failure at each of 9 reads without subsequent reads or interpretation, missing/
replaced manager pointers, invalid flags, overflowing addresses, sentinel
semantics, transitions and log deduplication. Existing native tests cover
inaccessible/guarded memory. Tests do not establish live renderer behavior.

The x64 Release DLL is **374,272 bytes**, SHA-256
`310014817EBF4C634525921D67911522682A7BD171ECA60EA33EBBD993B09753`.
CFG remains enabled with 104 targets, including all 15 distinct exported
provider wrappers/callbacks. Live acceptance is pending.

The current staged build is **v0.7.3**, which retains these observations after
[driver and Windows revalidation](driver-61088-revalidation.md). Its enable
test is on hold after run 44972's host depth-resource crash; use the
[current diagnostic status](../runtime/modern-bridge-probe-test.md).
The original v0.7.2 setup backed up the existing v0.7.1 DLL and graphics settings,
then sets only `NVIDIA DLSS=Off` before launch. Enable once from the main menu,
leave Options, load one save, remain there about 10 seconds, then exit. Returning
to the main menu is optional for this capture: run 43380 verified exit cleanup.
If exit leaves DLSS saved On, establish Off before the next one-shot attempt.
The diagnostic still performs no reconstruction and its intentional Evaluate
failure may leave an incomplete or stale picture if that boundary is reached.
