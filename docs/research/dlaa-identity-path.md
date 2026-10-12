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
- **Open: a Reflex frame fault in DLAA sessions.** Seen in game-64, 65, 66 and
  67 and in game-68 run 1, not run 2, and never in the scaled runs.
  - Frame generation stops for the session when it happens.
  - v0.18.4 logs the fault's reason in `fg_stopped` (`reflex_fault`).

## game-69 (v0.18.4): the fault is the game's exit

The user played DLAA, quit to the title, loaded back in, played about 30 s and
quit.
- **No fault this run.** DLSS ran 5,753 frames and frame generation was on at
  the last status, mostly 2 frames per Present (5,714 of them). It paused at
  the title screen and came back within a few seconds after loading in (the
  user saw it too). Quit-to-title is therefore not the fault path.
- **The fault is the last frame of the process (from the five faulted logs).**
  After the last heartbeat, the presentation hook counted exactly as many
  Presents as the Reflex hooks completed frames (158, 758, 1,478, 1,515 and
  2,014). So the game presented nothing after the fault.
  - The 7 to 11 s "stop" window measured from the last heartbeat *to* the
    fault. It is not rendering after it, as v0.18.4's notes said.
  - Each log then ends within 5 s, with no `fg_off` window.
  - The two clean DLAA logs (game-68 run 2, game-69) end at a heartbeat with
    nothing after it, like every scaled log. The process ended before the
    next poll, so a fault on the way out may simply not have been logged.
- **The exit path (static, clean database).**
  - `sAppExt::run` (the window thread) sets the quit flag (`+0x2BE8`) on
    WM_QUIT and waits for the game thread.
  - The game thread `sub_1422E4AE0` loops on that flag: each turn calls the
    renderer (`sub_14229C0C0` or the vtable `+0x68`, then `sub_14229B020`),
    then Main:Update (`vtable +0x30` of sMhMain, the hooked `0x1af2b60`).
  - After the loop it destroys its systems on the same thread. `sMain`'s
    destructor (`sub_141AEE800`) first waits on `sub_141B1FB90` (32 ms
    sleeps), then tears down units, and finally clears the engine global
    (`qword_1451C4148 = 0`, `sub_14225CA50`).
  - Candidates: a hooked render call during that teardown would fail the
    identity or owner check *before* the hook sees it is outside a frame;
    or the last frame returns with changed identities; or Streamline
    refuses a marker while the swap chain goes away.
- **v0.18.5 (diagnostics only).**
  - The first fault records which call (Main:Update, Publish, Submit, their
    returns, Present), the thread and owner, the identity links that failed
    (a mask), the Streamline result or Present HRESULT, the failing stage,
    the frame id and whether a frame was active.
  - The worker logs a `reflex_fault` event the moment it sees the fault,
    before turning frame generation off, so a quick exit still leaves it.
  - No behaviour change: the fix waits for the logged cause.

## game-70 (v0.18.5): the cause, and the fix (v0.18.6)

**The logged fault.** The run lasted 22,358 Reflex frames, with DLSS on 10,315
of them and frame generation on to the end. On exit:

```
reflex_fault "frame-policy-or-sdk" detail {call 100, thread = owner, links 0,
  code 275, stage 5, in_frame true}
```

- Call 100 is Main:Update's return, on its own thread, with every identity
  link intact.
- Code 275 is 0x100 | 19: the policy was disabled, and 19 is
  `eErrorInvalidIntegration`, which `FrameSession::apply` returns for a policy
  fault, not a Streamline error.
- Stage 5 is `FramePolicy::end`. With the thread right, its only fault is
  `order`: the engine frame ended before its Present.
- The log ends right after that event. The process exited during the 3 s
  wait for frame generation to switch off, so there is no `fg_stopped`.

**Why the last frame has no Present (static, clean database).**
- The window procedure `sub_1422E3160` handles WM_DESTROY (message 2). It
  saves the window position, then calls `sub_1423ACBF0(renderer+0x78,
  hwnd)`, then `PostQuitMessage(0)`.
- `sub_1423ACBF0` sets `cD3DRender+0x1482 = 1` when the window is the swap
  chain's (`cD3DRender+0x1460`). Only `initForD3D11/12` clear it.
- `cD3DRender::present12` checks it before every Present
  (`0x23a9e20 cmp byte ptr [rbx+1482h],0; jnz 0x23aa22c`). The target leaves
  the critical section and returns success without calling Present.
- So the engine frame running when the window goes away (the window thread
  sets the flag while the game thread is mid-frame) renders but skips
  Present. WM_QUIT then sets the quit flag, and the game thread's loop ends.
- Ruled out along the way: `sRender+0x91` is `mFrameSkip` (with
  `+0x93 mParallelRendering`, `+0x92 mDisableRendering`, from
  `sMhRender::populatePropertyList`). It gates the other early return, in
  `sub_1422980B0`. The constructor clears it and no game code sets it, so
  that path does not skip frames here.
- It is a race with the window thread. That explains why the fault shows in
  some exits and not others, and why some faulted logs never reached
  `fg_stopped`. Nothing ties it to DLAA itself.

**v0.18.6: the game's exit closes the session instead of faulting.**
- Three read-only code spans are gated at install, each against the clean
  image:
  - the setter `0x23acbf0`, 17 bytes;
  - present12's check and jump `0x23a9e20`, 13 bytes;
  - the jump target `0x23aa22c`, 91 bytes, which returns without Present.
  The result is logged as `exit_gate` in `frame_hooks_installed`. If a span
  differs, the flag is never read and the old behaviour stays.
- On Main:Update's return, after the identity and thread checks, the hooks
  read `backend+0x1482`:
  - if it is set and the frame did not present (`FramePolicy::presented()`),
    the session closes;
  - if it did present, the frame ends normally and then the session closes.
  "Closes" means `running` is off and `closed` is set; the policy aborts
  with no fault and no closing markers.
- Main:Update's entry also closes rather than begin a frame the game cannot
  present.
- The worker logs `game_exit` and stops frame generation with reason
  `game_exit`. It does not mark Reflex or the worker as failed.
- Unit test `reflex_frame_policy_tests`: `presented()` through a frame, the
  skipped-Present exit, and the close.

**Confirmed in game-72 (v0.19.0, which carries this change):** the log ends
with `game_exit` after 24,175 frames, `exit_gate` true, no fault.

## Depth missing on the title and loading screens (v0.19.1)

In every DLAA session DLSS skipped about 1,100–2,700 frames for missing
depth, mostly on the title and loading screens. Gameplay was nearly clean
(game-70, last two windows: 1 and 105).

**The trace (game-69 and game-70 `depth_capture`).**
- **Stage-5 outcomes per window** (the copy hook):

  | Code | Meaning | Gameplay | Title and loading |
  |---|---|---|---|
  | 0 | leased | one a frame | fewer than one a frame |
  | 8 | mismatch | two a frame | 4,841 (game-70, first window) |
  | 2 | lease already held | about 0 | up to 2,998 a window |
  | 4 | no request for this stamp | about 0 | up to 6,460 a window |

- **A missing frame, in detail** (frame 4182):
  - three depth copies were produced: 9505, 9506 and 9507. All have the same
    stamp, source and destination and the full 2560x1440 rectangle;
  - the TAA request chose 9507, the newest matching copy (stage 2);
  - 9505's copy failed the copy checks (8);
  - 9506's copy matched by stamp and took the lease, on generation 48233 of
    its list;
  - 9507's copy would have been skipped (lease held; seen directly in the
    next sample, copy 9515);
  - the evaluation expected the lease on its own recording, generation
    48234. It found the lease elsewhere, so DLSS skipped the frame
    (depth missing).
- **Why gameplay works:** the two extra copies a frame differ in their
  resources and fail the copy checks, so the stamp alone already picked the
  request's copy.

**v0.19.1.** `depth_copy_belongs` (`depth_lease.hpp`) is the lease rule. A copy
belongs to a request only when:
- the stamp, container and destination match, as before;
- and, in the scaled build, its sequence is the request's `depth_sequence`,
  the copy the request chose.

Earlier identical copies no longer take the lease, so the request's own copy
is leased on its own recording. The unscaled build has no chosen copy and
keeps the stamp rule. Unit test `depth_lease`: the title pattern (three
identical copies), gameplay, each stamp field, destination, retired and
forwarded requests, and the unscaled rule. The in-game check is game-74
(DLAA).

## game-74 (v0.19.1): fewer misses, and the real cause

- **Depth missing over the session: 748**, against 1,100–2,700 before.
  - The first window (the title) had none: 596 frames, all leased.
  - The other windows (menus and some gameplay) had 100–300 each.
- **The remaining misses have no lease at all.** In the first missing frame
  (7351):
  - copies 20830–20832 were produced, and the request chose 20832;
  - before the evaluation, only 20830's consumer ran, on list 2014888304, and
    it failed the copy checks (code 8);
  - the evaluation ran on list 2013933520.
- **Correction to the v0.19.1 diagnosis.** Re-read with the list identities,
  game-70's misses had the same cause:
  - 9506's lease was on list 1991235504, the evaluation on 1991241440;
  - the request's own copy 9515 was recorded on 1991235504 as well, not on
    the evaluation's list.
  - So the cause is a **depth copy recorded on another command list than
    the TAA dispatch**, not the choice of copy. The lease design needs both
    on one list, because the private depth copy rides on the native copy's
    list.
  - v0.19.1's rule is still the right one (the request's own copy), and it
    stays.
- **The proper fix: copy at the TAA dispatch, on its own list.** This is what
  the scaled presets do (`DepthConsumerAdmission` with
  `record_consumer_depth`):
  - the game orders its own copy before its own TAA reads that texture;
  - so a copy made at the dispatch is correct whichever list the native copy
    used, provided the depth's D3D state there is proven from the native
    schedule;
  - that proof exists for the scaled nested draw (writer 12 → 0x100, then a
    read state), but not for the 100% plain copy (opcode 10, whole-copy
    path).
- **v0.19.2 (diagnostics only):** `IdentityDepthSurvey`. At render scale 1.0,
  on the first 8 evaluations and on 8 missing frames, it records:
  - the writer's copy path and kind;
  - the depth tracker's spans (packet role, last use, native before and
    after states, first-use flag);
  - the writer's and the TAA dispatch's schedule positions;
  - every depth state node between them, nested child draws included.

  The result is logged as `identity_depth` in `dlss_summary`. The proof
  will be written from that data.

## game-75 (v0.19.2): the 100% depth schedule

The game skipped 1,585 frames for missing depth in 8,249 identity
evaluations. All 16 surveys completed (reason 0), and the first 8 match the
8 missing ones exactly:
- **Writer:** copy path 1 (whole copy), kind 8.
- **Depth tracker spans** (role, last use, native before → after), for
  example:

  | Role | Last use | Before → after |
  |---|---|---|
  | other | 199 | 8 → 2048 |
  | other | 1269 | 2048 → 8 |
  | other | 1271 | 8 → 2048 |
  | other | 1333 | 2048 → 12 |
  | **writer** | **1348** | **12 → 2048** |
  | other | 1422 | 2048 → 8 |

  The writer's span is the outer writer packet's own, and its last use is
  the writer's index.
- **Between the writer (1348) and the TAA dispatch (1417),** exactly one depth
  node: 2048 → 8 at 1397. That is the next span's transition, hoisted ahead
  of its start (0x229eb22).
- **The writer's own 12 → 2048 transition** is hoisted before the writer
  (outside the interval).
- **Native 2048 converts to COPY_DEST** (converter 0x259ba50: values from
  0x101 other than 0x200 and 0x400). So at the TAA dispatch the scene depth
  is in PIXEL_SHADER_RESOURCE.

## v0.19.3: whole-copy admission at the TAA dispatch

- **`DepthConsumerAdmission` gains a whole-copy mode,** used when render ==
  output. Its two snapshots must agree, and each requires:
  - **Entry:** the full rectangle, and the pinned executor's whole-copy branch
    (kind 8, same size, the full rectangle, no child draw).
  - **Spans:** the writer's span is the writer's own and ends in 2048; the
    next span goes from 2048 to a shader-read state.
  - **Transitions:** exactly that next transition executes between writer and
    consumer, after the writer's last use and no later than the span start. A
    writer transition on the writer itself must be the writer span's own.
    Anything else rejects, including no read transition before the consumer
    (COPY_DEST at the TAA).
  - **Everything else** is as in the scaled mode: schedule positions,
    following spans, interval nodes, rereads, and the TAA list's API
    barriers.
- **The runtime:**
  - the scaled build no longer leases at the native copy at any scale;
  - depth is copied by `record_consumer_depth` on the TAA's recording, from
    the proven state;
  - qualified depth means admitted depth.

  So which list the native copy used no longer matters.
- **Tests (`depth_consumer`):**
  - hoisted pixel, combined and compute reads, a read at the consumer, the
    writer transition on the writer;
  - negatives, each failing on its intended rule:
    - read only after the consumer, or at the writer (6);
    - scaled mode (1), sub-rectangle (1), region path (2);
    - writer not COPY_DEST, next not a read, or the span on the child (5);
    - an extra or foreign node (6);
  - a scaled writer in whole mode (1);
  - all earlier scaled cases unchanged.

## game-76 (v0.19.3): the proof was too strict (frame generation off)

- **The run:** every identity evaluation (1,647) was rejected with reason 6,
  detail 43, state 1024. So DLSS never evaluated, and frame generation had no
  ready frame for the whole session. Fail-closed held: the game rendered
  natively, with no crash.
- **The schedule this time:** writer 1079 and TAA dispatch 1139. The next
  span, 2048 → 8, starts at 1142, and there is no depth node between writer
  and consumer. Unlike game-75, the planner did not hoist the read
  transition ahead of the TAA. So the depth is in COPY_DEST there.
- **The error:** v0.19.3 required a read state at the consumer, reasoning
  that the TAA reads the depth. The native schedule says otherwise for these
  frames: the first read starts after the TAA dispatch. The proof must follow
  the schedule, as the scaled mode does when no read transition has run yet
  (state RENDER_TARGET).
- **v0.19.4:**
  - whole-copy mode accepts no executed read transition, giving state
    COPY_DEST;
  - a span that has started at or before the consumer without its
    transition still rejects (6/43);
  - `depth_copy_state` admits COPY_DEST, and `record_consumer_depth`
    transitions COPY_DEST → COPY_SOURCE → COPY_DEST around the private copy;
    the game's own later 2048 → read transition then runs unchanged.
- **Tests:**
  - `depth_consumer`: the read after the consumer gives 1024; a read span
    started before the consumer without its transition gives 0.
  - `depth_consumer_gpu`: six writer/state kinds, two of them whole copy
    (COPY_DEST, and COPY_DEST then a hoisted read); 96 frames and 786,432
    exact pixels on the RTX and WARP, with GPU validation clean.

## game-77 (v0.19.4): DLAA depth fixed

| | |
|---|---|
| DLSS frames | 8,688 |
| Frames skipped | 1, for missing depth (one reason-5 rejection out of 8,689 attempts) |
| Before, per session | 1,100–2,700 missing |
| Whole-copy admissions | 8,688 accepted, all in the COPY_DEST state (no hoisted read in this session) |
| Exceptions | 0 |

- Title screen, menus and gameplay were all covered.
- Frame generation was on the whole session.
- The session ended with `game_exit`.
- Tagged v0.19.4.

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
