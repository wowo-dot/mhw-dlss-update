# Cutscene vertex-gate diagnostics

The v1.25.23 run on 2026-10-01 repeatedly rejected a cutscene overlay at
qualification 327683 (effect stage 5, check 3). DLSS recovered after each
cutscene. External read-only attempts did not capture a stable layout. Saved
shader captures establish the expected vertex shader, not the failing stride,
offset, or CPU arena. Offline vtable inspection did not establish those values.

v1.25.24 records the existing operands at that check. It preserves the exact
short-circuit read order, bounds and rendering decisions. There are no new
process-memory reads, hooks, shader substitutions, resource-state assumptions,
or admission exceptions. This build is diagnostic; it does not fix the cutscene.

Each bounded output graph sample contains `vertex_gate`, null unless check 3
failed. Its ten unsigned values are packet, IA descriptor, context, vertex count,
first vertex, successful-read mask, stride, byte offset, CPU arena, and failed
predicate. Mask bits 1/2/4 indicate successful stride/offset/arena reads. Values
not successfully read stay zero and must not be interpreted as observed zeros.

| Predicate | Meaning |
| --- | --- |
| 1 | Stride read failed |
| 2 | Stride differs from 16 |
| 3 | Offset read failed |
| 4 | Offset exceeds 16 MiB |
| 5 | CPU arena read failed |
| 6 | CPU arena is zero |
| 7 | CPU arena exceeds the existing pointer-overflow bound |

The existing route mutex protects these observations. The first four rejected
graphs and latest rejected graph retain their existing bound; pointers are
diagnostic identities and do not acquire resource ownership. Successful checks
clear the receipt. No new hook or calling convention is introduced. The host
remains SHA-256 `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
All existing hook and resource-lifetime contracts remain in effect.

Python review validates read masks, failed predicates, bounds, context identity,
and short-circuit ordering. Historical logs without operand receipts remain
readable; v1.25.24 full review requires the new field. Native fixture cases
exercise an unreadable IA descriptor, incorrect stride, excessive offset, zero
arena, overflow arena, and clearing after recovery. No invalid case is admitted.

Offline verification: warning-free Release build; 55 focused native checks;
35 Python tests; 288 GPU fixture frames across WARP and RTX 5090, including
native and reconstructed choices with inherited overlay state. The historical
v1.25.23 log still passes full review, a synthetic v1.25.24 schema example passes,
and five corrupted examples reject. Synthetic examples are not live evidence.

Evidence and exact deployment backups: `local/overlay-layout-20261001`.
Next test: arm from the main menu, replay the affected cutscene once, capture
the rejection, verify recovery and stop. No extended roaming is required.

## Live result, 2026-10-01

PID 45364, creation 134352792900580032, loaded DLL
`0d7bba4a3efd36c5c4d397ba39789195be62214368cbc94f737fec6506004ab8`.
Startup, frozen sources and runtime module/configuration provenance passed.
The cutscene reproduced qualification 327683. The operand receipts identify
stride **28**, successful-read mask **1**, and failed predicate **2**. Vertex
count is four and first vertex is zero. Offset and arena were not read because
the stride comparison short-circuited; the zeros in those fields are not values
observed in native memory. This confirms a rejected non-16-byte vertex record,
not its complete attribute layout.

The user reported a clean scene after recovery. We then intentionally stopped
the test: 4,671 DLSS evaluations, zero runtime/stream faults, 1,676 rejected
overlay plans. All 6,353 private GPU signals and recordings completed and
released, downstream live references reached zero, all 6,799 jitter changes
were restored, and full resolution returned. Live read-only checks verified
38 complete native function spans and 18 hook heads. Full review passed
accounting and cleanup, not cutscene correctness. The user confirmed normal
exit, and game absence was checked. The v1.25.24 DLL stays installed.

Two bounded external layout readers produced zero stable snapshots. Attribute
offsets/formats remain unproven. A future generic correction should decode
positions/UVs using the native input layout, retain the native vertex buffer
and pipeline, and preserve coverage, bounds and lifetime checks. Merely changing
the permitted stride to 28 would not establish those properties.
