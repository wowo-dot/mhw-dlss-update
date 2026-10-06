# Nine live DLAA outputs; supplemental depth trace rejected

Run 50324 on 2026-09-25 used the frozen v1.15.1 recorder and produced nine
private 2560x1440 DLAA outputs for consecutive frames 22695–22703. The full
frozen reviewer rejected the run with `depth marker index`. This rejection is
preserved; successful individual checks do not replace full acceptance.

Evidence: `local/dlaa-sequence-live-20260925/v1.15.1/`. Host v0.0.0.0 SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
diagnostic DLL SHA-256
`07e8170f5fa54fda226fa4f0cd1b2bda254dbd17a21d466628c552fb9b4802b9`.
Capture SHA-256:
`36fb950d4fa6f4a15d447f182d040814339556f889f51f768c03477b2bb010ba`.

## Confirmed results

The native recorder stopped with zero faults and pending callbacks, all eleven
hooks disabled, all seven injected jitter values restored, and all nine native
consumers and GPU completions recorded. Two packet addresses were reused after
their prior consumers completed; the independent generation-chain reviewer
accepted both. The previous v1.15.0 packet-lifetime rejection is resolved in
this live sequence.

`component-audit-50324.json` runs ten unchanged frozen component reviewers
individually against the original capture and artifacts. Shader, consumer,
submission, motion, color, projection jitter, scene depth, selected depth pixels,
owned input conversion and SDK checks all pass. The conversion comparison
checks 232,243,200 float values exactly; SDK output checking covers 99,532,800
RGB values. All nine selected depth readbacks have valid submission markers,
matched frames, original resource states and completion witnesses.

Startup ownership passes. One feature is created, nine evaluations complete,
and feature/parameters are released. Reset is true only on the first frame;
the seven interior frames use their measured jitter and the final recovery
frame uses zero jitter. There is no second initialization or borrowed-device
shutdown. Timing remains the explicit fixed 16.666666 ms diagnostic policy.

## Why the full reviewer rejected

Depth consumers 40 and 41 refer to requests 41 and 40 respectively, both from
frame/epoch 22706, after the last selected frame 22703. Both native copies
returned, but their marker index is `UINT_MAX`. Neither was selected for pixel
readback: their pixel receipt is also `UINT_MAX` and pixel begin/end are zero.
The frozen reviewer requires a valid marker for every recorded consumer and
correctly refuses to invent their command submission history.

The recorder's shutdown order is a concrete race candidate: `probe::cancel`
stops depth admission and submission admission together before draining depth
callbacks. A depth callback admitted before that point can request its marker
after submission admission closes. `native_copy_begin` returns `UINT_MAX` in
that case; the depth consumer still reports its native-copy result. Other
unqualified-list paths can return the same value, so the trace alone does not
prove which branch executed. Reproduce the cancellation interleaving offline
and retain explicit shutdown/missing-marker diagnostics in the correction.

The user reported slight camera drift and a context compaction. Drift limits
static-image comparisons; it does not establish a cause for missing recorder
markers. Recording and cleanup execute in the DLL independently of the chat.

## Scope and next step

Preserve the rejected full review and the separate successful component audit.
Correct/reproduce the observer shutdown problem before claiming a fully
verified longer run. There is no on-screen DLAA replacement or sustained
temporal-quality claim: native TAA still supplies the displayed image. Actual
frame timing, production jitter/reset handling and controlled presentation
remain unfinished.

The user confirmed normal exit. The exact previous diagnostic DLL was restored
at 23:33:49 +03, the output sidecar was removed, and all four configuration
hashes remained unchanged. No next diagnostic is installed. The sealed archive
contains 411 files, SHA-256
`ff8afc8e3b0f5ee869d5dad581f34f6d777b278c6343ea3a05181eed569c2d8a`.
