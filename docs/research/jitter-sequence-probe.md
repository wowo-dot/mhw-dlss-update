# v0.14.0: bounded CPU jitter sequence

Updated 2026-09-23. Run 30924 accepts the bounded CPU sequence and the existing
one-item diagnostic receipt; GPU/raster qualification remains pending.
This follows [run 21636](camera-type-probe.md#run-21636-cpu-projection-observation-accepted),
which captured zero stored camera jitter and identical CPU projection matrices
despite nonzero table candidates. One frame cannot distinguish activation from
continued zero jitter, and that capture omitted the native TAA gates.

## Run 30924: both CPU phases accepted

The user enabled once at the main menu, reported a brief freeze, then applied
Off and exited normally. They were unsure whether the final wait reached three
seconds. The trace nevertheless contains all 16 wrapper-absent samples before
shutdown; that phase spans 1953 ms. No rerun is needed for missing coverage.

| Observation | Wrapper present | Wrapper absent after release |
| --- | ---: | ---: |
| Valid samples | 16/16 | 16/16 |
| Source frames | 14156–14499 | 23502–23796 |
| Sample span | 1640 ms | 1953 ms |
| Stored camera jitter | Zero throughout | Zero throughout |
| Jittered/unjittered CPU matrices | Identical throughout | Identical throughout |
| Layer `mJitterEnable` | 0 throughout | 0 throughout |
| Renderer `+0xA5` byte | 1 throughout | 1 throughout |

All samples use the verified TAA layer vtable `0x351BDA8` and camera vtable
`0x33F82A8`. Layer type is 8, flags are `0x13` (passing the `0x12` mask),
blend bits are `0x3F733333` (approximately 0.95), and view flags are
`0x2D00D1FD` (clear under `0x840000`). The live reset-vector bits are zero.
Thus **the enabled byte is the sole false clause of the captured native
jitter predicate**. The native callback at `0x23931E0` branches past the
camera-jitter setter when that byte is zero. These are values at the producer
hook; execution or values at the earlier callback are still not observed.

The absent phase follows a successful feature release, but the renderer byte
remains 1. Do not describe this as every internal setting having settled to
Off. This run contains neither a pre-enable baseline nor a loaded scene.
It does not establish whether the flag is menu-specific, controlled by scene
properties, affected by the toggle, or changed earlier in the frame.

The original limits hold: one producer, four preparations, one depth
correction, one consumer/Evaluate, and zero recorded rejection. The matching
recording reset/close/submit calls are 246/387/406; Signal 408 and completion
615 match fence value 14181. The journal drains after 615 records with flags
0. One release, two shutdowns, cleared cleanup and normal trace closure
(120125 ms) are verified. The saved Off/TAA settings, DLL, sidecar and all
138 staged source/archive pairs match their staged hashes at exit. The
unchanged build retains 47 passing native and 64 Python tests.

Private evidence is `local/jitter-sequence-20260923/acceptance-30924.json`,
`sequence-review-30924.json`, `review-30924.json`, the channel receipt/payloads,
and `bridge-30924.final.log`. Capture SHA-256 is
`A46A5B1C01E0CD72A515E16C21BA421A83C34E68E916B3AE7AAF93C25BEC3FAD`;
journal SHA-256 is
`5D6764B1877CA278AB7EDBF600592EA00B4E70D6DE9D789D5A34D2CAA07142E1`.

### Offline follow-up and next decision

The pinned-image scan confirms constructor `0x2391320` initializes the byte
to 1. Registration `0x2391830` exposes its address as a boolean property named
`mJitterEnable`; property data can therefore reach it without a literal
`[layer+0x184]` store. Another property registration at `0x16E41D0` exposes
`mJitterEnable .mValue` at its own object `+0x19`, and `0x16EA1E0` binds the
named property through `0x16E8EC0`. The link from that property object to the
observed TAA instance is **not yet established**.

The direct-displacement search found 851 candidate unwind ranges and 451
ranges with stores at displacement `0x184`, predominantly unrelated types.
Only the constructor writes the byte in the examined TAA implementation
region. This is not an exhaustive writer proof: generic property stores,
adjusted pointers, leaf functions and wider overlapping stores can escape
that search. Private disassemblies and candidate lists are under
`local/jitter-sequence-20260923/jitter-writers/`.

Next, resolve the scene/property assignment and obtain a loaded-scene CPU
comparison, keeping renderer state, layer identity and camera stamps explicit.
The current sequence fills its present-phase budget in the menu, so another
unchanged menu run cannot provide that comparison. Choose a bounded observation
point or explicit sampling trigger before requesting that launch. No jitter
write, new rendering admission or SDK integration is justified by this result.

## Scope and sampling contract

The existing producer hook at host RVA `0x23913F0`, admitted only from return
RVA `0x2392F8E`, now copies an optional sequence before its existing admission
decision. Its Windows x64 ABI remains `uintptr_t(uintptr_t args)`. The exact
caller passes its TAA layer as `args[1]`; `args[0]` addresses the optional DLSS
wrapper pointer. No additional hook, GPU command, resource reference or
optional rendering admission is added. The original one-producer/four-
preparation/one-consumer/one-Evaluate budget and native fallback remain.

The sequence begins only after a nonnull wrapper is seen. It reserves at most
**16 samples with the wrapper present** and **16 with it absent after that**,
at least **100 ms apart per phase**. Both phases share a **120-second window**
from the first sample. A reservation always consumes one slot, including an
invalid or repeated-frame observation. Repeated/regressing frame numbers are
marked invalid; they do not lower the retained frame high-water mark. Handle
recreation does not replenish either phase. The recorder stops on capacity,
deadline/clock regression or trace closure. Missing Off samples are reported
as missing coverage, not inferred from the user action.

The hook uses a nonblocking acquisition of the existing probe mutex and copies
only named scalars; contention skips the opportunity. No borrowed pointer is
retained for later dereferencing. Production timestamps are obtained under the
mutex so a preempted thread cannot supply a timestamp preceding another thread's
first sample. Fixed storage holds 32 immutable records. The worker copies and
formats records outside the hook, with an 8192-byte serialization limit per
record. Closing the trace permanently stops further sampling.

Each reservation captures the existing stable producer frame, native gates,
camera projection snapshot, then repeats the gates and source frame anchors.
The fields include:

- Layer identity/vtable, type `+0x0C`, flags `+0x14`, `mBlendRate +0x180`,
  and `mJitterEnable +0x184`.
- Main-view identity and flags `+0x58`, renderer DLSS-use byte `+0xA5`,
  wrapper-slot identity/value, and the shared reset-vector float bits.
- Source counter/epoch/bank/mode, candidate tables, camera type/stamp,
  viewport, and jittered/unjittered projection matrices.

The TAA layer must have exact vtable RVA `0x351BDA8`; an unknown type is
recorded and rejected. A false native gate is a valid observation, not an
invalid snapshot. The independently computed predicate requires clear view
bits `0x40000/0x800000`, nonzero finite blend, layer type 8, flags masked by
`0x12` equal to `0x12`, and enabled jitter. This is the predicate's value at
the producer hook, not proof that the earlier jitter callback executed.
Sampling the shared reset vector removes the prior initial-image-only
assumption about its live contents.

Gate rejection codes: 1 invalid source frame, 2 read failure, 3 changed data,
4 unknown layer type, 5 invalid/nonfinite values. `attempted=false` distinguishes
an unattempted gate/projection snapshot from observed zero values. A complete
fixture uses 57 producer reads, 26 gate reads twice, 110 projection reads and
five final anchor reads: **224 scalar reads per sample**. Component budgets
bound a sample to 405 reads, each at most eight bytes. Matrix mismatch is
retained separately from snapshot validity. All GPU/raster qualification flags
remain false; these sequence frames have no publication/submission receipt.

## Exact-build gates and verification

The host version remains `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
Decoded-image SHA-256 remains
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
The earlier hook ABI, frame/resource lifetime limits and projection contract
remain those in the linked camera/projection/publication notes.

Two added code gates pin the TAA constructor and property registration:

| RVA | Bytes | SHA-256 |
| --- | ---: | --- |
| `0x2391320` | 198 | `46D851B5EFA9AF23EAE20385A0D5CBD44B7618366E21FA2577E82FC0EDDF7C4F` |
| `0x2391830` | 1778 | `E977A8A458AD4EDC53984366FDC3254E166081A8A0E8733CD2FEB32A5A3B0F10` |

All 25 declared `native_code_span_verified` calls were checked against the
pinned image. Preflight separately checks 25 spans across host and bridge,
totaling 16950 bytes. Both tests count different sets of spans.

All **47 native tests** and **64 Python tests** pass. The new gate tests reject
each of 26 failed reads and every changed reread. Sequence tests cover spacing,
phase budgets, failure consumption, duplicate/regressing frames, deadline,
clock regression, immutable copies and permanent closure. The real hook fixture
captures 16 valid present and 16 valid absent samples while retaining exactly
one original producer, four preparations and one consumer/Evaluate. Its JSON
passes the independent Python predicate/matrix/sequence review. The existing
saved run 21636 still replays with flags 0 and GPU qualification false.

CFG checks (126 targets, 18 distinct wrappers), helper assembly/unwind checks,
and the hardware D3D12 completion fixture pass. Accepted build logs contain no
compiler warnings or errors. No game process was opened for these verifications.
Private artifacts remain in `local/jitter-sequence-20260923/`.

## Deployment, next run and rollback

Version is `0.14.0`; mailbox/journal producer is 1400. Their binary layouts and
frame JSON schema 2 remain unchanged. Sequence JSON is separate schema 1,
written as `bridge.jitter-sample` and `bridge.jitter-sequence-state` trace rows.
The ordinary Evaluate mailbox and completion-bounded journal are still saved
through the existing read-only tool. The sequence is read from the trace log.

The DLL is 785920 bytes, SHA-256
`8BDEE78C69B4A20E74AB4F229932517877D25F34C727CCB15F22A4CFF1E2888D`.
The staging script preserves the deployed v0.13.1 DLL as
`mhw_dlss_bridge_probe.v0.13.1.dll.disabled`, hash
`A668DC1E553B96ADC1AE968710300A5C84D8D2A7CE6448581C71D9AEFE9A347E`,
and the sidecar as `mhw_dlss_bridge_probe.mode.before-0140.disabled` before
replacement in the MO2 `dlssTEST` mod. All older backups remain. The mode stays
`native-depth-probe`, inherited environment stays `capture-one-evaluate`, and
the physical game directory is unchanged. `staging-0140.json` archives source,
DLL, tests and settings.

Saved settings remain Off/TAA/DX12/FullScreen/2560x1440, SHA-256
`02BAB1311AD739420182D2FF5DB8A6ED9536235E80B51D2B736A6232633CB235`.
Sidecar SHA-256 remains
`E94AC2A93344FD2069AA07C24FC58A005F59F0FCB68882C07B9C0CFD562154A6`.

1. After deployment verification, launch the same offline MO2 test entry.
   Leave DLSS Off at the main menu and wait for the v0.14.0 preflight.
2. Enable once, close the popup and leave Options to apply. Stay at the main
   menu and reply from the phone. Save both Evaluate channels and inspect
   the first 16 sequence samples before another action.
3. Once instructed, turn DLSS Off and leave Options to apply. Remain at the
   main menu for **at least three seconds**, then exit normally. If unavailable,
   leave the setting alone and exit; do not fabricate an Off phase.
4. Archive the final trace/settings, review the complete sequence and verify
   cleanup, settings and deployed/source identities. No MO2 restart, external
   recorder or intentional terminal crash is required.

```powershell
python local/jitter-sequence-20260923/preflight_0140.py --pid <pid>
python tools/save_capture.py --pid <pid> --output-directory local/jitter-sequence-20260923
python tools/review_submission.py --receipt local/jitter-sequence-20260923/channels-<pid>.json --output local/jitter-sequence-20260923/review-<pid>.json
python tools/review_jitter_sequence.py --log local/jitter-sequence-20260923/bridge-<pid>.final.log --output local/jitter-sequence-20260923/sequence-review-<pid>.json
```

With the game closed, rollback restores the exact v0.13.1 backup over the active
bridge DLL and verifies its hash; the mode file needs no change. Full removal
disables the dedicated MO2 Force Load rule and `dlssTEST` mod. Preserve evidence
and backups. Modern DLAA/DLSS, nonzero raster-jitter validation, GPU content,
motion/depth conventions and temporal image alignment remain unfinished.
