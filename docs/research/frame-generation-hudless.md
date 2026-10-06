# In-game HUD-less colour for DLSS-G (v0.9.0, fixed in v0.9.1)

Design, 2026-10-06. Goal: give DLSS Frame Generation a HUD-free image in the
backbuffer's colour space, so generated frames stop warping the HUD. Background:
`frame-generation-hudless-capture-result.md` (RenderDoc) and
`frame-generation-game-slice-result.md`.

## Where the inputs come from

- **HUD-free image.** The final scene target `T` (2560x1440 `R11G11B10_FLOAT`,
  linear) right after our replaced final-screen draw (`stream_route`
  `record_effect`, stage 2) and before the first UI draw. At that point `T` is
  in `RENDER_TARGET`, bound as the only RTV, and native state has been verified
  restored.
- **Brightness parameter.** The game's `CBSystem` constant buffer (b3, 32
  bytes: `iRegion@0`, `fSourceMipLevel@16`, `fGammaCorrect@20`,
  `fGammaCorrectEx@24`). It is bound to the final-screen draw as graphics root
  CBV parameter 5 (PS bN = parameter 2 + N in MHW's root-CBV layouts). The pass
  binds that same GPU address, so it reads exactly the value the game uses that
  frame. No CPU read of game memory.
- **Transform.** Per channel: `c = saturate(T)`,
  `k = min(0.05 g + 0.95, 1)`, `c' = 1 - (1 - k c)^g`, then the sRGB encode
  (`screen_transform.hpp`, unit-tested; the HLSL mirrors it). If `g` is outside
  [0.25, 4], or `fGammaCorrect` is not 1/2.2 (layout check), the pass uses the
  neutral curve `g = 1` and reports it.

## Recording

1. The DLSS replacement now only *stages* the frame's converted depth/motion and
   camera (`game::stage`, no GPU work).
2. The final-screen draw of the same frame calls the FG observer (installed only
   in `mhw_fg_game_probe`). The frame and epoch must match the staged frame;
   otherwise nothing is recorded. On the game's DIRECT list, with this runtime's
   hooks bypassed (`injected`) and the stream route's `injecting` set, it:
   - acquires a free ring slot (fence-gated, as before) and copies depth and
     motion into it;
   - saves bindings (`RestoreBindings`) and runs the HUD-less compute pass:
     transition `T` RENDER_TARGET → NON_PIXEL_SHADER_RESOURCE, dispatch into
     the slot's `R8G8B8A8_UNORM` image, transition back, copy a 16-byte probe
     (`g`, `fGammaCorrect`, ok flag) to a readback buffer;
   - restores the game's heaps, root signatures, PSO and root arguments.
     `stream_route` re-verifies the restore and faults otherwise.
3. Present tags depth, motion, backbuffer extent and `kBufferTypeHUDLessColor`
   (output extent, `NON_PIXEL_SHADER_RESOURCE`). A slot without HUD-less tags a
   null HUD-less resource.

Fail-closed: if the CBSystem argument is not a root CBV, the compute root is
unknown on that list (no restore possible), pass creation failed, or recording
threw, that frame has depth and motion only, and the counters say why
(`hudless.skips`).

## Telemetry (fg_status `session`)

`hudless.{available, failed, recorded, tagged, skips, probe_ok, probe_fallback,
probe_invalid, gamma_ex, gamma}` and `staged`, `staged_dropped`,
`final_unstaged`, `final_mismatch`. A probe is read when its slot is reused,
after the fence proves the pass is complete.

## Test

As for v0.8.0: arm at the main menu, gameplay at 2x and 4x. Watch the HUD in
generated frames (minimap, item bar, damage numbers, prompts) while moving the
camera. Accept when `hudless.tagged` tracks `tagged`, the probe reports the
expected brightness, there are zero skips in gameplay, and the user sees a
stable HUD.

## v0.9.0 in game (game-07): HUD-less pass never created

The FG path itself worked at the new recording point: 2x for 2553 tagged
Presents with no record failures, exceptions or SDK errors; `staged_dropped`
4, `final_unstaged` 7, `final_mismatch` 17, all around loads. HUD-less was
`failed` from the first frame, so every frame was skipped as `disabled` and
DLSS-G got a null HUD-less tag. The failure reason was discarded.

Cause, reproduced offline with fxc: X3571 ("pow(f, e) will not work for
negative f") on `pow(c, 1/2.4)` in the sRGB encode, made fatal by
`D3DCOMPILE_WARNINGS_ARE_ERRORS`. No test compiled the shader.

## v0.9.1

- The encode clamps its input to [0, 1] (C++ mirror too; no value change for
  the transform's outputs).
- `HudlessPass::compile()` is exposed. `hudless_pass_warp` (ctest) compiles it
  as the DLL does and runs the pass on WARP with the debug layer and GPU-based
  validation. It checks five CBSystem cases (g = 1, 2, 0.5; g out of range;
  wrong `fGammaCorrect`) on a 72x40 scene (not a multiple of the group size),
  covering saturate and the linear sRGB segment. Every channel is within 1 LSB
  of `screen_transform.hpp`, and the probes report the CBSystem values and
  verdicts. Barriers leave the scene in RENDER_TARGET and the target in its tag
  state.
- The debug layer also showed that the probe buffer was created in
  `COPY_SOURCE`, which buffers ignore (they start in COMMON). The probe now
  rests in COMMON: COMMON → UAV → COPY_SOURCE → COMMON on each record.
- The first HUD-less failure message is kept (`hudless.error`, 159 printable
  characters, JSON-safe) for both creation and recording failures.

## Not covered

UI colour+alpha (DLSS-G recomposition), menu-specific behaviour (no DLSS pass
means FG Off), HDR output.
