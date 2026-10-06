# Current-state log through 2026-10-05

Dated status entries moved out of `current-state.md` on 2026-10-06. Newest first.
Byte changes from the original: one cp1252 en dash re-encoded as UTF-8 and one
personal video path generalized. Original SHA-256 of this section:
`1ba85c69c682683524318778bf1c752400241a941d89914ee3603db6cff57a25`.

# 2026-10-02: v1.25.44 first live silhouette correction passed; STOPPED cleanly

The user reproduced the hidden hunter and confirmed matching silhouette size/
position. PID48764 / creation134353879484316052: 1,565 DLSS evaluations,
four unfiltered resolves, zero runtime faults. 7,075 late indexed nodes recorded;
4,909 corrected conditional alternatives and 4,909 state restorations, 2,166
native draws, eight private PSOs. The corrected count is a CPU recording receipt,
not per-draw GPU selector readback. No matched full-resolution game control or
corrected pixel readback was captured. State/buffer eligibility rejections remain
follow-up questions; broad silhouette coverage has not been established.

Full native log and all 38 external timeline snapshots passed unchanged frozen
reviewers. All 1,569 GPU signals completed, 1,568 tracked recordings released,
1,700 jitter changes restored, full scale restored, hooks disabled. Independent
live checks verified 41 full native functions and 19 hook heads. All 715
prelaunch files and four configuration identities remained unchanged. User
confirmed a normal scene after stop and normal game exit.

The user reported severe slowdown (seconds per frame) and deferred performance
work. Preserve this for profiling; no Present timings or CPU/GPU measurements
establish its cause. Do not assume it is entirely logging or predict final FPS.
See docs/research/hunter-silhouette-route.md and docs/research/beta-backlog.md.

Sealed private evidence: local/hunter-silhouette-output-20261002;
750 files / 28,392,892 bytes, archive SHA-256:
a115e7ae162d367cf6e5c8fef388a3750a1275022989946e5fe7915c9b7bcc5a
DLL v1.25.44 remains installed, SHA-256:
5c8e6949841132529b6bea6fe56e74ef4431b38db713ed712bea7dc87d6a7bd5
No active collector. After normal exit, the sidecar was moved to fresh
local/hunter-silhouette-followup-20261002; its previous bytes are preserved there.
The tested DLL was retained and no follow-up was armed. Never append new evidence
to a sealed capture. Earlier entries below are historical preparation/results.

# Current work: v1.25.44 indexed silhouette correction prepared

The correction is connected to the existing late indexed wrapper. It enlarges
silhouette raster coverage while preserving native mask/depth coordinates,
vertex/index buffers, root bindings and draw order. Conditional native/full draws
share the immutable frame selector, and late nodes join GPU submission/retirement
tracking. New telemetry distinguishes native draws, corrections and restoration.

Offline verification and a backed-up deployment are being prepared in the fresh
ignored local/hunter-silhouette-output-20261002 folder. In-game corrected pixels
remain unverified. Keep native AA/DLSS Off and the current window size for the next
focused hidden-hunter check. The previous two silhouette archives remain sealed.

# 2026-10-02: Indexed silhouette contract captured; v1.25.43 STOPPED cleanly

PID34140 / creation134353853807668522. 6498DLSSframes,4early skips,0faults.
16indexed API samples across all5known PSvariants: exact arguments, stable
attachments, observed roots/raster, device-qualified color/depth/mask and buffer
ranges. Native raster remains1280x720 on2560x1440 color output. Color stateRT.
Single vertex stream strides32/36/40; R16UINT index ranges fit actual buffers.
Two eye draws write no color; retain native blending/order in the correction.
Material hidden-branch flag/mask/intensity same; wet roughness0 or0.5.

Full native log and43timeline snapshots passed frozen semantic review.
All6502signals completed;7020jitter restored;fullscale restored;hooks disabled.
Live memory independently confirms41complete native functions/19hookheads.
All711prelaunch files unchanged. Freshroot local/hunter-silhouette-fix-20261002.
DLL1.25.43 kept installed. User confirmed normal scene and normal exit.
Archive sealed:743files/30673234bytes, SHA256:
57121b4290c1a2018bc6c4dc9616a8dcf42354c2a4de62cb844a5c0809e051bd

Offline shader adapter passed owned GPU tests; NOT yet connected to the game.
Samples filled in first2gameplay frames before user's visible-silhouette report;
they prove execution bindings, not isolated visible/hidden pixel attribution.
Uploaded descriptors/constants and input states still not established here.
No corrected live pixels or accepted native full-resolution control.
Next: qualify and connect private silhouette PSO/viewport remapping while
preserving native mask/depth coordinates, selector order and GPU lifetimes.
See hunter-silhouette-route.md and indexed-contract-review-34140.json.

# 2026-10-02: Offline silhouette correction passed; v1.25.43 binding test READY

The five captured hidden-surface pixel shader variants now have a runtime
position-remapping adapter. Owned D3D12 WARP/debug/GPU-validation tests passed:
129024 pixel checks and 13 rejection/negative checks; 1x equivalence and 2x
mask/depth coverage verified. Enlarging the viewport without shader remapping
failed the negative control. This is synthetic offline evidence, not corrected
live game pixels or a full-resolution game control.

v1.25.43 is installed for a read-only indexed binding check. It forwards native
silhouette draws unchanged; the correction is NOT connected to the game yet.
Actual indexed arguments, four vertex streams, index buffer, API raster/roots,
attachments and device/resource descriptions are bounded to16samples/4096visits.
Native wrapper0x259d740 full214-byte hash checked; cleanup adds this hook.
Forwarding, fault shutdown, 34raster checks and 96reordered GPUframes passed.
Diagnostic review/version tests passed with required existing semantics retained.

Installed DLL SHA256:
344637bb8b6f632a32181152cdd4ef93a603ba8b3693a21c19994ea966e33f81
Fresh output: local/hunter-silhouette-fix-20261002.
Prelaunch manifest711files; frozen source664files; all41native codegates checked.
Prior silhouette archive930files verified unchanged, sealed manifestSHA256:
72f21bcd724c31280826d816aea2d5c53b4f86e730fb74b4dd236aff7efd4747
Exact destination DLL/output-sidecar backups retained. Four configs unchanged;
game exe/assets untouched. Keep current DLL installed as requested.
Game remains closed. Next: launch main menu, verify startup and arm, load save,
reproduce the silhouette and read actual indexed binding receipts. Stop once
captured; verify41fullfunctions/19hookheads live before normal exit.
See hunter-silhouette-route.md for assumptions and evidence limitations.

# 2026-10-02: Hidden-hunter material path captured; focused run CLOSED

See hunter-silhouette-route.md. PID37748 / creation134353802015513459,
retained v1.25.42, about25min,54325DLSSframes,3earlyskips,runtimefault0.
Full native log and all298timeline snapshots passed frozen semantic review.
All54328signals completed;54911jitter changes restored;fullscale restored;
40nativefunctionspans/18hookheads independently restored before normal exit.
DLLkeptinstalled;4configsunchanged. No new render hooks or external writes.
Three paired hidden-surface recipes each with silhouette absent/present.
Late phase15 indexed character draw after grading still uses1280x720 rectangle
on2560x1440 output. Explicit native hidden-surface PSbranch active, with
integer G-buffer mask load and normalized depth sample; CBScreen domains saved.
OutlineFinal lead inactive, not the demonstrated route. No correction installed.
Full-resolution control attempts accepted0; GPU descriptor/pixel/state proof
and indexed execution/lifetime contract remain pending before correction.
Evidence: local/hunter-silhouette-20261002; archive-final.json records closure.
Previous broad-run reviewer discrepancies remain separate, not closed here.
Game/collector closed. Keep current DLL installed as requested.

# 2026-10-02: Hunter silhouette route identified offline; focused capture PREPARED

User selected the undersized occluded-hunter silhouette first. See
hunter-silhouette-route.md. Identified uOutlineFilter, its phase14/order98,
mask/filter/blur/final chain and independent final raster/UV preparation.
Live draw and correction remain unproven. No rendering changes or new hooks.
Exact v1.25.42 retained, fresh local/hunter-silhouette-20261002 output sidecar
with exact backups, SDK path and frozen source. Prior929-file broad archive
reverified; its reviewer discrepancies remain queued. Four configs unchanged.
Game closed; MO2 running. Next launch main menu, verify/arm, loaded-save
baseline, reproduce silhouette and bounded read-only paired CPU recipe capture.
Keep current DLL installed after test. No public multiplayer.

# 2026-10-02: v1.25.42 broad play CLOSED, PID8968

local/dlss-broad-run-20261002; creation134353722167009226.43.14min,44351DLSSframes,
runtimefault0. User reports solid image, brief area-change skips, known small hunter
silhouette.2061skipattempts/2052graphrejects,21suspensions all recovered. Discovery12
and nativeDOF14 receipts saved; exact visual/action alignment unresolved.
Cleanup46277signals complete,46276main/2057unfiltered/45913output recordings
released;144957jitter restored;fullscale;40native spans/18heads restored. Normal
exit confirmed; game/collector/readers absent. DLLretained,4configs unchanged.
681prelaunchfiles intact. Final14channels accept after reviewer-only key/packet
contract correction(23tests pass); frozenfailure preserved. Correctedfullreview
still fails state rejection stage(two temporal receipts/rawproofs missingallroles),
timeline one unfiltered completion-order discrepancy across506snapshots. No claim
full verification passed; rawbarrierreplay/zero fault/finalcleanup independentlypass.
Export15:12.74,startup/Gallery/cutscenes;not whole43min. Exactclockalignmentunverified.
Nextvisibleissue hunter silhouette; preserve effect/DOF/diagnostic followups.
Fresh output root required before nextlaunch. See [result](dlss-broad-run-result.md).

# 2026-10-02: broad PID8968 still healthy; recoverable paths recorded

User reports only brief area-change skips and otherwise solid image. At03:25local,
27800DLSSframes,zero runtime/routefault,2060skipattempts,2052historicalgraphrejects,
all21suspensions recovered. Later30694frames with unchangedskip/rejectcounters.
Two saved signatures: dependencydiscovery12 and nativeDOF14. Gallerytitles and
recordingalignment pending; do not equate all rejects with transitions or displayedframes.
13/14frozencomponentreviews accept. One reviewer still expects absentkey/packet
for qualification12; v42producer deliberately reports actualstagekey/packet.
Mismatch documented, frozenreviewfailure preserved; reviewer fix/tests afterrun.
Recorder remainsactive, onehourceiling03:51:52local. No rendererchange or stop.
See local/dlss-broad-run-20261002/recoverable-rejections-notes-8968.md.

# 2026-10-02: v1.25.42 broad play session ACTIVE, PID8968

Evidence local/dlss-broad-run-20261002; creation134353722167009226.
Exact tested v42DLL reused. Launch681-file manifest, seven modules, fourconfigs,
modern SDK startup and currentRTX5090 adapter verified. Arm tick9412171;
collectorPID37676/creation134353723128659395,60min ceiling,1024MiB timeline,
approximately03:51:52local end. Mainmenu868DLSSframes;loaded-save1674frames,
fault0,graph/overlayreject0,onlyone recovered startupwarmup;14reviews pass.
User reports loaded-save active/clean, now Gallerymenu for varied cutscenechecks.
Do not stop healthy run after first issue/checkpoint. New reports need timestamp
and counter deltas. Whole-sessionSteamrecording;AA/DLSSOff;unchangedwindow.
Keep installedDLL after closure. See local evidence live-handoff.md.

# 2026-10-02: v1.25.42 broad play session PREPARED

User requests another longer test to discover other issues. Fresh evidence:
local/dlss-broad-run-20261002. Exact tested v42DLL retained; no rebuild or new
hook. Prior803-file radial combat archive reverified. Frozen build-time source,
SDK path, backups, four unchanged configurations and dynamic adapter verifier
copied. Game closed during setup; MO2 already running.
Plan30-60min varied normal solo/offline play with whole-session Steam recording;
start at main menu,verify/arm,then let user roam.60min collector ceiling,1024MiB
timeline; announce deliberate stops and do not end a healthy test after a single
checkpoint. User may finish earlier. Capture report times and counter deltas;
separate known small silhouette and transition/warmup skips from new issues.
Read/write closure evidence only for this process generation; keep DLLinstalled.
Main-menu launch pending. See run-plan.md and preparation/deployment/prelaunch.

# 2026-10-02: v1.25.42 CLOSED — targeted radial combat checks passed

PID16396/creation134353690532440251, local/radial-native-contracts-20261002.
23340DLSS frames,2972copy/radial pairs,zero runtime/graph/overlay faults.
User reports Safi nova and Alatreon frost/nova active/clean. PairedCPU captures
Safi26201/26203 and Alatreon38262 confirm native1x1engine7mask,t1/t2sameowner/view.
Each capture1descriptor/2draws;equivalent multiple-descriptor correction remains
offline-proven,not separately demonstrated by this run's accepted CPU snapshots.
71skipattempts across whole session,10suspensions/resumptions;62attempt burst
beforefrost report lacks exact action/video alignment. Nova interval one haze
pipeline warmup;no output graph rejection. Do not claim zero skips globally.
Intentional stop healthy:23411GPU signals completed;23410main/6unfiltered/27621
output recordings released;24119jitter restored;fullscale;40native spans/18heads
readback matches. Frozenfull/14component/144snapshot reviews pass. Normal scene/
exit confirmed;game/collector/readers absent.706prelaunchfiles and4configs intact.
Keep v42DLLinstalled. No new candidate. Fresh output folder REQUIRED before next
launch. Hunter silhouette remains separate/unmapped;camp deferred. See
[result](radial-native-contracts-result.md), closed-result-16396.json and archive.

# 2026-10-02: v1.25.42 prepared — native radial contracts

Game closed. New candidate accepts the native format-7 RGBA8 default radial mask,
including shared t1/t2 ownership, and equivalent source-free descriptors at one
sort key. Each draw retains its own shader/binding/resource checks and native
queue order; conflicting owners and duplicate packets still reject. Dependency
failure receipts now identify the actual key/packet. No new hooks or game assets.
Mid-plan owner replacement still rejects; existing same-owner storage refresh
and fence+Reset lifetime remain unchanged.
WARP and NVIDIA production graph/recording/refresh fixtures pass, plus the
96-frame/288-effect runtime regression, 11 focused CTests and 78 Python tests.
Historical41/synthetic42 review and required-channel rejection pass. Default and
repeated-pair additions prove graph admission, not complete combat pixel replay.
Live Safi/Alatreon verification PENDING. No claim that all effects are covered.
Previous PID1464 combat archive reverified: 763 files. Candidate/build/source/
backup identities live in local/radial-native-contracts-20261002 receipts. Keep
new DLL installed after test; four settings files unchanged. Fresh SDK/output
configuration prepared. Next main-menu -> save -> Safi nova -> Alatreon frost/
nova -> recovery -> announced stop/cleanup. Silhouette and camp remain separate.
See [radial-native-contracts.md](radial-native-contracts.md).

# 2026-10-02: v1.25.41 CLOSED; combat radial contract gaps captured

Run local/postprocess-family-validation-20261002-r2, PID1464/creation134353662552278232.40626DLSS frames,6321skip attempts,zero runtime faults;59suspensions all recovered. This is a successful diagnosis/cleanup, NOT uninterrupted combat compatibility.
Safi repeat paired snapshot67341: qualified copy/radial shaders;radialt2 is native1x1 engine7 default, identical to t1 owner/resource/SRV. Native helper24819d0 uses global51c4368 manager+288 when custom mask absent. In-process descriptionrole3/engine7/DXGI28/Texture2D/1x1. Current role3 engine23-only gate rejects valid default. Custom256x256 engine23 snapshot65196 also saved.
6255output rejections:5165dependency discovery,1090ambiguous key. Two effect-system radial descriptors share key78001004/output, with4draws+4state packets. Need group equivalent contracts and retain native stable draw order;reject conflicting keys. Alatreon frost/nova same rejection patterns, user recovered clean, but no accepted external Alatreon binding snapshot.
Intentional stop verified:46947main/6259unfiltered/46487output recordings released;48007jitter changes restored;full scale;40native spans/18heads restored. Frozen full/14component/timeline reviews pass. Normal scene/exit confirmed;game,collector,readers absent.682prelaunchfiles and4configs unchanged;v41DLL retained.
No new candidate. Next shared default-mask/duplicate-key correction with offline fixtures; improve failed-stage/key diagnostics (key0 currently misses capture). Fresh output root REQUIRED before another launch. Hunter silhouette separate/unmapped;camp deferred. See [result](postprocess-family-validation-result.md) and closed-result-1464.json/archive-final.json.

# 2026-10-02: v1.25.41 shared-family validation ACTIVE, PID1464

Game PID1464 creation134353662552278232; output local/postprocess-family-validation-20261002-r2. Startup,682prelaunchfiles,7modules,4configs and SDK path/runtime identity pass. GPU is NVIDIA RTX5090; current DXGI LUID0:93911 differs from prior boot0:93877. Read-only DXGI verification and a separate verifier amendment preserve the sealed original; all other checks unchanged.
Armed tick3514109. Main-menu checkpoint1304DLSSframes,1startup skip,zero faults/graph or overlay rejections,all1304GPU signals completed;all14component reviews pass. Family summary active/notwaiting. Collector PID31904 creation134353664353930754,session-1464,timeline512MiB,60minute ceiling;no errors. Do not start another collector or arm again.
Next load save and verify baseline, then representative Safi and Alatreon attacks. Native AA/DLSS Off;window2560x1440;recorded Stracker-enabled profile. Keep healthy run active between checkpoints;announce any intentional stop BEFORE signaling. Use frozen tools/checkpoint.py then family_checkpoint.py. Retained graphs are historical, not automatically current failures. Capture native code restoration while game remains open at eventual stop, then normal exit/archive. Current DLL stays installed;camp deferred.

# 2026-10-02: setup omission corrected; v1.25.41 ready in fresh r2 output

PID6808 creation134353659329255475 never armed: preparation omitted sdk-runtime-dir.txt, leaving only the start record and no reservation/startup transaction. Worker source exits before hook installation for that condition; exit code itself not observed. Live verification found40native spans/18hook heads intact. User normal exit, game absent,4configs/DLL unchanged. Failed attempt sealed in local/postprocess-family-validation-20261002 (684files).
Corrected local/postprocess-family-validation-20261002-r2 now selected by output sidecar. Required SDK path file copied from successful v41 archive; runtime hash validated before deployment, sealing and launch review. Missing/empty/relative configuration and wrong runtime hash negative checks pass. Same DLL797d937e8203b1c2d5ab8ab0079559fed15cf02ef7c88a536316273f4c548dbc;no renderer changes. Prior844files/653sources reverified.
Ready for ONE new launch -> main menu -> verify startup/modules/prelaunch -> arm -> save -> representative Safi/Alatreon effects. No live validation yet. Keep DLL installed;camp deferred. Use r2 run-plan/checkpoint/family tools. Correct readiness detail: v41 waits30minutes after ready, with separate startup deadlines;older five-minute comments are historical.

# 2026-10-02: shared-family audit complete; unchanged v1.25.41 ready for combat validation

Offline code/type-record audit links 15 native type names to 14 of 15 direct filter-registration callback groups. Base/effect-system radial types share Execute0x2481900 and rendering helper0x24819d0. Their own type initializers confirm the parent relationship. Scope is these registration helpers, not every screen effect or shader variant.
Old Safi-repeat and Alatreon-nova reports retain order4100 effect-system radial rejection from v25, before the v32 shared dependency/auxiliary-refresh fix. Some old combat interruptions may already be covered; exact attack-frame GPU recipes/current behavior remain UNVERIFIED. Gallery six-pass route already passed v41. Hunter silhouette separate/unmapped;camp entry deferred.
Audit/evidence local/postprocess-contract-audit-20261002; see docs/research/postprocess-family-audit.md. Future-native-effects.md stale radial/Gallery/tool status updated. No rendering source changes, new hooks or additional in-game logging.
Prepared local/postprocess-family-validation-20261002 for ONE next process. Current DLL797d937e8203b1c2d5ab8ab0079559fed15cf02ef7c88a536316273f4c548dbc kept unchanged;only output sidecar redirected. Prior844-file archive and653frozen sources reverified;4configs unchanged;no game running at setup. File-only family checkpoint labels native units and reports counter deltas;retained history is not a current-failure claim.
Next main menu -> save baseline -> representative Safi/Alatreon effects -> optional silhouette checkpoint -> announced stop/cleanup. Keep DLL installed. Prelaunch manifest/proof and run-plan.md in new root. No new live validation yet.

# 2026-10-01: v1.25.41 CLOSED; Gallery native compute path passes live

884 complete six-pass sequences and matching full-size composition draws;5304 dispatch receipts,1768 adapted/restored,zero sequence/binding/runtime rejections. Actual native views/resources/barriers retained;no descriptor reconstruction. User reports clean cutscene, gameplay and area change. Test armed in loaded save, not main menu.
2892 DLSS evaluations,6 skipped dispatch observations across initial/Gallery/area transitions (not displayed frame drops). No sustained Gallery suspension;all4 suspension periods recovered. Full live queue admission now exercised. No live pixel readback or general effect coverage claim.
Intentional stop:all2898 main/5 unfiltered/2946 output recordings completed/released;12084 jitter changes restored;full scale. Frozen full/14component/timeline reviews pass;40 native spans/18 heads restored. User confirmed normal scene/exit;game/collector absent;v41 DLL kept installed,4configs unchanged,805prelaunchfiles verified.
Evidence local/dof-native-compute-route-20261001;final archive and closed-result-50900.json. No next candidate. Fresh output root REQUIRED before another launch.
Next monster-effect interruptions from wider run, then hunter silhouette scaling. Camp-entry deferred. See [dof-native-compute-route-result.md](dof-native-compute-route-result.md).

# 2026-10-01: v1.25.41 prepared; native Gallery six-compute integration

Candidate keeps native six-pass resources/views/constants/barriers and dispatch order. Private PSOs adapt pass0 synchronization and pass3 reconstructed-color reads; other four dispatches remain native once. Six nodes join existing frame selector and fence+Reset lifetime. Exact shader/binding/resource/constant recipe gates, 2560x1440 output and 1280x720 render only. No new hooks or game assets changed.
This supersedes the proposed second set of owned blur buffers: native descriptors stay bound, full depth is preserved. Complete live queue admission and actual Gallery image remain unverified.
Both v40 full-depth samples pass production dispatch/observer/lifetime fixtures on NVIDIA and WARP: all7 intermediate images and final pixels exact, seven malformed contracts rejected, PSO/predicate restored, real fence+Reset retirement. Synthetic color and fixture GPU views; no claim of decoded native descriptor contents. 60 targeted CTests/105 Python tests and 96-frame/288-effect runtime regression pass. Final builds clean; earlier test/build/setup failures retained and corrected.
Evidence local/dof-native-compute-route-20261001; v40 runtime and offline replay archives reverified. preparation/deployment/prelaunch receipts identify source snapshot, installed DLL and exact prior DLL/sidecar backups. Four settings files remain unchanged; keep candidate installed after exit.
Next main menu -> save -> affected Gallery cutscene -> verify dof_native_sequence six-pass/adapted/restore counters and image -> gameplay recovery -> announced stop/cleanup. Gallery before monsters/silhouette; camp deferred. No live success claim yet.
See [dof-native-compute-route.md](dof-native-compute-route.md).

# 2026-10-01: captured-depth Gallery replay passes offline; reduction synchronization defect found

Two v40 depth samples with paired constants/native formats pass NVIDIA and WARP replay: all7 intermediate images and final pixels match the independent reference exactly. Both paths include one horizontal group barrier. Synthetic color/explicit fixture views remain;live Gallery is not integrated.
Original horizontal shader omits synchronization after128-thread shared merge before16-thread stores; real varying depth exposes incorrect partial reductions on both devices,including NVIDIA with GPU validation off. Exact-hash offline sync_g_t insertion fixes it. No live/game shader patched. Separate CPU oracle now models allowed MAD/DIV arithmetic intervals before packed conversion;original race still rejects.
Sample1 outer-depth clearing changes final image by0.00310171;sample0 is insensitive at output but intermediate controls detect it. Preserve full allocation. Four device/sample cases,2 synthetic regressions,shader/input negative controls passed;readback preserves3686400 depth pixels per execution.
No game running;v40 DLL and4configs unchanged. No new candidate or output path. Previous runtime root remains sealed;prepare fresh output before next launch.
Next preserve/verify input-view contract and move validated six-pass pipeline into owned live resources,then connect final composition through existing branch/lifetime rules. Gallery before monsters/silhouette;camp deferred.
See [dof-live-depth-replay-result.md](dof-live-depth-replay-result.md) and local/dof-live-depth-replay-20261001/review.json.

# 2026-10-01: v1.25.40 CLOSED; full Gallery depth inputs saved

Two 2560x1440 R32F copies before native reduction at frames14140/14200,14,745,600 bytes each. Both Reset/fence retirement and saved files verified;pixel ownership released.
All921600 active-view pixels changed;all2764800 outer pixels bit-identical. Retained contents are a hypothesis,not proved writer/age. Do not crop/clear the outer depth. Native descriptor contents remain unverified.
12/12 native dispatch observations,36 known texture-use states,48 barriers,zero capture errors/lost observations. Resource descriptions/8 shaders matchv39. Gallery rendering still suspends.
Explicit gameplay recovery verified after initial checkpoint still showed waiting.3595 DLSSframes,2235 skipped dispatches,zero faults;5830 main/2235 unfiltered recordings complete/released;3621 output recordings/18 observerrefs released;44159 jitter edits restored;full scale.
Frozen full/13component reviews pass;40 native spans/18 heads restored. Normal scene/exit confirmed. Game/collector absent;v40 DLL kept installed,4configs unchanged,690prelaunchfilesverified. No next candidate. Fresh output root required before launch.
Next use real full-depth inputs in offline replay and finish descriptor/view validation,then connect six-pass Gallery/composition. Gallery before monsters/silhouette;camp deferred.
See [dof-depth-pixels-result.md](dof-depth-pixels-result.md) and local/dof-depth-pixels-20261001/closed-result-43576.json.

# 2026-10-01: v1.25.40 prepared; full native Gallery depth pixels

v39 captured all12 dispatch states. v40 adds two bounded full2560x1440 R32F readbacks immediately before native first depth reduction, with exact shader/dimensions/state/predicate/recipe checks. Source state restored before native Dispatch. Gallery rendering admission still unchanged.
Separate compute_depth_pixels schema1: generation/queue/fence/Reset required before worker mapping and reference release. Existing compute_resources schema3 remains observation-only. No new hooks or scene writes.
60 targeted native tests and79 Python tests pass;6 synthetic negative full-review controls. WARP/NVIDIA hooked-copy/production-queue fixtures each preserve3686400 exactpixels and native state/content. Both retirement orders checked. 96frames/288effects runtime regression passes786432 exactpixels. Final /W4 /WX builds clean; initial fixture Windows small-macro build error retained and corrected.
Evidence local/dof-depth-pixels-20261001; prior725-file v39 archive reverified. Snapshot/deployment receipts identify installed candidate and exact previous DLL/sidecar. Four configurations unchanged.
Next main menu -> save -> Gallery -> two saved full-depth images -> recovery -> announced stop/cleanup. No descriptor-content or current-frame depth-footprint inference yet. Keep current DLL installed after exit.
See [dof-depth-pixels.md](dof-depth-pixels.md). Gallery before monster effects/silhouette; camp deferred.

# 2026-10-01: v1.25.39 CLOSED; all Gallery dispatch states captured

Frames14666/14726;12/12 successful native compute observations,36 known texture-use states,48 barriers,zero capture errors/lost events. Both samples agree on states.11 raw scheduling-pointer changes retained;no stable recipe changes.9 texture descriptions and8 shader files matchv38.
Recovery and normal exit confirmed.3613 DLSSframes,1582 skipped dispatches,zero faults.5195 main/1582 unfiltered recordings complete/released;3642 output recordings/18 diagnosticrefs released;7758 jitter edits restored. Frozen full/12components pass;40 native spans/18 hookheads restored.
Game/collector absent;v39 DLL keptinstalled,4configs unchanged. No next candidate prepared. Fresh output root required before another launch.
Next validate live view/descriptor contract and depth sampling footprint,then connect six-pass pipeline/composition. No liveGalleryfix or general-stability claim. Gallery before monsters/silhouette;camp deferred.
See [dof-stable-recipe-result.md](dof-stable-recipe-result.md) and local/dof-stable-recipe-20261001/closed-result-51896.json.

# 2026-10-01: v1.25.39 prepared; stable Gallery dispatch recipe comparison

v38 localized11/12 diagnostic rejects to packet+0x8 native state-list head. v39 excludes only prefix words2/3 from preparation-to-execution recipe comparison, retaining raw prefix/mask and stable_difference. Same-stage snapshots, resource descriptions and all other recipe fields remain exact. No rendering admission/new GPU work/hooks.
Schema3 native-compute-stable-recipe-v3; full review requires it forv39. Unknown resource states remain unknown. 60 native and82 Python checks pass,6 synthetic negative schema controls; WARP/NVIDIA complete Gallery sequence still refused;96-frame/288-effect runtime regression passes786432 exactpixels. /W4 /WX builds clean.
Evidence local/dof-stable-recipe-20261001. Prior v38 archive839files reverified. Preserve v38 DLL/sidecar, install candidate with fresh output and seal; deployment.json records actual installation. Four configurations unchanged.
Next main menu -> save -> Gallery -> inspect all12 dispatch observations/states -> recovery -> announced stop/cleanup. Gallery may still suspend; no complete-state/descriptor/depth-footprint or rendering-fix claim. Keep candidate installed after exit.
See [dof-stable-recipe.md](dof-stable-recipe.md). Gallery before monster effects/silhouette;camp deferred.

# 2026-10-01: v1.25.38 CLOSED; Gallery recorder mismatch localized

Two samples frames50798/50858 retain nine resource descriptions each and44 barriers,zero lost. Eleven of12 dispatch checks reject only packet+0x8 state-list pointer changes;all other recipe groups match. Sample0pass1 passed native dispatch/state observation.
The native planner updates this list between preparation and consumption (existing code-gated writer/converter analysis). Separate scheduling metadata from stable recipe comparison next;keep exact same-stage snapshots and independent state checks. No live Gallery fix or complete-state/descriptor/depth-footprint claim.
Recovery and normal exit confirmed.2253 DLSSframes,2021 skipped dispatches,zero faults.4274 main and2021 unfiltered recordings complete/released;2275 output recordings,18 diagnostic refs released;8989 jitter edits restored. Frozen full review/12 components pass;40 native spans/18 hook heads restored.
Game/collector absent;v38 DLL kept installed,4configs unchanged. No next candidate prepared. Fresh output root required before next launch.
See [dof-entry-capture-result.md](dof-entry-capture-result.md) and local/dof-entry-capture-20261001/closed-result-47856.json. Gallery before monster effects/silhouette;camp deferred.

# 2026-10-01: v1.25.38 prepared; specific Gallery entry diagnostics

v37 proved9 resource descriptions per sample but only2/12 dispatch observations; entry error1 was combined. New schema2 records13 specific entry reasons, observed context/generation/frame, recipe-read result,8-group difference mask and12 observed prefix words. No validation relaxation or compute admission change.
Offline replay now uses captured R11G11B10/RGBA16/R32_UINT/R8 formats: both frames11342/11402 pass NVIDIA/WARP with all7 intermediate writers and final outputs exactly matching their independent reference. Wrong coordinates/stale writer/outside depth controls remain sensitive. CPU packed oracle uses representable brackets; exact adapter comparison unchanged.
60 native tests,76 Python tests,5 new-version schema controls, actual-description/rejection checks on2 devices and96-frame runtime regression pass. /W4 /WX builds clean.
Evidence local/dof-entry-capture-20261001. Game closed; freeze sources, preserve v37 DLL/sidecar, install v38 with fresh output and seal before launch. Consult deployment.json for actual installation status. Four configurations unchanged.
Next main menu -> save -> Gallery -> inspect entry reasons -> recovery -> intentional stop/cleanup. Gallery may still suspend. Keep DLL installed after exit; camp deferred. No live descriptor/depth-pixel proof or Gallery fix claimed.
See [dof-entry-capture.md](dof-entry-capture.md). Prior v37 archive723 files reverified.

# 2026-10-01: v1.25.37 CLOSED; Gallery resource descriptions complete, states partial

Two samples frames21416/21476, nine actual Texture2D descriptions each, identical formats/extents. Depth R32_FLOAT; scene/Image/tile resources R11G11B10_FLOAT; CoC RGBA16_FLOAT; packedDOF R32_UINT; alpha R8_UNORM.
Only2/12 native dispatch observations passed (sample0pass1/sample1pass5);ten entry error1, precise predicate not recorded.44 barriers,zero lost events. No descriptor-content/pixel-footprint/live Gallery-fix claim.
Recovery and normal exit confirmed.1990 DLSSframes,1789 skipped dispatches,zero runtime faults.3779 main and1789 unfiltered recordings complete/released;2010 output recordings and18 diagnostic refs released;9451 jitter edits restored.
Frozen full review/12 components pass;40 full native spans/18 heads restored. Game/collector absent; v37 DLL kept installed,4 configs unchanged. No next candidate prepared.
See [dof-resource-capture-result.md](dof-resource-capture-result.md) and local/dof-resource-capture-20261001/closed-result-49960.json. Fresh output root required before next launch.
Next distinguish entry predicates, then actual-format offline replay and remaining state/descriptor/depth-footprint validation. Gallery before monster effects/silhouette; camp deferred.

# 2026-10-01: v1.25.37 prepared; Gallery resource/state capture pending

Offline coordinate replay passed; next capture actual GPU resource descriptions and native recording states for both six-pass Gallery bundles. Rendering admission unchanged: Gallery may still suspend DLSS. No GPU work added by this diagnostic.
Existing compute executor/API hooks supply packet, command-list generation, pipeline and bounded transition observations; up to32 COM references are released after callback drain. Unknown states remain unknown. No pixel footprint or descriptor-content proof claimed.
Clean /W4 /WX candidate build;60 targeted native tests and72 Python tests pass. Four actual formats plus wrong-device rejection pass on WARP/NVIDIA; full Gallery admission remains refused.96-frame replay regression passes with GPU validation.
Evidence local/dof-resource-capture-20261001. Freeze source, back up exact v36 DLL/sidecar, deploy v37 into the same MO2 slot with fresh output root, then seal prelaunch. Consult deployment.json for installation receipt.
Game closed at preparation; four configuration identities unchanged. Main menu -> save -> affected Gallery cutscene -> recovery -> intentional stop/cleanup. Keep installed after normal exit. Camp deferred.
See [dof-resource-capture.md](dof-resource-capture.md). Previous v36 archive892 files reverified.

# 2026-10-01: Gallery captured-constant coordinate replay PASSED offline

Both frames11342/11402 passed NVIDIA and WARP with GPU validation: seven retained intermediate images match; final maximum delta0 against independent native-shader/proxy reference.
Wrong color-coordinate and stale-Image negative controls remain sensitive. Six legacy cases also passed. Targeted /W4 /WX build clean.
Outside-active-view depth affects native reductions and final pixels; live full-depth contents/valid footprint must be established, not assumed or discarded.
Offline adapter only: native formats/subresources/states and live lifetimes still unverified. No live compute admission change and no next runtime candidate prepared.
See [dof-coordinate-replay-result.md](dof-coordinate-replay-result.md). Evidence local/dof-coordinate-replay-20261001/review.json; SHA 57a9dfae9cc5ec6bbf59989676f95070577fceceb104aecf92da4236f5087c3f;101 evidence files.
Original sealed archive892 files reverified; installed v1.25.36 DLL and4 configurations unchanged. Game closed. Fresh output root/sidecar required before another launch.
Native-upscale quick audit found no proven compatible shortcut; keep current route, separate native temporal-upsample branch parked. Camp deferred; Gallery first, then monster effects/silhouette.
Next establish known Gallery bundle's actual GPU descriptors/states and external input footprints, then chronological owned replay. Do not simply admit opcode6.

2026-10-01: Bounded native-upscale audit complete; keep current integration.
CAS has a late UpscaleOut color handoff, but no compatible shortcut was proven.
A separate native temporal-upsample branch is parked, not validated. See
[native-upscale-quick-audit.md](native-upscale-quick-audit.md). No live changes.
Resume captured-constant Gallery replay; preparation adapter is an unbuilt
offline draft. v1.25.36 remains installed; use a fresh root before next launch.

# 2026-10-01: v1.25.36 run CLOSED; Gallery constants captured

Two complete paired bundles frames11342/11402, zero capture errors. Six CS + VS/PS match v35.
Twenty-two bounded constant records, five unique files under local/dof-pipeline-20261001/gallery-extracted-23216.
Compute and composition CBScreen bytes IDENTICAL despite different pointers: allocation2560x1440, view1280x720, offset0, scale0.5.
Blur/vignette/projection depth conversion vary between frames. Preserve animated artistic parameters.
Full synthetic GPU fixture passed WARP/NVIDIA, but not yet replayed with live constants/valid footprints or native GPU formats/states.
Gallery still suspends at compute admission. Next replay full pipeline with captured constants and explicit valid input footprints, then coordinate adaptation/native state integration.
No next candidate prepared. Recovery/cleanup passed:3424 frames,2327 skipped dispatches,zero faults.
5752 main signals/recordings complete/released;2327 unfiltered and3447 output recordings released;13044 jitter edits restored.
Full scale and40 native spans/18 heads restored; full frozen review and11 component checks pass.
User confirmed normal scene/exit; game/collector absent; v36 DLL kept installed;4 configurations unchanged.
See dof-pipeline-constants-result.md and closed-result-23216.json (pre-exit runtime-result is retained as evidence).
Fresh output root/sidecar required before another launch. Camp deferred; monster effects/silhouette follow Gallery.
Archive local/dof-pipeline-20261001: 892 files; SHA 0832ad24a369206242280a7fa1662658e3f9a5958a1dc782e207ea98a01b3a95.

## 2026-10-01: v1.25.36 prepared; Gallery constants capture pending

The complete six-compute DOF pipeline now has an offline GPU fixture. Its synthetic cases pass on WARP and NVIDIA hardware; native live formats/footprints and scaled-pipeline equivalence are not yet established. v1.25.36 captures the actual CBScreen, CBViewProjection and CBNewDOFFilter bytes from both paired command snapshots. Compute passes share a screen-buffer owner; composition has a different owner. Rendering admission remains unchanged and Gallery may still suspend DLSS. Next: main-menu baseline, save, affected Gallery cutscene, extract constants, recovery and cleanup. Keep the candidate DLL installed; camp deferred.

See [DOF pipeline and constants](dof-pipeline-constants.md). Evidence: `local/dof-pipeline-20261001`; prior sealed evidence: `local/dof-compute-capture-20261001`.

# 2026-10-01: v1.25.35 run CLOSED; complete Gallery compute bundle captured

Two stable samples frames17155/17215; six CS shaders and composition captured with zero errors.
Private extraction/disassembly: local/dof-compute-capture-20261001/gallery-extracted-23128.
Pipeline: horizontal/vertical depth reduction -> Neighbor -> CoC/Image preparation -> packed DOF/alpha -> unpack Image -> composition.
Image reused: first preparation, then unpack overwrite. Eleven internal edges and four external reads per sample.
External reads: depth in CS1/CS4; scene in CS4 and final PS. Descriptor source identity agrees.
CS4 integer-loads a 2x2 HDR block; constants/valid depth/color footprint remain unverified.
Next full six-compute/composition GPU fixture and native CBScreen/coordinate contracts before admission changes.
Gallery still suspends, DOF replacement not executed. No next candidate prepared.
Recovery confirmed. Final3264 DLSS frames,2249 skipped dispatches,zero faults.
5513 signals complete;5512 main/2248 unfiltered/3292 output recordings released;12379 jitter edits restored.
Full scale and40 native spans/18 heads restored. Frozen full review/all11 components pass.
Normal scene/exit confirmed; game/collector absent. v35 DLL remains installed,4 configurations unchanged.
See dof-compute-bundle-result.md. Camp deferred; monster effects/silhouette pending.
Archive local/dof-compute-capture-20261001: 705 files; SHA ee373deba6684307fd420c7d33f67235ced87856918bdb1b1bebda16fb72bed9.

# 2026-10-01: v1.25.35 ARMED; awaiting loaded scene then Gallery

PID23128, creation134353170129823683; collector48448 (60min/1024MiB).
Fresh root local/dof-compute-capture-20261001. Verified652 prelaunch files,
7 modules,4 configuration hashes. Main-menu baseline776 DLSS frames,
2 startup skips,zero faults; all11 checkpoint reviews pass.
New compute_dependencies channel empty at main menu, as expected.

User asked to load save and stay10seconds, native AA/DLSS Off,no resize.
Next Gallery cutscene: expect suspension, capture2 complete bundles with
six compute shaders/bindings and composition. Extract with frozen
extract_compute_dependencies.py; all evidence is CPU-only until GPU verified.
Use frozen checkpoint.py and tools. Announce intentional stops and independently
verify native cleanup. Keep DLL installed. Camp deferred.

---

# Current handoff: v1.25.35 DOF compute capture prepared (2026-10-01)

Game is closed. Next candidate: `local/dof-compute-capture-20261001`.
The previous v1.25.34 run is sealed at `local/dof-output-route-20261001`:
718 files; archive SHA-256
`462259894b1f62a5906c4e25a3445717da6d86e0ee265ecf5350cba3f6438b58`.
Do not write new logs there.

v1.25.35 adds read-only paired compute/draw snapshots at the existing prepared
queue callback. Rendering admission is unchanged; Gallery still suspends at
its first unsupported compute. The native fixture now includes the complete
observed sequence `[11,6,6,6,6,6,6,0]` and proves that rejection explicitly.
Offline checks passed: 59 native tests, 63 Python tests, WARP and NVIDIA plan
fixtures, 96-frame continuous GPU regression, historical v34 review and seven
negative cases for the new telemetry schema. No live success claimed.

Next: verify installation/prelaunch receipts, main-menu startup/arm, loaded
scene baseline, affected Gallery cutscene, two complete bundle captures, then
recovery and intentional stop/independent cleanup. Keep native AA/DLSS Off,
windowed 2560x1440, same solo/offline profile. Keep the current DLL installed.
Read [the capture contract](dof-compute-bundle.md). Inspect compute shaders,
SRV/UAV producer edges and sampling conventions before changing admission.
Camp remains deferred; monster effects then silhouette follow Gallery.

---

# 2026-10-01: v1.25.34 run CLOSED; Gallery compute bundle remains unsupported

Gallery suspension reproduced; reason17/branch7 rejects first opcode6 under DOF key0x78006595.
State11 + six compute6 + composition0; DOF draws0 and no dependency binding checks.
Offline plan fixture missed compute commands. Next full-bundle fixture and compute input/output/coordinate capture.
Recovery confirmed. Final4471 DLSS frames,2199 skipped dispatches,zero runtime faults.
All6670 main and2198 unfiltered GPU recordings released;4557 output recordings released.
All13873 jitter changes/full scale restored;40 full native spans and18 heads verified restored.
Frozen review/all10 components pass lifecycle; Gallery fix failed and shader adapter was not exercised.
User normal scene/normal exit confirmed; game/collector absent. v34 DLL remains installed;4 configs unchanged.
See dof-output-route-result.md. No next candidate prepared. Camp deferred; monster effects/silhouette pending.
Archive local/dof-output-route-20261001: 718 files; SHA 462259894b1f62a5906c4e25a3445717da6d86e0ee265ecf5350cba3f6438b58.

# 2026-10-01: v1.25.34 STOPPED cleanly; awaiting normal game exit

Gallery DLSS suspension reproduced: reason17/branch7 rejects opcode6 at key0x78006595.
Eight native commands: state11, six compute6, composition draw0. DOF draws0; new adapter never ran.
Offline plan fixture omitted compute bundle; next map its full resource/coordinate contract before admission.
Recovery confirmed by user and counters. Final4471 DLSS frames,2199 skips,zero faults.
All6670 main and2198 unfiltered recordings completed/released;4557 output recordings released.
All13873 jitter changes/full scale restored;40 native spans and18 heads independently verified restored.
Frozen full review/all10 checkpoint components pass lifecycle only; Gallery rendering fix NOT verified.
PID49484/creation134353150299535872, collector46060 ended; root local/dof-output-route-20261001.
Asked user to exit normally; keep v34 DLL installed. Close/archive once absence/config hashes checked.
See docs/research/dof-output-route-result.md. Camp deferred; no new build prepared.

# 2026-10-01: v1.25.34 save-load PASSED; awaiting Gallery playback

PID49484/creation134353150299535872; collector46060 active, root local/dof-output-route-20261001.
User reports clean DLSS after save load. Checkpoint2018 frames,3 total skips (one since baseline),zero faults.
All10 component reviews pass; waiting false,2 suspensions/resumptions; DOF draws0 before Gallery.
Asked user to replay affected Gallery cutscene and report image/DOF coverage; keep settings unchanged.
Next read checkpoint; do not infer DOF success until its draws execute. Keep DLL installed. Camp deferred.

# 2026-10-01: v1.25.34 ARMED; awaiting loaded scene then Gallery

PID 49484, creation134353150299535872; collector46060 (60min/1024MiB).
Fresh root local/dof-output-route-20261001. Verified671 prelaunch files,7 modules,4 settings hashes.
Main-menu baseline878 DLSS frames,2 startup skips,zero faults; all10 checkpoint reviews pass.
DOF draws0 at main menu, no dependency rejection. New DOF route live validation still pending.
User asked to load save then stay10seconds, AA/DLSS Off and no resize; Gallery cutscene next.
Use frozen checkpoint.py and arm tools. Announce intentional stops; independently verify cleanup.
Keep DLL installed. Camp deferred; attack effects and silhouette remain later work.

# 2026-10-01: v1.25.34 DOF composition candidate INSTALLED; awaiting main menu

Verified DLL SHA fedc7958f339ca573cb886a2b26527019c5dd8c1e64689ea5499cfcb9b48f97f.
Evidence local/dof-output-route-20261001; exact v33 DLL/output sidecar backed up there.
Game closed; same four settings hashes. 608 frozen source files, checks documented in validation.json.
Complete prelaunch seal before launch; verify_launch.py then frozen arm/collector tools.
Main menu baseline -> loaded scene -> affected Gallery cutscene -> normal-scene recovery.
Inspect dependency_route.dof_draws and five-texture binding/state checks; live success still unverified.
Native AA/DLSS/MotionBlur Off, DX12 windowed 2560x1440; keep current DLL installed afterward.
Camp deferred. Announce intentional stop and independently verify native cleanup before normal exit.
See docs/research/dof-output-route.md for coordinate contract and remaining live assumptions.

# 2026-10-01: v1.25.34 Gallery DOF candidate READY for deployment

Connects intermediate source/destination owners with a qualified five-input DOF composition.
Preserves native auxiliary bindings, constants and vignette; t2 alone uses reconstructed color coordinates.
Offline WARP and NVIDIA tests: 8 cases each, exact reference/production pixels, GPU validation enabled.
Five-input queue plan and 8 negative cases pass on both; fifth binding changes diagnosed.
Wingdrake regression, 96-frame continuous regression, 59 native tests and 64 Python tests pass.
Synthetic v34 reviews and historical v33 review pass. No live Gallery success claimed yet.
Candidate local/dof-output-route-20261001; version 1.25.34; DLL SHA fedc7958f339ca573cb886a2b26527019c5dd8c1e64689ea5499cfcb9b48f97f.
Game closed. Next: install with exact prior backup, seal prelaunch, verify and arm at main menu.
Same Stracker-enabled solo/offline profile, windowed 2560x1440, native AA/DLSS/MotionBlur Off.
Check loaded scene, affected Gallery cutscene and recovery. Keep DLL installed after exit.
Camp deferred; attack effects and hunter silhouette remain next. See dof-output-route.md.

# 2026-10-01: v1.25.33 Gallery capture PASSED; run CLOSED

Two complete CPU recipes identify DOF composition with optional vignette, five texture inputs.
Current route rejects the intermediate owner between motion output and grading input.
3,449 DLSS evaluations, zero faults; three suspensions and successful resumptions.
Full frozen review and all ten checkpoint components pass.
All 5,633 main and 2,183 unfiltered GPU recordings completed/released;
3,475 output recordings released, 9,249 jitter changes and full scale restored.
Read-only cleanup verified 40 native spans and 18 hook heads; normal exit confirmed.
Game/collector absent. Current v1.25.33 DLL remains installed; four settings hashes unchanged.
Gallery rendering fix remains OPEN. No live constant bytes or GPU resource descriptions in this capture.
Next: dependency/coordinate contract and offline GPU fixture for the DOF composition pass.
Camp-entry investigation deferred; attack effects and hunter silhouette still pending.
See gallery-intermediate-result.md. No next candidate prepared.
Archive local/gallery-followup-20261001: 685 files; SHA e9ce5091754cff9ad77e9e792b6b64372a50479c88efe0eb527ebc1f94c7c6a5.

# 2026-10-01: v1.25.33 ARMED; awaiting affected Gallery cutscene

PID 52460, creation134353089850045137; collector4976 (60min/1024MiB).
Fresh root local/gallery-followup-20261001. Verified637 prelaunch files,7 modules,4 settings hashes.
Main-menu baseline635 DLSS frames,1 startup skip,zero faults; all10 checkpoint component reviews pass.
Intermediate recorder has0 attempts/samples so far. Next affected Gallery replay; no camp or wingdrake test requested.
DLSS may suspend during capture; inspect intermediate_dependencies, save extracted shaders/bindings, then check recovery.
Use frozen checkpoint and arm tools. Warn before intentional stop; verify native bytes before normal exit. Keep DLL installed.

# 2026-10-01: v1.25.33 Gallery recorder INSTALLED; awaiting main menu

Step 1 complete: command qualification reviewer fixed with exact dependency receipts; 31 diagnostic/full-review tests pass.
v32 wingdrake and v25 long-run re-reviews pass; old sealed archives unchanged.
Step 2: record Gallery intermediate color pass at native preparation, avoiding unstable external polling.
New intermediate_dependencies channel: two bounded double snapshots, up to four draws with shader/vertex/texture data.
Rendering admission unchanged; this is not a Gallery fix. DLL build and two native fixtures passed; 65 Python tests passed.
DLL SHA 7b7584f95f0ad162d5b31ae807b8cbda39e57c4557aa7e82ed6d25b318277785. Exact v1.25.32 slot and sidecar backed up.
Game closed; no hooks/collector active. Same Stracker-enabled MO2 profile and four settings hashes unchanged.
Fresh evidence root local/gallery-followup-20261001; prelaunch 637 files, 600 frozen source files.
Next: main-menu verify/arm, affected Gallery replay, inspect intermediate_dependencies, recovery and cleanup.
Camp-entry investigation remains deferred. Keep DLL installed after normal exit.
Use verify_launch.py/checkpoint.py/read_stop_native.py in the fresh root. No next capture has run yet.
See gallery-intermediate-capture.md and the 2026-10-01 agreed milestones.

# 2026-10-01: v1.25.32 wingdrake replacement PASSED; run CLOSED

Same-owner radial t2 mask 128x128 -> 256x256 admitted at frame11215; 347 copy/radial pairs.
User reported travel active and clean. 9,246 DLSS frames, zero runtime/route faults.
All 9,274 main GPU signals/recordings and 26 unfiltered recordings completed/released;
9,861 output recordings released, 9,740 jitter changes restored, full scale restored.
Read-only cleanup verified 40 native spans and 18 hook heads; normal exit confirmed.
Game/collector absent. Current v1.25.32 DLL kept installed; four settings hashes unchanged.
Later camp-entry skips deferred at user request; no further camp investigation or next build prepared.
Frozen aggregate review remains failed: command qualification bound<=11 excludes producer12/13.
Eight other checkpoint reviews pass. Preserve failure; fix offline schema coverage before next run.
See auxiliary-binding-refresh-result.md and beta-backlog.md. Other scene/effect issues remain open.
Archive local/auxiliary-binding-refresh-20261001: 801 files; SHA c69b448dbf5294f01d707e72a1093d55fcdeec576962f64db2856462985d8366.

# 2026-10-01: v1.25.32 ARMED at main menu, awaiting save load

Game PID 6072, creation 134353070882316766; collector PID 52768 (60 min/1024 MiB).
Evidence root local/auxiliary-binding-refresh-20261001. Prearm verified 766 manifest files, seven modules and four config hashes.
Main-menu checkpoint healthy: 686 DLSS frames, two startup skips, zero faults. All reviews pass; collector running.
Next save load -> one wingdrake trip. Read dependency_refresh and API checks. Keep DLL installed; do not overwrite active files.

# 2026-10-01: v1.25.32 INSTALLED, awaiting main-menu launch

Auxiliary entry refresh and old/new resource retention candidate. Offline checks pass; no live fix claim.
Game closed; no active hooks or collector. Same MO2 profile/settings.
DLL SHA-256 `3d809257854d14defd454fcd0b40783a85a74f7ab3e8f07a1c3b5a06d7c54588`. Exact v1.25.31 DLL preserved.
Fresh root local/auxiliary-binding-refresh-20261001; prelaunch 766 files.
Next main menu -> save -> one wingdrake trip. Read dependency_refresh and entry/API diagnostics.
An entry difference may now be accepted; require healthy runtime and stable API bindings.
Use terminal-aware checkpoint.py and independent native cleanup checks; keep DLL installed.
See auxiliary-binding-refresh.md.

# 2026-10-01: v1.25.32 auxiliary replacement candidate prepared

Same-owner radial t1/t2 storage may refresh at native draw entry; current snapshot must remain exact through DrawInstanced.
Old and new resources pinned through fence+Reset; t0/scene/output identities remain strict.
Seven-field 128-to-256 live failure reproduced and accepted offline; nine unsafe cases still reject.
Production radial/copy, resource lifetime and GPU validation passed WARP/RTX5090; 96 continuous regression frames, 59 native and 62 Python tests passed.
Evidence/install root local/auxiliary-binding-refresh-20261001. v31 archive verified unchanged.
No live success claim. Next main menu -> save -> wingdrake; keep DLL installed after normal exit.
See auxiliary-binding-refresh.md.

# 2026-10-01: v1.25.31 mask replacement diagnosed; run CLOSED

Radial PS t2 changed from 128x128 to 256x256 between preparation and native entry.
Same owner; new backend, GPU resource, SRV view and descriptor. Entry/API receipts agree.
3,289 DLSS evaluations; 9 copy/8 radial draws; runtime45/route6 binding failure.
All 3,574 signals/main recordings completed/released, 4,052 jitter changes restored; scale/hooks verified.
Two output-route references stayed pinned until normal exit. Game and collector closed.
Current v1.25.31 DLL kept installed; four settings hashes unchanged. No next candidate prepared.
Next: validate and retain replacement auxiliary resources at draw time; preserve strict scene provenance and fence+Reset lifetime.
See dependency-binding-boundary-result.md. Archive 791 files; SHA e75c4f50842c4455d36169f880760c354c81d7271ee50e8d5c73f518d03c847c.

# 2026-10-01: v1.25.31 INSTALLED, awaiting main-menu launch

Binding-boundary diagnostic only; no rendering/admission change or live fix claim.
Game closed, no active hooks or collector. Same MO2 profile/settings.
DLL SHA-256 `bb0d06f3e942975cf0b19206da92a1b42a90acd7806dc3c20495e4fffa278f45`. Exact v1.25.30 DLL preserved.
Fresh output root local/dependency-binding-check-20261001; prelaunch 760 files.
Next main menu -> save -> one wingdrake trip. Inspect dependency_binding_checks entry/API receipts.
Use terminal-aware checkpoint.py, nested route faults, and independent native cleanup checks.
Keep this DLL installed afterward. See dependency-binding-boundaries.md.

# 2026-10-01: v1.25.31 binding-boundary diagnostic prepared

Observes dependency bindings at native draw entry and DrawInstanced; keeps exact existing rejection rules.
Bounded field differences/read-failure phases replace an opaque failure message. This is not a wingdrake fix.
59 native and 59 Python tests passed; production reader/copy fixtures passed WARP and RTX5090; 96-frame runtime regression passed.
Evidence/install root local/dependency-binding-check-20261001. Prior v30 archive verified unchanged.
Next main menu -> save -> one wingdrake trip. Keep DLL installed after normal exit.

# 2026-10-01: v1.25.30 wingdrake binding failure CLOSED

RGB-mask gate passed; 11 scene-copy and 10 radial draws executed before dependency bindings changed.
2,791 DLSS evaluations; route fault 6, terminal runtime fault 45; full feature shutdown.
All 2,835 GPU signals completed, 3,325 jitter changes restored, scale/hooks restored.
Two output-route references stayed pinned until normal game exit; game and collector now closed.
Current v1.25.30 DLL kept installed. No next candidate prepared yet.
Retained frames 25446-25448 saved; exact differing field at failure still unknown.
Next: investigate mutable binding-table lifetime / actual native draw inputs offline.
See radial-rgb-write-mask-result.md. Archive 821 files; SHA fbd0eca533e345fd63582a7ce2d4acbc3b88335b86ca09792f13cec24036d0c0.

# 2026-10-01: v1.25.30 INSTALLED, awaiting main-menu launch

RGB-only radial native mask correction; offline reproduction and regression checks passed.
Game closed, no active hooks or collector. Same MO2 profile/settings.
DLL SHA-256 `1cf3542717ca5a0bb113246785d39a33b35f0be0663de146d145f82368caad81`. Exact v1.25.29 DLL preserved.
Fresh output root: local/radial-rgb-write-mask-20261001. Prelaunch 774 files.
Keep current DLL installed. Next main menu -> save -> wingdrake -> Gallery if healthy.
Use terminal-aware checkpoint.py; inspect nested scaled_route.fault as well as top-level fault.
No live fix claim yet.

# 2026-10-01: v1.25.30 RGB-mask correction prepared

Qualified radial pass now preserves and admits RGB-only mask 7; copy still requires RGBA.
Original failure reproduced offline. RGB production comparisons passed on WARP/5090;
RGBA regression passed. Component fault monitoring corrected; terminal log outranks status.
Evidence/install root local/radial-rgb-write-mask-20261001. No live success claim yet.
Next main menu -> save -> wingdrake -> Gallery if healthy. Keep installed DLL after tests.

# 2026-10-01: v1.25.29 wingdrake failure CLOSED

Native radial RGB-only mask 7 rejected by RGBA-only gate; confirmed retained frame 37896.
2,755 DLSS evaluations; output-route fault 6, full shutdown. No recovery claim.
All GPU signals/jitter/scale/hooks restored; one output pin retained until normal process exit.
Current DLL kept installed. Preparing v1.25.30 in local/radial-rgb-write-mask-20261001.
See scene-dependency-route-result.md. Archive 719 files; SHA 9f205a907f1a2bd9be4050676d5ac23b1306858446d26291ef7bb6d68d6476ae.

# 2026-10-01: v1.25.29 INSTALLED, awaiting main-menu launch

Game closed, no active hooks/collector. Scene-copy/radial integration candidate;
not live verified. Same MO2 profile/settings. Fresh output directory:
local/scene-dependency-route-20261001. DLL SHA-256:
5d36914afd08c31b4707db72c0ebcac6810c003c19acf52a960415d615233587.
Exact old DLL preserved; keep candidate installed after the test.
See installed-handoff.md there and docs/research/scene-dependency-route.md.
Next: verified main-menu arm,save,wingdrake,Gallery. No new launch yet.

# 2026-10-01: v1.25.29 scene-dependent runtime candidate ready

Closed v1.25.28 capture remains sealed. Copy/radial resource dependencies now
join the continuous frame predicate, with private shaders/geometry and
fence+Reset retention. No in-game verification yet. 58 native tests,57 Python
tests,192 continuous regression frames;192 Effect cases each on WARP and RTX
5090 passed. Production copy route and rejection tests passed on both devices.
See scene-dependency-route.md and local/scene-dependency-route-20261001.
Deployment receipt is authoritative. Next: main menu,save,wingdrake,Gallery.
Keep DLL installed. Native AA/DLSS/MotionBlur remain Off,window size unchanged.

# 2026-10-01: Scene-derived temporary path implemented and verified OFFLINE

Generic bounded read/write dependency planner and private radial coordinate adapter added.
Captured scene -> temporary -> scene sequence validated outside the game.
384 WARP cases exact; 384 RTX 5090 cases passed (292 exact, others within documented
filter/format bounds). Every temporary copy exact; 56 coordinate and 16 point-filter
hardware controls exact. Three native tests passed; no GPU validation warnings/errors.
See scene-color-dependencies.md for numerical limits and failed experiments retained.
No live resource/state/descriptor/lifetime admission added; wingdrake is NOT fixed yet
in the installed DLL. Next: connect qualified per-draw resources and private copy/radial
replacements to stream_route, verify rollback/retirement offline, then prepare live test.
Gallery/combat effects/hunter silhouettes remain open. No scene-name whitelist.
Game remained closed; v1.25.28 DLL, sidecar and four configs verified unchanged.
Keep current DLL installed. Its sidecar still points to the sealed prior run:
redirect to a fresh output directory before the next game launch.
Offline archive local/scene-dependencies-20261001: 292 files, manifest SHA-256
`a9582f2d0b044ea38684069c64f29724c64563a1a75f7be8ecdc5249efd567ae`.
No runtime work active and no new runtime build installed.

# 2026-10-01: v1.25.28 dependency capture CLOSED and sealed

Two accepted CPU samples establish scene -> temporary copy -> radial-blur scene write.
Shaders/vertices/bindings captured; CBRadialBlurFilter identified. No compatibility fix yet.
Need dependency-aware extent/UV handling, including screen-position constants and distinct
auxiliary UVs; next work can proceed offline. See prepared-draw-dependencies-result.md.
Gallery/combat-effect/silhouette issues remain open. No blanket effect whitelist.
3,789 DLSS frames,991 unfiltered,zero faults;15 suspensions/15 resumptions.
4,780 GPU recordings completed/released;5,232 jitter changes restored.
40 native spans and18 hook heads verified. User normal exit; process absence checked.
DLL/sidecar/four configs unchanged. Keep v1.25.28 installed. No runtime work active.
Archive local/overlay-dependencies-20261001: 666 files; SHA-256 `447efa834b10911cdfe899c76c2b545861a50aaec7ecc2a5f20951128ebd156a`.
Do not append another run here; prepare a fresh output directory before next launch.

# 2026-10-01: v1.25.28 INSTALLED, awaiting main-menu launch

DLL `aa24b8e6b7dc63ae1beb0d25f6f7d07252babaa544ad20b79ae9ae5dc059b636`; previous DLL and sidecar backed up exactly.
Fresh output local/overlay-dependencies-20261001. All four configurations unchanged.
Diagnostic dependency capture only; rendering admission unchanged. No live fix claimed.
57 native tests,54 Python tests,96 WARP fixture frames passed. No runtime work active.
Prelaunch 617 files,574 frozen sources, manifest `88b6993dc36c1248652432b754e66955bc7c2810f207c8856d8bfb8d37b93456`.
See installed-handoff.md and prepared-draw-dependencies.md. Next: main-menu arm,
load save,wingdrake capture. Keep DLL installed after normal exit.

# 2026-10-01: v1.25.28 dependency capture candidate, offline checks passed

Previous v1.25.27 run CLOSED/sealed. Read-only prepared-queue capture now records
per-draw targets, sampled resources, vertices/layouts and VS/PS bytes together.
Rendering admission unchanged; no wingdrake fix claimed. 57 native tests,54 Python
tests,96 WARP fixture frames passed. See prepared-draw-dependencies.md.
Deployment receipt, if present, is authoritative. Next: main menu then wingdrake.

# 2026-10-01: v1.25.27 multi-overlay test CLOSED and sealed

18,056 DLSS frames, zero runtime faults; wingdrake still suspends and recovers.
Two draws now enumerated, but first targets an auxiliary DXGI-24 texture rather than scene color;
geometry stage 4 phase 2 rejects it. Shared-key prepared/completed both zero.
Need per-draw input/output dependency proof; do not simply skip auxiliary work.
See multi-overlay-draws-result.md. External dependency snapshots: none accepted.
All GPU work completed/released; scale, jitter and native code restored. Normal exit verified.
DLL, sidecar and four configs unchanged. Keep v1.25.27 installed.
Archive local/multi-overlay-draws-20261001: 666 files; manifest SHA-256 `47c58677fc394fb621209b2c29eb75f65a181969eb0d358cebc3762055f4923d`.
No runtime work active. Redirect output sidecar to a fresh run before next launch.

# 2026-10-01: v1.25.27 ACTIVE, PID43144

Armed04:45:37 local,creation134352926375121966. CollectorPID9136,deadline05:45:37.
Run local/multi-overlay-draws-20261001;runtime-handoff-43144.md is current.
Main-menu baseline490DLSS frames,2startup skips,zero faults/overlay rejections.
Prelaunch622 files,seven module identities,four configurations verified.
User asked to load save and report clean/active; answer pending. Then wingdrake.
Do not stop a healthy run before the requested reproduction. Keep DLL installed.

# 2026-10-01: v1.25.27 INSTALLED, awaiting main-menu launch

DLL `3b0a49d099455ee70c677fc98b0242bb16a5dc4f54e33bff257f824a396e715a` installed in the MO2 test slot, exact
previous DLL/sidecar preserved. Fresh run: local/multi-overlay-draws-20261001.
All four configurations unchanged; no game or collector active.
Prelaunch manifest 622 files, SHA-256 `cc498872843f07781a4d1799a0b67bd6260b6cf2b50e2fcd310286915677df38`.
Use installed-handoff.md and frozen source-1.25.27 tools. Next: verify startup,
arm, load save, repeat wingdrake. Multiple-draw support is NOT live-verified.
Gallery color pass and silhouette remain unresolved. Keep current DLL installed.

# 2026-10-01: v1.25.27 multi-overlay candidate; offline checks passed

Previous v1.25.26 run is closed and sealed. Multiple overlay draws per key
now preserve proved native insertion order and qualify independently.
395 native sort replay cases, 56 native tests, 46 Python tests, 384 GPU
fixture frames passed. Gallery and silhouette remain unresolved.
See multi-overlay-draws.md. Deployment receipt, if present, is authoritative.
Next: main menu, arm, load save, repeat wingdrake travel. No game is open.

# 2026-10-01: v1.25.26 command capture CLOSED and sealed

Game closed normally; cleanup, installed DLL and four configurations verified.
7,632 DLSS frames, zero runtime faults; wingdrake rejects a second overlay draw
under the same key. Gallery has a separate intermediate color connection.
No visual fix claimed. See prepared-command-rejections-result.md.
Archive local/command-rejections-20261001: 653 files; manifest SHA-256
`ab1429233aa4f2604307e7756a2664cd529e9db6f8180c0c0c0d343b23a4bcb3`. Keep v1.25.26 installed; redirect its output
sidecar to a fresh run before the next launch. No game/collector remains active.

## ACTIVE v1.25.26 runtime (2026-10-01)

PID33364 / creation134352891968308322. Armed03:48:32 local. Collector41616,
60min cutoff04:48:32. Root local/command-rejections-20261001; see
runtime-handoff-33364.md. Baseline942 DLSS frames,3 skips,zero faults/overlay
rejections. User asked to load save; wingdrake reproduction next.
Do not deploy/restore while game open. Keep this DLL installed after test.

## v1.25.26 installation verified (2026-10-01)

Deployment complete, awaiting a fresh main-menu launch. Installed DLL SHA256
`74239e455632d8d5fa4e18b04f58835275128728c01ce4bd0a2eafe56ab029fe`.
Fresh output root: local/command-rejections-20261001. Previous exact DLL and
sidecar backed up; all four configuration hashes unchanged. 565 sources frozen,
prelaunch manifest verified. See installed-handoff.md in that root for arming,
capture inspection and clean-stop steps. No live test or fix claim yet.

# Prepared command rejection diagnostic: v1.25.26 (2026-10-01)

The 29-minute v1.25.25 run is closed and sealed. New code adds bounded exact
command-rejection receipts at the completed preparation hook. Rendering
admission is unchanged; Gallery/wingdrake/combat/silhouette fixes are pending.
56 native tests, 44 Python checks and 288 GPU fixture frames passed.
Candidate/deployment records: local/command-rejections-20261001. Check its
deployment.json for installation; do not launch into the sealed prior root.
Next reproduction: main menu, arm, load save, wingdrake; Gallery if healthy.
Native AA/DLSS/MotionBlur Off; DX12 windowed 2560x1440. Keep Stracker/profile
unchanged. Keep the current DLL installed after normal exit as requested.
See prepared-command-rejections.md for capture bounds and remaining work.

## 2026-10-01 - broad DLSS run CLOSED and archived; issues ready for fixes

User requested stop after about 29 minutes and confirmed normal appearance and
normal exit. Game/helper absence checked. 40,178 DLSS frames, 11,751 skipped
attempts, zero runtime/stream faults, zero request collisions; all 80 suspension
periods resumed. This is clean shutdown/telemetry, NOT a functional pass.
All 51,929 GPU signals completed; 51,928 private and 47,336 downstream records
released, none live. All 81,817 jitter changes/full resolution restored. Live
comparison verified 38 complete native spans and 18 hook heads restored.
Frozen full review accepted. All four configurations and DLL unchanged.

Evidence sealed in `local/dlss-long-run-20261001`: 666 files; manifest
SHA-256 8cd707342263673dfc501e8dad924a573fd8d64861bb930df57fd0461b97ac6d.
Collector: 351 snapshots/events, 55,989,685 bytes, 1,758.891 seconds. Nine status
read retries, no collector fault. Full timeline hash/monotonic/fault checks pass.
Two original silhouette screenshots preserved; Steam replay not supplied yet.
DLL v1.25.25 remains installed, SHA e84e443bfd8a32f62b892681a6e0949c6babd1912378999158d7c0d444bc117c.
Its output sidecar still names the sealed root; create/redirect to a fresh root
before any new launch. Earlier active-run notes below are superseded.

Three issue groups: Gallery intermediate color pass (reason 6/detail 4);
wingdrake/combat overlay rejection (reason 17/qualification 0, order 4100,
sometimes with haze); occluded-hunter silhouette size/placement. Specific
Safi/Alatreon attack sub-guard and silhouette draw remain unproven. Controlled
wingdrake repeat confirms dropout/recovery. Targeted reader obtained no stable
draw snapshot; capture opcode/count facts BEFORE narrowing draw types next.
See `dlss-long-run-20261001.md` for evidence, limits and recommended generic
renderer work. No new renderer build was made or installed during this run.

## 2026-10-01 - Alatreon nova skips captured; overlay plus haze combination

At 02:29:49 local user reports Alatreon nova skips. Latest graph about 02:29:39
again records reason 17/detail 0/qualification 0 with order-4100 overlay, now
alongside recognized order-20000 haze. 38,618 DLSS frames, 11,750 skips, zero
runtime/route fault, waiting=false; all 80 suspension periods resumed. Exact
rejected command/sub-guard still unresolved. Report and recent timeline saved
in local/dlss-long-run-20261001/report-alatreon-nova-17352.json.
Game PID 17352 and collector 48580 remain active; no stop requested. Broad
run continues; normal cutoff approximately 03:02:03 local. DLL unchanged.

## 2026-10-01 - Alatreon frost attack report; same overlay rejection signature

User suspects Alatreon's frost-down attack caused skips. At 02:25:45 local:
32,426 DLSS frames, 9,346 skips, zero runtime/route faults, waiting=false;
71 suspensions and 71 resumptions. Latest graph about 02:25:34 again records
reason 17/detail 0/qualification 0 with order-4100 overlay. Exact move timing
and specific failed sub-guard remain unproven; investigate shared handling.
Saved report-alatreon-frost-17352.json with last 90 seconds of timeline counters
in local/dlss-long-run-20261001. Game PID 17352 and collector 48580 continue;
no stop sent, no renderer changes. Normal cutoff remains 03:02:03 local.

## 2026-10-01 - wingdrake repeat reproduced and recovered; broad run continues

PID 17352 / creation 134352828027431998, collector 48580 still active. User
repeated wingdrake ride after ready signal and confirmed dropout/recovery.
Timeline independently shows steady baseline -> reason-17 overlay suspension
at about 02:19:57 -> recovery by 02:20:06 local. Report checkpoint: 23,110 DLSS
frames, 8,447 skips, zero fault, waiting=false. +969 skips and +969 overlay
rejections since pre-repeat baseline. Evidence saved in local/dlss-long-run-20261001,
issue-wingdrake-repeat-17352.json and wingdrake-repeat-timeline-17352.json.

Read-only reader PID 49976 finished its two-minute window normally, zero stable
snapshots. 17 attempts hit its bounded opcode-0 draw-count guard; actual native
opcode/count not retained. Exact guard/shader remains unproven. Future focused
capture should save bounded opcode/count facts before filtering on draw type.
No renderer changes or stop request. User continues broad monster tests;
normal long-run cutoff remains approximately 03:02:03 local. Keep DLL installed.

## 2026-10-01 - hunter silhouette scaling report captured; broad run active

User supplied two screenshots showing a small white occluded-hunter silhouette
while fighting Safi jiiva. Original PNGs/hash manifest are preserved under
local/dlss-long-run-20261001/hunter-silhouette-images. 1280x720 rendering into
2560x1440 output verified; an unmapped silhouette extent/coordinate is a
hypothesis, not a captured draw diagnosis. Its relation to order-4100 overlay
rejections is unproven. See issue-hunter-silhouette-17352 JSON/MD.
At 02:15:32: 16,100 DLSS frames, 4,293 skips, zero fault; waiting=true then.
No stop requested. Continue user monster tests; cutoff remains 03:02:03 local.

## 2026-10-01 - Safi attack skips suspected; no separate cause established

User suspects Safi jiiva attacks cause skips but is unsure. At 02:14:11 local,
13,780 DLSS frames, 3,862 total skips, zero fault, waiting=false. No new skips
since preceding checkpoint (+468 DLSS frames). Last rejection remains the same
order-4100 overlay, reason 17/qualification 0. Prior intermittent rejections
may overlap combat; do not attribute all of them exclusively to wingdrake.
See report-safi-suspected-17352.json in local/dlss-long-run-20261001.
Run/collector remain active; no stop sent. Continue broad test as authorized.

## 2026-10-01 - long run issue 2: wingdrake overlay rejection and recovery

User reports DLSS looked off during wingdrake travel then returned. Timeline
confirms suspension/recovery at about 02:11:41-02:11:56 local, followed by some
intermittent rejections. Zero runtime/route faults. Graph reason 17/detail 0,
qualification 0, overlay order 4100; exact failed guard unresolved. Different
receipt from Gallery reason 6/detail 4. Read-only follow-up had no stable shader
capture; native graph evidence is preserved. Latest: 13312 DLSS frames,
3862 skips, fault 0, waiting=False.

Run STILL ACTIVE: PID 17352 / creation 134352828027431998, collector 48580.
No stop requested, renderer unchanged. Continue broad monster tests; normal
cutoff about 03:02:03 local. Evidence in `local/dlss-long-run-20261001`:
issue-wingdrake-17352 JSON/MD, wingdrake-timeline-extract, checkpoints and
runtime-handoff. Keep DLL installed; no cleanup until user done/fault/deadline.

## 2026-10-01 - Gallery recovery confirmed; long DLSS run continues

At 02:09:20 local, user and telemetry confirm recovery after leaving Gallery.
5,546 DLSS frames, 2,392 total skips, zero runtime/route faults, waiting=false;
both suspension periods resumed. Six checkpoint reviews passed. User is now
roaming/testing different monsters. No stop requested. PID 17352 / creation
134352828027431998, collector PID 48580; deadline still about 03:02:03 local.
Evidence: `local/dlss-long-run-20261001`, checkpoint-gallery-recovered and
Gallery issue/recovery report. Gallery extra-pass issue remains unfixed and
saved for later analysis; exact cutscene title supplied as "trust hunbting
companion". Keep collecting; do not stop after a healthy checkpoint.

## 2026-10-01 - long DLSS run issue 1: Gallery cutscene suspension

PID 17352 / creation 134352828027431998 is STILL RUNNING; collector PID 48580.
No stop requested. User reports Gallery cutscene looks DLSS off. Captured
reason-6/detail-4 graph mismatch: an additional non-haze color pass sits between
motion and grading. DLSS suspended with zero runtime/route faults; no overlay
rejections. First rejection about 02:04:56.574 local. At 02:07:24, 4,535 DLSS
frames and 2,392 skips; recovery not established. Native graph evidence saved;
read-only shader capture had no accepted snapshot. No renderer changes.

See `local/dlss-long-run-20261001/issue-gallery-cutscene-17352.md` and JSON.
Pending user reply: exact Gallery cutscene and recovery after leaving. Keep
broad test going; normal collector cutoff remains approximately 03:02:03 local.
Use checkpoint.py with unique labels. Do not mistake accepted telemetry review
for successful reconstruction while reconstruction.waiting is true.

## 2026-10-01 - broad DLSS session ACTIVE (PID 17352)

PID 17352 / creation 134352828027431998; v1.25.25 unchanged. Evidence in
`local/dlss-long-run-20261001`. Prelaunch/startup/provenance checks passed.
Armed tick 11581484 at about 02:02:03 local (+03:00). Collector PID 48580
runs independently in the background, 60 minutes with 1 GiB timeline cap.
Scheduled normal stop at about 03:02:03 local (tick 15181484). User is being
informed of this cutoff; do not mistake it for a new rendering failure.

Main-menu baseline: 996 DLSS frames, two skips, zero runtime/route faults,
997 issued/completed signals, 996 overlay completions, zero overlay rejections.
Six checkpoint reviews passed. Collector alive, timeline growing, no stderr.
This is baseline evidence only. User will load save and play broadly; do not
stop at individual checkpoints. Preserve reported anomaly timing. Keep native
AA/DLSS Off and window size unchanged. See runtime-handoff-17352.md and
checkpoint.py in the evidence root for exact tools, identity and cleanup steps.
Keep current DLL installed after the run. Earlier prepared state below is
superseded by this active run; prior sealed evidence remains unchanged.

## 2026-10-01 - broad 60-minute DLSS test prepared; awaiting main menu

User authorized the longer regression. Tested v1.25.25 remains installed,
unchanged: e84e443bfd8a32f62b892681a6e0949c6babd1912378999158d7c0d444bc117c.
Game is closed and MO2 remains open. Fresh evidence root is
`local/dlss-long-run-20261001`; output sidecar redirected there with exact
previous bytes saved. Native AA/DLSS/MotionBlur Off, DX12, Windowed 2560x1440,
barebonesdlss with Stracker; all four configuration hashes remain unchanged.

Only the host collector changed: optional bounded capacity up to 1 GiB and
streamed final hashing. 23 tests passed, including a simulated full hour.
Use the separately frozen `session-tools/collect_continuous_session.py` with
`--minutes 60 --timeline-mib 1024`; original source-1.25.25 holds build-time
sources and still contains the old collector. Use that original tree for
arming/review. All 560 sources and three session tools verified. Prelaunch
manifest (579 files): 9dbd9ad11af487be7eb6b042429e2dc847af3ae8c27b5697ccd2db48d14b01e5.

Next: verify actual startup and process identity, arm at main menu, start the
long collector, verify baseline, announce its 60-minute cutoff and allow broad
solo/offline play with Steam recording. Do not stop after one checkpoint.
Coverage: cutscenes, rain/smithy/sauna, monster effects/nova, quest abandonment,
area loads, panning and menus. No runtime result yet. Follow installed-handoff.md
for cleanup and archive; keep the DLL installed. Prior root remains sealed.

## 2026-10-01 - v1.25.25 cutscene layout support passed live; run closed

The affected cutscene now retains DLSS with no rectangle. User confirmed clean,
active reconstruction while walking/panning in Castle Schrade afterward. The
live 28-byte layout has four descriptors: POSITION0 float2 at offset 0 and
TEXCOORD0 float2 at offset 8. 800 recorded overlays used it; all 3,148 overlay
draws completed with zero overlay rejections or runtime/stream faults.

Intentional stop: 6,346 DLSS frames, all 6,353 GPU signals completed; 6,352
private and 6,408 downstream recordings released, none live. All 6,817 jitter
changes restored, full resolution returned, and 38 complete native functions
plus 18 hook heads verified restored. Frozen full review passed. User confirmed
normal appearance/normal exit; game absence and unchanged configuration checked.
Seven total skips, five resumed suspensions and two reason-6/detail-4 graph
refusals remain recorded; this targeted success is not universal scene coverage.

PID 13240 / creation 134352812940432461. Current DLL stays installed:
e84e443bfd8a32f62b892681a6e0949c6babd1912378999158d7c0d444bc117c.
Evidence sealed in `local/overlay-layout-support-20261001`; use a new root for
further work. See `overlay-vertex-layout.md`, result-13240.json and runtime
handoff for details. No game currently running; MO2 remains available. Native
AA/DLSS/MotionBlur Off, DX12, Windowed 2560x1440; Stracker/profile unchanged.

## 2026-10-01 - v1.25.25 native overlay vertex layout installed; live test pending

The confirmed 28-byte cutscene stride now uses bounded native POSITION0/UV0
layout decoding instead of a packed-16 assumption. GPU buffer retention also
checks the exact qualified stride/offset and complete draw range. Native
vertices, PSO, shaders, UVs, bindings and blending remain unchanged; the same
qualified overlay route expands only viewport/scissor. No effect-name patches.
See `overlay-vertex-layout.md` for supported layouts, bounds and telemetry.

Warning-free Release; 56 native checks, 41 Python tests, 384 continuous GPU
fixture frames and WARP/RTX 5090 pixel comparisons passed. First padded fixture
reruns exposed/fixed its own old 64-byte buffer-rotation helper; logs retained.
Historical v24 and synthetic v25 schema reviews passed. Live cutscene remains
unverified. v24 run is sealed (641 files) in `local/overlay-layout-20261001`.

Installed DLL e84e443bfd8a32f62b892681a6e0949c6babd1912378999158d7c0d444bc117c.
Evidence/frozen sources/backups: `local/overlay-layout-support-20261001`.
Game closed at installation, MO2 remains open; profile/settings unchanged.
Next: main-menu arm, save load, affected cutscene, recovery, announced stop,
native cleanup verification and normal exit. Keep this DLL installed afterward.

## 2026-10-01: cutscene stride mismatch confirmed; v1.25.24 run closed

Evidence: `local/overlay-layout-20261001`, PID 45364 / creation
134352792900580032. Native vertex stride is 28; our gate expects 16.
Qualification 327683 / read mask 1 / predicate 2. Four vertices, first vertex 0.
Offset and arena were NOT read. Attribute offsets/formats remain unproven;
external readers yielded zero stable layouts. DLSS recovered after the cutscene.

Clean intentional stop: 4,671 DLSS frames, zero faults, all 6,353 private GPU
recordings and all downstream references released, all 6,799 jitter changes
restored, full resolution returned. 38 native spans and 18 hook heads verified;
user confirmed normal exit. Current DLL remains installed, settings unchanged.
Next: structural native-layout support; do not simply replace the stride check.
No new candidate prepared. See `docs/research/overlay-vertex-gate.md`.

## 2026-10-01: v1.25.24 installed and ready for cutscene capture

Current test root: `local/overlay-layout-20261001`; frozen tools under
`source-1.25.24/tools`. DLL SHA-256: `0d7bba4a3efd36c5c4d397ba39789195be62214368cbc94f737fec6506004ab8`.
Previous DLL/output sidecar preserved; profile and native AA/DLSS Off settings
unchanged. Verify startup then arm within five minutes. Replay the affected
cutscene once; capture vertex_gate operands, verify recovery, then announce
and stop. No roaming test is needed. Keep this DLL installed.
This is diagnostic only; no cutscene fix claimed. Live test pending.

## 2026-10-01: v1.25.24 cutscene operand recorder prepared

Next build records the existing stride/offset/CPU-arena gate values at the
cutscene rejection, including successful-read bits and the exact failed
predicate. No extra process reads or rendering-policy changes. It is a
diagnostic build, not yet a cutscene fix. Evidence: `local/overlay-layout-20261001`.
Passed warning-free build, 55 native checks, 35 Python tests and 288 GPU fixture
frames. Historical v23 review passes; five corrupted v24 examples reject.
See `docs/research/overlay-vertex-gate.md`. Next runtime test needs only the
main menu, affected cutscene, and recovery. Keep the latest DLL installed.
Deployment status is recorded separately in the evidence directory.

## 2026-10-01: v1.25.23 live test closed; current DLL remains installed

Evidence: `local/overlay-inherited-20260930`, PID 40428 / creation
134352781643765871. User ended testing; intentional stop and game absence
verified. Full review accepted accounting/cleanup: 9,384 DLSS frames, zero
runtime/stream faults, 5,855 completed overlay draws; all 12,596 private GPU
recordings and 9,962 downstream references released, all 13,277 jitter changes
restored, full resolution restored. Live checks verified 38 native function
spans and 18 hook heads. Settings/profile unchanged; v1.25.23 stays installed.

Castle Schrade shutdown did not recur. Cutscene is NOT fixed: repeated
qualification 327683 = stage 5 / check 3 (stride, offset, CPU arena), with DLSS
recovery afterward. All three read-only capture attempts produced zero stable
snapshots. Next: capture the existing check-3 operand values; do not guess the
layout or loosen admission. No new candidate has been prepared.
See `docs/research/overlay-native-state.md` and the sealed local run result.

# Current handoff - 2026-10-01 v1.25.23 INSTALLED and READY

Game confirmed absent; user authorized installation. Exact v1.25.22 DLL and
output sidecar backed up. Stracker-enabled barebonesdlss settings unchanged:
DX12, Windowed2560x1440, native AA/DLSS/MotionBlur Off.
Installed mod DLL SHA 91d6113e634cd99a6b078e055f3e2a65f0bfae88ca3b14c100b772879797cc82.
Root local/overlay-inherited-20260930; frozen source-1.25.23 (557 files).
preparation.json, deployment.json, prelaunch-verification.json and manifest
establish readiness. Native spans38 rechecked against the recorded PE.
Use frozen tools for arm/review; current-state.md is intentionally newer than
its immutable build-time source snapshot. No live v1.25.23 run yet.

Next launch: same MO2 test entry, report MAIN MENU. Verify startup/process
identity and arm promptly within the five-minute readiness window. Record
60min bounded session. Check save loading, cutscene qualification and Castle
Schrade idle/roaming. User wants to continue roaming/showing friends after
checkpoints; do not intentionally stop merely after a checkpoint. Announce
stops and preserve evidence on faults. Keep installed DLL after tests.

Candidate permits native state inheritance only for qualified in-place
overlays; private effects retain local-state requirements. Contradictory
states, changed bindings/targets and submission/ownership failures reject.
Cutscene correction is NOT proven; new bounded qualification details identify
its failed geometry/input check. Offline55 native/85 Python/19 integrated
corruption checks and12 GPU checks passed (672 final continuous fixture frames).

Previous v1.25.22 PID17396 is closed and SEALED after12965 DLSS frames,
runtime45/stream6 at an overlay with unknown local output state. User was idle
in Castle Schrade. Save load and first quest-abandon passed, cutscene rejected.
GPU15967/jitter17485 and native38 spans/18 heads verified clean;two downstream
pins held until exit. Exit method not separately confirmed; process absent.
Archive local/copy-recording-20260930,680 files,SHA
2226b27eabf9a8866ea6e0331c1bc1f4584023f1adc781baabc3e9105ae62359.
Last fully accepted live run remains1.25.16.
See docs/research/overlay-native-state.md. Restore only when requested with
local/overlay-inherited-20260930/deploy.ps1 -Action Restore, game closed.

# Current handoff - 2026-09-30 v1.25.23 BUILT, NOT INSTALLED

v1.25.22 process17396 / creation134352731393638593 failed after12965 DLSS
frames: runtime45/stream6, unknown local output state at overlay stage4.
User was idle in Castle Schrade; no nova or menu transition. No stop requested.
Save load and first quest-abandon passed. Cutscene overlay remained unqualified
(2924 rejected plans); no rectangle visually is NOT a DLSS cutscene pass.
3071 overlay draws did complete elsewhere. Full GPU/jitter/native cleanup
verified;two downstream recording pins retained until process exit.
Game remains open at packaging. Do not replace DLL until it is closed.
Current installed v1.25.22 remains SHA 1e4204f95fd592a2ecacae414b7fd4363f152b639c870bedd92d891c9c0a75f8.
Evidence local/copy-recording-20260930 is NOT yet sealed; preserve it.

v1.25.23 candidate preserves native state inheritance only for qualified
in-place overlays, whose draw/resource/state footprint stays native. Private
replacement effects still require observed local state; contradictory state,
RTV, bindings, ownership and submission failures still reject. Added bounded
geometry qualification details for the separate cutscene rejection.
Offline repro failed before and passes afterward;55 native tests,85 Python
tests,19 integrated corruptions,12 GPU checks pass. Includes672 final continuous
GPU fixture frames across WARP/hardware;no live v1.25.23 verification.
Candidate SHA 91d6113e634cd99a6b078e055f3e2a65f0bfae88ca3b14c100b772879797cc82; root local/overlay-inherited-20260930.
Frozen source-1.25.23 and candidate manifest are ready for later deployment.
After normal exit: close/archive v1.25.22 evidence, verify unchanged configs,
back up exact DLL/sidecar, deploy candidate, prepare installation/prelaunch
receipts, then main-menu test. Do not arm old v1.25.22 again unintentionally.
User asked to keep runs active for roaming/showing friends; no stop merely
because a checkpoint passed. Existing collector is60min; previous run faulted
well before that bound. Keep Stracker and current settings;AA/DLSS Off.
See docs/research/overlay-native-state.md. Last accepted live run isv1.25.16.

# Current handoff - 2026-09-30 v1.25.22 READY for runtime verification

v1.25.21 is sealed after2647 DLSS frames, then a load-transition failure:
runtime45 / stream5. No overlays exercised. All2752 private GPU signals and
records completed/released,2754 jitter changes restored, full scale and native
hook bytes restored;one downstream record held until process exit. User
confirmed normal scene and normal exit. Stracker/settings preserved.

v1.25.22 fixes a demonstrated integration gap: copy_override previously
required an existing plan-local recording, although the state journal knew
both copy states for a new command list. It now imports that exact generation's
observations before checking the unchanged copy/state/identity predicates.
GPU reproduction failed before correction and passed96 frames afterward with
resolve and copy on separate lists, all24 CPU orders and split GPU batches.
Exact in-game predicate was not logged; replay the load before claiming fixed.

55 focused native tests, 84 Python tests,19 integrated
corruptions and12 GPU checks passed, including invalid states/caller/packet/
box/duplicate guards, overlay/haze routes, shutdown and WARP copy-first route.
No overlay algorithm or native hook change. Last accepted live run remains1.25.16.

Evidence local/copy-recording-20260930; frozen source-1.25.22.
DLL SHA 1e4204f95fd592a2ecacae414b7fd4363f152b639c870bedd92d891c9c0a75f8. Deployment/prelaunch receipts establish installed readiness.

Next: same MO2 test entry, barebonesdlss with Stracker, native AA/DLSS/MotionBlur
Off, Windowed2560x1440. MAIN MENU ready; verify startup and arm promptly. LUID
may change across reboot (current93877). Collect60min/128MiB; verify load, then
cutscene rectangle and quest-abandon return; continue agreed broad effects test.
Announce intentional stops. Terminal log is authoritative after faults.
Keep current DLL installed. See docs/research/copy-recording-registration.md.

# Current handoff - 2026-09-30 v1.25.21 runtime CLOSED (failed)

PID8860 main-menu baseline ran2647 DLSS frames, then stopped while loading a
save. Runtime fault45 / stream fault5 (color-copy route), zero overlay draws.
No overlay correction exercised; do not call the cutscene rectangle fixed.
No copy predicate detail exists yet: null plan-local recording is a source-code
candidate, not a proved diagnosis from this trace. New journal observes without
creating plan entries; draw imports it, but copy_override still requests an
existing entry only. Reproduce a copy-first recording offline before changing.

All2752 private GPU signals completed and private records released,2754 jitter
changes restored, full resolution returned. Live checks passed38 full native
spans and18 hook heads. One downstream record retained until process exit.
User confirmed normal scene and normal exit. Latest DLL kept, Stracker/settings
unchanged. Frozen source-1.25.21 and all prelaunch files verified.

Evidence local/output-recording-20260930 is SEALED. Create a fresh root for the
next candidate. Installed hash941dcf6687377999e0466a232672cbe2af9305141684c5c4524eccb7694ddb16.
Last fully accepted live version remains1.25.16. Continue generic state/overlay
work, then cutscene and quest-abandon checks and broad effects testing.

# Current handoff - 2026-09-30 v1.25.21 candidate ready for runtime verification

Prior v1.25.20 broad run is sealed. Smithy and rain looked clean; the cutscene
rectangle remained. Quest abandonment triggered output state mask2, runtime
fault45/stream fault6. GPU signals and jitter drained;2 downstream records
remained pinned until process exit. It was not an intentional stop.

This candidate observes native output transitions outside selected callbacks,
scoped to exact command-list Reset generations. It also includes qualified
in-place UV-based overlays in the immutable output plan and expands their
viewport/scissor on the reconstructed branch, retaining native PSO/blend/UVs.
No effect-name exceptions, new native hooks or game-setting changes.

55 focused CTest cases, 84 Python tests,19 integrated
corruption cases and11 GPU checks passed. The vignette shader matches an
independent full-size draw at every pixel, and its half-size control reproduces
the rectangle. Native/full overlay routes and unscoped transitions pass96-frame
ordering/lifetime tests. Earlier failed fixture attempts are retained.
See docs/research/recording-state-and-overlay-extent.md for limits and evidence.

Evidence local/output-recording-20260930; frozen source-1.25.21.
DLL SHA 941dcf6687377999e0466a232672cbe2af9305141684c5c4524eccb7694ddb16. Deployment/prelaunch receipts establish installed readiness.
No in-game result yet. Last fully accepted live version remains v1.25.16.

Next: same MO2 test entry, Stracker enabled, native AA/DLSS/MotionBlur Off,
windowed2560x1440. Start at MAIN MENU, verify startup and arm promptly.
Collect60min bounded128MiB; check cutscene rectangle and quest-abandon return,
then continue broad effects testing. Do not stop simply after a checkpoint.
Announce any intentional stop. Use terminal main log after failure because
periodic status may retain a stale active flag. Keep this DLL installed.

# Current handoff - 2026-09-30 v1.25.20 broad run CLOSED (failed)

User confirmed normal scene and normal exit; PID53880 is absent. Keep v1.25.20
installed and preserve Stracker-enabled profile/settings. Evidence root
local/copy-branch-output-20260930 is SEALED; use a fresh output root before
another launch. No new build prepared. Last fully accepted run remains1.25.16.

Depth branch correction held in smithy/rain:15425 admitted copies,2 schedule
rejections,zero invalid-child/node rejections. User reported both scenes clean.
Runtime completed15314 DLSS frames and2 spatial frames before an actual fault.

Cutscene screenshots show a top-left1280x720 rectangle on2560x1440 output.
Captured actual vignette shader (type RVA0x3526650) used that reduced rectangle
against a full-size target. Adjacent god-rays composite RVA0x3536c98 also used
reduced rectangle. Actual shader contents establish their names; screenshots
are not matched to exact telemetry frames. Strong visual-cause candidate,
not yet verified by a corrected rendering comparison. Fix post-resolve extent,
geometry and sampling coherently; do not globally resize intermediates or
patch scene/effect names.

User reproduced shutdown by ABANDONING QUEST; animation and menus appeared.
Output fault6 now isolated to mask2: final stage2 texture state UNKNOWN on
command-list generation220910/frame32246. Output resource/RTV/pipeline/root
all match. No invalid known GPU state or SDK error established. Barrier tracker
currently scopes observations to selected callbacks; missing observations and
cross-recording state inheritance remain candidates, not proven causes.

Runtime45, full reviewer rejects clean stop. Diagnostic channels pass17 native
statuses/109 timeline snapshots. All15427 GPU signals/private recordings
completed/released,15886 jitter changes restored,full scale restored. Live
reads verified38 native function spans and18 hook heads restored. Two downstream
records remained pinned until exit. Status sidecar is stale; terminal main-log
record gives the completed cleanup. No assistant/collector stop request.

See docs/research/copy-branch-output-broad-run-result.md and the archived result.
Next: output-state lifetime/observation correction and shared downstream sizing.

# Current handoff - 2026-09-30 v1.25.20 READY for runtime verification

Prior v1.25.19 broad run is sealed and failed: smithy/rain depth rejection,
nova/cutscene rectangle report, then output fault46/stream fault6. User wants
shared fixes rather than individual scene patches. Game closed; keep Stracker.

v1.25.20 corrects opcode10 depth traversal: native direct-copy/skip branches do
not execute child state lists. Parent transitions, writer child, double-read
identity and all state checks remain. New copy_paths receipts prove branch
coverage. Native branch gate remains0x259df30/681bytes. No new hook.

Output guard now records exact predicate bits and compared bindings/states.
Prepared-queue captures add target/shader metadata for all sampled post draws.
An extra native overlay used1280x720 on2560x1440 during nova; resize/UV remedy
still unproven. Output fault and effect rectangle are NOT claimed fixed.

53 focused CTest cases, 72 Python tests and15 integrated
corruption cases passed. Six96-frame GPU routing runs passed (five5090, one
WARP), plus real forwarding and fault-shutdown fixtures. An accidentally broad
645-test invocation was stopped at an unrelated historical case; its partial
log is retained separately, not called a pass. Builds warning-free.

Evidence local/copy-branch-output-20260930; frozen source-1.25.20.
DLL SHA bf0437b8a759585a5c101a308324bf3e3328c5c9f043eb0f621567999426c97a. Deployment/prelaunch receipts determine installed readiness.
Preserve current DLL after testing and user-enabled Stracker configuration.

Next: same MO2 test entry, native AA/DLSS/MotionBlur Off, unchanged window,
MAIN MENU ready. Check startup, arm frozen tools promptly and collect60min with
128MiB bound. Verify baseline, then smithy/rain depth correction; continue
attacks/effects/cutscenes without stopping merely after a checkpoint. Stop on
user completion, actual fault or disclosed bound; announce intentional stops.
Use fresh .status.json while active; terminal main-log status after shutdown.
Preserve Steam recording, report timestamps and any new faults.

See docs/research/native-copy-branch-admission.md. Last fully accepted live
version remains v1.25.16; this build has not yet run in game.

# Current handoff - 2026-09-30 v1.25.19 broad run CLOSED (failed)

User requested the same run continue beyond smithy through rain, monsters,
attacks and cutscenes, with Stracker's Loader enabled. This run ended on an
actual fault after about 16m18s, NOT an intentional checkpoint stop or timer.
PID54508 is absent; user confirmed game closed. Keep v1.25.19 installed and
preserve the user-enabled Stracker profile. Sidecar points at a sealed root;
prepare a fresh output root before the next launch.

11092 DLSS frames,15954 spatial fallback frames. Smithy and rain share depth
reason6 bad-node traversal (15956 occurrences; plus2 schedule rejections).
Opcode10 nested traversal is unconditional in our reader but conditional in
native execution. Fast-copy child-state hypothesis remains unproven for an
exact failing parent; do not ignore invalid nodes or weaken depth invariants.

User reports a 16:9/720p-sized effect rectangle/bar during Fatalis nova and an
earlier cutscene. Screenshot and timestamped checkpoints saved. Suspected
effect extent mismatch is not a confirmed cause; exact video path pending.

Runtime then faulted46, route fault6: stream draw order or state. Collector
first observed at08:43:45.325 local. Frozen full review rejects clean stop.
Separate shader accounting and depth-receipt review pass18 native statuses
and195 timeline snapshots; this does not reclassify the failed run.

Fault shutdown restored full scale, all27503 jitter changes and native code
(38 function spans,18 hook heads independently read). All27048 private GPU
signals completed. One private and two downstream recording records remained
pinned until process exit; do not claim all resources released before exit.

Evidence local/depth-interval-detail-20260930 SEALED. See
docs/research/depth-interval-broad-run-result.md. No new build prepared.
Next: fix/prove native opcode10 child traversal, isolate output-state predicate
that failed, and trace effect rectangle propagation. Preserve structural
solutions rather than effect-name patches. Last fully accepted live version
remains v1.25.16.

# 2026-09-30: v1.25.19 smithy depth-rejection diagnostic ready

v1.25.18: 7718 DLSS +156 full-size spatial frames, zero runtime/route faults,
all GPU work retired, all jitter/scale restored, 38 native functions/18 heads
verified restored. User reported sustained DLSS-off look in SMITHY BEFORE
intentional stop. Source frame return correction held; visual issue remains.
Frozen full reviewer rejected combined preparation/dispatch shader accounting.
Do not retroactively call that run accepted. Its 610-file archive is sealed.

v1.25.19 separates preparation shader counters/cache from dispatch and adds
bounded depth failure receipts: first8 +latest failed attempt, exact packet/node
site from existing reads. Admission/state checks, routing, hooks and temporal
inputs unchanged. This is targeted evidence for a shared depth check, not a
smithy effect patch or claimed fix. 154 prior depth rejections were interval
inspection reason6; exact failing packet was previously missing.

52 CTest targets and54 Python tests passed;12 integrated corrupted logs rejected.
Actual forwarding and fault-shutdown fixtures passed on RTX5090. No new GPU
routing algorithm; prior576-frame routing matrix remains archived evidence.
Build/source/settings/executable checks included in preparation and deployment.
Local root local/depth-interval-detail-20260930, frozen source-1.25.19.
DLL SHA e417154259690c46195d0aa2c615fc4953af1adbf716ff24f3d533f343476439; deployment receipt is authoritative. Exact previous DLL/sidecar
preserved, settings unchanged. Keep latest DLL installed after tests.

Next: same bare offline MO2 test entry, MAIN MENU ready; native AA/DLSS and
MotionBlur Off, DX12 Windowed2560x1440. Validate startup, arm frozen tools promptly,
start collector. Load save and enter SMITHY; stay at affected spot to capture
failure_diagnostics before leaving. Then compare recovery and stop normally;
verify all native hook bytes/jitter/full scale. Use current .status.json for
checkpoints: the bounded main log may lag between its periodic samples.

See docs/research/depth-interval-diagnostics.md and prepared-frame-return-result.md.

# 2026-09-30: v1.25.18 closed normally; smithy visual issue remains

7718 DLSS +156 spatial frames, zero runtime/route faults. All7877 private GPU
signals/recordings and7910 downstream recordings completed/released. All8656
jitter changes and full scale restored; live audit38functions/18heads native.
59 early CPU draws,36 multi-list frames; no abandonment. User reported smithy
DLSS-off look BEFORE intentional stop, despite prior clean pan/menu/sauna/load
reports. Do not call visual issue fixed or full run accepted.

Frozen reviewer rejects shader admission accounting: preparation+dispatch now
share accepted counters, reviewer only counts dispatch. Final15754=7877+7877;
all retained samples reconcile with dispatch checks+prepared publications.
Next separate counters and exercise preparation/dispatch. Original rejection
preserved, no log normalization. Depth rejected154 interval checks(reason6),
one schedule(reason5). Failing receipt state0/nodes1 narrows to packet/node
reading before state replay, but exact packet/condition missing. Add bounded
rejection details and reproduce smithy; do not patch effect names or relax states.

Evidence local/frame-output-return-20260930 SEALED, installedv1.25.18 retained.
Sidecar points at sealed root: prepare fresh root before relaunch. Settings
unchanged. See docs/research/prepared-frame-return-result.md. Last full accepted
live run remainsv1.25.16; v18 forwarding/cleanup observations are independently
verified but visual issue and frozen review acceptance remain open.

# 2026-09-30: v1.25.18 entry-packet and fault-shutdown correction ready

v1.25.17 FAILED at the main menu after one DLSS frame: first stream fault 8,
later runtime fault 45. All 357 private GPU signals/recordings completed and
released, 358 jitter changes and render scale restored, but 17 of 18 native
hook heads remained installed and one uncertain route recording stayed pinned.
User confirmed normal scene and normal exit; no live patch/unload was used.
Failure root local/frame-output-20260930 is sealed (750 files), archive SHA
addabc746e26d5fbe1930a183908dd8799b12f3524b2ce8ada9a5d6e79757db4.

Confirmed ABI bug: the native forwarding wrapper saves return registers into
the frame, so its volatile return R8 cannot identify the original entry packet.
v1.25.18 preserves that packet separately. Fault shutdown can remove interception
after callbacks return while retaining all uncertain pins and the original
fault. No completion or retirement is invented. Telemetry exposes abandoned;
successful-run reviewers require false. No resource/state validation relaxed.

Offline verification: warning-free DLSS/DLAA builds, 50 native CTest targets,
40 Python tests, 9 integrated corruption rejections; both actual forwarding
and fault-shutdown fixtures pass on WARP and RTX 5090. Production-route GPU
regressions: 576 frames, reordered recording, stop, alternating haze/aliasing.
No game/NGX execution in fixtures. The prior auxiliary-texture validation
discrepancy remains documented. Last accepted LIVE run is still v1.25.16.

Current prepared root: local/frame-output-return-20260930; source-1.25.18.
DLL SHA 06f65da264637ce491e605f5bda4bca56a5045eb899a99d53b390e6e94734a67. Deployment receipt is authoritative for installation.
Exact previous DLL/sidecar preserved. Keep latest DLL installed after testing.
Settings unchanged: native AA/DLSS/MotionBlur Off, DX12 Windowed2560x1440.
Same bare offline MO2 test entry. Start MAIN MENU, validate startup and arm
frozen tools promptly, start collector, verify more than one successful frame
before loading save. Then pan, menus and area transition. Stop audit remains
38 function spans/18 native heads, all jitter and render scale restored.
Do not call the structural route or skips fixed until runtime evidence passes.

See docs/research/prepared-frame-return.md and prepared-frame-output-first-run.md.

# 2026-09-30: v1.25.17 first runtime FAILED; closed normally

PID15060/134352126976240901: one main-menu DLSS frame, then stream fault8.
Later error text: stream draw order or state. Shutdown fault45: unresolved
prepared frame prevented hook removal. 357 private GPU signals completed and
recordings released, 358 jitter restored, full scale restored, but17/18 native
hook heads still installed. Independent live audit confirmed; no memory writes.
User reported normal scene and exited normally; process absence checked.
Keep v1.25.17 installed as requested, settings unchanged. Evidence SEALED in
local/frame-output-20260930; sidecar still points there. No relaunch there.

Code bug found: native_seh uses f->call.gpr[3] AFTER mhw_binding_forward SAVEs
native return values, including volatile R8. Must preserve entry packet value
for finish_resolve, or TLS resolve_slot can survive into the next frame and
fault8. Later draw-state error is not proof of independent state-inheritance
failure. Add real forwarding-wrapper clobber test and failure cleanup test;
remove hooks after fault while retaining uncertain resources, don't fake drain.
See docs/research/prepared-frame-output-first-run.md. Offline v17 checks passed
but v17 is NOT runtime accepted; last accepted live result remains v1.25.16.

# 2026-09-30: v1.25.17 prepared frame output ready for runtime verification

User requested a structural fix for DLSS/DLAA skips. v1.25.17 prepares the
qualified output chain before CPU recording workers, records native/full
downstream choices under an immutable per-frame GPU predicate, and proves
logical GPU order across list generations and split batches. Full-size spatial
recovery is available when current color/output is valid but temporal inputs
are missing. Busy private recordings retain original-size output consistently.
Unscaled DLAA is unchanged; shared runtime/observer regression checks pass.

Known motion/haze/grade/final adapters remain: this is not universal unknown
effect support. No game proof yet. Last accepted live run is v1.25.16, 3,224
DLSS frames and 32 unfiltered frames (29 output-readiness, 2 warmup, 1 depth).

50 native test targets, 40 Python tests, 8 integrated corruption cases passed.
Production-route GPU matrix: 1,152 frames, WARP and RTX 5090, plus six rejected
copy contracts and one rejected missing-Close receipt. Generic GPU model adds
144 frames with exact pixels and actual spatial shader recovery. No NGX or
game run in these fixtures. An isolated GPU-validation discrepancy on a tiny
auxiliary texture remains documented; explicit debug-state assertions pass
and the instrumented matrix is clean. No diagnostic warnings were filtered.

Local root: local/frame-output-20260930; frozen source-1.25.17.
DLL SHA d4a278916b94a2b277fabeff75afc02cff7cd002d5826514a4e5da812bd9580d.
Deployment receipt is authoritative for installation. Previous DLL and sidecar
must be preserved; do not run with the sidecar pointing to sealed v1.25.16.
Keep this latest DLL installed after tests as requested. Settings unchanged.

Next: same bare offline MO2 test entry, MAIN MENU ready. Native AA/DLSS and
MotionBlur Off, DX12 Windowed2560x1440. Validate startup identity/hash, arm
frozen tools and start collector promptly. Route/readiness schema 3; verify
early CPU draws, multi-list frames, immutable choices, downstream-only fences,
full retirement, no faults. Then load save, pan/open menus and change area.
Stop verification includes new native copy hook 0x259c650 (18 native heads,
38 function spans), jitter/scale restored and all owners disabled. Never call
unknown effects or visible skipping solved before runtime evidence.

See docs/research/prepared-frame-output.md for ABI, limitations and removal.

# 2026-09-30: v1.25.16 capture CLOSED; shared output migration next

PID2596/134352074187384157 closed normally; user reported clean loaded scene
and normal scene after intentional stop. Latest DLL kept installed, settings
unchanged. No game/collector active. Evidence SEALED in
local/prepared-color-chain-20260930; installed sidecar still points there.
Do not launch again without a fresh evidence destination.

Main-menu baseline:650DLSS,651accepted depth copies,zero rejection. New depth
rule exercised live in combined-read192 and already-begun read intervals.
Final:3224DLSS,32unfiltered=29output CPU-order+2warmup+1depth rejection.
No runtime fault;9suspensions/9resumptions. One depth rejection's detailed
receipt was not retained; don't guess cause. Technique23086 used throughout,
29842 variant remains unexercised. All3255GPU recordings completed/released,
3643jitterchanges restored,fullscale restored,allhooks disabled. Independent
38code spans/17hook heads restored; frozen full reviewer accepted.

Prepared queue:26accepted captures,25distinct snapshots retained,2arrangements.
Main menu extra pre-temporal phase15/order1,typeRVA353e048; gameplay omits it.
No inferred effect name. Scratch owners swap roles; native postdraws1280x720
inside2560x1440allocations; finaldraw2560x1440. Logical chain connected.
Recorded57hazeroutes but sampler didn't retain that brief arrangement.

Output receipts mask6(grade+final) or4(final) already CPU-recorded; GPU
submission samequeue/batch,temporal index1,downstream index2. Existing late
claim must migrate to early prepared dimensions/admission/ownership; do not
remove guard,add named effect patch,or claim shared output already implemented.
See docs/research/prepared-color-chain-result.md and shared-renderer-boundary.md.
Future effects side project documented in future-native-effects.md,parked.
Current installed SHA82260448154c3798347af46bb5f159d189d5edac7d6ae1ea5d2aaf1db5437d77.

# 2026-09-30: v1.25.16 prepared color-chain diagnostic ready

User requested structural output integration, no more effect-specific patches.
This build includes v1.25.15 depth read-interval correction and a read-only
shared postqueue snapshot before the current route's named-effect admission.
Captures every descriptor's links, allocation dimensions, prepared draw
rectangles/opcode counts, context rectangle and scale domains. No new hooks,
COM/GPU work or shader adapters; current output route is STILL the prototype.
Do not present graph adjacency as a GPU binding/state/lifetime proof.

47 native targets, 46 Python tests and 8 integrated corruption cases passed.
Prior v1.25.14 archive verified (587 files); all 38 code gates verified.
Evidence local/prepared-color-chain-20260930, source-1.25.16.
DLL SHA 82260448154c3798347af46bb5f159d189d5edac7d6ae1ea5d2aaf1db5437d77.
Deployment receipt in that root is authoritative for installation status.
Exact old slot/sidecar are preserved; no config changes. Keep latest DLL after
runs as requested. Restore is available but points back to a sealed folder.
No game or collector running at preparation; no v1.25.16 runtime proof yet.

Next launch: same bare offline MO2 test entry, MAIN MENU ready. Keep native AA,
DLSS, MotionBlur Off, DX12 Windowed2560x1440. Validate startup DLL/hash/root,
arm frozen arm_continuous_dlaa.py and start collector promptly. Read the new
scaled_route.prepared_queue and consumer_depth(schema2), then request loaded
save. Capture ordinary scene graph; no per-effect safari. Missing sampler
capture is a diagnostic issue, not automatic failure of DLSS. Existing depth/
SDK/output/stop verifiers still mandatory; readiness may still reject final
CPU-recorded-before-temporal plans. Do not delete that guard.

See docs/research/prepared-color-chain.md and shared-renderer-boundary.md.
Last live result remains v1.25.14 (main menu combined-depth rejection,
rain-reported begun-read-span rejection, loading output CPU-order skip).

# 2026-09-30: shared renderer work, v1.25.15 OFFLINE ONLY

User requested a structural solution instead of effect-by-effect exceptions.
First implementation: complete scheduled depth read intervals, accepting the
proven RT/compute/pixel/combined state at the consumer, including already-begun
read spans and multiple transitions. Intervening writes, incomplete/ambiguous
nodes and conflicting API state remain rejected. Copy restores exact state.
No new hook, game run or installation. Unscaled DLAA runtime path unchanged.

48 native tests, 35 Python tests and 8 integrated corruption cases passed.
WARP + NVIDIA each: 64 frames, 524288 exact depth pixels, 4 states, 7 queue
switches, zero validation warnings/errors, pins retired. v1.25.14 archive and
review verified unchanged. New DLL SHA 0a183267cda88885baee413403b31dfb089087a62686b61b341ad603acc48e8e.

Installed slot remains v1.25.14 SHA
48567ef9214b302e94f5936507a3a7bd79faeec306e80ce6958eea8d93368e69;
sidecar still points to SEALED local/main-menu-variant-20260930. Do not launch
into that root. No collector active. New offline evidence/source freeze is
local/renderer-boundary-20260930; no deployment script has been run.

Output migration is NOT implemented. Existing effect adapters/single active
recording and executed-draw rejection remain. Native queue prepares all targets
before callbacks; CPU order differs from GPU order. Need dimension domains and
common output ownership established before worker recording, with correct
color vs auxiliary coordinates. Do not remove guards or add another named
effect adapter. See docs/research/shared-renderer-boundary.md for audit and
remaining work. Last runtime outcome remains v1.25.14 menu depth rejection,
rain-reported read-span rejection and loading output-recording-order skips.

# 2026-09-30: v1.25.14 wider run closed; menu/rain/loading issues captured

PID35100/134352038274769723 closed normally, normal scene. Evidence SEALED
local/main-menu-variant-20260930. Keep DLL1.25.14 SHA48567ef9214b302e94f5936507a3a7bd79faeec306e80ce6958eea8d93368e69 installed.
No game/collector/feature active. Full review accepted telemetry/cleanup, NOT
visual coverage:9,200DLSS,12,093unfiltered,0faults,10,405GPUcomplete/released,
22,500jitterrestored,38code spans/17hook heads independently restored.

Main menu used23086; new29842 shader admission NOT exercised. Zero menu
evaluations due following depth state12 instead of8. Loading skips: final draw
CPU-recorded before temporal hook on another list, but submitted later in
same queue batch. Rain-reported area: depth reader begins BEFORE temporal,
strict writer<consumer<next-reader gate rejects though temporal is inside read
span. Two stable captures confirm ordering and256->8node.1,867extra skipped
attempts/0newframes across rain checkpoints. Moving away recovered. Rain itself
not proven causal. Intentional stop explained to user; normal close confirmed.

Next depth admission supports demonstrated combined-read state and consumer
inside begun read span while retaining full interval/state/identity proof.
Separate output timing fix still needed. See main-menu-rain-depth-result.md.
New evidence root required before relaunch; preserve exact DLL/sidecar. Wider
cutscene/monster coverage pending. History distance-only diagnostic unchanged.

# 2026-09-30: v1.25.14 menu shader admission prepared; next launch MAIN MENU

Previous v1.25.13 PID47580 capture sealed local/main-menu-shader-20260930,
archive SHAb429a35cd3450afb93fd22681f3c217f9a5311581e400ab78ebfb49f2be4764e.
Failure was exact shader technique/size gate, not a depth or SDK failure.
Menu29842 uses same six SRVs/one UAV as gameplay23086; history filter differs.

v1.25.14 permits ONLY the two captured exact shaders in scaled runtime,
separate byte-verification caches across transitions, new per-variant counters.
Unscaled policy unchanged; all depth/state/lifetime/output guards retained.
44native+51Python tests and11admission-corruption cases passed. Build clean.
Evidence local/main-menu-variant-20260930. Frozen sources/build, prior DLL/
sidecar preserved on deployment. No runtime verification yet; game absent.

Next same MO2 test entry and bare offline profile, MAIN MENU ready. Verify
startup, arm frozen tool, start collector; verify actual DLSS evaluations and
menu variant count before asking user to load save. Then check both variants
and continue wider cutscene/monster testing if healthy. Keep AA/DLSS Off,
DX12windowed2560x1440,unchanged size. Keep latest DLL installed. No collector
active now. History distance-only diagnostic limitation still applies.

# 2026-09-30: exact main-menu shader variant captured; game closed

v1.25.13 PID47580/134352030501838281 fault29, zero Evaluates. Captured menu
technique29842/15284bytes, SHAe14459733cbbccc47fdaed77ce818062968cf96303f12e28bde7d86a50d6b47c.
Same texture roles/declarations and packed-motion loads as gameplay23086;
native history filter differs. 1x1 named depth is also normal in gameplay.
Full cleanup independently verified38spans/17heads; user normal scene/exit.
local/main-menu-shader-20260930 SEALED. DLL1.25.13 kept, settings unchanged.
Next prepare exact menu variant admission and independent per-variant cache;
keep all depth/state/lifetime/route guards. Main-menu-to-save test pending.
No active game or collector. See main-menu-resolve-rejection.md.

# 2026-09-30: v1.25.13 main-menu rejection recorder prepared

v1.25.12 PID44148/134352021939485649 stopped at main-menu shader guard fault29
before Evaluate. Game closed normally, cleanup verified; wider test did NOT run.
local/scaled-wide-test-20260930 sealed, archive SHA
9754289050575bf22dffbf454602a6f90ecb368a8a4a039d157816598b8c6134.
Shape/copy/hash subcheck unknown; post-stop packet reused, no shader recovered.

v1.25.13 records the first rejected frame/epoch/packet/generation, owners,
recipe/bindings and bounded shader bytes. Reuses exact tested bytes when read;
otherwise bounded copy at rejection. Fixed record under280KB. No new hooks,
same shader admission/fault behavior/history policy. File written after stop
with CREATE_NEW: inputs-PID-CREATION-rejected-taa.dxbc; JSON size/hash/save.
44native+51Python tests passed, synthetic healthy review checks new channel,
no compiler warnings. See main-menu-resolve-rejection.md.

Evidence local/main-menu-shader-20260930; preserve exact prior slot/sidecar on
installation. Game absent, unarmed. Same MO2 entry/bare profile, nativeAA/DLSS/
MotionBlurOff,DX12windowed2560x1440,Performance1280x720. Next launch MAIN MENU,
verify startup/arm; stop still expected. Capture and diagnose exact mismatch,
check native cleanup then normal exit. Wider session pending. Keep latest DLL.

# 2026-09-30: main-menu arming hit shader guard; wider test not started

v1.25.12 PID44148/134352021939485649 closed normally, normal scene. Startup
passed; arming accepted, but fault29 before any DLSS evaluation. One native
resolve candidate rejected by combined shape/bytecode guard. Exact subcheck
not logged. SDK Create succeeded, Evaluate never attempted. No private GPU
signals, both jitter changes restored, full scale and all hooks restored;
live verification matched38fullcode spans/17hookheads. Standard healthy-run
review rejects, independent failure cleanup passed. Post-stop packet reused;
no exact shader recovered. See main-menu-resolve-rejection.md.

local/scaled-wide-test-20260930 now SEALED. Same1.25.12DLL retained,settings
unchanged. Broader gameplay/cutscenes did not run. User approved wider testing.
Next prepare bounded first-rejection metadata and shader capture without
weakening guard; exact failing subcheck needed. New evidence root required
before next launch; preserve slot/sidecar. Current game absent. No active collector.

# 2026-09-30: wider v1.25.12 session prepared; awaiting main-menu launch

User approved a broader playtest starting at the main menu, then cutscenes,
different monsters/effects and transitions. Same unchanged diagnostic DLL
SHA 91b777dedbf4ad4f502ac5c090d3cbcea023ffd29f33e0fb8896505970a3ebf7.
No new build or renderer changes. Prior panning run is sealed, archive SHA
ed014ee0d1725cbe18f8364c934ab84f64f6be3a46e2db2eeec99b17631054fa.
Fresh root local/scaled-wide-test-20260930; DLL and exact sidecar preserved.
Only sidecar redirected. Same bare profile, AA/DLSS/MotionBlur Off, DX12,
2560x1440 windowed, Performance1280x720. Game absent, NOT armed yet.

Next: user launches same MO2 entry and stays at MAIN MENU. Verify startup
and current process generation, arm with frozen arm_continuous_dlaa.py,
start collect_continuous_session.py --minutes 60 immediately afterward.
Collector is NOT running yet. Check active/evaluating at main menu, then
loaded save, then allow natural varied play with Steam whole-session recording.
Use checkpoint.py PID LABEL [PREVIOUS_LABEL] in new root for saved summaries.
Keep settings/window size fixed. After user finishes or 60-minute stop, verify
full review, GPU completion, jitter/full-scale restoration and 38 code spans/
17 hook heads using read_stop_native.py PID before asking them to exit.
Latest DLL stays installed; no automatic restore. Timestamp issues for replay.

Known: this diagnostic suppresses ONLY distance-only history resets, including
pure translation cuts. Combined reasons preserved. Clean earlier panning;
genuine cut behavior needs observation. Consumer-depth reason5 skips remain.
Main-menu activation and broad coverage are not yet live-verified. Evidence
plan and undo instructions in local/scaled-wide-test-20260930/run-plan.md.

# 2026-09-30: v1.25.12 panning comparison passed; game closed, DLL retained

PID45600/134352014002467634 closed normally; user reported clean slow/fast
panning and normal scene after intentional stop. Final3125DLSS frames,
3actualresets vs780controlresets,777distance-onlyresets suppressed. Atpan
checkpoint2763frames,535suppressed,2actualresets. Baseline1154frames,1reset.
Zero runtime/SDK faults. Combined frame/epoch/interval/etc resets preserved.
22skips:1startup pipelinewarmup+21consumer-depth reason5 rejections; separate
schedule/interval issue unresolved (12atpancheckpoint,9later), no visible
defect reported. All3147outputplans built,0unqualified. No objective pixels.

Cleanuppassed:3126GPU signalscomplete,3148jitter changesrestored,fullscale
restored,allhooksdisabled;3125route recordsreleased,zero live,9375effectdraws.
Live read-only check verified38fullcode spans+17hookheads. Fullreviewaccepted.
Collector22events/112.672sec,7status-read retries,0logfailures. See
scaled-pan-history-comparison.md. Evidence local/scaled-pan-history-20260930
SEALED by archive-final.json. KeepinstalledSHA
91b777dedbf4ad4f502ac5c090d3cbcea023ffd29f33e0fb8896505970a3ebf7.
Same bareprofile,AA/DLSS/MotionBlurOff,DX12windowed2560x1440,Performance1280x720.

Interpretation: cleanpanning while retaining777otherwise-reset histories
supports displacement heuristic as contributor; not identical replay and not
proof allskips fixed. This remains diagnostic: puretranslation teleports also
retainhistory. Nextdesign a justified camera-cut policy preserving normal
movement and validate actualcuts. Do not ship ablation as final or relaxdepth
guard. User has not approved/started another test yet. No nextbuild installed.
Before nextlaunch redirectsidecar to NEWroot and preserve currentDLL/sidecar.
Currentgameclosed,unarmednextlaunch;noautomaticrestore.

# 2026-09-30: v1.25.12 panning history comparison prepared

User approved focused panning work following the clean v1.25.11 sauna test.
Previous game closed normally. Prior evidence local/scaled-haze-queue-completion-20260930
is sealed, archive SHA 2e3777d5f2cc26172db5fd2ef4d09856c542f915589e6f43aece7c7b10880190.
3843 DLSS frames (2203 haze), zero faults/unqualified plans, only two PSO warmups;
all GPU work, jitter, full scale and native hook bytes restored. 280 displacement
resets remain a separate candidate for panning instability.

v1.25.12 removes ONLY resets whose complete reason mask is camera_displacement
(1024), logs control-vs-actual counts and bounded typed examples. All other and
combined reasons unchanged. Scaled runtime only; DLAA uses previous policy.
Diagnostic comparison, NOT final cut detection: translation-only teleports also
retain history. No native hook/resource/depth/shader/queue changes. Keep test
to stationary baseline and slow/fast panning in same area with menus closed.
43 native tests and 51 Python tests passed; 39 real bounded examples replayed,
16 distance-only resets suppressed, 6 reset examples preserved, 17 already
retained. Synthetic new-version parser checks passed (not live evidence).

Evidence local/scaled-pan-history-20260930; see scaled-pan-history-comparison.md.
Preparation/deployment records exact sources/build/config/backup identities.
Same MO2 entry, barebonesdlss, DX12 windowed2560x1440, Performance1280x720,
native AA/DLSS/MotionBlur Off. Next: load outside sauna, report Save loaded,
verify startup/arm, still baseline then panning. Build unarmed on launch.
Live visual outcome pending. Latest DLL remains installed per user; no auto restore.

# 2026-09-30: v1.25.11 sauna route passed; game closed, DLL retained

PID46780/134351995074220844 is closed. User reported clean inside sauna and
thought walking/panning plus exit/re-entry stayed clean. Final 3843 DLSS frames,
2203 complete haze frames, 1640 ordinary; all 3845 published plans built,
zero faults/rejections. Only 2 skipped frames: ordinary and haze shader warmup.
No further evaluation skips during this tested sequence. 13732 replaced draws.
All 3845 GPU signals complete, all 3846 jitter changes restored, full resolution
restored, all hooks disabled; live check matched 38 full gates and 17 hook heads.
3843 scaled-route records retained/released, zero live. Reviewer accepted.
Live auxiliary GetDesc: R32_FLOAT2560x1440, RGBA8_UNORM volume32cubed, filter1x1.
All 3 bounded haze captures succeeded. Auxiliary GPU descriptors not read back.
See scaled-haze-queue-timing-result.md for result and limitations.

Evidence local/scaled-haze-queue-completion-20260930 is SEALED by archive-final.
Installed DLL SHA194da1b7f5e9ae144603555fbdcc5d2c6266a8a4596aaf90b905547057762cb4.
Keep installed; no rollback. Same bare profile, native AA/DLSS Off, DX12,
2560x1440 windowed. No next build installed. Redirect sidecar to a NEW evidence
root before the next launch; preserve exact current slot and sidecar backup.

Next target: panning history resets. v1.25.11 had281 resets/280displacement;
only startup/initialhaze warmup actually skipped evaluation. Bounded first/latest
examples (12 unique,10displacement-only) span20.29..91.95units/29.63..57.98ms;
not a complete distribution and no visible defect established in this cleanrun.
Prior v1.25.7 isolatedpan reported visible instability plus241displacementresets,
with no new evaluation/output/depth skips. Fixed >20units/frame rule likely
overreacts to ordinary camera motion at these frame intervals; causal visual
comparison still needed. Next design a bounded comparison preserving frame,
resource, projection and other cut checks, without guessing a permanent cutoff.
Separate pending: early CPU downstream recordings and depth interval rejections.
Performance cleanup remains deferred. No history-policy change made yet.

# 2026-09-30: v1.25.11 queue-completion correction prepared

Last run v1.25.10, PID26224/134351976947449200, closed normally with a normal
scene. 2558 DLSS frames, zero runtime faults; all 12107 GPU signals complete,
all 12192 jitter changes restored, full scale restored, 38 native code gates
and 17 hook heads verified. Evidence local/scaled-haze-route-20260930 SEALED;
archive SHA 9906ec5a828bfd5a08b6eefe0e326b1eb541d51a2dcca1b687be1f31c1a922c4.
Sauna did NOT work: 9548 unbuilt plans, rejection 15, zero haze completions.
Graph links accepted, auxiliary texture qualifier not reached. Native loop
executes motion order100 before haze order20000; motion return scans too early.
User saw recovery outside sauna. See scaled-haze-queue-timing-result.md.

v1.25.11 keeps a motion-return receipt, builds after the owned outer native
postqueue returns. Incoming arguments saved before forward overwrites Frame.
Same frame/epoch/view/context/TAA/thread and unique motion return required.
No new hook, shader change or relaxed graph guard. Nested/exception/stop/stale
callbacks rejected. Reuses existing gated 0x24f1fc0 hook and normal lifetime.
47 native tests, 23 Python tests, ordinary 96-frame/288-effect GPU run and
mixed aliased 96-frame/336-effect run (48 haze) passed. Partial queue regression
passes ordinary/haze/aliased paths. Live haze textures/PSO still unverified.

New evidence local/scaled-haze-queue-completion-20260930; preparation/deployment
records exact DLL/config/source hashes and previous slot backup. Keep latest
DLL installed; no automatic restore. Same MO2 entry, barebonesdlss, DX12,
2560x1440 windowed, native AA/DLSS/MotionBlur Off, Performance1280x720.
Next: load save outside sauna, verify startup/arm and baseline, enter sauna,
read route and auxiliary descriptors, then exit/re-enter if healthy. Panning
history resets and other depth admission skips are separate pending issues.

# 2026-09-30: v1.25.10 optional haze route prepared for runtime

The first live-enabled haze adapter now connects motion filtering to color grading
through the optional haze draw. Three/four-effect ordering, resource/descriptor-heap
retention, exact shaders/vertices, actual auxiliary GetDesc qualification and
binding stability are implemented. No new hooks. In-game verification is pending.

Offline RTX 5090 tests passed: ordinary 96 frames/288 effects; mixed ordinary/haze
96 frames/336 effects; native haze VS/adapter 50 draws and 860160 float comparisons;
malformed graph/order/binding/state checks; complete GPU/recording retirement;
native and Python reviewer tests. See docs/research/scaled-haze-output-route.md.

New evidence root: local/scaled-haze-route-20260930. Deploy v1.25.10 to the existing
MO2 dlssTEST slot with exact v1.25.9 backup and redirect output there before launch.
The prior local/scaled-haze-bindings-20260930 remains SEALED. Keep the new DLL
installed after the test as requested. AA/DLSS Off, DX12, windowed 2560x1440,
barebonesdlss; launch the same test entry and load outside the sauna before arming.

Panning history resets/depth admission are separate remaining issues. Auxiliary
GPU descriptors/states are reused through the unchanged native executor, not
independently read back. The next game run must confirm this live path.

---

# 2026-09-30: v1.25.9 captured all three native haze recipes

PID48080/134351952741962897 closed normally; latestDLL retained, settings unchanged.
Evidence local/scaled-haze-bindings-20260930; see scaled-haze-enqueue-result.md.
Three attempts/three accepted same-frame CPU recipes, zero capture rejections:
frames15795/15855/15915. Exact1draw append, shader/layout/geometry and t0..t3
named-owner/packed-binding agreement. Captured native triangle3verts/first12,
rectangle1280x720 in2560x1440 allocations, UV0..0.5 across viewport; zerooffset.
First sample correlates missing graph link motion_output->haze->grade_input.
VS712 SHA c7c73b6ac661514c34c53681e217dc3fe7014e21a99a41adb0fcb7befc1f5de6;
PS14596 SHA 4f890ffa89ea59a218db7e191d8aab48e9d5fe4be200c50a606b22594bb57f69.
VS0/PS9bindings, t0color2560x1440,t1depth2560x1440,t2volume32x32,t3default1x1.
Pipeline/bindings/geometry equal across samples; strength ramp .0037002->.075.
CPU metadata/handles only: GPUbindings/resourceDesc/states still require proof.

1897DLSSframes,0faults,1670skips=1646hazeplans+22depthreason5+1warmup+1stop.
Both suspensions recovered.3545recon/depth+1670unfiltered GPUrecordings retired
and released;3568jitter restored;fullscale restored;38gates/17nativeheads match.
Use FINAL JSONL status; collector latest-status-at-end predates shutdown.
120camera_displacement resets preserved as separate panning issue.

No new build prepared this run. Next integrate optional haze fourth effect
between motion/grade using observed recipe and offline-tested adapter; preserve
reduced depth/noise/filter coordinates and constants, scale final color only.
Validate actual native VS/triangle offline, execution-time resources/state and
four-effect lifetime while retaining three-effect path. Do not loosen graph or
execution guards. Sauna still suspends until integration. KeepDLL installed.
After archive seal, redirect sidecar into NEW output root before next launch.

# 2026-09-30: v1.25.8 capture diagnosed; v1.25.9 binding-reader correction

Game38772 closed normally. Run archived under local/scaled-haze-adapter-20260930:
720 files; archive SHA00df91f1a23fde26ea75c0213958ba9147531ed29b661d530556e79a3fb0ed0f.
2538DLSS frames, zero runtime faults; 6878skips=6877haze+1warmup. Both suspensions
recovered.9416GPU recordings+6878unfiltered retired/released,9417jitter restored,
fullscale returned;38native code gates and17hook heads independently restored.
No complete haze recipe: all16 attempts phase5/reason8 (bindings).

Retained CPU snapshot pinpointed recorder bug:12-byte per-stage blocks contain
ONE u16 count/start pair plus8metadata bytes, not3ranges. Native259f330 loop
and259e670 dispatcher confirm. ObservedVS halfwords[0,0,0,65280,65535,1],
PS[9,0,4,4,255,0]. All4SRV CPU handles nonzero; noGPU/frame alignment proof.

v1.25.9: corrected parser+opaque metadata repeat checks; schema2 stage_bindings.
Existing hooks/admission/rendering unchanged. Offline haze adapter still NOT
live. Native3capture+10rejections; real-metadata regression;96frame/288effect
GPU regression;25Python tests+actual2538frame replay+syntheticnewversion pass.
See docs/research/scaled-haze-binding-capture-result.md.

New evidence root local/scaled-haze-bindings-20260930. See deployment.json and
prelaunch-verification.json for actual installed identity. Next same MO2 test
entry/profile; nativeAA/DLSS Off; windowed2560x1440; load outside sauna, arm,
baseline, enter/stay menusclosed for3haze recipes, leave/recovery, intentional
stop/cleanup. Keep latestDLL installed. No game settings edits. Panning/history,
earlyCPUrecording and optimization remain separate work. Sauna not fixed yet.

# 2026-09-30: v1.25.8 haze adapter tested; enqueue recorder prepared

Evidence: local/scaled-haze-adapter-20260930. See scaled-haze-adapter.md.
The hash-gated haze PS adapter passed WARP and NVIDIA tests: each40 draws,
532,480 float comparisons and13 rejected inputs. All five techniques tested
with and without distortion. Initial fixture edge failure was corrected by
providing the native t3 bilinear border; adapter unchanged. Native geometry
is STILL unverified. Adapter not called by the live route; sauna still suspends.

New bounded CPU recipe recorder uses EXISTING fullscreen2297e20 hook narrowed
to haze caller24c7ff9. Before/after native forwarding: same primary frame/view,
one exact queue append, named t0..t3 vs packed shader owner table, VS/PS/layout,
vertices/pipeline fields and repeat checks. Three successful samples,16 attempts,
60-frame spacing. NO GPU binding/state proof; NO extra hooks or admission change.
Synthetic native capture passed3 recipes+10 rejects+foreign/capacity checks.
96-frame existing GPU route regression and108 selected native tests passed.
New Python reviewer4 tests/14 corruptions, prior reviewer13 tests, live2271-frame
replay and labeled syntheticv1.25.8 version integration passed.38 native code gates.

Deploy with evidence/deploy.ps1, preserving exact v1.25.7 slot/sidecar. Keep
latest installed. Next launch SAME MO2 test entry, native AA/DLSS Off, unchanged
window. Load outside sauna, wait for arm+baseline, enter and stay with menus
closed for three recipes; recover outside, intentional stop and cleanup.
Do not claim sauna fixed from shader fixture; actual recipe/state/lifetime still
needs integration. Panning history/CPU ordering/performance remain separate.

# 2026-09-30: sauna haze shader extracted; run closed and DLL retained

Game 46396 closed normally; v1.25.7 remains installed; settings unchanged.
Evidence: local/scaled-haze-20260930. See scaled-haze-shader-result.md.
2,271 DLSS frames, zero faults; 29,769 skips = 29,768 unbuilt haze plans +
1 startup warmup. Both suspensions recovered; no depth/order rejections.
All 32,040 reconstruction/depth and 29,769 unfiltered recordings retired and
released; 32,041 jitter changes restored; full scale and 36 code spans /
17 native hook heads verified restored. Collector ended healthy, intentionally.

Strict external full-recipe capture FAILED: primary-view/queue build window too
short. Separate retained-tail capture extracted equal double-read VS/PS bytes:
PS 14596 bytes, SHA 4f890ffa89ea59a218db7e191d8aab48e9d5fe4be200c50a606b22594bb57f69;
VS 712 bytes, SHA c7c73b6ac661514c34c53681e217dc3fe7014e21a99a41adb0fcb7befc1f5de6.
CBHazeFilter reflection; t0 color/t1 depth/t2 volume/t3 filter. These are
retained shader identities, NOT current-frame geometry/bindings/GPU proof.
PS SHEX 2604 words; six temps at46; five identical color sample instructions
at487,942,1585,2135,2580. Candidate adaptation: new temp scales only final t0
coordinates, preserves depth/noise/reconstruction. Needs geometry/edge/GPU proof.

Next prepare offline haze shader adaptation/fixtures plus bounded same-frame
recipe capture through EXISTING fullscreen hook2297e20, caller24c7ff9. Optional
haze stage goes between motion and grade. Do not simply accept changed graph
links or weaken execution guards. CPU recording order and panning history
remain separate issues; no correction/new build prepared in this run.
Keep DLL installed. Redirect its output sidecar to a NEW evidence directory
before another launch, preserving this sealed archive. Performance deferred.

# 2026-09-30: read-only haze recipe capture prepared; DLL unchanged

Ready for another launch through the current MO2 test entry. Keep AA/DLSS Off
and window unchanged. Load outside sauna, wait for arming, then enter sauna.
Installed DLL remains v1.25.7 (dd9566f6887a9ebc9da94fb18c4a0be1b73f0f8ec6ba6fe6eda97e1edeb04ff8).
Its output sidecar now points to local/scaled-haze-20260930; prior sidecar bytes
are backed up there. SDK location copied; settings unchanged; sealed prior
archive verified. No new hook or rendering correction was installed.

tools/read_scaled_haze.py is ready: query/VM_READ only, exact process/hash/code
gates, bounded VS/PS/geometry/owner/range capture with matching double snapshots.
6 offline tests passed; source frozen under evidence/source. Live reader still
unverified. See scaled-haze-live-reader.md. External CPU snapshots do not prove
GPU descriptors/states and do not grant admission. Do not weaken route guards.

After arming, use new input log with --output-dir under this evidence directory.
Capture while inside sauna; inspect pixel shader coordinates and named inputs,
then recovery/intentional stop/cleanup. Follow current v1.25.7 collection/review
tools. Panning-history correction remains separate from haze/output ordering.

# 2026-09-30: v1.25.7 isolated panning, haze, and CPU ordering

Game23652 closed normally; latest DLL retained; settings unchanged. Evidence:
local/continuous-dlss-20260929/v1.25.7. See scaled-output-correlations-result.md.
4,164 DLSS frames; zero faults; full capture/cleanup review passed. All20,610
reconstruction/depth and16,468 unfiltered GPU recordings completed/released;
20,636 jitter changes restored; full scale and36 gates/17 native heads restored.

Isolated pan:1,355 additional evaluations,NO additional skipped evaluations,
241 extra camera-displacement history resets. Fixed20-unit/frame rule is a
candidate visible cause; controlled comparison still needed. Do not attribute
this pan checkpoint to output-order rejections.

Sauna:16,438 unbuilt decisions; retained graphs prove uHazeFilter(type35372b0,
stage15/order20000) connects motion output to a different grade input. Read
mask7 excludes failed reads as the cause in those samples. Haze expanded-output
bindings/depth states still need mapping; do not just accept the changed link.

Recovery:7 CPU-order rejections;5 retained receipts. All rejected stages were
recorded early on list2, submitted in the SAME queue/batch after temporal list1.
CPU recording order is not GPU consumption order. Plan replacements before
parallel recording; do not simply drop the already-recorded guard.22 separate
depth interval rejects had no failed indices retained in this run's snapshots.
All6 suspension periods recovered; user confirmed normal scene and normal exit.

No code fix installed during the run. Next: optional haze output support,
pre-recording replacement routing, and an independent reset-policy comparison.
Preserve diagnostics; performance optimization deferred. No new build ready.

# v1.25.7 output correlation diagnostic prepared

Adds bounded callback/list-generation/submission receipts for executed-mask
rejections and cached filter/connection snapshots for rejected output plans.
No admission/GPU behavior change; panning/sauna corrections are not yet claimed.
48 native tests,112 Python tests and8 GPU fixture runs passed. The prior6,266-frame
v1.25.6 capture replays. Current NVIDIA adapter LUID is [0,94196].
Next live sequence: still baseline, panning without menus, then sauna entry/exit.
Keep latest installed; native AA/DLSS Off; unchanged window. Performance deferred.
See scaled-output-correlations.md; evidence local/continuous-dlss-20260929/v1.25.7.

# 2026-09-29: v1.25.6 captured output-order and sauna rejections

Game21952 closed normally; latest DLL retained; settings unchanged. Evidence:
local/continuous-dlss-20260929/v1.25.6. See scaled-output-readiness-result.md.
6,266 successful DLSS frames; zero runtime faults; capture/cleanup review passed.
All6,796 GPU signals completed,7,417 jitter changes restored, full scale and
36 code gates/17 native hook heads restored. No implementation change this run.

1,145 skips = 1 startup + 25 downstream CPU callbacks already completed +
503 unbuilt output plans + 615 depth interval rejections + 1 intentional-stop
rejection. Sampled failed plans report reason6/detail4 (grade-input/motion-output
connection or adjacent reads). Failed depth samples put next native use BEFORE
temporal consumer. An intervening sauna effect is a hypothesis, not yet mapped.
All15 pre-stop suspension periods recovered. These are distinct paths.

User correction: actual brief frame skip is triggered by PANNING; do not call
it menu-only. Sauna/similar spots also interrupted DLSS; leaving recovered it.
Next investigate downstream CPU recording/submission assumptions and separately
map intermediate depth/post-processing use. Do not weaken validation blindly.
Keep research tools; performance optimization deferred. No new build is ready.

# 2026-09-29: v1.25.6 output-readiness diagnostic prepared

Consumer-depth correction remains unchanged. The new bounded readiness channel
classifies the remaining brief menu-opening rejections without changing admission.
47 native and104 Python tests passed; WARP/NVIDIA route fixtures and six rejection
fixtures passed. The sealed v1.25.5 run replays successfully. Live cause remains
unproven; next test repeats menu openings at different camera angles.
Keep latest installed; native AA/DLSS Off, window size unchanged. Performance
cleanup remains deferred by user preference; diagnostic overhead is not quantified.
See `scaled-output-readiness.md`; evidence local/continuous-dlss-20260929/v1.25.6.

# 2026-09-29: v1.25.5 tested; depth fixed in capture, brief menu skips remain

Game16700 closed normally; latest DLL retained; all settings unchanged.
4,176 DLSS frames, zero faults; all4,181 consumer-depth proofs/copies passed
with zero missing depth leases. All GPU work released, jitter restored, full
scale returned,36 code gates and17 native hook heads restored. Full review passed.

User still observed brief skips during menu opening, possibly at a different
camera angle. Four non-startup frames failed stream-route readiness; all resumed.
Initial quiet/open-menu reports were clean but do not establish a complete fix.
Current boolean readiness combines several gates; the exact rejecting one is
not logged. Next add bounded readiness-reason receipts, preserving admission.
See `scaled-consumer-depth-result.md`; evidence local/continuous-dlss-20260929/v1.25.5.

# 2026-09-29: v1.25.5 consumer-owned depth copy prepared; live test pending

The scaled private depth copy now runs at reconstruction, removing the old
post-writer CPU-order dependency. Fresh native schedule/owner/span/node proof
derives RT or hoisted PS state; balanced transitions and Reset+fence retirement
protect the copy. Old scaled writer injection is disabled. This relies on the
pinned native execution contract; ambiguous evidence suspends reconstruction.
See `scaled-consumer-depth-copy.md` for the contract, limits and reason counters.

Both DLLs build without warnings. 46 native and 94 Python tests pass. WARP and
NVIDIA each verify 64 copies/524,288 pixels, consumer-first CPU recording,
both depth states, queue switches and complete release without GPU warnings.
Menu correction is not yet live-verified. Next: same save/settings, quiet
baseline, reproducing menu open, close/recovery, intentional stop and native
cleanup check. Keep latest installed; native AA/DLSS Off, window unchanged.
Evidence/deployment: `local/continuous-dlss-20260929/v1.25.5`.

# 2026-09-29: v1.25.4 menu schedule captured; game closed, DLL kept

Run34780 captured the menu skips and normal recovery. 2,341 DLSS frames, zero
faults; 1,202 missing-depth dispatches plus one readiness skip; all39 suspension
periods resumed. All2,342 GPU signals completed, all2,341 route recordings
released, all3,545 jitter changes restored. Full scale and35 native code gates/
17 hook heads restored. User confirmed normal scene/exit; settings unchanged.

Eleven unique retained schedule observations (one baseline, ten misses) all show
selected outer depth writer before reconstruction, its nested span at native
0x100 (D3D render-target), and next depth transition0x100->0x8 at consumer+3.
The tracker final0x8 describes that later use. See
`scaled-menu-depth-schedule-result.md` and local/v1.25.4/schedule-analysis-34780.json
under `local/continuous-dlss-20260929/`. This is observed native schedule evidence;
GPU admission/copy correction is not implemented. Next: qualify this interval
and record the private depth copy in reconstruction's own recording, with
balanced transitions and existing resource retirement. Avoid the CPU-order
assumption and old post-writer duplicate copy. Keep latest installed.

# 2026-09-29: v1.25.4 depth-schedule diagnostic ready for menu reproduction

Preparing the consumer-point native depth-schedule capture described in
`scaled-menu-depth-schedule.md`. Builds and 45 native / 86 Python tests passed.
This is observation only: the v1.25.3 CPU recording-order dependency is still
unfixed. The next run compares a normal baseline with the reproducing open menu,
then verifies recovery/cleanup. Do not treat a planner's final state as the state
at reconstruction. No stale-depth reuse, live-bit relaxation or worker wait.
Native AA/DLSS remain Off; Windowed 2560x1440. Keep the latest diagnostic installed.
Evidence and deployment receipts: `local/continuous-dlss-20260929/v1.25.4`.

# Current: v1.25.3 menu failure isolated; game closed (2026-09-29)

The controlled menu test is complete and verified. User saw skips while the
menu was open and normal recovery after closing it. Found a CPU recording
order/lifetime dependency: selected depth writers pass draw checks but their
same-frame temporal request is already marked consumed (mask 510/511). The
writers and temporal consumption can use different recording lists. Do not
merely relax live matching or reuse old depth; GPU ordering/state still needs
proof. See `scaled-menu-depth-result.md` for exact evidence and correction path.

Final: 19,779 DLSS frames, 8,375 missing-depth suspensions at frame level plus
one readiness skip, 55 suspension periods all resumed, zero runtime faults.
All 19,780 GPU signals completed, all 28,156 jitter changes restored, all route
recordings released, full render scale restored. Full review accepted; all
35 native gates and 17 native hook heads verified restored. User confirmed
normal exit. DLL v1.25.3 remains installed, settings unchanged, no game running.
Evidence: `local/continuous-dlss-20260929/v1.25.3`. Collector session 92239 ended.
Next: make depth copying follow proven scheduled GPU order instead of CPU
callback arrival, with correct recording ownership. No correction deployed.

---

# Current: v1.25.3 menu-depth diagnostic prepared (2026-09-29)

User requested fixing the visible menu-opening skips before moving on.
The v1.25.2 run is sealed: 6,901 DLSS frames, 230 missing depth-copy leases,
two later readiness skips, zero runtime faults, clean verified native stop.
The cause earlier in the depth path is still unconfirmed. Do not weaken
checks or reuse stale depth. The pending video clip is not a blocker.

v1.25.3 adds bounded per-frame stage/reason records for depth production,
publication, temporal selection, consumer/draw admission and private copying.
It adds no hooks or GPU commands and preserves the v1.25.2 fixes. Both DLLs
build; 44 native and 79 Python checks passed. See
`scaled-menu-depth-admission.md` for the schema and focused menu test plan.
Evidence/deployment: `local/continuous-dlss-20260929/v1.25.3`.
Game is closed; MO2 remains open. Verify deployment/prelaunch receipts before
arming. Settings remain native AA/DLSS Off, DX12, 2560x1440 Windowed. Keep the
latest DLL installed after the run, per the user's standing preference.
Live verification of v1.25.3 is pending. No claim that menu skips are fixed.

---

# 2026-09-29: first continuous live DLSS output verified; menu skips remain

- v1.25.2 PID 21704, creation 134351205854423564, closed normally.
- 6,901 successful Performance DLSS evaluations, 6,901 full-output copies,
  20,703 downstream effects; zero runtime/SDK faults. 1280x720 -> 2560x1440.
- User initially reported clean still/moving scene and normal HUD. Then
  identified opening menus as a reproducible brief skip trigger; clip pending.
- 232 unfiltered frames: 230 missing current-frame depth-copy leases and
  2 later stream-readiness skips. All 22 suspension periods recovered.
  Native depth identity and all downstream plans passed. Exact earlier
  depth-admission rejection is NOT yet isolated.
- Stop was intentional and healthy: all 6,903 primary GPU signals complete,
  jitter 7,134/7,134 restored, full scale restored, all stream pins released.
  Read-only checks proved 35 native code gates and 17 hook heads restored.
- Current installed DLL remains 3b6d3174215ee62343af6e9da4a5033c10fbfb18743bc145227aa313f2d36627.
  Keep it installed. No native binary changes after this run.
- Full reviewer corrected reason-7 aliasing for scaled versions only;
  15 depth + 57 broader Python tests passed. Review accepts telemetry,
  not pixel quality or all transitions. Postrun reviewer sources frozen.
- Archive: local/continuous-dlss-20260929/v1.25.2/archive-final.json,
  581 files, SHA-256 7e3487f82edad0c405562b76c2892aa22b2714534f9ed0c56a76a860e4101764.
- Next: instrument the missing depth path at producer/match/draw/copy
  admission for controlled menu open/close. Do not assume stale depth safe.
  Area/focus checks were paused and are unverified for continuous DLSS.
- Details: docs/research/first-continuous-dlss-result.md.

# 2026-09-29: v1.25.2 continuous DLSS correction, awaiting live validation

- v1.25.1 PID 48048 is closed normally; user saw a normal scene. No DLSS
  frame was admitted. All 9,681 plans failed final-source proof.
- Confirmed recorder defect: renderer+0x93 is one byte, but the saved
  snapshot read four bytes (257 instead of 1). v1.25.2 fixes the width.
- Shutdown fault 25 left eight activation hooks installed despite false
  disabled telemetry. Actual native bytes proved this. GPU signals 9,681
  completed, jitter 9,682 restored, full scale restored. Game then exited.
- v1.25.2 permits stopped forwarding callbacks to drain after hook removal,
  retains the owned-camera guard, and marks only successfully queued/applied
  hooks disabled. Both old failures reproduced offline; corrected native
  tests and WARP/NVIDIA output-chain checks pass.
- v1.25.1 immutable archive: 555 files, manifest SHA-256
  5507a9cf29cb3895cee6fbec4dcd24ee178801600fedb651b3129527e0c4729b.
- Next slot/evidence: local/continuous-dlss-20260929/v1.25.2. See its
  preparation/deployment receipts for installed build identity. Keep the
  latest DLL installed; do not automatically restore.
- Next launch: same barebonesdlss MO2 test entry, native AA/DLSS Off,
  windowed 2560x1440. Load a save, explicitly arm, verify successful SDK
  and downstream frames before asking for visual/transition tests. Verify
  actual native hook restoration after stopping.
- Details: docs/research/continuous-scaled-final-source.md.

## 2026-09-29: v1.25.1 installed; final-source capture timing corrected

The game is closed. Next user action: launch the usual MO2 test entry and load
save; then arm with tools/arm_continuous_dlaa.py, exact live generation/log and
DLL SHA `abc7e1b58ade193c1a23b1a671f39afc1f6e87baa565aa42c62c5f89334cf86f`.
Evidence/output sidecar: local/continuous-dlss-20260929/v1.25.1. Native AA/DLSS
and MotionBlur remain Off, DX12 windowed 2560x1440. Keep this DLL installed.

Previous v1.25.0 PID37120 creation134351180096861258 rejected every output plan
(reason6) and issued ZERO SDK evaluations. It stopped cleanly:2801completed
signals,2802jitter restorations, full scale restored and all hooks disabled.
User confirmed closed; settings unchanged. Its 521-file archive is SEALED;
manifest SHA 7c8104385f5f7313d0a3375ea89cb85f72736297f33d39fb92cfbec0de71eb9e.

Confirmed defect: continuous code reread mutable tBaseMap after motion setup.
Both final and motion preparation bind the same parameter; accepted bounded
recorder captured the final source earlier. v1.25.1 now captures at the same
final quad boundary (RVA2297e20/caller23c7b08), matches exact frame/owners and
queued final packet later, and keeps GPU retirement unchanged. New telemetry
final_captured/final_rejected and plan_detail split chain rejection causes.
The compound v1.25.0 log did not capture its exact failed subexpression;
continuous success still requires a live test. No claim of live fix yet.

/W4/WX build passed. 8GPU fixture runs passed:96frames on WARP and NVIDIA,
13malformed graph cases including stale/missing captured final identity,
6route rejections.57Python tests plus both versions' synthetic reviews passed.
35native code gates verified. New runtime has48gameplayhooks. SDK/history
unchanged; prior96-evaluation real SDK result applies. Prelaunch manifest:
8bb5c880c46bf94d2a07388122eb6d996b74f5c2120215725c9da0691d95190e (531files).
Research: docs/research/continuous-scaled-final-source.md. Runtime guide:
docs/runtime/continuous-scaled-output.md.

## 2026-09-29: continuous Performance DLSS installed, awaiting first live run

v1.25.0 is now installed in the existing MO2 dlssTEST slot. Installed SHA-256:
`d674323f4a1a0da8c4a73bba949201dfe6d2e7ad39b9e9b3652a7e48ef608f41`.
The game was closed during deployment. Previous v1.24.5 DLL and exact sidecar
are preserved in `local/continuous-dlss-20260929/v1.25.0`; deployment receipt
and prelaunch manifest verify the installation and unchanged configurations.
Offline results below remain the evidence; no continuous live success yet.

Next: launch the existing test entry, keep native AA/DLSS Off and the
2560x1440 window unchanged, load a save, then arm with
`tools/arm_continuous_dlaa.py` (not the previous bounded scaled helper).
This is fixed Performance mode, 1280x720 to 2560x1440, retained history and
continuous full-output routing. Inspect baseline counters before movement,
then area and focus transitions. Keep the new DLL installed afterward.

## 2026-09-29: continuous Performance DLSS prepared (v1.25.0)

The accepted v1.24.5 live result is now connected to the continuous DLAA
lifecycle: 1280x720 rendering, 2560x1440 output, retained history, full output
copy and three native effects on every admitted frame. Native AA and DLSS
remain Off. Offline checks passed: 152 native cases, 206 Python review tests,
96-frame WARP/NVIDIA output-route fixtures and 96 real NVIDIA SDK evaluations.
No continuous live success is claimed yet. The DLL is prepared under
`local/continuous-dlss-20260929/v1.25.0`; deployment waits for the game to close.
See `continuous-scaled-output.md` and `../runtime/continuous-scaled-output.md`.
The user wants the latest DLL left installed after testing; do not auto-restore.

# Current checkpoint: 2026-09-28 — first live full-output DLSS capture ACCEPTED

Latest user reply: "Scene normal; game closed normally". Game closure verified.
v1.24.5 remains installed as requested, unchanged/unarmed. No next build yet.
Next work: continuous Performance DLSS with retained history, using the
established DLAA lifecycle and the now verified scaled output chain. Do not
ask for another launch until that implementation/test mode is ready.

- DLL mhw_scaled_output_probe1.24.5 SHA
  5ca85334d9ce489c2975c621ab4cebacc57229aeb3cab907bc6366b8fa5e3e9c.
- Slot D:/MO2/MHW/mods/dlssTEST/mhw_dlss_bridge_probe.dll. Output-dir still
  local/scaled-temporal-20260928/v1.24.5. That completed root is now SEALED;
  allocate a fresh evidence directory before another run. Keep exact DLL and
  sidecar backups for any deployment; do not restore automatically.
- Native AA/DLSS/MotionBlur Off, DX12 windowed2560x1440. Profile/mode/settings
  identities unchanged; no game assets changed. User can reply from desktop.
- PID24448 creation134351019970206394. Frames14998,14999,15000; SDK receipt2,
  command357905456 Resetgeneration2426. One1280x720->2560x1440 Performance
  presetK Evaluate, expandedcopy1, fullsizeeffects3, nativeTAA bypass1.
- Full reviewer PASSED: all GPU work/fences/recording retirement, SDK/effect
  resource release, all286 submission refs released, jitter1write/1restore,
  full scale restored, all hooks disabled. Native packed handoff matches all
  3686400 float output pixels exactly. Color/depth/motion conversion checks,
  frame jitter and native draw/binding checks pass. User reports normal scene.
- Saved SDK preview coherent full scene, no obvious cropping/missing regions.
  Diagnostic tone mapping before game's effects; not final swap-chain image.
- Raw log SHA c40d81f8c6594fd40e274c79d0a636f4c761f6dc001f7d17edfcd0d0fcc7c9e3.
  Final review review-24448-markers-fixed.json. Original review-24448.json
  rejected old "depth marker coverage" assumption and is preserved.
- Reviewer-only correction: kind4 markers partition between validated depth
  and validated full-output route; require disjoint exact coverage. No extra
  unowned markers allowed.9 output tests+7 shared tests+priorv1.23.12 accepted
  live regression passed. Corrected sources/hashes in reviewer-correction.
  Installed DLL/source C++ unchanged; prelaunch snapshot remains unchanged.
- Evidence local/scaled-temporal-20260928/v1.24.5 SEALED 566 files;
  archive-final SHA 3c9435461484ccf73c062f9b872e63a96432709d3190bf63ea6ff6217e75fff9.
- Research: docs/research/first-scaled-live-output.md. Existing v1.24.5 copy
  guard fix and fixture details in docs/research/scaled-live-output-route.md.

Scope: one live reset-history DLSS frame and full output-chain GPU completion
verified. Continuous upscale history, visual quality while moving, transitions,
focus changes and performance remain unverified. Do not claim continuous
DLSS, quality-mode switching, Reflex or frame generation works yet.

---

# Current checkpoint: 2026-09-28 — v1.24.5 installed, awaiting game launch

Latest user reply: "Scene normal; game closed normally" after v1.24.4.
Game closure verified. Partial capture archived; late-copy admission correction
built, tested and installed in existing dlssTEST MO2 slot. Next: usual Monster
Hunter: World - DLSS 1.2.110 test entry, load a save, report ready.
Native AA/DLSS/MotionBlur Off; DX12 windowed2560x1440, unchanged window size.
No v1.24.5 game capture yet. Keep the DLL installed per user preference.

- Target mhw_scaled_output_probe version1.24.5, SHA-256:
  5ca85334d9ce489c2975c621ab4cebacc57229aeb3cab907bc6366b8fa5e3e9c
- Output local/scaled-temporal-20260928/v1.24.5, with sdk-runtime-dir.txt.
- Exact backup previous-slot.dll.disabled is v1.24.4, SHA-256:
  5f3a910551271eafb7869eb1423e32d81ce1c30c3bdee273e521b23ddf33cbcc.
- Settings/profile/mode/provider/host identities unchanged;48 native gates
  rechecked;483 source files frozen. Same49 hooks,23 binding hooks;
  observer owns viewport, uses owned-targets-only control; resolution1hook.
- Review matching startup log first; arm with tools/arm_scaled_temporal.py,
  exact input log/PID/creation/DLL hash once save loaded. No native DLSS toggle.
  Review tools/review_scaled_temporal.py after automatic bounded stop.
- New behavior: after expanded copy and3 restored output draws, copy_override
  immediately forwards subsequent copies untouched. This happens under the
  route lock before guard checks on reused textures; later GPU fence/Reset
  still govern retirement. Early duplicate remains a fault. No new GPU work.
- Still receipts0/1 prepare, receipt2 attempts one Performance DLSS
  1280x720->2560x1440, bypasses native TAA once, expands final copy and3 output
  effects on same command list/Reset generation. Full review/pixels pending.

v1.24.4 PID16048 creation134351014225002367: one SDK success, one full copy,
three full output draws. Independent submission review PASSED: selected
frame15197, command357912960, generation3124, all4 markers precede Close3172,
Execute3178, private completion3252, Reset retirement3224. All3 recordings
completed;340 submission references released. Binding/jitter reviews passed.
Later same-resource copy hit routefault4 after3draws. Fault prevented private
effect retirement and SDK image collection; completion cleanup set aggregate
bindings_equal=false even though SDK and draws restored their bindings.
All hooks disabled, full scale restored,1jitter write/restoration; normal
scene and normal exit. No output image verification, no continuous DLSS claim.
Whether the late copy was same or next generation is not logged; both are
reproduced/covered offline. See completion-review-16048.json for exact limits.
Log SHA 4f500d44f3006f13f810f1dad8abb4071a28fe20508ce9075d0abf3335c110b6.
Archive local/scaled-temporal-20260928/v1.24.4 SEALED, manifest SHA 1a0eaf2920fc80b175b957cc1b129ebf5eac80366b7d1102ba05445b67f21b43.

Offline v1.24.5: reproduced fault before fix. Both WARP/NVIDIA route tests now
allow native copies after selected work, before Close and after Reset; later
half-size copy pixels remain exact, markers stay4, GPU validation/retirement
pass. Six negative cases include early duplicate.111 native tests,29 reviewers
and294912 exact effect pixels passed. SDK/effects code unchanged; three sealed
v1.24.4 SDK cases reused with source/artifact hashes verified (see proof JSON).
No build warnings. Offline local/scaled-copy-lifetime-20260928 SEALED,
manifest SHA e1b57f0d7b6a10890295c5e17a6152649c17b2d594aaf51fe64deddf5c65ed26. Prelaunch manifest freezes sources/DLL/deployment;
append runtime files beside it. Never edit sealed evidence. Research/runtime
documentation: docs/research/scaled-live-output-route.md and
docs/runtime/scaled-live-output-route.md.

---

# Current checkpoint: 2026-09-28 — v1.24.4 installed, awaiting game launch

Latest user reply: "Scene normal; game closed normally" after v1.24.3.
Game closure verified. Partial capture archived; final-screen topology fixed,
built, tested and installed in the existing dlssTEST MO2 slot. Next: launch
usual Monster Hunter: World - DLSS 1.2.110 test entry, load a save, report ready.
Native AA/DLSS/MotionBlur Off; DX12 windowed2560x1440, unchanged window size.
No v1.24.4 game capture yet. Keep the DLL installed per user preference.

- Target mhw_scaled_output_probe version1.24.4, SHA-256:
  5f3a910551271eafb7869eb1423e32d81ce1c30c3bdee273e521b23ddf33cbcc
- Output local/scaled-temporal-20260928/v1.24.4, with sdk-runtime-dir.txt.
- Backup previous-slot.dll.disabled is v1.24.3, SHA-256:
  790610a3b3365427dce404ceb658262b15af5dda95f29f35b46f964ae0c4261f.
- Settings/profile/mode/provider/host identities unchanged. 48 native code
  gates rechecked; 483 source files frozen. See preparation/deployment receipts.
- Review matching startup log first. Arm with tools/arm_scaled_temporal.py,
  exact input log/PID/creation/DLL hash, once save loaded. No native DLSS toggle.
  Review with tools/review_scaled_temporal.py after the automatic bounded stop.
- Same49 hooks,23 binding hooks; sole observer viewport owner, resolution1hook.
  Observer uses owned-targets-only control, preserving queued AA activation.
- Only new rendering behavior: the final three-vertex screen draw preserves
  TRIANGLESTRIP (5), matching the native setup. Exact validation remains.
- Receipts0/1 prepare effects; receipt2 attempts one Performance DLSS
  1280x720->2560x1440, bypasses native TAA once, expands final copy and3 output
  effects on same command list/Reset generation. GPU completion, retirement,
  jitter/scale restoration and hooks disabled must all pass review.
  Full live DLSS output and continuous DLSS remain unverified.

v1.24.3 PID12360 creation134351004950496566: three inputs and nine draws.
One SDK Evaluate returned success, expanded copy and first two full-size
effects recorded; final draw rejected before commands and native draw forwarded.
Routefault7/drawfault12/submissionfault18/parentfault13; selected recording's
GPU completion and retirement NOT verified. Private references pinned until
exit. Full scale/jitter restored, hooks disabled, normal scene and normal exit.
Log SHA 4a7a3ea7bd216e28902ab0e9ab5a7671d91d0e02f442a64a7a1ac4247ca9f0c9.
Native flags16 select table index4 -> strip5, whereas implementation expected
list4. Read-only live code/table check in failure-review-12360.json proves the
mapping; previous v1.23.12 draws have the same flags. Archived under
local/scaled-temporal-20260928/v1.24.3, manifest SHA 8497fb10a79e804df824c6973dcf70375a1fea8179ae933bd24759cc333062bb.

Offline v1.24.4: 18 actual native vertex arrays AND topologies match production
geometry; that exact regression failed before the fix. All111 native tests,
29 reviewers, WARP/NVIDIA production route,5 negative cases,294912 exact effect
pixels and3 real SDK/effect cases with33177600 exact pixels passed. GPU
validation clean; no build warnings. Queue-isolation/listener checks pass.
Offline local/scaled-topology-20260928 SEALED, manifest SHA 6421f1b499ff19633217b00b70449857b3af30507bc1e9cff732bda1bae33222.
Prelaunch manifest freezes sources/DLL/deployment; append runtime logs beside
it. Never edit sealed older evidence. Docs: scaled-live-output-route.md under
docs/research and docs/runtime.

---

# Current checkpoint: 2026-09-28 — v1.24.3 installed, awaiting game launch

Latest user reply: "Scene normal; game closed normally" after v1.24.2.
Game closure verified, partial capture archived, vertex-order correction built,
tested and installed in the existing dlssTEST MO2 slot. Next: launch usual
Monster Hunter: World - DLSS 1.2.110 test entry, load a save, report ready.
Native AA/DLSS/MotionBlur Off; DX12 windowed2560x1440, unchanged window size.
No v1.24.3 game capture yet. Keep the DLL installed per user preference.

- Target mhw_scaled_output_probe version1.24.3, SHA-256:
  790610a3b3365427dce404ceb658262b15af5dda95f29f35b46f964ae0c4261f
- Output local/scaled-temporal-20260928/v1.24.3, with sdk-runtime-dir.txt.
- Backup previous-slot.dll.disabled is v1.24.2, SHA-256
  294e6de6332297e0f24731308b9d99a247e2fb828292b8ae2201c141cd07e39e.
- All settings/profile/mode/provider/host identities unchanged. 48 native code
  gates rechecked;483 source files frozen. See preparation/deployment receipts.
- Review matching startup log first. Arm with tools/arm_scaled_temporal.py and
  exact input log/PID/creation/DLL hash once save loaded. No native DLSS toggle.
  Review with tools/review_scaled_temporal.py after the automatic bounded stop.
- Same49 hooks,23 binding hooks; sole observer viewport owner, resolution1hook.
  Observer uses owned-targets-only control, preserving queued AA activation.
- Only new runtime change: color-grade quad order is top-left,top-right,
  bottom-left,bottom-right; motion-filter stays top-left,bottom-left,top-right,
  bottom-right. Exact validation remains, full-output UVs follow native order.
- Plan remains receipts0/1 prepare effects, receipt2 attempts one Performance
  DLSS1280x720->2560x1440, bypasses native TAA once, expands final copy and3
  output effects on same command list/Reset generation. Full GPU completion,
  retirement, jitter/scale restoration and hooks disabled must all pass review.
  Full live DLSS output and continuous DLSS remain unverified.

v1.24.2, PID41480 creation134350990791981848: setup PASSED. Two input receipts,
two native draw snapshots. Motion effect prepared; color-grade constructor
failed "effect native vertices". It used the motion-filter strip order for
both stages. Color-grade values exactly match the earlier v1.23.12 capture;
this was an implementation assumption, not a changed game path. No SDK
evaluation/expanded copy/output replacement. Scale restored, hooks disabled,
one jitter injection restored. Private references retained until normal exit.
Partial log SHA c7fd2a3a18da31e9ebdf103d61f371135364fd442730386eb100e6e408809155.
Archive local/scaled-temporal-20260928/v1.24.2 SEALED; manifest SHA
24a04bcb5f5ff8195d1ec0980d06da99a02464b0b1ae76ccaefadbcd07c5df62.

Offline v1.24.3: all12 actual saved vertex arrays match production geometry
(nine fromv1.23.12,three fromv1.24.2); that exact check failed before fix.
111 native tests,29 reviewers, WARP/NVIDIA production route,5 negative cases,
effect comparisons294912 exact pixels and real SDK/effect success/spatial/
recovery33177600 exact pixels passed with clean GPU validation. Builds clean.
Observer queue-isolation/listener lifecycle checks remain passing.
Offline root local/scaled-vertices-20260928 SEALED; archive-final SHA
077ec768546a1dc7b922dc4f0659d498f37b5bd157e2588cae6d212459ef990f.
Prelaunch manifest seals sources/DLL/deployment; append runtime logs beside it.
Docs: docs/research/scaled-live-output-route.md and
docs/runtime/scaled-live-output-route.md. Never edit sealed older evidence.

---

# Current checkpoint: 2026-09-28 — v1.24.2 installed, awaiting game launch

Latest user report: "Scene normal; game closed normally" after v1.24.1.
Game closure verified; setup-order correction built/tested and installed in
the existing dlssTEST MO2 slot. Next launch uses the usual Monster Hunter:
World - DLSS 1.2.110 test entry. Load a save and report ready; leave native AA,
DLSS and MotionBlur Off, DX12 windowed2560x1440, window size unchanged.
No v1.24.2 game capture exists yet. Keep this DLL installed as requested.

- DLL version1.24.2, target mhw_scaled_output_probe, SHA-256:
  294e6de6332297e0f24731308b9d99a247e2fb828292b8ae2201c141cd07e39e
- Output: local/scaled-temporal-20260928/v1.24.2.
- Exact previous DLL backed up there as previous-slot.dll.disabled, SHA-256
  5571e644127d08c966bd5ed1d935661236576824e391af755ef02cf45f50d36b (v1.24.1).
- Source snapshot483 files,48 native code gates verified, all graphics/profile/
  mode/provider/host identities unchanged. See preparation.json/deployment.json.
- Startup capability interception still precedes gameplay. Read/review matching
  startup log; arm with tools/arm_scaled_temporal.py using exact live log/PID/
  creation and DLL hash once save loaded. Header admits1.24.2. No need to toggle
  native DLSS or AA. The explicit recorder deadline is2 seconds after install.
- Binding observer solely owns viewport slot21 and forwards observations to
  resolution controller. Total49hooks;23 binding hooks; resolution owns1.
- New binding_hook_control/header and hook_control/observer telemetry must be
  owned-targets-only. Observer install/finish use targeted MH_EnableHook and
  MH_DisableHook and never MH_ApplyQueued, preserving queued AA activation.
- Bounded plan remains3 input receipts:0/1 prepare native effect PSOs; receipt2
  attempts one Performance DLSS1280x720->2560x1440, native TAA dispatch bypass,
  full copy and3 full-size effects. Same list/Reset generation, GPU completion,
  retirement, scale restoration and hooks disabled all require review.
  This is not continuous DLSS or a live quality/performance result yet.

v1.24.1 result, PID19212 creation134350984651080025:
- Startup accepted; viewport setup fixed. Failed downstream-code before any
  input receipt/SDK evaluation. Observer MH_ApplyQueued activated8 AA hooks
  queued by activation::prepare; gate0x24f1fc0 saw our own enabled detour.
- Activation ran209 frames before recorder admission. After shutdown all3
  downstream code ranges matched original digests in bounded read-only checks;
  full scale1.0, all hooks disabled. User confirmed normal scene/normal exit.
- Input log SHA090746be3f02dc9662a60a74bf4c5e556ec6c70e0b26f1d231203d307025d1f8.
- Archive local/scaled-temporal-20260928/v1.24.1 is SEALED; manifest SHA-256
  08266ec517589c86662d4f70021a4c7264f16609b25c3c5801f81ebfde1eb7e2.

v1.24.2 offline evidence: regression fixture reproduced old observer applying
another owner's queued enable/disable. Both operations now remain pending
across observer install and finish, then apply only when their owner requests.
WARP/NVIDIA effects and route checks pass, no GPU warnings/errors,294912 exact
effect pixels,9 application-list viewport notifications perroute,5 negative
route cases,111 native regression tests,29 reviewer tests. Prior live review
still accepted. Builds clean. SDK execution unchanged; prior real SDK/effect
results remain in local/scaled-route-20260928 (not rerun this correction).
Offline root local/scaled-setup-20260928 is SEALED; archive-final SHA-256
5a7111bef10a5cf6b9e6211972854b4f0db4fe7260d2ad7915a57a11e31e5f70.
Prelaunch snapshot sealed; append game logs beside it, do not edit snapshots.
Read docs/research/scaled-live-output-route.md and
docs/runtime/scaled-live-output-route.md for implementation and test details.

---

# Current checkpoint: 2026-09-28 — v1.24.1 installed, awaiting game launch

Latest user report: "Scene normal; game closed normally" after the v1.24.0
setup failure. Closure verified. Correction built, checked offline, and
installed in the existing MO2 test slot. MO2 PID40380 is responsive.
Next: launch the usual Monster Hunter: World - DLSS 1.2.110 test entry,
load a save, leave native AA/DLSS/MotionBlur Off, DX12 windowed2560x1440.
Do not change the window size. No v1.24.1 game capture exists yet.

- Version1.24.1 mhw_scaled_output_probe DLL SHA-256:
  5571e644127d08c966bd5ed1d935661236576824e391af755ef02cf45f50d36b
- Runtime output: local/scaled-temporal-20260928/v1.24.1.
- Backup v1.24.0: previous-slot.dll.disabled, SHA-256
  424fadc64abb2ab7f47b02c2470a6cd01826836682e21578e578816d596ab90a.
- Full configuration/sidecar/source identities are in preparation/deployment
  receipts. No game assets/settings changed. User wants this DLL kept installed.
  Do not restore the previous DLL automatically at normal exit.
- SDK directory sidecar and provider identity unchanged. Startup interception
  precedes gameplay. Check matching startup log, then arm exactly that input
  log/PID/creation/DLL SHA with tools/arm_scaled_temporal.py when save is loaded.
- Only the binding observer now owns RSSetViewports (slot21). It forwards
  read-only admitted observations to the resolution controller, which owns
  only its scale-update hook. Budget49; binding observer23. API forwards once.
- Reviewer expects viewport_hook_owner=binding-observer, listenertrue,
  resolution shared_viewporttrue/owned_hooks1, positive viewport observations.
- Same bounded output plan: receipts0/1 prepare native effect PSOs; receipt2
  attempts one Performance SDK evaluation1280x720 ->2560x1440, one full copy,
  three full-output effect draws, native TAA bypass for that selected frame.
- Exact command list/Reset generation, submission markers, complete GPU work
  and resource retirement must pass review_scaled_temporal.py. This is not yet
  continuous DLSS or a live quality/performance result.

v1.24.0 result: bindings-create-21-3 = MH_ERROR_ALREADY_CREATED because two
components owned RSSetViewports. Startup and CreateFeature succeeded; no input
receipts or SDK evaluations. Scale restored, all hooks disabled, normal exit.
Archive local/scaled-temporal-20260928/v1.24.0 is SEALED; manifest SHA-256
03dc190947c4f48013846b5463145ed8f31d03de13782f54c15fee87cf9fb30c.

Correction checks: clean builds,111 native tests,29 Python reviewer tests,
prior live captures still accepted, production route WARP/NVIDIA with9 exact
application-list listener notifications,5 negative route cases, listener
admission/stop/resume/removal,23 raster checks peradapter,294912 exact effect
pixels and no GPU warnings/errors. The debug layer also calls a different
internal list; initial fixture failures and diagnostic identity traces are
preserved. Fixture now measures exactly-once delivery perapplication list.
SDK execution path unchanged; prior real SDK/effects tests remain archived
in local/scaled-route-20260928 and were not rerun for this ownership correction.
48 native code gates and both executable identities rechecked.

Offline correction archive local/scaled-viewport-20260928 is SEALED.
archive-final.json SHA-256: b8bf08fb0dbdd9702c77b547b899154a3b1b56512b3de696676b5d9f5117ba35
The v1.24.1 prelaunch manifest freezes482 source files, DLL and deployment
evidence; append new game logs beside it, do not edit frozen files.
See docs/research/scaled-live-output-route.md and
docs/runtime/scaled-live-output-route.md for details and removal procedure.

---

# Current checkpoint: 2026-09-28 — v1.24.0 installed, awaiting game launch

Latest authorization: user “nice work thank you <3 lets do”. The live full-output
route is implemented, checked offline and installed in the existing MO2 test
slot. The game was closed at deployment; MO2 PID 40380 was responding. Next user
step: launch the usual “Monster Hunter: World - DLSS 1.2.110 test” entry, load a
save and report ready. Do not toggle native DLSS or AA. No new game capture yet.

- DLL target: mhw_scaled_output_probe, version 1.24.0.
- Installed SHA-256: 424fadc64abb2ab7f47b02c2470a6cd01826836682e21578e578816d596ab90a.
- Output/evidence: local/scaled-temporal-20260928/v1.24.0.
- Backup: previous-slot.dll.disabled in that directory, SHA-256
  83fbad2cc7c41464be83f2997ab57e69b2a5473aaac76d142a0c402d4efbf9ae (v1.23.12).
- SDK config is sdk-runtime-dir.txt in that output directory. Startup capability
  interception precedes gameplay. Modern runtime hash remains be6e434a94ca32499515eb62ca0e6c274526055d568d0426e4c652dcdfb6ee6e.
- Native AA/DLSS/MotionBlur Off, DX12 windowed2560x1440; graphics/profile/mode
  files unchanged and identity-checked in deployment.json. User wants the DLL
  kept installed. Do not automatically restore at normal exit.
- Three input receipts, one Performance SDK evaluation on receipt2, one expanded
  native final copy and three full-size output draws. The two earlier receipts
  qualify/prepare private PSOs. Exactly the same command list and Reset generation
  are required throughout. The existing private fence covers the entire recording.
- New draw/output API observer has23 hooks (default binding observer still18).
  Total armed hook budget50. Existing two-second capture deadline remains.
- Native code gates48 verified against both executable identities before freeze.
- arm_scaled_temporal.py now parses1.24.0. Use the exact PID/creation log/hash;
  inspect matching startup log, then arm once the user is in the save. Run
  review_scaled_temporal.py afterward; new temporal_scaled_output.py validates
  frame/generation/markers, SDK geometry/pixels, startup and native TAA bypass.
- Accepted capture must report route prepared_draws9/replaced_draws3/copy_count1,
  resolve_outcome1, SDK evaluations1, route retired/hooks_disabled true, no faults.
  Continuous DLSS, image quality, transitions and speed remain untested.

Offline checks:111 native regression tests,29 reviewer tests, production output
route on WARP/NVIDIA,5 negative copy contracts,23 raster/OM admission checks,
294912 exact effect pixels,3 real SDK/effects success/spatial/recovery cases with
33177600 exact combined pixel comparisons. New warnings fixed; successful final
builds clean. Earlier failed builds/test logs are retained as development evidence.
Offline archive local/scaled-route-20260928 is SEALED; do not modify it.
Its archive-final.json SHA-256 is e4c02f4e9761c477b0957ba8a723a187871346082f3ca09bb68c3160e7854670.
The deployment folder's prelaunch-manifest.json seals the482-file source snapshot,
DLL, config, preparation and deployment receipts; new runtime logs go beside it.

Read docs/research/scaled-live-output-route.md and docs/runtime/scaled-live-output-route.md
for ownership/calling conventions, fail-closed boundaries and exact test scope.

---

# Current state and next decision

IMPLEMENTED AND VERIFIED OFFLINE 2026-09-28: **full-resolution native effects**.
[Implementation, verification and live-integration boundary](scaled-full-effects.md).
The production Effect component creates private full-output motion-filter,
color-grade and final-screen pipelines. It preserves native effect constants
and auxiliary coordinates, expands color/output coverage, and restores the
original PSO, vertex slot0, viewport and scissor after each replacement draw.
Optional API raster tracking has22 hooks; default binding observer stays18.

WARP and NVIDIA [0,93935] each passed66 draws,147456 exact packed pixels,
381 rejected contracts and15 raster state checks. Geometry24 supported/22
rejected;3 focused CTest regressions passed; final builds /W4 /WX.
Combined SDK -> native-effects GPU chains passed normal Performance DLSS,
spatial-only output and injected after-handoff SDK failure:33177600 exact
2560x1440 packed-pixel comparisons total, zero GPU warnings/errors. Inputs
stay on the GPU between stages; actual API state restoration is checked.
Synthetic effect constants/auxiliaries: this is not live game quality or
performance proof. All fixture recordings/GPU work retired; hooks disabled.

No new runtime DLL was deployed or prepared. v1.23.12 recorder remains installed
and unarmed, SHA256
83fbad2cc7c41464be83f2997ab57e69b2a5473aaac76d142a0c402d4efbf9ae.
Game closed; MO2 responding PID40380 at final check. Native AA/DLSS/MotionBlur
Off, DX12/windowed2560x1440. Installed DLL and four configuration hashes
verified unchanged. No pending question or capture; no launch requested yet.

Next: connect the verified scaled resolve to the native final-copy and all
three selected output draws. Prepare pipelines before committing full output;
qualify actual API bindings/RT state; restore touched state and forward other
calls; enlarge only the matching copy rectangle; prove downstream list/fence
retirement separately from earlier input-copy completion. This live routing
is not implemented yet. Original vertex-buffer COM ownership is still
unqualified; bounded IA-helper static mapping and exact code hash are in the
new research note. Do not guess from unrelated resource layouts. Existing
DrawInstanced hook is in depth_routes/depth_draw.inl: extend that route rather
than install a competing hook. Frame/packet identities already exist in
scaled_temporal/draw_execution and postfilter. No further live capture is
needed just to recover the known shader or CPU vertex recipe.

SEALED local/scaled-effects-20260928/:872 files plus archive-final.json.
Archive SHA256
fddad4bfab8283a516cb6dd51ca85d929e3fa5b9427e5b1cd5eb5c1755977020.
Includes final source/binaries, final and earlier test evidence, verification
report and static mapping. Do not modify this root or prior accepted archives.
Latest standalone cases: warp-final/,hardware-final/; combined cases:
sdk-final-upscale-resolve-{success,off,after-handoff}/. Frozen source excludes
this append-only rolling checkpoint. Keep the latest installed DLL as requested.

Previous entries below are historical.

# Current state and next decision

ACCEPTED, CLOSED AND ARCHIVED 2026-09-28: **v1.23.12 output-draw execution**.
PID11952 / creation134350922451217513, frames13642-13644. User confirmed normal
scene and normal exit; process exit verified. Current DLL stays installed.
No active game, pending question/capture or next prepared runtime build.
[Result and full-output implementation consequence](scaled-draw-execution-result.md).

All nine native draw-wrapper observations passed the frozen reviewer: motion
filter, color grading and final screen for three frames. All18 VS/PS files
saved; five unique programs disassembled. First two stages use1280x720 active
viewports; final stage uses2560x1440. All resources are2560x1440 R11G11B10_FLOAT,
single-sampled. Native shader/layout/render metadata stable per stage.
Final-screen VS passes position/UV; PS is one explicit-LOD sample t0/s0 with
cb3[1].x mip level. It does not apply gamma or consume depth/motion. Preserve
its native sampling constants and half-texel convention when expanding UVs.
No new live capture needed merely to obtain these shader/state recipes.

All three temporal input triplets passed, including19,353,600 exact converted
float comparisons. Existing input GPU work completed/retired. New draw evidence
is bounded CPU execution/cache evidence, not GPU submission/completion or
API-captured PSO descriptors. No full-output handoff or DLSS evaluation yet.
Next: build private full-size effect pipelines/output routing and connect the
verified scaled SDK resolve + motion-filter adapter, with offline tests first.
Preserve exposure/LUT/tone mapping and low-resolution auxiliary coordinates.
Continue DLSS -> Reflex -> FG/MFG; beta polish and CMFM parked.

Cleanup verified: all reference pins released, no pending callbacks or faults,
all hooks disabled, one jitter write restored, full render scale restored.
Settings/profile/modlist/mode, installed DLL/backup and sidecar unchanged.
SEALED local/scaled-temporal-20260928/v1.23.12/:605files, archive SHA256
f66aea915701e54fc90062bd5a4147f8230247cd359ff5e3c55240aa743981bc.
Final/captured log SHA256
0d95398561970f573beb61aea806c9f7d8e92cc36d5f23b52a5531cdfb599ab3.
Installed DLL SHA256
83fbad2cc7c41464be83f2997ab57e69b2a5473aaac76d142a0c402d4efbf9ae.
Slot D:/MO2/MHW/mods/dlssTEST/mhw_dlss_bridge_probe.dll.
Do not modify the sealed root (including frozen source-1.23.12/) or earlier
archives. Capture remains available as inputs-11952-134350922451217513-captured.jsonl;
review-11952.json accepted; run-result.json; shader-disassembly.json; archive-final.json.
Current NVIDIA LUID [0,93935]. DX12/windowed2560x1440/native AA,DLSS,MotionBlur Off.

Previous entries below are historical.

# Current state and next decision

PREPARED AND INSTALLED 2026-09-28: **v1.23.12 output-draw destination fix**.
Game closed; recorder unarmed; MO2 open/responding PID40380. Next user action:
launch the same MO2 test entry, load a save, keep native AA/DLSS Off, MotionBlur
Off and window size unchanged, then report loaded. Arm only that process
generation after checking the startup header and installed DLL hash. Keep the
latest DLL installed. No runtime capture has occurred with this build.
[Hook contract, failed-run finding and fix](scaled-draw-execution.md).

v1.23.11 CLOSED AND SEALED: PID18584 / creation134350911012735423.
Postfilter fault4 happened before new draw execution: view+0x460 is an
attachment, not the texture owner (attachment+0x58). Existing downstream
observations already identified the correct destination. v1.23.12 passes that
verified owner into postfilter and revalidates it before publishing. The new
integrated publish_draws regression distinguishes the two objects, requires
all nine publications, and rejects completion until draws actually execute.
No capture-acceptance criteria were relaxed.

Failed-run cleanup was complete: all references released, full scale restored,
all hooks disabled, no jitter writes and no SDK evaluations. User confirmed
normal scene and normal exit; process exit verified. Original rejected review
and partial log preserved in local/scaled-temporal-20260928/v1.23.11/.
611-file archive SHA256
4fce596c4982deb24ac274c339cdac6322f912bc427987118385235de75387c1.
Partial/final log SHA256
7ec0685794e3d05efb2986045888a03f981913d0e8a4f1f32106736ea3c109d8.
Do not modify that sealed root or earlier archives.

v1.23.12 builds with /W4 /WX; 101 affected native/GPU tests and 23 external
reviewer tests passed, including the current-version shared review. Prior
accepted v1.23.10 capture re-reviewed successfully; combined retained WARP
capture reports clean GPU validation. All 45 code-gate checks verified against
the pinned host/decrypted image. No new hook or resource mutation added by fix.

Installed DLL SHA256
83fbad2cc7c41464be83f2997ab57e69b2a5473aaac76d142a0c402d4efbf9ae.
Slot D:/MO2/MHW/mods/dlssTEST/mhw_dlss_bridge_probe.dll.
Output/frozen 474-source tree: local/scaled-temporal-20260928/v1.23.12/.
547-file prelaunch manifest SHA256
67582382795e812a97cde543d4681ae6229f707cb22de61b37312b690d4b493a.
Exact previous DLL preserved as previous-slot.dll.disabled, SHA256
97ea0967f17b4b4373996a257b0371ae216057c17feb83b9f26907c6571da643.
Previous output sidecar and four unchanged configuration files preserved.
Use frozen source-1.23.12/tools/arm_scaled_temporal.py and review_scaled_temporal.py.
The root is open for the next run; frozen/prelaunch-listed files must not change.

Capture contract unchanged: 26 hooks; three input frames; one scoped/restored
jitter sample; nine native output-draw snapshots across motion filter, color
grading and final screen; 18 bounded VS/PS artifacts with input-layout/render
recipe/attachment metadata. Original native draws forwarded. No modern SDK
call or full-output replacement. New draw records certify bounded native
wrapper execution, not GPU submission/completion or descriptor contents.

After qualification, construct private full-size native-effect pipelines and
connect verified scaled SDK resolve/filter adapter. Preserve exposure/LUT/tone
mapping and low-resolution depth/motion coordinates. DLSS -> Reflex -> FG/MFG;
beta polish and CMFM remain parked. Current NVIDIA LUID [0,93935]; settings
DX12, windowed2560x1440, native AA/DLSS/MotionBlur Off.

Previous entries below are historical.

# Current state and next decision

PREPARED AND INSTALLED 2026-09-28: **v1.23.11 output-draw execution capture**.
Game is closed; recorder unarmed; MO2 open/responding PID40380. Next user action:
launch the same MO2 test entry, load a save, leave native AA/DLSS Off, MotionBlur
Off and window size unchanged, then report loaded. Arm only that new process
generation after checking its startup header and DLL hash. Keep latest DLL
installed. No live run or SDK/output replacement has occurred with this build.
[Contract, evidence scope and removal procedure](scaled-draw-execution.md).

This extends the accepted v1.23.10 owner-chain capture with one native hook,
26 total. It qualifies all three selected output draws across three frames:
motion filter, color grading, final screen. Eighteen bounded VS/PS artifacts,
input-layout semantics, blend/depth/raster recipe, actual output-resource
descriptions, cached root/PSO/RTV and viewport/scissor are captured at native
wrapper execution. Original draw forwarding, native effects and geometry stay
unchanged. It adds no GPU commands. Existing half scale, one jitter sample and
full-scale restoration remain. Nine draw completions are required.

100 affected native/GPU tests and23 external reviewer tests passed; previous
v1.23.10 live capture re-reviewed successfully. Native fixture uses synthetic
packet/wrapper state with real WARP resources; no claim of game GPU draw evidence.
Full input GPU fixture also passed with clean validation. Project /W4 /WX.
45 code-gate checks verified against pinned host/decrypted image. Shared
reviewer version branches include1.23.11 and are exercised by the current-version
combined fixture, avoiding the last run's header/common-review mismatch.

Installed DLL SHA256 97ea0967f17b4b4373996a257b0371ae216057c17feb83b9f26907c6571da643.
Slot D:/MO2/MHW/mods/dlssTEST/mhw_dlss_bridge_probe.dll.
Output/frozen474-source tree: local/scaled-temporal-20260928/v1.23.11/.
603-file prelaunch manifest SHA256
31f1e3dd771eadcfba067f9104350b01085fd25eeec6dc3937f1ad05803bca21.
Exact previous DLL preserved as previous-slot.dll.disabled, SHA256
326daf080aad3768d77d9c6407f36a5f5ae07745b396150c4460a128e479b905.
Exact previous output sidecar and four unchanged configurations preserved.
Use frozen source-1.23.11/tools/arm_scaled_temporal.py and review_scaled_temporal.py.
Prelaunch root is open for this run, but frozen source/prelaunch-listed files
must not be changed. Earlier v1.23.10 and offline implementation archives stay sealed.

After live qualification, build private full-size native-effect pipelines and
connect the verified scaled SDK resolve/filter adapter. The final-screen shader
is included in this capture. Keep original exposure/LUT/tone-map behavior and
low-resolution auxiliary depth/motion coordinates. No direct GPU submission/
completion or descriptor-table contents are certified for the new draw records.
Continue DLSS -> Reflex -> FG/MFG; beta polish and CMFM remain parked.
Current NVIDIA LUID [0,93935]; settings DX12, windowed2560x1440, AA/DLSS/MotionBlur Off.

Previous entries below are historical.

# Current state and next decision

ACCEPTED, CLOSED AND ARCHIVED 2026-09-28: **v1.23.10 color-grading capture**.
PID48440 / creation134350890950996078, frames25218-25220. User confirmed
normal scene and normal exit. Process exit verified; full scale restored;
all hooks disabled and GPU work completed; settings/profile/modlist/mode,
installed DLL, backup and sidecar unchanged. Keep this DLL installed.
No capture/question/tool is pending and no next runtime build is prepared.
[Result, shader observations and integration consequence](scaled-colorgrade-result.md).

The corrected reviewer accepts all three triplets (19,353,600 exact converted
float values). Frozen prelaunch reviewer initially rejected resource references:
seven shared Python modules omitted version1.23.10. Only version membership was
corrected; no runtime code/evidence/tolerance changes. Original rejection and
frozen sources preserved. Separate reviewer-correction/ tools accept this and
prior v1.23.9 capture;18 reviewer tests pass, including latest-version shared
review and bad pin-count rejection. Use the corrected review, not the frozen
prelaunch reviewer. This supersedes the prepared-only entry below.

Confirmed CPU owner chain: TAA final copy -> motion filter -> color grading ->
final-screen draw. Color-grading PS is5824bytes, ps_5_0, SHA256
174a09e5b23c7e01fb6c47ce86b61250d695113109abb4ffae7219117c31a8cb.
All three quads use UV0..0.5. Shader samples scene color through UV only;
SV_POSITION/depth/motion unused. Other sampling is a color-derived3D LUT.
Inference: full-size target/viewport and UV0..1 should preserve this native
shader and its tone-mapping effects. Live CB values/GPU states not captured.
Final screen already targets full viewport but still samples half-size source.

Next implementation: qualify native GPU bindings/state and lifetimes, apply
full-size motion-filter/color-grading draws and final-screen coordinates, then
connect the already verified scaled SDK transaction. Preserve low-resolution
motion/depth coordinates in the motion-filter adapter and earlier exposure pass.
No live SDK evaluation/full-output handoff occurred here. DLSS -> Reflex ->
FG/MFG remain the priorities; DLAA beta polish and CMFM stay parked.

SEALED evidence: local/scaled-temporal-20260928/v1.23.10/ (816files).
Archive SHA256 4b6ba9779c235df8505a8f0b151a04a3d414863d2f6496cbfee78f13f8586304.
Final log SHA256 9ca7602aace6f68cc93c9a09381bb7a58f0e85a5e6c0f35c2f2afb15389d7d8c.
Installed DLL SHA256 326daf080aad3768d77d9c6407f36a5f5ae07745b396150c4460a128e479b905.
Do not modify the sealed root or earlier archives. Settings remain native AA/
DLSS Off, MotionBlur Off, DX12, windowed2560x1440. Current NVIDIA LUID [0,93935].
Local GPU fixtures and static exports from the preceding entry remain available.

Previous entries below are historical.

# Current state and next decision

PREPARED AND INSTALLED 2026-09-28: **v1.23.10 color-grading capture**.
Game is closed; recorder is unarmed; MO2 is open/responding (PID40380).
Next user action: launch the same MO2 test entry, load a save, keep native
AA/DLSS Off, MotionBlur Off and the window size unchanged, then report loaded.
Arm only the new process generation after its startup log is checked. No
live test of this version has happened yet. Keep this DLL installed afterward.
[Capture contract](scaled-colorgrade-observer.md).

Offline motion-filter adaptation implemented and verified on WARP and current
NVIDIA adapter [0,93935]. Each:16draws,229376 exact float comparisons,13 rejected
inputs,zero GPU warnings/errors. Total458752 comparisons. Native 1x equivalence,
2x copy/edge/alpha checks and varying-color blur texture-layout references pass.
Original shader/branches/auxiliary coordinates retained; only full-size color
reads and raster coordinate basis adapted. This is a private shader component,
NOT live DLSS presentation. [Implementation and limits](scaled-filter-adapter.md).
The earlier full-size SDK/spatial recovery transaction remains verified.

v1.23.10 adds no hooks (still25) and no SDK evaluation/output mutation. It
captures the already-constructed color-grading quad/PS for three selected frames,
qualified by AutoExposure unit+238 endpoint and complete downstream owner chain.
Twenty callback cases and87 affected native/GPU tests passed;7 current reviewer,
6 historical reviewer (+1skip),10 combined reviewer checks and prior live review
passed.42 native byte gates match the pinned executable. Project code /W4 /WX.

Installed DLL SHA256:
326daf080aad3768d77d9c6407f36a5f5ae07745b396150c4460a128e479b905
Slot D:/MO2/MHW/mods/dlssTEST/mhw_dlss_bridge_probe.dll.
Output and frozen469-source tree: local/scaled-temporal-20260928/v1.23.10/.
Exact previous DLL and output sidecar preserved there; four configurations
verified unchanged. Backup SHA c6b22619dc59a4ae78f06608e31e3b3b6b279c191dcfda6664d5641d9d8380d1.
522-file prelaunch manifest SHA
6213717294e736e50080f6452cf82e0b569b0b6ba0d3c0a62e9455db76fc7f5d.
Use frozen tools/arm_scaled_temporal.py and tools/review_scaled_temporal.py for
this run. No automatic restoration; manual removal procedure in the capture doc.

Offline adapter evidence SEALED: local/scaled-filter-20260928/,564files;
archive SHA42c9364e90bb00de4edfdfbd0f64634fc1efaaed07324d582d23829be15bbf22.
Do not modify that archive or prior v1.23.9/scaled-recovery archives. Current
NVIDIA LUID changed after restart; old [0,98547] is stale. Hardware fixture can
explicitly select the sole NVIDIA adapter with --hardware 0 0 and records its
actual identity. Private shaders remain ignored; public DXBC hash dependency
is pinned/licensed in third_party/dxc_hash and cmake/DxbcHash.cmake.

Next integration: analyze color-grading PS/quad, qualify native GPU bindings and
states, full-output viewport/UVs and final-screen triangle, then connect the
verified scaled SDK transaction. Preserve native effects. Continue DLSS ->
Reflex -> FG/MFG. DLAA beta polish and CMFM remain parked.

Previous milestones below remain historical.

# Current state and next decision

OFFLINE IMPLEMENTATION VERIFIED 2026-09-28: **full-size scaled output transaction**.
Game remains closed; installed v1.23.9 remains unchanged/unarmed. No new live
probe is deployed or ready, and no user question/capture is pending.
[Implementation and evidence](scaled-output-recovery.md).

New C++ component scaled_temporal::record_resolve connects the existing scaled
InlineFrame to an explicitly reserved full-output spatial recovery. It uses the
SDK object's exact geometry/private raw color, validates before commands, restores
bindings/states, and never calls native TAA. temporal_present::Output now has an
explicit SpatialSource overload; old same-size behavior remains the default.

Seven focused native tests passed. Six final hardware cases passed with no GPU
warnings/errors: three retained-history Performance evaluations, three spatial-
only frames, three frames for each of three injected failure points, and one
DLAA regression. Every pixel of all twelve recovery frames matches the independent
packed bilinear reference (44,236,800 pixels). Padding is poisoned. Recovery
fence/reset retirement passes. Existing failed SDK object retention until process
exit is unchanged. These are archived-image GPU fixtures, NOT live presentation
or moving-scene quality tests.

Static final transfer identified: the last stage15/order134217713 entry belongs
to the AutoExposure unit at +0x238 in all three v1.23.9 records. Execute RVA
24d0780 (vtable3538400+60) constructs ToneMappingApply_ColorGrading with input
unit+248/outputunit+250; it is not a plain copy. Both its quads use saved
context/allocation UVs. No new native hook/gate or proprietary shader added.

Next: native output routing still needs the motion-filter mixed-resolution
color/depth/motion sampling, full-size color-grading viewport/UVs and final
screen-triangle coordinates. Preserve native effects. Use the new full-output
transaction when that routing is qualified; do not claim it is wired in-game.
Continue DLSS -> Reflex -> FG/MFG; DLAA beta polish and CMFM remain parked.
Current settings stay native AA/DLSS Off, MotionBlur Off, DX12, windowed2560x1440.
Evidence and frozen sources: local/scaled-recovery-20260928/.

Previous accepted live captures and preparations below remain historical.

# Current state and next decision

ACCEPTED, CLOSED AND ARCHIVED 2026-09-28: **v1.23.9 post-filter capture**.
PID 49960 / creation 134350275210098440; frames24926-24928. Frozen reviewer
accepted all three. User confirmed normal scene and normal exit; process exit,
unchanged settings/profile/modlist/mode, installed DLL, backup, frozen sources
and prelaunch manifest verified. Keep current DLL installed. No pending
question/tool/capture, no next build prepared. MO2 was open PID3800.
[Result, shader observations and limits](scaled-postfilter-result.md).

New: order100 Execute records opcodes11,6,6,6,11,0 on each selected frame.
Three compute commands plus the pixel draw remain with MotionBlur Off. Pixel
shader is ps_5_0,10008bytes,SHA3eed9b57bbbdf38d14d9a94596fc93a107aa99eab275af7118b95febbbc1ba18.
Local postfilter.ps.asm has reflection and disassembly. It reads color, depth,
packed motion, fur/additional motion and neighbor motion; cbuffers CBScreen,
CBViewProjection, CBMotionBlurReconstruction. Runtime-dependent color-copy and
blur branches exist. Live constant values/branch execution were NOT captured;
reflection defaults are NOT live values. Do not claim MotionBlur Off is a copy.

Both native vertex layouts now observed, stable and reviewer-validated:
final triangle count3/stride16, half-allocation UVs; filter quad count4/stride16,
UV0..1 with context1280x720. CPU address formula arena+offset+first*stride holds.
Post-filter output resource1966376560 differs from final source1416345904;
input/TAA copied resource1966366768. Owners1957307104/4969952272/5368212416.
The intervening final-source transfer remains unresolved. The assigned endpoint
alone is NOT proof of a GPU copy. CPU construction verified, downstream GPU
execution/presentation not independently traced.

Next: full-size output integration must account for the filter's color versus
low-res depth/motion coordinates AND final source transfer/UVs. Native temporal
inputs remain our route. No new SDK evaluation/output replacement this run.
Do not expand old copy rectangles over unwritten pixels or drop a filter based
only on its option setting. Continue DLSS -> Reflex -> FG/MFG; beta polish and
CMFM menus parked. No original game assets/settings writes.

All9 raw/9 converted inputs saved;19353600 exact float comparisons, GPU
completion/recording retirement passed. One jitter write/restoration,3matches,
11projection waits,45depth startup waits. Middle native jitter[.34375,
-.48765432834625244],derived SDK shift[.0859375,.12191358208656311]. Allocation
projection basis2560x1440; active1280x720. All color states locally proved.
483scaleoverrides restored;25hooksdisabled,zerofault/pending. Pinsreleased
24input,3consumer,36depth,327submission,6final-copy. Preview visually inspected.

Root local/scaled-temporal-20260928/v1.23.9/. DLL SHA
c6b22619dc59a4ae78f06608e31e3b3b6b279c191dcfda6664d5641d9d8380d1.
Final log SHA d7b33bff22b5377ea00b192c27b58d92519a4aea2bfc2cf1fa51bd2c0e8ed2b2.
Sealed588-file archive SHA
be209cb3150617a5ce66ba8900fdf0a969d940ca921658e8baadbed0a6145fc7.
Do not edit sealed artifacts or source-1.23.9. close_run.py verified all closure
conditions and emitted postfilter-analysis/capture-analysis/run-result files.

Previous preparations and accepted milestones below are historical.

# Current state and next decision

READY, DEPLOYED, UNARMED 2026-09-28: **v1.23.9 post-filter construction capture**.
Game closed; MO2 open (PID 3800). No capture or question pending.
Installed DLL SHA-256:
`c6b22619dc59a4ae78f06608e31e3b3b6b279c191dcfda6664d5641d9d8380d1`.
Root `local/scaled-temporal-20260928/v1.23.9/`; 550-file prelaunch manifest SHA
`d7bf103b0dfaa436cf6b395cda9b78d5057461c53ea40ba791948ec25fbf680b`.
463 frozen sources, 41 byte gates. Exact v1.23.8 DLL/output sidecar preserved;
graphics/profile/modlist/mode unchanged. Keep the latest DLL installed.

Next user step: launch the same MO2 entry, load a save, keep the camera still.
Native AA/DLSS Off, MotionBlur Off, DX12 On, windowed 2560x1440, bare/offline.
Verify fresh PID/creation/header/DLL hash; use frozen source-1.23.9/tools/
arm_scaled_temporal.py. The three-frame half-scale/jitter capture now adds one
read-only Execute hook (25 total). No SDK evaluation/output replacement yet.
[Design, ABI, ownership and limits](scaled-postfilter-observer.md).

The initial plan was full-output integration. Static inspection found a
blocking assumption: MotionBlur=Off does not itself prove the registered
order-100 shader is a color copy. Its Execute still has tile/neighbor work and
MotionBlurStandard/Weak draw paths. Also differing engine owners may reference
the same GPU resource. The next capture records the executed pass's exact
shader, input/output/final-source engine resource addresses, selected queue
commands, property words, and final/filter vertex geometry before committing
full output routing. Do not call the Off path a confirmed pass-through.

Current source: src/scaled_temporal/postfilter.{hpp,cpp}, real assembly entry
in handoff_entry.asm; downstream selects it from the same TAA frame. Completion
waits for all three filter returns. It forwards every native call and SEH;
no native geometry/resources are changed. Capture shader saved as
inputs-PID-CREATION.postfilter-{0,1,2}.ps.dxbc. Record lives inside
capture.final_handoff.downstream.postfilter. CPU construction evidence only,
not GPU execution/presentation. Final packet vertex decoding is statically
based on the full 445-byte native producer, needs this live confirmation.

Validation: warning-clean final build; 132 native checks passed, then all 77
affected checks passed again after adding explicit post-filter completion.
34 Python checks (one inapplicable skip), two retained WARP combined captures
with clean GPU validation, new header budget/tampering tests and accepted
v1.23.8 re-review. Initial build failure was a missing test-only <sstream>
include, corrected; first/final build logs retained. No live result claimed.

After capture: inspect the post-filter PS and actual geometry/ownership;
finish full-size DLSS output routing with a valid full-output failure path.
The reduced-rectangle native copy alone remains insufficient. Preserve native
temporal inputs and post-processing. DLSS, then Reflex and FG/MFG; beta polish
and CMFM menu work remain parked. Latest DLL stays installed per user request.

Previous preparations and accepted milestones below are historical.

# Current state and next decision

ACCEPTED, CLOSED AND ARCHIVED 2026-09-28: **v1.23.8 downstream scaling capture**.
PID 43940 / creation 134350258564033909, frames 15458-15460. Frozen reviewer
accepted all three. User confirmed normal scene and normal exit; process exit,
unchanged settings/profile/modlist/mode, installed DLL, backup, sources and
prelaunch manifest verified. Keep current DLL installed.
[Accepted evidence, ownership chain and limits](scaled-downstream-result.md).

Confirmed final draw: source rectangle1280x720 inside allocation2560x1440,
destination viewport2560x1440, source view+420, destination view+460, UV offsets
approximately minus half an allocation texel. One opcode0 command at stage16
(key2147483648), recorded before TAA on CPU but sorted later. Same-frame filter
queue: stage15/orders0,2(TAA),100,134217713(final endpoint). The order100 unit
(vtableRVA0x3537a70, ExecuteRVA0x24ca1f0) consumes TAA's copied output and writes
the intermediate used by the final endpoint. Helpers identify a motion-blur
reconstruction path by inference. CPU ownership/construction verified; later
GPU filter/draw execution and presentation are not independently traced.

Next: integrate full-size DLSS output through the order100 filter AND final
System_Copy geometry. Expanding only TAA's1280x720 copy is insufficient. Reuse
the native temporal route. Live NGX/full GPU handoff, scaled motion convention,
independent raster jitter and continuous history remain integration checks.
Continue DLSS, then Reflex and FG/MFG; DLAA beta polish stays parked.

All9 raw/9 converted images saved; GPU completion/retirement passed;19,353,600
float comparisons exact. Color states all used local transitions. Projection
viewport2560x1440; one jitter[0.390625,-0.3395061492919922] restored, mapped SDK
render-pixel jitter[0.09765625,0.08487653732299805]. Three frame matches;
9 projection waits and33 depth startup waits.408 scale overrides then full
restoration; zero faults/pending, all24 hooks disabled. Pins released24 input,
3 consumer,42 depth,297 submission,6 final-copy. New observer mutations0.
Readbacks retained until verified process exit. Preview inspected.

Root local/scaled-temporal-20260928/v1.23.8/. Installed DLL SHA
78daa3778b39f1de9dde8eb988c3ed22d8203f85872c318ec89c742807a18356.
Final log SHA988d331b5c3d11b4b9f42b1bd7e86e3c3c4234e79453623e8d10b880a39b1345.
Verified568-file archive SHA
e79d12c1cc5506f992263f1478c3a3600c853a87b40b29949329a4733df5de55.
Game closed; no pending question/capture/tool session; no next build prepared.
Offline exports remain in local/scaled-handoff-20260928/. Earlier attempt to
read MSVC RTTI at vtable-8 failed: those preceding bytes are not a valid COL;
no RTTI identity was claimed. Execute targets were read from vtable+0x60.

Previous preparations and accepted milestones below are historical.

READY, DEPLOYED, UNARMED 2026-09-28: **v1.23.8 downstream scaling observer**.
Game closed; MO2 open (PID 3800). Installed DLL SHA-256:
`78daa3778b39f1de9dde8eb988c3ed22d8203f85872c318ec89c742807a18356`.
Root `local/scaled-temporal-20260928/v1.23.8/`. Verified 534-file prelaunch
manifest SHA `7e65c194da52f47e69ab390fe4106d8ca2c3777db7106a85bface16068328a82`.
Exact previous v1.23.7 DLL and output sidecar preserved. Graphics/profile/
modlist/mode unchanged: native AA/DLSS Off, DX12 On, windowed 2560x1440,
bare/offline profile. Keep the latest DLL installed after testing.

Next user step: launch the same MO2 entry, load a save and keep the camera still.
Verify fresh PID/creation/header/hash, then use the frozen
`source-1.23.8/tools/arm_scaled_temporal.py`. No capture or question pending.
The existing three-frame inputs/final-copy capture and one restored jitter
sample gain one read-only hook (24 total). No NGX/output replacement yet.
[Design, ABI, gates and limits](scaled-downstream-observer.md).

Offline finding: the final scaling draw is recorded before TAA on the CPU,
but tagged stage 16; TAA is stage 15/order 2. CPU recording order does not
establish GPU execution order. Capture the actual bound source, rectangle,
allocation/UV offset, destination viewport/owner, command sort keys and at most
16 post-filter entries for the same three TAA frames. This identifies the
full-size output route. Do not expand the old copy over unwritten pixels.

Validation: warning-clean build, 117 native checks, 29 Python checks (one
inapplicable skip), new header and five altered header budgets checked;
two retained combined WARP input/final-copy captures with clean GPU validation;
v1.23.7 live capture re-review accepted. New standalone tests use the actual
six-argument ASM entry, covering forwarding, SEH and changed frame/thread/
owner/packet/viewport/bounds rejection. They do not simulate GPU downstream
execution. The raw fixture caller is normalized only in the production
code-location reviewer test; an initial rejection and corrected passing log
are both retained. 39 byte gates verified, 456 source files frozen.

After capture: connect full-size reconstructed output through the final copy
and downstream consumers. Live GPU handoff, scaled motion convention,
independent raster jitter and continuous history remain integration checks.
Continue DLSS, then Reflex and FG/MFG; DLAA beta polish remains parked.

Previous completed milestones and preparations below are historical.

# Current state and next decision

ACCEPTED, CLOSED AND ARCHIVED 2026-09-28: **v1.23.7 native final-copy capture**.
PID16580 / creation134350241839399603; frames14906–14908. The helper appended
one opcode13 copy per frame from each selected TAA ring output into a stable
2560x1440 destination, but the actual copied rectangle was **1280x720** at
origin. Both source/destination allocations are2560x1440,R11G11B10_FLOAT(26).
Exact TAA/frame/resource/queue associations passed. Six observer pins released;
zero observer mutations. [Accepted evidence and limits](scaled-final-handoff-result.md).

All nine raw/nine converted inputs saved; GPU completion/recording retirement
and19,353,600 exact active float comparisons passed. Three projection matches,
one jitter write/restoration, viewport2560x1440; same SDK jitter mapping as the
previous run. Native-current-use color proof accepted on the first frame.
Eleven unowned projections and33 pre-baseline depth requests skipped. Full scale
restored after436 overrides; all fault/pending zero, hooks disabled and pins
released. User confirmed normal scene and normal exit. Process exit, unchanged
settings/profile/modlist/mode and installed DLL verified. Keep DLL installed.

Installed v1.23.7 SHA
`e0174e7e258daba2d9ddf4c67ddb6379f654dba56a72bf17be6fd49b5cab7c41`.
Root `local/scaled-temporal-20260928/v1.23.7/`; final log SHA
`85d7c21c04266495f558d5dcd56736941ddfc1bb47e1136bdfe09665711f2ab4`.
Verified555-file archive SHA
`d29d692a0397a3104789d2f95f572758284f3485b8db2e5c01fd9e5e0c7a7970`.
No capture/question/tool session pending; game closed; no next build prepared.

Next: connect full-size reconstructed output through the now-observed native
final-copy route, accounting for packet extent AND downstream spatial scaling.
CPU copy construction is verified; final-copy GPU consumption/full-output
handoff/live NGX are not. Do not simply expand the copy over unwritten pixels.
Scaled motion convention, independent raster jitter and continuous history
remain integration checks. Continue DLSS; Reflex then FG/MFG; beta polish parked.

Previous preparation and earlier milestones below are historical.

READY, DEPLOYED, UNARMED 2026-09-28: **v1.23.7 native final-copy observer**.
DLL SHA `e0174e7e258daba2d9ddf4c67ddb6379f654dba56a72bf17be6fd49b5cab7c41`.
Installed 2026-09-28T02:08:45.4454051+03:00. Game closed at deployment; exact previous
v1.23.6 DLL and output sidecar backed up. Graphics/profile/modlist/mode unchanged:
native AA/DLSS Off, DX12 On, windowed2560x1440, bare/offline MO2 entry. Keep the
latest DLL installed after the test, as requested. No live capture armed.

Next user step: launch the same MO2 entry, load a save and keep camera still.
Verify exact fresh PID/creation/header/hash, then use the frozen
`source-1.23.7/tools/arm_scaled_temporal.py` and reviewer in
`local/scaled-temporal-20260928/v1.23.7/`. Three inputs and the middle jitter
sample remain unchanged; one new read-only hook observes the native TAA final
copy helper at RVA0x23917a0. It records source/destination COM identities and
actual native copy rectangle/packet for exactly the three selected TAA frames.
[Design, ABI, identity gates and limits](scaled-final-handoff-observer.md).
No live NGX evaluation or output replacement. This qualifies CPU copy
construction; GPU final-copy consumption and full-output handoff remain open.

Validation: warning-clean native build, 98 native checks, 25 Python checks
(one inapplicable skip), two retained combined WARP captures with clean GPU
validation, and accepted v1.23.6 live capture re-review. Fixtures caught a
16-byte rectangle read exceeding the safe scalar reader limit; corrected to
four bounded scalar reads before deployment. 36 native byte gates rehashed;
448 source files frozen. Verified 522-file prelaunch manifest SHA
`01401aff7b187f01a97f0a5d1738e79b3b9988def107d5ffbc7db3085b8c5abd`.

All current-turn preparation work is complete. Await the loaded save; no
question or tool session remains pending. The successful v1.23.6 milestone
and older historical preparations follow.

ACCEPTED, CLOSED AND ARCHIVED 2026-09-28: **v1.23.6 scaled projection capture**,
PID 48312 / creation 134350225380327709. Frames 13986–13988: active1280x720 inside
2560x1440 allocations; measured projection viewport remained2560x1440. One
native jitter `[0.34375,-0.48765432834625244]`, derived render-pixel SDK jitter
`[0.0859375,0.12191358208656311]`, one restoration. Exact frame association and
matrix checks passed. [Accepted evidence and limits](scaled-projection-accepted.md).

All nine raw and nine converted images saved; GPU completion/recording retirement
and 19,353,600 exact active float comparisons passed. Native-current-use color
proof exercised and accepted on frame13986; later two used local transitions.
Seven unowned projections and 21 pre-baseline depth requests skipped safely.
All faults/pending zero, pins released, hooks disabled, full scale restored after
356 overrides. User confirmed normal scene and normal exit; process exit and
unchanged settings/profile/mode verified. Successful readbacks ended on exit.

Installed DLL remains v1.23.6 SHA
`6d69db8c7a79e7f54c2dc87de842c56a0ff514cb6c4550fdbc4269a828a66156`.
Keep it installed. No next build prepared; no capture/question pending. Root
`local/scaled-temporal-20260928/v1.23.6/`; final log SHA
`740ed4b56363350a43f8fe1b44c30f64343022b1b7f5a8fc0ae01ada25c3243d`,
541-file archive SHA
`c70cbe40938f74e09f4818808073b30b7cfdc6d7822455a49510b5bcb083a671`.

Next: qualify the live native TAA final copy's destination/rectangle and connect
full-size reconstructed output. Existing map in `scaled-projection-capture.md`
and `local/scaled-handoff-20260927/` shows an active-rectangle copy is possible.
No live NGX evaluation or output replacement yet. Scaled projection basis is
confirmed, but independent color-raster jitter fitting, scaled camera-motion
convention and continuous history/recovery remain integration checks. Continue
DLSS implementation; DLAA beta polish remains parked, then Reflex and FG/MFG.

The preparations and completed evidence below are historical.

READY, DEPLOYED, UNARMED 2026-09-28: **v1.23.6 depth startup gate**.
DLL SHA `6d69db8c7a79e7f54c2dc87de842c56a0ff514cb6c4550fdbc4269a828a66156`.
Installed 01:41:00 +03:00, exact prior v1.23.5 DLL and output sidecar backed up.
Game closed; native AA/DLSS Off, windowed2560x1440; profile/mod list/mode and
graphics hashes unchanged. Keep this DLL installed after the test.

v1.23.5 PID 42532 / creation 134350218727074430 confirmed the ownership fix:
20 unowned projections skipped, valid owned baseline frame 20595, projection
fault zero. Capture then rejected with observer fault 15 / depth fault 3:
64-request depth buffer exhausted by startup traffic, 63 consumers completed,
zero paired inputs and zero jitter writes. Full scale restored after 544
overrides; all hooks disabled/pending zero, 189 depth and 382 journal pins
released. User confirmed normal scene/normal exit; process exit/settings and
installed DLL verified before sealing. Final log SHA
`a7425851d74ffb4be2600e5498062f69df602ea03ed7d59f5afe3801006d5fb6`;
515-file archive SHA
`40c5a97363ed2dca41517a8a341f4b0a142581fc5ccba28136de0cacc40b346b`.

v1.23.6 starts depth recording only at the exact selected baseline projection
stamp. Once opened, every producer remains tracked, preserving packet reuse
checks. Existing capacities/deadline/restoration unchanged; no additional hook.
[Evidence and correction](scaled-depth-startup-result.md). Warning-clean build,
75 native tests, five projection review checks and ten combined review checks
(one inapplicable skip) passed. Two retained WARP cases each reject 96 startup
requests plus six wrong-stamp requests, then complete the three-frame capture.
Prior v1.23.3 live capture re-review passed; all 33 byte gates rehashed.

Evidence/deployment: `local/scaled-temporal-20260928/v1.23.6/`, 438 source files
frozen. Verified 509-file prelaunch manifest SHA
`826cf1bd5df7e2910014e29e7ece4991cf964ddccfd3062a4171404e0c3a928a`.
Use `source-1.23.6/tools/arm_scaled_temporal.py` and its reviewer. Await fresh
loaded save in the same bare/offline MO2 entry, camera still; verify exact new
PID/creation/header/hash before arming. No capture/question pending. Still no
live NGX evaluation or full-output handoff; scaled jitter remains unverified.

The preparations and completed evidence below are historical.

READY, DEPLOYED, UNARMED 2026-09-28: **v1.23.5 activation-scoped scaled
projection capture**. DLL SHA
`d4fc891e90a62afef260ebd821e322fb8d841d773d618713e4afffe398c1ede1`.
Installed 01:29:38 +03:00; game closed, previous DLL/sidecar preserved exactly,
graphics/profile/mod list/mode unchanged. Keep this DLL installed afterward.

v1.23.4 live run PID 17220 / creation 134350210941800810 rejected before any
jitter write: one unchanged projection, zero paired TAA inputs, projection
fault 3 / observer fault 14. Full scale restored, all hooks disabled/pending
zero/pins released. User confirmed normal scene and normal exit; exit and
settings verified. Sealed 517-file archive SHA
`d9264f9b2edf5c7fd6802d731b895ff1eef026140c0dd1bd49b712f69dea189c`.
The observed baseline viewport was 2560x1440; no jittered scaled frame was
captured, so the scaled jitter contract is still not verified live.

The projection hook could admit a baseline before queued AA activation entered
its owned frame. v1.23.5 requires existing same-thread/context/view activation
ownership, waits only before the baseline, and rejects loss mid-sequence.
No additional hook or native setting write. The reviewer checks versioned
ownership receipts. [Finding, correction and tests](scaled-projection-startup-result.md).

Warning-clean build, 52 native tests passed; five projection review checks and
ten combined review checks (one inapplicable skip), two retained real WARP
captures with simulated pre-activation frames, and prior v1.23.3 live re-review
passed. 33 code gates rehashed, 437 source files frozen. Prelaunch 508-file
manifest SHA `eb231d3bf25e7b103227c55034b9f942230f80fe632f3429155f18f4d7c7f1d3`.
Evidence/deployment root: `local/scaled-temporal-20260928/v1.23.5/`.
Use frozen `source-1.23.5/tools/arm_scaled_temporal.py` and reviewer.

Next: same bare/offline MO2 entry, native AA/DLSS Off, windowed2560x1440;
load save, keep camera still, verify fresh PID/creation/startup before arming.
Three-frame capture, one scoped jitter write, no SDK evaluation or displayed
DLSS output yet. Awaiting next loaded save; no active capture/question.

The preparations and completed evidence below are historical.

READY, DEPLOYED, UNARMED 2026-09-28: **v1.23.4 scaled projection capture**.
DLL SHA `97c2661871ae3e46ff3edf6d7c6192f5e68df398a4939d24cf58e5345c2fd64a`.
Build /W4 /WX, 48 native tests, 65 Python checks (10 inapplicable skips), two
retained combined projection/depth/input WARP cases and prior live-capture
re-review passed. All 33 explicit native gates rehashed; 436 source files frozen.
[Contract and next test](scaled-projection-capture.md).

The new probe adds the established scoped projection hook to the half-scale
input recorder: three frames, one middle-frame jitter sample, restoration,
exact TAA/scale-frame association. 22 hooks, no NGX evaluation. Pure jitter
mapping distinguishes render and allocation projection bases; the next live
capture must identify which actually applies. Static tracing also found TAA's
final handoff can queue an active-rectangle copy; full DLSS output routing is
still future work. No output-copy mutation was added.

User confirmed closure; process absence verified. Installed at
2026-09-28T01:16:58+03:00 after the earlier in-use rejection (which made no
changes). Installed DLL hash matches the prepared v1.23.4 build. Exact prior
v1.23.3 DLL SHA
`b27776f260b750f952432d656832e55ee07be36f513b956e5266a2fbb3651ae2`
and output sidecar preserved and verified. New sidecar targets v1.23.4.
Graphics, profile, mod list and mode hashes remain unchanged. Receipt:
`local/scaled-temporal-20260927/v1.23.4/deployment.json`.
All 502 prepared artifacts reverified; separate 510-file prelaunch manifest SHA
`6e1ea78fcea466f8517b6f1e0ebd006f74e991dc617f086193ea52e709845179`.
Game closed, MO2 open (PID 3800), no capture armed. Keep this DLL installed
afterward. Await a fresh launch and loaded save; verify the new process before
arming. Live scaled projection basis remains unverified.

Evidence/preparation: `local/scaled-temporal-20260927/v1.23.4/`. Frozen arm/review
tools under `source-1.23.4/tools`. Use the same bare offline entry,
native AA/DLSS Off, windowed2560x1440, load a save and keep camera still. Verify
the new PID/creation/startup before arming. Output-copy static exports are in
`local/scaled-handoff-20260927/`. No original game assets/settings changed.

The preceding offline SDK result and live captures below are historical.

ACCEPTED OFFSCREEN 2026-09-27: **first modern DLSS Performance outputs from
the saved scaled game inputs**. Three 1280x720 inputs reconstructed to 2560x1440.
The SDK now separates allocation/render/output extents, crops conversion to
the active rectangle, converts motion in render pixels and validates the SDK's
recommended input size. Existing equal-size DLAA remains supported.

All three GPU replays passed, with complete finite outputs, exact packing and
restored downstream bindings. Poisoning all inactive input pixels produced
bit-identical output. Five native targets and four DLAA GPU regression cases
(including 600 frames/60 output allocations) passed. Eight hardware cases total,
zero GPU warnings/errors. [Implementation, evidence and limits](first-scaled-sdk-output.md).

Next: matching scaled projection/jitter, continuous scaled-depth ownership and
the game's full-output consumption/recovery contract. No live upscaling yet;
these reset-only saved-frame replays do not prove temporal quality. Motion's
render-size conversion has animated-object reprojection support; global camera
motion and scaled projection units remain to be bound live.

Game closed, MO2 open, no capture/question pending. Keep installed v1.23.3 SHA
`b27776f260b750f952432d656832e55ee07be36f513b956e5266a2fbb3651ae2`;
no deployment/settings changes. It is still an input recorder, not the new
upscale path. Evidence `local/scaled-input-contract-20260927/`; no next live
build prepared. DLAA polish remains parked; then DLSS, Reflex, FG/MFG.

The preceding live capture and preparations below are historical.

ACCEPTED, CLOSED AND ARCHIVED 2026-09-27: **v1.23.3 first complete half-scale
temporal input capture**, PID 46960 / creation 134350141126140646. Three frames
15673–15675, active 1280x720 within 2560x1440 allocations, dispatch 80x45x1.
All nine raw and nine converted images saved; GPU completion/retirement and
19,353,600 exact active-region conversion values verified. Normal restored
scene/exit confirmed by user; process exit and unchanged settings verified.

Color used explicit local transitions on all three frames. The supplementary
native-current-use proof was not attempted; its live branch remains unobserved.
Do not claim this run demonstrated the missing-transition fallback. Depth's
pixel-only final writer path passed, selected receipts 2/5/8. All faults/pending
zero, observer pins released, hooks disabled, activation removed and full scale
restored after 442 overrides. Readback resources were retained by policy until
the verified process exit. No jitter injection or NGX evaluation.

Images qualitatively agree across color/depth/motion. Scaled motion units and
jitter still need establishing, using the captured shader and existing findings,
before adapting SDK dimensions and full-size output handoff. DLAA beta polish
remains parked. [Result and next implementation](scaled-temporal-accepted.md).

Keep current installed DLL SHA
`b27776f260b750f952432d656832e55ee07be36f513b956e5266a2fbb3651ae2`.
Root `local/scaled-temporal-20260927/v1.23.3/`: 478-file final archive SHA
`af733ab76b8ec26501f06ce9ee869737012211c76a606e20de7c6ae9e4cfa3f7`;
final log SHA `bccc863dbb4cf641f6e23ac487c3c969d7603b688bb8c0cb1f52b2dfe45042af`.
No active process/capture, pending question or next build prepared.

The preparation and prior results below are historical.

READY, DEPLOYED AND UNARMED 2026-09-27: **v1.23.3 scaled temporal capture
with native current-use color proof**. Game closed at installation; MO2 remains
open (PID 3800). No active capture or pending question. Launch the same bare
MO2 entry, native AA/DLSS Off, windowed 2560x1440. Load a save and stay still;
agent verifies the new process generation/startup and arms afterward.

The older recorder now reuses the unchanged continuous-DLAA NativeStateAdmission
helper when color has no local transition. Missing/wrong/conflicting proof still
rejects. Raw local state evidence remains unchanged; the separate receipt and
offline replay bind the proof to this exact frame, packet, owner and recording.
Depth draw qualification, GPU completion/retirement and full-scale restoration
remain mandatory. Same 21 hooks, three frames, no NGX evaluation or injected
jitter. [Contract, tests and limits](scaled-color-state-admission.md).

Build /W4 /WX, 28 native cases and 94 Python cases passed; 13 inapplicable cases
skipped. Six retained WARP cases include earlier color transitions/current output
UAV proof, exact GPU pixels/conversions, and rejection of stale/aliased metadata.
All 32 native code gates rehashed; continuous DLAA helpers/runtime unchanged.
Live acceptance is pending; no scaled motion or DLSS readiness claim.

Root `local/scaled-temporal-20260927/v1.23.3/`: RUNBOOK.md, preparation,
deployment, 259 frozen source/tool files, retained GPU fixtures/checks, exact
previous DLL/sidecar backups, and 447-file prelaunch seal. Installed DLL SHA
`b27776f260b750f952432d656832e55ee07be36f513b956e5266a2fbb3651ae2`;
prelaunch SHA `c0c0766dc80db1a8240478f69dcdf20e612585d0450a759ee3061bf5e8a00c0e`.
Use frozen `source-1.23.3/tools/arm_scaled_temporal.py` and review tool. Verify
exact PID/creation/header; never arm a stopped process. Keep DLL installed
after the test. Explicit rollback: `deploy.ps1 -Action Restore`, game closed.
Saved settings/profile/mode verified unchanged. No game assets were modified.

The v1.23.2 result and preparation below are historical.

PARTIAL CAPTURE, CLOSED AND ARCHIVED 2026-09-27: **v1.23.2**, PID 4296 /
creation 134350126378930622. User confirmed normal scene and normal exit;
process exit, unchanged settings/profile/mode and installed DLL verified.
No active capture or pending question. Keep the DLL installed. No replacement
build is prepared after this run.

The pixel-only depth-state correction passed live: nine depth draws returned
with stable snapshots and zero depth-route faults. Frame 15007 selected the
last matching writer (receipt 8, source 0x80, firstVertex=3). One depth readback
and owned-depth copy were recorded, with native RENDER_TARGET state restored.
Depth and TAA share the exact command-list/reset/allocator generation.

The next gate rejects missing local color-state evidence: native consumer
fault 16 -> submission 17 -> capture 13. Color has no transition in this TAA
callback, output has only a UAV barrier, and motion has the expected 0xc0
transition. One valid CPU input record and one incomplete record were retained.
No color/motion copy, private signal, GPU-completion proof, images or NGX run.
The issued depth readback remains explicitly quarantined until process exit;
do not report it as completed, saved, or freed at hook shutdown.

All hook groups disabled/pending zero, observer pins released (producer 16/16,
depth 27/27, consumer 1/1, submission 231/231). Full scale restored after 411
overrides. Activation patch removal and callback drain both pass. Normal
process exit closes the remaining process-lifetime GPU allocation ownership.
[Evidence, limits and next implementation](scaled-color-state-result.md).

Next: adapt the continuous-DLAA native current-use proof to this older capture
recorder when the local color transition is absent. Existing
NativeStateAdmission/StateTrace already handle this missing-color/UAV-only-
output pattern in v1.21.3+. This run did not capture those native planner
receipts, so do not assume its missing color state is automatically 0x40.
Keep bounded callback-owned metadata reads/rereads, all malformed-state
rejections, exact resource/generation checks and versioned offline evidence.
No new shader or scale-policy change is indicated by this trace.

Root `local/scaled-temporal-20260927/v1.23.2/`: final frozen log/review,
state-analysis.json, run-result.json and 396-file archive. Final archive SHA
`756cc797eac33c0f0671d7af92ba7b5782a4506b52d93a4503955cb668b47e61`;
log SHA `46121af44a9e3451a19467e8439810270924c4748228a2a886389eabead1f316`.
Installed SHA remains
`716c862950d1e0a76e2bf2066a03652324dbd0ed2b839052793b6a34117b2b35`.

The preparation below is historical and remains archived unchanged.

READY, DEPLOYED AND UNARMED 2026-09-27: **v1.23.2 scaled temporal-input
capture with pixel-only depth-source support**. Game closed at deployment;
MO2 remains open (PID 3800). No active capture or pending question. User can
launch the same bare MO2 entry, keep native AA/DLSS Off and windowed 2560x1440,
load a save and stay still. Agent verifies the new PID/creation/startup and arms.

The depth draw accepts exactly source state 0x80 or 0xc0, with destination
still exactly RENDER_TARGET. Resource, packet/argument, frame/view, ownership,
barrier and GPU completion checks remain intact. No hook or native address was
added: 21 hooks, three frames, no injected jitter, no NGX evaluation. Full
native scale restores automatically. Continuous DLAA source is unchanged.
[Correction, tests and limits](scaled-depth-pixel-state.md).

Build /W4 /WX and 24 native checks passed. Four retained WARP cases exercise
combined-state and pixel-only sources, single/multiple writers, and the observed
firstVertex=3 on the final writer. GPU validation and exact active-region pixel/
conversion checks pass. 74 Python cases pass; nine historical jitter cases skip.
Both depth-route and depth-pixel reviews share the versioned state predicate;
v1.23.1 remains strict. Invalid states, malformed barriers and rehashed active
pixel corruption reject. Original full-size copy regression passes. All 27
pinned native code spans are rehashed. Live capture is still required.

Evidence root `local/scaled-temporal-20260927/v1.23.2/`: RUNBOOK.md,
source-1.23.2 (250 frozen sources/tools/dependencies), preparation/deployment,
fixture images/logs, previous exact v1.23.1 DLL and output sidecar. Keep this
DLL installed after testing; deploy.ps1 rollback is explicit only. Graphics,
profile and mode files are unchanged. Installed SHA
`716c862950d1e0a76e2bf2066a03652324dbd0ed2b839052793b6a34117b2b35`.
388-file prelaunch manifest SHA
`1cbf5a3a84dae26f24a5a18e602d16950352472421f4783fdb4246957184cf1b`.

Use frozen arm_scaled_temporal.py against inputs-PID-CREATION.jsonl;
Local\MhwTemporalInputs-v1, 30-minute startup wait, no hooks before arm.
After automatic stop, freeze the log beside image files and run frozen
review_scaled_temporal.py with the installed hash and a new output receipt.
On failure, preserve evidence and do not rearm the process. No new DLSS claim
until GPU/image checks and scaled motion/output integration are established.

The v1.23.1 result below is historical and remains archived unchanged.

PARTIAL CAPTURE, CLOSED AND ARCHIVED 2026-09-27: **v1.23.1**, PID 13188 /
creation 134350115518128752. User confirmed normal scene and normal exit;
process exit, unchanged graphics/profile/mode and installed DLL identity verified.
No active capture or pending question. Keep this DLL installed; do not rearm
this process. No replacement build has been prepared after this run.

The new hook observes all three reduced-region depth draws. Their packet,
rectangle, arguments and pipeline snapshots remain stable and all return
normally. First two establish source state 192; the final writer establishes
128 (PIXEL_SHADER_RESOURCE), with destination state 4 in all three. The strict
source==192 check rejects that final writer: depth fault 17 -> submission 18 ->
consumer 15 -> capture 13. That writer and TAA use the same command-list pointer;
allocator/reset/fence completion is not yet verified because capture stops first.
One valid CPU input record, frame 60616, has active rectangle 1280x720 inside
2560x1440 allocations. No diagnostic image copies or private signals attempted;
no image/shader saved, no NGX evaluation.

Full native scale restored after 437 overrides. All hook groups disabled,
pending zero; observer references released (producer 8/8, depth 9/9, consumer
1/1, submission 256/256). Both activation_unhooked and activation_disabled are
true, resolving the earlier contradictory cleanup report for this run.
[Observed cause and next correction](scaled-depth-draw-result.md).

Next: accept the two observed pixel-readable source states (0x80 or 0xc0)
for this depth draw, retain exact RENDER_TARGET destination and all barrier,
ownership and GPU-generation checks. Add real pixel-only WARP draw coverage,
including startVertex=3 and three writers, plus negative state checks; update
the offline reviewer consistently. Then prepare the next version and repeat
the three-frame capture. Motion scaling and DLSS evaluation remain pending.

Evidence: `local/scaled-temporal-20260927/v1.23.1/`, frozen final log/review,
draw-state-analysis.json, run-result.json and a 362-file final archive.
Final archive SHA
`1281dd4458f618c18098a96564433c80717f45ab94a70044d7475e603f4177d5`;
final log SHA
`8719b1f9d718a7e35b3022da7955e54efc99e02f0c14acb71830f1d37f6b5dc5`.
Installed DLL SHA remains
`94ad8eaa7adacbf7e0d92e87f4c3e3691da97c38dd42ff2e6fe3dfce0b759624`.

The preparation below is historical; its files remain unchanged.

READY, DEPLOYED AND UNARMED 2026-09-27: **v1.23.1 scaled native temporal
input capture with depth-draw support**. Game closed at deployment; no active
capture or pending question. User should launch the same bare MO2 entry,
native AA/DLSS Off, fixed windowed 2560x1440, load a save and stay still. Agent
checks the new process and arms. No NGX/DLAA evaluation; this captures three
input sets and restores full native scale automatically.

[Draw capture contract and verification](scaled-depth-draw-capture.md).
Added one scoped DrawInstanced hook (21 total), validated against the pinned
native helper/caller, nested packet and arguments, rectangle, cached pipeline,
source/destination identities and actual barriers. Readback restores depth to
RENDER_TARGET for a draw, while the original copy path restores COPY_DEST.
For several depth writes before TAA, select the last matching published
producer; retain exact TAA list/generation and GPU completion requirements.
Offline reviewers now distinguish active rectangles from full-size allocations.
Full images remain archived, but only active pixels claim semantic conversion
verification. Scaled DLSS motion-vector units remain unverified.

Activation cleanup separately records patch removal and waits for callbacks
already redirected through trampolines to return; both must succeed. This
addresses the contradictory cleanup flag in v1.23.0 without relaxing checks.

Root `local/scaled-temporal-20260927/v1.23.1/`: RUNBOOK.md, frozen tools under
source-1.23.1, preparation/deployment receipts, exact prior DLL and sidecar,
explicit deploy.ps1 rollback. Keep the new DLL installed after the test.
Installed DLL SHA
`94ad8eaa7adacbf7e0d92e87f4c3e3691da97c38dd42ff2e6fe3dfce0b759624`.
354-file prelaunch archive SHA
`d0def364067f51c555e732d5f4560ea58c7dafab737576259819ffff6f44e346`;
248 frozen sources/tools/dependencies. Settings/profile/mode remain unchanged.

Build /W4 /WX and 22 native cases passed; 66 Python cases passed across four
suites, 9 historical jitter cases skipped. Actual WARP draws/copies verify
depth in an 8x6 active region of a 17x13 target, unused pixels unchanged,
single and multiple writers, packet/caller/argument rejection, absent GPU
retirement and explicitly injected private-signal failure. Active-region
corruption fails review even with recomputed artifact hashes. Full-size copy
regression passes. 27 pinned native code spans rehashed. Live verification of
the new depth route is pending.

Use frozen arm_scaled_temporal.py, namespace MhwTemporalInputs-v1, against
the exact inputs-PID-CREATION.jsonl; 30-minute startup wait. After its automatic
stop, freeze the final log beside its image files and use frozen
review_scaled_temporal.py. Do not resume parked DLAA polish or claim DLSS
evaluation; the next milestone remains reduced-input/full-output upscaling.

The v1.23.0 entry below is historical; its evidence remains unchanged.

PARTIAL CAPTURE, CLOSED AND ARCHIVED 2026-09-27: **v1.23.0**, PID 46000 /
creation 134350099409052660. User confirmed normal scene and normal exit;
process absence, unchanged settings and installed DLL identity verified.
No active capture, pending question or requested launch. Keep the DLL installed.

One valid frame (12663) establishes temporal rectangle [0,0,1280,720] and
dispatch [80,45,1] inside actual 2560x1440 color/history/motion/output resources,
with matched half-scale stamps. Three native depth requests use that same small
rectangle. The depth consumer returns normally with zero CopyResource calls;
barriers instead put depth destination into RENDER_TARGET. Pinned static code
explains the alternate DrawInstanced path when the rectangle is smaller than
the allocation. The current recorder only accepts CopyResource, so it rejects
the draw and closes the submission journal: depth fault 9 -> submission 18 ->
top-level 13. No diagnostic image copy attempted, no images saved, no DLSS run.

All final hook records report disabled, pending zero and all references released;
full native scale restored after 333 overrides. Read-only live checks matched
all 26 original native code spans and nine scale values at 1. The combined
activation_disabled=false flag disagrees with its later drained/disabled state;
likely a post-unhook callback-drain reporting race, requiring a bounded
post-disable drain check rather than weakened validation. Original failed
review is retained. [Result and next implementation](scaled-temporal-first-result.md).

Next: extend this recorder to observe/qualify the existing native depth draw
route, including nested packet, target, shader/arguments, state restoration
and retirement, then repeat the three-frame scaled-input capture. Preserve the
full-scale copy route. This blocks scaled input verification; it is not the
parked intermittent DLAA depth-copy polish. No corrected build is prepared yet.

316-file final archive SHA
`cf45f2ced41d683608800df6a9ba1661ae5af1c3ce3de946c0f24fd8f020281f`.
Final log SHA
`0e12f111b06692f974c4ea2fd64e5d441d66159fb8fcace145dfc0e7956eddae`.
Evidence root and installed DLL are unchanged from the preparation below.

HISTORICAL PREPARATION 2026-09-27: **v1.23.0 scaled native temporal
input capture**. Game is closed; MO2 remains open (last observed PID 3800).
Next user action: launch the same MO2 entry/profile, native AA/DLSS Off, fixed
2560x1440 window; load a save and stay still. Agent checks startup and arms.
This combines proven 0.5 native scale and native temporal-stage activation with
the existing three-frame input recorder. It does NOT run DLAA/NGX or bypass
native TAA; the latter temporarily supplies its existing bindings. Capture
automatically restores native full scale. Existing continuous DLAA source is
unchanged. [Scope and contracts](scaled-temporal-inputs.md).

Root `local/scaled-temporal-20260927/v1.23.0/`: RUNBOOK.md, frozen arm/review
tools under source-1.23.0, preparation/deployment receipts and explicit rollback.
Installed DLL SHA
`0d3ba09376a6fcd54bb95d5bbc00ea39d41cb2f5c012cf84bfd8b0f48adcdef2`.
306-file prelaunch archive SHA
`0537a2568d800c4fee7dc31607761b263919c1524bbf479ca758f40bbc36827e`;
239 frozen source/tool/dependency files. Build /W4 /WX passed, 13 native tests
passed, 62 Python checks passed (9 inapplicable historical jitter tests skipped),
26 native code gates rehashed, retained WARP image copies/conversions verified.
Initial retained fixture had the wrong filename for the established review
harness; rerunning with canonical inputs-123-456 passed without weakening checks.
No live scaled temporal routing or scaled motion-vector convention is verified.

Use frozen `arm_scaled_temporal.py`, namespace `MhwTemporalInputs-v1`, on the
exact new `inputs-PID-CREATION.jsonl`. Startup waits up to 30 minutes. After arm,
scale waits at most five seconds for 32 applied observations, then capture
admits three frames in its two-second window. Wait for `stopped`, freeze/review
the log with `review_scaled_temporal.py`, and inspect raw active pixel regions.
Full-sized allocations are expected; do not assume the temporal rectangle or
motion-vector units without evidence. Converted motion remains allocation-size
diagnostic arithmetic only. The resolution record's command count describes
the scale component, while the input recorder separately records its GPU work.

Exact previous v1.22.1 DLL and output sidecar are preserved at this root;
deployment and unchanged graphics/profile/mode identities verified. Keep this
DLL installed after the run as requested; rollback is explicit, not automatic.
No pending question or active test. The completed scale-only run below is
historical; its archive and observations remain valid.

PASSED, CLOSED AND ARCHIVED 2026-09-27: standalone **v1.22.1
native render-resolution probe**, PID 43572 / creation 134350067555165293. The user
loaded a save via the existing MO2 entry/profile, AA/DLSS Off and fixed 2560x1440.
Arm tick 15834953; agent stop tick 15929953 (95 seconds). Stop was intentional.
The final frozen JSONL/review confirms healthy, cleaned shutdown. This DLL
does not run DLAA/NGX. It requests native
0.5 active scale (expected 1280x720) while requiring allocation/content scale 1.
FINAL CHECK PASSED: 19,692 overrides, 19,692 applied-half-scale observations,
19,692 native request restorations, zero faults/unqualified visits/foreign
requests, all nine scale values restored to 1, pending zero, hooks disabled.
Actual viewport counts: 77,766 full-size, 230,883 half-size, 725,867 other.
Depth CPU metadata/content scale stayed full-size. User confirmed full-window
scene and normal HUD/text during slow pan, then normal scene after restoration
and normal exit. Process exit and unchanged settings verified; current DLL
left installed. No active process, pending question or requested launch.
140-file final archive SHA
`b1fb2f32a8c6f1f0db0ac94e6bb049d127a33f55d3ea176a3e1dff0fc3e3cd92`.
[Verified result and next step](dlss-render-resolution-result.md).
Final log SHA d6c7ee54f4a3a21927055ae5f6a460589f5cd5b78e8b832cc51b3e5a8541dc81.
The periodic .status.json retains its last active sample; the stopped JSONL is
the authoritative final state.
Full resource descriptions and exact per-scene viewport attribution remain
unverified; do not call this a DLSS evaluation.

Next: use the proven native scale control to establish the reduced color/depth/
motion input region at the native temporal resolve, separate active input and
full-size output dimensions, then connect resolution-changing DLSS. Existing
equal-size DLAA runtime remains unchanged. Do not resume the parked depth-copy
polish work unless it becomes a concrete blocker.

Build `/W4 /WX` passed, native geometry/ownership/ABI/stop/exception contract
passed, six Python tests passed, six native code spans rehashed against pinned
host/capture identities. 99 frozen sources/dependencies, 126-file prelaunch
archive. Native scale and reduced viewports now observed live; exact scene
and output/UI routing still require qualification. Native tests also cover enabled override
width with matching dimensions, and rejection of different width/height/flags.

Root `local/render-resolution-20260927/v1.22.1/`, including RUNBOOK.md,
deployment/preparation receipts, reversible deploy.ps1, and frozen arm/review
tools. New arm/stop namespace is `MhwRenderResolution-v1`, not the DLAA event.
DLL SHA `1c74e921d9cacd443cf205bb5fae61c3c242bfad325ff71870bc91404f2e71db`.
Prelaunch SHA `76e1004f06011daac98c3f3d3d9f0b0ae61486e40fa2fbd3846627a696a65f38`.
Exact v1.22.0 DLL/sidecar preserved at this root; working v1.21.5 DLL/sidecar
remain preserved at the v1.22.0 root. Graphics/profile/mode
hashes unchanged. Keep the new DLL installed after the test as requested;
restoration script is available if wanted, not an automatic post-test action.

v1.22.0 run PID 43364 / creation 134350040609261239 stopped at native sizing
admission before installing hooks or applying scale. Its live code gates passed.
Read-only live checks found config +0x2cda=1, +0x2cd0/+0x2cd4=2560x1440, matching
renderer and depth CPU metadata; all active/content scales were 1, view flags
passed and pointers stayed stable. v1.22.0 unnecessarily required that native
width source to be disabled. v1.22.1 admits it only at exact display dimensions
and records the source/basis in every snapshot. No native code gate, sizing
control point or restore policy changed. User confirmed normal exit; process
absence and unchanged settings verified. No pending question.
v1.22.0 134-file final archive SHA
`0bb455741cc699954e6384ebdc01d1415f54fe4e0fa07a2b4b183653c18aed28`;
final log SHA `5d9d59c77b18624e378aacc530cf9bad5bfec3c232af8c952a6f646bcb77f60f`.

PRIORITY CHANGE 2026-09-27: user explicitly parks intermittent recovered DLAA
skips for beta and moves to **render-resolution control -> DLSS upscaling ->
Reflex -> frame generation/MFG -> integrated beta testing/polish**. Keep diagnostics.
[Revised milestones](dlaa-to-dlss-milestones.md), [beta backlog](beta-backlog.md),
[render-resolution starting map](dlss-render-resolution-control.md).

Do not resume missing-depth-copy investigation as a prerequisite. Reopen it
only for a concrete new-feature blocker, persistent dropout, visible corruption,
crash or unsafe resource handling. The working v1.21.5 baseline is preserved;
its continuous runtime source remains unchanged. New sizing work uses the
separate probe above, with no saved graphics/profile configuration changes.

User follow-up explicitly puts Reflex before MFG. Streamline DLSS-G requires
Streamline Reflex with correctly aligned frame markers; verify pacing/frame
IDs before enabling generation. Dependency source is linked in the milestones.

Initial static sizing map completed against both pinned executable hashes.
Renderer +0x1f0/+0x1f4 is one scale channel; +0x200/+0x204 is another, with their
product at +0x208. Native update/view-constructor/temporal-allocation evidence
is under local/render-resolution-20260927. +0x204 also scales temporal texture
allocations, so do not assume it preserves a full-size resolve target. Next
implementation work must select the native sizing boundary and establish the
lower-input/full-output route. Current SDK/runtime use equal input/output sizes.
The focused native active-scale probe is now prepared/deployed as recorded above.

The next-step statements in historical entries below are superseded by this
priority change; their verified results and artifact identities remain valid.

CLOSED AND ARCHIVED, DLL LEFT INSTALLED 2026-09-27: **v1.21.5 depth destination
correction verified**. PID 44500 closed normally; process absence verified.
No armed run or pending question. [Live result](native-aa-depth-destination-result.md).

6,521 DLAA evaluations in 84.219 seconds, zero runtime/SDK faults, collisions or
actual state rejections. User reported active, clean rendering after an area
load and walking/panning. All ten checkpoint channels and frozen full-log
review passed. All 66 attachment-only mismatches passed the corrected
destination check; actual destination rejections stayed zero.

87 frames still lacked a matching private depth copy, across 30 suspension
periods; every period resumed. Nine retained lease values are all zero, and
the failing predicates are list/generation/frame/availability. This remaining
condition is upstream of consumption; its cause is not yet established.
Counts stayed at 87 for another 1,153 DLAA frames after the area checkpoint.

All 6,522 SDK/depth recordings and 87 unfiltered copies completed and released;
all 7,018 jitter scopes restored, hooks disabled, pending zero. One unclaimed
native resolve appears only in the final checkpoint; no tracked forwarded
requests. Its precise timing/cause is not separately recorded. Do not claim
whole-renderer AA exclusivity (`exclusive_mode=false`).

Root `local/native-aa-depth-destination-20260927/v1.21.5/`; installed DLL SHA
`9031276806f2c175fd52c6c8fdfffaa0a855f411f0df4e05ef8072a425e35f7a`.
406-file final archive SHA
`9f349ef3d86566f7df4ae0a4c9b4bfcdfab892cb9828641d0de563730194de0c`.
334 source/dependency identities and four configurations reverified; prior
v1.21.4 DLL/sidecar preserved. Do not restore or uninstall automatically.

Next: trace the earlier private depth-copy lifecycle for empty leases,
distinguishing unavailable game stages from tracking/order gaps. Keep all
resource/state/frame/list-generation guards. Attachment-comparison correction
passed this run; no additional patch has been prepared. No automatic game
launch/arming. Native AA/DLSS remain Off, Windowed 2560x1440. CMFM/UI, upscaling
and FG remain deferred. No new pixel, resize/focus or soak coverage in this run.

Historical preparation and results below are superseded by this closed result.

PREPARED, INSTALLED AND SEALED 2026-09-27: **v1.21.5 depth destination correction**.
Game CLOSED, MO2 open (last checked PID 3800). No run armed or pending question.
[Contract and runtime plan](native-aa-depth-destination.md).

Dispatch admission now requires readable scene + exact container/full destination
Owner/SRV/CPU-descriptor/SRV-readability identity. Excludes only the four transient
source-attachment diagnostic fields demonstrated in v1.21.4. Original full
SceneDepth comparison remains observed; producer-time full equality, copy
packet/state, pins, list/generation/frame/availability and SDK/history gates
remain unchanged. No extra game reads, hooks, settings writes or queue change.

42 native and 163 Python checks passed (205 total). Twenty native destination
cases exercise each comparison field, combined attachment+texture changes,
unreadable depth, optional source-attachment failure, every lease predicate and
bounded first/latest retention. Nine saved attachment samples replay correctly.
Original runtime source is recovered by reverting the one dispatch conditional
and removing the new trace/include/serialization. New live behavior unverified.

Root `local/native-aa-depth-destination-20260927/v1.21.5/`.
334 frozen sources; installed DLL SHA:
`9031276806f2c175fd52c6c8fdfffaa0a855f411f0df4e05ef8072a425e35f7a`.
387-file prelaunch seal SHA:
`c7f416aa3ac2239cf6da32e256e44ab7a859e74b08089eb3443dcf251aedf22c`.
Prior v1.21.4 DLL/sidecar preserved; 401-file prior archive verified. All four
saved configs unchanged. Do not restore or uninstall automatically.

Next user action: launch same bare MO2 forced-DLL entry and load save. Keep
native AA/DLSS Off, DX12, Windowed 2560x1440. Verify seal/startup/modules/process
identity/configs, arm frozen tool once, then capture baseline with all TEN
channels. depth_eligibility_verified now also requires its returned destination
object/destination_identity_verified. Header/status depth_destination policy is
depth-destination-identity-v1. Old scene.rejected counts full-comparison misses;
actual reason 3 must equal NEW depth_destination.rejected. Do not confuse these.

Inspect attachment_only admissions, SDK growth and lease failures. Admission
alone does not promise Evaluate for that frame. If healthy, request one area
load plus walking/panning, then announce stop, verify drain, request normal exit
and archive without restoration. Missing-copy lifecycle remains unresolved and
must stay protected by existing lease checks. CMFM/UI, DLSS and FG deferred;
exclusive_mode=false. No automatic arming or game launch.

Historical results below are superseded by this installed, unarmed correction.

CLOSED AND ARCHIVED, DLL LEFT INSTALLED 2026-09-27: **v1.21.4 depth eligibility
cause captured**. PID 33872 closed normally; process absence verified. No pending
question or armed run. [Result and next correction](native-aa-depth-eligibility-result.md).

7,841 DLAA evaluations in 99.844 seconds arm-to-intentional-stop, zero runtime
faults, SDK failures, collisions, state rejections or tracked native forwards.
All ten checkpoint channels and frozen full-log review pass. User reported clean
active DLAA after an area load and walking/panning. Four state supplements
accepted. All 323 suspensions resumed by final stop.

312 scene-depth rejections are exclusively changes to attachment, attachment
owner, attachment resource and attachment readability. Every destination Owner,
scene container, SRV/descriptor and SRV-readability difference counter is zero.
Nine retained scene samples show absent-at-producer / present-at-consumer source
attachment, with zero failed reads. This identifies diagnostic attachment churn;
it does not show a changed depth destination.

126 other rejections fail lease list/generation/frame/available checks; all nine
retained lease snapshots are entirely empty. Why the earlier copy is absent is
not yet established. Stage-3 early returns prevent per-frame lease observation
for the 312 attachment rejections; do not promise all become SDK evaluations.

All 8,153 SDK/depth and 438 unfiltered-copy recordings completed/reset/released;
all 8,680 jitter scopes restored, hooks disabled, no live recording references.
Current DLL remains installed; prior DLL/sidecar and all four unchanged configs
verified. Do not restore or uninstall automatically.

Root `local/native-aa-depth-eligibility-20260927/v1.21.4/`.
401-file archive SHA:
`533f43e492b53c96b56a2bd39788fa1673e7ae88035825d27de0e0448999307b`.
Installed DLL SHA:
`1b3e32588459ce8473bb2506d23ff8076a5a9787cf296edde9c33944c12d0d5c`.
Final log SHA:
`5f293fdf0a1921faa172592d99f0325b2734dae0a1026fc11d6fe2de52bc4ad6`.

Next: implement/test an explicit dispatch-time destination identity comparison,
excluding only the four diagnostic attachment fields. Preserve core readability,
container/full Owner/SRV identity, producer-time full equality, actual copy
packet/state checks, resource pins and frame/list/generation lease validation.
Keep missing-copy frames unfiltered; their earlier copy lifecycle remains a
separate investigation. Retain diagnostics to verify accepted attachment-only
changes. No corrected build prepared yet. CMFM/UI, DLSS and FG deferred;
exclusive_mode=false.

Historical preparation and earlier results below are superseded by this closed
run. Preserve their contemporaneous limitations.

PREPARED, INSTALLED AND SEALED 2026-09-27: **v1.21.4 depth eligibility diagnostic**.
Game CLOSED, MO2 open (last checked PID 3800). No run armed or pending question.
[Diagnostic contract and runtime plan](native-aa-depth-eligibility.md).

Adds bounded observations at existing scene-depth and depth-copy lease checks;
all admission decisions, GPU work, native state supplement, ownership, hook set
and saved settings remain unchanged from v1.21.3. Scene reader is called once
through a delegate preserving its read order/results. Full equality still gates
scene depth; exact list/generation/frame/available/not-evaluated still gates the
lease. No hypothesis has been accepted as the cause of the remaining skips.

41 native and 151 Python checks passed. New owned-memory fixture covers 18 cases,
all 21 SceneDepth field mappings, unchanged read counts/results, individual lease
predicates and bounded retention after 1,000 failures. New version requires the
tenth checkpoint channel, depth_eligibility_verified. New live behavior not yet
verified. Runtime source reproduces the prior version after stripping the
explicit observation points; original scene_depth source and policy sources
are byte-identical. Existing /W4 /WX build passes.

Evidence root `local/native-aa-depth-eligibility-20260927/v1.21.4/`.
330 frozen sources; installed DLL SHA:
`1b3e32588459ce8473bb2506d23ff8076a5a9787cf296edde9c33944c12d0d5c`.
382-file prelaunch seal SHA:
`eb3c208f61c827fec042c21e64328c6c177bb24bb12b0cc7a8ce011a27ce9244`.
Prior v1.21.3 DLL/sidecar preserved; prior 394-file archive verified. All four
configurations unchanged: native AA/DLSS Off, DX12, Windowed 2560x1440, bare
profile. Do not restore or uninstall automatically.

Next user action: launch the same bare MO2 forced-DLL entry and load a save.
Then verify seal/startup/modules/process identity/configs and arm the frozen
tool once. Capture baseline with all TEN channels, inspect scene_changed_fields
and lease_failed_predicates, and check actual SDK growth. If healthy, request
one area load followed by walking/panning; preserve failures without changing
the running checks. Announce intentional stop, verify full drain, ask for normal
exit and archive without restoration. No automatic game launch or arming yet.
CMFM/UI, DLSS upscaling and FG remain deferred; exclusive_mode=false.

Historical results below are superseded by this installed, unarmed diagnostic.

CLOSED AND ARCHIVED, DLL LEFT INSTALLED 2026-09-27: **v1.21.3 current-use state
admission passed its bounded live test**. Game PID 2524 closed normally; process
absence verified. No pending user question or armed run.
[Result and remaining depth checks](native-aa-state-admission-result.md).

18,485 successful DLAA evaluations over 245.890 seconds from arm to intentional
stop; zero runtime faults, SDK failure, request collisions, state rejections or
native forwards among tracked replacement candidates. All four missing-state
cases accepted: two at activation, two at area load, replaying both expected
native-use shapes. All nine channels pass at baseline, area-load and final;
full frozen log review accepted. User reported the scene stayed active/clean
through the loading screen and subsequent walking/panning.

All 18,897 SDK/depth and 443 unfiltered-copy recordings completed/reset/released,
all 19,381 jitter scopes restored, hooks disabled, resources drained. The
single-fence telemetry correction produced no verification failure in this run.

Remaining observation: 443 unfiltered frames (412 grouped scene-depth reread
failures/mismatches, reason 3; 31 depth-copy lease rejections, reason 7). Both
live checkpoints were active/not waiting. At intentional stop the controller
reported 399 suspensions/398 resumptions and waiting=true, without fault/latch;
do not claim all suspensions recovered. Counters do not identify which field or
predicate changed, or prove a focus/menu trigger.

Evidence root `local/native-aa-state-admission-20260927/v1.21.3/`.
394-file archive SHA:
`1afe05d49e2b464fb369fa244937e8263e6907dc4a49782d79bea13e00b14a7a`.
Final log SHA:
`56c41283a6049e64f130049d90a34ca83a3221f6925c246ca5e5fb6d15793048`.
Installed DLL remains
`0c1aaba52e641dffd7bdc51b64cbdf32537e49273e053dc9ad3a9c73899ab763`;
prior DLL/sidecar backups and all four unchanged configurations verified.
Do not restore or uninstall automatically.

Next: inspect scene-depth identity versus mutable attachment/descriptor metadata,
and split depth-copy lease rejection predicates with bounded evidence if needed.
`SceneDepth` default equality includes diagnostic attachment and SRV fields;
this is a candidate explanation only, not permission to remove checks. Preserve
resource ownership, frame alignment and list/generation validation. No new
probe is prepared. No pixel readback or broad stability/AA-exclusivity claim;
exclusive_mode=false. CMFM/UI, DLSS upscaling and frame generation remain deferred.

Historical preparation and earlier results below are superseded by this closed
run. Preserve their contemporaneous limitations.

PREPARED, INSTALLED AND SEALED 2026-09-27: **v1.21.3 current-use state admission**.
Game CLOSED; no run armed. [Implementation and test contract](native-aa-state-admission.md).
Launch the same bare MO2 forced-DLL entry, native AA/DLSS Off, Windowed 2560x1440,
DX12; load a save and report ready. All saved settings unchanged.

Fixes the unfiltered telemetry fence mismatch by collecting/serializing one
completion observation, with fault-mode pin retention. Adds a bounded native
current-use supplement only when the original color/output API proof fails.
Both captured patterns qualify; conflicting barriers and incomplete/changing
native evidence still reject. Current native use is checked twice with exact
backing/owner and packet identities. Final planned tracker state is not used.
This relies on the pinned native planner/execution contract, not an independent
GPU state query. No new hook, SDK mode, queue model or settings write.

40 native and 138 Python checks passed, including a WARP completion-during-log
regression, 22 owned-memory admission cases and replay of all four saved misses.
The existing API state_of implementation and activation/SDK/retirement policy
sources are unchanged. This new supplement is NOT live verified yet.

Evidence root `local/native-aa-state-admission-20260927/v1.21.3/`.
324 frozen sources; installed DLL SHA:
`0c1aaba52e641dffd7bdc51b64cbdf32537e49273e053dc9ad3a9c73899ab763`.
375-file prelaunch seal SHA:
`955cfe4da3bf6a8bb8887a054414cef9c3a3641c11b23b70b23f60175b6d0fc1`.
Prior v1.21.2 DLL and output sidecar backed up in this root; prior 453-file
archive verified. Do not automatically restore or uninstall after this run.

Next after save loaded: verify seal/startup/modules/process generation/configs,
arm the frozen tool once, then use frozen capture_phase.py to check all NINE
channels (new state_admission_verified). Inspect accepted/rejected proof receipts
and actual SDK work, then one area load to exercise the second pair. Announce
intentional stop before signaling, verify final drain, request normal exit and
archive without restoration. Keep the recovered depth-lease suspension as an
unresolved observation. exclusive_mode=false; CMFM/UI, DLSS and FG deferred.

Historical closed results below are superseded by this installed, unarmed build.

CLOSED AND ARCHIVED, DLL LEFT INSTALLED 2026-09-27: **v1.21.2 provenance captured**.
Game PID 19116 closed normally; process absence verified. No pending user
question. [Result and limitations](native-aa-state-provenance-result.md).

Four rejections replayed: two consecutive frames at activation, two by area
load. First of each pair has color 0x100->4 and output 0->0x400 on a compute
packet two schedule entries earlier. Its color span extends through the TAA
consumer. Second has local color transition, output initial state 0x400.
All current output spans are 0x400->0x400 while final planned output is 0x20;
final tracker state is not a current-state getter. Native scheduling intent
still needs execution-order/lifetime validation before admission changes.

6,101 DLAA / 5,107 unfiltered; zero faults, SDK failure or collisions. All 6,463
SDK/depth and 5,107 copy recordings completed/reset/released; all 11,622 jitter
scopes restored; all hooks disabled. Full frozen log review accepted. Three
live/final checkpoints pass all eight channels; the area checkpoint preserves
one reconstruction-review failure from inconsistent fence observations during
collection/serialization. Do not claim every live checkpoint passed.

An extended depth-lease suspension (reason 7) recovered after the user returned
to the scene: 1,407 more evaluations at the return checkpoint. Exact trigger
is not established. User reported clean scenes; no pixel readback this run.

453-file archive SHA:
`5fbc9ba3be94585d364464d66a32ae09b03091387f08edf1e4f6004e42f76a96`.
Root `local/native-aa-state-provenance-20260927/v1.21.2/`; installed DLL remains
`c256f8c970f2d39b42f4af7ebebd0e1e99f48447831ea5ec039562036e89e881`.
Prior v1.21.1 slot backup and all four configurations verified unchanged.
Do not restore or uninstall automatically. No new run is armed.

Next: fix unfiltered fence snapshot consistency with a deterministic regression
case; design bounded native current-use evidence with epoch/resource/order and
lifetime validation to supplement the existing local API state proof. Keep
depth-lease suspension as a separate unresolved observation. Rendering gates
were not changed in this diagnostic; exclusive_mode remains false.

Historical preparation and earlier checkpoints below are superseded by this
closed, archived result.

PREPARED, INSTALLED AND SEALED 2026-09-27: **v1.21.2 native state provenance**.
Game CLOSED; no capture armed. Launch the same bare MO2 forced-DLL entry and
load a save, native AA/DLSS Off, Windowed 2560x1440, DX12. Settings unchanged.
[Offline findings and capture contract](native-aa-state-provenance.md).

Offline planner 229CEE0 can move barriers onto earlier packets, explaining why
local consumer evidence need not cover native state setup. Resource tracker
backing+70 holds per-epoch state-use spans and final planned state; the latter
can describe later packets and is not safe as a direct current-state getter.
The native destructor frees that tracker before releasing its D3D resource,
so existing COM pins do not pin native metadata. These are confirmed static
findings; association with live missing-color samples remains to capture.

This build extends only rejection diagnostics: first-eight/latest samples add
three bounded tracker/span snapshots, bounded packet-table search and up to 32
preceding packets' relevant state nodes. Captures mark incomplete/changing data;
no new hook, GPU command, settings write, or rendering admission change. The
original state_of source and all SDK/activation/recovery/retention paths remain
unchanged. Four new code-hash gates check scheduler/constructor interpretation.
39 native and 128 Python checks passed; frozen source count 318.

Evidence root `local/native-aa-state-provenance-20260927/v1.21.2/`.
Installed DLL SHA:
`c256f8c970f2d39b42f4af7ebebd0e1e99f48447831ea5ec039562036e89e881`.
432-file prelaunch seal SHA:
`cc2c7eb274c28c4628efab2d6f78ada2898b8a7b220b953121be9962894d8ea3`.
Prior v1.21.1 DLL and output-dir sidecar preserved in this root before replacing
the test slot. Original baseline remains archived in v1.21.1. No restoration
was performed. User prefers leaving the diagnostic installed; do not restore
automatically after the next test.

Next: verify sealed files, startup capabilities, module paths, configurations
and process/log generation; arm only once user reports save loaded. Use frozen
`capture_phase.py` for baseline with all eight channels, inspect provenance,
then one area load only if useful. Full reviewers require state-trace schema 2
and scheduled-state-evidence-v1 for this version. Announce intentional stop,
verify drain, request normal exit, archive explicitly without automatic restore.
Still exclusive_mode=false; this build diagnoses, not repairs, the four native
fallbacks. CMFM/UI and resolution-changing DLSS remain deferred.

Historical checkpoints below are superseded by this installed, unarmed build.

CLOSED AND ARCHIVED, DLL LEFT INSTALLED 2026-09-27: **v1.21.1 passed**.
[Captured state-rejection result](native-aa-state-trace-result.md).
Game CLOSED normally, process absence verified. No pending user question.
Current v1.21.1 DLL and output sidecar retained at user request; DO NOT restore
or uninstall as cleanup. Baseline backup and all four configurations verified
unchanged. Next launch would still load this diagnostic; do not arm a new run
until the next investigation/test is ready.

Four rejected states captured completely: two consecutive frames at activation
and two by area load. Every output has native same-state UAV words and an API
UAV barrier, with no transition in this consumer scope. First frame of each pair
also has no color transition/native color entry; second has color 4 -> 0x40.
This explains the local proof failure without establishing persistent GPU state.
No admission behavior was changed. Next inspect native state provenance and
reset/recreation rules for both resources before designing a safe admission fix.

Final 7,825 DLAA / 578 unfiltered / 498 recovered suspensions; zero faults,
collisions or latch. All 8,325 SDK/depth and 578 copy recordings completed/reset/
released, all 8,905 jitter scopes restored, hooks disabled. User reported area
load and movement clean. The stop was intentional and announced beforehand.

Sealed evidence `local/native-aa-state-20260927/v1.21.1/`, 389 files. Archive SHA:
`9c8df14ee1efbe7ab679f40dad4bac7a61594d6b334702239233c72c2b8512c0`.
Installed SHA `bc33760a39b20cbee92b0e5ace26e1d72ed72979f307097c0bd4fc934579cfaa`.
Sources frozen unchanged; current-state and result documentation added afterward.
Still exclusive_mode=false, CMFM/UI deferred, no resolution-changing DLSS yet.

Historical checkpoints below are superseded by this closed, archived run.

STOPPED HEALTHY 2026-09-27: **v1.21.1 captured both rejection pairs**.
PID 2584 / creation 134349919514657184 is pending normal-exit confirmation.
User reported area loading followed by walking/panning stayed active and clean.
Area checkpoint: 6,523 DLAA frames; all eight review channels passed, two more
rejects with the identical missing-color-then-valid-color / same-state-output
pattern. All four API and native-node captures are complete and replayed.

Intentional stop announced first, requested tick 1092796 (106.625 seconds after
arm). Final review accepted: 7,825 DLAA replacements, 578 unfiltered copies,
498 suspensions and resumptions, zero faults/collisions/latch; all 8,325 SDK/depth
and 578 copy recordings completed/reset/released, zero live references. All
8,905 jitter scopes restored, all hooks disabled, pending zero. Four native
forwards remain; their scope is now explained but not repaired.

PENDING USER REPLY: leave AA/DLSS Off, exit normally and confirm closed.
Keep this DLL installed as requested; do NOT run restore. After closure verify
configs and slot/backup identity, archive with explicit installed=true and
restored=false. Log SHA:
`363dd4ec644e5ba092ffcf2868879d3d59c930db3d578077a426b8c50251c590`.
Evidence `local/native-aa-state-20260927/v1.21.1/`; frozen sources unchanged.

Historical checkpoints below are superseded by this stopped run.

LIVE AND ARMED 2026-09-27 17:14 +03: **v1.21.1 baseline captured**.
Game PID 2584 / creation 134349919514657184 is OPEN; user confirmed save loaded.
Prelaunch seal (368 files), installed DLL, all four configurations, host SHA,
module paths and startup capability review verified before arm tick 986171.
Evidence root `local/native-aa-state-20260927/v1.21.1/`.

`baseline-2584` passed all eight review channels: 1,447 successful DLAA frames,
zero faults, two output-state rejects. Both have native output words
0x400 -> 0x400 and a matching API UAV barrier, but no output transition within
this consumer. First also lacks a color transition and matching native color
node; second has color 4 -> 0x40. Both have motion 4 -> 0xc0. Native node lists
are complete, API traces fully replayed. This confirms the suspected same-state
output barrier in these samples, NOT persistent GPU state or a safe exception.
The first sample's missing color state needs separate proof. Analysis saved in
`baseline-state-analysis-2584.json`; no state admission changed.

PENDING USER REPLY: one area change through a loading screen, then ten seconds
of walking/panning; whether DLAA remains active and clean. Keep AA/DLSS Off and
window size unchanged. Capture with frozen `capture_phase.py --phase area-load
--previous <root>/baseline-2584.review.json`, compare rejected states, then
announce intentional stop, verify drain and preserve final evidence. Do not
intentionally stop before the user's pending checkpoint reply unless a fault.
User asked at yesterday's session end to keep this DLL installed rather than
restore it; honor that standing request unless the user revises it. No automatic
restore after this run. Preserve baseline backup and sealed files.

Historical checkpoints below are superseded by this live run.

SESSION ENDED AT USER REQUEST 2026-09-27: leave v1.21.1 installed for tomorrow.
User explicitly requested no uninstall and no restoration of the previous DLL.
No live run was started or armed for this build. Keep the prepared DLL, output
sidecar, backups and sealed evidence intact. Resume the short state-rejection
capture when the user returns; verify current process/installation state first.
No scheduled launch or automatic follow-up was requested.

PREPARED, INSTALLED AND SEALED 2026-09-27: **v1.21.1 state-rejection trace**.
Game CLOSED; MO2 OPEN (PID 41020). Ready for the same forced-DLL launch and
loaded save, native AA/DLSS Off, Windowed 2560x1440, DX12. No hooks armed yet.
[Diagnostic design and offline findings](native-aa-state-trace.md).

This build only adds bounded rejection evidence; the original state admission,
DLAA/copy lifetime, recovery and activation paths are unchanged. Native code
emits a UAV barrier when its before/after state words match, while our existing
proof requires a transition in the current callback. This is a plausible cause
of v1.21.0's four raw/output state rejects, NOT yet confirmed live. Do not
weaken the state check without the new resource-specific evidence.

38 native and 119 Python checks passed. Generated native fixtures compared
the explanatory trace to original admission across 4,096 barrier cases. The
initial parser immutability test failure was fixed and preserved alongside the
passing rerun. Rendering behavior has not been changed by this diagnostic.

Evidence root `local/native-aa-state-20260927/v1.21.1/`; frozen 312 sources,
368-file prelaunch seal SHA:
`a367e2a1445688b9c05321f0fb7e17fbe002e06d21d98887a338ddf356a22595`.
Installed DLL SHA:
`bc33760a39b20cbee92b0e5ace26e1d72ed72979f307097c0bd4fc934579cfaa`.
Exact baseline slot and configurations preserved, no settings changed. The
output-dir sidecar points to this evidence root. Restore with this root's
`deploy.ps1 -Action Restore` only after game closure. Still exclusive_mode=false.

Next: verify sealed startup and process generation, arm the loaded save, capture
a short baseline with frozen `capture_phase.py` (eight review channels), then
one area load if needed. State trace keeps first eight and latest rejection,
API barriers and bounded native packet-state records. Compare all checkpoints;
announce intentional stop before signaling, verify clean drain, request normal
exit, restore and archive. No need to repeat the entire sauna/focus suite for
this diagnostic-only change. Prior v1.21.0 archive remains sealed immutable.

Historical checkpoints below are superseded by this installed, unarmed test.

CLOSED, RESTORED AND ARCHIVED 2026-09-27 04:42 +03: **v1.21.0 passed**.
[Live result and remaining scope](native-aa-unfiltered-result.md).
Game CLOSED; MO2 remains available. Exact baseline slot DLL restored, output-dir
sidecar removed, all four configurations unchanged (AA/DLSS Off, Windowed
2560x1440, DX12). No live action pending; no next build installed.

25,008 successful DLAA replacements; 967 verified unfiltered copies across
190 suspension periods, all resumed. Zero faults/collisions/latches. All 25,091
SDK/depth recordings and 967 independent copy recordings completed/reset/
released, zero live references; all 26,438 projection scopes restored, hooks
disabled. User reported clean movement, sauna/reentry, area load and focus.
Intentional stop was announced, then normal exit confirmed and verified.

Still **exclusive_mode=false**. Four native candidates remain, all raw-color/
output resource-state proof (reason 6): two early, two by area loading. No
resource-specific failed-state samples yet. Next inspect that state path and
add bounded evidence if needed; do not loosen unknown-state checks. Producer
publication/unmatched consumer coverage also remains to audit. Actual SDK
failure latch and other saved AA choices only have offline fixture coverage.
Keep native postqueue and validated DLAA/copy lifetime paths; CMFM/UI deferred.

Evidence `local/native-aa-unfiltered-20260927/v1.21.0/` is sealed immutable.
377-file archive SHA:
`baae40911fbd60c6593c9841a1bd77c44b0c5c084cb3da011b577513d1575243`.
Final log SHA:
`035af88dc6ea83085616b5339b8847f9ac1fbe73163e2e4c1b425d6badcfa385`.
Source remains v1.21.0, 37 native / 108 Python checks passed. No changes after
the 306-source freeze except current-state/result notes and run archival tools.

Historical checkpoints below are superseded by this completed/restored run.

STOPPED HEALTHY 2026-09-27: **v1.21.0 live recovery test passed**. Game PID
49076 / creation 134349464479679036 still OPEN pending normal-exit confirmation.
Assistant explicitly announced intentional stop; stop tick 57445375, duration
347.954 seconds. `final-review-49076.json` accepted the stopped capture.

25,008 DLAA replacements, 967 unfiltered frames, all 190 suspension periods
resumed, zero faults/collisions/latches. All 25,091 SDK/depth recordings and
all 967 independent copy recordings completed, reset and released; zero live
resource references. All 26,438 projection scopes restored and all hooks
disabled. User reported clean still/moving scene, sauna/reentry, area load and
windowed Alt-Tab cycle. All five phase checkpoints passed seven review channels.

Four unclaimed native candidates remain, all output-resource-state gate (6);
two early, two by area loading, no further by final. Unfiltered reasons are
scene-depth identity 80 and unavailable depth lease 887. Queue rejections 458
(457 registry group, one native gate). Still `exclusive_mode=false`; no real
SDK failure injected, so latch behavior remains offline-tested only.

PENDING USER REPLY: leave native AA/DLSS Off, exit normally, confirm closed.
Then verify process absence, save `normal-exit.json`, restore using current
root's `deploy.ps1 -Action Restore`, run `finalize_run.py`, and document result.
Evidence root `local/native-aa-unfiltered-20260927/v1.21.0/`; prior baseline DLL
and configuration backups are present. Do not overwrite while game is open.
Do not re-arm or prepare another test before completing restoration/archive.

Historical checkpoints below are superseded by this healthy stopped run.

LIVE AND ARMED 2026-09-27: **v1.21.0 baseline passed**. Game PID 49076 /
creation 134349464479679036 is OPEN. User confirmed save loaded. Arm tick
57097421; installation acknowledged, no faults. Verified sealed manifest (352
files), all four configuration hashes, loaded MO2 DLL and SDK module paths,
startup capabilities and exact process/log generation before arming.

`baseline-49076.status.json` / `.review.json` in
`local/native-aa-unfiltered-20260927/v1.21.0/` passed all seven review channels.
2,411 reconstructed frames, two unfiltered copies, two suspensions/resumptions,
two zero-jitter scopes; both unfiltered GPU recordings completed/reset/released.
No failure latch. Two early unclaimed native candidates remain; reason 6
resource-state gate accounts for both (the new temporal-motion reason would
be claimed/unfiltered). Two scene-depth-identity suspensions account for the
copies. Do not claim full exclusive AA or complete stability yet.

User confirmed clean still scene and movement. `walk-pan-49076` passed all seven
review channels at 6,458 reconstructed frames, 17 unfiltered frames and 17
successful resumptions. All 17 copy recordings completed/reset/released;
no faults/latch, early native-forward count still two. All copy reasons are
scene-depth identity (3). User observation saved in `user-observations.jsonl`.

User confirmed sauna walk/pan/reentry stayed clean and active. `sauna-49076`
passed all seven channels at 10,455 DLAA frames / zero faults. 32 unfiltered
frames across 26 suspension/resumption periods; all copy recordings completed
and released, peak two. Reasons: scene-depth identity 25 and unavailable depth
lease seven. Native-forward count still two (early resource-state failures).

User confirmed area-load plus movement remained clean and active.
`area-load-49076` passed all seven channels: 14,873 DLAA frames / zero faults,
573 unfiltered frames across 78 fully recovered suspension periods. All 573
copy recordings completed/reset/released. Reasons: depth-lease 524, scene-depth
identity 49. Four native forwards total, all resource-state rejections (6).

PENDING USER REPLY: final stationary five-second Alt-Tab away/back and ten
seconds in game, no travel/resize; whether scene remains clean. After checkpoint
passes, announce intentional stop before signaling it, verify final closure,
then request normal game exit to restore/archive.
Keep native AA/DLSS Off. Do not stop intentionally while awaiting the visual
reply unless telemetry fails. Next capture movement checkpoint, then sauna and
an area load. Tell user before intentional stop. Frozen `capture_phase.py`
includes all review channels; compare each checkpoint with previous. Restore
baseline slot only after confirmed normal game exit.

Historical preparation below is superseded by this live run.

PREPARED, INSTALLED AND SEALED 2026-09-27: **v1.21.0 qualified unfiltered
suspension**, ready for a live test. Game CLOSED; MO2 OPEN (PID 41020).
Native AA/DLSS Off, Windowed 2560x1440, DX12, bare profile; all configuration
hashes unchanged. User should launch the same forced-DLL entry and load a save.
No hooks armed yet. [Contracts, scope and verification](native-aa-unfiltered.md).

Verified color/output candidates now copy raw color when temporal readiness is
missing, with separate Reset-plus-fence resource retention. Waiting projections
publish zero jitter; DLAA resumes at a history reset boundary. A recovered SDK
failure latches reconstruction off, suppresses jitter and removes native AA from
subsequent owned postqueues. Fresh diagnostic/process launch is the current
explicit retry. Infrastructure faults still stop hooks. No new game hooks.

**Still exclusive_mode=false:** output identity/state failures, producer
publication failures and unmatched consumers remain possible native forwards.
The new dispatch reason counters must determine what remains; do not claim
the prior 1,172 native candidates are all fixed or were all depth-lease misses.
The whole-run reviewer rejects a latched failure as successful DLAA evidence;
the separate reconstruction reviewer can validate/report its failure state.

37 native and 108 Python checks passed. Actual WARP output is bit-identical to
raw color; two-list batches, pending GPU work through Reset, cross-queue order,
replay rejection and zero-jitter/latch behavior are covered. New live behavior
has NOT yet been verified. Use frozen tools for baseline/movement, sauna, then
area-transition checkpoints, checking both GPU lifetime trackers and remaining
native-forward reasons. Keep native AA and native DLSS Off.

Evidence root: `local/native-aa-unfiltered-20260927/v1.21.0/`.
Installed DLL SHA-256:
`43690e5830661be4eac4e8dd5d88687d09479749837f010975f57f7e1a6359cf`.
352-file / 306-source prelaunch manifest SHA-256:
`ff6a013a995c52f8ea0c0456b32cccde304f03caafc07118a170eaec93adab9d`.
Prior v1.20.2's 353-file archive was verified unchanged before freezing.
`deployment.json` preserves the baseline DLL (SHA 689bef7c…88cc) and exact
sidecar state. After normal game exit, run this root's `deploy.ps1 -Action
Restore`; verify all configuration hashes and seal the run. Never replace the
slot while the game is open. Use this root's `capture_phase.py` to preserve
each checkpoint; it now includes reconstruction control/lifetime review.

Historical checkpoints below are superseded by this prepared test.

CLOSED, RESTORED AND ARCHIVED 2026-09-27 04:06 +03: **v1.20.2 passed with
15,903 DLAA replacements while native AA remained Off**.
[Result, remaining scope and archive](native-aa-postqueue-result.md).
Game CLOSED; exact baseline slot DLL restored, output-dir sidecar removed.
All four configurations unchanged (AA/DLSS Off, Windowed 2560x1440, DX12).
No next build installed and no live action pending.

User confirmed clean still/moving scene, sauna walk/pan/reentry, area loading,
and windowed focus cycle. Runtime and activation faults/collisions stayed zero.
17,076 native queue insertions/preparations and both comparison overrides ran;
17,655 camera scopes all observed AA Off. 579 transient queue rejections accrued
between sauna and area-load checkpoints (578 registry group / one native gate),
then evaluation continued. Do not claim a more specific registry failure cause.

The assistant announced and intentionally stopped after 228.578 seconds. All
15,925 GPU recordings completed and released, all 17,655 jitter changes restored,
all scopes/callbacks drained and all hooks disabled. Normal exit confirmed and
process absence checked; archive and exact DLL/config rollback verified.

Important remaining work: **exclusive_mode=false**. 1,172 unclaimed candidates
still forwarded native TAA. Finish the agreed unfiltered suspension and latched
failure policy without silent native-AA fallback, preserving native postqueue
preparation and the tested GPU path. Other saved AA choices (existing entry and
competing-stage suppression) have fixture coverage but still need live checks.
Do not jump to CMFM/UI or resolution-changing DLSS yet.

Evidence root: `local/native-aa-ownership-20260927/v1.20.2/`.
PID 56980 / creation 134349443685469575.
353-file archive SHA-256:
`aefcf378556f4f0f100ade49260b43c8c716e11b635829eca065692a2e566bf4`.
Final log SHA-256:
`087049f1074ac6c589e7fc8e3d155babdee54c4ec40091e24d7b7196efbe8da8`.
Do not modify the sealed archive. Source code remains v1.20.2, Release verified
by 35 native and 47 Python checks. User settings remain untouched.

Historical checkpoints below are superseded by this closed and restored run.

LIVE AND ARMED 2026-09-27 04:01 +03: **v1.20.2 now evaluates DLAA with
native AA Off.** Game PID 56980 / creation 134349443685469575 is OPEN.
Baseline: 1,226 successful qualified DLAA replacements, zero faults/collisions,
1,228 native queue insertions/admissions, all saved-AA observations Off. Modern
history is retained (one initial reset). All baseline review channels passed.
This is a live baseline, not completed transition or exclusive-AA verification.

User confirmed the still scene and movement look clean. The walk-pan checkpoint
passed all channels at 4,510 replacements / zero faults. User also confirmed sauna walking/panning and reentry stayed active and clean;
its checkpoint passed at 7,723 replacements, zero faults/collisions/rejections.
An area change through a loading screen plus ten seconds walking/panning is now
requested; reply pending. Keep native AA/DLSS Off and window size unchanged. Do not intentionally stop while awaiting the visual reply
unless telemetry fails. Explicitly tell the user when intentionally stopping.

Exact modules/process/startup, 326 sealed files, 300 sources and all four
configurations verified before arming. Modern SDK is loaded from the local SDK
runtime directory. Arm requested tick 55045218. Evidence and frozen tools:
`local/native-aa-ownership-20260927/v1.20.2/` (source-1.20.2/tools).
`baseline-56980.status.json` and `.review.json` preserve the first verified phase.
The installed DLL hash is
`f6a56c8ea444f1bd7bb4bef621b11fd02591048b6d154cef6fa14b76513795c9`.
Stop through arm_continuous_dlaa.py --stop for this exact generation, review final
closure/GPU/jitter, then ask for normal exit before restoring. Baseline slot is
backed up; do not replace it while game is open.

Historical checkpoints below are superseded by this active runtime test.

PREPARED AND INSTALLED 2026-09-27: **v1.20.2 native postqueue activation**.
Game CLOSED; MO2 OPEN (PID 41020); not armed. Native AA/DLSS remain Off,
Windowed 2560x1440, DX12; bare profile and all four configuration hashes unchanged.
[Implementation, hook contracts and test limits](native-aa-postqueue.md).

The missing boundary is now handled before native postqueue sorting/routing.
Further offline tracing corrected the earlier inference: workers register TAA,
then the queue invokes its preparation on the main context directly, bypassing
the generic unit-dispatch hook. This build uses normal native registration after
worker join, deduplicates TAA, removes competing AA entries before routing and
scopes comparison overrides around the queue's TAA Execute. It does not adopt
worker contexts or change the proven GPU/input/request/history implementation.

Release /W4 /WX clean; **35 native and 47 Python checks passed**. Owned fixtures
cover worker prefilter/join, all four saved choices, existing/missing entries,
capacity/duplicates, unrelated stages/threads/views, retained native exclusions,
stop, reentrant queues/Execute and exceptions. This is offline verification;
AA-Off live preparation and rendering are not yet proven. Final exclusive AA
and unfiltered suspension/latched-failure policy remain pending; no CMFM/UI work.

Evidence/control root: `local/native-aa-ownership-20260927/v1.20.2/`.
Installed DLL SHA-256:
`f6a56c8ea444f1bd7bb4bef621b11fd02591048b6d154cef6fa14b76513795c9`.
326 prelaunch files / 300 frozen sources; prelaunch SHA-256:
`b0ec6202246db01c2b2f2f2b03f59299d2606434ae6a762cbf8bf3a82c2b3a20`.
Previous exact slot DLL is backed up. `deploy.ps1 -Action Restore` restores it
and its previous sidecar after game exit without changing saved settings.

Next user action: launch the same MO2 test entry, load a save with native AA and
DLSS Off, then report save loaded. Use frozen `source-1.20.2/tools/` and the local
capture_phase.py. Verify process creation/loaded modules/DLL/startup/configuration
identities, then arm within the five-minute startup window. Establish advancing
queue insertion/admission, table and sharpen override counters and DLAA frames
before asking for panning/sauna/area changes. Schema 2 rejection reasons are:
0 queue identity/collection phase/count, 1 registry, 2 native layer filters,
3 descriptor/entry ownership, 4 full queue, 5 nested queue. Both schemas remain
supported by the reviewers, but v1.20.2 requires schema 2.

Historical checkpoints below are superseded by this installed build.

CLOSED AND RESTORED 2026-09-27 03:36 +03: **v1.20.1 admitted 2,739 camera
scopes with AA Off, but zero qualified TAA unit visits and zero DLAA evaluations.**
[Result and native scheduling map](native-aa-scheduling-result.md).
The entry-order fix was exercised; native job admission and worker-context
coverage are the next missing pieces. No new build is prepared or installed.
Game CLOSED; MO2 OPEN (last observed PID 41020); baseline DLL restored.

The assistant intentionally stopped after 25.891 seconds. Both fault counters
stayed zero, all 2,739 jitter changes restored, both hook sets disabled and all
scopes drained. No private GPU submissions occurred. User confirmed normal exit;
exact baseline DLL and sidecar rollback verified, all four configurations unchanged
(AA/DLSS Off, Windowed 2560x1440, DX12). The full reviewer rejects the zero-frame
run; valid zero-count telemetry is not a rendering pass.

Live bounded inspection found the unit manager's render entry at RVA 0x221a050
and registered TAA2 with flags 0x11. Offline review found an inline AA-enable
filter before job publication and native cloned worker contexts. v1.20.1's
parent-thread TLS/context checks cannot cover that worker route. The exact
runtime manager branch/worker arguments were not captured, so do not claim
which branch ran on a specific frame. Preserve the native filtering, job
synchronization, context copying and queue merge while implementing admission
and worker lifetime; cover cached and uncached paths. Add reason counters and
fixtures for prefilter-before-callback and worker propagation. Do not globally
force serial rendering. Qualified GPU resolve/request retirement stay unchanged.

Evidence: `local/native-aa-ownership-20260927/v1.20.1/`.
PID 45428 / creation 134349427329869354. Final log SHA-256:
`7854bc7a02efe83eb47727a036c7bf4e66a5b18be887bbcb099403805adbc728`.
356-file final archive SHA-256:
`5f6a5f9bdf693e7687a89d34557887418b3c84a2412649404142f5d400fe6e2c`.
All frozen sources, prelaunch files and settings were rechecked before sealing.
Private `dispatch-analysis/` contains reviewed manager, submission, worker,
context-copy and output-merge spans. `dispatch-live-45428.json` has the bounded
live pointer/type/flags observation; it is not a frame-synchronous call trace.

No further live action is pending. Next work is the scheduling/worker integration;
keep CMFM/UI and DLSS resolution changes deferred until this AA ownership path
works. Final exclusive AA/suspension/failure policy is still incomplete.

Historical checkpoints below are superseded by this closed run.

PREPARED 2026-09-27: **v1.20.1 entry-timing correction is INSTALLED; game CLOSED;
MO2 OPEN; not armed.** [Failure, correction and limits](native-aa-entry-timing.md).
The v1.20.0 run admitted zero scopes and performed zero DLAA evaluations. It
stopped intentionally, restored all 2,375 jitter changes and disabled every hook.
Normal exit and exact baseline/config restoration were verified before this
replacement was installed. The failed run's 382-file archive SHA-256 is
`a89e847dbb3714b6f09758ff18eb024f3a40775fb26f4a68c565224e010494d7`.

v1.20.1 validates the incoming primary view in R8 instead of requiring the
context's view to be bound before native setup. Unit dispatch revalidates the
actual bound view; comparison overrides apply only inside admitted TAA2 Execute.
Main display only; other views, display calls and threads retain native gates.
Tests cover initially null/stale context views, non-primary bound views, stop,
reentrancy and exceptions in both scopes. Release /W4 /WX clean, **35 focused
native and 43 Python checks passed**; the expanded native fixture was rerun.
Two incorrect startup-test fixture invocations ran no tests and are preserved;
the corrected fixture passed all 16. No live v1.20.1 success yet.

Native AA/DLSS remain **Off**, Windowed 2560x1440, DX12; all four configurations
are unchanged. Qualified GPU resolve, SDK integration, request retirement and
retention are unchanged. `exclusive_mode=false`; final unfiltered suspension
and latched-failure integration remain pending. No CMFM/UI work.

Evidence/control root: `local/native-aa-ownership-20260927/v1.20.1/`.
Installed DLL SHA-256:
`191b54e007c75fc2573ceb12282be4faf8719461de39aaa46458c4b66c8f68d9`.
325 prelaunch files / 299 frozen sources; prelaunch SHA-256:
`4bbaafbd745fda782999d088dddf02bdaeeefaa1a7182d348cc0d7debd349254`.
Use frozen `source-1.20.1/tools/` startup/arm/final reviewers and `capture_phase.py`.
On the next save-loaded reply, verify exact process creation, loaded module path,
DLL/startup/configuration identities, then arm within the startup five-minute
window. Confirm nonzero activation frames/Off-choice counts, TAA admission,
both table/sharpen branch counters and advancing DLAA frames before requesting
panning/sauna/area travel. Stop and inspect promptly if activation remains zero.
Do not mistake valid zero-count telemetry for successful activation.

Explicit stop must drain both scopes, complete GPU/jitter checks and disable all
hooks. After game exit, `deploy.ps1 -Action Restore` restores the exact previous
DLL/output sidecar and records settings identities without writing settings.
Preserve this prelaunch seal and all prior archives. The next live test is the
only pending user action; user need not select TAA or change any settings.

Historical checkpoints below are superseded by this installed correction.

PREPARED 2026-09-27: **v1.20.0 scoped native preparation is INSTALLED; game
CLOSED; MO2 OPEN (PID 41020); not armed.**
[Implementation and limits](native-aa-activation.md).
Native AA and DLSS remain **Off**, Windowed 2560x1440, DX12; all four settings
files are unchanged. This build supplies native temporal preparation independently
of saved AA, suppresses competing AA registration/native jitter, and selects the
unsharpened current-ring output. It does not write native option/layer fields.
The actual qualified DLAA GPU replacement and v1.19.1 request retirement are
unchanged. Unqualified/publication-blocked GPU callbacks still use the diagnostic
native path, so `exclusive_mode=false`; final unfiltered suspension/failure
integration remains pending. No CMFM/UI work.

Release `/W4 /WX` clean; **35 native and 111 Python checks passed**. New owned
ASM/MinHook fixtures cover all four saved choices, native gate exclusions,
other-thread isolation, competing AA/jitter suppression and stop/SEH cleanup.
One request-review invocation used an incompatible archival retention fixture;
its failure log is preserved, and the corrected v1.18.3 fixture passes all 19.
No v1.20.0 live success is claimed yet.

Evidence/control root: `local/native-aa-ownership-20260927/v1.20.0/`.
Installed DLL SHA-256:
`199463bdb449435b9e6bd4ba08ee00ce189c739ffa025047559e7aa1116a4140`.
359 prelaunch files / 298 frozen sources; prelaunch SHA-256:
`70c3dd47094ba6cec03b415da7fbcca168a14c84c4c0967b7d0bca55bc8d55de`.
Use frozen `source-1.20.0/tools/` startup/arm/final reviewers and `capture_phase.py`,
which now checks `activation` in addition to all previous review channels.
User should launch the usual MO2 test entry, leave both AA/DLSS Off, and load a
save. Verify the exact loaded DLL/generation, startup readiness and configuration
identities before arming. Confirm advancing DLAA replacements with activation
choice 0 observed before asking for still/pan, sauna and area-loading reports.
Table and sharpening branch counts must advance; inspect native fallback counts
without describing this as complete exclusive AA. Explicit stop must drain the
camera-render scope, disable all old/new hooks and complete GPU/jitter checks.
After game exit, this root's `deploy.ps1 -Action Restore` restores the exact
previous DLL/output sidecar and records configuration identities, without writing
settings. Preserve the 359-file seal and all prior archives.

Historical checkpoints below are superseded by this installed diagnostic.

CLOSED AND RESTORED 2026-09-27 02:48 +03: **v1.19.1 completed 15,761 DLAA replacements with
zero faults or request collisions in PID 47596 / creation 134349394655214204.**
The repaired overlap occurred once: frame 23341's published request retired after
validated native return, and reconstruction continued to frame 36270, beyond
12,929 frames and the 32-frame slot reuse. Both reported area-change checks and
the sauna walk/pan/re-entry check stayed active and clean. Final review accepted;
all 15,798 GPU recordings completed and released, all 17,397 jitter injections
restored, zero resource pins/pending callbacks and all hooks disabled.

**The diagnostic was explicitly stopped by the assistant after the last successful
checkpoint; the user's subsequently observed DLAA stop was intentional.** User
confirmed normal exit; restoration verified game absence, exact prior DLL and
all four configurations (including native AA/DLSS Off, Windowed 2560x1440).
The temporary output-directory sidecar was removed. No new build is installed
or armed. [Result and scope](native-taa-bypass-retirement-result.md).
Evidence root: `local/native-taa-bypass-20260927/v1.19.1/`;
`final-review-47596.json`, `stop-47596.json` and the phase reviews/user reports.
Final log SHA-256:
`494ff5fa2facc8ccaea1ca00970de4464a48ee0472a009ce0975f645ebe4aa43`.
This remains a qualified bypass (`exclusive_mode=false`): 149 unclaimed resolve
candidates and the one validated overlap used native forwarding. No long soak,
dedicated focus/resize/device-recreation test or pixel readback in this run.

All 346 prelaunch files and 290 repository sources were rechecked before sealing
the 371-file final archive. Archive SHA-256:
`7a0875b1d5e7f8c243f600ee7393e0b16d416bbb393fd72c219e6d61b2ad6829`.
Next: renderer-level AA ownership around this native temporal stage, including
activation independent of the player's AA selection, suppression of other AA,
one jitter owner, correct output selection and agreed failure/suspension behavior.
Then resolution-changing DLSS; CMFM/menu work remains deferred.

Historical preparation below is superseded by this successful stopped run.

Prepared 2026-09-27: **v1.19.1 fixes the reproduced already-published request
retirement case and is INSTALLED for the next focused live test. Game CLOSED;
MO2 OPEN (PID 41020); not armed.**
[Implementation and scope](native-request-forward-retirement.md).
Native AA temporarily TAA, DLSS Off, Windowed 2560x1440, DX12, bare profile.
The native callback still forwards during publication, but a uniquely validated
request is now reserved through the call and retired only after successful return
with matching packet/sequence. Unknown callbacks do not retire future requests;
in-flight slot overwrite, stale tokens and exceptions remain fail-closed.

Release `/W4 /WX` clean; **34 focused native checks and 99 Python checks passed**.
Eleven new actual native-hook fixtures include deterministic producer/consumer
overlap and the 32-frame slot reuse. Qualified DLAA GPU code and hook gates are
unchanged. The failed first Python request-test invocation used a nonexistent prior
fixture path; the corrected invocation passed all 19 tests, and both logs remain.
No live repair/transition success is claimed yet.

Ignored evidence: `local/native-taa-bypass-20260927/v1.19.1/`.
Installed DLL SHA-256:
`fd5000ead97248c29062bd58a150d42009b8f9e1fe91a70965a0ce7889cf93ae`.
346 prelaunch files / 290 frozen sources; prelaunch SHA-256:
`75bb4c3f83619ef1ab1025dca0ae9929009a24f3d2c619d3bcadfaee2a100208`.
Use this version's frozen `source-1.19.1/tools/` startup/arm/final reviewers and
`capture_phase.py`, which also verifies `forwarded_requests` and retirement deltas.
The header must carry `request_retirement=validated-native-return-v1`.
Start with save loaded, verify startup and exact generation, arm, then confirm
DLAA progress before repeating the user's area-change/sauna route. For evidence
that the fix actually exercised, look for successful native-forward retirements;
a zero-retirement stable run alone does not reproduce the previous overlap.
After stop/drain and game exit, this session's `deploy.ps1 -Action Restore`
restores the preserved baseline DLL, sidecar and exact graphics configuration
when unchanged. CMFM remains deferred; `exclusive_mode=false` still applies.

Historical checkpoints below are superseded by this prepared diagnostic.

Closed 2026-09-27 01:56 +03: **v1.19.0 replaced 5,703 native TAA resolves
successfully, then stopped on request-slot collision (fault 6) during a reported
area change.** [Result and next fix](native-taa-bypass-first-result.md).
No SDK or claimed-resolve failures; 94 unclaimed native candidates. The old live
request survived a publication-blocked, pointer-matching native callback and
collided exactly 32 frames later. Full packet identity was not validated on that
guard branch; fix proven request consumption/lifetime, not ring capacity.
User reported the sauna mostly fine earlier; no separate sauna checkpoint or
exact loading/focus sequence is established. Do not conflate this with independent
AA's depth-provenance fault 19.

**Game CLOSED normally; exact prior DLL restored; output sidecar removed; all
four configurations restored/matched, including native AA and DLSS Off.**
All 5,736 GPU signals completed, all 6,279 jitter injections restored, callbacks
drained and hooks disabled. One completed/reset recording remained conservatively
pinned by the freeze-on-fault policy until process exit; healthy retention/full-run
reviews correctly reject this failed run. No next build deployed or armed.
327-file archive SHA-256:
`03c3527994b37f26ce3043af8430551585241bc551dd4d6680eb771422c550fc`.
Evidence root `local/native-taa-bypass-20260927/v1.19.0/`; sources and prelaunch
unchanged. Next: validated request bookkeeping for publication/consumer overlap,
owned interleaving fixtures, then another focused area/sauna live check. CMFM deferred.

Historical checkpoints below are superseded by this closure.

LIVE 2026-09-27 01:53 +03: **v1.19.0 ARMED in PID 53628, creation
134349367051776333.** User reported save loaded. Startup and all 307 frozen
prelaunch files verified; exact loaded DLL/module and four prepared configuration
identities match. At the first saved checkpoint: 1,793 DLAA reconstructions and
SDK evaluations, seven unclaimed native candidate dispatches, zero faults or
request collisions; history, SDK, retention and resolve reviews pass. This is
scoped native-resolve bypass evidence, not whole-renderer exclusive-AA coverage.
Pending user report: still scene plus ten-second slow pan outside sauna. Keep
native AA TAA and DLSS Off during this diagnostic. Capture the next checkpoint
after the user's report, then test sauna if healthy. Evidence root below;
`inputs-53628-134349367051776333.jsonl`, `arm-53628.json`,
`startup-review-53628.json`, `live-preflight-53628.json`,
`baseline-53628.status.json` and `baseline-53628.review.json`. Game still OPEN;
do not restore the DLL until stopping, draining and game exit.

Prepared 2026-09-27: **v1.19.0 qualified native TAA bypass is installed for its
first focused in-game test. Game CLOSED; MO2 OPEN (PID 41020); not armed.**
[Procedure and rollback](../runtime/native-taa-bypass.md). Native AA is temporarily
**TAA**, native DLSS Off, Windowed 2560x1440, DX12, bare profile. This activation is
for the scoped diagnostic; it is not the final player's AA-setting requirement.
Start with a loaded save outside the sauna, verify startup, arm, and verify
reconstruction counters before pan/sauna/area/focus phases. CMFM remains deferred.

Installed DLL SHA-256:
`aab26314bf1b099cc5dcf48aee23e14a85c1c594cafec2cd4bb3ce4b1ccf0c13`.
The DLL is unchanged from the offline GPU-verified build. Updated Python arm,
startup and final reviewers plus the resolve reviewer passed **67 checks**.
Resolver checks require accounted candidates/outcomes, matching SDK/frame counts,
zero failed replacements, monotonic counters, and drained final pending work;
they explicitly retain unclaimed native counts and `exclusive_mode=false`.
The preparation initially expected 15 startup tests; all 16 passed, so the stale
expected count was corrected without rerunning or discarding successful logs.

Ignored evidence: `local/native-taa-bypass-20260927/v1.19.0/`, 287 frozen source
files and 307 prelaunch files. Prelaunch SHA-256:
`62abd037c7b0c6aeb5fa0873828f26ce857be321099efacbea370bbf73c8ad91`.
Use the frozen `source-1.19.0/tools/` control/review files and `capture_phase.py`.
Previous exact DLL and four configurations are backed up; the only setting change
is `Anti-Aliasing=Off -> TAA`. After stopping and game exit, run this session's
`deploy.ps1 -Action Restore`. It restores the baseline DLL and sidecar and restores
graphics only when the prepared file is still unchanged, preserving conflicts
for review. No original game executable, runtime DLL or asset changed.

The older offline/current-state checkpoints below are historical; the installed
diagnostic and temporary TAA setting above supersede their deployment state.

Scope clarified by the user: **player-facing settings, the warning popup and
native AA-control lockout wait until the user's CMFM (Configuration Menu For
Mods) framework exists.** Do not build that framework or investigate menu hooks
as a prerequisite for the current bypass diagnostic. Use existing diagnostic
controls for now. Current focus is native temporal preparation, TAA resolve
replacement and its scoped in-game verification; renderer correctness and
resource/failure handling remain distinct from future menu integration.

Implemented offline 2026-09-27: **v1.19.0 replaces qualified native TAA resolves
with DLAA and provides unfiltered per-invocation recovery.**
[Implementation and results](native-temporal-resolve-implementation.md).
Release build, 25 focused native checks and six hardware cases passed, including
600 continuous replacements with zero native resolves and all 600 recordings
retired. **Not deployed or live-tested. Game CLOSED; baseline slot unchanged.**
The user requested temporal/bypass implementation before mode state and UI.
Unqualified native forwarding and diagnostic-stop restoration remain in this
intermediate runtime; it is explicitly not a complete exclusive-AA mode and must
not be deployed as one. A scoped bypass diagnostic may use a manually selected
TAA stage and must report unqualified native frames explicitly. Broader renderer
activation/failure ownership, jitter and sharpening coordination remain separate
integration work. Popup/AA-control lockout is deferred until CMFM.

Previous offline assessment:

Assessed offline 2026-09-27: **recommend reusing native TAA preparation and output
routing, replacing its main resolve with exclusive DLAA.** User requested this
review to reduce independent queue reconstruction and manual testing. See the
[native-stage assessment](native-taa-reuse-assessment.md) for the confirmed
dispatch boundary, activation/jitter/sharpening requirements and failure policy.
The old hour-long soak allowed native fallback skips; it does not establish
exclusive replacement. No implementation, build, option or deployment change
was made for this assessment. **Game CLOSED; baseline DLL restored; no test armed.**
Next: implement verified internal activation and exclusive routing around the
native stage. Further independent queue/depth-provenance work is deferred under
this direction; its evidence remains preserved below.

Menu requirement clarified: enabling our DLAA/DLSS should show the native
DLSS-style warning and lock the game's AA controls, without requiring TAA to be
selected first. Reuse the relevant UI behavior where separable from the legacy
renderer. The popup/lockout handler remains unmapped; this is a design requirement,
not an implemented UI change. See the assessment's menu section.

Previous completed independent-route diagnostic:

Closed 2026-09-27 00:42 +03: **v0.8.3 identified fault 19: depth is untracked
in the selected command-list Reset generation. Color/motion transitions pass.**
[Result](independent-aa-boundary-result.md). The same depth resource has a
COPY_DEST -> PS-SRV transition earlier in each successful sampled recording;
the failing recording has none. A predecessor-list transition is plausible,
not proven. Next: establish depth provenance in actual submission/queue order.
Do not seed from another recording's latest CPU-observed state.

**Game CLOSED normally; exact previous DLL restored, matching sidecar removed,
four configurations unchanged. No build armed.** Native AA and DLSS Off,
Windowed 2560x1440, DX12, bare offline profile. 7,408 successful DLAA handoffs
and GPU signals completed; 7,409 jitter changes restored. All hooks disabled,
descriptors detached. Two retained slots include one consumed/unsubmitted marker.
Strict review rejects full-run acceptance. Source/tests/tools remain unchanged
through capture and closure.

Haze now has 2,126 successful evaluations, five entries and four exits. First
haze and base-after-haze native graphs independently pass, as do first base/haze
input boundaries. User initially saw DLAA stay active in sauna, then reported
deactivation after further walking/panning/entry-exit testing. The trigger is
not guaranteed on the first entrance. Session:
local/independent-aa-boundary-20260927/v0.8.3/.

Previous preparation (superseded):

Prepared 2026-09-27T00:37:54.392312+03:00: **v0.8.3 input-barrier diagnostic built and verified
offline; installed. NOT armed or game-tested.**
[Runtime procedure](../runtime/independent-aa-boundary.md).
Game CLOSED. Native AA and DLSS Off, Windowed 2560x1440, DX12 On, bare offline
profile. The v0.8.2 partial archive is sealed: 427 files, manifest SHA-256
`113c4e4f59a94ecf584087e193c21af19d92178ea35221c9fce7e93f7dad79b0`.

The new build records the first accepted base/haze barrier and first rejected
barrier, marker/before/after states, and the first invalidating transition for
each tracked resource. Fault-19 admission stays strict; no rendering, SDK,
history or native queue policy change. This is a diagnostic, not a sauna fix.
13 native checks, 66 AA Python and 16 startup checks passed. The real observer
callback's synthetic WARP-resource rejection receipt passes independent review.
DLL SHA-256:
`e4d25cb2290035e036028e29a50bfbfbd41e944565fd2a87936b1354a8e12513`.
Session local/independent-aa-boundary-20260927/v0.8.3/.
Next: load outside sauna, verify startup, arm, capture baseline, enter sauna.
Do not edit source/tests/tools during capture or closure.

Previous completed run:

Closed 2026-09-27 00:27 +03: **v0.8.2 verified the native haze queue graph and
rendered 23 haze frames, then stopped at input barrier/state fault 19.**
[Result](independent-aa-haze-result.md). User reports a reproducible trigger:
entering the sauna; the older TAA-dependent route did not fail there. The exact
barrier subcondition is not captured yet. Do not infer that the SDK failed.

**Game CLOSED normally; exact previous DLL restored, matching sidecar removed,
four configurations unchanged. No test armed.** Native AA and DLSS Off,
Windowed 2560x1440, DX12, bare offline profile. 2,906 successful DLAA handoffs
and completed GPU signals; 2,907 jitter changes restored. All hooks disabled
and descriptors detached. Fault retained two slots, including one consumed
but unevaluated marker, until process exit. Strict review rejects full acceptance.

First haze graph independently verified at callback, return and restoration.
No queue rejection or failed native return. Haze exit was not observed. Next:
inspect the barrier predicate and old route; capture first rejected batch plus
bounded input-state history using the sauna as the reproducible trigger.
Session local/independent-aa-queue-layout-20260927/v0.8.2/.

Previous preparation (superseded):

Prepared 2026-09-27T00:21:55.028543+03:00: **v0.8.2 optional-haze queue correction built and verified
offline; installed. NOT armed or live-verified.**
[Runtime procedure](../runtime/independent-aa-haze.md),
[native haze mapping](independent-aa-haze-queue.md).
Game CLOSED, MO2 PID 41020 open. Native AA and DLSS Off, Windowed 2560x1440,
DX12 On, bare offline profile. Previous v0.8.1 restored and archived: 420 files,
manifest `ad0534b3b14d24a24813b97b7ca330c5b83e19e477d345ab5eb94831c9740dbc`.

The fifth native node is uHazeFilter. v0.8.2 handles base/haze layouts, predicts
scheduler-assigned owners instead of comparing stale collection endpoints,
and restores exact N-node pointer ordering plus unused tail. The owned in-place
pass stays at order 2; haze runs normally downstream. Unknown layouts still
reject. SDK rendering and camera/history policy are unchanged. New counters
and first complete haze/base-after-haze graph receipts enable independent review.

Build /W4 /WX and 11 native, 60 AA Python, 16 startup checks passed. All 144
native enqueue layouts and 720 six-entry pointer permutations were checked.
The normalized captured failure matches the test fixture. No live success yet.
DLL SHA-256:
`00acf00cbde7e28913be1d5412e77a214d41976faa99bc09ca89f97d1ee25279`.
Session local/independent-aa-queue-layout-20260927/v0.8.2/.
Next: launch, load the save, arm, repeat walking/panning where haze triggered.
Verify actual haze evaluations and graph receipts before claiming the fix.
Do not edit source/tests/tools during capture or closure.

Previous completed diagnostic:

Closed 2026-09-27 00:05 +03: **v0.8.1 identified queue fault 8: a fifth native
post node (vtable RVA 0x35372b0, order 20000) violates the exact four-node plan.**
[Result](independent-aa-admission-result.md). The user walked/panned only and
reported the stop before any area change. Descriptor recycle/removal completed;
all original four queue nodes and native AA-Off flags were unchanged.

**Game CLOSED normally; previous DLL restored, matching sidecar removed,
four configurations unchanged. No build armed.** Native AA Off / DLSS Off,
Windowed 2560x1440, DX12 On, bare offline profile. The continuous run remains
partial: 9,674 handoffs/signals completed, 9,675 jitter changes restored, all
hooks disabled. Strict review rejected acceptance. Two completed records and
one projected record stayed pinned until exit. History-reset quality remains
separate from the admission failure.

Next: inspect the extra native pass and its scheduler-assigned color endpoints;
extend insertion/restoration for the observed layout with neutral-chain tests.
Do not simply change count 4 to 5 or assume collection-time null owners stay
null. Preserve the partial archive before source edits. Session:
local/independent-aa-admission-20260926/v0.8.1/.

Previous preparation (superseded):

Prepared 2026-09-26T23:59:50.787888+03:00: **independent continuous AA-Off DLAA v0.8.1
admission diagnostic installed; NOT armed or game-tested.**
[Runtime procedure](../runtime/independent-aa-admission.md).
Game CLOSED; MO2 PID 41020 open. Native AA Off / native DLSS Off, Windowed
2560x1440, DX12 On, bare offline profile. The failed v0.8.0 archive is sealed:
465 files, manifest SHA-256
`e54286a1e61fa8a09186214cf538f98b7a171461c4f9246a223cdca6321fea9d`.

The new build retains continuous native output and all admission checks. It
records the first exact recycle/insert failure step, both relevant queue
snapshots and prior accepted queue. This is a diagnostic revision, not a fault-8
fix. No native TAA, automatic retry or history-policy change was introduced.
Repeat ordinary walking and panning after arming; no area transition required.

Build passed /W4 /WX; 11 native, seven stream Python and 16 startup review
checks passed. The first-failure fixture also passes JSON parsing. SDK pass,
shaders and rendering logic are unchanged; v0.8.0 hardware evidence retained.
DLL SHA-256:
`3ea0e489466c9bb0428efa1d09a25cf31793c2459377f730545f7b7e9f83b2a0`.
Session: `local/independent-aa-admission-20260926/v0.8.1/`.
Use sealed arm.py/stop.py/restore.py/archive.py. Restore only after game exit;
do not edit source/tests/tools while this test is deployed for capture/closure.

Previous live result:

Closed 2026-09-26 23:49 +03: **independent continuous AA-Off DLAA v0.8.0
PARTIAL: 4,196 displayed DLAA frames, then queue admission fault 8.**
[Result and next investigation](independent-aa-continuous-result.md).
**Game CLOSED normally; exact previous DLL restored, output sidecar removed,
all four configurations unchanged. No test armed.** Native AA Off / native
DLSS Off, Windowed 2560x1440, DX12 On, bare offline profile.

The user reported clean still/panning output, then noticed the feature stopped
while only walking and panning. No menu/map or area change preceded it. The
planned loading-transition check is NOT a pass. SDK evaluation did not fail:
all 4,196 handoffs/signals completed, all 4,197 jitter changes were restored,
all hooks disabled and descriptors detached. Fault froze two completed marker
records and one projected record until process exit; strict review rejected
full acceptance. Use final durable log, not the earlier status-file snapshot.

Fault 8 localizes to descriptor recycle or queue insertion. Its precise
subreason and rejected queue were not recorded. Next: bounded first-failure
diagnostics and offline reproduction before changing admission rules. Also
review 153 camera-distance history resets during ordinary movement separately.
Preserve the sealed partial archive before modifying source/tests/tools.
Evidence: local/independent-aa-stream-20260926/v0.8.0/.

Previous preparation (superseded by the live result above):

Prepared 2026-09-26T23:41:08.320633+03:00: **independent continuous AA-Off DLAA v0.8.0 installed;
NOT armed or game-verified.** [Runtime contract](../runtime/independent-aa-continuous.md).
Game CLOSED; MO2 PID 41020 remains open. AA Off / native DLSS Off, Windowed
2560x1440, DX12 On, bare offline profile. All four configuration hashes unchanged.

The native output connection is implemented for continuous rendering. One
persistent DLAA feature copies its packed result into the game's in-place color
buffer at the independent AA-Off read point, restoring PS-SRV state 128 and all
native bindings. No TAA route or fixed eight-frame cutoff is used. Source jitter,
measured frame time, history decisions, native input pins and marker retirement
are tracked continuously; no per-frame image capture. Actual failures latch off
without silent native TAA activation or automatic retry.

Use the usual **Monster Hunter: World - DLSS 1.2.110 test** entry and load a save.
Leave both AA and DLSS Off. Arm via the sealed session helper after its separate
startup receipt passes. First observe the still scene, then pan. Stop before
normal exit so completion/retirement and hook drainage can be reviewed. Windowed
focus changes are allowed after the initial visual check; avoid resizing for this
fixed-extent test. Area-transition behavior is implemented but not yet tested on
this independent streaming route.

Session: `local/independent-aa-stream-20260926/v0.8.0/`.
DLL SHA-256: `a75d57fc07f03ae33b53b54a214a4e5573e291f1f66d28b8dddcc3167172709b`.
Exact prior `689bef7c...` is in previous.dll. Only the MO2 slot and matching output
sidecar changed. Helpers: arm.py, stop.py, restore.py, archive.py; frozen reviewer
source/tools/review_aa_stream.py. Restore only after game exit. Do not change
source/tests/tools during capture or closure.

Offline verification: /W4 /WX build; 12 native checks, including 1,000 real WARP
submissions across two queues and 10,000 marker lifetime cycles; six Python
review tests with corruption subcases; 16 startup checks. Eight actual NVIDIA
SDK fixtures passed with zero GPU warnings/errors: 600 frames at 2560x1440,
600 at 256x144, and six failure cases. Every fixture checks exact native output
or unchanged color on failure, untouched depth/motion and downstream bindings.
The SDK pass and actual submission callbacks are tested separately; their live
engine integration and visual quality still require this game run.

General Native/Off/DLAA mode selection, suppressing other AA choices, resize,
resolution-changing DLSS and frame generation remain later work. This version
requires native AA Off; it is independent of selecting native TAA.

Previous completed milestone:

Completed 2026-09-26 23:04 +03: **independent AA-Off private DLAA v0.7.0 PASSED**.
[Result and next integration step](independent-aa-sdk-result.md).
**Game CLOSED normally; exact previous diagnostic DLL restored, matching output
sidecar removed, all four configurations unchanged. No next build installed or
armed.** Native AA Off / DLSS Off, Windowed 2560x1440, DX12 On, bare offline profile.

PID 56588 / creation 134349264954825425, source frames 16038-16045:
eight successful real DLAA evaluations with history retained after one initial
reset. Native TAA/FXAA flags were zero. Early startup ownership passed. All eight
projection/GPU associations and binding restorations passed; six jitter stimuli
were restored, with matching SDK jitter (rawX/2,-rawY/2).

All eight GPU completions and recording retirements were observed before hook
removal. Journal closed at 3,213 events and released all 494 pins. All hooks
disabled/drained, descriptors detached, resources released, no faults/pending
work. Nine native input images plus 15 private converted/DLAA/packed images
passed review. Exact conversion and output packing each verified 11,059,200
pixels. The paired preview shows a coherent matching scene.

This verifies independent private DLAA computation. Native color was not
replaced; presentation, temporal quality, measured color-jitter response and
cross-queue producer freshness remain unverified. The diagnostic uses fixed
frame time and waits between evaluations; it is not a continuous implementation.

Next: native output handoff at this AA-Off read point, respecting its PS-SRV
state 128. Adapt the copy/state contract rather than reusing the historical
TAA-entry UAV assumption. Verify downstream consumption and restoration.
Continuous lifetime/history, other-AA suppression and mode switching remain.
Do not rearm this one-shot capture or substitute the old TAA-dependent route.

Evidence: local/independent-aa-sdk-20260926/v0.7.0/, 361 frozen sources and
445 prelaunch files. /W4 /WX build, 48 native, 46 AA Python and 15 startup checks;
seven real SDK fixtures passed GPU validation with zero errors/warnings.
Capture 860,981 bytes, SHA-256
`f18e4a59c85fdb3066980c286f48b915e4b009c63daa0a435f2bd3da78560ac7`.
Test DLL `5a1c00ae...`; exact previous `689bef7c...` restored at 23:04:11 +03.
No native source, tests or tools changed during capture or closure.

Previous completed milestone:

Completed 2026-09-26 22:28 +03: **independent-AA projection/bindings v0.6.1
PASSED**. [Result and next integration step](independent-aa-projection-closure-result.md).
**Game CLOSED normally; exact previous diagnostic DLL restored, matching output
sidecar removed, all four configurations unchanged. No next build installed or
armed.** Native AA Off / DLSS Off, Windowed 2560x1440, DX12 On, bare offline profile.

PID 45344 / creation 134349242098501078, source frames 11895-11902:
all eight projection/request/GPU associations and native binding snapshots passed;
six jitter stimuli restored; nine color/depth/motion images saved and reviewed;
all eight fence completions and recording retirements observed. All hooks
removed/drained, descriptors detached and retained input/readback references
released. Journal released all 437 pins, with no faults.

The closure fix worked: all completions collected and observation closed before
slow hook removal. Journal froze at 2,831 of 4,096 events, 422ms after start;
GPU unhook began 281ms after closure. Full frozen reviewer accepted
`completion_before_hook_removal_verified`. Prior v0.6.0 failure remains rejected.

All nine decoded images are finite and visually coherent. Color/depth reflect
the same scene; motion is localized with a mostly still background. Source
association and matrix relations are proven, but measured raster jitter,
cross-queue producer freshness and independent DLAA output are not yet certified.
The capture did not call the SDK or replace native color output.

Next: connect the existing private conversion/DLAA evaluation at the verified
AA-Off input read point, using the immutable source projection and native binding
snapshot. Verify restoration after shader work. All three native inputs are in
PS-SRV state 128; the historical TAA-entry handoff assumes UAV and needs adapting
before native output can be replaced. Other-AA suppression, mode switching and
production failure latching remain integration work. Do not rearm the old
TAA-dependent path or this one-shot capture.

Evidence: `local/independent-aa-projection-closure-20260926/v0.6.1/`.
352 frozen sources, 391 prelaunch files; 48 native and 43 Python checks passed.
Capture 764,381 bytes, SHA-256
`9dd2d5efe123178d55d104f82d56113830677961e415b2cf19e56a7257592093`.
Test DLL `dbb5632e...`; exact previous `689bef7c...` restored at 22:28:29 +03.
No native source, tests or tools changed during the capture or closure.

Previous completed milestone:

Completed 2026-09-26: **independent-AA projection/bindings v0.6.0 PARTIAL**.
[Result and recorder-closure diagnosis](independent-aa-projection-bindings-result.md).
**Game CLOSED normally; exact previous DLL restored at 22:04:28 +03, matching
sidecar removed, all four configurations unchanged. No next build installed or
armed.** Native AA Off / DLSS Off, Windowed 2560x1440, DX12 On, bare offline
profile. MO2 remains open.

PID 51352 / creation 134349228791640896: eight source frames 15322-15329.
Projection/request/GPU receipt associations and eight native binding snapshots
passed independent scoped review. Six jitter stimuli restored, matrices verified,
native bindings unchanged. All 25 hooks disabled/drained and descriptors detached.

Full capture rejected: submission journal fault 2 at its 4,096-event cap before
completion polling; zero completed fence values recorded and no images saved.
All eight Signal calls succeeded and selected recordings retired; all nine copies
and resource-state restorations recorded. GPU completion is unverified, not known
failed. Journal retained 74 of 608 pins plus input/readback references until exit.

The last selected retirement was 407ms after journal start; its last event was
516ms and stop 829ms. Worker currently disables scheduler/jitter hooks before
entering the deferred completion loop, leaving journal traffic active meanwhile.
Next: correct shutdown order, poll selected completions/retirements before slow
hook removal, close journal admission before disabling, and test excess trailing
traffic. Do not simply increase the cap or rearm this one-shot DLL. Rerun with
unchanged settings to recover images; no independent-DLAA/pixel-jitter claim yet.

Evidence: `local/independent-aa-projection-20260926/v0.6.0/`.
351 frozen sources, 376 prelaunch files; 38 native and 41 Python checks passed.
Capture 1,064,243 bytes, SHA-256
`a7937f60ac72918497d4c6e8d9245ba9e682b419263ac03727750199e2b53709`.
`review-partial-51352.json` explicitly accepted=false. Test DLL `e22e17b5...`;
restored `689bef7c...`. No native source/test/tool edits during this capture or
closure; previous v0.5.0 archive untouched.

Previous preparation record (now superseded):

Prepared 2026-09-26 22:00 +03: **independent-AA projection/bindings v0.6.0
installed and sealed for the next launch; NOT armed or runtime verified.**
[Runtime contract and rollback](../runtime/independent-aa-projection-bindings.md).
Game CLOSED; MO2 PID 41020 remains open. Native AA Off / DLSS Off, Windowed
2560x1440, DX12 On, bare offline profile; all four configurations unchanged.

Use the existing MO2 **Monster Hunter: World - DLSS 1.2.110 test** entry.
Load a save, keep the character/camera still (a pose is useful), then report
ready. Arm only via the sealed session helper after the exact process's two-line
start/ready log appears. Nothing is hooked before explicit arming.

This combines scoped projection association and native GPU binding snapshots
at the qualified AA-Off input read point. Eight consecutive receipts: baseline,
six distinct jitter stimuli, final control. Each scheduler request must match
frame/epoch/bank/context/view/async; GPU receipt retrieves that same immutable
projection. Captures 0/3/7 each save color/depth/motion with state restoration.
Twenty-five hooks (previous six + projection + eighteen binding setters); eight
fence completions and recording retirements still required. No SDK evaluation,
output handoff, native AA option edits or TAA fallback. CPU association is not
a claim of measured raster shift, history correctness or independent DLAA.

The binding observer follows the existing successful Reset journal; each
snapshot must be ready and share the exact generation. Both compute/graphics
state is logged and must remain unchanged by copies. Missing bindings produce
a scoped failure report. Next, if runtime association/matrices/bindings and
image review pass, connect the existing conversion/DLAA record here. Other-AA
suppression and production AA-Off failure latching remain integration work.

Offline: clean /W4 /WX Release build; 38 native tests and 41 Python tests pass.
Real WARP binding fixture verified all eight snapshots with independent reviewer,
all copied pixels and a clean debug queue. Six identity mismatch cases and
scoped jitter failure/exception cleanup covered. Prior sealed v0.5.0 untouched.

Session: `local/independent-aa-projection-20260926/v0.6.0/`.
351 frozen sources, test binaries/logs, exact backup/configuration receipts.
Deployed DLL SHA-256:
`e22e17b580e1fa879f3744417c0bbe928e94351979737a361b16f80e6e6298fc`.
Previous DLL `689bef7c...` in `previous.dll`; only the MO2 slot and matching
`.output-dir` sidecar installed. Separate mode sidecar unchanged.
Use frozen `source/tools/review_aa_projection.py`, then normal exit and this
session's `restore.py`; archive only after successful closure review. Preserve
partial evidence on failure. Do not edit frozen source/helper files mid-run.

Previous completed milestone:

Completed 2026-09-26: **independent-AA input readback v0.5.0 PASSED**.
[Result, pixel inspection and next integration](independent-aa-input-readback-result.md).
**Game CLOSED normally; exact previous DLL restored at 21:39:22 +03, test
sidecar removed, all four configurations unchanged. No next build installed
or armed.** Keep native AA Off / DLSS Off, Windowed 2560x1440, DX12 On,
bare offline profile. MO2 remains open.

PID 46856 / creation 134349214275665878: eight source frames 9469-9476,
packets consumed once each, native parameters/queue/AA-Off unchanged. All eight
read points qualified after the first native paired color/motion 4 -> 128
transition on the same recording; depth already known 128. The earlier marker
still records unknown motion. No cross-list/previous-frame state was guessed.

Nine images from receipts 0/3/7 (source frames 9469/9472/9476) saved and hash/
footprint verified, 132,710,400 bytes total. All sources restored 128 after the
copies; eight fence completions and recording retirements verified. Zero faults,
all six hooks disabled, descriptors detached, 548 journal pins and private
readback references released. No SDK, jitter, native AA writes or output handoff.

Pixel inspection: color/depth show matching scene geometry; motion is localized
to animated objects with most still background at zero. All decoded values
finite; all three input types change between captures. About 4.66-4.69% of motion
pixels exceed 0.01 pixels. These observations do not prove exact temporal
alignment, jitter/history correctness, cross-queue writers or independent DLAA.
Local contact sheet and numeric report preserved; do not commit game imagery.

Next: connect immutable projection/request association, scoped jitter and the
existing input conversion/DLAA record to the qualified read point. Compute work
must restore native bindings as well as resource states; these plain copies
only needed state restoration. Verify frame-matched jitter and history before
output handoff. Other-AA suppression and AA-Off failure latching remain
integration work. Do not repeat unchanged input readbacks as a separate stage.

Evidence: `local/independent-aa-inputs-20260926/v0.5.0/`, 347 frozen sources,
389 prelaunch files; 10 native and 37 Python checks passed before launch.
Capture 891,486 bytes, SHA-256
`37afc3c65dffe2349ab6f2dd03d2ed9f3598dc578d81bebaa934fd2ac58b5332`.
Prelaunch SHA-256 `a1a4e76424910c1a62cdcdbbeab33e4d92d133e079ce6bfc3eab89d73f090980`.
Frozen `review-46856.json` accepted; nine image files and `pixel-inspection-46856.json`
preserved. No native source/test/tool changes during capture/closure. Tested DLL
`41275553...`; restored previous `689bef7c...`.
Final archive: 413 verified files; manifest SHA-256
`90420709a06e373cc3841bb51c243451d023e3edcae96317b38540f0d1c1082e`.

Previous completed milestone:

Completed 2026-09-26: **independent-AA GPU trace v0.4.0 PARTIAL: all eight GPU
completion/retirement chains verified; local motion state unqualified.**
[Result and next implementation](independent-aa-gpu-trace-result.md).
**Game CLOSED normally; exact previous DLL restored at 21:15:28 +03, test
sidecar removed, all four configurations unchanged. No next build installed
or armed.** Keep native AA Off / DLSS Off, Windowed 2560x1440, DX12 On,
bare offline profile. MO2 remains open.

PID 47780 / creation 134349200024568753: eight source frames 11189-11196,
unique packets consumed once; native parameters/queue/AA-Off preserved. Every
sample has Reset -> marker -> Close -> Execute -> private completed fence plus
recording retirement. All pins released, descriptors detached and six hooks
disabled. GPU/submission faults zero; deferred fault 9 / outer fault 6 correctly
report unknown motion state. No SDK, jitter or private image GPU commands.

Color was RENDER_TARGET (4), depth PIXEL_SHADER_RESOURCE (128); motion had no
transition before the marker in this list. Motion is active: 396 transitions
were captured. Each sampled queue batch submits the motion producer list before
our list. Its final transition leaves motion in RENDER_TARGET (4); our list
transitions it 4 -> 128 after the marker. Offline replay in actual batch/list
order finds consistent explicit states [4,128,4] at all eight markers. This
inference does not change the full review rejection or prove pixel freshness.

Next: qualify resource state across the native ordered command-list batch, then
connect input conversion and existing DLAA recording. Do not copy stale state
from a previous recording/frame or use CPU thread order as GPU order. Record-time
access needs native scheduled-state provenance available at the insertion point;
offline post-submission inference alone cannot authorize a live resource read.
No need to repeat the same neutral test unchanged. Independent jitter/controller,
other-AA suppression and AA-Off failure recovery remain integration work.

Evidence: `local/independent-aa-gpu-20260926/v0.4.0/`, 341 frozen sources,
376 prelaunch files; 18 native and 33 Python checks passed before launch.
Capture 855,513 bytes, SHA-256
`a524b181b510236c8358da1c5e5e7ae6268abe2795ef8ce5794d22d80103d783`.
Prelaunch SHA-256 `6b294fa4afd7baa91b8474bb3548f70474d82b8f897452cac5a82df6329e2fd0`.
`review-partial-47780.json`: accepted false, GPU completion true, full states
false, explicit queue replay consistent. No native source/test/tool changes
during capture/closure. Tested DLL `285a138f...`; restored previous `689bef7c...`.
Final partial archive: 390 files verified; manifest SHA-256
`ffc29213bcb152ccbbb63dd54f7c5d402bdc8a0053e560c0d3b9734c3101b293`.

Previous completed milestone:

Completed 2026-09-26: **independent-AA deferred marker v0.3.0 PASSED**.
[Result and remaining integration](independent-aa-deferred-marker-result.md).
**Game CLOSED normally; previous DLL restored at 20:49:07 +03, test sidecar
removed, all four configurations unchanged. No next build installed or armed.**
Keep native DLSS Off / AA Off, Windowed 2560x1440, DX12 On, bare offline profile.

PID 55784 / creation 134349183990087877: eight source frames 14132-14139,
eight unique empty packets consumed once each by the native DX12 consumer.
Owned post callbacks thread 58224; deferred consumer thread 58816. Sort key
phase 15/order 2; exact key and post-queue restoration. Native parameter table
unchanged; DIRECT list on the discovered device; identical full-size color,
depth and packed-motion resources before and after consumption. All pins released,
no fault/pending callbacks, all descriptors detached and both hooks disabled.

The global renderer counter was source frame +1 at every consumption. Request
frame/epoch/bank stayed immutable. Queue-entry addresses repeated across frames;
only unique packet address/generation establishes the request identity. This
validates the deferred CPU entry, not GPU freshness/states/submission or live
independent DLAA. No SDK/jitter/private GPU work was performed.

Next: establish actual resource states and ordering at this independent entry;
add command-list recording generation, submission completion and retirement,
then move existing input conversion/DLAA recording onto it. Do not repeat the
neutral marker-only test. Independent jitter/controller, suppression of other
native AA selections and AA-Off actual-failure recovery remain integration work.

Evidence: `local/independent-aa-deferred-20260926/v0.3.0/`.
333 frozen sources and 363 prelaunch files verified; frozen reviewer accepted.
Capture 52,881 bytes SHA-256
`459bf5b19d4934a05dbb0e6bdf1870389997c133b195e2d6813c221068854e84`.
Prelaunch SHA-256 `d828b4cd8ddc68b7b6d77eb21bfa1bef60d72e0286e39907b1520adef57da648`.
DLL tested `b7fd6e3f...`; restored previous slot `689bef7c...`.
Six native tests / 27 Python checks passed before launch, plus 96 private-memory
enqueue replays and eight consumer replays. No native sources changed during
capture/closure. Runtime contract and exact rollback are in the result link.
Final archive: 375 verified files; manifest SHA-256
`344c824248145503546d35b60da9406ac81fae7e95a49d6c6dd51ae233d47690`.

Previous completed milestone:

Completed 2026-09-26: **independent-AA owned in-place pass v0.2.0 PASSED**.
[Result and remaining integration](independent-aa-owned-pass-result.md).
**Game CLOSED normally; exact prior DLL restored at 20:22:53 +03, test sidecar
removed, all four configurations unchanged. No next build installed or armed.**
Keep native DLSS Off / AA Off, Windowed 2560x1440, DX12 On, bare offline profile.

PID 56352 / creation 134349167940833460: eight consecutive frames 23695-23702,
eight distinct descriptors, exactly one owned callback each at phase 15/order 2.
Native queue 4 -> 5 -> 4; sorted insertion and exact original queue restoration
passed. All original color endpoints stayed unchanged. The callback saw full-size
R11G11B10_FLOAT color, one mip/slice/sample, RT+UAV flags, same renderer device.
Native AA options remained Off. Each callback added zero packets; each native
phase appended 31. Fault/pending zero, all descriptors detached, hook disabled.

This proves owned CPU scheduling and neutral Off behavior in this scene, **not
independent DLAA or deferred GPU input validity**. No jitter/SDK/private GPU work.
Next: connect this callback to a versioned deferred request and identify GPU
consumption before downstream effects, establish input states/alignment/lifetime,
then connect the existing DLAA record and pre-projection jitter/controller.
Suppression of native AA when other options are selected and AA-Off actual-failure
recovery remain integration work. Do not repeat the same scheduling-only launch.

Evidence: `local/independent-aa-owned-20260926/v0.2.0/`, 326 frozen sources and
352 prelaunch files verified. `review-56352.json` accepted; independent saved
`capture-56352.jsonl` is 43,350 bytes, SHA-256
`293ca7b7961ffe1743e2309aa275d261322dbfcd26cd567eb8db89abaa9c9af9`.
DLL used `109f3902...`; restored previous slot `689bef7c...`. All arm/prearm/save/
rollback receipts preserved. Prelaunch SHA-256
`d3b4c6f0823dc7b438f628bb50ed5617eb9021d9518f327f9c504e9610933900`.
Five native tests and 21 Python checks passed before launch; native code did not
change during this capture/closure turn. See result for scope and resource limits.
Final archive: 365 files verified, all 352 prelaunch files and 326 sources
reverified. Manifest SHA-256
`9380e113e1f83b6a7c737c5245be473bde84887ed57ce30e6b2c414c3f100823`.

Previous completed boundary control:

Completed 2026-09-26: **independent-AA post-queue boundary v0.1.0 PASSED CPU
capture review**. [Result and next implementation](independent-aa-queue-boundary-result.md).
**Game CLOSED normally; previous DLL restored at 20:01:13 +03, test sidecar
removed, all four configurations unchanged. No next diagnostic installed/armed.**
Keep native DLSS Off / AA Off, Windowed 2560x1440, DX12 On, bare offline profile.

PID 61960 / creation 134349152740821787: eight complete before/after pairs on
consecutive frames 17259-17266. The post-effect queue at RVA 0x24f1fc0 entered
enabled/collecting with four descriptors, sorted them and appended 31 context
packets each frame. Both TAA/FXAA option bytes remained zero and no known native
AA unit appeared in this queue. All owned-slot preparation preconditions passed.
Fault 0, pending 0, callbacks drained, one hook disabled. Adapter LUID 98931:0,
swap-chain format 87, full 2560x1440 rectangle.

This establishes a live boundary independent of TAA selection. **No owned pass
was inserted; exclusive AA and independent live DLAA remain pending.** Owner
pointers already matched their returned values on entry; GPU pixels, freshness,
states and lifetime are not established by this observation. Existing continuous
DLAA success below is historical and still includes native TAA.

Next: implement owned descriptor/callback scheduling with bounded admission and
per-frame lifetime/removal. Prove its unfiltered output/deferred command path
before DLAA integration: a no-op filter can redirect output without writing it.
Then qualify inputs and connect the pre-projection jitter/controller and existing
DLAA recording, preserving AA-Off failure latching. No need to repeat this same
queue-only control or force native TAA flags/selection.

Evidence: `local/independent-aa-scheduler-20260926/v0.1.0/`, 317 frozen sources,
338 sealed prelaunch files. `review-61960.json`, independent `capture-61960.jsonl`,
arm/prearm/save receipts and `restored.json` retained. Capture SHA-256
`7bda19e369a3e87081970acebcdfaf0eca5ca843561154608da8d5db3275e980`.
DLL used `27e1dac6...`; restored previous slot `689bef7c...`.
Three targeted CTests and five Python arm checks passed before launch; no new
native code changes during this capture/closure turn.
Final archive: 351 files verified, all 338 prelaunch files and 317 sources
reverified. Manifest SHA-256
`8b9de11f111eb8a1978380223a708b6d9af819de25b44543e9439d131035a70d`.

Previous completed control / broader direction:

Active 2026-09-26: **independent AA selection + AA-Off recovery**, explicitly
authorized after the user rejected silently falling back to TAA and requiring
TAA selected in the game. [Foundation and live control](independent-aa-foundation.md).
The new pure controller and unfiltered GPU handoff pass offline checks: 24 native,
ten scalar-reader cases, nine hardware GPU cases (including 600 frames / 60 outputs)
and four legacy GPU regressions. **Not connected to the game renderer yet.**

Completed: [AA-Off resource control](independent-aa-off-control.md), PID 57764 /
creation 134349127635642237. **Game CLOSED normally; diagnostic mode restored
19:21:43 +03; original DLL unchanged, no new rendering DLL installed or armed.**
Keep native **DLSS Off / AA Off**, Windowed 2560x1440/DX12, bare offline profile.
Eight broad and eight focused CPU snapshots verified; six focused samples had
stable fields within one frame. Scene resources and TAA rings remain allocated,
but TAA input/destination bindings are null. Special mode=1 and TAA table=0 expose
a second early-return gate inside Execute. GPU contents/freshness/state/lifetime
are still unknown; enabling a layer alone does not establish independent AA.

Fixed the engine-owner height decoder in Python/C++: bits 30/31 are flags, so
0x85a00a00 is 2560x1440, not 2560x34208. Two live code spans verify packing.
Original captures remain unchanged. Ten Python tests and the rebuilt Release
C++ input-contract test pass. Fix sources frozen separately; not deployed.

The unfinished v1.19.0 TAA-fallback implementation was superseded, preserved under
that local root and removed from runtime/dllmain; those files match frozen
v1.18.4 again. The one-hour result below is historical and unchanged. Do not use
the old arm instructions/settings as the next test. Next: own a serialized
pre-projection decision and independent render preparation/scheduling, preserving
needed engine bookkeeping while suppressing TAA/FXAA. Qualify raw-output recovery
before publishing replacement work; live integration and jitter-free recovery
remain unproven. Do not ask for another unchanged CPU-control launch.

All 468 prelaunch files reverified, manifest SHA-256
`93d12856fb3d39e7f91aeff8f44ad9cbe1391ecaffe84ff3840dee51f40e53d6`,
303 frozen sources. Graphics `da3f357f...` (only TAA -> Off); mode restored to
`e94ac2a9...` (`native-depth-probe`); slot `689bef7c...`; profile/modlist unchanged;
`.dll.output-dir` absent. Evidence, review and exact restore receipt are under
`local/independent-aa-20260926/` (`review-aa-off.json`, `restored.json`).

Previous completed milestone:

Completed 2026-09-26: **v1.18.4 full one-hour output-retirement soak PASSED**.
[Result, evidence and remaining scope](continuous-output-retirement-result.md).
**Game 20320 CLOSED normally; collector 3840 EXITED; previous DLL restored at
18:31:40 +03, sidecar removed, all four configurations unchanged. No test installed.**
Keep Windowed 2560x1440, native DLSS Off/TAA On, DX12 On, bare offline profile.
User reported no visible issues and confirmed area changes through loading screens.

296,528 successful DLAA evaluations; 297,110 GPU recordings all submitted,
completed, reset and released; peak two live, zero retained recording references
at final stop. All 312,038 scoped jitters restored, zero faults/request collisions,
pending callbacks/private recordings zero, all hooks disabled. All 719 timed
snapshots and the final native log passed independent review. Thirty-two distinct
native output pointers observed, beyond the former 16-destination limit. Stop was
the planned 60-minute deadline (18:27:10 +03), not a diagnostic fault.

Evidence `local/dlaa-output-retirement-20260926/v1.18.4/`: **323-file final archive**
SHA `51a68aa6900c14e9eae909f7bfdf0c3470e03da16cd6f9d399acd5397b6f6981`.
296 prelaunch files / 221 frozen sources reverified. DLL
`044bede4ad9efca0d74e952988b4e06d22a708e52335ccc8e9eced3fac14420c`.
Process creation 134349063362149844. Native log
`776405bd7c1fd4896e0be3545fe3baffcae7f8078d56d5b77c1f432444482b23`;
timeline `540d7a478f87c0297474ce483fd1e6e6d97db7dbda7d9981288a69f7fe25f365`.

Output lifetime fix is verified in this run. Remaining: safe native TAA bypass
on replacement frames, then DLSS and FG/MFG under the agreed plan. The older
request-slot fault did not recur and is not independently fixed. Native TAA still
forwards; 13,189 skips and 42,961 history resets recorded. No objective quality/FPS
claim or universal lifecycle proof; no replay export supplied/reviewed this run.
Do not launch new implementation/testing merely from this completed status.

Preparation history:

Prepared 2026-09-26: **v1.18.4 output-resource retirement implemented and verified
offline; repeat recorded gameplay pending**.
[Ownership contract, tests and procedure](../runtime/continuous-output-retirement.md).
Evidence root `local/dlaa-output-retirement-20260926/v1.18.4/`.
The usual MO2 test slot is to be deployed with this build and sealed before launch;
check `deployment.json` / `prelaunch.json` for the exact identity and installation.
MO2 remains open. Keep Windowed 2560x1440, native DLSS Off/TAA On, DX12 On,
bare offline profile. User can reply from desktop and preserve the whole recording.

Resource pins now belong to request/consumer callbacks and a bounded table of
command-list generations. Old GPU references release only after successful Reset
AND the covering private fence completes. Faults freeze collection. Runtime-owned
continuous output mode removes the accumulating 16-destination set and the former
128 process-lifetime resource set; legacy helper mode remains available. One depth
history reference prevents recycled pointers from hiding a resource change.
Private SDK objects and list registry remain retained. No native TAA bypass yet.

23 native checks, 90 Python checks, 600-evaluation hardware SDK replay with 60
newly allocated destinations and both queues passed. All 600 recordings retired
and released, peak three live, no GPU validation warnings/errors, exact final
packed/downstream pixels. Four one-frame SDK regression cases passed. This does
not establish the new lifetime policy in the game until the repeat test runs.

Next: after user loads a save, verify startup/DLL/process generation, arm, start
the frozen collector for 60 minutes, verify healthy baseline and increasing
recording releases, then ordinary play. Preserve full recording and time any
symptom. Review closure, exit, exact rollback and archive at the end.

Previous completed result:

Completed 2026-09-26: **recorded soak failed at the prototype output-destination
capacity; replay reviewed and exact rollback complete**.
[Failure, video correlation and next work](continuous-soak-output-capacity-result.md).
**Game CLOSED; collector 11500 EXITED; previous DLL restored at 16:50:07 +03,
sidecar removed, four configurations unchanged. No next diagnostic installed.**
Preserve Windowed 2560x1440, native DLSS Off/TAA On, DX12 On, bare offline profile.
User may reply from desktop. No explicit normal/crash exit classification was
supplied; process exit was observed before restoration.

Run 51280 / creation 134349028442589442 used exact v1.18.3 DLL 470b48c2... and
stopped after ~11m16s, 45,964 evaluations, fault 14, failure string
`presentation destination capacity`. Frozen Output::bind retains 16 destinations;
a new output exceeded that bound. Native TAA fallback explains the likely FPS
rise. This is not an SDK Evaluate or GPU-memory-exhaustion error. Original request
fault 6 did not recur: all 49,801 published requests matched, zero collisions,
blocked/changed/multiple/reentrant consumers.

The intended 30–60 minute test did not complete. Standard review accepted=false;
separate closure passed: 45,980 signals/depth copies complete, 50,294 jitters
restored, zero pending/private lease, hooks disabled. Final 13,468 resets, 3,837
skips, 21 lists, 22 resources. Collector saved 136 snapshots / 1,953,385 bytes,
ended on native fault without a stop request or duration/storage exhaustion.

User supplied `%USERPROFILE%/Videos/Monster Hunter- World - 2026-09-26 4-43-37 PM.mp4`.
Video SHA 78d3d010282243306f2b012fbe9c3d4eef785ee14753cd9806a6f295d98351bd.
Replay inspection correlates the fault to hub-to-quest departure/loading around
13:10–13:16 in the export. Matching source recording and two audio correlations
establish source time = export time +5.080s (correlations >0.97); directory-level
wall-clock anchor remains approximate. Last healthy observed 16:40:44.892, first
fault 16:40:49.929 local. Do not claim the exact video frame of failure.

Evidence: `local/dlaa-soak-20260926/v1.18.3/`, **330 local files + one external
video identity**, final manifest SHA
`78b6a6fbeaf3bcae9ea3a770faeb5ce0e145c3cc952b88c850b50c0151f6aaca`.
248 prelaunch files / 215 frozen sources reverified; all snapshots reviewed.
Native log SHA `5f075568aae529bc9190f24011cbbdad0adadea7214abf1f5da708d9fe7e6a85`.
Timeline SHA `8d4942fcd588ee65b29a56e6cd9a986b9e231a0efadf20aca0749c6de8caf8c4`.
Original video remains outside repo; generated frames and sanitized source
recording metadata stay in ignored local evidence. No original game assets changed.

Next: design/implement safe output retention and retirement across transitions,
including references held both by temporal_present::Output and runtime pins.
Test >16 distinct outputs, pending GPU work and queue changes. Do not just raise
16 or discard references without GPU/lifetime proof. Then repeat the long test.
The old request-lifetime fault is still unresolved; native TAA bypass remains
planned before experimental DLAA signoff, followed by DLSS and FG/MFG while
keeping diagnostics. No runtime fix or performance/quality claim yet.

Previous completed result (historical status):

Completed 2026-09-26: **v1.18.3 request-lifetime diagnostic passed this run;
earlier collision NOT reproduced or fixed**.
[Result and limits](continuous-request-lifetime-result.md).
**Game CLOSED; exact previous DLL restored at 16:05:18 +03, sidecar removed,
all four configurations unchanged. No next diagnostic installed.**
Preserve Windowed 2560x1440, native DLSS Off/TAA On and DX12 On, bare offline
profile. MO2 Portable remains open/available. User may reply from the desktop;
phone replies are not required. Do not infer exact focus timing from replies.

Run 10652 / creation 134349011993956692 had active fault-free checkpoints at
2,526 baseline evaluations, 6,172 after first area load, 9,344 after second area
load and 13,050 after a separately cued stationary focus cycle. User reported
normal images after all three actions. Final review accepted: 14,245 evaluations,
14,277 GPU signals/depth copies complete, 15,074 jitter injections/restorations,
zero faults/pending/private lease, all hooks disabled. 444 resets, 170 skips,
21 lists and 12 resources. No collision, changed/multiple packet or reentrancy.

Eleven TAA and twelve depth calls hit the publishing guard. Saved first/latest
examples matched no pending record; intermediate matches are not individually
logged. All published TAA requests were consumed at both area checkpoints.
One of 14,415 published entries remained unmatched at stop; this is tracking
metadata, not a pending private GPU recording. Native TAA still always forwards.

Next: review publication/consumer overlap and cancellation lifetimes offline,
including consumption before publication. The diagnostic adds locking to the
previously immediate-return branch and can perturb timing; consider bounded
event-time recording without that wait before another reproduction. Do not
silently drop records or label this successful run a lifetime fix. Keep the
agreed TAA bypass/DLSS sequence below, with diagnostics retained throughout.

Evidence: `local/dlaa-request-lifetime-20260926/v1.18.3/`.
262-file final archive SHA-256
`f6bb29c4260f77e14c2ba88fa742e418bcb5d8857e50cd2e227f9d9c93b56850`.
All 240 prelaunch files / 211 frozen sources reverified. DLL
`470b48c2ef625489cee7aac7bbf58938cbe706fa3c7d4946b9114d8509cb4958`.
Final log `9e52b33d7d00ad05a08ef4330332c57d33a5a569bf77af45402f4036dc413b74`.
83 targeted offline checks; unchanged GPU fixture evidence inherited.
No objective pixel-quality/performance or full transition-stability claim.

Agreed sequence, 2026-09-26: **fix request lifetime -> safely bypass native TAA on
DLAA/DLSS replacement frames -> experimental DLAA milestone -> DLSS upscaling ->
FG/MFG -> broader cleanup/optimization/release work**.
[Milestones and native TAA bypass contract](dlaa-to-dlss-milestones.md).
Keep diagnostic hooks/telemetry through feature development. Normal mod controls,
broad testing and major performance work are not prerequisites for DLSS work.
Native TAA bypass is now an explicit prerequisite for the experimental DLAA
milestone. It is planned, not implemented: the current binary still always
forwards native TAA. Keep the game's TAA setup and use native rendering on
unsupported/failed frames. The request-lifetime failure remains the next task.

Completed 2026-09-26: **windowed baseline works; mixed focus/area transition
failed closed**, unchanged v1.18.2 run 49168.
[Failure and next investigation](windowed-area-transition-failure.md).
Windowed 2560x1440 DLAA was active with zero faults at the 1,762-frame baseline
and 5,409-frame first focus-return checkpoint. The later interval ended in
**fault 6: live TAA request slot on producer reuse** after 13,690 evaluations.
The user reported normal images, confirmed movement in the first cycle, and
clarified an area change/loading screen occurred **between the two focus reports**.
Do not label focus loss as the cause or claim the overall run passed.

**Game CLOSED; exact rollback completed at 14:49:25 +03. No next diagnostic is
installed.** All 13,835 private GPU signals/depth copies completed, all 14,377
jitter injections restored, zero pending/private recording, all hooks disabled.
Recorder healthy=false; success review rejected; separate closure review passed.
SDK result remained successful. Final totals: 635 resets, 159 skips, 21 lists,
9 resources. Counts are sparse and do not pinpoint failure relative to loading.

Prior DLL `689bef7c...` restored, output sidecar removed. **Preserve Windowed**,
2560x1440, DLSS Off/TAA On, DX12 On. The only saved graphics change was Window
PosX 0 -> 1280; current graphics SHA-256
`2fdf8da9c3de5c333e1f275b82cdae944bffd67abfd750f4c2b85a7f6154b410`.
The assistant made no config writes; other three configurations are unchanged.
MO2 Portable remains open/available; launch without `-i wowodot`.

Next: investigate request publication/consumption lifetime. The 32-slot ring
faults on an occupied frame-modulo entry; normal retirement is in matching
consumer lookup, which is bypassed while `publishing` is nonzero. Same-frame
multiple producers, delayed/omitted consumers at loading, and bypassed consumers
are unresolved hypotheses. Add bounded old/new collision metadata and consumer
bypass counts; exercise actual callback fixtures before selecting a retirement
rule. Do not enlarge the ring or silently drop live entries as a correctness fix.
Keep subsequent focus and area-transition actions separately checkpointed.

Evidence: `local/dlaa-windowed-focus-20260926/v1.18.2/`.
253-file archive SHA-256
`17b7ba4a6121c5a7af877c715df2425fbe3176b7fae53b4baa1e1475f4f62643`.
All 228 prelaunch files / 205 frozen sources reverified. The exact DLL remains
`f460559ffc15d0f8a6fdbb81fc292d18ba2c8713afa406f02a9c189097a0f159`.
Final log `88e0242f92c75cabeaa8f903b75ace7cd02eb63bbb79a5d9f4e7285e8042d114`.
PID 49168 / creation 134348966026012165; DX12, 2560x1440, format 87,
adapter [0,98931]. Runtime rendering code has not changed. No objective image
quality/performance claim, cutscene or full device/swap-chain recreation coverage.

Previous completed result (historical status):

Completed 2026-09-26: **menu/title/reload round trip passed**, unchanged v1.18.2,
run 51752. [Result and evidence](continuous-transition-result.md) and
[test sequence](../runtime/continuous-transition-test.md).
DLAA resumed after reload with one history reset covering the changed view/depth,
loading gap and camera discontinuity, then **3,266 retained-history evaluations**
through shutdown. The user reported normal images after the menu, at title, and
after reloading. This verifies one round trip, not full transition stability.

**Game CLOSED; exact rollback completed at 14:23:28 +03. No next diagnostic is
installed.** Final review accepted: 11,395 evaluations and completed GPU signals,
18,737 jitter injections/restorations, 60 resets, zero faults/pending, all hooks
disabled. Prior DLL `689bef7c...` restored, sidecar removed, four configurations
unchanged. Native DLSS Off/TAA On, bare profile; MO2 Portable remains the launcher.

At the title checkpoint, DLAA lacked the matching depth/list lease (last_skip=7)
and skipped evaluation. Gameplay resumed without a fault after reload. The final
6,505 skips are not individually attributed by sparse telemetry. There were
32 tracked lists and 19 pinned resources, within bounds. The one reload reset
spanned 7,258 frame/epoch steps and 64.573 seconds, with changed view/depth and
2,726.731 units of camera displacement. Reset reason counts overlap.

Next: focus/alt-tab and a cutscene or abrupt camera transition, then remaining
lifecycle behavior. Full device/swap-chain recreation is still unimplemented.
No objective temporal-quality or performance measurement was added. Native TAA
still runs; performance profiling remains a separate step.

Evidence: `local/dlaa-transitions-20260926/v1.18.2/`; **250-file final manifest**
SHA-256 `ff7528af35812292f858bc846c419ca8e7822bc98032f5fde1987f15edfc1dea`.
All 226 prelaunch files / 204 frozen sources reverified during archival.
Test DLL `f460559ffc15d0f8a6fdbb81fc292d18ba2c8713afa406f02a9c189097a0f159`.
Final log `512b0391e7ae62b521aa365f406b880538eed4d0b790095fd08705ecfa82251e`.
PID 51752 / creation 134348950486889994; DX12, 2560x1440, format 87,
adapter [0,98931]. Arm-to-stop requests span 190.922 seconds. Generated data is
ignored; no original game assets changed and no new build/tests were required.

Previous completed result (historical status):

Completed 2026-09-26: **camera-panning history fix verified**, v1.18.2 run 52584.
[Result and evidence](continuous-camera-history-result.md) and
[policy, telemetry and tests](../runtime/continuous-camera-history.md).
Across **10,002 evaluations**, only the initial evaluation reset history; the
remaining **10,001 retained history**. The corrected camera-displacement policy
avoided **3,473 resets** that the old view-translation check would have triggered.
The user confirmed panning and reported **"Clean scene; game closed normally"**.

**Game CLOSED; exact rollback completed at 14:08:04 +03. No next diagnostic is
installed.** All 10,002 GPU signals completed, all 10,003 scoped jitter injections
restored, zero faults/pending, all hooks disabled. One attempt was skipped.
Prior DLL `689bef7c...` restored, sidecar removed, all four configurations unchanged
(native DLSS Off/TAA On, bare profile). MO2 Portable remains the launch setup.

Next: validate genuine camera cuts and menu transitions with reason attribution,
then lifecycle behavior. A clean pan establishes continuous history retention in
this scene; it does not prove the cut threshold or full temporal quality. Live
pixel readback, cuts/cutscenes, alt-tab and resolution/resource recreation were
not tested in this run. No GPU-path source changed; the native TAA dispatch still
runs, and performance profiling remains deferred.

Translation cuts now use validated recovered camera displacement >20 world units;
other reset guards remain. The independent reviewer recomputes camera positions
and decision masks under schema 2 / `camera-displacement-v1`. All 68 targeted
offline checks passed, including 100,000 randomized poses and a compiled replay
of three saved live matrix pairs. Release /W4 /WX builds succeeded.

Evidence: `local/dlaa-camera-history-20260926/v1.18.2/`; 240-file final manifest
SHA-256 `992707510eb87c6398d783ce2a29db582b11393bdd3cab986d9a4be2198d766b`.
All 224 prelaunch files / 203 frozen sources were reverified during archival.
Test DLL `f460559ffc15d0f8a6fdbb81fc292d18ba2c8713afa406f02a9c189097a0f159`.
Final log `389e34903d91dd2277240bb018acf25b978186fc4967f901fc7295635df4acdc`.
Process 52584 / creation 134348940520110774; DX12, 2560x1440, swap-chain format
87, adapter [0,98931]. Generated artifacts remain ignored.

Previous diagnostic result (historical status):

Completed 2026-09-26: **history-reset cause identified**, v1.18.1 run 10484.
[Result and next implementation](continuous-history-reset-result.md).
Stationary checkpoint: 4,056 evaluations, only the initial reset. After panning,
the final 12,244 evaluations contain 2,411 additional resets, **all from the
view-translation check**. Every other reason count is zero. In two saved examples,
camera-center displacement was only 1.496/3.132 units, while view translation
changed by 23.898/47.952 and crossed the threshold of 20. Rotation affects that
translation term; it cannot serve directly as camera displacement.

**Game CLOSED; user confirmed normal exit; exact rollback completed at 13:41:15
+03. No next diagnostic is installed.** All 12,244 GPU signals completed, all
12,245 jitter injections restored, zero faults/pending, all hooks disabled.
Prior DLL `689bef7c...` restored, sidecar removed, all four configurations unchanged.
Evidence: `local/dlaa-history-reasons-20260926/v1.18.1/`; 237-file final manifest
SHA-256 `ad6ecada8bede412ecd1d73a4895a0dfa4f13d6a3ccb44deeae21b54f4f88782`.
The 201 frozen sources and all 219 prelaunch files are preserved and verified.

Next: replace the direct view-translation cut heuristic with validated recovered
camera displacement, retaining other reset guards and fail-closed matrix checks.
Test actual translation jumps and rotation-only/orbit cases before another live
stationary/panning/cut run. The replacement policy is not implemented yet.
Performance profiling remains deferred; this run made no FPS/quality claim.

Historical preparation record below (installation was rolled back):

Prepared and installed 2026-09-26 after the user resumed work: **continuous
history-reset diagnosis, v1.18.1**. [Procedure and contract](../runtime/continuous-history-reasons.md).
The game is CLOSED. Next: launch the usual MO2 test entry in the bare offline
profile, native DLSS Off/TAA On; load a save and keep the camera still. Verify
startup and the exact loaded build, then arm with `tools/arm_continuous_dlaa.py`.
Save a stationary status after about 20 seconds, request about 20 seconds of
panning, and compare reason counts and typed examples. Stop and restore after
the evidence is saved. No automatic game launch or live arming has happened yet.

This version preserves the v1.18.0 reset policy and GPU rendering path. It adds
eleven overlapping reason counts, retained-history streaks, first examples per
reason and the latest reset (below 8 KiB fixed storage). The synthetic fixed-camera
yaw test demonstrates the view-translation threshold can reset during rotation;
whether that caused the previous live resets remains unproven. Do not adjust
thresholds based only on that synthetic example. The live reviewer independently
reconstructs the saved reason/element masks and compares totals to the SDK.

Evidence: `local/dlaa-history-reasons-20260926/v1.18.1/`.
Installed DLL SHA-256:
`150a47a33b3434afd37b23d3c45c488d181d431882e97e5a68c217e951f517aa`.
Prelaunch: 219 verified files, 201 frozen sources; SHA-256
`a84bab9cfc7dca61e4b6539e2e14eab4dd1eccadeb59f99e81be21fd880b42fe`.
50 targeted checks pass: 10 continuous native checks (including 100,000 policy
comparisons), 12 history reviewer, 13 prior continuous reviewer, 15 startup reviewer.
Release /W4 /WX builds succeed. All 55 inherited code spans and six dependency
hashes verified; 18 GPU-path source files unchanged from the accepted prototype.
All four configurations are unchanged. The exact prior DLL `689bef7c...` and any
prior sidecar are preserved by this root's `deploy.ps1`; use `-Action Restore`
with the game closed after the run. Generated artifacts remain ignored.

MO2 launch correction: this setup uses the existing **Portable** instance at
`C:/Modding/MO2`, configured by `C:/Modding/MO2/ModOrganizer.ini`. Launch
`ModOrganizer.exe` without `-i wowodot`; that argument requests a nonexistent
global instance. The portable MHW window is open (PID 41020), profile
`barebonesdlss`. The executable dropdown currently shows the normal MHW entry;
select **Monster Hunter: World - DLSS 1.2.110 test** before the diagnostic launch.

The user reported about 80 FPS versus 200–240 baseline during v1.18.0. No timing
attribution is established; the user asked to retain focus on history correctness.

Previous completed milestone (historical status):


Completed 2026-09-26: **first continuous DLAA prototype run accepted**, v1.18.0.
[Run 18336 result](first-continuous-dlaa-result.md) and
[implementation contract](../runtime/continuous-dlaa.md).
20,802 successful evaluations/native handoffs, all 20,802 GPU signals completed,
all 20,876 scoped jitter writes restored, zero faults or pending callbacks and
all hooks disabled. The user reported a clean image, requested the stop after
testing, and confirmed normal exit. Arm-to-stop requests span 237.296 seconds.
This verifies continuous operation and telemetry; the build has no per-frame
live pixel readback and does not establish full temporal quality or transition
stability. There were 73 skipped attempts and 4,566 history resets.

**The game is CLOSED and exact rollback completed at 04:49:44 +03. No next
diagnostic is installed.** Prior DLL SHA-256
`689bef7cf00ff71e4518f452f0467edf6c3d877f2a6dd090553388d8b5f288cc`
is restored, the output sidecar is removed, and all four configurations are
unchanged (native DLSS Off/TAA On, bare profile). MO2 was last open as PID 62012.
The user ended this test; do not restart or arm another experiment automatically.

Next: identify the history-reset causes and refine the policy, then validate
camera cuts, menus/cutscenes, alt-tab and resource re-creation. The high reset
count is confirmed; over-sensitive view-matrix/projection checks are hypotheses,
not an established diagnosis. Resolution/device rebuilds remain unsupported.
The prototype intentionally retains bounded GPU resources until process exit.

Evidence: `local/dlaa-continuous-20260926/v1.18.0/`, including 196 frozen build
sources and the post-run reviewer. All 207 prelaunch files reverified; 13 reviewer
corruption checks passed. Final archive: 227 local files plus 48 referenced
offline-check files, SHA-256
`e8f68af76572ec0db4119aebb8a7b9ec976d969f0f0c152f53480af7850685f0`.
Offline validation included 240 serial/600 asynchronous evaluations, exact final
SDK packing/downstream compute/draw with no GPU validation errors/warnings,
1000 projection callbacks, 1000 WARP submissions, and prior temporal regressions.
Build DLL SHA-256:
`cbd768404b2d752f39256282dfaa27abb3b6e8f10d6eefa3dfee0d48580782e6`.

Previous milestones below are historical (immutable evidence):

Live milestone 2026-09-26: **one native-output DLAA replacement verified**, v1.17.0.
[Run 37072 result](first-inline-dlaa-result.md). The full frozen reviewer accepts
frame 22879 within capture 22875-22883. All 3,686,400 packed native pixels match
the SDK output; converted inputs and exact frame/jitter/recording/fence match.
Bindings restored, feature/parameters released, all hooks disabled, zero faults
or pending callbacks. The game process remained responsive. The user was away
during capture, then reported a normal scene and normal exit. All 68 run files
are preserved; the final 545-file archive SHA-256 is
`3f97e1381186b1e1e5e45cbaaed8e6e84d22642abf1c1aa049dcfc69b709c7e8`.
**Exact rollback completed at 04:02:05 +03; no next diagnostic is installed.**
The sidecar is removed and all four configurations remain unchanged. This is one reset
SDK frame, not continuous DLAA. Next: consecutive same-frame evaluations with
explicit queue ordering, resource reuse and retained SDK history.


Prepared 2026-09-26: **first same-frame DLAA handoff, v1.17.0**.
[Implementation, verification and bounded test contract](../runtime/inline-dlaa.md).
The native-list conversion, one DLAA Evaluate, output replacement and binding
restoration pass the 1440p RTX fixture with clean GPU validation. Missing state,
wrong output format and invalid jitter preserve native pixels. All 158 temporal
native tests pass. The first in-game run is pending: only receipt 4 of nine is
eligible for replacement, with a reset SDK history. Continuous DLAA remains
unverified. Evidence: `local/dlaa-inline-live-20260926/v1.17.0/`.

Completed 2026-09-26: **all nine native binding snapshots accepted**, v1.16.0.
[Run 36704 evidence and result](native-binding-snapshots-result.md).
Frames 23122-23130 match native root/PSO caches and exact Reset generations.
Compute tables 0/1 and CBVs 2/3/5/6; graphics tables 0/1/16 and CBV 5 were observed.
Nine offscreen DLAA outputs retain history and match their projection jitter.
All 29 hooks disabled with zero pending/faults; 1,337 journal events and 41 valid
depth markers. 232,243,200 converted floats and 99,532,800 SDK RGB values pass.
The user confirmed normal exit; exact rollback completed at 02:57:18 +03,
sidecar removed and all four configurations unchanged. Earlier PID 28908 never
armed during the service outage; its exit cause is unknown. Evidence:
`local/dlaa-bindings-live-20260926/v1.16.0/`, 486-file archive SHA-256
`4d6ac60d792ef702732746a40c0c8d5203eeb1e004fa7f9d0cc18f40c429f735`.
Earlier entries below are historical.


Prepared 2026-09-26: **output handoff implemented; native binding observation
is the next live prerequisite**, v1.16.0.
[Contract, verification and test procedure](../runtime/dlaa-output-handoff.md).
The private RGBA32-to-native-R11G11B10 conversion/copy and binding restoration
pass WARP and three RTX image fixtures with clean GPU validation. All 157
temporal native tests pass. The nine-frame recorder now captures the actual
compute/graphics bindings from Reset and compares the root/PSO against native
caches. This DLL retains offscreen SDK evaluation and makes zero presentation
writes. Same-frame SDK insertion and native output replacement remain to be
integrated after the live snapshots are accepted. Evidence/deployment:
`local/dlaa-bindings-live-20260926/v1.16.0/`. Earlier entries are historical.

Completed 2026-09-26: **nine-frame live DLAA passes full frozen verification**.
[Run 14468 result and image inspection](dlaa-nine-frame-accepted.md).
v1.15.3 frames 60795–60803 retain history after the first frame, with seven
paired jitter samples/restorations and two verified packet-address reuses.
The journal closes before 843 ms of hook removal: 1,335 events, no overflow,
all 39 depth markers valid, all hooks disabled and zero pending callbacks.
232,243,200 converted floats match the reference; 99,532,800 output RGB values
pass verification. One SDK feature and nine evaluations complete successfully.

The camera remained fixed. First/middle/final previews and four enlarged crops
are coherent, with edge smoothing and some texture softening. This is private
offscreen output; native TAA is still displayed. Next: bounded output conversion
and presentation with explicit GPU ordering/fallback, then continuous history,
real frame timing and lifecycle/temporal-quality tests.

At the user's explicit request, the game was terminated after the capture and
hooks settled. Exact previous DLL restored at 00:44:22 +03, sidecar removed,
four configurations unchanged. This was not a normal-exit test. No diagnostic
is newly installed. Evidence: `local/dlaa-sequence-live-20260926/v1.15.3/`,
722-file archive SHA-256
`2b3f963a6a96f3691fb85c57ea1213bac55f2a45ad41a04083d5e63c1de70618`.
Image inspection: `local/dlaa-sequence-analysis-20260926/v1/`.
Earlier entries below are historical.

Prepared 2026-09-26: **close journal admission before hook removal, v1.15.3**.
[Contract, evidence and test procedure](../runtime/dlaa-journal-cutoff.md).
The old ordering reproduces offline: 8,192 forwarded D3D calls expand the
journal from 46 to 4,096 events and fault. After the fix, both the simple
stress case and a paused-depth-callback case leave the journal byte-for-byte
unchanged while forwarding all later calls. An admitted D3D Close return also
completes correctly after cutoff. Dependency drain is bounded before removal;
timeout latches fault 17, and explicit shutdown timestamps bind the cutoff.

All 148 native tests and seven current/four historical combined reviewer groups
pass (13 tests per group, inapplicable cases skipped), along with 61 current
three-frame input, 15 SDK and seven startup-link tests. Positive WARP GPU
validation is clean with expected clear-value warnings counted. Earlier live
acceptance and both rejected longer runs retain their classifications. SDK
code and runtime are unchanged; the sealed SDK fixtures are reused.
Evidence/deployment: `local/dlaa-sequence-live-20260926/v1.15.3/`.
Next: same barebones entry, DLSS Off/TAA On; load a save, stay still/pose, verify
startup/build/process and arm. Nine private outputs and seven scoped jitter
samples remain the test. Live success and displayed DLAA are still pending.
Earlier entries below are historical.

Completed 2026-09-25: **v1.15.2 fixes missing depth markers; cleanup exhausts
the journal**. [Run 54036 result](dlaa-journal-capacity-result.md).
Nine native frames 35062–35070 completed with seven jitter restorations, all
42 depth markers valid, and both dependent observers drained before disable.
Frozen consumer, jitter, scene-depth and depth-route checks pass. Full capture
remains rejected: submission fault 2 at exactly 4,096 events, 343 ms after the
last selected completion. No typed conversion, saved pixels or SDK evaluation.
All hooks disabled, zero pending callbacks and reference counts balanced.

Next: stop admission and drain all dependent callbacks while hooks/journal
remain available, then close journal admission before any slow hook removal.
Exercise continued unrelated D3D traffic during teardown offline; preserve
the fixed journal budget and fail-closed checks. See the result for exact scope.
Normal exit confirmed; previous DLL restored at 23:58:35 +03, sidecar removed,
four configurations unchanged. No next diagnostic installed. Evidence:
`local/dlaa-sequence-live-20260925/v1.15.2/`, 537-file archive SHA-256
`fbf852c6d710965faf5119151bd03b93737f0c3fc36a1bf1854b7b596aa89824`.
Earlier entries below describe historical states.

Prepared 2026-09-25: **drain admitted callbacks before closing the submission
journal, v1.15.2**. [Contract and tests](../runtime/dlaa-journal-drain.md).
The offline WARP fixture reproduces v1.15.1's missing-tail-marker outcome by
cancelling after depth admission but before marker creation. The corrected
ordering keeps dependent hooks and journal available until callbacks drain;
any missing marker now fails closed immediately with a specific reason.
All 145 native temporal tests pass, including synchronized callback draining,
timeout, exception and missing-marker cases. Four current and three historical
combined fixture groups pass 12 reviewer tests each (inapplicable cases skip),
along with 61 current three-frame input, 15 SDK and seven startup-link tests.
The accepted v1.14 live run remains accepted, and v1.15.1 remains rejected.
SDK implementation and pinned runtime are unchanged; sealed SDK validation is
reused. No new live success or visible DLAA is claimed.

Evidence/deployment: `local/dlaa-sequence-live-20260925/v1.15.2/`.
Next: same barebones entry, DLSS Off/TAA On; load a save, remain still/pose,
reply from phone, verify exact startup/process and arm. Nine private outputs,
seven scoped jitter samples and retained history remain the diagnostic scope.
Archive and restore after normal exit. Earlier entries below are historical.

Completed 2026-09-25: **nine live DLAA outputs; full trace rejected on two
supplemental depth markers**. [Run 50324 result](dlaa-nine-frame-result.md).
v1.15.1 frames 22695–22703 complete with seven jitter injections/restorations,
two verified packet-address reuses, one feature and nine history-retaining SDK
evaluations. Ten frozen component checks pass, including all selected depth
readbacks, 232,243,200 exact converted floats and 99,532,800 output RGB values.
The frozen full reviewer still rejects `depth marker index`: two unselected
consumers from frame 22706 have no submission marker. That failure is preserved;
successful component checks do not promote this to a fully accepted run.

Next: reproduce the admission/drain race in recorder cancellation and correct
the missing-marker handling before another live run. The stop ordering is a
strong lead, but the log does not distinguish all marker-rejection branches.
Slight camera drift does not explain missing observer markers. Presentation
remains native TAA; continuous DLAA and temporal-quality validation are pending.
Normal exit confirmed; exact prior DLL restored at 23:33:49 +03, sidecar removed,
four configurations unchanged. No next diagnostic installed. Evidence:
`local/dlaa-sequence-live-20260925/v1.15.1/`, 411-file archive SHA-256
`ff8afc8e3b0f5ee869d5dad581f34f6d777b278c6343ea3a05181eed569c2d8a`.
Earlier entries below describe historical prepared/completed states.

Prepared 2026-09-25: **nine-frame recorder packet generations, v1.15.1**.
[Contract, tests and rollback](../runtime/dlaa-packet-generations.md).
The v1.15.0 run rejected frame 36350 because its packet address had already
been used in frame 36348, although that earlier consumer had completed.
[Full result](dlaa-packet-reuse-result.md): four injections restored, all hooks
disabled, no SDK evaluation, normal exit and exact DLL restoration complete.
Its 343-file archive SHA-256 is
`1d95ef4090e78a9175c41de490f24d6d5e1a5f038632fe299a81b470987161d1`.

v1.15.1 allows an address's next generation only after prior CPU consumption
finishes before the new producer starts. It keeps frame/epoch checks and
independent GPU completion, rejects same-generation duplicates and overlapping
lifetimes, and logs the full predecessor chain. 78 native tests and current
unique/reused-address GPU fixtures pass, with exact nine-frame conversions.
Current and historical reviewers pass; the SDK implementation is unchanged.
Evidence/deployment: `local/dlaa-sequence-live-20260925/v1.15.1/`.
Next: launch barebones with DLSS Off/TAA On, load a save, remain still/pose,
reply from phone, then verify startup and arm. The nine-frame live result is
still pending; no sustained visible DLAA is claimed. Older entries below
describe previous frozen states.

Prepared 2026-09-25: **nine-frame retained-history DLAA v1.15.0**.
[Contract and rollback](../runtime/dlaa-history-sequence.md), following the
[v1.14 image review](dlaa-history-image-review.md). Three-frame previews are
coherent and show smoother edges; strict alignment fits lacked sufficient
stable pixels, so no compensation-fit result is claimed. The unchanged first
frame is bit-identical to isolated reset replay. Analysis archive: 17 files,
SHA-256 `dbf64d17cbc6300d946e1c9598aa1715418cbfcbfea2686f9134405775a230a2`.

Next test: unchanged baseline, seven distinct scoped jitter samples, unchanged
recovery, all adjacent, one feature and nine evaluations with reset only first.
56 native checks and five clean RTX fixtures pass. Current reviewers accept
99,532,800 output RGB values from the synthetic longer fixture, reject rehashed
NaN and accept the prior v1.14 live capture. Adversarial checks cover nine-frame
completion, late restoration failure, duplicate jitter, skipped frames,
ownership and invalid SDK histories. The SDK-free recorder remains three-frame.
Evidence/deployment: `local/dlaa-sequence-live-20260925/v1.15.0/`.
Launch barebones with DLSS Off/TAA On, load a save, stay still/pose and reply
from the phone; validate startup and exact process before arming. A longer live
result is pending. Actual timing, production jitter/resets, controlled output
presentation and sustained visible DLAA remain unfinished. Earlier entries
below describe previous frozen states.

Completed 2026-09-25: **first bounded live DLAA history and paired jitter**.
[Accepted run 49200, evidence and limits](first-live-dlaa-history.md).
Frames 23398–23400 pass the frozen full v1.14 reviewer: one scoped jitter
injection/restoration, exact projection/TAA association, 77,414,400 converted
input floats and 33,177,600 output RGB values verified. SDK resets only frame
one and receives the measured middle-frame pixel offset. Startup ownership,
one CreateFeature, three EvaluateFeature calls and cleanup all pass; no extra
Init or borrowed-device Shutdown. Eleven capture hooks disabled, no faults
or pending callbacks. Game remained responsive; user confirmed normal exit.
Previous DLL restored at 22:23:10 +03, sidecar removed, four configurations
unchanged. No next diagnostic installed. Archive has 279 files, SHA-256
`9ba9715e7df45595b33b5145bfe957a8c18ae87f37004c9b86d5863aff2cb87c`.
Evidence: `local/dlaa-history-live-20260925/v1.14.0/`.
Next: inspect retained-history image behavior, then extend bounded accumulation
and controlled presentation. Actual timing, continuous jitter and production
resets remain. This three-frame offscreen result is not sustained visible
DLAA or a temporal-quality certification. Earlier entries below are historical.

Prepared 2026-09-25: **bounded retained-history DLAA v1.14.0**.
[Contract, verification and rollback](../runtime/retained-history-dlaa.md).
Three adjacent projection-associated input sets feed one private feature with
reset true/false/false and measured projection offsets converted to pixel
coordinates. Native stimulus is the existing one-frame injection/restoration.
Startup ownership, converted inputs and SDK cleanup remain established.
43 native checks, four clean RTX fixtures, combined/capture/startup/SDK
reviewers and full output/corruption checks pass. Offline retained-history
outputs differ after the first frame, including the zero-jitter control;
this does not establish live temporal quality. Fixed diagnostic timing is
explicit, and presentation remains native TAA. Live v1.14 result is pending.
Evidence/deployment: `local/dlaa-history-live-20260925/v1.14.0/`.
Next: seal/install, launch barebones with DLSS Off/TAA On, load a save and
remain still (stationary character pose preferred), then arm after exact
startup/process checks. No toggle. Archive and restore after normal exit.
Earlier entries below are historical.

Completed 2026-09-25: **first live offscreen DLAA outputs, v1.13.0**.
[Accepted run 47780, evidence and limits](first-live-dlaa-output.md).
Startup capabilities and native Init pass; retained ownership admits the
evaluator without another Init. Frames 17950–17952 yield one successful feature
creation, three successful DLAA evaluations, completed GPU work and successful
feature/parameter release. All 77,414,400 converted input floats match exactly;
all 33,177,600 output RGB values pass the full frozen review. Native hooks are
disabled with zero faults/pending callbacks. Game remained responsive and
closed normally; previous DLL restored, four configurations unchanged.
Archive: `local/dlaa-startup-live-20260925/v1.13.0/`, 279 files, SHA-256
`dbaaa17185b546464d75f2919f4a18f015bd4abb77f669cbe7cf283cce1da339`.
**Next: continuous history with correctly paired projection jitter, then
controlled output presentation.** This reset-per-evaluation, zero-jitter
control leaves native TAA presented; temporal quality and working visible
DLAA remain unproven. No next diagnostic installed. Preserve the working
startup ownership and input path rather than repeating their discovery.
Earlier entries below are historical.

Prepared 2026-09-25: **startup-owned live DLAA v1.13.0**.
[Handoff, tests and rollback](../runtime/startup-live-dlaa.md).
Startup v1.12.1 passed live: [accepted run 26296](startup-sdk-capability-result.md).
All four gates became ready after 547 ms; public and native Init succeeded,
capabilities stayed `(1,0,1)`, hook disabled, normal exit and exact rollback
verified. Its 90-file archive SHA-256 is
`ec9e90a5e3491933a1e14b75b45c835378e01dd40d29f380f15b123382842ff4`.
The next build reserves the native hook pool first, runs that startup
transaction, and exposes capture readiness only after full startup acceptance.
Evaluation reuses the retained SDK/device through explicit ownership checks,
with no repeated Init or borrowed-device Shutdown. Unknown ownership fails.
Fourteen current-build hardware fixtures pass; the three retained-startup
DLAA outputs exactly match fresh initialization. Focused native, capture,
startup/SDK reviewers and full output/corruption checks pass.
Next: after installation/prelaunch sealing, launch the same barebones entry
with DLSS Off/TAA On, load one save and stay still. The agent arms the three
private-output capture after verifying startup. Presentation remains TAA;
live DLAA evaluation and continuous temporal quality are still unproven.
Evidence: `local/dlaa-startup-live-20260925/v1.13.0/`.
Earlier entries below are historical.

Prepared 2026-09-25: **NGX startup readiness correction v1.12.1**.
[Startup result, guard contract and rollback](../runtime/ngx-startup-capabilities.md).
The v1.12.0 run (PID 46468) rejected `startup-live-code` before installing
any hook or attempting public SDK initialization. Later read-only comparison
found all four regions matching exactly (844 bytes), with native dispatch
already populated. The initial mismatch was not recorded; early readiness
is a supported hypothesis, not a proven cause. Normal exit, exact previous
DLL restoration and four unchanged configurations are verified. Its immutable
87-file archive SHA-256 is
`aa5a7b700264068d17ed00b3bd8c241bf6287b7adafbbc0b8d34a915979bc119`.
Version 1.12.1 waits up to five minutes for those same four code hashes while
requiring seven readable zero native slots; populated slots or expiry reject
before installation. A versioned readiness receipt exposes gate masks and
timing. Release /W4 /WX build, focused C++ admission controls and 15 Python
reviewer tests pass. Shared hook/SDK code is unchanged from the five hardware
fixtures. Capability-only: no feature creation, evaluation or rendering change.
Next: after deployment and prelaunch sealing in
`local/ngx-startup-20260925/v1.12.1/`, launch the barebones entry with DLSS Off
and TAA On, reach the main menu only. Recording starts automatically.
Earlier entries below are historical.

Prepared 2026-09-25: **NGX startup capabilities v1.12.0**.
[Boundary, tests and rollback](../runtime/ngx-startup-capabilities.md).
Mapped the executable's public client Init at +0x273F230, called from
+0x23842D7 during D3D12 renderer startup (API 18). One hash-gated early hook
attempts our public project-ID initialization with the modern directory
before forwarding the original call unchanged. Exact capability values before
and after, native result, adapter and later swap-chain description are logged.
One real-hook hardware fixture plus four admission controls pass; original
failure status/arguments preserved, second calls forward and disabling works.
Headless swap-chain identity/format query and nine reviewer tests pass.
This is capability-only: zero feature creation/evaluation, no borrowed-device
Shutdown, no visible rendering change requested. Live coexistence is unproven.
Evidence: `local/ngx-startup-20260925/v1.12.0/`; offline map/fixtures in sibling
`v1/`. Next: launch the barebones test entry with DLSS Off/TAA On and reach
the main menu. Recording is automatic; no arm command, toggle or save load.
Earlier entries below are historical.

Closed 2026-09-25: **live SDK v1.11.0 input path passes; SDK capability fails**.
[Live result and isolated initialization-order controls](live-sdk-capability-result.md).
Run 52904, frames 29787–29789: all native input reviewers pass and all
77,414,400 converted floats match exactly. SDK Init/Get calls succeed, but
the capability predicate fails before Create/Evaluate. Original value logging
was incomplete; no exact live error code is claimed. Hooks disabled, normal
exit confirmed, previous DLL restored and all four configurations unchanged.
Live archive: 260 files, SHA-256
`e1ba2c7075d92d4a23bf5b40ebc31a3c32dd49dc88bd5277ac8f7c8cbf2bec9c`.
An isolated public-API probe reproduces unavailable/FeatureNotFound when NGX
first initializes without the modern directory; a later Init does not fix it.
Modern-first works; shutdown/reinit works only in our exclusively owned fixture.
Legacy-directory-first crashes during Init; evidence preserved, cause unproven.
SDK logger v2 now records actual values; shared-code hardware outputs remain
byte-identical, validation clean, 11 reviewer tests pass for each log version.
Offline archive: `local/ngx-capability-20260925/v1/`, 35 files, SHA-256
`2b7d2d0f1c86eb348715a697548a18baa77bc7b7dde70695b379b66b5f863a9d`.
**Next: map initial game NGX setup/provider timing**, then a new versioned
diagnostic with exact capability values. No next DLL installed; game closed.
Do not repeat input discovery or shut down the game's borrowed SDK state.
Earlier entries below are historical.

Prepared 2026-09-25: **live offscreen DLAA v1.11.0**.
[Ownership, provider, test and rollback contracts](../runtime/live-dlaa-sdk.md).
Optional `mhw_temporal_dlss_probe` keeps the accepted native observation/copies
and conversion, then evaluates the three private GPU sets on the game device
after hooks are disabled and producer/conversion completion is proven.
One public SDK feature, three reset requests, private output, no visible TAA
replacement or jitter writes. ReleaseFeature/DestroyParameters follow final
retirement; borrowed-device SDK initialization is retained until process exit.
Offline same-code hardware fixture passes alongside original DLSS 1.1.13.0,
with clean GPU validation. Four unsafe-input controls reject before SDK Init.
Middle reused-feature output differs slightly from the prior fresh-feature
control; cause unresolved and recorded, no temporal-quality claim.
Evidence: `local/dlaa-live-20260925/v1.11.0/`. Next: after deployment/prelaunch
verification, launch the existing barebones test entry with DLSS Off/TAA On,
load one save and stay still. Arm only after exact process/DLL checks.
Earlier entries below are historical.

Completed 2026-09-25: **first offscreen DLAA SDK output**.
[Contract, evidence and limitations](first-dlaa-sdk-output.md).
Public NGX API 21 / signed DLSS 310.7.0.0 provider evaluates all three converted
frames from accepted live-owned run 51256. Native 2560x1440 DLAA, preset K
requested, fresh process/feature and reset history per frame. Initialization,
creation, evaluation, three fences/retirements and SDK cleanup succeed; D3D12
debug/GPU validation show zero errors/warnings. Independent review verifies
all 33,177,600 RGB values are finite and overwritten. Scene previews are intact.
Evidence: `local/dlaa-sdk-replay-20260925/v1/`; source capture remains unchanged.
Sealed `archive.json`: 30 files, SHA-256
`fd774992068b82cf6b1ec59b6f47a2f5551242275a57f8127f840ba3fda5a973`.
Preserved harness SHA-256
`ea69a9705771f7252893a05ae7555621a5cbcdfa2174317f9e413fc92257b9c4`.
Final environment check confirms four configurations unchanged, original DLL
hash restored, no output sidecar, game closed and signed SDK checkout unchanged.
Three relevant CTest entries pass. No game access or deployment; previous MO2
DLL still restored. **Next: public SDK evaluation on live-owned GPU textures
on the game's device, initially offscreen**, then continuous jitter/history
and output replacement. Zero-jitter reset replay does not certify temporal
quality, live exposure/jitter parameters, NGX coexistence or in-game rendering.
Earlier entries below are historical.

Accepted and closed 2026-09-25: **live owned GPU inputs v1.10.0, run 51256**.
[Result, evidence and next step](live-owned-temporal-inputs-result.md).
Frames 25383–25385: native depth/color/motion copied directly to private GPU
textures, qualified by exact producer generations/completion/retirement,
then converted on private lists with explicit views. All **77,414,400 float
values** match independent references exactly; depth also byte-identical.
All three conversion fences complete and private recordings retire; native
states restored, bindings unchanged, all hooks disabled and faults zero.
Nine converted files 309,657,600 bytes plus nine raw files 132,710,400 bytes.
Archive: `local/temporal-owned-live-20260925/v1.10.0/archive-51256.json`,
232 files, SHA-256 `017f8520effcb35986ff794b811155d0fb1d0930e5a34978ec7e0933b3b824b5`.
205 prelaunch files/142 sources verify. User confirmed normal exit; previous
DLL restored 19:17:03 +03, sidecar removed, all four configurations unchanged.
No next diagnostic installed/armed. **Next: first bounded native-resolution
SDK evaluation on the private GPU set, with explicit provider parameters and
private output.** SDK output, continuous history and presentation remain
unverified; no additional resource inventory run is required by this result.
Earlier entries below are historical.

Prepared 2026-09-25: **live owned GPU copies/conversion v1.10.0**.
[Native/worker contracts and test procedure](../runtime/temporal-owned-inputs.md).
The existing raw readback intervals now additionally snapshot each selected
source into a private GPU texture. No added hooks; each source is restored.
All original generation, producer-completion and retirement checks must pass,
and nine raw references must save, before the worker converts the private set
on private lists/queues with explicit SRVs/UAVs. No native pipeline bindings
change. Outputs remain private; no SDK calls or visible DLAA yet.
Seven new WARP fixture cases pass, including exact converted pixels and
rejection before conversion on incomplete evidence. Python review independently
compares every output value and rejects corrupted/rehashed pixels.
The in-game handoff remains unverified until the next capture.
Evidence/deployment directory: `local/temporal-owned-live-20260925/v1.10.0/`.
Next action: launch the existing barebones test entry with DLSS Off/TAA On,
load one save, stay still, then arm after checking the exact process generation.
Prior entries below describe historical state.

Completed 2026-09-25: **owned GPU input replay v1**.
[Implementation, verification and live handoff decision](owned-temporal-gpu-inputs.md).
New standalone C++20 library/executable creates explicit SRVs for R11G11B10
color, R32_FLOAT depth and R32_UINT packed motion, converting motion to the
tested current-to-previous pixel offset in R32G32_FLOAT. Two new CTest entries
pass (pure contracts and two WARP shapes). All three full-size pan frames replay
on the captured adapter 0:97816: 77,414,400 GPU float values match C++ and NumPy
references exactly, depth byte-identical, debug/GPU validation clean, fences
completed and recording allocators retired. No game process access, hook/DLL
changes, deployment or SDK calls. Prior exact MO2 slot remains restored.
Evidence: `local/temporal-gpu-inputs-20260925/v1/`, with native logs and
`hardware/verification.json`. Source pan archive remains intact (188 files).
Decision: use owned explicit views; opaque native descriptor internals need
not gate this path. Next implement bounded live GPU copies at the separately
verified depth-copy and TAA color/motion intervals, restore original states,
join exact frame/generation receipts and require producer completion before
private-list conversion/evaluation. That live handoff and SDK output are still
unverified. Keep first output private; presentation/continuous history are later.
Earlier preparation/next-action entries below are historical.

Accepted and closed 2026-09-25: **v1.9.0 camera-pan run 31044**.
[Result and scoped timing conclusion](ordinary-taa-depth-pan-result.md).
Unchanged recorder accepts frames 22592–22594 and all nine depth/color/motion
files. User clarified diagonal right/up panning; the stimulus is usable.
Final static-scene masks exclude actors and water: all 12 qualified frame/region
pairs peak at zero integer offset and prefer matching color over both other
captured frames (ratios 1.42x–7.06x). Three bench cases lack enough remaining
edges and are unqualified. Current depth/color content timing is supported in
this scene. Neighboring motion fields remain too similar for this edge metric
to identify them consistently; earlier run 43820 supplies independent motion
calibration. Original/revised analysis files are both preserved.
1,340 events, completion +218 ms; all faults/pending zero, hooks disabled;
24/3/54/303 metadata references released. User confirmed normal exit; previous
DLL restored 18:09:05 +03, sidecar removed, four configurations unchanged.
147 prelaunch files and 132 frozen sources verify. No next test installed/armed.
Evidence: `local/temporal-depth-pan-20260925/v1.9.0/`, final interpretation
`timing-analysis-31044-v2.json`. Broad review flags and DLAA-ready remain false.
Next: typed input views and state/lifetime at the proposed evaluation point;
consider an explicitly owned depth copy if the original candidate's later
state cannot be certified. No further depth-timing rerun is required by this
result. Earlier preparation/next-action entries below are historical.

Prepared 2026-09-25: **camera-pan reuse of unchanged v1.9.0**.
[Procedure and analysis controls](../runtime/temporal-depth-pan.md). Fresh
evidence: `local/temporal-depth-pan-20260925/v1.9.0/`. Reuse the exact archived
DLL and source/reviewer snapshot from accepted stationary run 25300; verify
its complete 207-file archive and current host/configuration identities.
Existing 115 native / 55 Python checks (nine jitter-only skips) are inherited,
not rerun. Deployment/prelaunch receipts establish installed status. Nothing
armed during preparation. Next user step once sealed: same bare-profile test
entry, DLSS Off/TAA On; load the save and continuously pan at moderate speed,
reply from phone with Panning. Require motion to discriminate same-frame depth
from adjacent-frame alternatives. Freeze captures, request normal exit, then
restore and archive. Typed views/evaluation-point state and SDK work remain
pending. Earlier preparation/next-action entries below are historical.

Accepted and closed 2026-09-25: **v1.9.0 depth/color/motion run 25300**.
[Result and limits](ordinary-taa-depth-pixels-result.md). The frozen reviewer
accepts three full-resolution sets for frames 11884–11886: nine files totaling
132,710,400 bytes. Matched native-copy depth readback, COPY_DEST restoration,
exact TAA generation, submission, completion and retirement all verify. All
pixels are finite; depth is non-flat and silhouettes match the scene. Offline
edge agreement peaks at zero pixel offset in all 15 tested frame/region pairs.
This supports spatial agreement, not subpixel or temporal calibration from a
stationary scene. Final writer, later injection-point state, typed views and
SDK evaluation remain unverified. 1,145 events, completion +219 ms, all faults
and pending zero; hooks disabled and 24/3/45/261 metadata references released.
User confirmed normal exit; previous DLL restored at 17:51:41 +03, sidecar
removed, four configurations unchanged. All 174 prelaunch files and 132
frozen sources are verified in the closure. No next test installed/armed.
Evidence: `local/temporal-depth-pixels-20260925/v1.9.0/`.
Next: reuse this recorder for a controlled camera pan to distinguish current
depth from a stale frame, then validate typed views and the evaluation-point
state/lifetime contract. No new capture code is needed for the pan.
Earlier preparation and next-action entries below are historical.

Prepared 2026-09-25: **v1.9.0 paired depth/color/motion pixels**.
[Contract and procedure](../runtime/temporal-depth-pixels.md). After a native
copy matching a selected TAA frame, pure source COPY_SOURCE and destination
COPY_DEST qualify one bounded destination readback, with original-state
restoration. Later TAA must match the exact command-list/allocator/Reset
generation before the existing fence and retirement can qualify its pixels.
Ten hooks, nine images, 27 observer GPU commands, three private Signals, 144 MiB
allocation bound; no projection writes or SDK calls. 115 native / 55 Python
checks pass (nine jitter-only skips), including exact WARP depth pixels and
negative state/selection/duplicate/retirement/map cases. Runtime pending.
Fresh evidence: `local/temporal-depth-pixels-20260925/v1.9.0/`; installation and
prelaunch receipts are authoritative. Nothing armed. Next after sealing: same
bare-profile MO2 entry, DLSS Off/TAA On, load save, keep camera still, optionally
pose, reply from phone. After capture exit normally, archive and restore.
Depth content alignment, typed views and SDK integration remain unverified.
Earlier preparation/next-action entries below are historical.

Closed 2026-09-25: **v1.8.0 depth-copy run 45404**, with a qualified partial result.
[Evidence and review distinction](ordinary-taa-depth-routes-result.md). The
recorder completed; the frozen full reviewer rejects incomplete unselected
frame-12668 generations. A separate tested audit verifies all nine copies in
selected frames 12665–12667. Each frame connects original R32_TYPELESS depth to
the selected R32_FLOAT candidate through three copies, including one on the
same list before TAA Dispatch (source 0x800, destination 0x400). Earlier stages
use source 0x840. Do not infer final writer or whole-batch serialization.
Six color/motion files verify; 1,108 journal events, completion +203 ms, all
faults/pending zero and hooks disabled. 24/3/36/255 metadata references released.
Original full-review rejection is preserved; supplemental scope has 50 Python
tests (nine jitter-only skips). No runtime code changed after capture.
User confirmed normal exit; exact DLL restored 17:26:32 +03, output sidecar
removed, four configurations unchanged. Evidence directory remains
`local/temporal-depth-routes-20260925/v1.8.0/`. No next test installed/armed.
Next: bounded depth-pixel readback at the matched copy on TAA's list with
explicit state restoration and existing completion witnesses, paired to color
and motion. Actual depth alignment, typed views and SDK work remain pending.
Earlier preparation and next-action entries below are historical.

Prepared 2026-09-25: **v1.8.0 native depth-copy routes with paired color/motion**.
[Contract and procedure](../runtime/temporal-depth-routes.md). Three added hooks
connect the selected depth candidate to its original copy request, native
CopyResource, local barriers and Reset/Close/Execute generation. Repeated copies
per frame are retained and reported individually; batch position is not a
whole-list serialization proof. Ten hooks, zero projection writes, no added
depth commands or signals; existing three color/motion pairs remain bounded.
108 native and 49 Python tests pass (nine jitter-only skips). Runtime pending.
Fresh evidence: `local/temporal-depth-routes-20260925/v1.8.0/`; deployment and
prelaunch receipts establish whether installation is complete. Not armed.
Next after sealing: same bare-profile MO2 entry, DLSS Off/TAA On; load a save,
keep camera still, optionally pose, reply from phone. Final writer, actual depth
pixels/alignment, typed views and DLAA remain unverified. After capture, request
normal exit, archive and restore the previous DLL. Earlier next actions below
describe their historical stages.

Accepted and closed 2026-09-25: **v1.7.0 depth-candidate run 54612**.
[Result and limits](ordinary-taa-scene-depth-result.md). Frames 14157–14159 save
six verified color/motion files. The separate view+628/container+A0 candidate is
stable through producer/consumer checks: full-size R32_FLOAT, ALLOW_RENDER_TARGET,
one mip/slice/sample, native SRV address observed. It differs from the previously
validated R32_TYPELESS depth buffer; ordinary TAA still binds its 1x1 fallback.
Current attachment is null and this candidate has no observed transition in
the matched TAA consumer: state, contents and depth frame alignment remain unknown.
All capture faults/pending zero, hooks disabled; 24 producer and 242 lifecycle
references released, 1,016 events, final completion at +204 ms. User confirmed
normal exit; previous DLL restored 16:50:07 +03, temporary sidecar removed,
four configurations unchanged. Evidence: `local/temporal-scene-depth-20260925/v1.7.0/`.
Next: associate this exact owner with the mapped native depth-copy destination,
source/frame/submission and a valid copy-state interval, then qualify paired
depth contents. R32_FLOAT destinations were seen in earlier native copies; this
candidate's membership remains unproven. Typed views and SDK evaluation still
pending. No next test installed or armed. Earlier preparation entries below
are historical.

Prepared 2026-09-25: **v1.7.0 scene-depth ownership with paired color/motion**.
[Contract and procedure](../runtime/temporal-scene-depth.md). The selected view's
separate depth getter is now mapped and hash-gated. Three consecutive TAA
requests add pinned depth-candidate metadata, producer/consumer owner checks
and recording-local barrier replay. The 1x1 named TAA depth binding remains
separate. Seven hooks, zero projection writes, existing six color/motion copies;
zero depth copies or descriptor writes. 93 native tests and 43 Python tests pass
(nine jitter-only cases skip on the new fixture). Runtime result pending.
Fresh evidence/deployment: `local/temporal-scene-depth-20260925/v1.7.0/`;
installation/seal receipts are authoritative. Nothing has been armed.
Next user action after installation/seal: existing bare-profile MO2 test entry,
DLSS Off/TAA On; load a save, keep camera still, optionally pose, reply from phone.
After capture request normal exit, archive and restore the previous DLL.
Typed views and actual same-frame scene-depth contents remain unverified. Do not
use owner equality alone as SDK eligibility. Earlier next-action entries below
refer to their historical runs.

Accepted and closed 2026-09-25: **v1.6.1 larger jitter run 44552**.
[Result and limits](ordinary-taa-strong-jitter-result.md). Frames/epochs
15538–15540 save six verified color/motion files. Two selection attempts, one
weak preview skipped, one injection/restoration. Prediction (+0.1953125,+0.1697531)
raster pixels; camera/unjittered matrices and baseline/recovery VP are identical.
All ten eligible original color fits have predicted direction; two strict 5×5
masks fail unchanged coverage limits. Original fits underestimate; exploratory
brightness-aware tile fits give medians within ~0.01–0.025 px per axis, with
material regional/method variation. Precise color scale remains unverified;
do not retune matrix-derived jitter. Static-motion exclusion reproduces: ~98%
of exact-control selected pixels stay zero through stimulus and recovery.
Capture faults/pending zero, hooks disabled, 966 events, 233 lifecycle refs
released, no events after completion. Evidence: `local/temporal-jitter-20260925/v1.6.1/`,
`analysis-44552.json`, `jitter-analysis-44552-v2.png`. 86 native/41 Python tests
passed before deployment. User confirmed normal exit; previous DLL restored at
15:39:58 +03, sidecar removed, four configs unchanged; 157 prelaunch/126 source
files verify. No next test installed/armed. The user offered a stationary pose
for future checks; this run did not assume one.
Next: audit actual typed bindings and connect real scene depth to the same
color/motion frame for bounded DLAA input assembly. Keep the 1×1 named TAA depth
fallback out of the SDK input. Preserve color photometric uncertainty as an
integration check; no SDK evaluation, continuous jitter or DLAA output yet.
Earlier preparation/next-user-action entries are historical.

Prepared 2026-09-25: **v1.6.1 larger native jitter selection**.
[Contract and procedure](../runtime/temporal-jitter-selection.md). Before
admitting baseline, preview the next native-table entry and require absolute
components at least 0.3125 (0.15625 raster pixels each). Up to twenty eligible
previews, within the original two-second/4,096-event bounds. Weaker previews
forward untouched; a selected candidate must match exactly before injection.
All eight hooks, three consecutive paired samples, one injection/restoration,
six copies and ownership rules remain unchanged. 86 native/41 Python tests pass.
Fresh deployment/evidence: `local/temporal-jitter-20260925/v1.6.1/`; previous
v1.6.0 remains sealed. Runtime result pending, no arming before loaded-save reply.
Next user action once installation/seal is complete: existing bare-profile MO2
test entry, DLSS Off/TAA On, load save and hold camera/character still; reply
Save loaded from phone. No pan or DLSS toggle. After verified capture, request
normal exit and restore using v1.6.1's deployment script. Earlier no-test-installed
and closure statements below refer to the preceding run.

Accepted and closed 2026-09-25: **v1.6.0 jitter/color/motion run 35156**.
[Result and measurement limits](ordinary-taa-jitter-result.md). Frames/epochs
26684–26686 have matched/acknowledged projection samples and six verified input
copies. One native injection and one restoration, no faults/pending; hooks off.
Predicted raster shift is (+0.046875, −0.064815) pixels. Camera and unjittered
matrices are bit-identical. All twelve control-selected color fits have the
predicted direction; exact-center fits measure about (+0.032…0.036, −0.048…0.054).
Known quantized/resampled shifts also underestimate, so precise color-scale
agreement remains unresolved and no correction factor is inferred.
Roughly 98–99% of those pixels have exactly zero motion in baseline, stimulus
and recovery, supporting jitter exclusion in the selected static majority.
This is scoped pixel evidence, not broad motion certification or working DLAA.
76 native/38 Python tests passed before deployment. Final evidence/plot:
`local/temporal-jitter-20260925/v1.6.0/analysis-35156.json`,
`jitter-analysis-35156.png`. Startup black screen recovered before arming;
read-only inspection found no active hooks, but its cause is undetermined.
Normal exit confirmed; previous DLL restored at 15:04:14 +03, temporary sidecar
removed, all four configurations unchanged; 152 prelaunch/124 source files verify.
No next test is installed or armed. Next: deliberately larger native-table
jitter sample to tighten color-displacement evidence, preserving control frames.
Same-frame scene depth, actual typed views and bounded SDK output remain needed
for DLAA. Historical prepared/next-user-action entries below are superseded.

Prepared 2026-09-25: **v1.6.0 bounded projection jitter + color/motion pairs**.
[Contract and procedure](../runtime/temporal-jitter-pairs.md). Adds the previously
validated projection-helper hook to v1.5's paired input copies. Three consecutive
samples are baseline, one native-table jitter injection, zero recovery. Each
must match/acknowledge the exact TAA frame/epoch/context/view before advancing.
Restoration runs during exception/cancellation; any missing invariant rejects
qualification. Independent review checks five matrices, native jitter bits,
provider/viewport, time and frame association. This is CPU source association;
the earlier full draw/CBV provenance chain is not repeated or implied here.
76 native and 38 Python tests pass; runtime result pending.
Fresh local evidence/deployment: `local/temporal-jitter-20260925/v1.6.0/`.
Next user action after installation/seal: launch the existing bare-profile MO2
entry, DLSS Off/TAA On, load the same save, keep character/camera stationary,
reply Save loaded from phone. No pan or DLSS toggle. After capture, verify and
request normal exit, then restore using this new directory's deploy script.
Earlier closure statements below refer to previous runs.

Accepted and closed 2026-09-25: **v1.5.0 paired color/motion run 43820**,
[result and quantitative limits](ordinary-taa-paired-result.md).
Frames 18042–18044 each save verified full-resolution current color and motion.
Independent image-only matching qualifies 109 and 122 static patches in the two
adjacent image pairs. Median disagreement from the same-frame decoded vectors
is 0.118/0.122 pixels; native scale and the shader-derived signs fit substantially
better than zero motion, sign/axis changes and the neighboring-frame vectors.
The matcher uses quarter-pixel refinement: this is one-scene quantitative
alignment support, not universal accuracy, jitter inclusion or working DLAA.
All six copies have successful restoration, submission, completion and retirement.
No faults/pending; hooks disabled, 1,370 journal events, none after completion.
Normal exit confirmed; previous DLL restored at 14:33:33 +03, temporary sidecar
removed, all four configuration hashes unchanged. Evidence:
`local/temporal-pairs-20260925/v1.5.0/`; frozen 142-file prelaunch manifest and
115 source files verify. Analysis and final plot: `analysis-43820.json`,
`alignment-43820-v2.png`. No next test is installed or armed.
Next: combine the bounded projection-jitter stimulus with paired color/motion
capture, using stationary baseline/jitter/recovery images to determine color
jitter propagation and whether motion includes its displacement. Same-frame
depth, typed views and bounded native-resolution SDK output remain integration
requirements. Earlier preparation/closure statements below are historical.

Prepared 2026-09-25: **v1.5.0 matched current-color/motion readback**.
[Contract/procedure](../runtime/temporal-paired-readback.md).
Three consecutive native TAA requests each copy R32_UINT motion and
R11G11B10_FLOAT current color before original Dispatch, restoring 0xC0 and
0x40 respectively. Six bounded buffers retain issued references until exit;
all pairs require completed submission and retirement before mapping. The
verifier checks pair identities, order, distinct allocations and all six files.
58 native and 32 Python tests pass, including exact WARP/SDK-packed RGB data.
Game paired pixels and quantitative alignment remain pending. New local
deployment and rollback receipts: `local/temporal-pairs-20260925/v1.5.0/`.
Next: existing bare-profile test entry, DLSS Off/TAA On, loaded save, gentle
rightward camera pan; arm only after the user confirms panning. Compare actual
scene displacement against decoded vectors after all captures are verified.
Earlier closure statements below refer to previous experiments.

Accepted and closed 2026-09-25: **v1.4.0 right-pan run 34276**,
[camera-pan result](ordinary-taa-motion-pan-result.md).
Frames/epochs 17696–17698 pass all three full-resolution R32_UINT readbacks
using the unchanged tested DLL/reviewer. All pixels decode to finite nonzero
vectors, versus ~6.9% in the stationary run; 94.28–94.39% have the predicted
positive history-X sign. The maps show coherent scenery motion. This supports
camera-motion coverage/direction in this view; scale/frame alignment are not
yet independently calibrated. No faults/pending, hooks disabled; 1299 journal
events and none admitted after final completion. Normal exit confirmed; prior
DLL restored, sidecar removed, all four configurations unchanged.
Evidence: `local/temporal-motion-20260925/v1.4.0-pan-right/`.
123 prelaunch files, 112 archived sources and the prior 151-file archive verify.
Next: implement matched current-color/motion copies for the same frame sequence,
then compare actual scene displacement to decoded vectors. Jitter inclusion,
typed views and sustained DLAA integration remain separate checks. No next
test is installed or armed. Preparation statements below are historical.

Next runtime test prepared 2026-09-25: **v1.4.0 steady rightward camera pan**.
Reuse the exact tested motion-readback DLL from run 48724; no native code or
reviewer changes. Fresh deployment/rollback receipts and capture output belong
to `local/temporal-motion-20260925/v1.4.0-pan-right/`. Verify that deployment
receipt before arming; the closed stationary directory must remain sealed.
Use the existing bare-profile test entry, DLSS Off/TAA On, same save/scene.
After the user reports the save loaded, ask for a gentle, steady camera rotation
to the right with the character stationary. Arm only after the user confirms
the pan is underway; maintain the pan until capture is acknowledged complete.
Replies should come from the phone to preserve game focus. A clear view of
static terrain/buildings helps distinguish background coverage. The expected
history displacement for ordinary yaw-right/background scrolling left is
positive X under the shader decode; this remains a hypothesis to check against
the user's observed screen movement. No new runtime result exists yet.
After capture, preserve/review artifacts, request normal exit, and restore the
previous DLL. Prior closure statements below describe the stationary run.

Accepted and closed 2026-09-25: **v1.4.0 run 48724**,
[first motion-pixel result](ordinary-taa-motion-result.md).
Three 2560×1440 R32_UINT images (frames/epochs 12257–12259) pass frozen review:
matched native Dispatch, exact source identity/state, successful restoration,
submission/completion/retirement, exact pixel lengths and hashes. All decoded
vectors are finite; 6.86–6.91% are nonzero and form structured animated regions.
This is a stationary-camera request, so camera-motion coverage remains unproven.
No faults/pending; hooks disabled. Early lifecycle admission closure reduced
this journal to 1022 events, zero after final completion. Issued copy objects
were retained until normal exit. Prior DLL restored, sidecar removed, all four
configurations unchanged. 133 prelaunch files and 111 archived sources verify.
Evidence: `local/temporal-motion-20260925/v1.4.0/`; no new test is installed.
Next: fresh deployment of the unchanged tested recorder for a steady horizontal
camera-pan capture, then compare background motion and direction. Typed views,
constant-buffer mode, jitter inclusion, broad motion semantics and DLAA remain
unverified. Preparation statements below are historical.

Prepared 2026-09-25: **v1.4.0 bounded motion-pixel readback**.
[Contract and procedure](../runtime/temporal-motion-readback.md).
Three exact R32_UINT copies precede matched native TAA Dispatch calls; each
restores the original state and requires independently reviewed submission,
completion and retirement before mapping. Issued references remain retained
until process exit. Lifecycle admission now stops before hook removal.
All 49 native and 26 Python tests pass, including real WARP copies with
byte-for-byte, row-padding and rejection checks. No game pixels captured yet.
Fresh preparation, installed identity and rollback receipts:
`local/temporal-motion-20260925/v1.4.0/`. Next: launch the existing bare-profile
test entry, DLSS Off/TAA On, load a save, keep camera still, then arm once.
Prior deployment statements below describe historical, closed runs.

Accepted and closed 2026-09-25: **v1.3.0 run 14268**,
[TAA submission/completion result](ordinary-taa-submission-result.md).
Frames/epochs 26815–26817 match three native Dispatch calls, three distinct
Reset/allocator generations, and three submissions on the same DIRECT queue.
Each private fence completed at value one, and each generation has a later
successful retirement Reset with no intervening resubmission. The unchanged
archived reviewer accepts submission, completion and retirement. All GBuffer4
states remain `0xC0`; the shader is byte-identical to run 4376. No pixel copies.
Zero faults/pending, all hooks disabled; 21 resource, three consumer-list and
637 cumulative lifecycle references released. Normal exit confirmed; exact
prior slot restored, sidecar removed and all four configurations unchanged.
106 archived sources verify. No diagnostic is installed or armed for a new run.
Evidence/rollback: `local/temporal-submission-20260925/v1.3.0-resume-20260925/`.
Next: bounded R32_UINT motion readback at the matched Dispatch site, validating
format/footprint, transition/restoration, submission/completion and allocation
retention before mapping. Stop lifecycle admission after complete witnesses
are fixed, before slow hook removal: this accepted run used 3263/4096 events,
including 1904 after the final completion poll. Motion pixels, typed views,
producer coverage and DLAA readiness remain unverified. Preparation below is
historical; do not relaunch the unchanged observer as the next experiment.

Resuming 2026-09-25: all 132 sealed preparation files and 106 archived sources
verify, as do the executable, prior slot and four configuration hashes. The
tested v1.3.0 DLL is unchanged (`8a61d013c1cfdab11c43960ce5e463589db48a0373ae4b1c0738e109d174311b`).
Fresh preparation/deployment/rollback receipts for this session are under
`local/temporal-submission-20260925/v1.3.0-resume-20260925/`.
Use that session's receipt and output directory; retain the original sealed
preparation and sleep-time rollback below as historical evidence. Next: load
the bare-profile save with DLSS Off/TAA On, keep the camera still, and arm
the submission observer once. Runtime evidence remains pending.

Session ended before the v1.3.0 game test (2026-09-25). Game was closed;
no v1.3.0 runtime log was created or recorder armed. The exact prior slot DLL
was restored and the temporary output sidecar removed; all four configuration
hashes are unchanged. The tested v1.3.0 build, 106 archived sources and test
results remain preserved in `local/temporal-submission-20260925/v1.3.0/`.
On resumption, verify that archive and prepare a fresh deployment receipt using
the same tested DLL. Do not overwrite or rerun the closed deployment's Install
action (its receipt intentionally prevents reuse). Then launch the bare-profile
test entry, DLSS Off/TAA On, load a save and arm once. No retest or new feature
work is needed before that launch unless verification finds a changed input.

Prepared 2026-09-25: **v1.3.0 TAA submission/completion observer**.
[Contract and procedure](../runtime/temporal-submission-observer.md).
It adds Reset, Close and Execute hooks to the accepted native recording
probe. Three original requests are linked to successful list generations,
actual submissions, three private queue Signals and later retirement resets.
The two-second window includes 150 ms of lifecycle warmup; unknown generations,
overlap, resubmission, failed calls, deadline expiry or missing completion reject.
No copies, transitions, jitter writes or observer command-list submissions.
43 native and 22 Python tests pass, including real WARP completion and real
lifecycle detours. Runtime evidence is pending. Local preparation/deployment/
rollback: `local/temporal-submission-20260925/v1.3.0/`.
Next run: existing bare-profile MO2 test entry, DLSS Off/TAA On, load a save,
keep the camera still and arm once. The deployment receipt identifies the
current test-slot DLL; the v1.2.0 rollback below is historical.

Accepted and closed 2026-09-25: **v1.2.0 run 53044**,
[native TAA recording result](ordinary-taa-consumer-result.md).
Frames/epochs 13801–13803 match the original producer packets to three native
Dispatch calls, each with groups 160×90×1. Each consumer records five full
transitions before Dispatch; GBuffer4 is `0x4 -> 0xC0` (render target to
combined pixel/non-pixel shader resource). Current color, history, previous
ID and output states are also established locally; depth/GBuffer1 remain
unknown within this observation scope. The shader is byte-identical to run
4376. No observer GPU commands or jitter writes. Zero faults/pending,
21 resource pins and three list pins released, all hooks disabled. Normal
exit confirmed; exact prior DLL restored, temporary sidecar removed, all four
configs unchanged. Frozen reviewer and separate transition analysis agree;
101 archived source files verified. No new diagnostic is installed or armed.
Evidence: `local/temporal-consumer-20260925/v1.2.0/`.
Next: associate the observed list with Reset/Close/Execute generations and
completion/retirement, then obtain bounded frame-associated motion pixels.
The local state result alone does not authorize a readback. Actual vectors,
typed views, producer coverage and working DLAA remain unverified.

Accepted and closed 2026-09-25: **v1.1.0 run 4376**,
[ordinary TAA shader result](ordinary-taa-shader-result.md). Three accepted
frames/epochs 14994–14996 saved the 15,492-byte compute shader, SHA-256
`2d7b26c742c27db2c83546c6cf6a1dfda47d7cfd4336e69f8c2302de5802b0d1`.
All seven named texture bindings agree with the shader declarations. Its
GBuffer4 decode is X = high binary16 half, Y = low half with bit 0 cleared;
the velocity branch computes previous NDC = current NDC minus that pair.
The other branch uses depth and the current-to-previous projection matrix.
Actual CB values, motion pixels, typed views, jitter inclusion and GPU
consumption remain unverified. Zero faults/pending, 21 pins released, hook
disabled. Normal exit confirmed; exact previous DLL restored and all four
configs unchanged. No new capture is installed or armed. Next: bounded
same-frame packed-motion readback, followed by static/camera-motion controls.
The preparation immediately below is historical; evidence and rollback are
in `local/temporal-shaders-20260925/v1.1.0/`.

Historical preparation 2026-09-25: **v1.1.0 TAA shader capture**, extending the accepted
v1.0.0 request observer with one bounded compute bytecode artifact and its
owner/descriptor recipe. [Static map](ordinary-taa-shader-map.md) and
[runtime contract](../runtime/temporal-input-observer.md). The same one hook,
three frames and two-second window apply; no jitter injection, GPU submission
or vendor SDK calls. All 17 selected native tests and 11 Python tests pass;
the owned shader saves and disassembles correctly. Forty-eight code spans
are gated. Runtime shader evidence is pending. Local preparation, deployment
and rollback receipts: `local/temporal-shaders-20260925/v1.1.0/`.
Next run: existing bare-profile MO2 entry, DLSS Off/TAA On, load a save and
stay in the scene; arm the inputs observer once. Decode the resulting shader
before designing motion-content validation. No DLAA-ready claim is made.

Accepted and closed 2026-09-25: [ordinary TAA input run 44924](ordinary-taa-input-result.md).
Three consecutive request frames 14224/14225/14226 pass the frozen v1.0.0
reviewer. Current color is R11G11B10_FLOAT, and output/history follow the
expected three-slot ring. The named depth binding is a 1×1 fallback; retain
the separately validated scene-depth path. Full-resolution GBuffer1 is
R8G8B8A8_TYPELESS and GBuffer4 is R32_UINT; typed views and motion contents
remain unverified. All 21 references released, hook disabled, zero faults or
pending callbacks. Normal exit confirmed, exact previous DLL restored,
temporary sidecar removed, all four configs unchanged and 90 archived sources
verified. Next: shader/typed-view mapping and motion validation. No new
diagnostic was installed or armed at that closure; preparation below is historical.

Prepared 2026-09-25: [ordinary TAA input observer v1.0.0](../runtime/temporal-input-observer.md).
The [offline map](ordinary-taa-input-map.md) follows the ordinary TAA texture
bindings and dispatch request; the legacy dense-motion producer is gated off
when its DLSS wrapper is absent. The standalone probe records three originating
frames and seven named inputs per request, pins their D3D12 identities, and
rechecks the source snapshot and queued packet. One native hook, two seconds,
zero jitter writes or GPU submissions. Ten native tests pass, including WARP
resource lifetime/failure cases, plus seven Python review/armer tests. The
contract executable additionally reports 46 checks and 620 missing-byte cases.
Local evidence/deployment/rollback: `local/temporal-bindings-20260925/v1.0.0/`.
Next user run: bare profile, DLSS Off/TAA On, load a save and remain in scene;
arm only the new inputs recorder. This is request association, not motion-content
validation or modern DLAA. The local deployment/prelaunch receipts determine
whether this observer currently occupies the MO2 test slot.

Accepted 2026-09-25: [v0.9.1 run 10232, three jitter directions in depth](depth-sequence-result.md).
All five readbacks pass the unchanged archived reviewer at request frames
14805/14806/14808/14809/14810. Three injections restored, one primary-draw wait,
zero recovery deferrals/faults/drops/pending. The capture uses 5132 history
entries; all 484 observer copy references release and hooks disable cleanly.
Separate measurement on the same 1,527,643 control-selected static depth pixels
finds all three predicted shifts within 0.001 pixel per fitted axis. Baseline
and recovery are bit-exact at 87.3183% of pixels; all view matrices match.
This extends scoped raster-depth evidence to three directions in one scene,
not continuous/full-renderer qualification or working DLAA. Broad reviewer
coverage flags remain false. User exited normally; exact previous slot restored,
sidecar removed, all four config hashes unchanged and 90 archived sources verified.
Next: color/motion request association and motion contents before bounded DLAA.
The v0.9.1 runtime test is closed; the new observer preparation is recorded above.

Prepared 2026-09-25: [v0.9.1 bounded history correction](../runtime/projection-depth-history.md).
Fixed history capacity increases to 8192 entries, with a 12 MiB static journal
ceiling and 64 KiB per readback history-pointer scratch array. The armer and
full reviewer bind the capacity to version 0.9.1; old versions retain 4096.
Native exact-bound and five-slot pressure/overflow fixtures exercise retirement
after delayed polling and rejection without mapping. CPU/GPU budgets, hooks,
warmup, polling and all validation requirements remain unchanged. Local v0.9.1
preparation/deployment/prelaunch receipts determine readiness and test counts.
That preparation preceded the accepted game capture above; the test DLL has
been removed from the slot and the previous diagnostic restored.

Closed 2026-09-25: [v0.9.0 run 49240, five-sample history capacity rejection](depth-sequence-capacity-result.md).
All three scoped native jitter injections restored, all five request stamps
matched, and all five private Signals completed. History reservation 4097
exceeded the 4096-entry bound before retirement validation completed. No images
were mapped or saved; the archived reviewer correctly rejects the capture.
Core faults/drops/pending are zero, all 396 observer references released and
hooks disabled. The user exited normally; previous DLL/sidecar state restored
and all four configuration hashes unchanged. The archive seals 214 evidence
files and verifies 87 archived sources. Next: size and test the bounded history
for five samples, preserving complete-history and retirement requirements.
That closure preceded v0.9.1 preparation above; the prior accepted triplet
remains the latest pixel result.

Prepared 2026-09-25: [v0.9.0 bounded jitter depth sequence](../runtime/projection-depth-sequence.md).
Five CPU/readback slots cover baseline, three distinct native jitter pairs and
zero recovery. The second/third stimuli wait for the primary selected draw's
return, under a 64-entry bound; only final recovery may defer on contention.
Every admitted injection retains immediate `__finally` restoration. Budgets:
three injections, five copies/Signals, two seconds, no new hooks. Later sample
readbacks retain exact request stamps and do not fabricate extra draw bindings.

Verification covers real core hooks and five-slot owned GPU routing/rejection,
plus sequence review and historical replay. Local `v0.9.0` preparation,
deployment and prelaunch receipts determine readiness and exact test counts.
That preparation preceded the rejected game run above. Its test DLL has been
removed from the slot; another launch requires a corrected, verified build.

Accepted 2026-09-25: [v0.8.4 run 48276, first jittered depth triplet](depth-triplet-result.md).
Three 2560×1440 images match baseline/stimulus/recovery request frames
15630/15631/15633. The archived reviewer accepts all three copies, Signals,
retirements, resource identities and request associations. Draw return now
precedes the stimulus copy submission. One injection/restoration, zero faults,
drops or pending calls; all 330 observer references release and hooks disable.

Offline analysis finds a shift of approximately (0.2342, 0.0225) pixels on
1,449,100 control-selected static depth pixels, versus (0.2344, 0.0216) predicted.
Baseline/recovery are bit-identical at 81.33% of pixels and their view matrices
are unchanged. This is strong one-sample evidence of jitter reaching rasterized
depth, not continuous/full-renderer validation or working DLAA. Broad reviewer
coverage flags remain false; the pixel measurement is separately archived.

The user exited normally. Previous DLL/sidecar state restored, all four config
hashes unchanged, all 83 archived sources verified. Release `/W4 /WX`, 241 native
and 543 Python tests passed. Local `v0.8.4` acceptance/archive and restored
receipts describe the closed run. Next: a bounded sequence with differing
jitter directions and recovery, followed by color/motion input association.

Preparation history: [v0.8.4 jitter depth triplet](../runtime/projection-depth-triplet.md).
The [v0.8.3 run 14072](depth-triplet-timing-result.md) exposed a recorder timing
bug: draw return was published only after CPU recovery, too late for the
stimulus copy submission. No pixels were saved. The rejected run is archived,
the user closed normally, the exact previous DLL/sidecar state was restored,
and all four configuration hashes matched. v0.8.4 publishes validated draw
return immediately while still requiring CPU recovery for overall completion.

The existing one-shot scoped jitter now publishes three source selections for
three independently stamped readback slots. The stimulus retains the full
selected-draw association; baseline/recovery are explicitly request controls,
with no fabricated draw markers. All three copies must match their CPU request
frames and pass resource, recording, submission, completion and lifetime review.
The camera field is restored before publication. Budgets: one injection,
three added copies, three private Signals, two seconds, no new hooks.

Release `/W4 /WX`, 241 native and 543 Python checks pass. Owned GPU clear-value
tests verify routing and rejection, not raster jitter. The expected displacement
is `(jx/2,-jy/2)` pixels; observed displacement remains to be measured from the
game images; this measurement is now recorded in run 48276 above. Local
preparation/deployment/prelaunch receipts preserve the original build state;
the later restored receipt records its removal from the test slot.

Capture accepted 2026-09-25: [v0.8.2 run 42232, matching projection/depth request](depth-alignment-result.md).
The selected projection and original copy request both carry frame/epoch 51739,
bank 1 and async mode 1. The depth copy is recorded before the selected draw
but submitted afterward on the same DIRECT queue. Nine checked recordings
contain no intervening observed clear, overwrite, applicable alias or bundle.
The unchanged archived reviewer accepts the complete association and pixels.

One added copy and one private Signal save 3,686,400 finite depth values at
2560×1440. The image shows coherent character and environment geometry.
All 266 observer copy references release; hooks disable with zero faults,
drops or pending calls. All 77 archived build sources verify. Release `/W4 /WX`,
159 native and 504 Python checks passed before deployment. Evidence and preview
are under `local/projection-binding-20260923/v0.8.2/`. Process exit was observed,
the exact prior v0.15 DLL restored, sidecar removed and all four configuration
hashes match. Acceptance and archive receipts are sealed. Normal-exit reporting
was still pending, so no inference about the exit cause is recorded.

The planned triplet succeeded in run 48276 above. No unchanged v0.8.2 rerun is
needed. All contributing pixel draws, final-scene coverage and cross-queue safety
remain unverified. This earlier control proves the narrower
request/selected-draw association.

Accepted 2026-09-24: [v0.8.1 run 43440, original copy-request frame](depth-copy-frame-result.md).
The admitted readback links to a stable original request in frame/epoch 14826,
one frame after selected projection draw 14825. The request was queued before
that older draw was recorded, demonstrating why worker-side global frame samples
are insufficient. Fifty-six receipts cover four copy requests per frame across
14 frames; 52 copies have a matching original receipt. The unchanged reviewer
accepts all resource/submission/readback checks and the selected packet chain.

The 2560×1440 depth image has 3,686,400 finite values. Hooks disabled with zero
faults/drops/pending; all 308 observer copy references released, while private
readback references were retained until exit. The user exited normally, prior
v0.15 DLL restored, sidecar removed and all four configuration hashes match.
Archive `local/projection-binding-20260923/v0.8.1/` includes acceptance, packet
analysis, raw pixels, preview and restoration receipts; 73 build sources verify.
Release `/W4 /WX`, 153 native and 471 Python checks passed before deployment.

Next: associate the projection stimulus with this same copy stage/frame before
an aligned baseline/jitter/restoration comparison. The observed +1 difference
is not a fixed offset to assume. Complete pixel-frame origin and raster jitter
remain unverified. The v0.8.1 cycle is closed; its alignment control has now
succeeded in v0.8.2 above.

Accepted 2026-09-24: [first GPU depth readback, v0.8.0 run 51816](depth-readback-result.md).
One added full-depth copy and private fence produce 3,686,400 finite values at
2560×1440. The copied list successfully Resets without an observed intervening
resubmission before Map. The unchanged deployed reviewer accepts the capture.
The rendered image shows coherent character and environment geometry; exact
producer-frame alignment, final-scene stage and raster jitter remain unverified.
No jitter was injected and no provider SDK was used.

Capture has 3,299 history calls, 7,658 barriers and 155 native copies. Observer
references release after drain; private readback references remain retained until
process exit. Hooks disabled with zero faults/drops/pending. The user exited
normally, the prior v0.15 DLL was restored, the sidecar removed and all four
configuration hashes match. Archive `local/projection-binding-20260923/v0.8.0/`
contains `acceptance-51816.json`, `archive-51816.json`, `restored.json`, raw depth
and a viewable preview; all 67 archived build sources verify. The cycle is closed.

Preparation passed Release `/W4 /WX`, 144 native and 433 Python checks, including
owned GPU pixel/lifetime tests. See [runtime design](../runtime/projection-depth-readback.md).
Its next-step original copy-request stamp is now established by v0.8.1 above;
complete pixel-production alignment remains open. The v0.8.0 cycle stays closed.

Accepted 2026-09-24: [v0.7.1 run 6868](depth-copy-submission-result.md).
All 52 native full-depth copies have unique successful submitted recording
generations and local COPY_SOURCE declarations. Thirteen cycles repeat four
copies into two 2560×1440 R32_FLOAT textures. The unique pure-COPY_SOURCE stage
is a structural readback candidate; destination identity alone is insufficient
because one texture receives three copies per cycle. CPU entry order differs
from submission-array order. No GPU pixels or raster displacement are proven.

Frame/epoch 14375 retains accepted view provenance, selected submission,
same-queue depth-write declaration and bounded graphics-fence completion
observation (value 14393). All broader GPU safety/content/readback fields stay
false. Capture has 2,972 history calls, 7,087 barriers and 143 copies; all 286
copy references released after drain, zero faults/drops/pending, hooks disabled.

The game closed normally, exact previous v0.15 DLL restored, output sidecar
removed, and four configuration hashes unchanged. Archive
`local/projection-binding-20260923/v0.7.1/` contains `acceptance-6868.json`,
`copy-history-6868.json`, `archive-6868.json` and `restored.json`; 59 archived
build sources verify. That runtime cycle is closed; its next-step readback is
now implemented and completed in v0.8.0 above. No unchanged v0.7.1 rerun.

Historical 2026-09-24: [v0.7.0 run 47748](depth-copy-result.md) is archived and rejected
during provenance warmup, before the selected draw. Partial evidence contains two
actual full-resource copies from a 2560×1440 R32_TYPELESS depth texture into two
R32_FLOAT render-target textures, preceded by COPY_SOURCE/COPY_DEST declarations.
The selected draw, submitted generations and GPU contents are not established.
A concurrent first-use color-resource lookup tripped the recorder's overlap guard.
All hooks/references drained, the game closed normally, the exact v0.15 DLL was
restored and all configuration hashes match. No unchanged v0.7.0 rerun is useful.
v0.7.1 is prepared with independent per-call resource references and metadata
merging after drain. Release `/W4 /WX`, 131 native and 390 Python checks pass;
30 owned journals include deterministic overlap, exception and merge-rejection
cleanup cases. See its preparation/deployment receipts for installation state.

Historical preparation 2026-09-24: [native copy observer v0.7.0](../runtime/projection-copy-recorder.md).
The [offline depth-copy audit](depth-copy-map.md) reproduces run 21900's accepted
review and maps eight depth transitions to three adjacent DIRECT submissions.
The native barrier-flush wrapper contains three actual copy call sites, but
v0.6.0 did not record their source/destination arguments. The next bounded capture
will identify copies involving the selected depth and their submitted generations.
No game readback, jitter injection or provider integration is included yet.
Release `/W4 /WX`, 201 native and 381 Python checks pass. The owned GPU depth
readback fixture verifies all 256 pixels after fence completion; this is test
data, not game depth. Twenty-four owned journals are retained for review.
Use the v0.7.0 preparation/deployment receipts to determine readiness; the last
completed game cycle remains v0.6.0 below.

Accepted 2026-09-24: [v0.6.0 run 21900](queue-synchronization-result.md).
Frame 14548 has a qualified depth view, submitted recording, and DEPTH_WRITE
declaration across adjacent DIRECT submissions. The following DIRECT Signal
sets fence 205261888 to 14566; a later native completion read reaches 14566.
The observed fence is unshared, with monotonic DIRECT-only writers and reads.
`same_queue_depth_state_declared=true` and
`selected_queue_fence_completion_observed=true` in the follow-up review.
Complete cross-queue resource safety, GPU state/contents and raster use remain
unproven; all broader GPU safety fields stay false.

The original reviewer rejected a Wait nested inside a later Execute on the
same thread. The narrow offline correction accepts that strictly contained
interval only after the selected Execute and before its following Signal;
83 synchronization tests pass. Both original and follow-up reviews are saved,
and the archived reviewer reproduces the original exactly. Five older accepted
game reviews, two rejected captures and all 16 owned fixture reviews are unchanged.

The clean capture contains 2,829 history calls, 113 submissions, 55 exact-value
Signal/Wait correlations and 70 unmatched waits. All 23 list, three queue and
11 fence references released; hooks disabled with zero faults/drops/pending.
Game closed normally, prior v0.15 DLL restored, sidecar removed, and four
configuration hashes unchanged. Archive `local/projection-binding-20260923/v0.6.0/`
contains `acceptance-21900.json`, `archive-21900.json`, both reviews and
`restored.json`; 52 original build sources verify. The runtime cycle is closed.
Next: audit depth accesses and choose a stable per-frame readback point.
No unchanged rerun is needed to resolve the nested-Wait review issue.

Accepted 2026-09-24: [v0.5.0 run 47376](projection-submission-result.md)
connects the selected frame-10691/root-9 draw to its actual successful
Reset/Close generation and returned DIRECT-queue submission, first in a
seven-list batch. View descriptor/lifetime and all three resource-query phases
pass. The immediately preceding DIRECT submission contains the transition
back to DEPTH_WRITE and the matching clear. COPY and COMPUTE submissions
are also observed; queue synchronization and completion are not captured.

`selected_recording_submitted=true`; local and submitted depth-state
qualification remain false. The v0.5.0 reviewer covers only the selected
batch's prefix and rejects unqualified cross-queue reasoning. Next: audit the
retained adjacent DIRECT generations offline and design the required
Signal/Wait/completion observation. Do not rerun unchanged v0.5.0 to recover
missing synchronization. GPU state/contents, raster use and working DLAA/DLSS
remain unverified.

The 2,260 history calls include 102 submissions (76 DIRECT, 13 COPY, 13 COMPUTE),
193 list entries and 6,535 barriers. All 23 list/three queue references released;
zero faults/drops/pending and hooks disabled. Game closed normally, prior v0.15
DLL restored (`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`),
output sidecar removed, four configuration hashes unchanged. The archive
`local/projection-binding-20260923/v0.5.0/` contains `acceptance-47376.json`,
`submission-history-47376.json`, `archive-47376.json` and `restored.json`;
47 archived sources verify. The v0.5.0 runtime cycle is closed; the v0.6.0
receipts above take precedence for the next cycle. Preparation passed 110 native
and 205 Python tests.

Accepted 2026-09-24: [v0.4.0 run 1544](depth-view-state-result.md) ties the
actual writable D32_FLOAT/TEXTURE2D/mip-0 DSV creation and native view lifetime
to the selected root-9/VS-b0 indexed draw at frame 20975. Its resource is
2560×1440 R32_TYPELESS; all three identity/device/description phases agree.
The 55-event view watch and 2,461-event history drained cleanly, with 7,536
barrier records, zero faults/drops/pending, and all retained references released.

State remains partial: the selected list's latest actual Reset is observed,
but its generation contains no matching transition before the draw. The same
depth resource has 112 transitions on six other command lists and 14 clears
on another list. CPU order cannot join these to the draw. Next: adapt actual
queue-submission observation to this view lifetime and recording generation,
capturing through Close/submission before designing readback. Do not rerun the
unchanged v0.4.0 capture to resolve cross-list order. GPU state/completion,
contents, raster displacement and working DLAA/DLSS remain unproven.

Completed v0.4.0 deployment state: game closed normally, exact prior v0.15 DLL restored
(`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`),
output sidecar removed, all four configuration hashes unchanged. The sealed
`acceptance-1544.json`, `depth-history-1544.json`, `archive-1544.json` and
`restored.json` are in `local/projection-binding-20260923/v0.4.0/`; 44 archived
source files verify. That runtime cycle is closed. The v0.5.0 receipts above
take precedence for the next runtime cycle.

Preparation for the completed run: [view-lifetime/history recorder v0.4.0](../runtime/depth-view-state-recorder.md).
It adds startup DSV creation/destruction/copy observation, pins the actual
depth descriptor heap, and records actual Reset/Close/barrier/clear/bundle
history before the selected projection draw. Shared binding/draw ordering
guards against descriptor replacement between the two calls. The existing
projection and three-phase resource checks remain intact. No jitter writes
or commands added to the game's rendering path.

Release `/W4 /WX`, 101 native and 164 Python checks pass. The journal has a
6,750,208-byte fixed footprint. Owned SDK fixtures caught and corrected a
32-bit clear-argument decode and the fourth stack argument of CopyDescriptors;
the deadline-reservation regression is covered as well. Accepted game reviews
31912, 46324 and 11804 must replay identically in the sealed preparation;
42212 and 5208 remain rejected. The v0.4.0 game result is summarized above.
DLL SHA-256: `93A161877B3C6CDFCAE6977102F31E7622B1002C60582578E57A9919898B12AB`.

Archive: `local/projection-binding-20260923/v0.4.0/`. `preparation.json` seals
the tested sources, binaries and evidence; `deployment.json` and `prelaunch.json`
confirm installation, and a later `restored.json` closes that runtime cycle.
These receipts take precedence over older deployment states below. Completed procedure:
use the existing bare MO2 test entry, DLSS Off/TAA On, load a save, then request
the short capture after validating startup readiness. A missing early DSV
creation or actual Reset remains partial evidence; do not repeat unchanged
runs to recover an event outside the observation window.

Offline follow-up completed 2026-09-24: [depth-view lifetime and earlier-state map](depth-view-state-map.md).
The exact DSV constructor, handle allocator/recycler, native clear/barrier
flush and engine command-cache setup are mapped. Ten complete functions and
two leaves pass 90 instruction checks and ten SDK method checks. Compiled SDK
layout assertions and 1,080 descriptor/781 barrier assignment replays pass,
with four interpreter rejection checks. No native game code was executed.

Two changes to the next observation strategy follow: DSV handles are recycled,
so the actual creation must be tied to a view lifetime; and `259BCC0` resets
engine caches without issuing D3D12 Reset, so actual API Reset/Close events
must identify recording generations. The native clear can flush barriers
before the matched draw, but that earlier event is still unobserved live.

The [design](../runtime/depth-view-state-plan.md) is now implemented as v0.4.0
above. It uses an earlier descriptor watcher plus a short pre-draw API history;
another unchanged two-second attachment capture cannot recover an old DSV
descriptor.
Evidence and source hashes are in `local/depth-view-state-20260924/`.

Accepted 2026-09-24: [v0.3.1 run 11804](projection-attachment-result.md)
links the zero-jitter projection source at frame/epoch 15908 through publication,
backend consumption, root parameter 9 / VS b0 and a recorded indexed draw to
one validated depth attachment, with zero color targets. Its actual resource
description is 2560x1440 `R32_TYPELESS`, one mip, one array slice, one sample,
`ALLOW_DEPTH_STENCIL`. All three identity/device/description query phases agree;
every query succeeds and releases its temporaries. The canonical `IUnknown`
address differs from the resource interface, validating the corrected check.
Native pipeline metadata enables depth writes with `GREATER_EQUAL` and maps
to `D32_FLOAT`; the actual DSV creation descriptor and cached PSO creation
descriptor remain unobserved.

All 39 live code gates and 24 hook targets passed. Eleven API events include
the depth-only output bind, 2560x1440 viewport/scissor, pipeline/root binds and
`DrawIndexedInstanced(2520, 1, 2586, 0, 0)`. Zero faults, drops or pending calls;
hooks disabled and drained, resource pins released. No jitter mutation or
added GPU commands. Release `/W4 /WX`, 76 native and 117 Python tests pass.
Tested DLL SHA-256:
`B42BC8BF5F82600A3A80E9120CE28A5B5786BCF859AC9A939CAA7E30C8F69D1B`.

Historical v0.3.1 rollback: the user exited normally; the exact previous v0.15
DLL is restored, SHA-256
`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
The test output sidecar is removed and all four Off/TAA/profile/mode hashes
are unchanged. `acceptance-11804.json`, `archive-11804.json` and `restored.json`
under `local/projection-binding-20260923/v0.3.1/` verify the accepted evidence,
42 archived source files and rollback. That closed the v0.3.1 cycle; consult
the v0.4.0 receipts above for the current test slot.

The subsequent offline map above identifies where to observe the actual
depth-view descriptor/subresource and earlier resource-state history before
preparing bounded GPU readback. No barrier occurred within this selected draw's capture scope;
that does not establish its incoming resource state. GPU submission/completion,
depth pixels and raster displacement remain unverified. Do not repeat this
unchanged attachment capture. The [identity protocol](../runtime/projection-attachment-identity.md)
and original failed run below remain available for reproducibility.

Partial run 5208, 2026-09-24: [v0.3.0 attachment capture](projection-attachment-first-run.md)
observed a returned zero-color/one-depth OM bind, native depth writes enabled,
and matching 2560x1440 viewport/scissor. Resource identity validation failed
before `GetDesc` was accepted or the actual draw API was recorded. All 39 live
gates and 24 targets passed; hooks disabled/drained, zero drops/pending.
A bounded live inspection found the candidate's stable System32 resource
vtable. An owned SDK reproduction on the same adapter exposes an invalid
assumption in the recorder: canonical `IUnknown` and resource interfaces can
have different addresses. The correction and new fixtures are described above.
Full run remains rejected; no GPU
or actual resource-description claim. The user exited normally; the exact
previous v0.15 DLL is restored, the output sidecar removed, and all four
configuration hashes were unchanged. `archive-5208.json` verifies the evidence
and 40 archived source files. The completed v0.3.1 cycle above supersedes this
historical rollback state.

Installed and preflight-verified 2026-09-24: [read-only attachment recorder v0.3.0](../runtime/projection-attachment-plan.md).
It captures attachment/view/cache associations, pinned resource descriptions,
native depth/blend/raster state, local barriers, viewport/scissor and naturally
occurring PSO creation for one projection-consuming draw. Zero jitter writes
and added GPU commands. Offline verification: 63 native tests, 102 Python
tests and all 39 decoded-image code spans; previous game reviews replay
unchanged. Initial runtime validation is partial as described above. Deployment, rollback and prelaunch
receipts are under `local/projection-binding-20260923/v0.3.0/`.
Tested v0.3.0 DLL SHA-256:
`B08EB9238DC392008B8F05B16D00E2D354E3A373DCDBA42EC38BFFA6A7F1BD47`.
The exact v0.15 slot was backed up as `previous-slot.dll.disabled`; no output
sidecar existed before installation. All four graphics/profile/mode hashes
are unchanged. Preflight verified 40 archived source files, the build, slot,
backup and new repository-output sidecar with the game closed. That test slot
has now been restored from its backup after run 5208.
The initial runtime step is now complete as partial run 5208 above. Do not
re-arm or repeat this unchanged v0.3.0 recorder; close/restore it and correct
the identity check offline.

Offline follow-up completed 2026-09-23: [attachment/PSO map](projection-attachment-map.md).
Nine complete native functions are mapped with verified unwind extents,
93 instruction checks, nine SDK method checks and compiled SDK layout
assertions. A bounded interpreter passes 64 shader-descriptor and 256 recipe
cases without executing game code. Both attachment binding and PSO selection
have cache paths, so missing fresh API calls cannot identify an absent target.
Backend view resource candidates use `+18`; this differs from texture backing
`+20`, and a cached depth pointer can remain stale after depth is unbound.
The [read-only diagnostic](../runtime/projection-attachment-plan.md) is
now implemented as described above. First qualify the matched draw's
actual resource descriptions, native depth-write state, cache/API provenance
and local barriers; then decide a bounded GPU readback. The actual attachment
and executing PSO descriptor from run 46324 remain unknown.

Accepted 2026-09-23: [v0.2.1 run 46324](projection-jitter-binding-result.md)
follows one nonzero native jitter sample through CPU `fViewProj`, publication,
backend consumption, the matched root-CBV bind and an indexed draw call.
Baseline/stimulus/recovery frames are 15574/15575/15577; exactly one injection
and restoration, with baseline/recovery constants identical. One unreserved
recovery observation deferred; zero critical drops, faults or pending calls.
All 36 live gates and twenty targets passed; hooks disabled and drained.
Tested DLL SHA-256:
`A468B34138ECA53B51935DB52CA3621BAF13288F835C4B18F2DEE6C0F84D1621`.
Release `/W4 /WX`, 38 native and 59 Python checks pass. The reviewer reproduces
run 31912 unchanged. Archive replay verifies 32 retained source files, the
new accepted log and rollback in `local/projection-binding-20260923/v0.2.1/`.

The selected source belongs to arena 2 while allocation has advanced to 0;
the actual bind is root parameter 9 / GPU address `12381849856`, followed by
`DrawIndexedInstanced(2520, 1, 2586, 0, 0)`. The observed 2560x1440 viewport
matches the matrices. Stable compact root metadata correlates VS b0 with the
same shader containers as run 6444. This proves command recording with the
jittered source, not executing-PSO bytecode identity, GPU contents/submission
or raster displacement.

At the end of v0.2.1 the game exited normally and the previous v0.15 DLL was restored at the MO2 slot,
SHA-256 `689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
Its output sidecar was removed; all four Off/TAA/profile/mode files were unchanged.
The new v0.3.0 deployment receipt supersedes that historical slot state. The completed
[stimulus protocol](../runtime/projection-binding-stimulus.md) remains documented.

The follow-up attachment qualification is now accepted as run 11804 above.
The captured PS has no color output, and the new evidence correlates a depth
attachment; its precise pass role and GPU writes remain unproven. View/state
qualification and a bounded GPU observation tied to queue completion remain
next. Do not repeat the unchanged CPU/binding experiment. Motion/history
alignment and working DLAA/DLSS remain later steps.

Partial result 2026-09-23: [v0.2.0 run 42212](projection-binding-stimulus-result.md)
retains baseline frame 15329 and stimulus frame 15330. One native sample
`[-0.03125, -0.37654319405555725]` changed CPU `fViewProj` as predicted;
independent matrix checks pass and exactly one camera-field restoration to
zero is recorded. The source reached publication and backend consumption,
but recovery entry collided with consumer-entry bookkeeping (sites 2/6).
Full experiment rejected: one drop, fault contention, no recovery/draw/API or
shader record. Hooks disabled and drained, zero pending; the game remained
responsive and exited normally. The previous DLL was restored and all four
configuration files unchanged before installing v0.2.1 above. Rejection,
partial math, 31 archived source hashes and rollback are verified under
`local/projection-binding-20260923/v0.2.0/`. Do not call this a completed
nonzero draw-binding or raster-jitter result.

Accepted 2026-09-23: [v0.1.3 run 31912](view-projection-contract.md#run-31912-zero-jitter-contract-accepted)
validates the zero-jitter view-projection matrices and stable native root
selection through a matched indexed draw. All 36 live code gates passed and
twenty hooks installed. Frame/epoch 15057 supplies the 1,072-byte source;
the compact root layout maps parameter 9 to VS b0. Actual root-signature,
root-CBV and indexed-draw API calls correlate with the same source and the
same two shader containers as run 6444. Zero faults, drops or pending calls;
all hooks disabled and drained. Tested DLL SHA-256:
`FE128312B414BDAAFADFC6EA0E86C150648C3668CCAD102FC509E300C5AB8C34`.
Release `/W4 /WX`, 24 native and 34 Python tests pass; the latter includes 128
nonzero-jitter numerical cases. Twelve separate static root-builder replay
cases pass. The game exited normally; the previous v0.15 DLL is restored:
`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
The output sidecar is removed and all four Off/TAA/profile/mode files are
unchanged. Exact sources, DLL, trace, shaders, acceptance replay and rollback
receipts are in `local/projection-binding-20260923/v0.1.3/`; all 27 archived
source hashes pass. That restoration preceded the v0.2.0 installation above.

Follow-up design, now implemented in v0.2.0: use the already
validated scoped camera stimulus, with immediate field restoration, and
follow its `fViewProj` through the matched source/publication/consumption/draw
chain. Require baseline and recovery observations, distinguish matrix changes
from camera motion, and reject incomplete or mixed-frame evidence. Do not
repeat the unchanged zero-jitter run. Executing-PSO bytecode identity,
GPU contents, submission, raster displacement and temporal reconstruction
remain separate unresolved questions.

Accepted 2026-09-23: [run 6444](projection-binding-result.md) records the complete
CPU source/publication/backend-consumption/root-CBV/draw chain. v0.1.2's startup
reservation succeeded, all 29 live code gates passed, and twenty targets were
installed. Frame 16528 supplies a 1,072-byte projection buffer in arena 1,
bound to root parameter 9 before a `DrawIndexedInstanced` call. Nine API events,
two shader containers, zero faults/drops/pending calls and clean hook disabling
are recorded. Independent review accepts `root-cbv-call-chain-observed`.

After run 6444 the exact previous v0.15 DLL was restored:
`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
Its test output sidecar was removed and all four graphics/profile/mode hashes
were unchanged before installing v0.1.3. The tested v0.1.2 DLL, sources, trace, shader containers, acceptance
replay, and restoration receipt are under
`local/projection-binding-20260923/v0.1.2/`. Its DLL hash is
`EBB3E88B25C5421ED2E82B5EB484E76898E2DF2C386E94661B7127288571F917`;
18 native and 23 Python preparation tests passed.

The retained VS uses `CBViewProjection` at b0, reading `fViewProj` at bytes
0–63 for clip position; it does not directly read `fProj` at byte 128. The PS
has no color output. Actual render/depth attachments and the executing PSO's
creation bytecode were not captured. Static root-builder `2626030` provides a
candidate mapping of parameter 9 to VS b0, consistent with the other recorded
CBV/table calls, but the runtime selector fields were not retained. Do not
equate a root parameter number with a shader register without that evidence.

Run 31912 above supplies the previously missing live root-selection metadata
and zero-jitter matrix relationships. It correlates the pinned native layout
with VS b0 and the actual API binding, without independently capturing the
serialized root blob or the executing PSO's creation bytecode. Submission,
GPU contents, shader execution, raster jitter and modern reconstruction remain
unproven. No nonzero jitter was injected by either binding recorder run.

Historical failures are preserved: [run 33032](projection-binding-first-run.md)
stopped after publication on recorder contention; [run 23832](projection-binding-allocation.md)
failed before the first hook on trampoline allocation. Their correction paths
are exercised successfully by run 6444, without proving the exact original
collision pair or VirtualAlloc failure cause.

Updated 2026-09-23: [run 17072](scoped-camera-jitter.md#run-17072-cpu-stimulus-and-recovery-accepted)
accepts the v1.0.2 CPU projection experiment. Frames/epochs 13406, 13407 and
13408 capture zero-jitter baseline, one native sample, and zero-jitter recovery.
The stimulus `[0.34375, -0.48765432834625244]` changes the two expected matrix
coefficients at viewport 2560x1440; independent review's maximum absolute
residual is about `1.43e-11`. Baseline/recovery are bit-identical to their
unjittered matrices. Exactly one injection and restoration, three visits,
zero drops/pending calls, and clean hook disabling are recorded.

The raw `mUseDLSS` switch is 1 while sizing flags are zero and the NGX wrapper
is absent throughout. The corrected provider exclusion passes. No original
TAA enable flag was forced. This is accepted CPU evidence only: GPU upload,
shader/raster consumption, motion vectors, history behavior and working
DLAA/DLSS remain unqualified.

At completion of run 17072, the game exited normally and the exact previous
v0.15 bridge was restored at the MO2 slot, SHA-256
`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
That run's output sidecar was removed; all four Off/TAA/profile/mode files were
unchanged. The retained v1.0.2 binary is
`63737E4102453B7ED7569CCD32CE7CDB29ABC54B06669BAE448E6CF205DD0B5B`.
Its 16 native/20 Python checks, nine code gates and 23 static instruction checks
apply to the unchanged build. Source/archive replay passes. Evidence:
`local/jitter-activation-20260923/provider-gate-1.0.2/acceptance-17072.json`.

The [offline upload/binding map](projection-upload-binding.md) now connects
CBViewProjection allocation, kind-11 publication, backend slot storage,
three-arena transfer, root/descriptor CBV binding and draw entry points. It also
locates the shader bytecode descriptors used to create those graphics PSOs.
Replay passes 85 instruction checks across 21 spans and 13 SDK ABI slot checks.
That map was initially static evidence; runs 6444 and 31912 above now match
live command recording and shader metadata to their captured producers. The
consumed arena can differ from the current allocation index, and the retained
vertex shader consumes fViewProj instead of fProj.

The implemented recorder above follows that map through exact publication,
backend slot, observed binding and draw, with bounded shader extraction.
GPU contents/raster displacement and motion-vector sign/scale/frame alignment
remain later qualification steps. Do not repeat the unchanged CPU test.
The [CPU runtime protocol](../runtime/scoped-camera-jitter.md) is completed.

The earlier v1.0.0 hook-install failure remains unexplained but did not recur.
Run 5152 exposed the now-corrected raw-switch eligibility bug; its later live
snapshots were not contemporaneous proof of its precise rejection. Both rejected
archives remain intact in the parent directory and `install-diagnostics-1.0.1/`.

## Completed track-source observation

Updated 2026-09-23: [track run 42104](jitter-track-writer-result.md) accepts
two complete source/destination pairs. Two TAA2 owners and embedded records
share the same source header/arrays: count 1, index 0, key word 0, raw value 0.
Both native calls change jitter-enable from 1 to 0, returning to `0x221D5ED`.
Source snapshots are unchanged and match the incoming low bit. Caller code
was verified live this time. One unclassified drop limits completeness.

The static follow-up identifies `uScheduler` / `mpScheduler` machinery, without
identifying the exact live controller or resource file. The preflight report's
settings field had a variable-shadowing metadata bug; the original and used
script are preserved, a hash-backed settings correction is explicit, and the
helper is fixed. Native capture/source evidence is unaffected. Archive replay
and scheduler-map checks pass; the unchanged observer has 31 passing tests.

The game exited normally. The previous v0.15 bridge is restored at the MO2
slot, SHA-256 `689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
The new output sidecar is removed; four Off/TAA/profile/mode files are unchanged.
Evidence, correction, build/source copies and rollback remain under
`local/jitter-track-writer-20260923/`. No recorder needs another unchanged run.

That result led to the scoped projection-consumer experiment above. The record
below describes the completed source run and its restoration; the new prepared
installation supersedes that slot state.

## Completed writer observation

Updated 2026-09-23: [writer run 44144](jitter-property-writer-result.md)
records two native calls changing two TAA2 `mJitterEnable` bytes from 1 to 0,
both returning to RVA `0x221D5ED`. DLSS remained Off/TAA throughout the
menu-to-save sequence. All selected snapshots pass; the hook stopped cleanly.
Ten unclassified admissions were dropped, so this is positive writer evidence,
not an exhaustive count or proof of active raster ownership.

The [property writer map](jitter-property-writer.md) now connects that return
address to the decoded track routine `0x221C9E0`, kind 11, which passes the low
bit of a selected array byte. The raw source byte, key and asset identity were
not captured. Additional caller spans were mapped offline, not live-hash gated
in this run. Archive replay and 22 new static instruction checks pass.

That game closed normally and the previous v0.15 bridge was restored at the MO2
force-load slot, SHA-256
`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
The writer's new output sidecar is removed; four saved/profile/mode files are
unchanged. The observer build, 11 source copies, receipts and rollback remain
under `local/jitter-property-writer-20260923/`. Five native and ten Python
checks apply to that unchanged diagnostic build.

That result led to the source-context extension above. Do not force the jitter
flag. GPU/raster and motion-vector qualification remain open.

## Completed callback observation

Updated 2026-09-23: [native callback run 37452](native-jitter-callback-result.md)
accepts 768 paired CPU calls across menu, loaded-scene still and user-reported
camera-pan windows, with DLSS Off and TAA selected. Every sampled TAA2 update
enters with jitter-enable zero while its other captured predicate clauses pass.
Each window records 85 ordered update/reset/setter groups; the setter receives
zero and advances the stale camera epoch. This supports the renderer clearing
path and establishes callback execution, but does not identify the enable-byte
writer or qualify GPU/raster behavior. Each window is capacity-limited and
reports one dropped admission; the pan is user reported, not input-instrumented.

The recorder disabled its hooks cleanly after phase 3. Normal exit is reported,
process absence was required for restoration, and the previous v0.15 bridge
is restored at the MO2 force-load slot with SHA-256
`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
The new output sidecar is removed; backups/evidence remain under
`local/jitter-callbacks-20260923/`. Acceptance replay verifies four unchanged
configurations, 11 source identities and the retained recorder build. Existing
native fixture/eight Python checks apply to the unchanged implementation.

That run led to the property-writer follow-up above. No identical isolation or
pan rerun is needed. The callback and both writer observers were removed after
their completed runs; the previous v0.15 bridge is restored at the slot.

## Offline callback map

Updated 2026-09-23: the [native callback map](native-jitter-callback-map.md)
now connects category dispatch to the typed TAA2 jitter/reset callbacks and
the camera-jitter setter. Sample selection uses the frame counter modulo 20;
the reviewed selector/callback has no direct camera-movement test. Renderer
camera preparation can clear stored jitter on an epoch mismatch. That clearing
does not explain the zero jitter-enable byte; its writer remains unresolved.

The user reports no deliberate camera movement during the previous tests.
Preserve their stationary-input scope; motion-vector behavior was not tested.
The bounded callback/setter recorder and separate still/pan windows have now
completed as recorded above. Offline identity/span/table/arithmetic checks and
runtime live-span verification pass. The map itself changed no native code;
subsequent instrumentation is documented separately. GPU/raster qualification
remains open.

## Bare-profile control

Updated 2026-09-23: [bare-profile run 2632](jitter-bare-profile.md)
retains jitter-enable 0 and zero stored jitter in menu and gameplay with no
enabled MO2 mods and no detected bridge/mod-loader DLL. All 64 attempts retain
the zero values; 11/16 focused attempts in each phase satisfy combined scalar,
anchor, type and counter consistency. These external CPU snapshots do not
establish callback execution, active ownership, frame coherence or GPU behavior.

The first empty-profile launch (34948) inherited the DLSS entry's Force Load
rule and is archived separately as a bridge-loaded auxiliary menu observation.
The regular game entry produced the verified no-bridge run 2632. Normal exit
is reported, process absence and unchanged Off/TAA/profile settings are verified.
No configuration, deployment or code was changed; nothing needs restoring.
The empty profile remains selected. No bridge Shutdown trace is expected.

The configured mods and bridge are not required for the sampled zero state.
Next: trace the native assignment and callback/reset order with a typed target;
do not assume zero proves broken native TAA or force the byte. No repeated
isolation/enable run is needed. Modern SDK and GPU/raster qualification remain
inactive/unverified. Archive replay and documentation checks pass.

## Disabled-provider comparison with the original mod list

Updated 2026-09-23: [disabled-provider run 8164](jitter-startup-control.md#run-8164-menu-and-gameplay-remain-zero-in-the-control)
retains jitter-enable 0 and zero stored camera jitter in menu and loaded
gameplay. The startup trace verifies disabled mode and zero synthetic routes;
the backend NGX/context pointer is absent, although renderer `+0xA5` remains 1.
Thus active bridge startup substitution is not required for the observed zero
state. The exact writer remains unresolved; other mods and retained observers
prevent calling this vanilla behavior.

All 64 bounded attempts retain the zero values; focused consistency passes
12/16 menu and 8/16 gameplay attempts. The broad gameplay pass crosses counters
throughout and is not accepted as a same-counter observation. These are CPU
snapshots, not callback-execution, ownership or GPU/raster qualification.

The passive trace ended after 30 seconds before sampling; it verifies startup,
not continuous coverage or Shutdown. User-reported normal exit/process absence,
unchanged Off/TAA settings and DLL/mod list/profile are verified. The original
native-depth-probe sidecar is restored, and archive replay passes. No native
source or binary changed. Next: trace the scene/property assignment or isolate
remaining mod/observer effects. No new launch or unchanged enable rerun is
needed; modern SDK integration remains inactive.

## Pre-enable observation with the active bridge

Updated 2026-09-23: [pre-enable run 5836](jitter-pre-enable-observation.md#run-5836-disabled-jitter-observed-before-first-create)
finds `mJitterEnable=0` and zero stored camera jitter in both menu and loaded
gameplay with DLSS left Off, before any Create/Evaluate or native depth/fallback
installation. A focused gameplay pass retains stable scalars in 16 attempts
and matching counters/epoch in 11. These remain asynchronous CPU observations,
not ownership, callback-execution or GPU/raster qualification.

The first Create is not required to produce this disabled state. Bridge startup
capability/sizing callbacks did execute; game properties and other enabled mods
also remain possible influences. The exact writer and vanilla behavior are
unresolved. Next: prepare an otherwise identical Off/TAA control with bridge
startup substitution disabled, or observe the earlier assignment. That control
has since completed as recorded above; do not repeat the unchanged enable test.

User-reported normal exit and process absence are confirmed; saved Off/TAA,
DLL/sidecar and previous backup are unchanged. The five-minute trace expired
before exit, so Shutdown and final cleanup are unobserved in this run. The
archived acceptance replay passes. No native code changed; prior 48 native /
78 Python checks still apply. Modern SDK integration remains inactive.

## Accepted v0.15.0 menu and loaded-scene comparison

Updated 2026-09-23: [v0.15.0 run 27200](jitter-scene-probe.md#run-27200-menu-and-loaded-scene-cpu-samples-accepted)
accepts 16 menu and 16 explicitly triggered loaded-scene CPU samples. Gameplay
uses a different verified TAA layer and different projection constants, but
stored jitter stays zero and `mJitterEnable=0` is the only false captured
predicate clause in both phases. Both retain a wrapper and `use_dlss=1`.
This disabled state is not confined to the sampled menu instance; its writer
and earlier callback execution remain unresolved.

One producer/four preparations/one correction/one Evaluate completes. The
517-record journal drains with flags 0 and a matching submission/fence chain.
The local Evaluate checkpoint remains unqualified because call 295 overlaps
its marker; later journal review does not upgrade that snapshot or GPU safety.

On the menu return, Release succeeds and a second Create is refused: this
diagnostic's observer is single-use and cannot rearm. The user saw greyed-out
Off and exited normally. Both Shutdowns, cleanup and original Off/TAA settings
are verified; no settings edit was needed. All 145 staged sources and deployment
identities were unchanged at exit. The unchanged build retains 48 native and
78 Python passing tests plus binary/hardware checks. Modern SDK integration
and GPU/raster temporal qualification remain inactive/unverified.

The subsequent pre-enable comparison is recorded above. The assignment from
scene properties into the live jitter-enable byte remains unresolved; the
named property's presence and value are distinct bytes. Do not force jitter
based only on these producer-time observations. Game is closed with saved Off/TAA.

## Accepted v0.14.0 baseline

Updated 2026-09-23: [v0.14.0 run 30924](jitter-sequence-probe.md#run-30924-both-cpu-phases-accepted)
accepts 16 wrapper-present and 16 wrapper-absent CPU samples. All have zero
stored camera jitter, identical projection matrices and `mJitterEnable=0`;
every other captured native jitter-predicate clause passes. The live reset
vector is zero. Renderer `+0xA5` stays 1 even after wrapper release, so that
phase does not prove every renderer setting has settled to Off.

One producer/four preparations/one correction/one Evaluate and a 615-record
completion stop with flags 0 succeed. User reports brief freeze, then normal
Off/apply/exit; uncertainty about waiting three seconds is immaterial because
all Off-phase samples are present. Cleanup, original settings, DLL/sidecar and
138 staged source/archive pairs are verified. The unchanged build retains
47 passing native and 64 Python tests, plus binary/unwind and hardware checks.

Offline tracing confirms the byte defaults to 1 and is exposed through a named
boolean property; the assignment that clears this live instance is unresolved.
Next: resolve its scene/property control and prepare a bounded loaded-scene CPU
comparison. The current menu sequence consumes its budget before gameplay, so
do not request an unchanged menu rerun. Earlier callback execution, pre-enable
state, gameplay jitter, GPU/raster temporal content and modern SDK integration
remain unverified or inactive. Game is closed with saved DLSS Off/TAA.

## Accepted v0.13.1 baseline

Updated 2026-09-23: [v0.13.1 run 21636](camera-type-probe.md#run-21636-cpu-projection-observation-accepted)
accepts the CPU projection snapshot: derived camera `0x33F82A8`, 110 stable
reads, matching epoch 10702, viewport 2560x1440, and bit-identical matrices.
Actual stored jitter is zero although both table candidates are nonzero. One
producer/four preparations/one correction/one Evaluate and a 627-record
completion stop with flags 0 succeed. User reports brief freeze then normal
Off/apply/exit; release, shutdown, cleanup and original settings are verified.
The unchanged build retains 45 native/52 Python passing tests and binary/hardware
checks. All 132 staged files were verified unchanged before these note edits.

Offline follow-up names the native TAA blend/jitter-enable fields and the
epoch-reset path before projection setup. Their actual gates and later-frame
behavior were not captured, so zero jitter's cause is still unproven. Next:
implement a bounded CPU sequence of all jitter gates and projection samples
at the existing producer hook, retaining the one-item GPU diagnostic budget.
That recorder is now implemented in v0.14.0 above; no unchanged rerun is needed. GPU upload,
nonzero jitter sign/scale, motion/depth and temporal contents remain unverified;
modern SDK integration is inactive. The game is closed with original Off/TAA.

## v0.13.0 rejection and v0.13.1 correction

Updated 2026-09-23: [v0.13.0 run 41776](projection-jitter-probe.md#run-41776-existing-capture-accepted-projection-observation-rejected)
preserves one producer/four preparations/one correction/one Evaluate and a
615-record completion stop with flags 0. The user reported a brief freeze,
turned Off/applied/exited normally; release, shutdown, cleanup and original
settings are verified. The optional projection snapshot rejected after 14
reads at checkpoint 2; its matrices/jitter were not read.

Offline follow-up found a base-only camera type gate that excludes a verified
derived class with the same projection slot. This is a concrete guard flaw,
but the old packet did not retain its actual vtable/rejection condition, so
it cannot prove the run's precise cause. [v0.13.1](camera-type-probe.md) accepts
the two verified types and records both values. All 45 native and 52 Python
tests, independent derived-type hook-fixture review, binary/unwind and hardware
completion checks pass. The resulting v0.13.1 run is recorded above; no unchanged
v0.13.0 rerun is needed. Modern SDK,
shader/raster jitter and GPU temporal content remain unqualified/inactive.

## v0.13.0 implementation before run 41776

Updated 2026-09-23: [v0.13.0 projection/jitter capture](projection-jitter-probe.md)
was built and verified offline before the run above. The camera and
`fJitterOffset` dispatch can select different tables. Static tracing establishes
the camera setter/epoch and `fProj=P*J*D`, `fNoJitterProj=P*D` CPU constant path.
The new optional observation copies the actual stored jitter, 20-entry candidate,
viewport and both matrices at the existing producer hook, with bounded reads
and unchanged one-item limits. It adds no hook or GPU command.

All 45 native tests and 48 Python tests pass, along with 512 multiply, 64 copy
and 24 projection-chain machine-code replays, binary/unwind audits and the real
D3D12 fixture. A missing projection snapshot preserves the previous diagnostic.
Next: one controlled main-menu run to test these CPU values. Shader execution,
raster jitter, motion/depth contents and temporal alignment remain unverified;
modern SDK integration is inactive. The linked note records deployment,
rollback, exact identities, and the read-only preflight/capture procedure.

## Accepted v0.12.0 baseline

Updated 2026-09-23: [v0.12.0 run 4768](publication-receipt-probe.md#run-4768-publication-binding-and-completion-stop-accepted)
accepts one publication-bound CPU work item and the completion-bounded recorder.
Source/publication counter and epoch 10791, bank 1, become 10792, bank 0 during
preparation/consumer/Evaluate while the same descriptor/payload and original
resources remain bound. One producer, four preparations, one depth correction
and one consumer/Evaluate complete without guard rejection. All 17 queries and
resource inspection complete; the native TAA fallback continues.

Submitted-order replay finds the expected state declarations and binds Reset
216, Close 324, submission 343, Signal 345 and completed query 518 at value
10816. The journal stops on that completion and drains with flags 0/frontier
518. This accepts the recorder's stop behavior, not GPU or temporal-content
correctness. The user turned Off/applied/exited normally; Release and both
Shutdown calls succeed, cleanup clears and settings match original Off/TAA.
The unchanged DLL and all 127 staged files were verified before updating notes;
43 CTests, 42 Python tests and binary/assembly/hardware checks still apply.

The subsequent offline projection trace is implemented in v0.13.0 above;
bounded GPU input-readback design remains future work. Resource lifetimes,
unobserved cross-queue uses, motion/depth convention and temporal image contents
remain unverified. Keep `gpu_safe=false`; no unchanged rerun or modern SDK
integration is requested by this baseline result. Saved settings are Off/TAA.

## Previous v0.11.0 result and offline trace

Updated 2026-09-23: [v0.11.0 run 17204](frame-packet-probe.md#run-17204-producerpreparation-counter-mismatch)
stopped before Evaluate. Its valid producer snapshot sampled renderer frame
24291; the valid first preparation sampled 24292. Renderer identity, rectangle
and reset bits match, but the new same-counter guard rejects the item. There
were four rejected preparations, zero depth corrections and zero Evaluate;
native TAA fallback continued after the user's brief freeze. The markerless
journal reached capacity and retains overflow; completion stopping is untested
in this game run.

The user turned DLSS Off and exited normally. One Release and both Shutdown
calls succeeded; cleanup cleared and saved settings match original Off/TAA.
All 125 staged files and the deployed DLL/sidecar were unchanged at exit, so
the existing 38 CTests, 37 Python tests and binary/hardware checks still apply.
The [offline queue trace](queued-work-provenance.md) now establishes the
producer-return -> queue-entry -> descriptor -> payload link. Eight code spans,
43 instruction checks and 96 owned-memory executions of the enqueue leaf pass.
Preparation can select the opposite command bank; its epoch comes from a
separate counter. Queue sorting can move entries, so capture the descriptor/
payload links immediately rather than retain the entry address. That receipt
is now implemented in v0.12.0 above. No universal +1 offset is assumed. Shader
jitter binding and temporal-content acceptance remain open.

## Accepted baseline and offline analysis

Updated 2026-09-23: [v0.10.0 run 47820](guarded-depth-probe.md#run-47820-one-corrected-item-recovery-and-clean-exit)
accepts the guarded CPU path: one producer, four preparations, one depth
correction and one completed consumer/Evaluate, with no guard rejection.
All 17 parameter queries and resource inspection completed; subsequent work
was suppressed through the TAA fallback. The user reported a brief freeze,
then turned DLSS Off and exited normally. Release and both Shutdown calls
succeeded; saved settings exactly match original Off/TAA. All 33 tests and
binary audits still apply to the unchanged native build.

The [offline submission review](submission-order-and-temporal-inputs.md) now
reconstructs the expected declarations at Evaluate: color/motion `0x40`,
depth `0xC0`, output `0x8`. Submission 395, same-queue Signal 397 and completed
query 596 form a retained API chain. Six relevant transitions were recorded
after the CPU marker on a list submitted before the Evaluate list. The
original incomplete snapshot and journal overflow remain coverage blockers;
this is not GPU correctness or frame-alignment acceptance.

Static analysis also locates frame-indexed jitter tables with 18- and 20-entry
addressing. Active values, units and shader alignment remain unverified.
The new offline analyzer passes 34 synthetic tests. Next implementation:
bind producer-frame/history/rectangle/reset/jitter evidence through Evaluate,
and stop journal admission after a bound submission's completed-value
observation, preserving failure flags and draining in-flight records. That
design is now implemented in v0.11.0 above. The game remains closed with Off/TAA;
no unchanged rerun or modern SDK integration is justified yet.

## Earlier lifecycle milestones

Updated 2026-09-22: [run 45100](native-taa-fallback.md#run-45100-recreation-and-menu-toggle-accepted)
accepts v0.9.2 fallback recreation and deliberate menu toggling. The user
reported selectable On after the requested save/menu return, and again after
Off/apply/On/apply. Evidence records three handle generations, three releases,
34/34 successful sizing queries, zero Evaluate and no handle-read fault.
The final sampled handle, creation list and copied-record binding were clear.
The user exited normally; saved settings exactly match original Off/TAA.

The five-minute text trace contains the first two Create/Release pairs. The
third generation and final retirement are corroborated by exact-build,
read-only bridge metadata after that trace ended. Shutdown wrapper returns
were outside the captured interval. This accepts the tested menu/save sequence,
not Alt-Tab, resolution/device recreation, cutscenes or GPU/image equivalence.
All 29 CTests still apply; native sources and the deployed DLL are unchanged.

The next implementation above builds on this accepted fallback: integrate the
offline-tested depth selector under exact caller/resource guards and validate
a bounded producer/Evaluate path, retaining native TAA when optional work
cannot be admitted. Depth correction is not enabled by this run. Modern SDK
integration still requires resource state/frame alignment and temporal-input
validation; no additional unchanged fallback run is needed.

[Run 19484](native-taa-fallback.md#run-19484-sizing-fix-accepted-lifecycle-still-single-use)
accepts the v0.9.1 sizing-limit fix: 22 queries, 20 successes, including four
beyond the 16 retained records. The user reported a normal scene but DLSS
greyed out Off after returning to Options. One Create and one automatic
Release succeeded; no Evaluate was called. That earlier build failed the
menu/scene lifecycle check.

An offline executable linked to the v0.9.1 policy library reproduces the
20 successes and two post-Release sizing rejections. It also confirms that a
second Create is rejected and a new copied-handle address conflicts with the
first binding. The lifecycle's ten-event ledger is another operational limit.
v0.9.2 fixes these together for the explicit fallback mode. Earlier capture
modes keep their limits. The initiating host event for the first Release
remains unobserved; the offline tests do not establish why the host released it.

The user confirmed normal exit. Settings already match the original Off/TAA
hash; no settings edit was needed. v0.9.2 is the accepted baseline and is
preserved when staging v0.10.0 in the existing MO2 mod. The new mode enables
only the bounded depth probe; modern SDK work remains inactive. Run 17772
below is historical.

## Earlier run 17772: admission stopped before Evaluate

The user enabled DLSS once after ARMED and reported a crash. Host/provider
preflights, the loaded bridge hash and all fourteen live code gates passed.
The existing null-backing fault occurred at host `+0x22927F2`, renderer counter
**22774**, preparation thread 17864. The complete 105-read work item and
32-read tracker snapshot have stable anchors and matching epochs.

The selected engine state was **0x8**, request **0x4**, with prior active state
0x800. The three-frame recorder permits only 0x800 or 0x4, so it reported
`tracker-branch`, applied **zero corrections**, forwarded the fault normally
and detached. All three Evaluate mailboxes stayed idle. The revised checkpoint
review was **not exercised**; this is neither acceptance nor a demonstrated
failure of that revision.

This state/branch was already captured in [run 4568](depth-state-timeline.md).
The rejection exposes a limitation in this test's chosen admission policy,
not a newly discovered crash site. Another unchanged launch would depend on
which tracker state happens to be present.

The stopped journal retains **204 complete calls / 469 barrier descriptions**,
zero pending/failed records or flags, and CPU orders 1 through 408 exactly.
Its final copy is identical. There are five natural fence queries and one CPU
signal, but no Evaluate marker or barrier naming the selected depth resource.
No GPU completion, depth content or reconstruction acceptance follows.

The crash log and exact saved-On settings are archived under ignored
`local/scoped-checkpoint-20260922/`. After verified game exit, only DLSS was
restored Off, reproducing the original settings hash. TAA, DX12, fullscreen
2560x1440 and the staged binaries/sidecar remain unchanged. The independent
`review_17772.py` validates transport/order, the historical state comparison,
binary identities, settings recovery and 59 unchanged native/test/build files.

## Root issue: preparation disagrees with producer and consumer

The original host remains version 0.0.0.0, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
The analysis capture SHA-256 is
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
Both hashes and all fourteen gate spans were rechecked before decoding the
selected instructions offline. No binary was modified.

| Stage | Recorded behavior |
| --- | --- |
| Texture construction, `+0x239C150` | Initially stores the backing at owner+96; kind 6 moves it to +240 and clears +96. |
| Work-item producer, `+0x23913F0` | Calls getter `+0x255CE10`, selecting view-container member +160. |
| Actual work item, run 17772 | Member +160 is kind 5, primary +96 populated, secondary +240 null. The distinct raw-depth member +144 is kind 6 with the opposite arrangement. |
| Subtype-3 preparation, load `+0x229E892` | Unconditionally obtains the depth backing from owner+240, then calls the original tracking helper with engine request 4. |
| NGX consumer, `+0x2383CF0` | Obtains Depth from the same payload owner's +96 backing and that backing's native pointer +32. |

The storage-slot mismatch is confirmed for this diagnostic configuration.
Whether other supported native configurations take a different resource path
remains open. Previous corrected Evaluate captures establish an R32_FLOAT
description for the selected depth in those runs, not its content convention.

The hash-recorded helper predicts that same-epoch state 8/request 4 merges to
engine state 0xC and searches the active record's earlier descriptor for a
matching backing, potentially updating its barrier record. Run 17772 did not
execute that corrected branch or capture that earlier linked record. Engine
state masks are not D3D12 state masks, and CPU recording is not GPU completion.

## Historical implementation scope after run 17772

1. Design a correction at this caller's backing selection, preserving the
   original tracking helper and its bookkeeping. Restrict it to a structurally
   verified subtype-3 kind-5 owner with a valid primary/native/tracker and absent
   secondary. Do not substitute the distinct raw-depth object, alias owner
   fields, bypass the helper or widen its behavior globally.
2. Validate the selector and the original helper's relevant branch effects
   offline, including the already-observed 0x4, 0x8 and 0x800 cases and rejection
   cases. Distinguish locally replayed behavior from live GPU evidence. A
   longer-lived diagnostic must not depend on a repeatedly replenished
   exception-correction budget.
3. Specify rendering output and rejection behavior before calling this stable:
   the current bridge deliberately returns NotImplemented and writes no
   reconstructed output. Fixing preparation alone cannot establish a valid
   displayed frame. A fallback must avoid activating the known defective path;
   returning its original null argument is not a usable fallback.
4. Request another game run only after those implementation choices are
   reviewable and the relevant captured failure/branch fixtures pass. State its
   hypothesis, terminal condition and decisions in advance. Recorder defects
   should first be reproduced against saved evidence.

The next milestone is a correctly bound, stable observation/rendering path,
followed by depth-content, motion/jitter and frame-alignment validation. Modern
DLSS/DLAA integration remains after those prerequisites. The v0.8.1 build and
its rollback are documented in [checkpoint preparation](scoped-checkpoint-review.md).
