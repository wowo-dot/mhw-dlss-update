# Five-sample depth history capacity v0.9.1

Runtime outcome: [run 10232](../research/depth-sequence-result.md) passes the
archived five-image review using 5132 history entries. Separate analysis agrees
with all three predicted shifts on static smooth depth interiors. The run is
closed and the previous DLL restored. The following records the build protocol.

This is the bounded capacity correction for the rejected
[v0.9.0 run 49240](../research/depth-sequence-capacity-result.md). That run
completed three scoped CPU injections/restorations and five private Signals,
but reservation 4097 exhausted the 4096-entry history before retirement
validation. It remains rejected; no images were saved.

The fixed append-only history increases to **8192 entries**. Reservation 8193
still closes admission with `Error::capacity`. No history is overwritten or
filtered, and the original pre-selection state interval is retained. All other
event capacities and the 50 ms history warmup remain unchanged. The private
readback worker scheduling and retirement algorithm are unchanged.

The static journal storage has a compile-time ceiling of 12 MiB; the exact
compiled size is logged in `provenance_ready` and sealed in the preparation
receipt. Each sorted history-pointer scratch array is 64 KiB, guarded by a
compile-time limit. No allocation is added to a hook. The GPU readback budget
remains five private buffers, each at most 64 MiB.

Production header version is `0.9.1`; the experiment and provenance schema
remain `projection-jitter-depth-sequence` and 4. Both the armer and full reviewer
require exactly 8192 history entries for this version; earlier captures retain
their 4096-entry contract and 8 MiB static-storage ceiling. The new ceiling is
12 MiB. Standalone owned-journal validation recognizes both fixed capacities;
full capture review also checks the header version. Companion file size remains
bounded at 12 MiB and the history count must still match every retained event.

## Unchanged mutation and lifetime contract

The [v0.9.0 sequence protocol](projection-depth-sequence.md) still defines the
hook targets, Windows x64 arguments, borrowed ownership and scoped mutation:
native helper RVA `228F3B0`, RCX camera `+50`, RDX renderer `+710`, AL result,
accepted caller return RVA `228E834`; camera `+38` restores in `__finally`.
The tested executable is version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
There are no new hooks or native offsets. All 45 distinct byte-gated spans
are rechecked against the private local image before deployment; live byte
checks still run inside the verified game process before arming.

The sequence is baseline, three distinct native jitter pairs, zero recovery.
Every admitted injection restores immediately. Only stage 1 has the traced
source/publication/consumption/CBV/draw chain; the other slots retain their
exact request stamps and narrower request association. Later stimuli wait
for the primary draw's validated return. Budgets remain two seconds, three
injections, five copies, five private Signals and no rearm.

Private fence completion, command-list retirement, surviving writable DSV,
resource identity and full request/state/submission review remain mandatory.
An overflow cannot be rescued using a partial trace. Issued private resources
remain pinned until process exit; the original game rendering continues after
hook removal. No vendor SDK or sustained jitter mode is added.

## Verification and operation

The native exact-bound fixture admits 8192 real observed fence reads, rejects
the next and checks that further calls still forward without new reservations.
The five-slot pressure fixture adds 6000 real fence reads before its sources,
then delays private polling until all five GPU recordings have completed and
reset. All five distinct owned clear-value payloads must save and pass review.
These values verify readback routing, not raster jitter.

The overflow fixture issues all five copies/Signals and resets the recordings,
then exhausts history before private polling. Every slot must remain unmapped
and quarantined, even though its fence completes. Python checks inspect these
actual native journals, damage the version/capacity/count/storage contract,
and verify the armer rejects mismatches. Full core/GPU fixture composition is
explicitly synthetic. Older accepted game reviews must remain byte-for-byte
equivalent; older rejected runs, including 49240, must remain rejected.

Use the same `barebonesdlss` profile and **Monster Hunter: World - DLSS 1.2.110
test** entry, preferably offline. Keep DLSS Off, TAA On and DX12 On; load a save,
keep the camera still and report readiness from the phone. This build requires
no DLSS toggle. Readiness is determined by the current process generation,
loaded DLL hash, versioned log and armer checks.

Local evidence is under `local/projection-binding-20260923/v0.9.1/`. Its
deployment script preserves the exact previous DLL and output-sidecar state.
After normal game exit, `deploy.ps1 -Action Restore` restores both and checks
all four configuration hashes. Original game executable/assets remain untouched.
No v0.9.1 game result is claimed at preparation time.
