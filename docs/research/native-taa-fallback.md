# Depth selector and native TAA fallback preparation

2026-09-22: **v0.9.2**, mode `native-taa-fallback`, passes the requested
save/menu return and deliberate Off/On cycle in run 45100. All three handle
generations were retired and all 34 sizing queries succeeded. The user
confirmed selectable On after both checks, then normal exit with saved Off.
All 29 CTests pass. Wider display/device transition coverage remains pending.

## Run 45100: recreation and menu toggle accepted

The loaded v0.9.2 DLL, host and both provider preflights passed. A read-only
baseline found no handle/producer suppression/Evaluate and two successful
sizing queries. The user enabled once, followed the requested save/menu return
and reported **selectable and On**. After a deliberate Off/apply/On/apply and
another Options visit, the user again reported **selectable and On**. Finally
the user applied Off and exited normally. The process was confirmed absent;
settings exactly match the original Off/TAA hash, without an agent edit.

| Evidence | Observed result |
| --- | --- |
| First Create / Release | Success; token `0x10000B12D` |
| Second Create / Release | Success; token `0x10000B12E`; Create explicitly entered from released state |
| Create contracts in text trace | Both complete: eleven scalar queries each, no latched fault |
| Third active generation after trace ended | Token `0x10000B12F`, creates 3 / releases 2; user confirmed selectable On |
| Final stable samples before exit | Creates 3 / releases 3; token zero, creation list absent, copied binding cleared |
| Sizing queries / successes | 34 / 34; 18 successes beyond the 16 retained records |
| Evaluate / unreadable producer arguments / other callers | 0 / 0 / 0 in every read-only sample |
| Last sampled producer suppression | 46,831 total; 38,869 with a host handle |
| Handle read fault | False in the trace and all lifetime samples |
| Normal exit / original Off settings | User confirmed / hash verified |

The trace records the first Release at tick 36,322,625 and second Create at
36,322,968, a 343 ms interval. That establishes successful live recreation;
it does not identify the original host event that initiated Release. The
five-minute text trace ended at tick 36,409,593 with two releases, no active
token, 23/23 sizing successes, clean provider routes and no boundary contention.
The user enabled again after that point. A matching-build reader verified
counter-access instructions plus the actual `HandleLifetime::stats` call and
copy layout before reading bridge-owned metadata at RVA `0xECFC0`. Each
56-byte metadata sample was read twice and retained only when equal. It did
not acquire a target mutex or dereference a foreign core record.

Three stable active-third-generation samples and two final retired samples
corroborate the user result after the text window. The latter show 34/34 sizing
successes and complete retirement. The next observer read returned Windows
ERROR_PARTIAL_COPY (299) as the game exited; the completed samples were saved.
This is an observer termination at process exit, not a recorded bridge read
fault. The user confirmed normal exit and a subsequent process check found
no game. **Shutdown wrapper returns were not captured**, so normal exit and
retired handle metadata are the cleanup evidence for this run.

This accepts recreation and the tested menu/save/toggle sequence. It does not
prove visual equivalence, GPU completion, live ledger exhaustion, Alt-Tab,
resolution/device recreation, camera cuts or cutscenes. The ten-entry ledger
overflow and concurrent Shutdown checks remain offline test evidence. Native
TAA provides the image; depth correction and modern SDK calls remain inactive.

Ignored evidence is under `local/fallback-lifetimes-20260922/`:
`bridge-45100.complete.log`, `preflight-45100.json`, five `live-45100-*.json`
sample files, `graphics_option.after-45100.ini`, and the independent
`review_45100.py` / `review-45100.json`. The reviewer checks both traced
Create/Release pairs, 56 read-only samples, later generation/retirement,
settings, deployed/backup hashes and 69 unchanged native/build/test files.
The trace SHA-256 is
`5B3458AD39CD8F5A3B4A1E9D68BC1D5C2B966CC12331CFE237D66959B8838AB7`.
The staging manifest remains the original deployment record; this review
records runtime acceptance separately. No native source or binary changed
during the run, so the existing 29-test result still applies.

## v0.9.2: reusable fallback lifetimes

Only the explicit fallback mode can size after Release and create another
generation. Every Create repeats the original caller, host/core identity,
D3D12 command-list and eleven-scalar contract checks with a fresh bounded
capture budget. Getter faults remain permanently latched. The producer
suppression hook must be installed before publishing any synthetic handle.
An unexpected Evaluate makes the fallback lifecycle inert.

The first token remains `0x000000010000B12D`. Each later generation increments
the low DWORD; the high DWORD remains feature 1. Exhaustion fails closed and
tokens never wrap. The pinned core copies all eight bytes into its record
before returning and selects the provider using the high DWORD. This contract
was rechecked against the current core SHA-256
`FA30CD56A48F83378FA25AE181B771D63FA6856D0D5A4D65A9D38C9C45432A39`:
Create RVA `0x67C60` (914 bytes), Release `0x8A30` (440 bytes), and Shutdown
helper `0x43550` (1209 bytes) match the previously archived function hashes.
The older IDA export has a different whole-file hash and was not treated as
current identity evidence. `core-token-contract.json` records this comparison.

`HandleLifetime` owns no core record or GPU resource. One mutex covers token
publication, bounded 32-byte record reads, retirement, and Shutdown. A valid
record must contain operation 1, reserved zero, a nonzero parameter clone,
the exact active token and that generation's creation command list. The first
verified copied-handle address binds the generation. Release validates and
retires it in the same critical section, clearing the active token, command
list and binding. Shutdown waits for an in-flight read, clears ownership and
permanently closes the manager. Source token storage lives with the pinned
DLL and may be reused only after retirement. Core record pointers are never
retained for later dereference after retirement.

Stale record tokens are rejected even when an allocator reuses an address.
A stale pointer that now names an entirely valid current record cannot be
distinguished by pointer value alone; the implementation does not claim that.
Writer failure, a protected-read fault, invalid identity/order, or boundary
contention still disables admission. Cleanup of a verified outstanding handle
remains possible after a poisoned boundary. The separate observation lock
continues to exclude renderer/temporal snapshots from Shutdown.

Full diagnostic storage no longer changes valid fallback behavior. The
ten-entry lifecycle ledger retains its first records and counts dropped
events while continuing validated transitions. The 16-slot sizing/event
queues remain bounded. New aggregate handle publication/release counts and
the active token appear in checkpoints and fallback reports; publication
counts are not a substitute for a successful-return record if a boundary
is subsequently cancelled. Fallback reports retain twelve initial samples,
then report changed handle/sizing counts at most every two seconds, up to
32 total. The existing five-minute text trace limit remains. None of these
logging limits stops the fallback. Earlier capture modes keep their limits.

Offline evidence under ignored `local/fallback-lifetimes-20260922/`:

| Check | Result |
| --- | --- |
| Release build, warnings as errors | Pass; no new warnings |
| Full CTest suite | 29/29 pass, 7.25 seconds |
| Production policy + lifetime manager | 256 Create/Release generations; changing command lists and reused/different record addresses |
| Sizing and full ledgers | 22 sizing admission checks per generation beyond retained-record limits; eleven Create scalars rechecked per generation |
| Invalid cases | Stale/malformed records, duplicate release, failed publication, read fault, unexpected Evaluate and foreign caller rejected |
| Concurrency | Release/Shutdown wait for an active read; two concurrent releases retire exactly once; no subsequent read after retirement |
| Legacy modes | One-shot lifetime and sizing restrictions retained |
| Final PE/CFG audit | x64; 123 CFG targets; 16 distinct provider wrappers verified |

Final DLL: **694784 bytes**, SHA-256
`B63D4B14372259FF0B8C66D2F9A4A38F303EF95E8A68C44B0B85E7ECEE2C1304`.
`build-final.log`, `ctest-final.log` and `binary-audit-092.json` identify the
tested build. The host remains version 0.0.0.0, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
No new host hook, depth correction, GPU work or modern SDK call is introduced.
The existing native producer hook ABI and ownership contract are documented
below; its code gates are unchanged.

Guarded staging requires the game closed and the exact v0.9.1 DLL, mode,
settings and host hashes. It preserves v0.9.1 at
`D:/MO2/MHW/mods/dlssTEST/mhw_dlss_bridge_probe.v0.9.1.dll.disabled`, archives
sources/build evidence and records `staging-092.json` before declaring the
new test installed. Staging completed with all identity checks passing and
the game closed; v0.9.2 is now active in the existing MO2 mod. With the game
closed, copying that preserved DLL over
the active `mhw_dlss_bridge_probe.dll` rolls back this change. The sidecar
remains `native-taa-fallback`; the settings file remains Off/TAA/DX12 On,
fullscreen 2560x1440. Original game assets/executable are not modified.

The runtime procedure used for run 45100 correlates the visible setting with successful
Create/Release/recreate and sizing counts: enable at the main menu, leave
Options, load a save, return to title-screen Options, inspect the setting,
then exercise Off/apply and On/apply if available. A greyed-out result should
be left intact for inspection. Verify normal exit and saved Off state.
No recorder or MO2 restart is required. Alt-tab, resolution/device recreation,
camera cuts and cutscenes remain separate untested cases. The initiating host
event behind run 19484's Release remains unknown; run 45100 now proves the
host attempts and completes a second Create. `read_fallback_092.py` is prepared
for exact-build, read-only scalar sampling after the text trace ends. Its
offline layout check passes; it still requires live module/code verification
before any runtime counter read. It requests no debugger or write access.

## Run 19484: sizing fix accepted, lifecycle still single-use

The correct v0.9.1 DLL, host hash and both provider preflights passed. The user
enabled once, reported a normal scene, returned to Options and found DLSS
greyed out Off. The user subsequently confirmed normal exit. The original
Off/TAA settings hash remained unchanged; the agent did not edit settings.

The complete five-minute trace and an independent read-only scalar reader agree:

| Observation | Result |
| --- | --- |
| Sizing queries / successes | 22 / 20 |
| Retained detailed sizing records | 16, all successful |
| Later sizing queries / successes | 6 / 4 |
| Diagnostic Create / Release | One successful call each |
| Suppressed producer calls with a handle | 3,127, then unchanged after Release |
| Last sampled total suppression | 9,070, increasing without a handle |
| Evaluate / unreadable arguments / other producer callers | 0 / 0 / 0 |
| Final bridge lifecycle | released; no outstanding token cleanup |

The first Create was at tick 34,075,953 and Release at 34,094,187 (18,234 ms).
The originating host event for Release was not captured; do not attribute it
to a manual Off toggle or claim that the handle stayed active throughout the
loaded scene. Later renderer reads had stable anchors, advancing counters,
DLSS requested, sizing flags 1 and backend-ready 0. Those are asynchronous CPU
observations, not GPU or image-quality validation. The user saw normal imagery.

An owned offline executable links the same built policy library and executes
the modeled sequence: populated state, ten sizing queries, Create, ten more
queries, Release, then two queries. It reproduces **20 successes and two
lifecycle rejections**, with 80 verified output writes/readbacks and no output
publication for rejected queries. Four post-capacity successes establish that
the v0.9.1 budget change works. The two later failures are consistent with the
unchanged `sizing_lifecycle_ready` rule that excludes `released`.

The offline audit also identifies independent barriers to repeat operation:

- Both the runtime Create gate and `OneShotLifecycle` only admit Create from
  initialized/populated. A second modeled Create is invalid-order and inert.
- The first core-owned copied-handle address remains bound. A different new
  record is rejected even if its token/creation metadata would otherwise match.
- The lifecycle ledger becomes inert after ten events. Unlike the separate
  16-slot text-event queue, its capacity currently changes feature behavior.

These are verified policy constraints; **no second live Create was observed**.
The earlier run-16380 budget explanation was incomplete: removing that budget
does not provide a reusable feature lifecycle.

Ignored evidence: `local/fallback-sizing-20260922/bridge-19484.complete.log`,
`review_19484.py`, `review-19484.json`, `lifecycle_replay.cpp`, its build/log, and
the read-only sample files. Renderer samples are in
`local/driver-61088-20260922/live-19484-fallback-after-transition.json`.
Trace SHA-256:
`4F0408AE29C2E364507275BCE7D0DC6F94C6FF6852A2A1504D6A6733BB43924B`.
The replay builds with `/W4 /WX` and passes. No shipping source, build or deployed
DLL was changed during this run; the existing 28-test result still applies.

## Design decision after run 19484 (implemented in v0.9.2)

Keep the earlier capture modes single-use. Give only the explicit fallback
mode a lifecycle that can size while initialized and safely create again after
its previous generation has been released. Track one active generation with
its token, copied handle and creation-command-list identity; retire that
binding under the lifetime lock before admitting another generation. Reject
stale/duplicate releases, foreign callers/records and reads after Shutdown.
Do not solve the problem by weakening handle checks or making `released`
unconditionally ready across every mode.

Keep diagnostic storage bounded without allowing full ledgers to disable
otherwise valid fallback operations. Preserve fail-closed behavior for actual
identity, callback, output-publication and concurrency failures. Validate the
combined sequence offline before another game launch: create/release/recreate,
different and reused core-record addresses, stale token rejection, repeated
sizing, record exhaustion, shutdown with/without an active handle, and legacy
mode invariance. Depth correction and modern reconstruction stay inactive.

## Run 16380: fallback and sizing limit

The user enabled once, reported a responsive main menu, loaded a save and
reported a normal scene. Create returned one diagnostic token. The installed
producer hook suppressed 29,988 calls with a wrapper through the confirmed
scene interval, with zero unreadable arguments, other callers or Evaluate
calls. Independent read-only sampling verified the exact loaded v0.9.0 DLL and
counter-access instructions; 20 renderer snapshots verified 20 code sequences,
advancing renderer counters 22,727 to 31,304, DLSS requested, backend ready,
2560x1440 dimensions and 1:1 sizing. These asynchronous snapshots are not GPU
completion or image-quality-equivalence proof. No depth correction or modern
SDK was active, and no exception recorder was attached.

Reopening settings produced **21 sizing calls but only 16 successes**.
`capture_bridge_optimal_settings` returns feature-not-supported after the
16-slot budget, before performing caller/input validation. The next five
queries therefore fail by construction. The bridge subsequently recorded a
successful handle release, and the user confirmed the row was greyed out Off
without manually disabling it. This budget is a strong explanation for the
automatic unavailability; a fixed-budget comparison run is still needed.
After release, the wrapper-present count stayed at 34,934 while total producer
suppression continued increasing; renderer reads showed backend-ready 0 and
sizing flags 0. Do not report this as a successful manual toggle or a fully
accepted menu-return lifecycle.

Evidence is in ignored `local/depth-selection-20260922/`, including the
completed trace, `review_16380.py`, `review-16380.json`, and bounded live scalar
samples. Renderer samples are in
`local/driver-61088-20260922/live-16380-taa-fallback-loading.json` and the
corresponding greyed-out snapshot. The trace SHA-256 is
`0CDD8A0ED243648C95435B92EA54D7FF0368A314A38981C2CCF4368DA71C998A`.
Earlier PID 51104 ended its five-minute trace before Create; the user restarted
before the experiment. No acceptance is derived from that session.

## v0.9.1 sizing revision

Only `native-taa-fallback` may continue sizing beyond the 16 retained events.
Later calls use stack-local event state through the same exact caller,
lifecycle, accessor, input, setter/readback and boundary checks. Published
slots are never reused, no parameter pointer is retained, and failures still
fail closed. The earlier capture modes keep their operational call limits.
Aggregate unrecorded-call/success counters distinguish bounded record retention
from successful operation. This does not add handle recreation or modern DLSS.

All 28 CTests pass, including a 21-query regression, retained-event bounds,
legacy limits, and invalid input/accessor rejection. Release `/W4 /WX` builds
without warnings; the final DLL passes the 121-target/16-wrapper CFG audit.
v0.9.1 is 690176 bytes, SHA-256
`FCD958241150B35A16B1CB1B7C4B027AAF0B64670B59F1959A1A2C01DE7A1026`.
Build evidence is under ignored `local/fallback-sizing-20260922/`.
Sizing beyond retained records is accepted in run 19484; repeated lifetime
handling is not. v0.9.1 was installed only after PID 16380
exited and no game process remained. The settings file already matched the
original Off/TAA hash, so it was archived without modification. Exact v0.9.0
was preserved at
`D:/MO2/MHW/mods/dlssTEST/mhw_dlss_bridge_probe.v0.9.0.dll.disabled`.
To roll back with the game closed, copy that file over the active DLL; the
unchanged sidecar remains `native-taa-fallback`. No MO2 restart is required.
`local/fallback-sizing-20260922/staging-091.json` records the deployment.

The run-19484 return to title-screen Options did not keep the setting available.
The reusable-lifecycle work above addresses that next. Keep the depth selector
inactive. Alt-tab, cutscenes, camera cuts,
resizing and device recreation remain untested.

## Implemented and verified offline

`select_depth_backing` selects the populated primary backing for the exact
depth preparation caller, engine request 4, and the captured kind-5 depth
layout. It checks the view-depth identity, excludes the distinct raw-depth
owner, checks class/flags/dimensions/formats, requires primary/native/tracker,
requires the secondary slot to be empty, and rechecks pointer identity. It
performs at most 15 bounded scalar reads, writes no object fields, and does
not invoke the tracking helper itself. Other callers and a supplied non-null
backing are returned unchanged. Rejection means dropping optional work; it
does not authorize forwarding the original null argument.

The selector is built and tested, but **is not wired into the live game yet**.
Its caller will need an accepted rendering fallback. The installed experiment
below verifies that prerequisite without an exception-correction loop.

The original tracking helper at host RVA `0x22927F0` is a self-contained
721-byte function: 191 decoded instructions, internal branches, no external
calls or RIP-relative data access. A local fixture loads those hash-verified
bytes into an owned executable allocation and invokes them only on owned
test objects, records, descriptors and arenas. No game process is involved.
Neither the extracted bytes nor a proprietary executable is in tracked files.

Actual helper execution passes for all three observed same-epoch states:

| Initial engine state / request | Verified owned-memory effect |
| --- | --- |
| 0x4 / 0x4 | Updates the operation without allocating a record. |
| 0x8 / 0x4 | Merges to 0xC, traverses an earlier descriptor's barrier chain, updates the matching backing's barrier and preserves the unrelated node. |
| 0x800 / 0x4 | Appends the usage/barrier record with the selected backing and preserves the previous state. |

This validates CPU bookkeeping in the fixture, not GPU completion, resource
contents, arbitrary tracker layouts or live stability. The helper SHA-256 is
`D27E289F7FDC9F5205E6E8E61615586366A18D0E725235801690A448CDDD8BA2`.

## Runtime experiment: reject optional DLSS work at its producer

Static review of TAA Execute (`+0x2392150`) shows its ordinary TAA dispatch
before the call at `+0x2392F89` to optional producer `+0x23913F0`. With no DLSS
wrapper, that producer returns before dense-velocity/DLSS work is queued.
The ordinary resolve/sharpen and final output code are outside this producer.
The call's return value is not consumed.

In the new explicit mode, DLSS Create must install the producer hook before
the bridge can issue its diagnostic token. Installation requires the existing
exact host/API/Create checks plus SHA-256 checks of the complete producer and
its TAA caller, in executable pages owned by the host image:

| Code contract | Size | SHA-256 |
| --- | --- | --- |
| Producer `+0x23913F0` | 737 | `0FA009DC0FF13AF4F39D5305A9A3392E5274B637230D7A531006A13DCFACB060` |
| TAA Execute `+0x2392150` | 4221 | `D2C6C0F6A5ED39C998BCF62AFED19132E6DD0D7CFBB19E18B80C15531AF2BF91` |

The Windows x64 hook receives the original argument-block pointer in RCX and
suppresses the optional producer only for return address `host+0x2392F8E`.
Other callers use the original trampoline, preserving return/side effects.
The exact caller receives zero (unused), with LastError preserved. Two bounded
scalar reads distinguish a non-null wrapper for telemetry. Unreadable telemetry
also suppresses optional work; it is not interpreted as a null wrapper. No
pointer is retained or AddRef'd, no host data field is written, and no new GPU
command or modern SDK call is added. MinHook changes only the live producer
entry; it does not modify the executable file or an integrity check.

Code/hook rejection refuses the diagnostic Create token before this defective
work path activates. The pinned DLL/hook/trampoline live until process exit;
fixture-only removal is tested with no concurrent caller. The next launch
after restoring the previous DLL/sidecar restores previous behavior.

`bridge.native-taa-fallback` reports installation status and counters at most
12 times, two seconds apart. Status 2 is set before code verification, 3 before
hook installation, and 4 after installation succeeds. Consequently, 2 or 3
can be transient while Create is running; classify rejection only with a
failed Create or a stable terminal status. `with-handle` must increase in the test.
Unrelated callers and unreadable arguments must remain zero for acceptance.
The old reader-only log fields now use `reader-host-hooks/writes` to avoid
claiming that the newly hooked process has no host hook.

The barrier observer is not armed in this mode. One existing mailbox is kept
only to retain an unexpected Evaluate; **zero Evaluate calls are expected**.
The external exception recorder must not be attached. There is no deliberate
terminal fault or budget of context corrections in this experiment.

Static scheduling is evidence for a possible fallback, not proof of a correct
displayed frame. Extra DLSS constants/sizing are still present in this mode;
the live check must establish that the original TAA output remains usable.
The DLSS menu label can say On while the experiment deliberately renders via
the original TAA path. This is not working DLSS/DLAA.

## Verification and local deployment

Release builds use `/W4 /WX`. All **28 CTests** pass. The real owned MinHook
fixture checks exact-caller suppression, unreadable arguments, unchanged
unrelated calls/return/LastError, no producer invocation or object-field write,
and clean removal. Selector tests reject each failed read, missing backing,
wrong layout/identity/request and changed pointers. The separate original-helper
execution described above passes. The DLL CFG audit has **121 targets** and
16 distinct verified wrappers including the new producer hook.

Bridge v0.9.0: 688640 bytes, SHA-256
`2AC65AE0760FED1BFDD5C79961A0B7EE15EDAD2B4FC450BBCC3718D7109C1763`.
The host remains version 0.0.0.0, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`;
the analysis capture remains
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.

Evidence, source snapshots and guarded staging are under ignored
`local/depth-selection-20260922/`. Preserve the exact active v0.8.1 DLL as
`D:/MO2/MHW/mods/dlssTEST/mhw_dlss_bridge_probe.v0.8.1.dll.disabled` and preserve
the old mode as `mhw_dlss_bridge_probe.mode.before-090.disabled` before deploying.
The new sidecar is exactly ASCII `native-taa-fallback` plus LF. The existing
MO2 environment remains compatible; no MO2 restart or game settings edit is
required. To roll back with the game closed, restore both preserved files to
their original active names. Do not attach the v0.7.1 recorder in the new mode.

## Next run and decisions

Launch the same MO2 test entry offline, with DLSS Off/TAA On, and stay at the
main menu. Verify v0.9.0 startup and the explicit mode before enabling DLSS.
Enable once, close its popup, leave Options, and stay at the main menu for
about 30 seconds while replying by phone.

- Normal image and responsive game, successful diagnostic Create, installed
  hook, increasing suppression with a handle, no unreadable/other callers and
  no Evaluate: accept this fallback at the main menu. Then test a loaded scene
  in the same process before calling it a useful scene fallback.
- Create rejected: inspect the code/hook status locally; do not retry the same
  build hoping for another tracker state.
- Black/invalid image or instability: the fallback assumption is unaccepted;
  retain evidence and revise it before enabling the backing correction.

Once fallback behavior is accepted, integrate the selector into the specific
preparation path while retaining the original helper and suppressing rejected
optional work. Resource-content/frame-alignment and GPU validation still
precede modern temporal reconstruction.
