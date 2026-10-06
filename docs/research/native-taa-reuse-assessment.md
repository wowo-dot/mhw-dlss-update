# Reuse native TAA preparation, replace its resolve

2026-09-27. Offline assessment requested after the independent AA route's
sauna/haze failures. **Recommend retaining the native temporal stage and replacing
its main AA dispatch.** Independence from the player's AA selection does not
require an independently reconstructed post-processing queue. This is a proposed
integration, not a newly implemented or live-verified replacement.

No runtime source, DLL, game option or deployment changed during this assessment.
Game remains closed and the previous diagnostic DLL remains restored. The
independent v0.8.3 capture and earlier archives are unchanged.

## Why this reduces the problem

The current independent route inserts an owned order-2 pass into the native
phase-15 queue. It writes into the assigned scene-color target in place and checks
the surrounding queue graph. Haze exposed an extra node, then a recording whose
depth state the observer could not establish. The latter is a tracking failure;
it does not establish an invalid depth resource or a vendor SDK failure. See the
[haze map](independent-aa-haze-queue.md) and
[exact boundary failure](independent-aa-boundary-result.md).

Native TAA already participates in that queue at phase 15/order 2. Its own code
prepares bindings, chooses its three-buffer output ring and hands the selected
result to the assigned downstream destination. Using that stage removes the
need for our replacement to predict every surrounding native node and endpoint.
It does not remove resource-state, lifetime or frame-alignment requirements.

Proposed flow:

```text
Native preparation and scheduling
  -> original scene color + validated scene depth + packed motion
  -> our conversion and DLAA, replacing the main native TAA dispatch
  -> native TAA output texture
  -> native output routing and later effects
```

DLAA keeps its own SDK history. We reuse the engine integration around temporal
AA, not the game's temporal blending algorithm. Existing motion conversion,
scoped jitter, SDK integration, lifetime tracking and diagnostics remain useful.

## Agreed menu behavior

Timing clarified by the user: this player-facing integration is for much later,
after the user's CMFM (Configuration Menu For Mods) framework. Do not build CMFM
or make popup/menu-hook mapping a dependency of the present temporal/bypass
diagnostic. Existing diagnostic controls suffice for that work.

User clarification after this assessment: enabling our DLAA/DLSS should show the
existing DLSS-style information popup and make the native AA controls unavailable.
The player should not have to select TAA first or manually disable competing AA.
Internal use of TAA preparation must not present native TAA as an active filter.

Prefer reusing the game's popup and AA-control lockout where their responsibilities
can be isolated. Earlier run 20164 observed the native information popup; it did
not identify its function or prove that the same handler can be called independently
of the legacy DLSS renderer. That UI handler and its side effects still need mapping.
The screenshot's original wording also mentions image-quality settings; determine
which controls each new mode owns before reusing that text unchanged.

Intentionally switching our mode off should release control of the native AA
selection and restore the retained preference. This is distinct from an actual
DLAA failure, which retains the agreed latched, unfiltered behavior without silently
enabling TAA. Menu selection/lockout and internal renderer preparation need separate
ownership even if the existing DLSS menu entry is reused. The legacy DLSS rendering
path must remain inactive; the menu label alone does not establish backend state.
This records the intended behavior only; no UI or runtime change is implemented.

## Confirmed replacement boundary

The preserved `src/continuous_dlaa/runtime.cpp` v1.18.4 already snapshots raw color
and motion **before** the native TAA dispatch. It obtains depth from the validated
native depth-copy route, not the TAA shader's sometimes-1x1 fallback depth binding.
It requires that depth capture and evaluation belong to the same DIRECT command
list Reset generation and originating frame.

Currently `dispatch_call` then forwards native TAA once, evaluates DLAA and
overwrites the selected TAA output. Thus DLAA's color input does not depend on
the native TAA result. The specific dispatch can be replaced while preserving
the enclosing native compute consumer's binding, barrier and bookkeeping work.
Skipping the entire TAA Execute or compute-consumer function would lose that work.

| Boundary | Verified location / contract |
| --- | --- |
| Native TAA stage | Execute RVA `0x2392150`; registers phase 15/order 2 at `0x23921f1` |
| Main resolve request | Producer `0x23c9980`, selected return address `0x2392f31` |
| Actual dispatch | Consumer `0x259cc20`, D3D Dispatch return address `0x259cfca` |
| Raw scene color | Layer `+0x150`, input role 0; copied before native AA |
| Packed motion | View G-buffer container `+0x620`, slot `+0x78`, input role 5 |
| Scene depth | Separate view `+0x628` / container `+0xa0` owner and validated copy |
| Main output | Layer `+0x168 + 8*(frame%3)`, input role 4, full-size R11G11B10 UAV |
| Final native routing | Helper `0x23917a0`, called at `0x23931ae` with the selected ring owner |

These are version-specific research locations, not portable signatures. Existing
producer wrappers preserve Windows x64 arguments/returns and capture the layer
from RBX at the admitted call site. D3D Dispatch uses RCX=list and EDX/R8D/R9D=XYZ.
Any replacement must retain resources through GPU completion and list Reset,
fully write the selected output, restore its UAV state and restore all bindings.

The captured main shader (technique 23086, 15,492 bytes, 16x16 groups) has six SRVs,
one output UAV and one typed UAV store instruction. No other external write is
declared in that captured variant. This supports the boundary for that exact
shader; it does not authorize replacing arbitrary compute dispatches or unreviewed
TAA variants. Shader SHA-256:
`2d7b26c742c27db2c83546c6cf6a1dfda47d7cfd4336e69f8c2302de5802b0d1`.

## Work still required

1. **Activate preparation independently of the saved AA choice.** The small
   renderer choice setter at `0x2287a00` writes separate TAA/FXAA table bytes
   `+0xe9e4/+0xe9e5`; it performs no save I/O itself. The apply function copies
   them into layer flags. However, TAA Execute also reads the table directly and
   Reset can rewrite the layer enable bit. A one-time layer-bit change is
   insufficient. Establish serialized ownership before projection and restore
   exact state on mode changes, without rewriting the user's saved selection.
   The safe mutation boundary and all UI/settings interactions are not yet proven.
2. **Give jitter and auxiliary work explicit owners.** TAA Update can set camera
   jitter when layer `+0x184` is enabled. The constructor enables it by default;
   the existing scoped jitter path expects zero before its exchange. Verify their
   ordering and prevent duplicate application. Preserve ring allocation and the
   reset initialization pass while replacing the main temporal resolve. Native
   DLSS rendering remains inactive: the dense-motion helper's wrapper guard then
   returns without recording that optional work. Future reuse of the DLSS menu
   must preserve this backend condition. Suppress FXAA while our AA mode owns the frame.
3. **Handle sharpening and every skipped invocation correctly.** Layer `+0x187`
   controls a separate sharpening pass. It reads the current ring entry and writes
   the next; final routing selects that next entry only when the branch runs.
   Simply dropping its Dispatch would route an unwritten image. Disable the
   branch with verified lifetime/ownership, or provide the correct identity
   output. Keep sharpening separate from the temporal-AA replacement decision.

The one-hour v1.18.4 soak had 296,528 successful DLAA evaluations and zero faults,
but also **13,189 native fallback skips**, and native TAA always executed. It is
strong evidence for this integration's resource ownership and normal operation;
it does **not** prove exclusive AA or continuous replacement across every frame.
Some skips can arise at different stages of one frame, so this count is not an
exact inventory of missing DLAA frames. See the
[soak result](continuous-output-retirement-result.md).

Carry forward the agreed selection policy: transient missing inputs suspend
reconstruction and reset history on resumption; actual failure latches DLAA off
to unfiltered rendering until explicit retry. Do not silently run native TAA.
Eligibility and unfiltered output must be established before committing to a
replacement. A late copy cannot remove jitter already used to rasterize a frame,
so admission, partial SDK failure and restoration need renderer-level handling.
The never-deployed `superseded-bypass` draft uses a different fallback policy and
must not be restored wholesale.

## Verification and next implementation

The existing independent-AA foundation has already tested DLAA without a native
pre-pass, full-output writes, binding restoration, qualified unfiltered recovery
and partial recording failures on owned GPU resources. Its continuous fixture
completed 600 evaluations with two queues and 60 output allocations, zero native
resolves and zero GPU validation warnings/errors. Those fixtures are reusable;
they do not prove the proposed live activation and suppression logic. See the
[foundation](independent-aa-foundation.md).

Next implementation should combine that exclusive routing/failure policy with
the preserved native-stage capture and output path. Count native resolves,
replacement evaluations, suspended frames and reasons explicitly. Verify mode
ownership, sharpening output selection and failure restoration offline before
one focused live sequence covering still/pan, sauna entry/exit, area loading and
focus changes. Saved AA-choice independence also needs an explicit check. This
reduces repeated manual queue discovery; it cannot establish universal game
coverage or predict FPS from source inspection alone.

Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
decoded comparison SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Both were rehashed before this mapping. Twelve complete native spans and their
hashes are recorded under ignored `local/native-taa-reuse-20260927/complete/`.
Parent-directory exports are exploratory scratch and are not the authoritative
complete-span receipt. Proprietary instructions/shader bytes stay local.
