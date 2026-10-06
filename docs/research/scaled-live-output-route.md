# Bounded live full-output route (v1.24.3)

The next game test connects the existing half-scale input recorder to one
Performance DLSS evaluation and the native output chain. It is prepared for a
2560x1440 output with a 1280x720 active render rectangle. This is not yet a
continuous DLSS implementation or a visual-quality/performance result.

Receipts 0 and 1 preserve the native TAA dispatch and output draws. Their actual
draw callbacks qualify the vertex/pixel programs, layout, root signature,
pipeline descriptors, vertices and API bindings, and build three private PSOs.
Receipt 2 substitutes the SDK/spatial resolve, expands its final CopyTextureRegion
to the full allocation, and replaces the matching motion-filter, color-grade
and final-screen draws. Native packets, descriptor tables, constant buffers and
game settings are not edited. The four temporary PSO/IA/RS bindings are restored
after each draw. The OM binding is now observed at the API as well.

## Code and ownership gates

Tested executable identity remains SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`,
version 0.0.0.0. The local decrypted image used for instruction mapping is
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
All RVAs below require the corresponding complete live-code digest before use.
They are version-specific research gates, not portable signatures.

- Existing native draw wrapper: RVA 0x259d670, x64 native packet ABI, with the
  DrawInstanced return site 0x259d72e. The existing depth DrawInstanced observer
  forwards selected calls to the output route; there is no second draw hook.
- Native texture copy wrapper: RVA 0x259c650, 1214 bytes, SHA-256
  `299ede85b4e2bbfb25f0ae2c20e003317efc2809c591eb46677ec154495daf88`.
  The new D3D12 CopyTextureRegion detour uses the Windows x64 COM ABI and only
  selects return site 0x259caf0. The matching native recording's packet pointer,
  command-list pointer and 64-byte payload are rechecked at the API call.
- Native IA resolver: RVA 0x259e470, 512 bytes, SHA-256
  `7db0a032642249004ebda64a7e8442407851cec8f25208e8966a63990ddade49`.
  IA packet +0 identifies the owner, owner +0x60 the backend; backend +0x20 is
  the committed resource, +0x40 its GPU address and +0x10 its byte count.
  The native view's offset, extent and stride must match before retaining it.
- Committed vertex-buffer constructor: RVA 0x24e6e50, 910 bytes, SHA-256
  `d961028c0e34625cd23343327e3ba3ad401c4d439a5c653a9ca0e6893836bc04`.
  Its CreateCommittedResource output and GPU-address store confirm those
  offsets. Resource QI identity, device, buffer dimensions and GPU address are
  independently checked. Suballocated buffers are not admitted by this probe.

The copy and all three output draws must use the selected temporal command list
and Reset generation. Each added operation reserves a production submission
journal marker after Dispatch returns and before Close. The same private fence
therefore covers the resolve and downstream commands, with subsequent Reset
retirement required before releasing retained resources. The accepted v1.23.12
capture used one list for all these operations; that observation is rechecked,
not assumed, in the new probe. Copy-source/destination and output RT states need
explicit native barrier observations within the selected recording.

## Failure and verification scope

Preflight rejection records no SDK commands and keeps the original dispatch.
After SDK work starts, its existing recovery transaction can overwrite the
full output with spatially enlarged current color. Native TAA is not called
again for a claimed resolve. Downstream API qualifications remain independent:
an unexpected later binding/packet/state change rejects the capture, forwards
that native operation and stops the bounded experiment. This first test does
not claim atomic rollback of an already-recorded full frame or continuous
recovery across arbitrary renderer changes.

Offline checks cover the production copy/draw route on WARP and NVIDIA,
four operation markers, exact expanded-copy pixels, restored bindings, GPU
validation and retirement. Five negative copy cases cover caller, rectangle,
packet mutation, missing state and recording-generation mismatches. The GPU
route fixture substitutes the game ABI entry and journal adapter; the real
journal follow-up exclusion is tested separately in `jowned_inline`.
The three-effect pixel fixtures and real SDK success/spatial/recovery fixtures
remain part of the verification. Synthetic effect inputs do not establish
live image quality. Evidence is kept under `local/scaled-route-20260928`.

The live reviewer requires exactly one successful SDK evaluation, no second
native TAA dispatch for that frame, all three replacement draws inside the
same completed recording, render-sized input comparisons, a complete packed
DLSS output and a matching startup capability log. CPU snapshots by themselves
cannot accept the new experiment.

## v1.24.0 setup result and v1.24.1 correction

The v1.24.0 run (PID 648, creation 134350973654461230) passed startup
capabilities and created the Performance SDK feature, but stopped before any
input receipt or SDK evaluation. `bindings-create-21-3` identifies a second
attempt to hook RSSetViewports: the resolution controller already owned it.
MinHook returned MH_ERROR_ALREADY_CREATED. This was a setup ownership failure,
not an observed DLSS rendering failure. Full scale returned, all hooks were
disabled, and the user reported a normal scene and normal exit. Evidence is
archived under `local/scaled-temporal-20260928/v1.24.0`.

In v1.24.1 the binding observer solely owns the RSSetViewports COM detour
(vtable slot 21, Windows x64 calling convention). After a successful native
call it forwards a read-only observation, with the original arguments and
return address, to the resolution controller. The listener runs inside the
observer's admitted callback interval and never calls the API itself. The
observer stops admission and drains callbacks before removal; listener code
and resolution state remain alive throughout. The resolution controller owns
only its native scale-update hook in this build. Standalone resolution probes
retain their previous two-hook implementation. Total armed budget is 49.
Executable identities and native code gates above remain unchanged.

The correction checks listener admission, original arguments, stop/resume and
removal on real WARP and NVIDIA command lists. The debug layer forwards through
another list object that can share the hook implementation; fixture counts are
therefore checked against the explicit application command-list identity.
This distinction affects observation counts, not native forwarding. Production
route checks require one notification for each native, expanded and restored
viewport on that list. Pixel comparisons and GPU validation still apply.
Correction evidence is under `local/scaled-viewport-20260928`; the subsequent
v1.24.1 game run is described below.

## v1.24.1 setup result and v1.24.2 correction

The v1.24.1 run (PID 19212, creation 134350984651080025) passed the viewport
hook installation and startup initialization, then failed `downstream-code`
before input capture or SDK evaluation. The observer's MH_ApplyQueued call
also enabled the eight AA activation hooks that the enclosing installer had
deliberately queued for later. The downstream code gate includes their native
queue target at RVA 0x24f1fc0. The activation telemetry recorded 209 frames
before the recorder's own admission began, confirming premature activation.
After shutdown, a bounded read of all three downstream code ranges matched
their original digests; live render-scale fields were back to 1.0. All hooks
were disabled and no input receipts or SDK evaluations were produced.
Evidence is under `local/scaled-temporal-20260928/v1.24.1`.

v1.24.2 enables and disables each observer-owned target directly, and never
applies the shared MinHook queue from the observer. Admission begins only after
all those hooks are installed, so installing them individually does not admit
partial binding observations. Cleanup tracks each target's actual disable
result and retains the existing callback drain. The outer scaled installer
still owns the final activation apply after original-code validation. No code
gate was removed or weakened; the viewport ownership and 49-hook budget remain.

A regression fixture queues an enable and a disable on two private executable
stubs before observer installation. Both must remain unapplied across setup and
cleanup, then take effect when their owner explicitly applies the queue. It
reproduced the v1.24.1 failure offline before the fix. The corrected fixture
also runs the existing GPU effects and listener lifecycle checks. Evidence is
under `local/scaled-setup-20260928`. A successful live DLSS output still requires
a separate accepted capture; neither failed setup run establishes that result.

## v1.24.2 partial capture and v1.24.3 vertex correction

The v1.24.2 run (PID 41480, creation 134350990791981848) passed installation
and captured two input receipts and two native draw snapshots. The first
motion-filter effect prepared successfully. The color-grade effect constructor
rejected `effect native vertices`; no SDK evaluation, expanded copy or output
draw replacement occurred. Full scale returned, all hooks were disabled, and
the one jitter write was restored. Some private references were intentionally
retained until exit after this partial capture. The user confirmed a normal
scene and normal exit. Evidence is under `local/scaled-temporal-20260928/v1.24.2`.

The native color-grade strip orders its corners top-left, top-right,
bottom-left, bottom-right. The motion-filter strip instead orders top-left,
bottom-left, top-right, bottom-right. The implementation and synthetic fixture
had incorrectly used the latter order for both stages. The old v1.23.12 capture
already contains the same color-grade ordering as the new capture; this is a
mistaken implementation assumption, not an observed change in game behavior.

v1.24.3 preserves each stage's own vertex order and changes only the color-grade
UV extent when producing full-size output. The exact input-vertex validation
remains; it now rejects the previously assumed swapped order. The standalone
geometry test also accepts an external list of captured vertex arrays. Twelve
arrays from the two hash-checked game logs cover all three stages; that check
failed before the correction. GPU comparisons include the correctly ordered
native draw, full output, restored bindings and the complete effects chain.
Evidence is under `local/scaled-vertices-20260928`. Full live output remains
unverified until the bounded game capture passes its complete review.

## v1.24.3 partial capture and v1.24.4 topology correction

The v1.24.3 run (PID 12360, creation 134351004950496566) captured three
input receipts and all nine native draws. SDK creation and one Performance
Evaluate returned success; an expanded copy and the first two full-size
output effects were recorded. The final-screen effect rejected its topology
before emitting commands, so that native draw was forwarded. The selected
recording did not reach verified GPU completion or retirement in the trace.
This is not an accepted displayed DLSS result. Scale and jitter were restored,
all hooks disabled, and unretired private resources retained until exit. The
user reported a normal scene and normal exit. Evidence is under
`local/scaled-temporal-20260928/v1.24.3`.

All nine draws have graphics flags 16. The hash-checked native setup at
RVA 0x259e670 (2554 bytes, SHA-256
088e67c77b7e96af64c83c8297f6fc9f6d355dbc170530dda125b41f4cb53727)
selects table index `(flags >> 2) & 255`. A bounded read-only live inspection
verified RVA 0x354e610 contains `[1,2,3,4,5,10,11,12]` (SHA-256
e874703e0444b901a448bb64fb01e709680edbc3f44b965fd801cbc1959fbfe8).
Index 4 maps to D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, including the final
three-vertex screen triangle. The earlier v1.23.12 capture has the same flags.
The implementation had instead required TRIANGLELIST for that stage.

v1.24.4 preserves the native strip topology without weakening exact admission
or changing any hook, shader, vertex order or ownership. The external geometry
regression now checks topology as well as every ordered position/UV against
18 draw samples from both hash-checked captures. It failed before the fix.
The GPU fixture also rejects the old triangle-list assumption before issuing
commands. Evidence is under `local/scaled-topology-20260928`; full live output
still needs an accepted bounded capture with completion and retirement.

## v1.24.4 completed recording and v1.24.5 copy-admission correction

The v1.24.4 run (PID 16048, creation 134351014225002367) recorded one
successful SDK evaluation, the expanded native copy and all three full-size
effects. Independent submission replay verified their order before Close,
queue submission, private fence completion and command-list retirement. The
binding snapshots and jitter review also passed. The selected recording was
frame 15197, command 357912960, Reset generation 3124. All three input
recordings completed, and all 340 submission references were released.

The whole capture still failed. Output-route fault 4 can only be raised on
a later copy of the same source/destination pair. Because the copy guard
remained active after the three draws, its `copied`/`replaced_draws` checks
rejected normal resource reuse. Completion cleanup then refused to retire
private effects and cleared the aggregate bindings_equal flag; the actual
SDK transaction and each draw had restored their bindings. SDK output images
were not collected, so this run does not establish pixel correctness or an
accepted displayed DLSS result. Its later copy identity was not recorded:
next-generation reuse is plausible, but cannot be distinguished from a later
same-generation copy in this trace. Evidence and these limits are in
`local/scaled-temporal-20260928/v1.24.4/completion-review-16048.json`.

v1.24.5 closes copy interception under the existing route lock immediately
after its one copy and three successfully restored draws are recorded. Later
copies forward natively while the original GPU fence and recording retirement
checks still govern resource release. All original caller, packet, state,
generation and rectangle checks apply until that point. An early duplicate
copy remains a fault. No new hook, GPU operation or rendering mode is added.

The production route fixture reproduced the fault before correction. It now
issues another native copy before Close, reuses the same resources after
Reset, checks that both leave the route healthy with exactly four markers,
and verifies the later copy's original half-size rectangle against zeroed
pixels outside it. An additional negative case rejects a duplicate before
the selected effects have completed. Evidence is under
`local/scaled-copy-lifetime-20260928`. Full live image collection remains pending.

## v1.24.5 accepted live output

The next run completed its one SDK frame, expanded copy and all three output
draws, including GPU completion and private-resource retirement. Full output
images were collected and their packed native handoff verified exactly. The
shared depth reviewer required a correction to assign the four validated
output markers separately from depth markers; no runtime change was needed.
See [first-scaled-live-output.md](first-scaled-live-output.md) for identities,
verification, preserved original review, user report and coverage limits.
