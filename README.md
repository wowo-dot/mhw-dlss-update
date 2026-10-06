# MHW DLSS Update

[First same-frame DLAA replacement verified in-game](docs/research/first-inline-dlaa-result.md).
One frame passes exact output and binding checks; continuous DLAA is next.


Open-source research into modern temporal anti-aliasing and upscaling for the
Windows PC version of Monster Hunter: World.

Latest accepted: [nine live native binding snapshots](docs/research/native-binding-snapshots-result.md).
v1.16.0 verifies the game’s root/PSO state and exact command-list generations,
alongside nine retained-history offscreen DLAA outputs. Normal exit and rollback
are complete. Next: matching-frame SDK insertion and native output replacement.
The output handoff already passes owned GPU fixtures; in-game presentation
and state restoration remain to be implemented. Earlier entries are historical.

Implementation in progress: [native output handoff and binding restoration](docs/runtime/dlaa-output-handoff.md)
pass owned WARP/RTX fixtures. The next live v1.16.0 capture validates native
bindings before same-frame SDK insertion; on-screen DLAA remains unfinished.

Latest accepted: [nine live DLAA frames with retained history](docs/research/dlaa-nine-frame-accepted.md).
v1.15.3 passes the full frozen capture review, including seven matched jitter
samples, every selected input copy and all nine private SDK outputs. Closing
the journal before hook removal resolves the live overflow. Fixed-camera
previews are coherent, with smoother edges and some texture softening.
The game was terminated at the user's request after capture; the exact previous
DLL is restored and all configurations are unchanged. Next: bounded presentation
and then continuous operation/lifecycle tests. On-screen DLAA remains unfinished.
Earlier entries below are historical.

Latest: [depth markers fixed; journal fills during cleanup](docs/research/dlaa-journal-capacity-result.md).
The v1.15.2 live run preserved every depth marker and drained the callbacks,
but continued journaling during hook removal exhausted the fixed event budget.
It stopped before SDK evaluation. Normal exit, exact DLL restoration and the
537-file archive are complete. Next: close journal admission after callback
drain and before hook removal, with offline teardown-traffic tests.

Latest: [nine live DLAA outputs and a remaining recorder issue](docs/research/dlaa-nine-frame-result.md).
Packet-address reuse now passes in the live game. Nine private outputs retain
history with seven matched jitter samples; all ten individual component checks
pass. The full frozen review rejects two trailing, unselected depth entries
without submission markers. Next is reproducing/correcting recorder shutdown.
Normal exit, previous-DLL restoration and the 411-file archive are complete.
The display still uses native TAA; sustained on-screen DLAA remains unfinished.

Earlier preparation: [nine-frame DLAA accumulation](docs/runtime/dlaa-history-sequence.md).
The [three-frame image review](docs/research/dlaa-history-image-review.md) shows
coherent outputs and smoother edges. The longer diagnostic adds seven distinct
scoped jitter samples while retaining history and the established input path.
Offline native/GPU/verifier checks pass; live nine-frame capture is pending.
On-screen DLAA and sustained temporal-quality validation remain unfinished.

Latest accepted: [live DLAA history with paired jitter](docs/research/first-live-dlaa-history.md).
Three consecutive private outputs pass full verification with history retained
after frame one and exact projection/input association. Normal exit and exact
previous-DLL restoration are complete. Next: image review, longer bounded
accumulation and controlled presentation. Sustained visible DLAA remains unfinished.
Earlier entries below are historical.

Accepted: [first live DLAA outputs](docs/research/first-live-dlaa-output.md).
The running game device successfully creates a DLAA feature and evaluates
three private full-resolution frames. Input conversion, output pixels, GPU
completion and SDK cleanup pass the full review. Previous DLL restored.
Prepared next: [bounded retained history with paired jitter](docs/runtime/retained-history-dlaa.md), then on-screen presentation.
The current reset-history control still displays native TAA.
Older entries below are historical.

Latest: [live SDK capability result](docs/research/live-sdk-capability-result.md).
Live capture and private GPU conversion pass; SDK capabilities fail before
feature creation. Isolated controls identify initialization order as a strong
lead, and capability logging is corrected. Previous DLL restored; next is
mapping the game's initial NGX setup. In-game DLAA remains unfinished.

Latest result: [first offscreen DLAA SDK output](docs/research/first-dlaa-sdk-output.md).
The public NVIDIA SDK successfully evaluates three captured MHW frames at
native resolution. All output RGB values are finite and written, GPU validation
is clean, and feature creation/evaluation/cleanup pass. The preceding
[live GPU input handoff](docs/research/live-owned-temporal-inputs-result.md)
also passed. Next: SDK evaluation on those live-owned textures, then continuous
jitter/history and presentation. Reset-per-frame replay does not establish
temporal quality or working in-game DLAA. Older milestones below are historical.

Latest result: [camera-pan depth timing, run 31044](docs/research/ordinary-taa-depth-pan-result.md).
Three verified depth/color/motion sets support current depth/color timing during
camera movement. All 12 qualified static-scene comparisons favor their matching
frame and zero pixel offset; actor/water exclusions and ambiguous controls are
preserved. Capture and rollback are complete. Next: typed input views and the
state/lifetime contract at the evaluation point, before bounded SDK integration.

Latest result: [paired depth/color/motion, run 25300](docs/research/ordinary-taa-depth-pixels-result.md).
Three full-resolution sets pass the frozen reviewer, including matched native
depth copies, state restoration and completed TAA generations. Depth silhouettes
match the scene and integer-grid edge agreement peaks at zero offset throughout
the tested regions. Capture and rollback are complete. Next: a controlled pan
with the same recorder to check temporal depth alignment, then typed views and
the SDK evaluation-point contract. DLAA output remains unverified.

Latest result: [native depth-copy routes, run 45404](docs/research/ordinary-taa-depth-routes-result.md).
A supplemental audit verifies nine copies across three selected frames,
including a copy from original scene depth to the candidate on TAA's own list
before Dispatch. The frozen full review remains rejected for incomplete
unselected tail generations. Capture and rollback are complete. Next: paired
depth-pixel readback and content alignment; SDK output remains unverified.

Latest result: [separate depth candidate, run 54612](docs/research/ordinary-taa-scene-depth-result.md).
Three verified color/motion pairs accompany a stable, full-size R32_FLOAT
candidate through native TAA dispatch. Its depth contents and GPU state remain
unknown at that point. The run is archived and the previous DLL restored.
Next: associate it with the native depth-copy destination/source and frame,
then qualify paired depth pixels before typed input views and SDK evaluation.

Latest result: [larger jitter run 44552](docs/research/ordinary-taa-strong-jitter-result.md).
Three pairs pass capture verification after one weak preview is skipped. The
larger shift reproduces color jitter direction and static-motion jitter exclusion.
Exact color displacement remains sensitive to the fitting method and region;
no scale correction is inferred. Capture, archive and exact rollback are complete.
Next: typed-binding audit and same-frame real-depth/color/motion input assembly
for a bounded DLAA test. SDK output and sustained rendering remain unverified.

Latest result: [bounded color/motion jitter run 35156](docs/research/ordinary-taa-jitter-result.md).
Three projection-associated pairs pass capture verification. Color responds in
the predicted jitter direction; roughly 98–99% of exact-control selected pixels
retain zero motion during stimulus and recovery, supporting static-scene jitter
exclusion. Precise color-shift agreement remains unresolved; known-shift controls
also show estimator bias. Capture, archive and rollback are complete; settings
are unchanged. Next: a larger bounded native jitter sample for color calibration.
Same-frame depth, typed views and SDK integration remain outstanding for DLAA.

Latest result: [paired color/motion run 43820](docs/research/ordinary-taa-paired-result.md).
Three consecutive pairs pass capture verification. Independent scenery matches
agree with the current frame's decoded motion to about 0.12 pixels median
disagreement, supporting its sign, native pixel scale and frame association in
this scene. Capture and rollback are complete; settings are unchanged.
Next: bounded projection-jitter validation against paired color and motion.
Jitter inclusion and sustained DLAA integration remain unverified.

Latest result: [camera-pan motion run 34276](docs/research/ordinary-taa-motion-pan-result.md).
All three images pass readback verification. Camera panning produces finite
nonzero vectors throughout the frame, with the predicted horizontal sign on
about 94% of pixels. Camera-motion coverage is supported in this view;
quantitative scale/frame alignment remain unverified. Capture and rollback
are complete. Next: matched color/motion displacement validation.

Latest result: [motion-pixel readback run 48724](docs/research/ordinary-taa-motion-result.md).
Three full-resolution packed images pass source, state restoration, GPU
completion and file-integrity checks. All decoded vectors are finite; about
6.9% are nonzero under the stationary-camera request. Capture and rollback
are complete. Next: controlled camera-pan validation of motion coverage.

Latest result: [TAA submission/completion run 14268](docs/research/ordinary-taa-submission-result.md).
Three consecutive request frames match distinct command-list generations,
actual submissions, completed private fences and later reset retirement.
The offline reviewer accepts all three. Capture and exact rollback are
complete; settings are unchanged. Next: bounded R32_UINT motion-pixel readback.
This run added three private Signals and no texture copies or rendering changes.

Latest result: [native TAA recording run 53044](docs/research/ordinary-taa-consumer-result.md).
Three consecutive requests match their native Dispatch calls. Each records
GBuffer4 transitioning from render-target to combined shader-read state before
Dispatch. The shader is byte-identical to run 4376. Capture and rollback are
complete; settings are unchanged. Next: command-list submission/lifetime proof
and a bounded motion-pixel readback. This run added no GPU commands.

Latest shader result: [TAA shader run 4376](docs/research/ordinary-taa-shader-result.md).
Three accepted requests identify the actual compute shader and its bindings.
The shader establishes GBuffer4's packed half-float motion decoding, component
order and reprojection sign, plus its depth/matrix fallback. Real motion pixels
and producer coverage remain unverified. Capture is archived, the previous DLL
is restored and settings are unchanged. Next: controlled motion readbacks.

Latest accepted request result: [ordinary TAA input run 44924](docs/research/ordinary-taa-input-result.md).
Three consecutive frames establish current-color/G-buffer bindings and the
expected output/history rotation. The named depth slot is a 1×1 fallback;
the separately validated scene-depth path remains necessary. The observer
closed cleanly and the previous DLL is restored, with settings unchanged.
Next: compute-shader/typed-view mapping and motion-content validation.

Latest accepted depth result: [three jitter directions, v0.9.1 run 10232](docs/research/depth-sequence-result.md).
Five request-matched images pass the archived review. On 1.53 million static
depth pixels, all three measured shifts agree with prediction within 0.001
pixel per fitted axis. This is bounded evidence in one scene, not sustained
jitter or working DLAA. Capture, separate pixel analysis and rollback are
complete; settings are unchanged. Next: color/motion input association.

Previous diagnostic: [v0.9.0 five-sample capacity rejection](docs/research/depth-sequence-capacity-result.md).
Three injections restored and five private Signals completed, but the recorder
filled its 4096-entry history before retirement validation. No images were saved;
the run remains rejected. Archive and exact rollback are complete, with settings
unchanged. The [v0.9.1 correction](docs/runtime/projection-depth-history.md)
retains 8192 entries and is validated by the accepted run above (5132 entries).

Previous accepted pixel result: [first jittered depth triplet, v0.8.4 run 48276](docs/research/depth-triplet-result.md).
Three independently stamped depth images pass the archived reviewer. Across
1.45 million control-selected static depth pixels, the measured shift is about
(0.2342, 0.0225) pixels versus (0.2344, 0.0216) predicted. This is one-sample
evidence that scoped jitter reaches rasterized scene depth; continuing jitter,
full-renderer coverage and working DLAA remain unverified. Capture, pixel
analysis, archive and verified rollback are complete; settings are unchanged.
The build passed 241 native and 543 Python checks.

Previous result: [matching projection/depth request, v0.8.2 run 42232](docs/research/depth-alignment-result.md).
The selected projection and original depth-copy request share frame/epoch 51739.
The copy was recorded first on a worker, then submitted after the selected draw
on the same DIRECT queue. The unchanged reviewer accepts that association and
all 3,686,400 depth values. This closes the zero-jitter request-alignment control;
capture, archive and verified rollback are complete. Next is an aligned
baseline/jitter/restoration pixel comparison. Complete pixel-production coverage,
raster jitter and working DLAA remain unverified. The prepared
[v0.8.4 triplet](docs/runtime/projection-depth-triplet.md) adds independently
stamped baseline/recovery depth controls around one scoped jittered sample,
with three readback copies total. The [v0.8.3 run](docs/research/depth-triplet-timing-result.md)
exposed a delayed draw-return marker and saved no pixels. v0.8.4 corrects that
ordering while retaining the copy checks. The successful triplet and scoped
pixel measurement are described above.

The [v0.8.2 control](docs/runtime/projection-aligned-depth.md) corrects the CPU-order
selection problem identified after [v0.8.1 run 43440](docs/research/depth-copy-frame-result.md),
which captured a request one frame after the selected projection. It requires
an exact source/request stamp match instead of assuming a fixed frame offset.

Previous result: [first game depth pixels, v0.8.0 run 51816](docs/research/depth-readback-result.md).
The [bounded readback](docs/runtime/projection-depth-readback.md) saves all
3,686,400 depth values at 2560×1440 after private fence completion and
command-list retirement. The image shows coherent character and environment
geometry. Review and rollback are complete; settings are unchanged. The
[v0.8.1 diagnostic](docs/runtime/projection-depth-frame.md) now traces the original
copy-request frame through its native packet to readback. An intervening depth
clear rules out simply assigning the prior pixels to the earlier selected draw.
Complete pixel-frame alignment, raster jitter and working DLAA remain unverified.

Previous result: [v0.7.1 run 6868](docs/research/depth-copy-submission-result.md)
records 52 full-resource depth copies, each associated with its submitted
command-list generation. All 13 observed cycles repeat four copies into two
full-resolution R32_FLOAT textures. One distinct COPY_SOURCE stage provides
a structural readback candidate. Capture, reference cleanup and rollback are
complete; settings are unchanged. Next: a bounded game-depth pixel readback.
No game GPU commands were added, and actual depth contents, raster jitter and
working DLAA/DLSS remain unverified.

The [v0.7.0 partial run](docs/research/depth-copy-result.md) remains rejected;
v0.7.1's independent per-call references resolve its recorder overlap failure.
See the [copy recorder](docs/runtime/projection-copy-recorder.md) and
[offline map](docs/research/depth-copy-map.md) for the implementation and scope.

Previous result: [v0.6.0 run 21900](docs/research/queue-synchronization-result.md)
observes the depth-write declaration across adjacent DIRECT submissions and
a following graphics-fence signal whose value is reached by a native read.
An offline correction handles a same-thread Wait nested inside a later Execute;
83 synchronization tests pass without another game run. The original review
and build remain archived. Capture and rollback are clean, and settings are
unchanged. Next: audit depth accesses and choose a stable readback point.
Complete resource safety, GPU contents and working DLAA/DLSS remain unproven.

Previous result: [v0.5.0 run 47376](docs/research/projection-submission-result.md)
connects the selected draw's actual Reset/Close generation to a returned DIRECT
queue submission. The preceding DIRECT batch contains the depth-write transition
and clear. Other queues are also observed; depth state, synchronization and GPU
completion remain unqualified. Capture and rollback are clean; the previous
diagnostic DLL is restored. Next is an offline audit of the adjacent batches
and the synchronization evidence needed. Preparation passed 110 native and 205
Python checks.

Previous result: [v0.4.0 run 1544](docs/research/depth-view-state-result.md)
matches the actual writable D32_FLOAT depth-view creation and lifetime to the
selected draw. Earlier history contains 112 transitions for that resource
on six other command lists; queue submission order is the next missing link.
The capture stopped cleanly, the game exited normally and the previous DLL
is restored. GPU contents, raster jitter and working DLAA/DLSS remain unproven.

Latest offline follow-up: [depth-view lifetime and earlier-state mapping](docs/research/depth-view-state-map.md)
identifies DSV creation, recycled handles and a barrier flush before depth
clears. The native command-cache reset does not issue D3D12 Reset. Ninety
instruction checks, SDK layout checks and 1,861 synthetic assignment cases
pass. The [v0.4.0 recorder](docs/runtime/depth-view-state-recorder.md) now
implements view-lifetime observation and earlier API history. Release checks
pass 101 native and 164 Python tests; its first game result is summarized above.
See the versioned preparation/deployment receipts in the current-state notes.

Latest accepted result: [v0.3.1 run 11804](docs/research/projection-attachment-result.md)
links the projection-buffer binding and recorded indexed draw to a validated
2560x1440 `R32_TYPELESS` depth attachment, with zero color targets. Resource
identity and description agree across all three query phases. All 39 live
code gates passed; zero faults, drops or pending calls, and hooks disabled
and drained. The game exited normally, the previous DLL is restored and all
four configuration hashes are unchanged. The build passed 76 native and 117
Python preparation checks. Exact depth-view settings, earlier resource-state
history, GPU execution and pixel displacement remain unverified. The mapping
above prepares the next view/state qualification before GPU readback. See
[current state](docs/research/current-state.md) for the archive and next decision.

Previous accepted result: [run 46324](docs/research/projection-jitter-binding-result.md)
follows one controlled nonzero jitter sample through `fViewProj`, publication,
backend consumption and the expected constant-buffer binding on a recorded
indexed draw. Camera restoration and zero-jitter recovery pass; baseline and
recovery constants are identical. One recovery admission deferred successfully,
with zero capture faults or critical drops. The game exited normally, the
previous DLL is restored, and settings are unchanged. The completed v0.2.1
probe has 38 native/59 Python preparation checks. Actual GPU execution and
pixel displacement remain unverified. The [offline attachment/PSO map](docs/research/projection-attachment-map.md)
now passes 93 instruction checks and 320 synthetic replay cases. The
[v0.3.0 read-only attachment recorder](docs/runtime/projection-attachment-plan.md)
is implemented, with 63 native and 102 Python preparation checks. Its first
[game capture, run 5208](docs/research/projection-attachment-first-run.md),
observed a depth-only bind and native depth-write state, but failed resource
identity validation. An owned D3D12 reproduction exposed an invalid identity
comparison. The [v0.3.1 correction](docs/runtime/projection-attachment-identity.md)
passes the production validator on a real owned depth texture and in game
run 11804 above. The rejected run 5208 retains its original verdict.

Earlier partial result: [run 42212](docs/research/projection-binding-stimulus-result.md)
shows the single native jitter sample changing CPU `fViewProj` as predicted,
with successful camera-field restoration and matching backend consumption.
The full experiment was rejected on recorder contention before recovery/draw;
the game exited normally and settings were unchanged. Run 46324 above completes
the corrected CPU/binding experiment; GPU/raster use remains unverified.

Previous accepted result: [run 31912](docs/research/view-projection-contract.md#run-31912-zero-jitter-contract-accepted)
validates the live view-projection matrices and the native root layout for a
matched D3D12 indexed draw. The stable compact layout maps root parameter 9
to VS b0, matching the captured vertex shader's `fViewProj` buffer. All 36 live
code gates passed; the capture ended with zero faults/drops. The game exited
normally, its previous DLL was restored, and settings were unchanged. This was
a zero-jitter baseline, followed by the nonzero experiment above.
GPU execution, raster validation
and working DLAA/DLSS remain unproven.
See [current state](docs/research/current-state.md).

The completed [v0.1.3 contract recorder](docs/research/view-projection-contract.md)
has 24 native/34 Python preparation tests and a separate twelve-case static
root-builder replay. It extends [run 6444](docs/research/projection-binding-result.md),
which first followed the CPU source/publication/consumption/root-CBV/draw chain
and identified `fViewProj` as the captured vertex shader's input.

Earlier [run 17072](docs/research/scoped-camera-jitter.md#run-17072-cpu-stimulus-and-recovery-accepted)
demonstrates one controlled nonzero native jitter sample changing CPU `fProj`
constants as expected, followed by zero-jitter recovery. The binding work builds
on the [offline map](docs/research/projection-upload-binding.md), fixes a
[recorder collision](docs/research/projection-binding-first-run.md), and mitigates
the [allocation failure](docs/research/projection-binding-allocation.md).

Previous accepted result: [track run 42104](docs/research/jitter-track-writer-result.md)
records two TAA2 jitter-enable writes from the same shared track data: one key,
raw value zero. Both change the destination from 1 to 0; source pairs and live
caller-code gates pass. The recorder stopped cleanly and that run's previous
DLL was restored with settings unchanged. An explicit preflight metadata
correction is included in the accepted archive. The next
[scoped camera-jitter experiment](docs/research/scoped-camera-jitter.md) is now
at v1.0.2 passed run 17072, with 36 preparation checks. Run 5152 had installed
its hook successfully but exposed an incorrect eligibility check: the raw
`mUseDLSS` switch can remain true with DLSS Off. The correction checks disabled
sizing and absent NGX wrapper instead, retaining the raw switch as evidence.
No jitter was injected in that earlier run. The completed experiment tests one
native sample at the projection consumer with immediate
camera-field restoration. See [current state](docs/research/current-state.md)
before launching.

Previous result: [writer run 44144](docs/research/jitter-property-writer-result.md)
records two native calls changing TAA2's jitter-enable byte from 1 to 0 with
DLSS Off/TAA. Both return to the same mapped track-value branch. The next
target was its selected source value and key, now observed in run 42104.
The recorder stopped cleanly, the game exited normally, and the previous DLL
is restored with settings unchanged. Archive and static-route checks pass;
the retained observer passed five native and ten Python fixture checks.
See [current state](docs/research/current-state.md) before another launch.

Previous result: [native callback run 37452](docs/research/native-jitter-callback-result.md)
records 768 paired CPU calls across menu, still gameplay and user-reported
camera-pan windows with DLSS Off. TAA2 callbacks execute with jitter already
disabled; later camera setters supply zero and update the frame stamp. The
bounded recorder stopped cleanly, the user exited normally, and the previous
bridge was restored with saved settings unchanged. This led to the writer
capture above. GPU/raster behavior and modern DLAA/DLSS integration remain
unresolved; no identical pan rerun is needed.

Previous result: [bare-profile run 2632](docs/research/jitter-bare-profile.md)
finds the same disabled jitter byte and zero stored camera jitter in menu and
gameplay with no enabled MO2 mods and no detected bridge/mod-loader DLL.
The next question is native assignment/callback order, not whether the bridge
or configured mods are required. Exit and unchanged Off/TAA/profile settings
are verified; that control installed or changed nothing. CPU archive checks pass, while
GPU/raster qualification and modern DLAA/DLSS integration remain open.

Previous result: [disabled-provider run 8164](docs/research/jitter-startup-control.md#run-8164-menu-and-gameplay-remain-zero-in-the-control)
finds the same disabled jitter flag and zero stored jitter in menu/gameplay
with synthetic startup behavior disabled and the backend NGX/context pointer
absent. Active bridge startup substitution is not required for this state;
the writer and remaining mod/observer effects are unresolved. The user exited
normally, original Off/TAA settings are unchanged and the diagnostic sidecar
is restored. CPU archive checks pass; GPU/raster qualification and modern
DLAA/DLSS integration remain open.

Previous result: [pre-enable run 5836](docs/research/jitter-pre-enable-observation.md)
finds the native jitter-enable flag already Off and stored jitter zero in menu
and gameplay with DLSS left Off, before any Create/Evaluate. The first Create
is not required for this state; bridge startup callbacks and other mods remain
possible influences. The next control will isolate bridge startup behavior.
Normal exit was reported and original settings are verified; the trace expired
before exit, so shutdown is unobserved. No native code changed. GPU/raster
qualification and modern DLAA/DLSS integration remain open.

Previous result: [v0.15.0 run 27200](docs/research/jitter-scene-probe.md#run-27200-menu-and-loaded-scene-cpu-samples-accepted)
captures 16 menu and 16 loaded-scene CPU samples. Gameplay uses a different
TAA layer, but its jitter-enable flag also stays Off and stored jitter stays
zero. One Evaluate and the completed submission journal are retained; an
overlapping call leaves the original checkpoint unqualified. On menu return,
the single-use diagnostic refuses recreation, explaining the unavailable
setting. Normal exit, cleanup and original Off/TAA settings are verified.
All 48 native and 78 Python tests apply to the unchanged build. Next is tracing
what disables native jitter; GPU/raster qualification and modern DLAA/DLSS
integration remain open.

Latest result: [v0.14.0 run 30924](docs/research/jitter-sequence-probe.md#run-30924-both-cpu-phases-accepted)
captures 32 valid CPU samples across wrapper-present and wrapper-absent phases.
Stored jitter stays zero; the native TAA jitter-enable byte is Off in every
sample while the other captured predicate clauses pass. One Evaluate and the
completion receipt succeed, with clean shutdown and original settings restored.
All 47 native and 64 Python tests apply to the unchanged build. Next is tracing
the scene/property control and preparing a loaded-scene comparison; the main
menu result does not establish gameplay or raster behavior.

Latest result: [v0.13.1 run 21636](docs/research/camera-type-probe.md#run-21636-cpu-projection-observation-accepted)
captures the verified derived camera, source-stamped CPU projection matrices
and matching viewport. Stored jitter is zero and both matrices are identical,
despite nonzero candidate table samples. One guarded Evaluate and the recorder's
completion stop succeed; Off/apply/exit and original settings are verified.
The unchanged build has 45 passing native and 52 Python tests. Next is bounded
observation of native jitter gates across several frames, not an unchanged rerun.
GPU/raster temporal correctness remains unverified; modern DLSS is inactive.

Latest result: [v0.12.0 run 4768](docs/research/publication-receipt-probe.md#run-4768-publication-binding-and-completion-stop-accepted)
binds one producer's published work through four preparations and Evaluate,
despite a global counter/bank advance. The recorder stops after its matching
completion observation and drains with zero journal flags. The user turned
DLSS Off and exited normally; original Off/TAA settings are verified. All 43
native tests and 42 Python tests apply to the unchanged code. Shader jitter,
motion/depth conventions and temporal image alignment remain unverified;
the new input-validation diagnostic above is the next controlled run.

Current result: [v0.10.0 run 47820](docs/research/guarded-depth-probe.md#run-47820-one-corrected-item-recovery-and-clean-exit) completed one guarded depth correction and Evaluate capture, continued through native TAA, and exited normally after DLSS was turned Off. [Offline replay](docs/research/submission-order-and-temporal-inputs.md) now reconstructs the expected resource-state declarations and a matching fence observation; static analysis locates frame-indexed jitter tables. Coverage and temporal-input validation remain open. The unchanged native build has 33 passing tests; the new offline analyzer has 34. Modern DLSS is not implemented. [Current findings](docs/research/current-state.md).

The repository is currently in Phase 0: observing and minimally restoring the
game's existing NGX/DLSS 1 integration before changing rendering behavior.
`mhw_dlss_probe` is a
bounded diagnostic DLL that observes the game's signed `nvngx_dlss.dll`
create/evaluate/release boundary and the system DXGI swap-chain creation
boundary. It verifies that NGX receives a real D3D12 command list and correlates
its adapter with the queue used to create the swap chain. It does not patch the
protected game executable.

The default smoke-test mode logs only lifecycle calls and the verified API.
Parameter/resource queries are a separate opt-in stage via
`MHW_DLSS_PROBE_QUERY_PARAMETERS=1`; the probe permanently stops querying after
the first protected getter or resource-description exception.

The probe is locked to the recorded executable and feature-DLL SHA-256 values
and to the feature's API, snippet, application IDs, and untouched in-memory hook
prologues. DXGI targets must be executable image addresses owned by the system
`dxgi.dll`. Captures are written by a background thread after the original call
returns. The default limit is 16 evaluations.

The probe has a CMake install target for staging as a distinct, disabled MO2
mod named `dlssTEST`. It must not be copied into the physical game directory.
Staging, controlled offline-run gates, and removal are documented in
[`docs/runtime/mo2-strackers-loader.md`](docs/runtime/mo2-strackers-loader.md).

The current input contract, confirmed MHW evidence, and unresolved runtime
questions are tracked in
[`docs/research/phase-0-requirements-gap.md`](docs/research/phase-0-requirements-gap.md).

`mhw_dlss_unlock` is currently an exact-build, observation-only tracer for the
legacy NGX startup boundary. Version 0.4.1 established loading before game code through
MO2 Force Load, retains the six verified resolver wrappers from v0.4.0, records
all base D3D12 Init lookups while returning their original pointers untouched,
and observes the pinned modern driver's per-feature D3D12 status recorder. The
new boundary is restricted to DLSS feature index 1, API 2, and exact caller RVA
`_nvngx.dll+0x67704`; it reports the raw status without replaying Init or
changing availability. The preceding v0.3.3 cache-classifier bridge was retired
after a clean runtime test recorded zero calls at both patched decision points.
See
[`docs/research/legacy-rtx50-classifier.md`](docs/research/legacy-rtx50-classifier.md)
and the isolated
[`docs/runtime/rtx50-unlock-test.md`](docs/runtime/rtx50-unlock-test.md)
acceptance procedure.

The shared source now builds observer v0.4.2 with the
[610.88 / Windows 26100.9278 identity update](docs/research/driver-61088-revalidation.md).
This standalone observer has not been deployed or runtime-tested on that environment.

`mhw_dlss_bridge_probe` is the next, separate milestone. It targets the pinned
modern-core-to-feature-provider boundary instead of patching DLSS 1. It is
disabled by default. Version 0.7.8 admits `capture-create` and the separate
[`capture-one-evaluate` diagnostic](docs/research/one-evaluate-capture.md), whose
parameters and resource descriptions are now captured as detailed below.
[Run 32832](docs/research/first-handle-lifetime.md)
passed Create success and copied-handle Release before both Shutdowns, with
zero Evaluate calls. ISP remains in Create-only mode.
The bridge substitutes
the complete stateful feature lifecycle as a fail-closed set, never mixes its
state with the old provider, and does not submit GPU work. It does **not** yet
load modern DLSS or render an upscaled frame. See the
[`boundary specification`](docs/research/modern-provider-bridge.md) and
[`first offline test`](docs/runtime/modern-bridge-probe-test.md).
The probe must observe a complete Init-first 14-lookup transaction for each of
DLSS and ISP before either provider can report synthetic Init success
(six lifecycle exports, seven null-only ABI guards, and the generic Populate
callback); if its asynchronous startup worker misses Init, that run stays
pass-through for the missed provider and is reported as inconclusive. The
shared admission check prevents synthetic Init success on the other route.

Version 0.5.2 confirmed DLSS availability and successful status through the
modern/legacy GetParameters returns, but the user still saw no DLSS menu row
and no Create call was recorded. The captured game code also requires ISP
availability. Version 0.5.3 confirms that ISP returns availability `0` and
Init status `0xBAD00002` through both core layers. Version 0.6.0 confirmed both
providers available but crashed in an old DLSS callback published through the
parameter object. Version 0.6.1 contains that callback and captures its inputs
before returning unsupported. Accepted run 34496 reached the Display menu with
DLSS visible but unavailable, captured its 2560x1440 sizing request, and completed
both bridge shutdown paths, with no Create/Evaluate in that run. See the
[`crash analysis and correction`](docs/research/optimal-settings-callback-crash.md) and
[`two-provider capture contract`](docs/research/two-provider-create-capture.md).
Version 0.6.2 adds [verified 1:1 sizing metadata](docs/research/diagnostic-optimal-settings.md)
to attempt Create capture while still rejecting feature creation without a
handle. It passes 11 tests. Run 20164 confirms four successful sizing responses,
a selectable DLSS setting and both bridge shutdown paths. The user stayed in
Options while On. [Run 19800](docs/research/first-create-capture.md) then left
Options with On applied and captured the first DLSS Create with a verified D3D12
command list and 11 scalar queries. The intentional no-handle failure and both
bridge shutdown paths completed. The v0.7.0 handle-lifetime test has passed;
[loaded-scene attempts](docs/research/loaded-scene-evaluate-gate.md) still recorded
zero Evaluate calls, so upstream render scheduling is under investigation.
Version 0.7.1 adds [read-only renderer gate samples](docs/research/renderer-gate-observation.md)
after Create, with bounded scalar reads and final sizing-budget telemetry.
All 13 tests pass. Run 35100 records two clean early samples with all selector
gates ready, but no Evaluate; an early Release consumes the one-shot lifetime
and later sizing requests reject admission. Run 43380 then reaches a loaded
save and captures 12 valid ready-gate samples through a 23.2-second lifetime,
with all 11 sizing calls successful and clean exit cleanup. Evaluate is still
absent. The next investigation is temporal-layer activation and enqueueing;
the probe does not instrument those execution points. No repeat gate test is
requested. Version 0.7.2 adds [historical shader-cache and deferral observations](docs/research/temporal-activation-observation.md)
before and after Create. Version 0.7.3 retains those observations and updates
the exact driver/Windows identities and relocated system resolver RVAs after
binary comparison. All 14 tests pass. [Live inspection](docs/research/taa-activation-depth-crash.md)
confirmed that the diagnostic's 1:1 sizing leaves saved AA Off unchanged and
TAA2 disabled. Selecting TAA enabled that path; run 44972 then crashed in host
depth-resource preparation before Evaluate. Run 8280's DLSS-Off baseline then
identified two depth representations with different backing fields.
[Run 14388's external exception capture](docs/research/depth-exception-capture.md)
confirmed that the actual depth work item reaches a null secondary backing
despite a present primary backing. All 105 reads completed. The separate
`mhw_depth_exception_capture` tool installs nothing into the game; its tests
bring the suite to 15 passing tests. No further crash is needed to confirm
this mismatch. Its [v0.2.0 one-use argument retry](docs/research/depth-argument-retry.md)
then reached the **first Evaluate capture in run 12196**: all 17 getters
completed, and color, motion, depth and output pointers exactly matched the
corrected work item. The following frame hit the uncorrected fault and crashed;
only one retry was allowed. That run did not inspect resource descriptions or
states. Modern rendering is unimplemented.
Version 0.7.4 adds [first-Evaluate resource descriptions and device identity](docs/research/evaluate-resource-descriptions.md),
plus bounded presentation-creation format observations. **Run 14392 accepted
all four 2560x1440 textures on the command list's device**: color/output
R11G11B10_FLOAT, motion R32G32_FLOAT and depth R32_FLOAT. All ten temporary
references were released without fault. The unique matching swap-chain creation
used B8G8R8A8_UNORM. The following uncorrected frame crashed after the one-use
retry was spent. All 16 tests pass; current resource states, depth/motion
contents, temporal conventions and clean lifecycle acceptance remain outstanding.
Version 0.7.5 adds a [bounded capture of actual ResourceBarrier calls](docs/research/evaluate-barrier-capture.md),
correlated with Evaluate resources and command-list resets. **Run 30272 captured
232 API calls and 705 barriers without gaps**. Color/motion reached declared
shader-read state and output reached declared unordered-access state in the
selected recording. Depth had no matching barrier there and remains unresolved.
All 21 tests and a hardware-adapter fixture pass. These declarations do not
establish GPU execution, initial-state correctness or texture contents.
Version 0.7.6 and external recorder v0.3.0 add a
[bounded depth tracker and cross-list queue timeline](docs/research/depth-state-timeline.md).
All 23 tests, hardware/WARP queue fixtures and the CFG audit pass. **Run 4568
accepted the tracker capture**: depth's active state was engine `0x8`, selecting
the static read-mask merge path to `0xC` when corrected for request `0x4`.
The next fault arrived 16 ms later; the bridge's finished Evaluate/queue timeline
did not reach its log. That portion remains unvalidated. Capture persistence
is addressed by [v0.7.7 and recorder v0.4.0](docs/research/capture-mailbox.md):
an immutable shared capture is saved externally before the next fault proceeds.
All 24 tests and the CFG audit pass, including abrupt producer exit and output
flush before fault forwarding. **Run 38476 saved and cross-checked the complete
first-Evaluate capture**: 244 API calls, 610 barrier descriptions, two depth
matches on other lists and thirty queue events. All retained submissions
precede the relevant lists' current Resets, so final depth state/submission
remains unresolved. Next work is bounded observation beyond Evaluate; do not
repeat the unchanged capture. GPU completion, pointer lifetimes and depth
contents remain unverified. The one-correction limit is unchanged.
That continuation is now implemented in [v0.7.8 / recorder v0.5.0](docs/research/post-evaluate-journal.md):
an append-only journal retains completed calls past the first-Evaluate checkpoint
within the original two-second bound. The external recorder saves both channels
before forwarding the next fault. All 25 tests, the hardware continuation fixture
and CFG audit pass. **Run 24944 accepted current-recording depth producer and
Evaluate submissions** with 387 completed calls, including 77 after Evaluate.
The observed depth declaration chain ends at combined shader read `0xC0`, which
includes the required non-pixel read bit; the summary's exact-equality mismatch
is a reporting limitation. The next uncorrected frame faulted as expected.
GPU completion, texture contents and temporal alignment remain unverified.
The [v0.7.9 / recorder v0.6.0 diagnostic](docs/research/bounded-depth-frames.md)
fixes combined-read reporting and prepares at most three corrections on
consecutive frames, with fresh checks and a 500 ms admission window. All 25
tests pass, including an owned process's three real instruction retries and
normal fourth-fault handling. Run 7148 stopped before any correction because
engine state was already `0x4`; the [v0.6.1 recorder](docs/research/read-ready-depth.md)
now admits that verified branch alongside `0x800`. **[Run 46868](docs/research/three-frame-result.md)
accepted three consecutive corrections and the intentional fourth-fault stop.**
Its 807-call journal includes closed/submitted depth recordings ending at `0xC0`
in each interval. The first-Evaluate capture stays one-shot; later journal records
observe CPU calls. Next work is bounded per-Evaluate input capture and actual
fence-completion evidence; do not repeat the unchanged test.
The staged setup preserves the previous DLL and graphics configuration and
currently has saved DLSS Off, TAA, DX12 On, FullScreen 2560x1440.
MHW ignores Evaluate failure,
so an incomplete frame is possible during
this brief diagnostic; modern rendering remains unimplemented.
The earlier capability evidence is in
[`capability publication`](docs/research/capability-publication.md).

## Build

Configure a 64-bit MSVC build:

```powershell
cmake -S . -B build -A x64
cmake --build build --config RelWithDebInfo
ctest --test-dir build -C RelWithDebInfo --output-on-failure
```

MinHook 1.3.4 is fetched at configure time from its pinned upstream commit; see
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

## Repository status

- Supported research target: DirectX 12
- Implemented: dependency-free binary string scanner, requirements/gap report,
  hash-gated observational NGX/DXGI probe with runtime identity correlation,
  exact-build NGX resolver/export/status tracer, disabled-by-default
  provider-contract bridge policy, and isolated MO2 packaging
- Not implemented: modern DLSS evaluation or frame generation
- Game files are never committed or patched
