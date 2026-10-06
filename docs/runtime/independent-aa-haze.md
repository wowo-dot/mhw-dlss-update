# Independent AA-Off continuous DLAA with optional haze, v0.8.2

This revision handles the native haze pass that stopped v0.8.1. The
[mapping and endpoint proof](../research/independent-aa-haze-queue.md) explain
why the native queue can contain four or five nodes and why final color-copy
owners change. Continuous SDK evaluation, GPU ownership, source jitter,
bindings, history policy and the failure latch remain as in v0.8.0.

## Runtime contract

Hook targets, Windows x64 ABI and lifetime rules are unchanged from the
[continuous runtime](independent-aa-continuous.md). Host version 0.0.0.0,
SHA-256 `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
There is an additional code gate on the haze callback (0x24c7760, 2,256 bytes,
SHA-256 `d1e1c91d7cd6ff65cf5fee677a998692fc4e9ead2bedcb7c0a79f88fbe752701`).
No additional hook or callback replacement is installed there.

Admission accepts the existing base set and the same set plus the observed
haze vtable/order/flags. Descriptor input/output fields at collection can be
stale because the gated scheduler overwrites them all. The plan predicts the
complete assigned native graph. Callback, return and restoration must match
that graph, with unchanged resources and the owned callback still at order 2.
Insertion/removal preserves the exact native pointer order and unused tail for
both N=4 and N=5. Native haze continues to run normally.

The existing first-rejection snapshot is retained. New bounded telemetry counts
admissions, successful returns and evaluations by native layout, and haze
appearance/removal transitions. The first complete haze return and first
complete base return after haze each save collection, predicted, callback,
return and restored graphs. A failed return also preserves one snapshot.
Copies occur on the serialized scheduler thread; log writing reads saved
structures under the runtime lock. No per-frame images are captured.

The independent reviewer reconstructs endpoints from the captured native
identities and checks both actual graphs and exact pointer-order restoration.
It cannot claim haze verification unless haze evaluations and their complete
snapshot exist. A base-only run can pass general closure without certifying
the new haze layout. A fault remains a rejected continuous run.

## Checks and test procedure

/W4 /WX build passed without compiler warnings. Eleven targeted native tests,
60 AA Python tests and 16 startup review tests passed. Native graph tests cover
all 144 base/haze enqueue permutations and 720 six-entry removal orders;
fixtures also reject every altered assigned endpoint and foreign entry.
The SDK/conversion/output pass is unchanged, so its v0.8.0 hardware evidence
is retained rather than claimed as newly run.

Session: `local/independent-aa-queue-layout-20260927/v0.8.2/`.
Use the usual bare offline MO2 profile and **Monster Hunter: World - DLSS
1.2.110 test** entry. Keep AA Off, native DLSS Off, Windowed 2560x1440 and DX12
On. Load the save, arm after startup verification, then walk and pan in the
area that previously triggered the stop. No fixed frame cutoff is used.
Confirm haze evaluations and preserved graph receipts before calling this
fix live-verified. Area loads and other AA selections are separate checks.

The exact prior DLL is saved as `previous.dll`. Use sealed arm.py/stop.py for
the current process generation. After stopping/capturing, exit normally and
use restore.py to restore the exact DLL and remove the matching sidecar.
Source/tests/tools stay frozen through capture and closure. Archive this run
with `independent-aa-haze-result.md` and the final current-state document.
