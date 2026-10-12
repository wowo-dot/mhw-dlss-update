# DLAA: the game's 100% render path (2026-10-12)

Reversed for true DLAA (the user's call: "we do things properly"). Sources:
- the clean IDA database `local\ida-clean-20261011\mhw.i64` (Fexty's names);
- the clean image `a78ea69b…1d80`;
- the exe `c2ebbbd2…90ea`;
- the in-game run game-64 (v0.17.3, DLAA).

## The symptom (game-64)

DLAA armed (render-resolution control at scale 1.0), and frame generation
armed. But DLSS evaluated **0 frames**.
- Every plan was rejected with reason 6, detail 5. The saved final-quad source
  was missing: the plan's link 7, `final.source`, was 0.
- DLSS skipped every frame (skip 7, route not ready), so frame generation had
  no ready frames.
- The post-filter queue also held `uGodRaysFilter` (order 1024, no input).
  This is harmless: filters without an input are not colour links.

## The branch (confirmed statically)

In `sub_1423C3790` (view preparation; it records the final screen step):

```
0x23c7a49  ucomiss xmm12, [view+0x6B0]      ; view scale == 1.0 ?
0x23c7a69  jnz     0x23c7add                 ; no  -> final quad
0x23c7a8c  cmp     {source size}, {dest size}; same packed extents?
0x23c7a93  jnz     0x23c7add                 ; no  -> final quad
0x23c7ad6  call    sub_1423C9250             ; yes -> a native copy (return 0x23c7adb)
0x23c7adb  jmp     0x23c7b08
0x23c7b03  call    sub_142297E20             ; the System_Copy quad (return 0x23c7b08)
```

- **The two paths.**
  - At render scale 1.0 with equal source and destination sizes, the game
    skips the System_Copy quad helper (`0x2297e20`, which the stream route
    hooks with caller `0x23c7b08`). It enqueues a copy instead.
  - Below 1.0 it draws the quad, which is how every scaled preset works.
- **The copy, `sub_1423C9250`.** It enqueues an 80-byte packet with opcode 13
  (`raw & 31 == 13`), the same packet kind as the TAA hand-off copy the route
  already reads:
  - `+0x10` destination owner;
  - `+0x18` source owner;
  - `+0x24` and `+0x28` subresources;
  - `+0x30` destination x, y (zero here);
  - `+0x38` source box (`var_4A0`: 0 and the packed source width and height);
  - `+0x48` an optional pointer (null).
- **Its sort key** is built from the context's current stage (`ctx+0x154`
  bits) like any queued command. It is enqueued where the quad would be
  (stage 16, `cDraw::setDrawPass(0x11)` follows).
- **Its operands.** The source is the colour-grading output (the quad's
  `tBaseMap` in the scaled path). The destination is `view+0x460 → +0x58`, the
  same final attachment as the quad's.

## What true DLAA needs (design, not yet built)

- **The final anchor.** At identity the stream route takes the opcode-13 copy
  at the final stage as the frame's final step. It needs the same proofs as
  the quad:
  - the source equals the grade output;
  - the destination is the view's final attachment;
  - same frame and stamp.
- **No effect replays at identity.** Every post effect already renders at full
  size; the native draws stay.
- **The DLSS hand-off is unchanged.** The TAA copy (key `0x78000002`) is
  where the reconstruction enters the chain, as in the scaled presets.
- **Frame generation's HUD-less capture** needs an anchor after the final copy
  is recorded. The scene is then in COPY_DEST, not RENDER_TARGET, so the
  HUD-less pass takes the scene's current state.
