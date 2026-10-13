# Depth of field before the motion filter, and HDR (2026-10-13)

Status at the end of the day's session. Run folders `game-103` to `game-105`
(private, `local/fg-game-20261006`).

## Depth of field in the hub and at the title

**Finding (game-103, game-101 title).** With the game's Depth of Field option
on, the hub and the title put `uNewDOFFilter` (vtable RVA `0x3526650`) at
**order 16**: after the game's TAA (order 2) and before the motion filter
(`uMotionBlurReconstructionFilter`, order 100).

| Order | Filter |
|---|---|
| 0 | uToneMapFilter |
| 2 | the game's TAA (uTemporalAAFilter2, `0x351bda8`) |
| 16 | uNewDOFFilter |
| 100 | the motion filter |
| 1024 | uGodRaysFilter (`0x3536c98`), title only |
| 0x7fffff1 | the endpoint |

- **In the field** (the training grounds) the depth of field comes after the
  motion filter, as a middle-chain link, where the route already admitted its
  six compute passes and composition.
- **The route took any filter between TAA and motion as a single-draw
  pre-motion screen filter** (the depth-colour slot). The depth of field's
  first compute dispatch hit the overlay command gate (plan rejection 17,
  branch 7 unsupported opcode, key 0x78000010), and every such frame fell
  back to the spatial path.
- **Frames lost:** 1,482 title frames in perf12 (game-101), and 2,202 and
  2,457 frames in the HDR test (game-103).
- **Why it never showed before:** the user's earlier runs had depth of field
  off. Depth of field is likely on for most players.

**v0.24.5:** the pre-motion slot takes the depth of field.
- It reads the six dispatches as in the chain (`dof_read`), then the
  composition draw: kind 3, PS `9f04832c…`, with its scene source (t2) the
  TAA output.
- Dependency discovery admits kind 3 there only when it closes that
  sequence, and requires its scene read to be the TAA copy (resource 1) that
  DLSS fills at full size.
- Qualification and recording are the existing six-pass path.

**game-104 (v0.24.5):**
- **Hub, after loading: fixed.** The depth of field qualified on 2,209 frames
  (six passes each, 0 failures, 0 rejections). DLSS ran 3,443 frames and
  frame generation tagged 3,442 with 0 errors.
- **Title screen: still refused** (898 frames, the same gate at a
  depth-of-field dispatch). The title's depth of field differs in some way:
  a seventh pass, an unreadable dispatch, or a second sequence.

**v0.24.6** (diagnostic only) records the reason in the gate's `seen` word:
0x100 * why + passes read.
- **why 1:** a dispatch after the composition;
- **why 2:** another sequence's key;
- **why 3:** a seventh dispatch;
- **why 4:** an unreadable dispatch.

**game-105 (v0.24.6) did not answer it.** The worker stopped with
`driver-On-not-enabled` before any frame: the mod armed after 3.1 s, before
the game's first frame, and the driver reported Reflex low latency off at the
first heartbeat. The user attributes it to a corrupted NVIDIA cache or driver
state, a known MHW issue, and will do a clean driver reinstall. See
`game-105/NOTES.md`.

**After the clean NVIDIA driver reinstall:**
- **game-106 (v0.24.6):** the first launch, slow while shaders compiled.
  Astera Loader hooked about 26 s in. The game exited about 10 s after
  arming, before the first 20 s summary, so it logged nothing.
- **v0.24.7:**
  - The worker writes a last `fg_status` and `dlss_summary` on the game's
    exit.
  - Reflex is confirmed at the first heartbeat with at least 60 frames.
    Before, it was checked at the first heartbeat. game-105 rendered no frame
    in its first 20 s, so a slow first launch after any driver update would
    have left DLSS off for the session.
- **game-107 (v0.24.7, title only):**
  - **0 plan rejections.** The pre-motion depth of field qualified on all
    5,482 title frames (six passes, 0 failures), and DLSS ran 5,479 frames.
  - Reflex was confirmed running (`low_latency: true`), and the exit summary
    was written.
  - game-104's title refusal (898 frames) did not recur, and nothing since
    changed the admission. It was transient: the corrupted cache, or the
    first seconds after boot. v0.24.6's gate reason stays in, to name it if
    it returns.

**Next:** confirm the hub's depth of field visually (the blur's size and
position) in the long session.

## HDR (game-103, v0.24.4)

The user turned on Windows HDR and the game's HDR mode (`ScreenMode=HDRMode`).
- **The swap chain:** the game creates it as B8G8R8A8 and resizes it to
  **R10G10B10A2 (format 24, HDR10)**, switching back and forth on mode
  changes. The mod followed every resize; there were no faults.
- **DLSS** ran whenever its plan qualified (628 frames). It works on the
  linear scene before tone mapping. Most frames fell back to the
  depth-of-field rejection above, not because of HDR.
- **Frame generation** armed and presented generated frames: 620 presents
  with a generated frame, 0 SDK errors.
- **The HUD-less image:** its CBSystem brightness capture never matched in HDR
  (626 fallbacks, 0 GPU matches). The game's HDR screen write is not the SDR
  one the HUD-less pass copies (brightness curve, sRGB encode, R8G8B8A8). So
  the HUD-less image was SDR-encoded for an HDR10 back buffer. **The user saw
  the HUD looking fine**, but the colour space is wrong in principle.
### The game's HDR screen write (shader census, 2026-10-13)

The census has three back-buffer copies:
- `System_CopyToBackBuffer` (PS `e5ee2062…`): the SDR brightness curve and
  sRGB encode the HUD-less pass already mirrors, with CBSystem at b3.
- `System_CopyToBackBufferPQSingle` (PS `68b496ea…`): HDR10 only.
- `System_CopyToBackBufferPQ` (PS `a9e2838c…`): SDR to o0 and HDR10 to o1.

The HDR10 path takes its parameters from **CBRenderFrame (b2)**:
`fHdrOutputWhiteLevel` @52 (default 100), `fHdrOutputGamutMappingRatio` @56,
`fHdrOutputGamma` @60, `bIsGUIHdrGamma` @64, plus `bHdrOutput` @48.
1. c = max(T, 0); b = the BT.709→BT.2020 rotation of c, with rows
   (0.627404, 0.329282, 0.043314), (0.069097, 0.919540, 0.011361) and
   (0.016392, 0.088013, 0.895595).
2. x = b + ratio * (c − b).
3. If bIsGUIHdrGamma: each channel below 1 becomes pow(max(x, 0), gamma).
4. ST.2084 PQ of saturate(x / (10000 / white)), capped at 1.

There is no brightness curve. That is why game-103's CBSystem capture never
matched.

### v0.24.8: HUD-less in HDR10

- **`screen::hdr::encode`** (screen_transform.hpp) is the C++ reference of
  that path.
- **HudlessPass:**
  - An HDR transform writes R10G10B10A2_UNORM. Until a valid capture it uses
    the shader's defaults: 100 nits, ratio 0, gamma 1, no GUI gamma.
  - An HDR capture of CBRenderFrame checks its layout: bHdrOutput set,
    booleans 0 or 1, white level 10-10000, ratio 0-1, gamma 0.1-10, delta
    time 0-10 s.
  - The capture keeps the last valid capture and records the graphics root
    parameter it was bound at.
  - The target's format selects the transform; the SDR path is unchanged.
- **The session:**
  - It learns the back buffer's format at arming and after every swap-chain
    call (the presentation's lifecycle listener, new `after` callback). The
    game resizes 8-bit then HDR10 on each mode change.
  - A slot's HUD-less image is re-created in that format when the slot is next
    used; slots are fence-gated, so it is free then.
  - With an HDR10 back buffer, each closing list in the capture window (at
    most 4 a frame) offers its CBV root arguments to the HDR capture.
  - Any other back buffer format gets no HUD-less image (skip reason
    `format`).
- **Status:** `hudless.back_buffer_format`, `hudless.recreated` and
  `hudless.hdr{captures, probe_ok, probe_default, used, valid, white, ratio,
  gamma, gui_gamma, gpu_attempts, gpu_matches, parameter, last_attempt}`.
- **Test (hudless_pass_warp, WARP with GPU-based validation):** 5 HDR steps
  with valid and invalid CBRenderFrame layouts, the defaults before any
  capture, and the matched root parameter; 43,200 channels within 1.5 steps
  of 10-bit of the reference. SDR is unchanged (69,120 channels), also after
  HDR.
- **Confirmed in game-109 (v0.24.8 tagged).** The user restarted the game
  with HDR on (the first process stayed 8-bit and its SDR path was as before).
  - In HDR10 the HUD-less image was recorded 11,086 times and tagged 11,081
    times, with 0 skips.
  - 11,082 probes used a valid capture and 1 used the defaults (the first
    frame).
  - Captured: white 720.69, ratio 0.137931, gamma 0.972414, GUI gamma off, at
    graphics root parameter 4.
  - DLSS fault 0. The user: "just georgeous".
  - Not exercised in game: a slider change during a run, and an SDR/HDR switch
    within one process (the WARP test covers the re-creation).

## Also open from the day

- Two single-frame rejections with a radial blur (order 4100) and a filter
  at order 26005 (perf12).
- The plan build's lock time (`route_queue`, 0.15-0.2 ms a frame), if a CPU
  with fewer cores shows it (performance.md, perf12).
