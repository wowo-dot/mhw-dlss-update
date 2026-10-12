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

## Built: v0.18.0 (the in-game test is pending)

**Building the plan.** When render == output, `build_identity_plan` replaces
the scaled builder. It proves:
- the TAA filter and the grade unit, one each;
- the grade output (`grade+0x250`) and the view's final attachment
  (`view+0x460 → +0x58`);
- in the completed queue, exactly one TAA hand-off copy (key `0x78000002`,
  TAA input to the motion-filter input, the render rectangle) and exactly one
  stage-16 opcode-13 copy from the grade output to the final attachment
  (rectangle `0,0,width,height`, zero mode, subresources, offsets and
  optional pointer);
- unchanged queue and stamp.

It then pins four resources: the TAA input, the hand-off destination, the
grade output and the final target.

**Plan contents.** Effect count 0, four resources, three handoff uses:
resolve, TAA copy, final copy. Overlays, DOF and silhouette pieces are not
planned, so they stay native.

**The copy hook.** `copy_before` also matches an identity plan's final copy
packet, through a per-thread flag. The final branch:
1. checks the executor caller, the packet and its fields, the resources and
   the full box;
2. checks states: a known contradicting state fails; an unknown one is
   accepted, because the native copy requires and leaves COPY_DEST;
3. records handoff use 2 and forwards the copy unchanged;
4. calls the final-scene observer with the scene in COPY_DEST.

**Frame generation.** `HudlessPass::record`, `game::record` and the observer
take the scene's state. It is RENDER_TARGET after the scaled final draw, and
COPY_DEST here.

**Telemetry.** The `route.identity` summary records plans, final copies and
observed copies.

## game-65 and game-66: depth at 100%

**game-65 (v0.18.0).** The identity route worked: 3,586 plans, 3,585 final
copies observed by frame generation. But DLSS skipped every frame
(`depth_missing`), so the clean image the user saw was the game's own render,
and frame generation had nothing to generate from.

**game-66 (v0.18.1, depth diagnostics).**
- **The admission.** The scaled depth admission rejected every attempt with
  reason 1, its entry condition: it requires the nested depth writer
  (`expected.nested`) and a render rectangle smaller than the depth texture.
  Both are false at 100%.
- **The hooks.** The depth producer and consumer hooks fired (about 14k each)
  and matched DLSS's frame. The nested depth draw never happened (trace
  stage 4: 0).
- **Conclusion.** At 100% the depth writer is the game's plain native
  `CopyResource` (caller `0x259e100`). That is the path the unscaled DLAA
  build already leased.

**v0.18.2.** In the scaled build, the unscaled depth lease runs when the
render extent equals the scene (`identity_extent`):
- the native depth copy inside the depth-consumer callback is checked for
  source, destination, barriers, list and frame;
- the lease is taken, and the depth is copied into DLSS's private texture
  right after;
- at identity the lease alone qualifies the depth, with no scaled admission.

**game-67 (v0.18.2).** The plain depth copy now reached the lease path. The
copy's caller (`0x259e100`), command list, source, destination and count all
matched. It was rejected (trace stage 5, reason 8) by `copy_states`, which
must *see* both transitions inside the depth-consumer callback. At 100% the
planner issues them earlier: the transition hoisting at `0x229eb22`
described in `scaled-consumer-depth-copy.md`.

**v0.18.3.** `copy_states_consistent` accepts a resource with no observed
transition. The native `CopyResource`, which ran first, requires COPY_SOURCE
and COPY_DEST, so the copy itself establishes them. Observed transitions must
still end there, and aliasing, split and subresource barriers still reject.
It is tested in `temporal_inputs_contract`. Only the 100% path uses it; the
unscaled build keeps the strict check.

**game-68 (v0.18.3): DLAA works (the user: two runs, "works correctly").**

| | Run 1 | Run 2 |
|---|---|---|
| DLSS frames | 3,334 | 3,791 |
| Identity plans and final copies | 4,781 / 4,781 | 3,877 / 3,876 |
| Generating at the last status | yes, mostly 2 frames per Present | yes |
| HUD-less records | 4,347 | 3,876 |
| Frames skipped for missing depth | 1,447, 1,150 of them in the first 20 s (title and loading) | 85 |
| Exceptions | 0 | 0 |

- No crash in either run.
- **Open: a Reflex frame fault in DLAA sessions.** Seen in game-64, 65 and 67
  and in game-68 run 1, not run 2, and never in the scaled runs.
  - It came after the last heartbeat. The game kept rendering about 9 s
    afterwards, which suggests a quit or return-to-title path.
  - Frame generation stops for the session when it happens.
  - v0.18.4 logs the fault's reason in `fg_stopped` (`reflex_fault`).

## What true DLAA needs (design)

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
