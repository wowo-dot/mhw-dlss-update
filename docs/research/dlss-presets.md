# DLSS presets (v0.17.0, 2026-10-12)

The roadmap's "DLSS ready to ship" step begins with the quality presets:
DLAA, Quality, Balanced and Performance. Ultra Performance is left out by the
user's call. The game's render-scale setter only goes down to 50%, so it would
need a fight with the game for little gain.

Step 1 (this version) picks the preset at start: the ini's `dlss_mode`, or the
Mods menu, where the change is marked "Restart needed". Step 2 will rebuild
the pipeline at a new render size while the game runs. That gives live preset
changes and fixes the live 16:9 / 21:9 stop.

## Presets

| Preset | Scale asked of the game | At 2560x1440 (quantized by the game) | NGX quality value, preset hint |
|---|---|---|---|
| DLAA | 1.0 | 2560x1440 | DLAA, `Hint_Render_Preset_DLAA` |
| Quality | 2/3 | 1696x954 | MaxQuality, `..._Quality` |
| Balanced | 0.58 | 1472x828 | Balanced, `..._Balanced` |
| Performance | 0.5 | 1280x720 | MaxPerf, `..._Performance` |

- Preset K (transformer) is used for every mode, as before.
- The game's setter quantizes the scale to 16-pixel steps of the basis width
  (`render_resolution::plan`). DLSS gets that extent, never a resized one.
- A DLAA basis width that is not a multiple of 16 quantizes just below 100%.
  DLSS then runs that extent as Quality.

## What changed

- **`dlss_sdk::single_scale`.** This is the one rule for every render extent:
  one scale in [0.5, 1] for both axes, truncated per axis.
  - Replaced the "identity or exact half" gates in three places:
    - the DLSS geometry contract;
    - the shader adapters' gate (`valid_filter_geometry`, which covers the
      filter, haze, depth of field, depth colour, effects, screen-shader and
      silhouette adapters);
    - the spatial fallback (`temporal_present::Output`).
  - The adaptations already used the actual ratio render/output.
- **`render_supported`** replaces `optimal_matches`. The render extent must
  lie in the mode's reported range (the NGX minimum and maximum), not within
  one pixel of the optimal extent. Example: Quality at 2560 renders 1696 wide,
  where NGX's optimal is 1708.
- **`continuous_dlaa::reconstruction_mode`** is set by the worker from the
  config before preparing. It replaces the three hard-coded `.5F` scales. The
  scaled resolve no longer insists on Performance.
- **Ini, menu and status.**
  - The ini key is `dlss_mode = dlaa | quality | balanced | performance`
    (default Performance). The log's `alpha_config` event records it.
  - The menu's Settings page has an "Upscaling" section with "DLSS mode"
    (CHOICE, RESTART) above the "Frame generation" section.
  - Status reads "Quality, 1696x954 to 2560x1440" or "DLAA, 2560x1440".

## Checked offline

- **Tests:** 668 of 668 pass, plus 182 of 182 code gates on the clean image.
  New tests:
  - the presets' geometries and range checks (`dlss_sdk_contract`);
  - the plan of every preset on nine common displays and on 21:9 pillarboxed
    in a 32:9 window is a single scale (`render_resolution_tests`);
  - the Quality and Balanced extents in the effect-vertex tests;
  - the ini key, menu model and status lines (`alpha_menu_tests`,
    `alpha_config_tests`).
- **Bug caught by the new test:** a first version of `single_scale` accepted
  touching intervals. That admitted 2560x1439 of 2560x1440, which no single
  scale can produce.

## game-61 (v0.17.0): the first Quality run

**Outline at about 2/3 size and position (confirmed cause).** Every
silhouette draw was rejected with reason 6 (160k). The check compared CBScreen
`[16..17]`, which is `fViewInverseSize` (1 / render), with 2 / output. At
Performance both are 1/1280, so the check had only ever passed by coincidence.
**v0.17.1** compares it with 1 / render. The log now records all four inverse
sizes when the check fails.

**DLSS and frame generation stop after a cutscene (an older bug, not caused
by the presets).**
- **What happened:**
  - Two same-size `resize_buffers` calls came around the cutscene.
  - From then on every plan was rejected: plan rejection 17 (an overlay
    failure), command-gate branch 10 (`input_bindings`), qualification 14,
    which is `dof_qualify`.
  - Readiness then stayed `unbuilt`, DLSS skipped every frame (skip 7), and
    frame generation had no ready frames.
- **It also happened at Performance:** two game-55 runs (v0.15.0) show the
  same rejection, 1,143 and 498 times.
- **Why the depth of field fails is still unknown.** The summary did not
  include the DOF diagnostics. v0.17.1 adds `route.dof`: the failing check,
  the failed pass, and the first failure's misses.

**DLAA would not have started.** The stream route's `prepare` refused identity
extents. v0.17.1 lets it run at identity; every adaptation then uses scale 1.

## game-62 (v0.17.1): Quality again

- **Outline: fixed (the user confirmed).** 115,506 draws were corrected. Only
  the eye and `Simple_Mt_AlphaTest` draws stay native.
- **NGX's range for Quality at 2560x1440:** optimal 1707x960, minimum
  1280x720, maximum 2560x1440. The game's 1696x954 lies inside it.
- **The depth-of-field cause (confirmed by `route.dof`).** Every cutscene DOF
  frame failed check 3. CBScreen's iViewSize, fViewSize and fViewInverseSize
  were 1696x954, while the check expected width/2 (1280x720).
  - The DOF texture layout matched: it is half the output allocation,
    independent of the render scale.
  - DLSS and frame generation resumed after the cutscene. All 1,100 rejected
    frames came during it.
- **Deeper finding.** The adapted DOF passes 0 and 3 average a 2x2 box of
  output colour per render texel, so they are exact only at 2:1.
- **v0.17.2:**
  - The view constants are checked against the render size.
  - At identity (DLAA) no pass is adapted: the native DOF already reads full
    size.
  - Other scales (Quality, Balanced) are rejected explicitly (96). DLSS skips
    frames while DOF is active and resumes afterwards.
  - A resampling DOF adaptation for those scales is open work.

## game-63 (v0.17.2): DLAA never armed

- **What happened:** the log stopped at `temporal_coexistence` "prepared"
  with no `alpha_arm`. The worker waited for the game to qualify for the
  whole session, so there was no DLSS and no frame generation.
- **Cause:** `render_resolution::ready` and `install` refused scale 1.0
  ("reduced extent plan"). They were written only to reduce the extent.
  The runtime still needs the module at 1.0: its per-frame input stamps prove
  which extent each frame rendered.
- **v0.17.3:**
  - Scale 1.0 is allowed. The override then holds the game's own full scale.
  - The wait loop logs `alpha_waiting` with its current reason after 15 s,
    so a game that never qualifies no longer leaves a silent log.

## v0.19.0: depth of field at every preset

Before this, Quality and Balanced refused the cutscene depth of field (96).
DLSS skipped those frames, so frame generation paused for the whole DOF shot.

**How the native DOF scales (from the pass-3 and composite DXBC).**
- The DOF works inside the render-scaled region of textures sized to half
  the output. That is how the game runs it at any render scale of its own.
- Pass 3 (preparation, `b7afd17a…`): thread t reads the scene at the render
  texels r0 + {0,1}², where r0 = min(2t, iViewSize - 2). It gathers depth at
  `(r0 + 1) * fScreenInverseSize`.
- The composite (`9f04832c…`) samples CoC, Image, Neighbor and alpha at
  `pixel * fScreenInverseSize`, in render space.
- In the scaled pipeline the scene is the reconstructed output-size image.
  Only pass 3's four colour loads and the composite's scene lookup have to
  change. Everything else keeps its native render-space coordinates.

**The general pass-3 adapter (`dof_compute_shader.cpp`).**
- Render texel c covers output `[c*S/s, (c+1)*S/s)` per axis:
  - S is the output size, `fScreenSize` (cb1[1].xy);
  - s is the render size, `iViewSize` (cb1[2].zw).
- Each colour load becomes the area-weighted average of the output texels in
  that interval:
  - the overlaps are exact integers in units of 1/s, divided by S;
  - two nested loops, bounded to 4 texels per axis.
- At 2:1 this is exactly the old 2x2 box. Quality and Balanced touch two or
  three texels per axis. The truncated half of an odd scene (3413 → 1706)
  touches up to four.
- No geometry is baked in, so one pipeline serves every preset.
- New temporaries r6..r12 (`dcl_temps` 6 → 13). The CBScreen declaration
  (`CB1[3]`) is checked before its fields are read.
- **The qualification:** `dlss_sdk::single_scale` replaces "2:1 or identity".
  Code 96 now means an extent that no single scale gives.
- **The composite (`dof_shader.cpp`)** already used the actual
  render/output ratios.

**Verification (offline, GPU validation on, RTX and WARP).**
- **`dof_prepare_scale_fixture` (new).** Pass 3 alone, at:
  - 2560x1440 from 1280x720 (Performance), 1696x954 (Quality) and 1472x828
    (Balanced);
  - 1920x1080 from 1280x720;
  - 3413x1440 from 1706x720.

  | Comparison | Result |
  |---|---|
  | Against a double-precision CPU area oracle | Performance exact; the rest at most 8.3e-8 |
  | Against the native shader fed a CPU area downsample in the render region (how the game feeds it) | within 8.9e-8 |
  | CoC writer | bit-identical |
  | Native shader on the output scene (negative control) | off by 0.19–0.28 |

  A hostile 1x1 `iViewSize` dispatch completes, so the loop bound holds.
- **`scaled_dof_fixture`: new fractional cases** at 2/3 (64x48 → 96x72) and
  0.575 (92x52 → 160x90).
  - The reference is the native composite drawn at output size, with
    `fScreenInverseSize` scaled by render/output and a uniform scene. Its
    DOF lookups then land at each output pixel's render-space position.
  - Result: delta 0 in both scenarios.
  - The negative control (unscaled constants) differs.
  - The chosen extents keep output pixel centres off DOF texel boundaries,
    so point sampling cannot tie.
- **`dof_native_route_fixture`** (the production six-pass route on the
  captured Performance constants and depth): output delta 0, as before.
  `dof_pipeline_fixture` with captured constants: delta 0.
- **`dof_native_route_fixture.hpp`:** a 2/3 extent now passes the scale gate
  and fails only on the capture's 2:1 CBScreen (check 3). A mixed-scale
  extent is refused (96).

**Not yet seen:** the game's real DOF at Quality and Balanced in a cutscene
(game-71 onwards).

## Not yet proven (hypotheses until the in-game runs)

- The shader adapters were verified on the GPU at 2:1 and at identity (and at
  a fractional 2:3 in `temporal_spatial_fractional_warp`). Real game shaders
  at Quality and Balanced ratios run for the first time in game.
- NGX's reported minimum and maximum for Quality and Balanced have not been
  seen yet. The SDK receipt in the log records the optimal, minimum and
  maximum. If the game's extent falls outside them, DLSS fails closed, with
  the reason "inline SDK render extent outside the mode's range".
