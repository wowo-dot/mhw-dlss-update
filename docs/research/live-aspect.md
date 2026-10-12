# Live aspect change (part B, research)

Roadmap step 2, part B: recover from the game's live 16:9 / 21:9 switch
instead of stopping DLSS. Part A, live presets (live-presets.md), built the
rebuild this needs. The scene size also changes here, and so do the targets
the game draws into.

## What the game does (clean image, IDA, 2026-10-13)

Names are Fexty's where they exist. Offsets are in `sRender` (the renderer,
`image+0x51c4480`) unless stated.

- **The aspect mode:** `+0x7b440` is requested and `+0x7b43c` applied, with
  0 the window's own, 1 16:9 and 2 64:27. Applying it uses ratios of
  1.7777778 and 2.3703704 (64:27: 2560x1080 in a 2560x1440 window).
- **Applying it: `0x229ef20` (named `sRender::finishScreenShot`).**
  - It runs on the render thread, from the frame-begin function `0x22980b0`
    (call at `0x22983f5`), which also calls the scale update we hook
    (`0x22982e7`).
  - When requested ≠ applied, under the renderer's lock, it:
    - copies requested to applied;
    - fits the window size (`+0x1f448`/`+0x1f44c`) to the ratio and writes the
      scene size `+0x198`/`+0x19c`;
    - writes the ratio to `+0x7b444`;
    - writes the scene rectangle in the window (`+0x188..+0x194`, via
      `0x229f090`; also used by the GUI);
    - sets flag 4 in the pending-change word `+0x1f548`.
- **Consuming the flags: `0x229b020`, on the game thread** (call at
  `0x22e4be4` in the game loop `0x22e4ae0`). With flags pending it waits for
  the render thread (`0x229cbf0`, `WaitForSingleObject`), clears the flags,
  and then:
  - **Without flag 0x40:** if the backend has a pending resize, or in
    exclusive full-screen with flag 4, it recreates the views. `0x229c880`
    presents, waits for the GPU and releases the views and target objects
    (`+0x1f4d0`, `+0x1f4e0`, `+0x1f4d8`, `+0x41b90`); `0x23ac560` resizes
    the backend (the same-size `ResizeBuffers` seen in game-15); `0x229b9a0`
    recreates them; `0x2293f10` creates the target set at the scene size.
  - **With flag 0x40:** the target set is resized in place or recreated
    (`0x2298db0`, then the renderer's virtual `+0x28`, then `0x2293f10`).
  - Then it publishes the scene size to the camera
    (`qword_1451C4400 + 0xd58/0xd5c`) and the scene rectangle.
- **The `+0x2cda` flag in `sAppExt` is not the aspect option.**
  `sAppExt::ctor` always sets it to 1 on PC. It makes the window take its size
  from the display config, and the render-scale basis (`+0x2cd0`/`+0x2cd4`) is
  then the window's client size (`0x22e5310`, window creation).

**Unknown until seen in game:**
- the order of these steps across the render and game threads, frame by frame;
- whether the window-mode path recreates the targets (no flag 0x40, no
  backend resize pending);
- whether the content scale (`+0x200`, our check 16) moves.

## The observation build (v0.22.1)

- **`render_resolution` captures the switch.** Each visit's snapshot now
  also reads the window size, the requested and applied aspect, the
  pending-change flags and the scene rectangle.
- **A visit is notable when:**
  - requested ≠ applied;
  - the scene or window size differs from the previous visit;
  - the sizing bit is set;
  - or it is the first refused snapshot.
- **Each capture** holds the 16 visits before its first notable visit and the
  240 from it, with QPC times. There are two per session: the switch and the
  way back.
- **The worker writes them** as `aspect_capture` events: in the loop, after a
  DLSS fault (waiting up to 3 s), and in the post-fault loop.
- **The switch still stops DLSS** (fault 4), but since v0.22.0 the game then
  renders natively at its own scale.
