# Modern provider bridge: offline tests

## Latest result: bare-profile external observation completed

[Run 2632](../research/jitter-bare-profile.md) uses the regular game entry,
zero enabled MO2 mods and no detected bridge/mod-loader DLL. All 64 menu/scene
attempts retain jitter-enable 0 and zero stored jitter; 11 focused attempts
per phase meet combined consistency. No hook or rendering admission was added.
Normal exit/process absence and unchanged Off/TAA/profile settings are verified.

An earlier empty-profile launch, 34948, inherited the test entry's Force Load
rule and is retained as a separate auxiliary observation. Empty mod lists do
not disable that rule; verify modules before calling a run bridge-free. Run
2632 has no bridge log/deadline or instrumented Shutdown. Nothing needs restoring;
no repeated unchanged launch is requested. Next is native assignment/callback
observation, with GPU/raster and modern integration still unverified/inactive.

## Latest result: disabled-provider Off/TAA control completed

[Run 8164](../research/jitter-startup-control.md#run-8164-menu-and-gameplay-remain-zero-in-the-control)
retains a disabled jitter flag/zero stored jitter in both menu and gameplay,
with zero synthetic provider routes and no backend context. Active startup
substitution is not required for that state. All four 16-attempt sample sets
are saved; focused consistency passes 12 menu and 8 gameplay attempts.
The 30-second passive trace ends before these external reads, so it verifies
startup mode rather than continuous runtime coverage or Shutdown.

The user exited normally, process absence/settings/DLL/mod list/profile checks
pass, and the original `native-depth-probe` sidecar is restored. Archive replay
passes. No unchanged rerun is needed; trace the jitter assignment or isolate
remaining mod/observer effects before choosing another runtime diagnostic.

## Completed procedure: disabled-provider Off/TAA control

The [startup map and control procedure](../research/jitter-startup-control.md)
uses the unchanged v0.15.0 binary with only its sidecar changed to select the
existing disabled fallback. Keep the same offline MO2 entry, mods, save and
Off/TAA settings. This comparison uses external read-only readers: do not arm
the active-mode recorder or enable DLSS. Verify the disabled startup trace and
loaded module before sampling. The original sidecar is backed up and must be
restored after the control exits. The completed run and restoration are recorded
above. This passive mode's trace lasts 30 seconds, not five minutes.

## Latest result: pre-enable Off/TAA observations archived

[Run 5836](../research/jitter-pre-enable-observation.md) used the unchanged
v0.15.0 DLL and external read-only samples with DLSS left Off. The TAA flag was
already disabled and stored jitter zero in menu and gameplay before any
Create/Evaluate or native depth/fallback installation. Startup bridge callbacks
still executed; this is not a vanilla baseline. Eleven focused gameplay
attempts retain matching counters/epoch and stable reads, without establishing
ownership or raster behavior.

The user exited normally and the process is absent. Settings and deployment
identities are unchanged. The five-minute trace closed before exit: Shutdown
and final cleanup are unobserved, not accepted. Archive replay passes. No new
launch is requested by this result. Prepare a separate control isolating bridge
startup substitution before requesting another otherwise identical Off/TAA run.

## Previous result: v0.15.0 menu and loaded-scene CPU samples accepted

[Run 27200](../research/jitter-scene-probe.md#run-27200-menu-and-loaded-scene-cpu-samples-accepted)
completed both 16-sample phases. Gameplay used a different TAA layer, but
jitter stayed zero with `mJitterEnable=0`. The one-Evaluate limit held and
the journal stopped on completion with flags 0. Its overlapping checkpoint
remains unqualified; GPU/raster correctness is not accepted.

The user saw DLSS greyed out after returning to the menu. The trace records
a clean Release followed by a refused Create: `native-depth-probe` requires
the single-use barrier observer, which cannot rearm. No second handle was
published. The user exited normally; cleanup and unchanged saved Off/TAA
settings are verified. No unchanged rerun is needed. The later pre-enable
observation is recorded above; the exact jitter-enable assignment remains open.

## Completed procedure: v0.15.0 scene-triggered jitter sequence

[v0.15.0](../research/jitter-scene-probe.md) captures 16 initial CPU samples,
then waits for an explicit event before taking 16 more. All 48 native and 78
Python tests, binary audits and hardware checks pass; CPU runtime acceptance
is recorded above. The one-Evaluate limit and exact-build gates remain unchanged.

Start offline at the menu with DLSS Off and wait for preflight. Enable once
and apply; remain at the menu while the first capture is saved. Then, when
instructed, load a save and confirm from the phone while staying in the scene.
The operator triggers the second burst after that confirmation. After its
evidence is saved, return to the menu, turn Off/apply if selectable and exit
normally. If greyed out, leave the setting alone and exit.
The three-second Off wait belongs to v0.14 below, not this test. No MO2 restart
or external debugger is needed. See the linked note for the bounded deadline,
trigger receipt/acknowledgement and rollback procedure.

## Latest result: v0.14.0 bounded jitter sequence accepted

[Run 30924](../research/jitter-sequence-probe.md#run-30924-both-cpu-phases-accepted)
captures all 16 samples per wrapper phase, with zero stored jitter and
`mJitterEnable=0` throughout. Every other captured predicate clause passes.
One Evaluate/completion stop, clean Off/apply/exit and unchanged settings are
verified. The user was unsure about the three-second wait, but the trace proves
complete second-phase coverage. No unchanged menu rerun is needed.

Next is offline scene/property tracing and a bounded loaded-scene CPU
comparison; prepare an observation point or explicit trigger before launch.
The current sampler fills its present-phase budget at the menu. GPU/raster
qualification and modern SDK integration remain unverified/inactive.

## Completed procedure: v0.14.0 bounded jitter sequence

[v0.14.0](../research/jitter-sequence-probe.md) adds fixed CPU sample budgets
for native jitter gates/projections with the wrapper present and absent. All
47 native and 64 Python tests, binary/hardware checks and independent fixture
review pass. Runtime acceptance is recorded above. The one-Evaluate budget remains.

After staging is verified, launch the same offline MO2 entry with DLSS Off/TAA
and wait at the main menu for preflight. Enable once and leave Options to apply;
stay at the main menu while the channels and initial sequence are saved. Once
instructed, turn Off, leave Options and **wait at least three seconds at the
main menu before exiting normally**. If unavailable, leave the setting alone
and exit. Archive the final trace for the second phase. No MO2 restart or
external recorder is required. The linked note records bounds and rollback.

## Latest result: v0.13.1 CPU projection observation accepted

[Run 21636](../research/camera-type-probe.md#run-21636-cpu-projection-observation-accepted)
captures the derived camera, matching source epoch/viewport and both CPU
projection matrices. Stored jitter is zero and the matrices are identical,
despite nonzero table candidates. One guarded Evaluate and a 627-record
completion stop with flags 0 succeed. The user reports a brief freeze, then
Off/apply/exit; cleanup and original settings are verified. The unchanged
build retains 45 native and 52 Python tests plus binary/hardware checks.

The game is closed. Next is a bounded observation of all native jitter gates
and multiple CPU frames at the existing producer hook, without increasing the
GPU work budget. That recorder is implemented in v0.14.0 above; do not request an
unchanged rerun. Shader execution, nonzero raster jitter and temporal contents
remain unverified. The linked note retains evidence and rollback.

## Completed procedure: v0.13.1 camera type correction

[v0.13.1](../research/camera-type-probe.md) accepts the verified derived camera
layout and captures the actual vtable/rejection reason. Run 41776 completed
the existing capture and clean exit, but its optional projection observation
rejected before reading matrices. All 45 native and 52 Python tests pass for
the correction, plus binary/hardware and independent fixture review. Runtime
projection acceptance is recorded above.

After deployment verification, use the same offline MO2 test entry with DLSS
Off/TAA. Wait at the main menu for v0.13.1/producer-1301 preflight; then enable
once and leave Options to apply. Stay at the main menu and reply from the phone.
Save/review the capture before Off/apply/exit. No MO2 restart or external recorder
is required. The linked note records exact identities, limits and rollback.

## Previous test: v0.13.0 camera jitter and projection capture

[v0.13.0](../research/projection-jitter-probe.md) is built and verified with
45 native tests, 48 Python tests, matrix machine-code replay and binary/hardware
audits. Run 41776 retained the existing diagnostic but rejected the projection
snapshot at checkpoint 2, motivating the correction above. It adds an optional CPU projection
snapshot to the existing one-producer/one-Evaluate experiment. Modern DLSS is
inactive and no additional hook or GPU command is introduced.

After staging is verified, launch the same offline MO2 test entry with DLSS Off
and TAA. Wait at the main menu for exact-code/transport preflight, then enable
once and leave Options to apply. Stay at the main menu and use the phone to
reply. Save and review the capture before Off/apply/exit; no MO2 restart or
external recorder is needed. The linked note records identities, procedure,
evidence paths, limits and rollback. Projection agreement alone does not qualify
shader execution, raster jitter or temporal image contents.

## Latest result: v0.12.0 publication receipt and recorder stop accepted

[Run 4768](../research/publication-receipt-probe.md#run-4768-publication-binding-and-completion-stop-accepted)
binds the original producer's published descriptor/payload through Evaluate
while counters/bank advance. Four preparations and one correction/consumer
complete without guard rejection. The recorder stops on the matching fence
observation and drains 518 records with flags 0. The user reported a brief
freeze, turned Off/applied/exited normally; release, shutdown, cleanup and
original Off/TAA settings are verified. The unchanged code retains its 43
native and 42 Python tests, plus binary/assembly and real D3D12 checks.
Shader jitter, image alignment and full GPU qualification remain open. The
new v0.13.0 input-validation diagnostic is described above; no unchanged rerun
is needed. The linked note retains the exact procedure, evidence and rollback.

## Latest result: v0.11.0 frame comparison rejected

[Run 17204](../research/frame-packet-probe.md#run-17204-producerpreparation-counter-mismatch)
captured stable producer/preparation counters 24291 and 24292, triggering the
new frame guard before Evaluate. The user reported a brief freeze, then turned
Off and exited normally; release, shutdown and original settings are verified.
The journal has no Evaluate marker and retains overflow. The completed
[offline queue trace](../research/queued-work-provenance.md) identifies a
producer-return descriptor/payload link and verifies it with 96 owned enqueue
replays. Its publication receipt is implemented in v0.12.0 above without a
permitted +1 offset. The historical v0.11.0 build passed 38 CTests and 37 Python
tests, plus hardware completion and binary audits.

## Current result: v0.10.0 guarded CPU path accepted

[Run 47820](../research/guarded-depth-probe.md#run-47820-one-corrected-item-recovery-and-clean-exit)
completed one corrected item and Evaluate capture with no guard rejection.
The user reported a brief freeze, then turned DLSS Off and exited normally.
Release and both Shutdown calls succeeded; settings match original Off/TAA.
GPU ordering and temporal inputs remain unverified; the journal overflowed.
The [offline follow-up](../research/submission-order-and-temporal-inputs.md)
reconstructs expected declarations and a matching fence observation while
retaining those coverage flags. The next frame/jitter and bounded-completion
capture is now implemented in v0.11.0 above. No unchanged v0.10.0 rerun is requested.

[v0.10.0](../research/guarded-depth-probe.md) passes all 33 tests, the original
tracking-helper replay and CFG/assembly audits. The completed procedure follows.
The `native-depth-probe` sidecar enables at most one optional work item and
one Evaluate capture, with preparation/consumer rejection and later native
TAA suppression. Keep DLSS Off/TAA for startup, launch the same offline MO2
entry, and wait at the main menu for verification. No recorder or MO2 restart
is required. After verification, enable once and leave Options to apply;
stay at the main menu without loading a save or toggling again. Inspect the
versioned stage/checkpoint, depth-correction, consumer and Evaluate evidence
before further actions. No intentional terminal crash is part of this test.
The linked note records exact identities, limits, acceptance and rollback.

## Current result: v0.9.2 recreation and menu toggle accepted

[Run 45100](../research/native-taa-fallback.md#run-45100-recreation-and-menu-toggle-accepted)
completed the requested save/menu return and deliberate Off/On cycle with
DLSS still selectable On. Evidence shows three generations, three releases,
34/34 successful sizing queries and no unexpected Evaluate/handle-read fault.
The user exited normally with the exact original Off/TAA settings. The game
is closed; no unchanged fallback relaunch is needed. The five-minute trace
ended before the third generation, which was checked with exact-build,
read-only scalar samples. Shutdown wrapper returns were outside capture.

The accepted procedure below is retained for reference. Wider display/device
transitions remain untested. The next implementation milestone is guarded
depth selection and bounded producer/Evaluate work with this fallback.

## Preparation and procedure used for run 45100

[v0.9.2](../research/native-taa-fallback.md#v092-reusable-fallback-lifetimes)
passes all 29 CTests, including 256 handle generations, stale/malformed records,
full ledgers, and cleanup racing a bounded record read. Release retires the
current binding; the next validated Create can publish a fresh generation.
The tested recreation and menu/save sequence is accepted above.

After the guarded staging manifest confirms v0.9.2, use the same **Monster
Hunter: World - DLSS 1.2.110 test** MO2 entry offline. MO2 need not restart.
Start with DLSS Off/TAA, reach the main menu and wait for the loaded-build and
startup checks. No external exception recorder is required. Then enable once,
leave Options, load a save and return to title-screen Options. Inspect before
changing it again. If selectable, exercise Off/apply and On/apply once more.
Record the observed setting and match it to Create/Release generations and
sizing counters before claiming success. If greyed out, preserve that state
for inspection. Use the phone during this test to avoid mixing in Alt-Tab.
Normal exit and Off settings are also acceptance checks.

The exact v0.9.1 DLL is preserved during staging under its versioned disabled
name. Full identities, rollback and evidence are in the linked note. The
depth selector and modern SDK remain inactive; this renders through native TAA.

## Earlier result: v0.9.1 sizing fix verified; lifecycle remained single-use

[Run 19484](../research/native-taa-fallback.md) verifies 20 successful sizing
queries, four beyond the 16 retained records. The user reported a normal scene,
but DLSS was still greyed out Off on returning to Options. Automatic Release
left the single-use bridge unable to size again. The offline replay reproduces
the two later rejections and identifies Create/handle-binding/ledger constraints
that must be fixed together. The user exited normally; settings remain Off/TAA.
This prompted the reusable-lifecycle change above. The procedures below
describe older builds.

## Earlier result: run 17772

[Run 17772](../research/current-state.md) stopped before applying any correction:
the tracker was in the previously observed engine state 0x8, outside this
three-frame test's admitted states. All Evaluate channels stayed idle, so the
v0.8.1 checkpoint review was not exercised. Logs/settings are archived and DLSS
is restored Off. Keep the game closed for offline backing-selection/fallback
work. Do not repeat the unchanged test. The preparation and earlier procedures
below are historical.

## Current preparation: v0.8.1 / recorder v0.7.1

[Stopped-process checkpoint review](../research/scoped-checkpoint-review.md) admits only proven unrelated overlap, retains the original incomplete snapshot, and rejects captured-resource or unknown overlap. All 26 tests, the exact run-49944 replay, owned three-correction debugger fixture, RTX 5090 continuation and CFG audit pass. Game acceptance is pending. Use the same MO2 entry with DLSS Off; reach the main menu and wait for ARMED from the matching v0.7.1 recorder. MO2 need not restart. Earlier results below are historical.

## Run 49944 stopped at an incomplete checkpoint

[Run 49944](../research/multi-evaluate-first-result.md) saved one Evaluate and 68 natural fence queries. One correction was applied; the second was rejected because an unrelated parallel barrier crossed the global checkpoint. The captured submission's GPU completion remains unverified. Evidence is preserved and DLSS is restored Off. This prompted the checkpoint revision prepared above. Earlier status/procedures below are historical.

## Accepted run 46868: three corrections and bounded stop

[Run 46868](../research/three-frame-result.md) accepted corrections on renderer
counters 18057/18058/18059, followed by normal handling of the fourth fault when
the budget was spent. The first Evaluate snapshot is complete; all three CPU
intervals contain closed/submitted depth recordings with continuous declarations
ending at combined shader read `0xC0`. The journal has 807 completed calls, clean
coverage flags and immutable prefixes. GPU completion and contents remain unverified.

The subsequent diagnostic was [v0.8.0 with recorder v0.7.0](../research/multi-evaluate-fence-capture.md): three independent Evaluate captures plus natural fence queries/CPU signals, with the same correction and time bounds. Preparation, exact identities, validation, opt-in and rollback are in that note. Its first game result is summarized above. MO2 can stay open. Procedures below are historical.

## Recorder v0.6.1 prepared: same bounded test, another verified branch

[Run 7148](../research/read-ready-depth.md) captured the fault but applied zero
corrections: engine state was already `0x4`, while v0.6.0 admitted only `0x800`.
The new recorder adds two exact live byte checks and accepts these two states
with matching tracker/rechecks. All 25 local tests pass; game acceptance is pending.
The installed v0.7.9 bridge stays unchanged, and graphics settings are restored
to DLSS Off/TAA/DX12 On/FullScreen/2560x1440.

Use the same MO2 entry and wait at the main menu with DLSS Off. Arm archived
recorder **v0.6.1** using `--repair-depth-three-frames`; wait for ARMED before
enabling. Three corrections maximum, consecutive counters, stable identities,
500 ms admission window and the original journal bounds remain required.
Another crash is expected. Full evidence, limitations and run steps are in the
linked note. Procedures below are historical.

## v0.7.9 prepared: three consecutive frames maximum

The [bounded-frame diagnostic](../research/bounded-depth-frames.md) corrects
combined-read reporting and pairs bridge v0.7.9 with recorder v0.6.0. All 25
tests and the CFG audit pass. The owned-process test verifies three corrections,
then normal handling of the fourth fault. **MHW runtime acceptance is pending.**

Launch the same **Monster Hunter: World - DLSS 1.2.110 test** MO2 entry offline,
reach the main menu with DLSS Off, and wait for ARMED. MO2 can remain open.
The recorder must use the explicit `--repair-depth-three-frames` option; it
requires consecutive counters, stable depth identities, fresh tracker/code
checks, clean capture channels and a 500 ms correction window. The original
two-second journal bound and first-Evaluate-only capture remain. Another crash
is expected after the budget or an invariant stops further corrections.

The exact v0.7.8 DLL is preserved beside the active DLL under its versioned
disabled name. Saved graphics settings are unchanged. Full contracts, local
artifacts, acceptance criteria and rollback are in the research note above.
All sections below are historical results and procedures.

## v0.7.8 result: corrected frame submitted with depth read declaration

The [post-Evaluate journal](../research/post-evaluate-journal.md) preserves the
first snapshot and appends subsequent completed D3D12 calls within the original
two-second bound. Recorder v0.5.0 saves both channels before forwarding the
next fault. All 25 tests, the hardware continuation fixture and CFG audit pass.
**Run 24944 accepted the current depth producer and Evaluate submissions**, with
387 completed calls and 77 calls after the Evaluate checkpoint. The correction
budget remains one.

The corrected frame is 29981. Both relevant lists were closed and submitted
after their current Resets and before their next Resets. The depth declaration
chain ends at combined shader read `0xC0`, including the required non-pixel bit.
The summary's exact-equality `expected_state_matches=false` is a reporting
limitation, not evidence of an absent read transition. GPU completion, resource
contents and temporal alignment remain unverified. The next frame faulted after
the one correction was spent, and the recorder preserved normal exception handling.

**Keep the game closed; do not repeat the unchanged one-shot run.** Next work
is read-state reporting and bounded repeated-frame validation of the narrow
depth-argument correction. No repeated correction is enabled. MO2 PID 7112 may
stay open for a later diagnostic using the same **Monster Hunter: World - DLSS
1.2.110 test** entry.

The previous v0.7.7 DLL is preserved at its versioned disabled path. Saved-On
settings were backed up after confirmed game exit, then only DLSS was reset Off;
TAA, DX12 On and FullScreen 2560x1440 remain saved. Local evidence and the
independent review are in `local/post-evaluate-journal-20260922/`.
The research note records the protocol, acceptance requirements and rollback.
The sections below describe completed earlier runs.

## v0.7.7 result: complete capture preserved, later submission still needed

The [capture mailbox](../research/capture-mailbox.md) preserves the completed
first-Evaluate snapshot independently of the bridge log worker. External
recorder v0.4.0 opens it read-only before attaching and flushes the payload to
its JSONL file before forwarding the next fault. **Run 38476 passed this
transport and the existing contract/resource/CPU timeline checks.** All 24
tests and the CFG audit pass. The correction budget remains one.

The recorder saved 15193 bytes before forwarding the next matching fault,
16 ms after the corrected fault. Both the mailbox and text log contain the
first Evaluate, bound to corrected frame 20683. Two depth transitions to
COPY_DEST were recorded on other lists, but all recorded submissions precede
those lists' current Resets. Depth's state at use remains unverified.

**Keep the game closed; do not repeat this unchanged capture.** Next work is
bounded observation beyond Evaluate through relevant list closes/submissions
and queue synchronization, preserving evidence before the next fault. That
continuation has not yet been implemented. The completed test procedure is
retained in the research note. MO2 PID 7112 can remain open for a later test
using the same **Monster Hunter: World - DLSS 1.2.110 test** entry.

The previous v0.7.6 DLL and exact saved-On graphics settings are preserved.
After confirmed game exit, only DLSS was reset Off; TAA, DX12 On and
FullScreen 2560x1440 remain saved. Evidence and the independent review are under
`local/capture-mailbox-20260922/`; the research note gives schema, hashes,
acceptance requirements and rollback. Historical results follow.

## v0.7.6 result: tracker accepted, queue timeline unavailable

The [depth-state diagnostic](../research/depth-state-timeline.md) was staged in
MO2 with the exact v0.7.5 DLL preserved. All 23 tests, WARP/hardware queue
fixtures and the CFG audit pass. **Run 4568 accepted the external tracker
snapshot and one-use retry.** Depth's active state was engine `0x8` at the
fault, selecting the captured helper's read-mask merge branch for request
`0x4`. Both 105-read work-item and 32-read tracker snapshots completed twice.
The next uncorrected frame faulted 16 ms later and the recorder detached cleanly.

The bridge log ends after Create. No finished Evaluate, resource inspection or
queue timeline was persisted, so none is accepted for this run. **Do not repeat
the unchanged capture.** The next diagnostic needs capture storage independent
of the worker's text-log scheduling before another launch is requested.

The saved-On configuration was backed up after game exit, then only DLSS was
reset Off. Settings remain TAA, DX12 On, FullScreen 2560x1440. The exact settings,
new binaries, sources, tests, audit and staging manifest are preserved in
`local/depth-timeline-20260922/`. See the research note for the hook contract,
hashes, acceptance criteria and rollback. Sections below are historical.

## Accepted v0.7.5 result: recorded resource transitions

The [barrier recording diagnostic](../research/evaluate-barrier-capture.md)
was staged in MO2 with the exact v0.7.4 DLL preserved. It passed all 21 tests
plus a hardware-adapter fixture and CFG audit.
It observes ResourceBarrier, Reset, Close and ExecuteBundle between admitted
DLSS Create and the first Evaluate. No GPU commands are added. Its two-second
capture has fixed call/description limits; any gap prevents declaration acceptance.
This records CPU declarations, not completed GPU states or valid texture contents.

**Run 30272** captured 232 completed API calls and 705 barrier descriptions
without capture gaps. In the Evaluate command list's selected recording,
color and motion reached declared `0x40` shader-read state; output reached
declared `0x8` unordered-access state. The final preparation flush supplied
motion's transition and an output UAV barrier. Depth had no matching barrier
in that recording, so its state remains unknown. See the research note for the
exact evidence, limitations and next depth-tracker investigation.

The next uncorrected frame crashed 32 ms later after the one-use correction
was spent. Both logs are preserved; saved DLSS was reset to **Off** after the
game closed, retaining TAA, DX12 and FullScreen 2560x1440. **Do not repeat this
unchanged capture.** Version 0.7.6 now supersedes it; exact v0.7.5 and v0.7.4
backups are retained.

## Accepted v0.7.4 result: resource descriptions

The [resource-description diagnostic](../research/evaluate-resource-descriptions.md)
completed in **run 14392**. All four textures were 2560x1440 on the command
list's device: color/output R11G11B10_FLOAT, motion R32G32_FLOAT and depth
R32_FLOAT. All ten temporary references were released without fault. The unique
matching swap-chain creation used B8G8R8A8_UNORM; current Present, resize and
colorspace remain unobserved. All 16 tests and the CFG audit pass.

The first corrected frame reached Evaluate; the next frame hit the same depth
fault 31 ms later, after the single retry was spent. The recorder detached and
the game crashed. Both logs and the exact post-crash settings are preserved.
Saved DLSS is now **Off**, with TAA, DX12 and FullScreen 2560x1440 retained.
**Do not repeat this capture.** Next work is actual resource-state/barrier and
depth/motion/jitter validation. Reconstruction and full lifecycle remain open.

That run used v0.7.4, SHA-256
`FA743BE800A67E646FB4145C23EB8E90D53169D86394D5C13F514F45E54006C0`;
the exact v0.7.3 DLL is preserved under its versioned disabled name. The same
MO2 entry and environment apply. See the linked research note for the current
deployment history, recovery procedure and evidence hashes. Sections below describe
earlier versions and their historical test procedures.

## Accepted v0.7.3 result: first Evaluate capture

Version 0.7.3 [revalidates the updated driver and Windows resolver](../research/driver-61088-revalidation.md).
It preserves v0.7.2's read-only temporal observations and one-Evaluate mode.
The exact supported environment is now NVIDIA 610.88 and Windows system DLL
version 10.0.26100.9278. All 14 tests and the binary CFG audit pass. Live runs
verified startup identities, but **run 44972 crashed before Evaluate** in the
game's depth-resource preparation. See the
[TAA activation and crash evidence](../research/taa-activation-depth-crash.md).
Run 12196 subsequently reached Evaluate with one external argument correction,
as described below. Rendering and full lifecycle acceptance remain outstanding.

MO2 PID 7112 has verified `capture-one-evaluate` mode and can stay open. The
`dlss` profile's **Monster Hunter: World - DLSS 1.2.110 test** entry enables the
new bridge DLL. The post-crash settings are DLSS **Off**, **TAA**, DX12 On,
FullScreen, 2560x1440. The exact post-crash settings were backed up before
resetting only saved DLSS from On to Off. The prior Windowed configuration and
exact active v0.7.2 DLL were also backed up before staging.

Run 8280 completed the **DLSS Off / TAA On baseline**. The subsequent
[external exception capture in run 14388](../research/depth-exception-capture.md)
confirmed the actual failing work item: its kind-5 depth owner has a primary
backing, while host preparation dereferences its null secondary backing.
All 105 scalar reads completed and the recorder detached successfully.
This captured payload mismatch is accepted.

The external recorder v0.2.0's
[one-use depth argument correction](../research/depth-argument-retry.md)
was accepted in **run 12196**. Frame 32186 reached the first complete Evaluate
capture: all 17 getters completed without fault, and the four non-null resource
pointers matched the corrected work item. Frame 32187 encountered the same
uncorrected fault; the exhausted one-use tool detached and the game crashed.
No Release/Shutdown was captured. All 15 tests and an offline replay pass.

Both logs and the post-crash settings are preserved. Only saved DLSS was reset
to Off after the game closed; TAA remains selected. **Do not repeat this run.**
The next step at that point was resource-description/state validation at the
confirmed Evaluate boundary. No permanent rendering fix was installed. The
then-active DLL was v0.7.3; v0.7.4 now supersedes it as described above. The
bridge worker still has a five-minute window from startup.

Require version 0.7.3, verified system/NGX identities, complete two-provider
startup, successful Create and the temporal contract. Inspect
`bridge.temporal-snapshot` and `bridge.temporal-summary` with the renderer
samples, sizing counts and clean Release/Shutdown. Shader-cache IDs are
historical observations, not per-frame execution counts. Evaluate remains
subject to the [one-Evaluate contract](../research/one-evaluate-capture.md).

Staged DLL: 374,272 bytes, SHA-256
`1F2309021E4DF15D3645C0B7C4A264F0B40D02394438DE1B3F6C65EEE1D29552`.
All 15 distinct exported wrappers/callbacks are in the 104-target CFG table.
See `local/driver-61088-20260922/staging-073.json` for deployment and backups.

To remove the experiment, close MHW and disable its bridge Force Load entry.
To restore the previous DLL, first preserve v0.7.3 under a versioned disabled
name, verify `mhw_dlss_bridge_probe.v0.7.2.dll.disabled` against SHA-256
`310014817EBF4C634525921D67911522682A7BD171ECA60EA33EBBD993B09753`,
and copy it to the active DLL name in the same mod. That older build rejects
the current driver/Windows identities, so it is an archival rollback rather
than a working capture setup on this environment. The settings backup
`local/driver-61088-20260922/graphics_option.before-073.ini` restores Windowed
and retains saved DLSS Off. No original executable or asset needs restoration.

## Historical v0.7.2 temporal path test

Version 0.7.2 adds [read-only shader-cache and deferral observations](../research/temporal-activation-observation.md)
before and after Create. This tests historical progress through the temporal
path while preserving the successful v0.7.1 renderer observations. It does not
count pass executions or prove an enqueue completed. All 14 tests pass; live
acceptance is pending.

MO2 PID 19460 still has the correct `capture-one-evaluate` mode and may stay
open. Use the same offline **Monster Hunter: World - DLSS 1.2.110 test** entry.
The setup preserves the previous DLL and graphics configuration, and changes
only `NVIDIA DLSS` from On to **Off** to avoid spending the one-shot handle
during startup. Keep the existing full screen 2560x1440 configuration.

1. Launch and reach the main menu with DLSS Off.
2. Enable DLSS once, close the popup and leave Options to apply it.
3. Load one save, remain in the scene about 10 seconds, then exit normally.
   Returning to the main menu to switch Off is optional; DLSS cannot be changed
   in the in-game menu. Do not turn it Off and back On in the same process.
4. If it starts On or greyed Off, or the picture prevents navigation, stop
   after that attempt and preserve the log. The intentional Evaluate failure
   can produce an incomplete frame if that call is reached.

Require v0.7.2, verified identities and temporal contract, a valid pre-Create
baseline where available, valid later samples, and the existing Create/Release/
Shutdown checks. Inspect `bridge.temporal-snapshot` and `bridge.temporal-summary`
alongside renderer snapshots, final sizing counts and lifecycle events. Cache
IDs can predate Create and an unchanged sentinel can mean failed lookup, so
neither is a per-frame execution count. Inspect the run before requesting any
repeat. Evaluate, if reached, retains the [one-Evaluate acceptance contract](../research/one-evaluate-capture.md).

Release v0.7.2: 374,272 bytes, SHA-256
`310014817EBF4C634525921D67911522682A7BD171ECA60EA33EBBD993B09753`.
All 15 distinct exported wrappers/callbacks are in the 104-target CFG table.
Deployment, backups and settings hashes are recorded in
`local/temporal-activation-20260908/staging-072.json`.

With MHW closed, roll back by preserving v0.7.2 under a versioned `.disabled`
name, verifying `mhw_dlss_bridge_probe.v0.7.1.dll.disabled` against SHA-256
`BAA156066CB3D555023EEE53378ACF29DB08D9FC5F318C71035A6136AEF654C2`,
then copying it to the active DLL name in the same MO2 mod. MO2 uses the same
mode. The pre-test graphics configuration is preserved in the local staging
folder; restoring it also restores saved DLSS On, so do not use that state
as an Off-start diagnostic baseline. To remove the experiment, close MHW and
disable its bridge Force Load entry. No executable or original asset was patched.

## Accepted v0.7.1 result: renderer gates ready

Run **43380** reached a loaded save, as confirmed by the user, and exited
without returning to the main menu. It captured 12 valid samples with all
selector gates ready over a 23.2-second token lifetime. All 11 sizing requests
succeeded with no budget exhaustion, and Release plus both Shutdowns completed
cleanly on exit. **Evaluate remains absent.** No rerun is needed to verify
cleanup. See the [latest evidence](../research/renderer-gate-observation.md#run-43380-ready-gates-through-a-longer-lifetime).

The next investigation is temporal-layer activation and the actual execution/
enqueue path. **Do not repeat the gate test yet.** Settings are now saved
**On**; establish a known Off baseline when a future one-shot test is prepared.
The DLL and game configuration were not modified during the review.

## Historical v0.7.1 gate test procedure

The [renderer gate diagnostic](../research/renderer-gate-observation.md) adds
read-only samples after successful Create. The previous loaded-scene attempts
recorded no Evaluate. This run checks the live selector flags and renderer
frame index; it does not count temporal-pass execution. The existing 1:1 sizing
and one-Evaluate provider behavior remain in place.

Run **35100** started from saved On, reached successful Create and two clean
samples with all selector gates ready, then released the handle after 2.6 s.
The one-shot probe rejected later sizing requests and the user saw greyed Off.
Both Shutdowns passed; no Evaluate was recorded. Its exact early Release reason
is unknown. See the [accepted observation](../research/renderer-gate-observation.md#first-live-renderer-observation-run-35100).
The settings file was then **Off**. Run 43380 used that starting state.

MO2 PID 19460 is still open with `capture-one-evaluate`, as verified by the
previous logs. It can stay open; no new launch command is needed. If MO2 has
since been closed, use the process-scoped launch command in the historical
section below. Do not start another instance alongside an existing one.

1. Launch the same **Monster Hunter: World - DLSS 1.2.110 test** entry offline.
   Keep full screen 2560x1440 and the other graphics settings unchanged.
2. Start with saved DLSS **Off**. Once at the main menu, enable it once, close
   the popup, leave Options to apply it, and load a save once. Do not turn it
   Off and back On in the same process. If it is already On or greyed Off at
   first inspection, report that state and exit instead of continuing the
   scene test with an uncertain initial lifetime.
3. Stay in the loaded scene for about 10 seconds, then return to the main
   menu. Change DLSS **Off there**, leave Options to apply it, and exit
   normally. DLSS cannot be changed from the in-game menu.
4. Keep the attempt brief: sampling permits 64 observations, spaced at least
   2 seconds apart after Create. If the game crashes or the picture prevents
   navigation, exit and preserve the log rather than retrying.

Require version `0.7.1`, recognized mode, complete two-provider startup and a
successful DLSS Create. Inspect `bridge.renderer-snapshot` events and the final
`bridge.renderer-summary`: a verified contract, multiple valid samples and no
read/validation stop are needed before interpreting flags. The first and last
valid frame indices may differ without creating another event. A blocked mask
identifies sampled selector conditions; all gates ready does not prove the
temporal pass executed. A single asynchronous sample can be transient.

Require verified copied-handle Release before both successful provider Shutdowns,
no outstanding Init/token, no faults or contention, and a quiesced complete
trace. Check both final provider checkpoints for sizing-budget exhaustion.
Evaluate may still be absent. If reached, assess the
[one-Evaluate contract](../research/one-evaluate-capture.md); its unsupported
result is deliberate. MHW ignores this result, so an incomplete or stale frame
is possible while On. Modern DLSS rendering remains unimplemented.

Release v0.7.1: 356,352 bytes, SHA-256
`BAA156066CB3D555023EEE53378ACF29DB08D9FC5F318C71035A6136AEF654C2`.
All 13 tests pass; all 15 distinct exported provider wrappers/callbacks are in
the CFG table (104 targets). Renderer gate observation and exit cleanup passed
in run 43380, which reached a loaded save; Evaluate remains pending. Local deployment is
recorded in `local/renderer-gates-20260907/staging-071.json`.

The active DLL is in the separate MO2 `dlssTEST` mod. To roll back with MHW
closed, preserve v0.7.1 under a versioned `.disabled` name, verify
`mhw_dlss_bridge_probe.v0.7.0.dll.disabled` against SHA-256
`14E343D0A023270EA786F5FF3D5584EC58632CBB88727BAD3B902F53C56CD90F`,
then copy that backup to `mhw_dlss_bridge_probe.dll` in the same mod. The mode
remains `capture-one-evaluate`, so MO2 need not restart for this rollback.
To remove the experiment, close MHW and disable its bridge Force Load entry.
No physical game-directory DLL or original game asset needs restoration.

## Historical v0.7.0 one-Evaluate test

The [one-Evaluate contract](../research/one-evaluate-capture.md) adds validation
of the core's copied handle and permits one DLSS Evaluate capture. ISP stays in
Create-only mode. [Run 32832](../research/first-handle-lifetime.md) passed Create,
copied-handle validation at Release, and both Shutdowns, but recorded zero
Evaluate calls. Full acceptance remains pending. MHW ignores Evaluate failure, so
the diagnostic can leave an incomplete or stale frame while On. Keep this to
a brief offline capture attempt.

The [loaded-scene attempts](../research/loaded-scene-evaluate-gate.md), runs
37452 and 28340, still record no Evaluate despite successful Create and cleanup.
The user confirms DLSS can only be changed from the main menu; the suggested
in-game toggle procedure is superseded. The user instead returned to the main
menu, enabled DLSS and reloaded the save. Do not repeat this sequence until the
upstream scheduling condition has been investigated. The settings file was
left with DLSS On; any later test must account for that initial state.

MO2 PID 19460 has the correct inherited mode, confirmed by run 32832, and may
stay open. No restart or new launch command is needed unless
MO2 has since been closed. The instructions below retain the initial launch
and title-screen procedure that established handle lifetime.

If MO2 needs restarting, close any existing instance and run this in PowerShell
to give the new instance the process-scoped mode:

```powershell
$env:MHW_DLSS_BRIDGE_MODE = 'capture-one-evaluate'
Start-Process 'C:\Modding\MO2\ModOrganizer.exe'
Remove-Item Env:MHW_DLSS_BRIDGE_MODE
```

The variable removal affects that PowerShell process, not the newly started
MO2 process. Do not use `setx` or change the normal launch configuration.

1. Launch the same **Monster Hunter: World - DLSS 1.2.110 test** entry offline.
   Keep the existing full screen 2560x1440 settings for this attempt.
2. Turn DLSS On once, close its popup, and leave Options fully back to the title
   screen, accepting settings if prompted. Wait about 3 seconds.
3. Reopen Options, switch DLSS Off if selectable, leave Options again, and exit
   normally. Report if it was still On, greyed Off, or the picture looked wrong.
   If the picture prevents navigation, exit the game rather than retrying.
4. If it crashes, stop after that attempt and preserve the log/dump.

Require version `0.7.0`, recognized mode, complete two-provider startup and
DLSS Create result `0x1` with token `0x000000010000B12D`. The first Evaluate
must have verified caller/command list, committed capture, a complete 17-query
snapshot and all four `bridge.handle` checks true. Its `0xBAD00012` result is
deliberate. Additional Evaluate calls must be suppressed without further
queries or events. Optional keys may be absent; preserve getter results.

Accept cleanup only with a verified copied-handle Release returning `0x1`
before both successful provider Shutdowns, no outstanding Init/token, no
fault/contention/event overflow, and a quiesced complete trace. The summary
must show ISP effective mode `capture-create`. A sizing `admission-rejected`
after Evaluate is expected and must show zero parameter reads/writes, with
no synthetic getter fault. If Evaluate is absent, inspect the log before
trying another scene; do not infer success from the menu setting.

Release v0.7.0: 335,360 bytes, SHA-256
`14E343D0A023270EA786F5FF3D5584EC58632CBB88727BAD3B902F53C56CD90F`.
All 12 tests pass, with all 15 distinct exported provider wrappers/callbacks in
the CFG table. Local deployment details are recorded in
`local/evaluate-20260907/staging-070.json`.

To roll back with MHW closed, preserve the current DLL with a versioned
`.disabled` suffix, verify `mhw_dlss_bridge_probe.v0.6.2.dll.disabled` against
SHA-256 `C2245D6D56A23DCE5AD3F449AE2370531D5C36AC9C530EE1AC87D57633CBFF48`,
then copy that backup to the active filename in the same MO2 mod. Restart MO2
with `capture-create` for that baseline. To remove the experiment, close MHW
and disable its bridge Force Load entry; no physical game-directory DLL was
installed and no game asset requires restoration.

## Accepted v0.6.2 sizing and Create test

The [v0.6.2 diagnostic](../research/diagnostic-optimal-settings.md) returns
verified 1:1 sizing metadata. It aims to reach the first DLSS Create, which
still fails without issuing a handle or GPU work. Modern rendering is absent.
Accepted run 20164 passed sizing publication and the menu On/Off transition,
with both bridge shutdown paths complete. The user stayed in Options while On;
no provider Create was recorded. See the
[accepted evidence](../research/diagnostic-optimal-settings.md#accepted-v062-run-20164).
Run **19800** then left Options while On and completed the
[first accepted DLSS Create capture](../research/first-create-capture.md),
with the intentional no-handle failure and both provider shutdown paths complete.
No further repeat is required. The steps below preserve the successful procedure;
`capture-one-evaluate` was disabled in that artifact. Its subsequent review and
new procedure are documented above.

Use the same offline **Monster Hunter: World - DLSS 1.2.110 test** entry with
process-scoped `MHW_DLSS_BRIDGE_MODE=capture-create`. MO2 PID 23484 remains open
from run 20164, whose log confirms the correct mode; no restart is needed.
If MO2 has since been closed, use the existing process-scoped launch command.

1. Launch once offline and open Display settings, keeping the existing full
   screen 2560x1440 configuration. Avoid display-mode changes during this run.
2. Turn NVIDIA DLSS On once, close its information popup, then leave Options
   fully back to the title screen, accepting the settings if prompted. Keep
   it On for about 10 seconds there. Do not repeatedly toggle or retry.
3. Reopen Options and report whether it is still On or has reverted, including
   any error message. Switch it Off, leave Options again, then exit normally.
   No save loading or gameplay is needed for this application-transition test.
4. If it crashes, stop after that run and preserve the log/dump.

Accept the sizing boundary only if the log reports version `0.6.2`, recognized
mode, both complete routes and verified Init/Populate, then
`bridge.optimal-settings` with the exact host caller, all admission checks true,
`boundary-clean=true`, and result `0x1`. The corresponding
`bridge.optimal-output` must show decision success, four completed writes and
four verified readbacks, unchanged dimensions, DLSSMode 1 and Sharpness 0.
Require no actual parameter exception or callback-budget exhaustion. Sizing
callbacks after the one-shot Create may reject admission without querying:
v0.6.2 reports their local summary as invalid-state/fault-latched, which is not
a host getter exception. See the accepted run's telemetry interpretation.

Create capture additionally requires a DLSS Create event with verified caller,
feature 1, verified D3D12 command list and committed scalar capture. Its result
must remain unsupported with no issued token; retain presence/result data for
each Create key rather than assume every queried key exists. Require both
Shutdown paths and a complete quiesced trace. If Create is absent, sizing can
pass independently, but Create reachability remains unproven.

Staged on 2026-09-07: Release v0.6.2, 331,264 bytes, SHA-256
`C2245D6D56A23DCE5AD3F449AE2370531D5C36AC9C530EE1AC87D57633CBFF48`.
All 11 tests pass. Runtime sizing passed in run 20164; run 19800 additionally
passed Create capture, intentional failure and both bridge shutdowns. Deployment
and backup hashes are in
`local/sizing-20260907/staging-062.json`.

To revert, close MHW, preserve the current DLL with a versioned `.disabled`
suffix, then copy the verified v0.6.1 backup to the active name. Its SHA-256 is
`0E41068F677E82989A0E0AC8000F54021C2F1011C0E2FE6F93D1E5CCE5BAF91B`.
To remove this test entirely, close MHW and disable the bridge Force Load entry
in MO2; retain the versioned backups. No game-directory DLL needs removal.

## Accepted v0.6.1 startup containment run

v0.6.0 crashed before the main menu in an old callback that its DLSS Populate
published. See the [crash analysis](../research/optimal-settings-callback-crash.md).
v0.6.1 replaces that metadata publication with a local callback which captures
four inputs and always returns unsupported. This test checks startup containment;
it does not try to enable DLSS or reach Create.

Accepted on 2026-09-07: PID 34496 reached the Display menu with DLSS visible
but unavailable, captured the four callback inputs at 2560x1440, and completed
both provider Shutdown paths with no outstanding cleanup. The complete trace
is `artifacts/logs/bridge-34496.run001.v0.6.1.log`; see the
[accepted evidence](../research/optimal-settings-callback-crash.md#accepted-v061-run).
No repeat of this containment run is required. The steps below document how it
was tested; Create and modern rendering remain untested.

Keep the same offline MO2 test entry and process-scoped
`MHW_DLSS_BRIDGE_MODE=capture-create`. MO2 can remain open from the previous test;
the new DLL loads when MHW starts. If MO2 was reopened normally, close it and
relaunch it with the process-scoped command from the historical procedure below.

1. Launch **Monster Hunter: World - DLSS 1.2.110 test** offline once.
2. If the main menu appears, stay there for about 30 seconds without toggling DLSS.
3. Exit normally and report whether the main menu was reached. If it crashes,
   stop after that run and preserve the new log/dump.

Require version `0.6.1`, both routed provider transactions and verified synthetic
Init, `local-callback-publications=2`, `callback-publication-failures=0`, and no
getter fault/contention. DLSS Populate must show `original-populate-permitted=false`.
If called, `bridge.optimal-settings` must report the host caller `+0x2383FE4`,
`original-called=false`, `outputs-written=false`, result `0xBAD00001`, and at most
four scalar inputs under `phase=optimal-settings`. This deliberate failure is
expected. A new missing/unavailable DLSS row is not a regression criterion for
this containment test. No matching callback means containment remains untested.

The v0.5.3 and crashed v0.6.0 DLLs are preserved with versioned `.disabled`
suffixes. The active artifact and rollback identities are recorded in
`local/dual-provider-20260907/crash/staging-061.json`. To revert, close MHW,
preserve/hash the active DLL, then copy the verified v0.5.3 backup to the same
active name. Game files and MO2 rules are unchanged.

Staged with MHW closed on 2026-09-07: Release v0.6.1, 320,000 bytes, SHA-256
`0E41068F677E82989A0E0AC8000F54021C2F1011C0E2FE6F93D1E5CCE5BAF91B`.
Source and deployed hashes match. All 10 tests pass, and all 15 distinct
provider wrappers/callbacks appear in the CFG table. PID 34496 validates startup
containment, callback capture and both bridge shutdown paths for this artifact.

## Historical v0.6.0 two-provider run

The next run uses the [two-provider contract](../research/two-provider-create-capture.md).
Both DLSS and ISP are contained. Only `capture-create` is supported; Create
fails with no handle and no GPU work for either provider. Modern DLSS rendering
is not implemented. This build is now archived after three startup crashes.

Build and run all tests with the same configuration as the staged artifact:

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

With MHW closed, preserve the existing exact active file before copying the
Release DLL to `D:\MO2\MHW\mods\dlssTEST\mhw_dlss_bridge_probe.dll`.
The v0.5.3 baseline hash is
`133FE2C46AB569937A0B195E1713B285E9029DCDB381052C5B2BC4DA97C9846B`;
its backup name is `mhw_dlss_bridge_probe.v0.5.3.dll.disabled` in the same mod.
If a backup exists, verify its identity rather than overwrite it. Keep source,
destination and backup hashes in `local/dual-provider-20260907/staging.json`.

Staged on 2026-09-07 with MHW closed: Release v0.6.0, 313,344 bytes, SHA-256
`1EC47281A3B054CD4AF5DA52C19CE70D348ED1A1FBE6075CBCBCDF6A69DD4835`.
The source and active destination matched; the v0.5.3 backup was also hash-verified.
All 10 tests passed, and the x64 DLL's 14 distinct provider wrappers were verified
in its CFG target table. The existing MO2 rules were not edited.

Use the existing dedicated **Monster Hunter: World - DLSS 1.2.110 test** MO2
entry, with the bridge as its only active forced DLL. Retain the process-scoped
`MHW_DLSS_BRIDGE_MODE=capture-create` environment, offline mode, DirectX 12,
fullscreen, and CAS + Upscaling off. Do not change settings during the initial
startup of this run. No game-directory deployment is needed.

1. Launch through that MO2 entry and open Display settings.
2. Record whether the DLSS row is missing, unavailable, or selectable.
3. If selectable, select it once to request Create. Failure to enable rendering
   is expected from this no-handle build.
4. Remain in the menu for about 30 seconds so the 20-second checkpoint is
   flushed, then exit MHW normally. Avoid repeated setting toggles.
5. Preserve the new `%LOCALAPPDATA%\MhwDlssBridgeProbe\bridge-<pid>.log` before
   another run. A checkpoint without final Shutdown is partial evidence, not
   proof of cleanup.

Accept only version `0.6.0`, recognized `capture-create`, and verified preflight
for both provider hashes. Require each provider's 14 ordered lookups, masks
`0x3FFF`/`0x3F`, no route/guard rejection, and Init validation mask `0x7F`.
The final aggregate route coverage is `complete-two-provider-transactions`.
Lifecycle calls/checkpoints identify `provider=dlss` or `provider=isp`.
Compare both GetParameters sources for DLSS and ISP Available/FeatureInitResult;
the 28-query capability budget remains unchanged. If Create occurs, expect
`phase=create-dlss` or `phase=create-isp`, at most 11 scalar attempts per provider,
failure, and a null handle. Either feature may be requested first. No Create
means the run has not yet captured the contract. All existing fault, contention,
identity and cleanup requirements below still apply to each owner.

To roll back, close MHW, archive/hash the current active DLL, verify the v0.5.3
backup hash above and copy that backup back to the exact active filename. To
remove the experiment, disable its MO2 forced-library rule and the `dlssTEST`
mod. Backups ending in `.disabled` are not loaded. No original game file needs
restoring.

## Historical v0.5.x single-provider procedure

This procedure is for the first `capture-create` run only. The bridge changes
NGX lifecycle results in that explicit mode, so it is more invasive than the
v0.4.1 observer. It does not load modern DLSS, evaluate an upscaler, or produce
a valid feature handle.

## Build and disabled staging

Build all policy/snapshot tests and the bridge probe before putting it in MO2:

```powershell
cmake -S . -B build -A x64
cmake --build build --config RelWithDebInfo --target ALL_BUILD --clean-first
ctest --test-dir build -C RelWithDebInfo --output-on-failure
```

The CMake component `bridge-probe` installs
`mhw_dlss_bridge_probe.dll` at the MO2 mod root, not under
`nativePC/plugins`. It must be available before the first NGX export lookup;
Stracker's plugin scan is too late.

Stage the first build with a disabled suffix while MHW is closed:

```powershell
$bridgeSource = 'D:\mh world modding\dlssupdate\build\bin\RelWithDebInfo\mhw_dlss_bridge_probe.dll'
$bridgeDisabled = 'D:\MO2\MHW\mods\dlssTEST\mhw_dlss_bridge_probe.dll.disabled'
if (Test-Path -LiteralPath $bridgeDisabled) {
    throw "Preserve the existing staged bridge before continuing: $bridgeDisabled"
}
Copy-Item -LiteralPath $bridgeSource -Destination $bridgeDisabled
Get-FileHash -Algorithm SHA256 -LiteralPath $bridgeDisabled
```

Stop if the destination already exists; preserve and identify it instead of
overwriting it. Staging is complete while `dlssTEST` and every Force Load rule
for the bridge remain disabled. Do not copy the DLL into the physical game
directory or over `nvngx.dll`, `_nvngx.dll`, `nvngx_dlss.dll`, a loader, or the
game executable.

For the controlled run, preserve any existing active bridge file, rename this
one to `mhw_dlss_bridge_probe.dll`, and configure that exact root DLL as the
only enabled MO2 Force Load library for the dedicated MHW executable entry.
`mhw_dlss_unlock.dll` may remain archived in the mod, but its Force Load rule
must be disabled. Do not load the old observer and bridge together.
The bridge rechecks for a mapped `mhw_dlss_unlock.dll` when the provider
transaction begins, but this is a final rejection gate rather than permission
to configure both DLLs.

## Preconditions

- Use offline Steam and the title/options flow or a private solo save. Never
  take this experiment into public multiplayer.
- Confirm MHW is fully stopped before changing the forced-library rule.
- Require the exact executable, cores, cache path, hashes, PE metadata, and
  versions listed in
  [`../research/modern-provider-bridge.md`](../research/modern-provider-bridge.md).
- Use DirectX 12, fullscreen, and FidelityFX CAS + Upscaling off. These retain
  the known menu-eligibility state; they are not proof that DLSS is available.
- Disable every unrelated forced library, including stale LuaEngine entries.
  Record any unavoidable loader or installation confounder before the run.
- Keep `mhw_dlss_probe` under `nativePC/plugins` disabled for this test.

Close MO2 completely, then start it from a PowerShell process that supplies the
one exact opt-in value. This keeps the variable scoped to that process tree:

```powershell
$env:MHW_DLSS_BRIDGE_MODE = 'capture-create'
& '<path-to-ModOrganizer.exe>'
```

Do not use `setx`, and do not begin with `capture-one-evaluate`. If MO2 was
already running, it will not inherit the new process environment.

## Test sequence

1. Launch MHW through the dedicated MO2 executable entry.
2. Stay at the title/options flow and open Display settings. Record whether a
   DLSS row appears; its absence is a valid diagnostic result.
3. If a DLSS choice appears, select it only once if needed to provoke Create.
   Do not repeatedly toggle settings or continue into multiplayer.
4. Allow the bridge to capture Create and complete its exact Shutdown cleanup,
   or let the five-minute bounded window finish, then exit MHW normally. The
   log writer does not close while synthetic Init cleanup remains outstanding.
5. Inspect `%LOCALAPPDATA%\MhwDlssBridgeProbe\bridge-<pid>.log` before another
   run.

The run is acceptable for analysis only when the log proves all of the
following:

- mode was recognized exactly as `capture-create`;
- host, system resolver, current core, cached feature, and the four exact host
  getter RVAs all passed preflight; the pinned legacy core identity should also
  appear in the completed runtime telemetry when that observer slot resolves;
- the exact Init-first 14-lookup provider transaction completed in the pinned
  order, with `lookup-seen-mask=0x3FFF`, `route-mask=0x3F`,
  `guard-seen-mask=0x2FC0`, `callback-seen-mask=0x1000`,
  `route-table-complete=true`, and
  `startup-coverage=complete-init-first-14-lookup-transaction`;
- the six base lifecycle exports resolved to their pinned cached RVAs and were
  routed only to their matching wrappers; the seven guard-only ABI variants
  all resolved null, so a clean first transaction reports `guard-lookups=7`,
  `guard-rejects=0`, and `guard-suppressions=0`;
- the exact generic `NVSDK_NGX_PopulateParameters` target was cached
  feature `+0x1DFA0`, its resolver caller was `_nvngx.dll+0x43351`, it routed
  to `cache_populate_wrapper`, and `callback-routes=1`; a clean first
  transaction reports `routes=7` and `route-rejects=0` (later verified
  duplicate lookups may increase successful lookup counters but must not
  change the masks or introduce a rejection);
- Init came from `_nvngx.dll+0x6769A`; wrapped Populate invocation came only
  from `+0x67ACE` or `+0x67BC2`; and Create, if reached, came from `+0x67DE6`;
- Init reported application ID `0x0B9D2F5C`, API version `0x12`,
  `init-validation-mask=0x7F`, `init-validation-complete=true`, and
  `d3d12-object-verified=true`;
- the one Create snapshot either completed within its fixed query budget or
  stopped with an explicit getter-fault reason;
- Create returned without a feature handle and no Evaluate capture occurred;
- there were no dropped bridge events, recursion conflicts, unquiesced calls,
  boundary contentions, lifecycle-ledger overflow, access violations, hangs,
  or crashes.

No Create call means the run is timing/coverage evidence only, not a contract
capture. `startup-coverage=inconclusive-init-lookup-missed` specifically means
the async worker lost the first lookup and refused to join the 14-member
transaction halfway; the provider was left untouched and the result may be
retried. Any identity mismatch, altered prologue, wrong caller, partial or
reordered transaction, non-null guard, guard suppression/rejection,
generic-Populate binding failure, route rejection, lifecycle-order rejection,
callback fault, or dropped event is otherwise failed/inconclusive. Preserve
the log and do not advance to `capture-one-evaluate` until the Create result
has been reviewed.

## v0.5.3 capability and ISP run

The v0.5.3 build retains six scalar queries at each Populate and adds two
ISP queries at each GetParameters return (eight there), in the same
`capture-create` mode. Retain all preconditions above. Preserve the known
v0.5.2 DLL and its
SHA-256 before replacement. A repository build does not automatically update
MO2's forced DLL.

For this run, open Display once, leave the game open for at least 30 seconds
after reaching the menu, then exit normally. DLSS may still be absent. Do not
enable `capture-one-evaluate` or force an availability value.

Inspect these additional records before another run:

- `trace.start version=0.5.3` and exact recognized `bridge-mode=capture-create`;
- one `bridge.capability` record for each source `capability-populate`,
  `device-populate`, `modern-get-parameters`, and `legacy-get-parameters`;
- each source's `bridge.contract` records and `bridge.contract-summary`,
  with `queries=6` for Populate and `queries=8` for GetParameters,
  `complete=true`, and no fault;
- signed `SuperSampling.Available`, its encoded alias, raw-bit-preserved
  `SuperSampling.FeatureInitResult`, and `SuperSampling.NeedsUpdatedDriver`,
  plus unsigned minimum-driver major/minor values;
- signed `ImageSignalProcessing.Available` and raw-bit-preserved
  `ImageSignalProcessing.FeatureInitResult` at both GetParameters returns.
  Their early Populate records intentionally have `attempted=false`, since
  ISP publication follows DLSS in the core's feature loop;
- matching pointers and values at the modern/legacy GetParameters returns.
  Compare these to the capability Populate object; the per-device Populate
  object is distinct and is not the GetParameters clone;
- the 20-second `bridge.checkpoint final=false`, normally showing source and
  complete masks `0xF`, `capability-queries=28`, no getter fault and no busy
  skips. This checkpoint is progress evidence, not proof of Shutdown.

Missing fields (`present=false`), missing source bits, alias differences, busy
skips or faults are explicit diagnostic outcomes. Never interpret absence as
zero or a successful Populate return as proof of availability. The first
getter exception stops host getter calls across all capture types.

No availability setters or GPU work are added. NGX getters can allocate
internal entries for missing keys, so observation does not guarantee zero
internal bookkeeping changes. Full Create capture and cleanup acceptance
remain unproven when those calls are absent. See
[the static trace](../research/capability-publication.md) for interpretation.

The accepted v0.5.2 second run already showed DLSS availability `1` at all four
boundaries while the user reported the menu row missing. The accepted v0.5.3
run then showed ISP availability `0` and Init status `0xBAD00002` at both
GetParameters returns, with all 28 queries completed. Repeating this same
menu run is not the next gate. See the
[ISP lifecycle research](../research/isp-prerequisite.md). This build does not
change either availability value or route the DLISP provider.

## Rollback

1. Exit MHW completely.
2. Disable the bridge's MO2 Force Load rule. An absolute Force Load entry can
   remain effective even when the source mod is unchecked.
3. Disable `dlssTEST` and rename the active bridge back to
   `mhw_dlss_bridge_probe.dll.disabled`, or remove only that staged bridge file
   after preserving its hash and log.
4. In the PowerShell process that launched MO2, clear the temporary value with
   `Remove-Item Env:MHW_DLSS_BRIDGE_MODE -ErrorAction SilentlyContinue`, or
   simply close that process tree.

Rollback takes effect on the next launch. The test must leave
`MonsterHunterWorld.exe`, the physical game directory, the NVIDIA NGX cache,
and every signed NVIDIA binary unchanged.
