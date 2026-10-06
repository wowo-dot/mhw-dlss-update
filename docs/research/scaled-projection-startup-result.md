# Scaled projection startup admission — v1.23.5

## Rejected v1.23.4 run

2026-09-28, PID 17220 / creation 134350210941800810, installed DLL SHA
`97c2661871ae3e46ff3edf6d7c6192f5e68df398a4939d24cf58e5345c2fd64a`.
One unchanged projection from frame 21166 passed matrix inspection. Its viewport
was 2560x1440 while the scale recorder observed 1280x720 scene viewports. No
temporal input was paired with it: projection fault 3 (sequence/acknowledgement)
propagated as observer fault 14. No jitter write or SDK evaluation occurred.
This is not a verified scaled jitter sequence or a full DLSS result.

The three-frame reviewer correctly rejected the capture. Cleanup restored all
nine scale values to 1 after 388 overrides, disabled all hooks, drained pending
callbacks and released observer pins. User reported a normal scene and normal
exit; process exit and unchanged graphics/profile/mode files were verified.
The installed DLL was preserved when this run was archived.

Evidence: `local/scaled-temporal-20260927/v1.23.4/`. Final log SHA
`a3ed7a274b6f5e1dd2fcdf56957c0e8f0198f6869eaf66b5126bf638a0b9bfea`;
517-file archive SHA
`d9264f9b2edf5c7fd6802d731b895ff1eef026140c0dd1bd49b712f69dea189c`.

## Startup gap and correction

Code inspection found that the projection hook could begin observing before
the queued native AA activation hooks were applied. With saved AA Off, an
otherwise valid projection in that interval has no matching TAA dispatch.
The baseline was recorded before installation returned, consistent with this
startup gap. The old trace did not record activation ownership at projection
entry, so the exact first rejected predicate remains an inference.

v1.23.5 requires the projection callback to execute inside the activation
module's owned camera frame on the same thread, context and view. The check is
read-only and uses existing ownership, native primary-view and provider checks;
it adds no hook or game-memory write. Before the baseline, unowned projections
forward unchanged and increment a wait counter. Loss of ownership after the
baseline stops the capture before another jitter write. Existing exact frame,
epoch, bank, context, view and TAA acknowledgement checks remain unchanged.

The common projection recorder accepts an optional admission callback; only
the scaled probe supplies the native activation check. Other existing capture
paths keep their previous policy. New versioned telemetry requires scope
admission on every selected projection, and the reviewer rejects missing or
false admission receipts. Waiting does not extend the two-second capture limit.

Hook ABI, lifetime and executable identities remain those in
[the scaled projection contract](scaled-projection-capture.md). The existing
native camera hook at RVA 0x228e770 owns the thread-local frame until its native
body returns, including exception unwinding. The new query borrows this scope
only synchronously within the existing projection callback; it retains no game
object. All 33 original byte gates are rechecked during preparation.

## Verification and next test

Warning-clean /W4 /WX build. 52 native tests pass, including real activation
scope checks for foreign threads/context/view, unowned startup projections,
loss of ownership, scoped restoration, and two combined GPU captures. Retained
WARP captures each skip two simulated unowned projections, then collect three
matching projection/depth/color/motion sets with one jitter write and restore.
GPU validation reports no errors; the fixture's known clear-value performance
warnings are unchanged. Five projection-review tests and ten combined review
checks pass (one inapplicable branch skipped). The prior accepted v1.23.3 live
capture still passes the updated reviewer.

Prepared evidence: `local/scaled-temporal-20260928/v1.23.5/`. The next live test
uses the same bare/offline MO2 entry, native AA and DLSS Off, windowed2560x1440,
a loaded save and still camera. Verify the exact fresh process and startup
before arming. This still measures scaled inputs/projection; it does not run
NGX or replace the displayed image with DLSS. Keep the deployed DLL installed.
