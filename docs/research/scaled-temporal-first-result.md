# First half-scale temporal capture: native depth uses a draw path

2026-09-27, v1.23.0, PID 46000 / creation 134350099409052660.
The user loaded a save with native AA/DLSS Off at fixed windowed 2560x1440.
Arm tick 18945734. Installation succeeded; capture stopped on a diagnostic
qualification failure. This is a partial result, not a successful image capture
or DLSS evaluation. The user confirmed a normal scene and normal exit.

## Confirmed observations

One TAA input request at frame 12663 has matching before/after snapshots and
scale stamps: active 0.5, content 1, display 2560x1440, render 1280x720. Its
rectangle is [0,0,1280,720], dispatch [80,45,1]. Actual GPU descriptions for
color, history, output, packed motion and auxiliary G-buffers remain 2560x1440.
The ordinary tDepthMap is a 1x1 fallback; separately selected scene depth is
R32_FLOAT in a full-sized allocation.

Three native depth requests in that frame carry kind 8 and rectangle
[0,0,1280,720,0,0], sharing source/destination. The first consumer returned
normally but made zero observed CopyResource calls. Its recorded barriers
change source DEPTH_WRITE (16) to shader-resource state (192) and destination
shader-resource state (128) to RENDER_TARGET (4). The recorder requires exactly
one qualified CopyResource and rejects this consumer.

This propagates as depth-route fault 9, submission fault 18 (the marker receives
the consumer's invalid result as its returned flag), native-consumer fault 15,
and top-level capture fault 13. The native function did return normally; the
journal's returned=false must not be mistaken for a game exception. The original
three-frame reviewer correctly rejected the run. No diagnostic image copy was
attempted, no private GPU signal issued, and no image artifact was produced.

## Explanation from pinned native code

Consumer RVA 0x259df30, 681 bytes, SHA-256
`e84ae8742c4a0ef334692595bb89c04c8be80306d4416c20304566cc1e5dd85a`
checks kind 8, matching allocation dimensions and a full-allocation rectangle
before its direct copy branch. A smaller rectangle branches to 0x259e1aa,
which calls 0x259d670 with the nested work item. That helper binds graphics
state, flushes barriers and calls DrawInstanced (vtable +0x60) at 0x259d72b.
The reduced rectangle and render-target barriers are consistent with this
native draw route. Draw arguments, shader and completion were not independently
recorded in this run, so those remain the next observation target.

No need to change the game's depth generation: extend the capture to recognize
its existing draw path and qualify a depth readback after that draw, retaining
matching packet/frame/resource/state and GPU retirement checks. Keep the
working full-scale CopyResource case intact. This is required for scaled inputs,
not the parked intermittent DLAA depth-copy polish.

## Cleanup and retained evidence

Native scale reports 333 overrides, zero faults, restored=true and hooks disabled.
All nine scale values returned to 1. Capture, native compute, depth-route,
submission and activation hook records report disabled and pending zero.
All references released: producer 8/8, consumer 1/1, depth 6/6, journal 214/214.
Read-only live rechecks independently matched all 26 original code spans and
all nine full-scale values before exit.

The combined cleanup boolean activation_disabled=false contradicts the later
activation hooks_disabled=true, pending=0 and active_context=0 snapshot. Its code
samples drained() immediately after disabling hooks; a callback already routed
through a trampoline may still be returning. This is a likely cleanup-reporting
race, not proven by one snapshot. Fix it by separately recording successful
unhooking and waiting for post-disable callbacks to drain; do not weaken cleanup
checks or rewrite the original failed result.

Evidence root: `local/scaled-temporal-20260927/v1.23.0/`. Final log SHA-256
`0e12f111b06692f974c4ea2fd64e5d441d66159fb8fcace145dfc0e7956eddae`.
Includes failed review, read-only cleanup receipt, pinned consumer/draw
disassembly, deployment/source checks and final archive. Saved settings remain
unchanged and the same DLL remains installed. No test is armed.
