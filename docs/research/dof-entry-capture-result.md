# Gallery dispatch entry result — v1.25.38

The 2026-10-01 run localized the diagnostic rejection to the native packet's
resource-state list pointer at `+0x8`. This is a recorder correction to make;
Gallery DLSS rendering is still not implemented. The diagnostic did not change
rendering admission, add GPU work, or cause a runtime fault.

## Confirmed runtime evidence

Process 47856, creation 134353279891276994, used installed DLL SHA-256
`f41818316fe4390609f48d4f33bba2388be1b9df1393f5f046635459bf3ea978`.
Host SHA-256 remained
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
The prelaunch manifest covers 781 files, including 631 frozen source files.
Seven module identities and four configurations passed the prearm checks.

Two complete CPU bundles at renderer frames 50798/50858 contain six compute
passes and the final composition draw. Both samples again describe nine native
textures with the same formats and extents as [v1.25.37](dof-resource-capture-result.md).
All eight shader files match that run; there are 22 dependency edges and eight
external reads. The barrier recorder retained 44 events, losing none.

One of twelve dispatch observations passed: sample 0, pass 1. It recorded one
native Dispatch, clean return, and known depth/TileMaxH states `0xC0`/`0x8`.
The other eleven all rejected at entry reason 11, recipe difference mask 2.
Every complete reread succeeded. Backend identity, recording/list observation,
validity, Reset generation and frame interval passed before that comparison.

Comparing the prepared and entry prefixes shows only words 2/3 changing:
the eight-byte field at packet `+0x8`. No other prefix word changed. The complete
comparison reports no difference in shader identity/bytes, constants, binding
recipes/views, owners or textures. Dispatch dimensions also remain identical.
The first sample changes null list heads to pointers for passes 2–6; the second
changes the same field for all six passes. These values are transient metadata,
not persistent native object identities.

## Interpretation and correction boundary

The existing [state provenance analysis](native-aa-state-provenance.md) and
[state converter analysis](native-aa-state-trace.md) identify this field:
planner `0x229CEE0` clears and builds the packet's resource-state list, and may
move its barriers to earlier packets. Compute consumer `0x259CC20` passes the
packet to converter `0x259BA50`, which reads the list head at `+0x8` before
emitting barriers. Those full native code spans were also checked in this run.

Consequently, byte equality of this field between queue preparation and
native execution is not a valid test of compute-recipe stability. The next
diagnostic should retain the raw pointer as scheduling evidence while comparing
the stable recipe separately. Preserve the full same-stage double snapshot
comparison, all other recipe fields, frame/packet/list/generation checks, and
independent API resource-state observation. Do not replace state validation
with an assumption based on the native pointer.

This finding explains the recorder's entry failures. It does **not** establish
the eleven unobserved dispatch states, descriptor contents, live depth pixel
footprint, or correctness of a future Gallery replay. The native-format offline
fixture passed before this run, but still uses synthetic pixels and explicit
fixture descriptors; see [the v1.25.38 preparation](dof-entry-capture.md).

## Recovery and cleanup

The user confirmed clean recovery after leaving the Gallery. The first recovery
snapshot still showed suspension; the second confirmed advancing DLSS frames
and the second resumption. Final intentional stop recorded:

- 2,253 DLSS frames; zero runtime faults; 2,021 skipped dispatches (not a count
  of displayed frames), including one startup skip and 2,020 Gallery rejections.
- All 4,274 main and 2,021 unfiltered GPU recordings completed and released;
  all 2,275 output recordings and 18 diagnostic resource references released.
- All 8,989 jitter changes restored; full render scale restored; no pending work.
- Frozen full review and all twelve component reviews passed. Independent live
  reads verified 40 full native code spans and 18 hook heads restored.
- Collector ended on the healthy native terminal record: 26 events/snapshots,
  6,057,725 bytes, zero read retries and no fault observation.

The user confirmed a normal scene and normal exit. Game and collector absence,
installed DLL/backup identities, prelaunch immutability and four unchanged
configurations are checked by the closure script before sealing the archive.
The current DLL remains installed as requested. A fresh output destination is
required before the next launch; the closed archive must not be reused.

Evidence: `local/dof-entry-capture-20261001/`, especially
`entry-prefix-differences-47856.json`, `gallery-entry-review-47856.json`,
`closed-result-47856.json`, `stop-native-code-47856.json`, and `archive-final.json`.
No next candidate is installed by this closure. Gallery remains the priority;
monster effects and hunter silhouette follow, with camp-entry skips deferred.
