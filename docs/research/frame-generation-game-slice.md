# In-game frame generation: minimal ordinary-FG slice (v0.7.x)

Design, 2026-10-06. Goal: turn on Streamline DLSS Frame Generation (2x) in MHW
on top of the working DLSS Performance + Reflex renderer, with HUD artifacts
accepted, to surface integration problems early. MFG reuses the same inputs
later. Sources: the owned FG foundation (`frame-generation-foundation.md`),
the in-game DLSS input path, the v0.6.5 scene result and the RenderDoc capture
(`frame-generation-hudless-capture-result.md`).

## What DLSS-G needs and where it comes from

| Input | Source in our renderer | Format / extent | Status |
|---|---|---|---|
| Depth | DLSS converter output (private image, refreshed per evaluation) | R32F, 1280x720, reversed-Z | exists |
| Motion vectors | DLSS converter output | RG32F, 1280x720, render pixels, camera motion included, jitter excluded | exists |
| Frame token | Reflex `FrameSession` token on the owner thread | - | exists, needs accessor |
| Camera constants | per-frame `HistoryStamp`: un-jittered projection `fNoJitterProj`, view `fView`, previous stamp | row-vector matrices | exists, needs derivation |
| Jitter | the injected Halton offset (`geometry.hpp`) | render pixels | exists |
| Reset | DLSS history decision (`history.hpp`) | bool | exists |
| HUD-less colour | HUD-free `T` after our final draw, through brightness curve + sRGB | BGRA8 2560x1440 | v0.7.1 |
| UI alpha | - | - | later |

Depth and motion vectors are exactly the formats the owned fixture already ran
with. They are at render resolution (tagged with a 1280x720 extent) while the
backbuffer is 2560x1440.

## Constants (pure, unit-tested)

SL matrices are row-major for row vectors, the same convention as MHW's view
matrix. Per frame, from the current and previous stamps:

- `cameraViewToClip = fNoJitterProj` (already includes the reversed-Z remap),
  `clipToCameraView = inverse`.
- `cameraViewToWorld = inverse(fView)`; `cameraRight/Up/Fwd` are its first three
  rows (checked orthonormal), `cameraPos` its last row.
- `clipToPrevClip = clipToView * viewToPrevView * prevViewToClip`, with
  `viewToPrevView` computed camera-centred (subtract the current camera
  position before inverting) to avoid precision loss at large world
  coordinates; `prevClipToClip` is its inverse.
- `cameraNear/Far/FOV/AspectRatio` decoded from `fNoJitterProj` after undoing
  the depth remap; validated for finite, positive, near < far.
- `jitterOffset` = the injected offset in render pixels (same convention as
  NGX), `motionVectorsJittered = false`, `mvecScale = {1/1280, 1/720}`,
  `depthInverted = true`, `cameraMotionIncluded = true`,
  `motionVectors3D = false`, `reset` from the history decision.

Any failed check sends null tags for that frame (fail closed), never guessed
constants.

## Data flow and threads

1. **Render worker, DLSS evaluation (game DIRECT list):** after a successful
   evaluation, copy the converted depth and motion vectors into a free slot of
   a 3-slot ring of FG-owned textures and record the slot's frame identity,
   history stamp, jitter and reset.
2. **Submission:** the slot becomes *ready* only when the existing
   ExecuteCommandLists observer sees that list generation submitted.
3. **Owner thread, `present_begin`** (before the PresentStart marker, while the
   frame token is valid): take the newest ready, untagged slot. Then
   `slSetConstants`, `slSetTagForFrame` (depth, mvec, backbuffer extent; all
   `eValidUntilPresent`) and `slDLSSGSetOptions`, all with the same token. If no
   slot is ready, or a check fails, tag null and record the reason.
4. **Present** goes through the Streamline proxy under the `sdk_paced`
   contract, which owns pacing and generated presentation.
5. **Slot reuse:** after each Present, read `slDLSSGGetState`'s
   `inputsProcessingCompletionFence` value for the tagged slot. The render side
   reuses a slot only when that fence has completed (`GetCompletedValue`,
   non-blocking). If no slot is free, that frame gets no FG inputs; there is no
   CPU wait.

## Streamline and presentation

- `StreamlineFeatures::reflex_and_frame_generation` and `sdk_paced`, chosen at
  device creation for this target only. The runtime directory sidecar points at
  the pinned six-DLL Streamline 2.12.0 set (hash- and signature-checked).
- At device creation: `slIsFeatureSupported(DLSS_G)`, requirements, and after
  swap-chain creation `slDLSSGGetState` (max generated frames, min dimensions,
  status), all logged.
- Options: `eOff` with `eRetainResourcesWhenOff` until armed; `eOn`,
  `numFramesToGenerate = 1` while armed and inputs are valid; `eOff` when
  reconstruction is suspended (loading, cutscene paths) or inputs are missing.
  Option calls happen only on the presentation owner thread.

## Lifecycle decision

The owned fixture showed that, after Streamline shuts down, the FG-created
physical chain must be retired and a fresh native chain created. MHW keeps its
own swap chain and buffers, so we cannot do that underneath it. **The FG build
therefore never shuts Streamline down while the game runs.** "Stop" means FG
Off plus null tags; the hooks and SDK stay live until process exit, and there is
no vendor teardown from `DllMain`. The independent hook-restoration check does
not apply to this build; its acceptance is based on SDK state, our telemetry,
and normal process exit.

## Isolation

A new target, `mhw_fg_game_probe` (`MHW_FG_GAME`), built from the default
DLSS/Reflex sources plus the FG module. The default `mhw_reflex_dlss_probe` and
the scene diagnostic keep their behaviour. The pure logic (constants,
projection decode, slot ring state) lives in a header-only module with native
tests.

## First run (one build, two stages)

- **Stage A, FG loaded and Off** (startup, main menu, save load, area change):
  answers whether `slInit` with DLSS-G at device creation works in MHW, whether
  the game's NGX runtime and Streamline both loading `nvngx_dlssg` coexist,
  whether the four startup `ResizeBuffers` and normal presentation work under
  SDK pacing, and what the feature state reports.
- **Stage B, armed, FG On** in gameplay: generated-frame counts from the SDK,
  user-observed smoothness and artifacts, camera turns, a menu, an area change,
  alt-tab. HUD smearing is expected.
- Stop = FG Off, then normal exit.

## Risks to watch

- `slInit` with DLSS-G after device creation (untested); the fallback is an
  earlier init in the factory hook.
- Two NGX consumers on one device; `nvngx_dlssg` loaded by both.
- Render-resolution depth/MV with a full-resolution backbuffer (expected to be
  supported via extents; the SL log will say otherwise).
- Present cadence: MHW presents with `SyncInterval 0` and `ALLOW_TEARING`.
- Alt-tab/focus and resize with FG On are unproven anywhere.
- Async compute or queue changes in the DLSS list (the existing queue-wait
  logic covers DLSS; the FG copies ride the same list).

## Next after the slice

v0.7.1: HUD-less colour (brightness curve + sRGB applied to the HUD-free `T`),
with the `fGammaCorrectEx` source established. Then UI alpha, menus and MFG
(`numFramesToGenerate` up to the reported maximum).
