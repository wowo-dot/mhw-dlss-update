# In-game HUD-less colour for DLSS-G (v0.9.0 to v0.9.2)

Design, 2026-10-06. Goal: give DLSS Frame Generation a HUD-free image in the
backbuffer's colour space, so generated frames stop warping the HUD. Background:
`frame-generation-hudless-capture-result.md` (RenderDoc) and
`frame-generation-game-slice-result.md`. The current design is v0.9.2; the
sections after "Test" record how it got there.

## Where the inputs come from

- **HUD-free image.** The final scene target `T` (2560x1440 `R11G11B10_FLOAT`,
  linear) right after our replaced final-screen draw (`stream_route`
  `record_effect`, stage 2) and before the first UI draw. At that point `T` is
  in `RENDER_TARGET`, bound as the only RTV, and native state has been verified
  restored.
- **Brightness parameter.** The game's `CBSystem` constant buffer (b3, 32
  bytes: `iRegion@0`, `fSourceMipLevel@16`, `fGammaCorrect@20`,
  `fGammaCorrectEx@24`). It is bound **only to the screen write** (T → swap
  chain, after the UI) as graphics root CBV parameter 5 of its root signature.
  The screen write is the last draw of its command list. That list differs from
  the final-scene list but goes in the same `ExecuteCommandLists` batch
  (RenderDoc, see below). No CPU read of game memory.
- **Transform.** Per channel: `c = saturate(T)`,
  `k = min(0.05 g + 0.95, 1)`, `c' = 1 - (1 - k c)^g`, then the sRGB encode
  (`screen_transform.hpp`, unit-tested; the HLSL mirrors it). Without a valid
  captured `g` the pass uses the neutral curve `g = 1` and reports it.

## Recording

1. The DLSS replacement only *stages* the frame's converted depth/motion and
   camera (`game::stage`, no GPU work).
2. The final-screen draw of the same frame calls the FG observer (installed only
   in `mhw_fg_game_probe`). The frame and epoch must match the staged frame;
   otherwise nothing is recorded. On the game's DIRECT list, with this runtime's
   hooks bypassed (`injected`) and the stream route's `injecting` set, it:
   - acquires a free ring slot (fence-gated) and copies depth and motion into
     it;
   - saves bindings (`RestoreBindings`) and runs the transform: `T`
     RENDER_TARGET → NON_PIXEL_SHADER_RESOURCE, dispatch into the slot's
     `R8G8B8A8_UNORM` image with `g` from the FG screen-state buffer, transition
     back, copy a 32-byte probe to a readback buffer;
   - restores the game's heaps, root signatures, PSO and root arguments.
     `stream_route` re-verifies the restore and faults otherwise.
3. **Brightness capture** (`game::closing`, from the Close hook, before the
   native Close). Between a transform record and the next Present, each DIRECT
   list that closes with a root CBV at graphics parameter 5 gets, as its last
   commands, a one-thread compute pass (at most 4 per frame). The pass reads
   that CBV. If `fGammaCorrect` is 1/2.2 (layout check) and `g` is in
   [0.25, 4], it stores `g` in the FG state buffer; otherwise it leaves the
   buffer alone. It always counts the attempt (and a match) on the GPU. No
   restore is needed because Close follows immediately. The screen write's
   list is one of these lists, so the next frame's transform uses this frame's
   brightness. A changed setting reaches the HUD-less image one frame late.
4. Present tags depth, motion, backbuffer extent and `kBufferTypeHUDLessColor`
   (output extent, `NON_PIXEL_SHADER_RESOURCE`). A slot without HUD-less tags a
   null HUD-less resource.

Fail-closed: if the compute root is unknown on the final-scene list (no
restore possible), pass creation failed, or recording threw, that frame has
depth and motion only, and the counters say why (`hudless.skips`). With no
valid capture, the transform uses `g = 1`.

## Telemetry (fg_status `session`)

`hudless.{available, failed, recorded, tagged, skips, probe_ok, probe_fallback,
probe_invalid, gamma_ex, gamma, gpu_attempts, gpu_matches, last_attempt,
captures, capture_no_cbv, capture_limited, capture_failed, error}`, plus
`staged`, `staged_dropped`, `final_unstaged` and `final_mismatch`. A probe is
read when its slot is reused, after the fence proves the pass is complete.
`probe_ok` counts frames that used a captured `g`. `gpu_attempts` and
`gpu_matches` are lifetime GPU counters of capture passes.

## Test

Arm at the main menu, then play at 2x and 4x. Watch the HUD in generated frames
(minimap, item bar, damage numbers, prompts) while moving the camera. Accept
when:
- `hudless.tagged` tracks `tagged`;
- `probe_ok` covers gameplay frames;
- `gpu_matches` grows by one per frame;
- the captured `g` is plausible (expected 1.0 at the user's setting);
- there are zero skips in gameplay;
- the user sees a stable HUD.

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
- `HudlessPass::compile()` is exposed. `hudless_pass_warp` (ctest) compiles the
  shaders as the DLL does and runs the pass on WARP with the debug layer and
  GPU-based validation on a 72x40 scene (not a multiple of the group size),
  covering saturate and the linear sRGB segment. Every channel is within 1 LSB
  of `screen_transform.hpp`. Barriers leave the scene in RENDER_TARGET and the
  target in its tag state.
- The probe buffer was created in `COPY_SOURCE`, which buffers ignore (they
  start in COMMON). It now rests in COMMON.
- The first HUD-less failure message is kept (`hudless.error`).

## v0.9.1 in game (game-08): HUD stable; brightness was the fallback

- 19,176 Presents were tagged with HUD-less colour (`recorded` 19,177), with
  zero skips, record failures or exceptions, until the build's 600 s session
  limit turned FG Off. Presented frames per Present: 19,103 x2 and 73 x1.
- **User:** the HUD looked stable. The usual frame generation artifacts
  were visible in the scene, but not on the HUD.
- The probe reported `fallback` on every frame. Root parameter 5 at the
  final-scene draw held a different buffer: `fGammaCorrect` read as the bit
  pattern 1 and `fGammaCorrectEx` as 0. So the transform used `g = 1`, which
  matches the user's setting (`g` was 1.0 in the capture). The output was
  right, but not by design.
- Root cause (RenderDoc, `tools/rdc/cbuffer_report.py` with the 1.46 root
  signature API, and `tools/rdc/api_window.py`). In the unmodified frame,
  `ExecuteCommandLists(3)` submits lists [0] and [1] (T arrives on [1]), then
  list [2]: 239 UI draws, the copy of T, then the screen write. The screen
  write sets only its PSO and `SetGraphicsRootConstantBufferView(5, CBSystem)`
  on the UI's root signature, draws 4 vertices, and the list closes after a
  barrier and `ResolveQueryData`. CBSystem is never bound before the UI, so the
  v0.9.0 design read the wrong draw's parameter 5. The mapping was right; the
  draw was not.

## v0.9.2

The brightness capture described in "Recording" step 3. `hudless_pass_warp`
covers six steps:
- no capture yet, so `g = 1`;
- a valid capture;
- a bad layout, which keeps the previous `g`;
- another valid capture;
- an out-of-range `g`, which keeps the previous value;
- valid then invalid in one list, which keeps the valid one.

Each step checks the transform output, the probe's last-valid and raw
last-attempt values, and exact GPU attempt/match counts, with captures and
the transform in separate lists as in the game.

## Not covered

UI colour+alpha (DLSS-G recomposition), menu-specific behaviour (no DLSS pass
means FG Off), HDR output.
