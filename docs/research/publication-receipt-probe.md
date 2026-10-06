# v0.12.0: bind the producer's published work

Updated 2026-09-23. Run 4768 accepts one CPU publication-bound item and the
completion-bounded recorder, followed by a clean Off/apply/exit. This implements
the [offline queue contract](queued-work-provenance.md) after
[v0.11.0 run 17204](frame-packet-probe.md#run-17204-producerpreparation-counter-mismatch)
rejected a stable preparation whose global renderer counter had advanced.
It permits one guarded work item and one Evaluate capture, followed by native
TAA fallback. Modern DLSS and temporal image alignment remain unimplemented
or unverified; a successful CPU receipt does not establish either.

## Run 4768: publication binding and completion stop accepted

The exact loaded v0.12.0 DLL, host identity, 13 live code spans across the host
and bridge, and both idle
producer-1200 transport headers passed preflight. The user enabled DLSS once
at the main menu, left Options to apply and reported a brief freeze. Capture
was saved while the game remained open, through read-only mappings.

The source snapshot is stable at temporal counter **10791**, epoch **10791**,
bank **1**, async mode **1**. Its publication receipt agrees, using **48**
scalar reads and checkpoint 100: entry index **967**, count **968**, capacity
**687911**, with the returned entry equal to `base + 16 * index`. The exact
published descriptor, payload, wrapper/owners and original resources bind
through all four preparations and consumer/Evaluate. Source capture uses 57
reads; each preparation uses 29 and consumer uses 21.

All four preparations instead observe stable temporal counter **10792**,
renderer/tracker epoch **10792**, bank **0**, async mode **1**. Consumer and
Evaluate also observe 10792. This confirms the recorded counter/bank advance
while the same published work remains bound. It does not establish a universal
one-frame delay or temporal alignment of the images. Source history indices,
candidate jitter bits, rectangle and reset are retained unchanged; actual
shader jitter and units remain unverified.

The guard records one producer, four preparations, one depth correction and
one completed consumer/Evaluate, with no preparation/consumer rejection.
All 17 parameter queries and resource inspection complete. Color/output use
format 26, motion 16 and depth 41 at 2560x1440, on the verified command device;
PreviousOutput is null. The bridge still returns diagnostic `0xBAD00012`
(`not-implemented`) at Evaluate, then suppresses subsequent optional work
through native TAA. No modern SDK or reconstruction executes.

The Evaluate barrier snapshot reports complete bounded coverage with no
fault, overflow or in-flight callback. Independent submitted-order replay
finds declarations color/motion `0x40`, depth `0xC0` and output `0x8`.
Selected recording Reset **216**, Close **324** is list index **2** in
submission **343**. Signal **345** on the same queue names fence value
**10816**; completed-value query **518** observes 10816, 16 ms after the
Evaluate marker. The journal stops with reason **2 (bound completion)**,
status **1**, flags **0**, frontier **518**, completed order **1037** and
terminal order **1038**. It drains and validates its checksum before saving.
Both the runtime recorder and independent reviewer bind this terminal to the
selected work. No overflow flag is cleared or ignored.

Initial resource states/lifetimes, all cross-queue uses, shader jitter,
motion/depth convention and actual image contents remain unverified.
`gpu_safe=false` and `temporal_alignment_verified=false` remain correct.
The immutable Evaluate packet's `gpu_completion_observed=false` is also
retained: the later journal supplies the separate completion observation.

The user turned DLSS Off, applied and exited normally. One Release and both
provider Shutdown calls succeed; cleanup is clear. The trace covers shutdown
at 110359 ms. Saved Off/TAA settings exactly match the pre-run SHA-256 below.
The deployed DLL, mode sidecar and all **127** staged source files were
verified unchanged at exit, before updating these notes; the 43 native tests,
42 Python tests and binary/hardware checks still apply to the unchanged code.

Ignored evidence is under `local/publication-receipt-20260923/`:
`acceptance-4768.json`, `review-4768.json`, `channels-4768.json`,
`capture-4768-0.json`, `journal-4768.bin`, preflight, full closed trace and saved
settings. Capture SHA-256 is
`EAE2924B83FA9B91AD5CF8793361E4781D76C9188F6E291F9F536E979BFF4002`;
journal SHA-256 is
`4FB5033FBAD2BEAF143CCFA6E6B9CF32C0853AACD0E2B89851F54A3A4E246881`.

Next: trace the actual shader jitter binding and design bounded input readback
tied to this work's recording/completion, to validate image contents and
motion/depth conventions. No unchanged rerun is needed. Wider scene/display
transitions and modern SDK integration remain later gates.

## Contract and lifetime

The existing producer hook calls the original producer, then captures its
returned queue entry before returning that original result. It preserves the
post-call last-error value. No additional hook or game-code write is introduced
beyond the existing three-hook diagnostic. The x64 calling conventions,
register-preserving helper entry, caller restrictions, resource ownership and
cleanup assumptions in [the guarded probe](guarded-depth-probe.md) still apply.

The source snapshot now records renderer `+0x1F200` bank, `+0x1F208` epoch and
`+0x93` mode, alongside the original `+0x1F210` temporal counter, history/jitter
samples, rectangle, reset and resource identities. The recording context must
equal renderer `+0x710`. These source samples are retained unchanged.

The returned entry must be the final 16-byte entry in the live context's
array/count/capacity (`+0x2B78`, `+0x2B80`, `+0x2B84`), with bounded counts,
alignment and overflow checks. Its descriptor must have opcode 23/subtype 3;
descriptor `+32` supplies the payload. Capture validates and rechecks the
entry link, wrapper, four owner pointers, reset/rectangle bits and zero
reserved field. A full original-resource capture is bracketed by two equal
receipts. Each receipt permits at most 128 scalar reads; the resource capture
permits 160, for at most 416 reads during publication.

Publication must match the original source resources, recording context,
renderer epoch/bank/mode and scalar contract. A failed or changed receipt
rejects the item (frame checkpoint 4). The original producer thread owns this
single publication attempt. Preparation checks publication readiness before
taking the state mutex: an early callback rejects (checkpoint 5), including
one from another thread. Waiting cannot turn an early callback into an
accepted one, and rejection never replenishes the one-item budget.

Preparation binds the exact published descriptor and payload. Consumer and
Evaluate retain the payload/resource/reset/command checks and preparation
context/epoch agreement. Later queue entries may move during sorting;
descriptor `+8` is mutable bookkeeping, and payload/descriptor allocations
need not be adjacent. These facts are covered by the fixture. The receipt
does not own or extend any game's object lifetime; subsequent stages still
validate their required links and original resource identities.

Later global temporal counters are observations, not equality requirements
or a permitted `+1` window. Every individual snapshot must remain stable.
The receipt establishes the CPU source-to-work link; it does not prove the
images, motion, exposure or shader jitter belong to the same rendered frame.

## Exact-build gates and transport

Host executable version `0.0.0.0`, SHA-256:
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
Decoded analysis image SHA-256:
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
Five new live code gates cover enqueue, record publication, counter update,
queue sealing and sorting. Their RVAs, lengths and digests are recorded in
the offline queue note and implemented in `native_depth_probe.cpp`; all
earlier gates remain. The read-only preflight verifies 13 spans totaling 3834
bytes across the host (nine spans, 3613 bytes) and bridge (four spans, 221 bytes),
as well as loaded DLL/export identities and idle transport headers.

Trace version is `0.12.0`; mailbox and journal producer IDs are 1200. Shared
memory names/layouts remain mailbox schema 1 and journal schema 2. Embedded
frame JSON advances to schema 2 and contains the complete publication receipt,
source epoch/bank/mode and later stage observations. The worker's 8192-byte
packet buffer is checked using JSON emitted by the actual hook fixture.
`source_frame_link_verified` describes the CPU link only;
`temporal_alignment_verified`, shader-jitter and units qualification remain
false. The Python reviewer independently checks the receipt and each stage
while preserving incomplete/overflow flags and `gpu_safe=false`.

## Offline verification

- Release build passes with warnings treated as errors; all 43 native tests
  and 42 Python tests pass.
- Receipt tests cover failed/unstable reads, invalid bounds and pointer
  arithmetic, opcode/subtype/scalars, changed links and resources, source
  epoch/bank/mode, early same-thread/cross-thread preparation and duplicate
  publication. Rejection leaves fallback available.
- The real owned hook fixture accepts source/publication counter 6, consumer
  counter 17 and Evaluate counter 49, with source epoch 7 and preparation
  epoch 28. It overwrites the old queue entry and descriptor bookkeeping
  after publication while retaining the original descriptor/payload identity.
  The independent Python reviewer accepts its emitted packet and retains
  `temporal_alignment_verified=false`.
- Original return/last-error behavior, four original helper calls, one
  guarded depth correction, one consumer and later suppression are checked.
- The real D3D12 completion fixture passes. Historical run 47820 still
  reconstructs submission 395, signal 397 and query 596 without clearing its
  original overflow flag or qualifying GPU safety.
- The new DLL has 126 CFG targets and 18 distinct expected wrapper targets.
  The helper assembly retains aligned calls, preserved registers and valid
  unwind data. These checks qualify the build, not game execution.

Evidence is ignored under `local/publication-receipt-20260923/`, including
test logs, actual fixture JSON, binary/assembly audits and the next staging
manifest/source archive. No game binary or private runtime is redistributed.

## Deployment and completed controlled-test procedure

Build: `mhw_dlss_bridge_probe.dll`, 757760 bytes, SHA-256
`CF3741D4446FD7F0CC334CFB78AA374CBFE45FB30E1D4BEA24FBEC23F8905043`.
The existing MO2 `dlssTEST` DLL is preserved as
`mhw_dlss_bridge_probe.v0.11.0.dll.disabled` (SHA-256
`9F0D53B3A47D2FA42B935D6425CB89AAD54B75611E4867E0EAD10DCD751F41DA`).
The unchanged `native-depth-probe` sidecar is backed up as
`mhw_dlss_bridge_probe.mode.before-0120.disabled`.

With the game closed, the staging script verifies hashes and evidence,
archives sources and settings, and replaces only that MO2 DLL. Saved settings
remain DLSS Off, TAA, DX12 On, FullScreen, 2560x1440, SHA-256
`02BAB1311AD739420182D2FF5DB8A6ED9536235E80B51D2B736A6232633CB235`.
Rollback while closed: copy the preserved v0.11.0 DLL back to the active DLL
name and verify its hash; the sidecar is unchanged. To remove the diagnostic,
disable its Force Load entry and MO2 mod as described in the
[MO2 setup](../runtime/mo2-strackers-loader.md). Physical game files are untouched.

Launch the same offline MO2 test entry with DLSS Off and stay at the main
menu. No MO2 restart or external recorder is needed. Before enabling, verify
the fresh PID and run the read-only preflight (rejects trace age >=240000 ms):

```text
python local/publication-receipt-20260923/preflight_0120.py --pid <pid>
```

After successful verification, enable DLSS once, close its popup and leave
Options to apply. Stay at the main menu and report from the phone. Keep the
game open until both channels and the versioned log have been saved:

```text
python tools/save_capture.py --pid <pid> --output-directory local/publication-receipt-20260923
python tools/review_submission.py --receipt local/publication-receipt-20260923/channels-<pid>.json --output local/publication-receipt-20260923/review-<pid>.json
```

Check one source/publication, four bound preparations, one corrected item,
one consumer/Evaluate and the retained completion/drain result. Any rejection
or coverage flag remains evidence, not acceptance. Then turn DLSS Off, leave
Options to apply and exit normally; if unavailable, leave it alone and exit.
Verify release/shutdown and saved settings. There is no intentional terminal
crash. Save loading, display transitions and modern SDK integration are later
tests, after this CPU publication path is accepted in game.
