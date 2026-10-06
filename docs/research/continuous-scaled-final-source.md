# Continuous DLSS: capture final source before shader-binding reuse

## v1.25.0 live attempt

PID 37120, creation 134351180096861258, 2026-09-29. Startup capabilities
passed. Native scale and temporal preparation ran, but no output plan was
admitted and no SDK evaluation occurred. Every recorded plan rejection was
reason 6, the compound filter/resource-chain check. This is a rejected test,
not a DLSS success. The trace does not identify which subexpression failed.

The explicit stop completed cleanly: 2,801 GPU signals completed, all 2,802
jitter changes restored, full render scale restored and all hooks disabled.
The user confirmed the game was closed. Configurations and installed DLL
identity were unchanged. The v1.25.0 archive contains 521 files; manifest SHA:
`7c8104385f5f7313d0a3375ea89cb85f72736297f33d39fb92cfbec0de71eb9e`.

## Confirmed timing defect

The new continuous plan reread `context+0x710+8*(tBaseMap_id&0xfff)` after
motion-filter queue preparation and required it to equal color grading's
output. That slot is mutable shader-binding state, not a permanent final
source identifier. The accepted one-frame recorder instead saved the source
when the final quad was enqueued at RVA 0x2297e20, caller 0x23c7b08, before
later filter preparation.

Offline instructions from the pinned decrypted executable establish that
RVA 0x23c7890 binds the final source to the parameter named `tBaseMap`.
The motion draw preparation at 0x24ca675 binds its own input to that same
parameter name. The helper at 0x23d0130 writes the corresponding context slot.
The parameter names resolve to the same string RVA 0x3337e70. The original
one-frame captures confirm distinct owners for motion input and final source.
Thus the continuous check assumes stability the renderer does not provide.
This is a supported explanation of the live compound rejection; its exact
subexpression was not captured in v1.25.0.

## v1.25.1 correction

The stream route now saves the final quad's source, destination, frame/epoch,
view/bank/async identity and exact queued packet at that earlier boundary.
It requires one final draw with key 0x80000000 and stable owners on return.
The later plan must match that saved frame and packet, and must still connect
color grading's output to the saved source. Current owner/resource identities
are revalidated; no ownership check is removed. Missing, stale or mismatched
captures keep the frame unqualified. The slot can legitimately be rebound by
later shader preparation without changing the saved final source.

The additional hook uses the same Windows x64 register-preserving adapter as
the working bounded recorder, including its two stack arguments. Its target
is RVA 0x2297e20 (656-byte hash
`58fe604c883de6d505ce2211797c2efaddc360046d10bb14c2e50fafbae4ab6d`)
and caller region 0x23c7831 (740-byte hash
`0da63ff39cc114c78483728e19221e1dd22806398522ec0095ec6c6349801247`).
Executable identity remains the one documented in `continuous-scaled-output.md`.
The saved CPU evidence occupies a bounded 32-frame ring. It takes no COM
references; the later validated plan and existing recording retirement own
all GPU references. Hook count is now 48. Stop/removal includes this new hook.

Telemetry adds `final_captured`, `final_rejected`, and `plan_detail`. The latter
splits reason 6 into entry counts, endpoint identity, motion ownership, grade
ownership, saved final-source identity and final-destination identity (1..6).
No raw process-memory dump or new image readback is added.

## Verification and remaining test

Release DLL and fixture builds passed with `/W4 /WX`. WARP and NVIDIA each
passed 96 complete output chains, 288 native effect draws and 786,432 exact
transfer-copy pixels. The CPU graph regression captures the final quad, then
rebinds its tBaseMap slot to the motion input before admitting the plan.
Thirteen malformed graph cases are rejected, including missing/stale final
evidence, wrong final packet and changed output attachment. Six existing
copy-route rejection modes also passed. Fifty-seven Python review tests passed;
both scaled versions passed the synthetic complete-log check, which rejected
missing final capture coverage. SDK/history code was not changed; its previous
96-evaluation NVIDIA test remains applicable, not a new live claim.

The next live run must establish that source capture occurs, qualified plans
are built, and successful SDK frames reach all three full-output effects.
Keep native AA/DLSS Off and the 2560x1440 window unchanged. Do not claim the
continuous live failure resolved until those counters advance successfully.

## v1.25.1 result and v1.25.2 correction

The September 29 run (PID 48048, creation 134351194187678143) admitted no
DLSS frame. All 9,681 published plans were unqualified: reason 6/detail 5,
missing final-source proof. Its 9,681 final-quad attempts were rejected.
The native sizing and temporal paths ran, but the SDK was never evaluated.

A bounded read of the stopped recorder's 32 final-quad records established
the immediate cause. Every saved `Snapshot.async` was 257; the actual native
byte at renderer+0x93 was 1, and a four-byte read was 257. The snapshot member
is an unsigned integer. Reading directly into it accidentally incorporated
the neighboring native flags; `same_stamp` correctly read a byte and rejected
the mismatch before accepting any final quad. v1.25.2 reads a byte locally
and then widens it. The existing ownership checks remain intact.

Stop restored full scale and all 9,682 jitter changes, and completed all
9,681 GPU signals. It was **not a clean hook shutdown**: fault 25 was recorded.
The runtime nevertheless reported the activation hooks disabled. Read-only
inspection established that all eight activation entry points still contained
our hooks; the sampled resolution, main runtime and final-quad entry points
were restored. The user confirmed a normal scene and normal game exit.

The activation remover checked `pending==0` again after the initial drain and
scale restoration. A callback that merely forwards after stop can re-enter
in that interval, causing the remover to queue nothing. The parent then
unconditionally marked that group disabled when its own queued apply
succeeded. This sequence is reproduced offline; the exact pending/context
values at the live queue call were not recorded.

v1.25.2 requires stopped admission and no owned camera scope before queuing
activation removal. Forwarding callbacks may finish through the retained
trampolines in the pinned DLL. Each hook is marked removed only if its own
disable was queued and the apply succeeded. The parent applies the same
accounting and waits for entered callbacks to return before stopping the
observer. No DLL/trampoline unload is introduced.

Both regressions failed on the old code. The corrected Release build passes
43 relevant native tests, including actual hook restoration, a partial queue
failure, a forwarding callback at removal, an owned scope that must defer
removal, and an empty/failed apply that must not claim closure. WARP and NVIDIA
each pass 96 complete output chains, 288 effects and 786,432 exact transfer
pixels. The graph test has 14 negative cases, including a changed async byte,
and passes with nonzero neighboring flags. Six copy rejection modes and 57
Python review tests pass. Synthetic full reviews accept all three scaled
versions and reject missing final-capture proof in the newer versions.

The v1.25.1 archive has 555 files; manifest SHA-256:
`5507a9cf29cb3895cee6fbec4dcd24ee178801600fedb651b3129527e0c4729b`.
v1.25.2 evidence is under `local/continuous-dlss-20260929/v1.25.2`.
Continuous live DLSS remains unverified. The next run must advance capture,
plan, SDK, expanded-copy and three-effect counters; verify actual native hook
bytes after stop in addition to telemetry.
