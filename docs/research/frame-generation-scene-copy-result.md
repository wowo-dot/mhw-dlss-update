# FG scene-copy scope: v0.6.3 runtime result

2026-10-05. The four requested CPU metadata samples passed review. They identify
the outgoing native scene copy and the dominant missing draw-observer scope.
This is an accepted **partial submitted copy map**; MHW frame generation is
still disabled. The user reported clean DLSS after save loading and at the last
checkpoint, then confirmed a normal exit with a normal scene. The last reply
did not explicitly confirm performing the requested area change.

## Confirmed copy observations

Each sample contains one exact returned, replaced, API/native-qualified final
DrawInstanced call (three vertices, one instance). Its immutable plan, recording
generation and same-queue submission receipt link it to qualified Reflex
PresentStart. A returned, unreplaced native CopyTextureRegion then copies from
that final target to the same intermediate resource. Both locations name
subresource zero; destination XYZ is zero and the source box is
`[0,0,0,2560,1440,1]`. This specifies the native call's box, not verified resource
extents, format compatibility or complete pixel coverage.

| Requested sample | Plan counter | Recording / Present counter | Copy calls | Unreadable entries | Outside executor scope |
|---|---:|---:|---:|---:|---:|
| Main menu | 47260 | 47261 | 1 | 38 | 0 |
| Loaded save | 51558 | 51559 | 1 | 188 | 4 |
| Menu open | 53484 | 53485 | 5 | 1258 | 440 |
| After area-load request | 56085 | 56086 | 1 | 49 | 35 |

The final target label was `2508982288`; the outgoing destination was
`2492960416`. Neither matched the run's sampled GetBuffer return interfaces.
Those historical samples do not prove the current indexed buffer or lifetime.
The target's engine extent was 2560x1440 and its engine format field was
UINT32_MAX; its DXGI format remains unverified.

In the menu sample, four incoming copies from another resource used 1280x720
source boxes. All execute before the final draw: three are on batch list index
0, one on index 1, and the final draw/outgoing copy on index 2. One incoming
copy's CPU callback occurred after the outgoing copy's callback. Submission
ordering prevents misclassifying it as post-final HUD work. No cross-queue
ordering, full draw inventory, HUDless pixels or transparent UI was established.

## Missing observer scope and hypotheses

Rejection reason 9 accounts for 37, 152, 357 and 47 observations across the four
samples. Its condition combines the observer's complete raster snapshot with
known single-RTV output; it does not distinguish which component was absent.
Inspection of the forwarding wrapper corrected the initial argument hypothesis:
the source rejects OMSetRenderTargets observations unless there is one
descriptor and no depth descriptor pointer. Its `c` argument is the depth
pointer, not the contiguous-range flag. Actual OM arguments and individual
raster-known bits were not captured, so a depth attachment or any particular
raster field is not yet a confirmed runtime cause. This correction does not
change the accepted v0.6.3 capture or its immutable archive/source freeze.

The snapshots also preserve backing-label read failures, unsupported native
attachment modes and indexed calls outside the two known executor scopes.
Native RTV/root/PSO fields following reason 9 remain unread: their zero values
cannot prove the game used zero bindings. These are entry snapshots, not proof
of draw return or UI semantics. Event/resource capacity did not overflow.

Next record bounded successful OM arguments and raster completeness through
the existing hooks, keeping renderer admission unchanged. Use that evidence
to qualify missing composition draws and map the copied destination through
the native presentation attachment. Resource formats/pixel semantics, matching
camera/depth/motion frame tokens, immutable input ownership and producer fences
remain required work, followed by FG-chain retirement/recreation integration.

## Renderer, verification and cleanup

The run completed 8,244 DLSS Performance frames and 9,677 complete Reflex On
frames, 58,062 markers and 64 complete driver reports, with zero runtime faults
or request collisions. Three skipped resolves remain in the evidence; this
does not claim every frame was reconstructed. All 8,247 GPU signals completed;
all 8,246 submitted recordings retired and released, with zero live recordings
or resource references. All 9,267 jitter changes and full render resolution
were restored. Reflex Off, SDK shutdown and pin release were confirmed.

Independent post-stop checks verified all 37 native hook heads and 46 full
native functions restored (778 VM_READ operations / 75,163 bytes, zero process
memory writes). Routed/native presentations matched at 56,987. The user then
closed the game normally. Graphics bytes and runtime/mode sidecars were
unchanged; v0.6.3 remains installed as requested.

The /W4 /WX builds and 10 native plus 72 Python checks passed before deployment.
The exact tested source was rechecked after the run. No new hooks, resource
methods/references, GPU commands, FG tags or Streamline FG plugin were added.
The existing NGX nvngx_dlssg mapping is not MHW FG activation. This test does not
measure performance or latency improvement.

## Private evidence identities

Run: `local/frame-generation-scene-20261003/game-03`, PID 15944,
creation 134356205962141843. Accepted joined review, independent restoration,
all four checkpoints, copy facts and normal-exit result are preserved.
The 48-file `game-03-archive` manifest SHA-256 is
`1ca0fba93d4aba694ee120b7d92bf0b3bba3dc9babc4a59dfd86fc7418a2dd07`.

Exact 687-file `source-0.6.3` manifest:
`fcc0700a36c5990203005e246f83a2ca2f1ada51299ef8fc3aca45b7e6ce21c2`.
Installed probe:
`7b11a27d7dc7e7a21cdec9f977145a076a3b94fdf831dbe324b75b38779505ea`.
Retail executable version 0.0.0.0:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded research executable:
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Previous accepted owned-FG/MFG, renderer, ownership and scene archives were
rehash-verified unchanged. These local artifacts remain private and untracked.
