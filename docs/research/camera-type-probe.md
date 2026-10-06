# v0.13.1: accept the verified derived camera type

Updated 2026-09-23. This corrects the optional projection observation after
[v0.13.0 run 41776](projection-jitter-probe.md#run-41776-existing-capture-accepted-projection-observation-rejected).
That run preserved the accepted one-item CPU path and clean exit, but captured
no matrices. Run 21636 now accepts the corrected CPU projection observation,
with zero stored jitter and identical projection matrices. Modern DLSS remains
inactive; GPU/raster temporal correctness remains unverified.

## Run 21636: CPU projection observation accepted

Exact loaded DLL/host identities, all 23 preflight spans and both idle producer
1301 channels passed. The user enabled once at the main menu, reported a brief
freeze, then turned DLSS Off/applied/exited normally after capture was saved.
One producer, four preparations, one depth correction and one consumer/Evaluate
completed with no guard rejection. All 17 parameter queries and resource
inspection completed.

The source/publication frame and epoch are **10702**, bank **0**. Preparation
and Evaluate observe **10703**, bank **1**, retaining the same descriptor,
payload and original resources. The projection observation is valid at
checkpoint 100 after **110** scalar reads, with rejection 0 and actual derived
vtable RVA **0x33F82A8**. Its camera stamp is 10702 and viewport is 2560x1440,
matching the source. Constant slot 11 has 268 float words; the two 4x4 matrices
start at word offsets 32 and 200.

The actual stored camera jitter bits are **[0, 0]**, and the two matrices are
bit-identical. Independent Python review agrees with the CPU matrix relation
with zero coefficient error. In contrast, the sampled 20-entry candidate is
approximately `[-0.09375, 0.0185185671]`, and the earlier 18-entry candidate is
`[0.3433609903, -0.0381300002]`. Neither nonzero table sample describes the stored
camera jitter in this captured frame. The matrix relation with zero jitter
does not validate the sign/scale of a nonzero raster displacement. No claim
about later frames, actual GPU upload or shader execution follows.

Reset 252, Close 396, submission 415 (list index 1), same-queue Signal 417 and
completed query 627 at value 10727 form the retained API chain, 16 ms after
Evaluate. The completion terminal drained **627** records with **flags 0**.
The expected resource-state declarations replay; full GPU qualification stays
false. One Release and both Shutdowns succeed, cleanup clears, and saved settings
exactly match original Off/TAA. All 132 staged source files and deployed DLL/mode
were unchanged at exit before these note edits. The unchanged build's 45 native
and 52 Python tests and binary/hardware checks still apply. Trace duration is
112625 ms.

Private evidence is indexed by `local/camera-type-20260923/acceptance-21636.json`.
Capture SHA-256:
`B60692428408396E615C3885C91074CBDF64CD61D26CD7F8AEC03306829B39D9`.
Journal SHA-256:
`0E7808FD6C0A9830C8C7D3385809F243448AB3F6957CE32FEB5CBC9B5C700F8C`.
The derived class is confirmed in this run; the previous run's unrecorded type
remains unknown. No unchanged rerun is needed.

## Zero-jitter follow-up and next diagnostic

Offline parameter registration at `0x2391830` names TAA layer `+0x184` as
`mJitterEnable` and `+0x180` as `mBlendRate`. Constructor `0x2391320` defaults
these to true and approximately 0.95. Those defaults do not establish their
values in run 21636. The jitter update at `0x23931E0` also requires layer type
8, layer flags `& 0x12 == 0x12` and clear main-view flags `0x40000/0x800000`.

Publication `0x229A080` checks the camera epoch before calling camera projection
slot `+0x48`. With the special-view bit clear and a stale stamp, it calls the
setter using shared reset vector RVA `0x503C3E8`. That vector is initially zero
in the image's virtual tail, but its live contents were not sampled. A stamped
zero is consistent with this reset path; the capture does not prove which
branch ran or which native TAA condition prevented a nonzero sample.
`zero-jitter-followup-21636.json` records pinned spans and these limits.

The next diagnostic should observe all native TAA jitter gates and a short,
bounded sequence of distinct CPU source frames at the existing producer hook,
including the Off/TAA transition if available. This can distinguish initial
activation from continued zero jitter without admitting additional optional
GPU work. Keep the one-item/one-Evaluate budget, copy scalar snapshots only,
bracket every sample, preserve invalid observations and stop at an explicit
sample/time limit. This sequence recorder is now implemented in
[v0.14.0](jitter-sequence-probe.md), with runtime acceptance pending.
Do not write camera jitter or feed a table candidate to a modern provider on
the current evidence. GPU upload/content, motion and depth validation remain
subsequent requirements.

## Evidence and correction

The previous reader accepted only camera vtable RVA `0x350D188`, installed by
base constructor `0x228D7A0`. Offline caller tracing now identifies constructor
`0x1AC32B0`, which calls that base constructor with the same object pointer and
then replaces its vtable with `0x33F82A8`. Both vtables retain projection slot
`+0x48 -> 0x228E770`. Thus the base jitter/epoch layout and traced projection
routine apply to this derived type. A second array-construction path at
`0x1AC3003` also calls the base constructor and installs the derived vtable.

This proves a flaw in the previous base-only type gate. It is a plausible cause
of checkpoint 2 in run 41776, not a confirmed live type: that capture omitted
the observed vtable and shared several failure conditions at the checkpoint.
The new reader accepts exactly the two verified RVAs and retains its actual
vtable address/RVA and first rejection reason. Unknown types still reject.
It adds no reads, hooks, or GPU commands; the complete fixture still uses 110
scalar reads and the ceiling remains 192. Begin/end stability checks remain.

Two additional exact code gates pin the constructors before hook installation:

| RVA | Bytes | SHA-256 |
| --- | ---: | --- |
| `0x228D7A0` | 372 | `6FD1AFA8C8C0F2027F5CC7263443907484CBCAF750C0A77BABF320027148B9FF` |
| `0x1AC32B0` | 739 | `05958A8A632C195A172389E85BC6280413B32B99196D7C4D4AE10910CC137F2B` |

The host/decoded-image hashes, existing hook ABI, ownership, and projection
formula remain those in the v0.13.0 note. All 19 native host code-span digests
were rechecked against that pinned image. Read-only preflight now checks 23
host/bridge spans totaling 14974 bytes.

Projection rejection values are: 0 none, 1 invalid anchor, 2 failed read,
3 changed source anchor, 4 camera type, 5 camera epoch, 6 unavailable buffer ID,
7 matrix layout, 8 changed or nonfinite captured data. The Python reviewer
requires these diagnostics for producer 1301, distinguishes them from older
producer 1300 captures, and preserves invalid observations without qualifying
their unpopulated matrices. Stable matrix mismatches still remain evidence.

## Verification and deployment

All **45 native tests** and **52 Python tests** pass. New coverage accepts the
verified derived layout, rejects a nearby unknown vtable, distinguishes early
failure reasons, and validates versioned type diagnostics. The real hook fixture
uses the derived type; its emitted JSON passes independent Python matrix review.
Every failed scalar read still rejects. The saved run 41776 replays unchanged,
including its projection rejection and flags 0. CFG (126 targets, 18 wrappers),
assembly/unwind and real hardware D3D12 completion checks pass. The build has
no compiler warnings or errors. No repeated matrix-machine-code replay was
needed because the matrix algorithm did not change.

Private evidence and staging scripts are in `local/camera-type-20260923/`.
Version is `0.13.1`, transport producer 1301, unchanged mailbox schema 1,
journal schema 2 and frame JSON schema 2 with additive type diagnostics.
The prepared DLL is 771584 bytes, SHA-256
`A668DC1E553B96ADC1AE968710300A5C84D8D2A7CE6448581C71D9AEFE9A347E`.
Deployment remains the MO2 `dlssTEST` mod. Preserve the exact prior DLL as
`mhw_dlss_bridge_probe.v0.13.0.dll.disabled`, hash
`7F0F1DAA17C394D718D85F8A77BFAE160C09EDF3928BCF456083266E19A3530B`.
Preserve the unchanged mode file as
`mhw_dlss_bridge_probe.mode.before-0131.disabled`. Older backups remain.
`staging-0131.json` archives the source, DLL, tests and settings at deployment.

The mode remains `native-depth-probe` with inherited `capture-one-evaluate`.
Saved settings remain Off/TAA/DX12/FullScreen/2560x1440 with SHA-256
`02BAB1311AD739420182D2FF5DB8A6ED9536235E80B51D2B736A6232633CB235`.
The unchanged mode hash is
`E94AC2A93344FD2069AA07C24FC58A005F59F0FCB68882C07B9C0CFD562154A6`.

After deployment verification, launch the same offline MO2 entry with DLSS Off.
Wait at the main menu for preflight. Then enable once, leave Options to apply,
stay at the main menu and reply from the phone. Save/review both channels while
the game remains open; then Off/apply/exit and verify cleanup/settings. No MO2
restart, external recorder, or intentional terminal crash is required.

```powershell
python local/camera-type-20260923/preflight_0131.py --pid <pid>
python tools/save_capture.py --pid <pid> --output-directory local/camera-type-20260923
python tools/review_submission.py --receipt local/camera-type-20260923/channels-<pid>.json --output local/camera-type-20260923/review-<pid>.json
```

With the game closed, rollback restores the exact v0.13.0 backup over the active
bridge DLL and verifies its hash; no mode change is needed. Full removal disables
the dedicated MO2 Force Load entry and `dlssTEST` mod. Retain evidence/backups.
Even a successful next CPU snapshot will not establish shader execution, raster
jitter, GPU image contents, motion/depth convention or temporal alignment.
