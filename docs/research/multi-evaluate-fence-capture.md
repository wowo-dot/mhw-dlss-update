# Three Evaluate inputs and natural fence observations

Prepared 2026-09-22: bridge **v0.8.0**, recorder **v0.7.0**. The first game test, [run 49944](multi-evaluate-first-result.md), captured one Evaluate and 68 natural fence queries, then correctly stopped at an incomplete global checkpoint. Three-Evaluate acceptance remains pending. The [v0.8.1 checkpoint revision](scoped-checkpoint-review.md) is prepared separately. This follows [accepted run 46868](three-frame-result.md); that run
captured only the first Evaluate despite applying three depth corrections.

## Bounded input capture

The explicit `capture-three-evaluates` mode accepts at most three DLSS Evaluate
calls with the same verified diagnostic token. Every call repeats the pinned
caller/core/handle checks, 17 parameter getters and bounded resource inspection.
ISP remains Create-only. Each Evaluate still returns `0xBAD00012`; no vendor
Evaluate or temporal reconstruction is implemented. One-Evaluate mode retains
its original one-call budget. The lifecycle ledger has ten entries, sufficient
for verification, Init, two Populate calls, Create, three Evaluate calls,
Release and Shutdown.

Three independently named, immutable mailboxes bind records to process ID,
creation FILETIME and zero-based Evaluate index. Slot zero retains the original
mapping name; slots one/two add `-slot-1` / `-slot-2`. All use producer 800,
131128 bytes per mapping and a 131072-byte JSON bound. A checksummed qualification
bit requires clean boundary/handle/contract/inspection/barrier coverage. Failed
captures remain useful evidence but cannot authorize another correction.

The journal adds two immutable checkpoint records beside the original first
checkpoint (753832 bytes, producer 800). Each records command identity, tick,
CPU order and reserved/completed-prefix counts. A serialized Evaluate takes a
fresh prefix and selects the latest successful Reset for its actual command
list. A callback that crosses the marker makes coverage incomplete. Summaries
can truncate their small depth-history arrays; the full journal remains the
source for later cross-list analysis. First-capture data is never replaced by a
later frame, even when lists or output buffers rotate.

Recorder v0.7.0 opens all three slots before attachment. At each matching fault
it saves and flushes every available capture and the journal before considering
RDX correction. For correction N+1 it requires exactly N qualified captures and
complete, strictly increasing checkpoints; future slots must still be idle.
The final fault saves all three results, then the spent correction budget
forwards the exception normally. The existing 14 code gates, consecutive frame
counters, stable identities, exact tracker states 0x800/0x4, three-correction
maximum, 500 ms admission window and two-second journal window are unchanged.

## Natural fence observations

Two additional system-method hooks join the seven existing D3D12 hooks:

| Method | ABI | Vtable slot | Tested System32 D3D12Core RVA | Journal kind |
| --- | --- | --- | --- | --- |
| `ID3D12Fence::GetCompletedValue` | `UINT64 STDMETHODCALLTYPE(ID3D12Fence*)` | 8 | `0x88C80` | 7 |
| `ID3D12Fence::Signal` | `HRESULT STDMETHODCALLTYPE(ID3D12Fence*, UINT64)` | 10 | `0x88DD0` | 8 |

Discovery uses a temporary owned fence from the verified command list's device
before enabling hooks, then releases it. Targets must be distinct executable
image addresses in System32 D3D12Core/d3d12. No hard-coded RVA selects a hook.
Hot hooks forward the original call, preserve return values and LastError, and
record opaque fence labels, values, caller/thread and entry/return order. They
add no GPU commands, foreign-object reference retention, waits or extra fence
queries. All nine hooks share the original bounded append-only pools; overflow,
contention, fault or incomplete coverage prevents further corrections.

Tested host SHA-256:
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`
(version 0.0.0.0). Tested System32 D3D12Core 10.0.26100.9278 SHA-256:
`70710DE807F188A85A3F5AD3A729A41BFA7A92B7621CEEAF38059F5640399267`.

Microsoft documents [GetCompletedValue](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12fence-getcompletedvalue)
as returning the fence's current value; UINT64_MAX means device removal.
[Fence Signal](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12fence-signal)
updates it from the CPU, unlike queue Signal. Consequently a high query return
alone is insufficient to prove a selected submission completed. Review must
associate the exact command recording and Execute with a later successful
queue Signal and subsequent query, reject CPU writers, rewinds, overlapping or
multiple writers and removed-device values, and state the remaining lifetime
and external/shared-writer uncertainty. Pointer equality does not prove fence
or resource lifetime. No observed query, or a query below the required value,
leaves completion unverified; it is not a GPU failure. Resource contents and
motion/jitter/depth/exposure conventions remain unvalidated.

## Local validation and deployment

Evidence directory: `local/multi-evaluate-20260922/`. The build uses warnings as
errors. Tests cover bounded lifecycle/cleanup, malformed opt-in, independent
capture retention, failed qualification, missing/stale/reordered checkpoints,
and a real debugger child that executes three corrected loads, flushes all
three captures before the fourth fault and retains them after normal handling
and exit. Owned WARP and RTX 5090 fixtures exercise actual queue submission,
completion queries, CPU signal/rewind, LastError preservation, Reset generations
and changing command identity. These fixture results are not game acceptance.

The already-open MO2 retains `MHW_DLSS_BRIDGE_MODE=capture-one-evaluate`.
The new DLL reads an optional sibling `mhw_dlss_bridge_probe.mode` once at worker
startup. Exact ASCII `capture-three-evaluates` followed by one LF explicitly opts
that recognized environment into this test. Missing file preserves the original
environment mode; malformed/unreadable files fail closed. This avoids restarting
MO2 or silently redefining the older mode.

Stage only with the game closed. Preserve active v0.7.9 at
`D:/MO2/MHW/mods/dlssTEST/mhw_dlss_bridge_probe.v0.7.9.dll.disabled`, preserve any
existing sidecar, and record source/binary/settings hashes in `staging-080.json`.
The game executable and assets are unchanged. Settings remain DLSS Off, TAA,
DX12 On, FullScreen 2560x1440. To roll back with the game closed, restore the
preserved v0.7.9 DLL to the exact active destination and remove the new `.mode`
file (or restore its recorded predecessor if one existed).

Launch the same MO2 entry, stay at the main menu with DLSS Off, and wait for the
recorder's **ARMED** confirmation. Then enable DLSS once, close the popup and
leave Options. Stay at the main menu and report from the phone; do not load a
save or toggle again. A crash remains expected after the bounded test. Use only
an offline session. Archive the settings before restoring DLSS Off after exit.
