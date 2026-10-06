# One-frame packet and completion-bounded journal

2026-09-23: bridge **v0.11.0** implements the next diagnostic from the
[submitted-recording review](submission-order-and-temporal-inputs.md). It keeps
the `native-depth-probe` opt-in, one optional producer, four guarded
preparations, one consumer/Evaluate, and subsequent native TAA fallback.
**Run 17204 stopped at the first frame comparison; full game acceptance remains
open.** No modern SDK, GPU readback or reconstruction work is added.

## Run 17204: producer/preparation counter mismatch

The exact loaded v0.11.0 DLL, host disk hash, eight live code spans (565 bytes)
and both idle transport headers passed the read-only preflight. The user
enabled once at the main menu and reported a brief freeze. One producer was
admitted; all four preparation calls were rejected, no depth correction was
applied, the consumer was suppressed and **Evaluate calls stayed zero**.
Subsequent optional work remained suppressed through the native TAA fallback.

The user subsequently reported that Steam was unavailable and the game launched
offline. This is a user-reported test condition, not an independently verified
Steam outage. Offline runtime research is the repository's preferred setting.
Startup, capability publication and feature creation succeeded in this run;
the captured rejection is the explicit renderer-counter equality check.
Different launch conditions may affect timing, but no evidence links offline
mode causally to this mismatch. The evidence is retained; an online comparison
is not required before investigating queued-work provenance.

The immutable producer snapshot is valid after 51 reads, checkpoint 100:
renderer frame **24291**, current history index 0, next index 1, selected index
0, reset input 1, reset payload bits 0 and rectangle `[0, 0, 2560, 1440]`.
Mode 1 yielded raw candidate jitter bits `[1024872612, 3204392859]`; the encoded
shader parameter was still `0xFFFFFFFF`. These are source-table observations,
not verified shader bindings or jitter units.

The first preparation snapshot is also valid and internally stable after 23
reads, but its renderer frame is **24292**; its context epoch is also 24292.
Renderer identity, rectangle float bits and reset bits agree with the producer.
The frame equality clause in `FramePacketGate::matches` therefore rejects
preparation ordinal 0, retaining rejection **10** and one recorded stage. Later
preparation/consumer calls cannot revive the rejected item. Their frame values
are not captured by this rejected packet.

This establishes that the sampled global counter advanced between these two
stages. It does **not** establish mismatched image contents, a universal
one-frame delay, or correct temporal alignment. Deferred work under an advancing
renderer counter is the working hypothesis. Trace the producer's queue
payload/descriptor and its ownership/generation offline before revising the
gate. Preserve the producer's immutable history/jitter provenance; neither
relabel it with the later global counter nor accept an arbitrary `+1` window.

The Evaluate mailbox remained idle. With no Evaluate marker, the journal could
not select a completion chain and reached its 2048-call capacity. It closed
and published a checksummed terminal with reason **4 (overflow)**, flag **1**,
frontier **2048** and order **4097**. The submission decoder correctly rejects
this recording for its absent checkpoint. This is not completion-stop acceptance;
the overflow is retained. Raw journal and transport headers were saved while
the game remained open.

The user then turned DLSS Off, applied and exited normally. One Release and
both provider Shutdown calls succeeded, both cleanup obligations cleared,
and saved settings exactly match original Off/TAA SHA-256
`02BAB1311AD739420182D2FF5DB8A6ED9536235E80B51D2B736A6232633CB235`.
The trace captured the full shutdown within 219656 ms. All 125 staged source
files, the deployed DLL and sidecar were unchanged at exit; no game/settings
edit or unchanged rerun was needed. Evidence and hashes are in ignored
`local/frame-packet-20260923/review-17204.json`, with the rejected packet,
journal and final log alongside it. Prior process 44140 was closed by the user
before enabling DLSS; it produced no Create or Evaluate.

The subsequent [offline queue trace](queued-work-provenance.md) resolves the
producer's returned entry and descriptor/payload links, the command-bank
selection and the separate tracking-epoch source. Its 96 owned enqueue replays
pass. The subsequent [v0.12.0 run 4768](publication-receipt-probe.md) accepts
that receipt through Evaluate and the completion-bounded recorder, followed
by clean Off/apply/exit. Temporal content remains unverified. The contract
below describes the historical v0.11.0 guard.

## Captured frame contract

The existing producer/helper/consumer hooks retain their Windows x64 calling
conventions, target spans and process-lifetime trampolines documented in
[v0.10.0](guarded-depth-probe.md). No additional game-code hook is installed.
The original executable remains version 0.0.0.0, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`;
the analysis image remains
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.

Before admitting the original producer, bounded scalar reads retain the
renderer/frame, current/next/selected history indices, sharpen branch, view,
recording context, rectangle, reset input and the reset-related payload value.
The snapshot brackets mutable anchors; an additional frame check covers the
existing resource-owner discovery. It requires history indices matching the
observed frame modulo 3 and the existing 2560x1440 full-frame configuration.
Failed reads, changed anchors, unsupported modes and non-finite samples reject
admission.

The packet stores raw bits from the two-float candidate jitter entry selected
by mode 1 (18 entries), mode 2 (20 entries), or the static mode-0 source.
It also records the cached encoded shader parameter, which may be unresolved.
The selector getter, both table accessors and dispatch region are checked by
their exact code hashes before installation. These source spans and address
formulas are recorded in the preceding analysis note. No host table is changed.

Each preparation retains a bounded stage snapshot: frame, payload, descriptor,
preparation context/epoch, rectangle and reset bits. These must agree with the
producer and earlier preparation stages. The existing depth guard separately
requires the context epoch to match the selected depth tracker's epoch.
**Epoch and renderer frame are distinct fields**; the probe does not assume
they are numerically equal.

The consumer requires the same payload, frame and scalar values. Inside that
consumer's existing thread-local Evaluate scope, the packet binds the actual
command list, four captured native resources and Reset query. Mismatches
preserve the first rejection reason and abort diagnostic acceptance. The
original one-item and fallback gates remain in force.

All ownership labels are copied integers, not retained COM references or
dereferenceable work items for later use. Stage snapshots occur while the
original scoped callbacks own the arguments. The packet remains as immutable
evidence after completion; it cannot authorize a second item or Evaluate.
The worker emits bounded `bridge.frame-packet` telemetry on state changes,
including rejection before Evaluate. Successful Evaluate JSON contains the
same packet in `frame_packet`.

**Limits:** equal frame counters and matching work metadata do not establish
image contents. These are sampled table values, not a capture of the actual
`fJitterOffset` shader binding. Shader component count, jitter units/sign,
projection relationship, motion convention, depth content and exposure remain
unverified. JSON explicitly preserves `shader_jitter_verified=false` and
`jitter_units_verified=false`.

## Journal termination

Completion-based stopping is enabled only for `native-depth-probe`. Other
capture modes retain their previous stopping policy. The observer continues
to record original D3D12 API calls without adding GPU commands, signals,
queries or waits. Rendering callbacks never wait for another callback or for
GPU completion. The existing two-second deadline and fixed capacities remain.

On natural fence-query completion, a bounded, allocation-free replay examines
only an acquire-published, contiguous record prefix. It resolves the selected
command-list recording through Reset, Evaluate marker, Close and submission,
then requires a successful same-queue signal and a later completed-value query
reaching that value. It rejects observed recording reuse, bundles, overlaps,
failed calls, CPU/foreign/rewound fence writers and device removal. An atomic
try gate serializes replay without making another callback wait.

A matching observation closes admission. Already admitted callbacks can still
publish their slots. The final callback replays the drained records, so a
late-published competing writer can invalidate the candidate. Only then does
the recorder publish its checksummed terminal record with reason, flags,
reservation frontier, event order and bound call identifiers. Overflow,
expiry, memory faults and contention are retained and take precedence over a
successful stop reason. Admission closed and fully drained are distinct states.

This records an API completion chain. It does not exclude unobserved writers,
pointer reuse outside the capture, cross-queue resource use or invalid image
contents, and does not qualify GPU rendering. Resource declarations are still
reconstructed separately by the offline analyzer.

## Transport and tools

The Evaluate mailbox keeps its schema-1 layout and name but advances producer
identity to **1100** and adds the optional frame packet. Older readers reject
the new producer. The journal uses **schema 2 / producer 1100**, name
`Local\MhwDlssBarrierJournal-v2-<pid>-<creation>`, and size **753944** bytes.
Its original call pools retain their layout; the 112-byte terminal envelope
is appended after offset 753832. The terminal payload is checksummed and
published once after drain.

Use the new read-only collector while the game remains open:

```text
python tools/save_capture.py --pid <pid> --output-directory local/frame-packet-20260923
python tools/review_submission.py --receipt local/frame-packet-20260923/channels-<pid>.json --output local/frame-packet-20260923/review-<pid>.json
```

The collector checks process creation, producer/schema/size, stable copies,
checksums and the drained journal. Files are created exclusively to preserve
earlier evidence. It does not verify loaded game code; the runtime preflight
and launch log remain required. The analyzer reads both the old producer-801
journal and the new terminal envelope. Do not use `save_channels_0100.py` or
the old DLL's fixed-RVA scalar reader with this build.

## Verification and staged identity

The Release build passes `/W4 /WX`. **38 CTests pass**, including real owned
producer/helper/consumer detours, frame and payload rejection, every bounded
producer-frame read failure, changed snapshot anchors, binding to Evaluate,
transport corruption and the original lifecycle/resource tests. New D3D12
WARP tests exercise normal completion and a callback still in flight after
admission closes. That pause machinery is compiled only into the fixture.

The owned hardware D3D12 completion fixture also passes. The native replay of
run 47820 finds submission 395, signal 397 and completed query 596 at both the
596-record prefix and full saved journal. Its historical overflow is not
waived. **37 Python tests pass**, including the new terminal checksum/frontier
and closed-but-not-drained cases. Binary audits verify **126 CFG targets**, all
**18 distinct provider/hook wrappers**, and the unchanged assembly entry's
stack alignment, captured registers and unwind metadata.

DLL SHA-256:
`9F0D53B3A47D2FA42B935D6425CB89AAD54B75611E4867E0EAD10DCD751F41DA`
(744960 bytes). Evidence, exact source archive, tests and staging receipt are
under ignored `local/frame-packet-20260923/`. The stage script verifies the
executable, saved settings, prior DLL and mode, then preserves the exact prior
files before replacement. Original game files remain untouched.

## First game test and rollback

Start with DLSS Off/TAA and the same offline MO2 entry. Reach the main menu and
wait for the new build/transport preflight. Then enable DLSS once and leave
Options to apply it. Stay at the main menu, reply from the phone and keep the
game open while the immutable channels are saved. Do not load a save or toggle
again until capture is confirmed. No intentional terminal crash is planned.

Acceptance requires a bound frame packet with five successful stages and
`evaluate_bound=true`, the one-item counters, a drained journal with completion
reason and no flags, and an independently matching submission/fence replay.
A guard rejection or incomplete journal is diagnostic evidence, not acceptance;
inspect it before requesting another launch. After evidence is saved, turn
DLSS Off/apply and exit normally, then check settings and shutdown/lifetime
cleanup.

The destination is the existing local MO2 `dlssTEST` mod. Roll back with the
game closed by restoring `mhw_dlss_bridge_probe.v0.10.0.dll.disabled` over
`mhw_dlss_bridge_probe.dll` and `mhw_dlss_bridge_probe.mode.before-0110.disabled`
over `mhw_dlss_bridge_probe.mode`. The old DLL hash is
`3A075694B72E1EDED4E58CD961AF3F762959FF5F401D9EB31B1D6793046D49C1`.
Both versions use the unchanged `native-depth-probe` sidecar, SHA-256
`E94AC2A93344FD2069AA07C24FC58A005F59F0FCB68882C07B9C0CFD562154A6`.
The earlier v0.9.2 fallback backup pair remains available. Disabling this MO2
mod removes the test DLL from the virtual deployment; no game assets require
restoration.
