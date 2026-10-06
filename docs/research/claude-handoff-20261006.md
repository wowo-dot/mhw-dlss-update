# Handoff to Claude: MHW DLAA, DLSS, Reflex, FG and MFG

Prepared 2026-10-06, Europe/Istanbul. Repository:
`D:\mh world modding\dlssupdate`.

This is a continuation handoff, not a request to restart the project or a claim
that frame generation already works in Monster Hunter: World. The latest
accepted game diagnostic is **v0.6.4**, containing the **v1.25.44 DLSS
Performance renderer and Reflex On**. Ordinary FG and fixed-count MFG work in
our separate owned DX12 renderer. **MHW FG/MFG integration is still incomplete.**

The immediate next task is to observe completed native draws that the current
FG scene observer excludes because they have a depth attachment, then connect
the final scene/composition path to the currently presented buffer. This is
input preparation for FG, not another round of DLSS implementation from zero.

## 1. Read these first

Read `AGENTS.md`, then the following in this order. Paths here are relative to
the repository unless explicitly absolute.

1. `docs/research/current-state.md`: read the **top section**, dated 2026-10-05.
2. `docs/research/frame-generation-scene-raster-result.md`: latest accepted
   live result and its limitations.
3. `docs/research/frame-generation-scene-boundary.md`: recorder ABI, scope,
   return/submission/Present association, and rejection meanings.
4. `docs/research/frame-generation-scene-copy-result.md`: previous accepted
   v0.6.3 copy map; it contains useful observations absent from some v0.6.4
   samples, but must remain separate evidence.
5. `docs/research/frame-generation-game-ownership-result.md` and
   `docs/research/frame-generation-game-ownership.md`: MHW presentation owners.
6. `docs/research/frame-generation-foundation.md`: accepted owned FG/MFG
   execution, resource lifetime, SDK worker routing and shutdown contracts.
7. `docs/research/reflex-temporal-result.md`: working DLSS/Reflex coexistence.
8. `docs/research/beta-backlog.md`: deliberately deferred investigations.

**Documentation freshness matters.** `README.md` and many older research pages
still describe milestones as unfinished, or say a historical DLL remains
installed. Those were accurate at their recorded dates. They are not the
current deployment. The top of `current-state.md`, latest result, and current
deployment/archive receipts take precedence for present state.

The historical suffix of `current-state.md` contains invalid UTF-8 bytes. Do
not decode and rewrite the entire file with replacement characters. Existing
update helpers replace its UTF-8 prefix while preserving the suffix byte for
byte, starting at the first `---` separator. Its preserved suffix SHA-256 is
`1ba85c69c682683524318778bf1c752400241a941d89914ee3603db6cff57a25`.

## 2. User decisions and collaboration expectations

The agreed development order is:

1. Preserve working DLAA, DLSS Performance and Reflex.
2. Integrate **ordinary FG in MHW**.
3. Integrate **MFG using the same qualified inputs and queried capability**.
4. Create a configuration-file-based alpha with proper performance for wider
   testing, while keeping the full research build available separately.
5. Broader beta fixes, other DLSS quality modes, polish and packaging.
6. In-game settings integration later, after the user's **CMFM — Configuration
   Menu For Mods** framework exists. Do not start building that menu now.

Further settled preferences:

- **Keep the diagnostic DLL installed between runs.** Do not routinely uninstall
  it or restore an earlier version after normal exit. Preserve exact backups
  whenever a new build is installed.
- The user wants progress on FG/MFG before performance cleanup. Severe slowdown
  is recorded, but its cause has not been profiled. Do not assert that logging
  alone causes it, or promise a particular final FPS gain/cost.
- Preserve useful diagnostics during implementation. Later, measure and remove
  unnecessary work for the alpha; keep a separate research configuration.
- Native AA/DLSS should be Off for current tests. Our temporal path must not
  require the user to select TAA in the game's options.
- Reuse the game's correct temporal preparation and inputs while replacing
  the native resolve. Do not reintroduce independent temporal preparation
  merely to avoid the TAA menu setting.
- Avoid spending GPU work on native TAA and then adding DLAA on top. An already
  claimed replacement must not silently retry native TAA after an SDK failure.
  Preserve the established valid-output, binding restoration and suspension/
  failure policy instead.
- Fix shared renderer/post-processing contracts, rather than creating a patch
  for each named monster or location. Sauna, smithy, Gallery and monster attacks
  are repros of renderer paths, not configuration exceptions.
- Occasional transition skips and deferred beta issues are not automatically
  blockers for FG. Unsafe lifetime/state handling, persistent deactivation,
  visible corruption or a new integration blocker deserves immediate attention.
- The user tolerates useful experimental crashes, but every test should answer
  a concrete question. Prepare code and meaningful offline checks before asking
  for another launch. Avoid repeatedly asking approval already provided for
  ordinary research/deployment steps.
- Windowed testing and replying from the desktop are fine. Fullscreen and
  phone-only replies are no longer requirements.
- Give explicit notice before an intentional stop; otherwise the user may
  reasonably report the disappearance of DLSS as a failure.
- Report actual counters, faults, recovery and cleanup. A clean-looking scene
  alone does not establish SDK activity or correct resource ownership.
- Do not send this handoff or any project data to an external service/person
  without the user's authorization. This file is prepared for the user to give
  to Claude. No external message was sent.

## 3. Local environment and deployment

| Item | Local path / value |
|---|---|
| Repository | `D:\mh world modding\dlssupdate` |
| Game directory | `D:\SteamLibrary\steamapps\common\Monster Hunter World` |
| Game executable | `D:\SteamLibrary\steamapps\common\Monster Hunter World\MonsterHunterWorld.exe` |
| Private decoded audit image | `D:\mh world modding\ai\glavenus\MonsterHunterWorld_decrypted.exe` |
| IDA | `C:\Program Files\IDA Professional 9.3\ida.exe` |
| MO2 executable | `C:\Modding\MO2\ModOrganizer.exe` — existence checked at handoff |
| Exact test DLL slot | `D:\MO2\MHW\mods\dlssTEST\mhw_dlss_bridge_probe.dll` |
| Python | `%LOCALAPPDATA%\Programs\Python\Python310\python.exe` |
| CMake / CTest directory | `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin` |
| Native toolchain | Visual Studio 18 2026, x64, C++20, Release, `/W4 /WX` |
| Build tree | `build` |
| Current build output | `build/bin/Release/mhw_frame_generation_scene_probe.dll` |
| NGX SDK root | `local/nvidia-dlss` |
| Current Streamline SDK root | `local/reflex-foundation-20261002/sdk` |
| Owned FG SDK/evidence | `local/frame-generation-foundation-20261002` |

These are machine-local paths, not values to compile into a distributable mod.
Read `AGENTS.md` for safety and implementation rules. Never modify the retail
executable/assets, disable platform protections, or run experimental hooks in
public multiplayer. Keep binaries, dumps, shader captures, IDA databases and
proprietary SDK material private and untracked under `local`/build outputs.

The existing launch setup is MO2's `dlssTEST` mod slot, historically exercised
with the `barebonesdlss` profile. Stracker's Loader has also been used for
controlled monster tests. Verify the selected profile/mod list for each run;
do not silently change it. See deployment receipts and previous launch helpers
for exact local setup. No special new MO2 launch mode is needed merely because
the assistant changed.

The MHW data/mod directory is `D:\MO2\MHW`; it is not the verified executable
directory. Prior launch notes say to open the portable MHW MO2 without
`-i wowodot`: that argument requested a nonexistent global instance. Do not
launch the separate `D:\MO2\wilds\ModOrganizer.exe`, which is configured for
Monster Hunter: Wilds. This handoff does not launch either application.

The test DLL has these sidecars:

- `mhw_dlss_bridge_probe.mode` — **not** `.dll.mode`; current text is
  `native-depth-probe`. This is the preserved loader mode name, not a claim
  that the installed combined DLL performs only a depth probe.
- `mhw_dlss_bridge_probe.dll.runtime-dir` — current Streamline runtime directory:
  `D:\mh world modding\dlssupdate\local\reflex-foundation-20261002\sdk\bin\x64`.
- `mhw_dlss_bridge_probe.dll.output-dir` — currently points to
  `D:\mh world modding\dlssupdate\local\frame-generation-scene-20261003\game-04`.
- The selected output folder contains `sdk-runtime-dir.txt` for NGX, as well as
  `temporal-native-gates.json` and the run-specific evidence.

**Before the next launch, select a fresh exclusive output folder.** Preserve
the old sidecar bytes; do not append new evidence to an accepted archive.
`game-04` is finished, and `game-04-archive` is immutable.

The current test settings are DX12 On, windowed 2560x1440, native AA Off,
native DLSS Off; DLSS Performance renders 1280x720 and outputs 2560x1440.
The user set NVIDIA's per-game Low Latency Mode **Off**, after Ultra obscured
verification of Reflex Off. Do not change global driver settings silently.
Adapter LUIDs are session-local and must be read from new logs, not hard-coded.

At handoff inspection no MHW or ModOrganizer process was returned by the
process query. The previous run also has an explicit normal-exit confirmation.
Recheck process state before any deployment; this is a snapshot, not a promise
that the user will leave applications closed.

Git branch was `main`, HEAD `76796fbc7316552b017306572a6e9a086779e91f`.
There are pre-existing tracked modifications and substantial untracked research
source/artifacts. That commit alone does **not** contain the current work.
Do not reset/clean/checkout over the workspace or commit proprietary local data.
Compare against the exact tested source freeze for a focused change review.

## 4. What already works, and what is not yet established

| Component | Current evidence / scope |
|---|---|
| Modern DLAA | Continuous visible native-resolution replacement has been exercised. Native temporal preparation reuse and AA-Off activation were implemented after earlier independent-path failures. Preserve it. |
| Modern DLSS | Performance mode, 1280x720 to 2560x1440, with the existing qualified resolve and downstream output path. Focused camera, menu, load, focus, cutscene and effect tests have passed; universal coverage is not claimed. |
| Native AA bypass | Qualified native TAA resolve is replaced, while native preparation/input generation remains useful. Saved TAA selection is not required. Unknown/unclaimed paths and broad exclusivity remain evidence limitations. |
| Post-processing | Shared handling has been added for color dependencies, haze, overlays, temporary auxiliary textures, Gallery's six-pass blur, radial attack effects, and late hunter silhouette coordinates. Not every future variant is proven. |
| Reflex | On/Off verified against driver state; frame markers/token/Sleep boundaries mapped and integrated; combined DLSS/Reflex game test passed. Measured latency benefit is not established. |
| FG/MFG SDK foundation | Ordinary 2x and fixed 3x/4x/6x executed in our owned DX12 renderer, with focused-window and lifetime/cleanup checks. This is not game integration. |
| MHW presentation ownership | Observed game creation/buffer/Present calls share the Reflex owner in tested paths; native chain retained through tested area loads. Full recreation/worker behavior under FG remains unproven. |
| MHW final scene / UI separation | Partial submitted CPU map only. Current backbuffer identity, final HUDless pixels, transparent UI and immutable FG input ownership remain unresolved. |
| MHW generated frames | **Not enabled.** No Streamline FG plugin, FG tagging or generated MHW presentation has been introduced by v0.6.4. |
| Production performance/config | Deferred; no alpha release or optimization claim. |

Do not confuse version families:

- `v1.25.44`: underlying scaled temporal renderer.
- `v1.21.5`: the native-resolution temporal branch used by the combined DLAA
  target; preserve its scope separately from the latest combined DLSS test.
- `v0.5.1`: accepted combined DLSS/Reflex wrapper.
- `v0.6.0`: game presentation ownership diagnostic on that renderer.
- `v0.6.1–0.6.4`: progressively improved FG scene metadata diagnostics.
- `v0.3.2`: accepted owned FG/MFG fixture version.

Some older long runs were useful but not fully accepted by every diagnostic
review. In particular, the v1.25.42 43-minute broad DLSS run completed 44,351
evaluations with zero runtime faults and recovered suspensions, but retained
skip/diagnostic discrepancies. `dlss-broad-run-result.md` explains those limits.
Do not paraphrase it as an uninterrupted, fully verified production run.

## 5. Architecture to preserve

The useful conceptual flow is:

```text
Native game frame/input update and temporal preparation
  -> matched color/depth/motion + scoped projection jitter
  -> qualified replacement of native temporal resolve by NGX DLAA/DLSS
  -> adapted native post-processing / downstream output handoff
  -> qualified final screen draw
  -> composition/copy/presentation attachment path (still partially mapped)
  -> native swap chain Present, associated with the existing Reflex token
```

The NGX resolve output is **not automatically** the final post-processed,
HUDless FG color input. The unknown composition segment is the current work.

Inputs, shader recipes, command-list Reset generations, state proofs and
frame/epoch/view association are checked before replacement. The output handoff
preserves a per-frame rendering choice and recording/resource ownership until
successful Reset and GPU fence completion. Do not weaken these guards just
because pointer values look stable.

The previous approach that fabricated an independent temporal path with native
AA Off exposed sauna/transition failures. The project returned to reusing the
game's existing temporal preparation and replacing/bypassing the actual TAA
resolve. Native preparation does not mean native TAA must execute on the GPU.
Read the AA reuse and resolve documents if changing this area.

For a claimed resolve, the shared transaction restores bindings and supplies
valid complete unfiltered color when the SDK cannot supply the intended result.
Unknown restoration state escalates rather than retrying the native resolve.
Unavailable inputs can suspend and later resume; a real failure is recorded
and may latch. Do not hide faults or silently use stale depth/history.

Reflex owns one token per qualified engine frame. `FrameSession::input_token`
borrows that exact token on its owner thread before Present. It must not be
cached across end/stop, borrowed during Present, or replaced by a second FG
token/independent counter. Plan counters, native recording counters and Reflex
integration IDs are distinct domains and need explicit association.

## 6. Latest accepted v0.6.4 run

Run: `local/frame-generation-scene-20261003/game-04`.
Accepted archive: `local/frame-generation-scene-20261003/game-04-archive`.
Historical PID 37012, process creation FILETIME 134357035808482668. These are
evidence identities, not values to reuse for a new process.

Four requested captures cover main menu, loaded save, open gameplay menu, and
loaded gameplay after an explicitly confirmed area change.

| Metric | Final result |
|---|---:|
| Successful DLSS evaluations | 9,534 |
| Complete Reflex frames | 11,009 |
| Ordered Reflex markers | 66,054 |
| Complete driver reports | 64 |
| Runtime faults / request collisions | 0 / 0 |
| Skipped resolves | 3 |
| Completed GPU signals | 9,537 |
| Submitted, retired and released recordings | 9,536 each |
| Live recordings / resource references after stop | 0 / 0 |
| Restored jitter changes | 10,551 |
| Matched routed/native presentations | 40,478 |
| Independently restored hook heads / full native functions | 37 / 46 |
| Independent process reads | 778 reads / 75,163 bytes |
| External process-memory writes | 0 |

Full render resolution, Reflex Off, SDK shutdown and pin release passed. The
user confirmed normal appearance and normal exit. The installed DLL was kept.
The independently compared 37 heads exclude some temporal COM binding-observer
heads; their cleanup is additionally checked by telemetry. Do not claim every
hook was independently byte-compared merely because the aggregate check passed.

### The actual new finding

The original draw observer rejected reason 9 when it could not obtain a ready
raster snapshot **and known single-RTV output**. v0.6.4 adds bounded metadata to
separate missing raster state from its output restriction.

All eight sampled reason-9 entries have:

- complete observed raster fields, with matched root/raster generations;
- one successfully observed RTV descriptor, readable;
- a present and readable depth descriptor;
- raw contiguous-range BOOL zero;
- raster-known mask 31, with output-known false;
- native-cache read mask 255 (first eight fields read, later three unread).

The preserved observer sets output-known only for **one RTV and no depth
descriptor pointer**. That exclusion explains these eight samples. Its internal
`c` parameter is the **depth pointer**, not the contiguous-range BOOL; an earlier
range-flag interpretation was corrected in the documentation.

There are 242 reason-9 entries total, but only two samples per reason per
capture were retained. Do not claim all 242 have the same cause. These are
executor-entry snapshots: neither an optional later submission receipt nor
readable descriptors proves successful draw return, GPU completion or UI
semantics.

| Capture | Plan frame | Final draw / Present native frame | Qualified events | Reason-9 entries | Copies |
|---|---:|---:|---:|---:|---:|
| Main menu | 29423 | 29424 | 1 | 23 | 0 |
| Loaded save | 33056 | 33057 | 3 | 53 | 1 |
| Menu open | 37905 | 37906 | 1 | 59 | 0 |
| After area load | 39514 | 39515 | 1 | 107 | 0 |

All four contain the exact returned/replaced final draw, associated by recording
and same-queue submission with qualified Reflex PresentStart. Only gameplay
captured an outgoing returned, unreplaced CopyTextureRegion: final target
`2509987008` to intermediate label `2492296416`, subresource zero, destination
XYZ zero, source box `[0,0,0,2560,1440,1]`, after a 4 -> 2176 transition.
The other samples contain no copy; that is not proof no copy happens elsewhere.
Neither label matches historical sampled GetBuffer interfaces. The engine
format field is UINT32_MAX, **not a verified DXGI format**.

The previous v0.6.3 captures each contained a final-target outgoing copy to one
intermediate resource. Its menu sample also included four incoming 1280x720
copies which GPU submission ordering places before the final draw, including
one CPU callback observed later. Preserve that distinction: CPU callback time
alone cannot order work recorded on different command lists.

## 7. Code map and current diagnostic contract

| File / group | Purpose |
|---|---|
| `src/continuous_dlaa/` | Native temporal preparation/replacement, jitter/input eligibility and recording lifetime. |
| `src/dlss_sdk/`, `src/ngx_startup/` | NGX initialization on the native device, conversion/evaluation and geometry/history. |
| `src/scaled_temporal/` | Resolution control, adapted native post-processing, stream dependencies, output handoff and silhouette correction. |
| `src/scaled_temporal/scene_probe.inl` | FG scene CPU observer at existing native executor/draw/copy/barrier callbacks. |
| `src/frame_generation/scene_trace.hpp` | Fixed-capacity captures, requests, return/submission/Reset/Present linkage and serialization. |
| `src/temporal_present/observer.cpp/.hpp` | Existing COM binding/raster observer and opt-in OM metadata. |
| `src/reflex/frame_session.*`, `frame_policy.hpp`, `frame_stamp.hpp` | Token ownership, ordered boundaries and frame association. |
| `src/reflex/presentation.*`, `presentation_route.hpp`, `owned_interface.hpp` | Native/SDK presentation routing and ownership. |
| `src/reflex/streamline.*`, `frame_generation_policy.hpp` | Pinned SDK interfaces and capability checks. |
| `src/reflex/temporal_coexistence.*` | Shared native device, temporal drain before Reflex/presentation shutdown. |
| `tools/frame_generation_scene.py` | Current strict scene identity, capture controls and joined renderer review. |
| `tools/frame_generation_ownership.py`, `reflex_temporal.py`, `reflex_frames.py` | Reused ownership/combined/Reflex review and controls. |
| `tests/frame_generation_scene_trace_tests.cpp`, `tests/test_frame_generation_scene.py` | Scene bounds/order/return/schema/read-mask and negative-evidence tests. |
| `tools/frame_generation_*_fixture*`, `review_frame_generation_fixture.py` | Owned FG/MFG renderer and strict fixture review. |

The scene-only target is `mhw_frame_generation_scene_probe`.
The default combined DLSS/Reflex target is `mhw_reflex_dlss_probe`.
v0.6.4 compiles `observer.cpp` again into **`mhw_fg_temporal_state`**, with private
`MHW_FG_SCENE_PROBE`, linked only by the scene target. Other targets keep the
common `mhw_temporal_state`. Do not accidentally link both observer libraries
or expose new diagnostic logic to every renderer target.

Current wrapper log: schema 6, version 0.6.4.
Scene trace: schema 4, policy `submitted-scene-raster-scope-map-v4`.
Bounds: four captures; 512 events/capture; 16 followed resource labels; 16
rejection reasons; two entry samples/reason. No COM resource references,
GetDesc/QI calls, pixels or additional GPU work are introduced by this recorder.

`raster_inspect()` exposes 19 CPU metadata fields, in order:

```text
0 entry found          1 raster hooks installed  2 accepting
3 healthy              4 bindings known          5 bindings valid
6 root generation      7 raster generation       8 raster-known mask
9 vertex slot mask    10 OM seen                 11 RTV count
12 raw range BOOL     13 RTV argument present    14 RTV read
15 RTV handle         16 DSV argument present    17 DSV read
18 DSV handle
```

Known-mask bits: vertex 1, viewport 2, scissor 4, topology 8, index 16, output 32.
Index observation is present in these samples; do not change the existing
`RasterBindings::ready()` requirements by inference.

Native cache fields/read-mask bits 0–10 are:
`state/list/packet/attachment/active-attachment/flags/view/backend/native-RTV/root/PSO`.
A zero with its read bit clear is **unread**, not a valid native zero binding.

The qualified scene observer currently stops at reason 9 before reading:

```text
state + 0x98  native RTV handle
state + 0x110 native graphics root
state + 0x108 native PSO
```

Other existing scene reads include executor+8 -> state, state+0x20 -> list,
state+0x158 -> packet, packet+0x10 / context+0x10 -> active attachment,
attachment+0x48 -> flags, attachment+0 -> view, view+0x60 -> backend,
backend+0x18 -> resource label, backend+0x28 -> rotation discriminator.
These are **tested structural offsets for the qualified executable**, not
portable addresses or permission to traverse an unbounded rotating table.

Capture starts at the exact final draw's API entry. Concurrent recording can
therefore hide an earlier CPU callback whose draw executes later on the GPU.
There is no complete draw inventory or current-backbuffer proof. Submission
batch, list index, Reset generation and within-list order are required; no
cross-queue order claim without a producer fence.

## 8. Next implementation task, concretely

Make the next bounded diagnostic answer: **what successfully completed draws
are using the tracked output with a depth attachment, and where do those
outputs go before presentation?**

Recommended sequence:

1. Keep `RasterBindings::output_known`, reconstruction admission, shader/state
   guards and default observer target unchanged.
2. Add a **separate diagnostic qualification path** for the observed single-RTV,
   depth-bearing case. Consume the existing successful OM metadata. Require a
   valid matching recording generation and known binding/raster fields.
3. Under the existing qualified executor/packet/attachment reads, continue
   bounded native RTV/root/PSO agreement checks. Unknown reads/mismatches remain
   explicit rejection evidence; metadata must not authorize render changes.
4. Record the API/native draw identity before the original executor, then mark
   successful return through the existing forwarding/exception path. Do not
   upgrade the old incomplete entry samples into completed draws.
5. Associate the completed receipt with the exact ExecuteCommandLists batch,
   list generation/index and qualified Present. Preserve overflow, duplicate
   submission, premature Reset and failed-return checks.
6. Distinguish scene/effect/UI **candidates** using shader, blend/depth, target
   and order. Do not name them HUD until semantics are established. The evidence
   may require a subsequent bounded pixel/descriptor/resource-description
   diagnostic; record that as a separate experiment with ownership.
7. Follow the native presentation attachment/copy destination to the current
   native buffer, including index and resize generation. Historic GetBuffer
   pointer equality alone is insufficient.
8. Bump the diagnostic version/schema if the log contract changes. Preserve
   old frozen reviewers and archives. Add negative tests for the new scope,
   then build both scene and default targets to check isolation.
9. Repeat main menu -> gameplay/HUD -> menu open -> area-loaded gameplay.
   Stop/review/independent cleanup/normal exit as described below.

Success for this step is reliable returned/submitted metadata and a narrower
statement of scene/UI/presentation boundaries, not merely more events or a
successful DLSS counter. Do not enable the FG plugin simply to see what happens
before the required inputs and lifecycle are assembled.

## 9. FG/MFG foundation: contracts already learned

The owned renderer's accepted archive is
`local/frame-generation-foundation-20261002/accepted-archive`.
Pinned Streamline 2.12.0 public commit:
`e8aaa6eaac968711fb62473d4ae8256dde20919b`.
Do not silently upgrade it during integration.

Accepted focused-window modes ran 144 host frames each: 32 Off warmup,
96 enabled, 16 Off. Ordinary 2x reported 96 extra presentations; fixed 3x,
4x and 6x reported 192, 288 and 480 extras respectively. These are SDK-reported
execution counts, not independent display scanout or a measured FPS benefit.
Dynamic/automatic MFG was not tested. Query each adapter's maximum before
setting options; a historical 6x capability is not universal support.

The owned input contract uses depth R32_FLOAT, motion R32G32_FLOAT, HUDless
BGRA8 color, R8 UI alpha, and a backbuffer extent tag. Four textures are supplied
in NON_PIXEL_SHADER_RESOURCE state **64**, with `eValidUntilPresent`, the same
current Reflex token and viewport zero. The fixture is 1280x720, with a simple
moving square/checker scene and empty UI. Its camera/constants are synthetic;
they do not establish correct MHW FG constants or HUD separation.

Important integration lessons:

- Initialize the SDK before device/swap-chain creation where required. Existing
  early startup/device-factory ordering was carefully qualified; avoid attaching
  a second independently owned device or borrowing a game AppID.
- The explicit FG dependency set includes six pinned runtime DLLs and three
  features. Default Reflex uses its original four files/two features. Keep this
  opt-in separation. No OTA/downloaded plugins are enabled.
- FG execution is owned by SDK presentation. The wrapper does not manually
  invoke an invented FG evaluation API.
- SDK-paced Present is not the synchronous Reflex-only contract. Its HRESULT
  and zero/multiple synchronous native calls do not justify adding a fallback
  native Present. That can corrupt pacing or present twice.
- The FG pacer can call the same physical chain from another thread. Host calls
  route to logical SDK buffers; worker/other-chain calls must reach the original
  native implementation **before** taking the host-route mutex. Routing worker
  GetBuffer/index calls to logical buffers caused device removal in a control.
- Existing MHW ownership observations support a common host owner in tested
  native paths. They do not prove every future FG worker/recreation path.
- Retain immutable tagged inputs through the SDK consumption fence; fence the
  producing/presenting queue as needed. Zero-valued already-complete fence
  observations do not require SetEventOnCompletion.
- SDK 2.12 ResizeBuffers1 returned E_ABORT in an owned test. A narrow translation
  to ResizeBuffers was accepted only for explicit bounded count, one node,
  mask one and each buffer on the original creation queue. Do not broaden it to
  discard arbitrary node/queue assignments.
- **After SDK shutdown, retire the FG-created physical chain and create a new
  native chain on the native host queue before native presentation continues.**
  Reusing the FG chain after shutdown broke completion/ownership in controls.
- Keep interposer code alive while surviving COM proxy vtables depend on it.
  No vendor teardown from DllMain; drain callbacks/work before releasing pins.
- On/MFG resize/device recreation was not accepted by the owned tests. Treat
  game recreation handling as required implementation, not inherited proof.

Remaining MHW FG work after scene mapping: non-jittered camera constants and
matching frame token, depth/motion/color/UI semantics and formats, immutable
input acquisition, producer and consumption fences, compatible native/SDK
presentation routing, load/focus suspension and FG-chain recreation/retirement.
Then ordinary-FG runtime evidence precedes MFG using that same contract.

## 10. Build and test examples

Use the existing build cache rather than switching the whole tree to the FG
runtime while still testing the Reflex-only game diagnostic. It currently has
`MHW_NGX_SDK_ROOT=local/nvidia-dlss` and
`MHW_STREAMLINE_SDK_ROOT=local/reflex-foundation-20261002/sdk`.

PowerShell example, from the repository:

```powershell
$repoRoot = 'D:\mh world modding\dlssupdate'
$taskPython = "$env:LOCALAPPDATA\Programs\Python\Python310\python.exe"
$taskCmakeDir = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
& "$taskCmakeDir\cmake.exe" --build "$repoRoot\build" --config Release --target mhw_frame_generation_scene_probe mhw_reflex_dlss_probe frame_generation_scene_trace_tests
& "$taskCmakeDir\ctest.exe" --test-dir "$repoRoot\build" -C Release --output-on-failure -R '^(reflex_presentation_ownership|reflex_present_result|reflex_owned_interface|output_diagnostics|frame_handoff|frame_generation_scene_trace|scaled_recording_states|reflex_frame_policy|reflex_frame_stamp|reflex_boundary_abi)$'
& $taskPython -B -m unittest discover -s tests -p 'test_frame_generation*.py'
& $taskPython -B -m unittest discover -s tests -p 'test_reflex*.py'
```

The accepted v0.6.4 checks were 10 native and 76 Python tests (28 FG + 48 Reflex),
with clean warnings-as-errors builds. Rebuild affected native test targets if
changing their shared code; running an old executable is not validation of a
new change. Extend meaningful tests for modified pure logic; don't broadly
repeat unrelated checks without a reason.

Saved logs under `local/frame-generation-scene-20261003`:
`build-04-final.log`, `native-tests-04.log`, `python-FG-tests-04.log`,
`python-Reflex-tests-04.log`, `changes-04-review.json`.
An earlier warning failure is preserved in `build-04.log`; the final log records
the correction. Do not remove rejected evidence.

## 11. Runtime procedure and evidence handling

**Do not run the old one-shot deployment/closure scripts blindly.** They contain
exclusive output names, exact versions, old DLL hashes and final-counter checks.
Use them as reviewed templates for a new run/version, with new receipts and
correct expectations. Never edit historical results to make a new run pass.

Relevant latest helpers, all under `local/frame-generation-scene-20261003`:

| Helper | Intended use |
|---|---|
| `prepare_install04.py` | Old v0.6.4 deployment template: game-closed gate, exact destination backup, source freeze, prior archive verification and output sidecar. Already executed. |
| `finish_preparation04.py` | Source/build/installation/config verification and current-state prefix. Already executed. |
| `checkpoint04.py` | Save a bounded complete live Reflex prefix and paired temporal status; requires active healthy integration. |
| `inspect_scene04.py` | Preliminary metadata review of a saved checkpoint; no process reads. Not final acceptance. |
| `check_live04.py` | Independent post-stop bounded VM_READ comparison, identity/module/native-global checks. **Never run while hooks are active.** |
| `extract_facts04.py` | Facts from the accepted final review; already executed for game-04. |
| `close_archive04.py` | Exclusive archive after normal exit, exact source/deployment/prior evidence checks. Already executed. |
| `update_state04.py` | Publish accepted result while preserving historical bytes. Already executed. |

For a new run:

1. Ensure the game is closed. Record retail/decoded executable hashes, build
   identity and warnings/tests. Preserve the exact destination DLL and sidecars,
   settings/profile provenance, and removal instructions. Freeze tested source.
2. Point `.output-dir` to a new folder with correct NGX configuration/gates.
   Preserve the runtime/mode configuration unless the specific new experiment
   requires a documented change.
3. Launch normally through the agreed MO2 setup. Read `startup`, `inputs` and
   `reflex-frames` logs and confirm version/hash, native device, DX12, swap format
   and driver Off before arming. Some startup files initially appear empty and
   become readable shortly afterward; that alone is not a confirmed failure.
4. Use the current tool's generation-checked arm command. Read the actual arm
   acknowledgment and active healthy counters before asking the user to act.
5. Automatic capture 1 is the main-menu control. After the save is loaded and
   the user confirms it, request 2. Wait for `presented=true`, then save/check
   the gameplay checkpoint. Request 3 after explicit menu-open confirmation,
   and request 4 after explicit area-loaded confirmation.
6. Keep requests sequential and use exclusive receipt names. A sent request
   is not a completed capture. Save the user's actual observation and whether
   the requested action was explicitly confirmed.
7. Announce intentional stop, signal it, wait for both renderer channels to
   finish/drain, and run strict joined review. No cleanup claim from status
   flags alone.
8. While the game is still open **after stop**, run the adapted independent
   native-code/identity checker. Then ask the user for normal exit and scene
   appearance. Archive only once the exit is confirmed and process query agrees.
9. Rehash exact source/old archives/configuration, preserve all successes and
   rejections, update the current-state prefix. Leave the installed DLL in place.

Existing CLI shape, with paths/identities replaced for the **new** run:

```powershell
$taskLog = 'ABSOLUTE_NEW_RUN_PATH\reflex-frames-PID-CREATION.jsonl'
& $taskPython -B tools/frame_generation_scene.py arm --log $taskLog --output 'ABSOLUTE_NEW_RUN_PATH\arm-request.json'
& $taskPython -B tools/frame_generation_scene.py capture --request 2 --log $taskLog --output 'ABSOLUTE_NEW_RUN_PATH\gameplay-capture-request.json'
& $taskPython -B tools/frame_generation_scene.py capture --request 3 --log $taskLog --output 'ABSOLUTE_NEW_RUN_PATH\menu-open-capture-request.json'
& $taskPython -B tools/frame_generation_scene.py capture --request 4 --log $taskLog --output 'ABSOLUTE_NEW_RUN_PATH\area-load-capture-request.json'
& $taskPython -B tools/frame_generation_scene.py stop --log $taskLog --output 'ABSOLUTE_NEW_RUN_PATH\stop-request.json'
& $taskPython -B tools/frame_generation_scene.py review --log $taskLog --dll-sha256 'EXPECTED_NEW_DLL_SHA256' --output 'ABSOLUTE_NEW_RUN_PATH\ownership-scene-review.json'
```

These controls query process identity and use IPC/files, not arbitrary external
memory writes. The latest probe has a **600-second armed ceiling**; don't spend
that window redesigning code or writing long documentation. Prepare collectors
before arming. Status JSON may be briefly incomplete while being rewritten;
bounded retry is appropriate, not inventing a runtime fault.

The reviewer is strict for v0.6.4 and explicitly normalizes the wrapper only
after checking its schema/scope, reusing unchanged ownership/combined checks.
Older runs require their frozen reviewers. Don't force old/new identities to
match, ignore a failed check, or label a partial scope as full FG readiness.

The currently saved `.output-dir` is **not ready for a new test by itself**;
it names the finished run. Prepare a fresh run before asking the user to launch.

## 12. Exact identities and preservation anchors

| Identity | SHA-256 |
|---|---|
| Retail MHW, version 0.0.0.0 | `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea` |
| Private decoded audit image | `d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c` |
| Installed v0.6.4 DLL | `d173d34338b5e7aaddf451eefac4a721f14a4a4f10ee49ad6815dd5727bf2fee` |
| Previous v0.6.3 exact slot backup | `7b11a27d7dc7e7a21cdec9f977145a076a3b94fdf831dbe324b75b38779505ea` |
| Current graphics file | `da3f357f639e0fcd4ab90e6dacfbd088efd26c20ed998f6a173281990ec349c3` |
| v0.6.4 51-file archive manifest | `09640cee104e23c10684d96edf3d663b010f76169b40162253b38f4b216bb604` |
| v0.6.4 688-file source manifest | `a5943305109f13509741f700f4e2f8a2d3dddc5cc43c13a032cfb0028cc5025d` |
| v0.6.3 48-file archive manifest | `1ca0fba93d4aba694ee120b7d92bf0b3bba3dc9babc4a59dfd86fc7418a2dd07` |
| v0.6.3 687-file source manifest | `fcc0700a36c5990203005e246f83a2ca2f1ada51299ef8fc3aca45b7e6ce21c2` |
| v0.6.2 43-file archive manifest | `bfeb3e9a445f31b44d7297c6c4ddf0e6ec278e2cca7b7b49fd0bbf11c87c9226` |
| v0.6.1 rejected 32-file archive manifest | `ed55fbae8bebe52f7bea070bf7a6c04aded4a6a41e9e8f9704c7bf267f7b0100` |
| Owned FG/MFG accepted 94-file archive manifest | `93ab860784349f70d4602e054741d74a57191bf7aa4a759af015094766abd08b` |
| v0.6.0 game ownership 38-file archive manifest | `f37e5d87a0a7e1c0fe915b9dc78708b0eac8b536422c1de96cf320ca9fec2c85` |
| v0.5.1 combined renderer 38-file archive manifest | `1298d909af8a2d0ad49fefe5a54f6ebea579c268bc63a26ad0b8ab8eacc2b738` |

Latest receipts: `archive04-receipt.json`, `documentation04-receipt.json`, and
`game-04/{deployment,run-result,ownership-scene-review,live-restoration,scene-raster-facts}.json`.
Each source freeze has `manifest.json`. Most game archives have
`archive-manifest.json`; the owned FG/MFG accepted archive uses **`manifest.json`**.
Verify every listed file's size/digest, not only the manifest digest.

Exact previous-slot backup:
`local/frame-generation-scene-20261003/game-04/previous-slot.dll.disabled`.
Old output-sidecar bytes: `game-04/output-sidecar.before.bin`.
Manual rollback instructions: `game-04/remove-test.txt`. Rollback is available,
but is not the user's current request; keep v0.6.4 installed.

The existing NGX runtime can map `nvngx_dlssg.dll`, digest
`135eaf0733c1e37381a8c28abcf7a862404a54132b81787c04e35d09efc5e36f`.
That mapping alone does **not** mean FG is active. In the latest game run,
`sl.dlss_g.dll` was absent and no MHW FG calls/tags were made.

## 13. Deferred work and common mistakes

Keep these on the backlog unless they become direct blockers:

- Severe silhouette-correction slowdown; logging versus validation/PSO/extra
  draw cost is not measured.
- Rare missing-depth suspensions and complete AA exclusivity across every path.
- Camp-entry skips reported after the wingdrake fix.
- Broad post-processing/DOF/dependency variants and old diagnostic consistency
  discrepancies. Do not mark them universally solved from targeted passes.
- Resize/device recreation, additional output sizes/aspects and modes.
- Other DLSS quality settings; scaling affects input geometry, jitter/MV scale
  and downstream contracts, so don't call them proven merely because Performance
  works. User expects reuse of the established path rather than more reversing
  unless new evidence requires it.
- Reflex Boost/latency measurements, dynamic MFG and production performance.
- CMFM/native menu state, packaging/distribution and optional future effect
  customization. FSR is not the current task.

Avoid these recurring interpretation errors:

- Zero runtime faults does not mean zero skipped/unfiltered frames.
- A normal visual after stop does not prove the feature is still running.
- CPU draw/copy recording or PresentStart is not GPU completion.
- Historical GetBuffer pointer equality is not current-buffer ownership.
- Engine metadata format UINT32_MAX is not a valid resource-format confirmation.
- An incomplete rejection snapshot is not a completed draw or transparent UI.
- SDK presentation counts are not physical display measurements; the synchronous
  native counter omits asynchronous generated presentations.
- A mapped NVIDIA FG DLL is not generated-frame activation.
- A pinned SDK/proxy must not be torn down while dependent resources/vtables or
  pacing work survive. Reflex-only shutdown is not sufficient for an FG chain.
- Visual screenshots/replay filenames do not establish exact log/video time
  alignment. Keep capture provenance and user actions explicit.

## 14. Suggested first response / continuation

Tell the user you have read the current state and understand the priority:
ordinary FG then MFG, with the DLL retained and performance/menu work deferred.
Verify current deployment/source/evidence before editing. Implement the isolated
completed-draw diagnostic described in section 8, with meaningful tests and a
fresh prepared capture folder. Only then ask for a launch and the short sequence
of runtime actions. If the user's latest instruction changes this plan, follow
that instruction rather than treating this handoff as immutable policy.

No renderer change, deployment, launch, arming or external message was performed
to create this handoff. It supplements the existing research record and does
not replace or rewrite the accepted archives.
