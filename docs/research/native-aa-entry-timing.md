# Native AA activation entry timing — v1.20.1

The first v1.20.0 live run did not activate DLAA. PID 58628 / creation
134349417114189662 installed the hooks, but admitted zero camera scopes,
published zero TAA requests and performed zero SDK evaluations. The assistant
stopped it after 26.922 seconds. All 2,375 jitter injections were restored,
both hook sets disabled, and no private GPU recordings remained. The user
confirmed normal exit. The baseline DLL and output sidecar were restored;
all four configurations were unchanged, including AA/DLSS Off.

The run is archived as **activation not exercised**, not successful rendering.
Evidence: `local/native-aa-ownership-20260927/v1.20.0/`.
Archive SHA-256:
`a89e847dbb3714b6f09758ff18eb024f3a40775fb26f4a68c565224e010494d7`.

## Timing error and correction

The old camera-entry predicate required `context+0x1d0` to contain the primary
view. The native caller at RVA `0x229a339..0x229a353` passes that view in R8,
the context in RDX and display index zero in R9. Inside the camera invocation,
the native setup at `0x23c3790` clears and then assigns `context+0x1d0`; the
binding is therefore a later-stage condition. Eight bounded post-stop live
samples observed that field alternating between null and the primary view.
These are asynchronous samples, not a frame-entry register capture; they
support the ordering diagnosis without establishing the arguments of a
particular call. The old owned fixture pre-populated the field and missed it.

v1.20.1 validates the incoming R8 view at entry, requires the main display,
and retains the existing renderer, DX12, camera type and backend predicates.
At unit dispatch it separately requires the actual bound view to match both
the scoped incoming view and the renderer's current primary view. Null,
different or excluded bound views retain native dispatch. Native render flags,
mask and display gates remain in effect.

The table/sharpen comparison overrides now belong only to an admitted TAA2
Execute call. The outer camera scope alone does not authorize them. They stay
active through that call's normal return, including an explicit stop, and
clear in SEH cleanup. Nested admission latches fault 5 and forwards the nested
native unit with comparison overrides temporarily suspended; the enclosing
scope is restored until its return. An exception from admitted TAA also
latches fault 5 and clears both scopes. The existing frame exception remains
fault 2. Hook locations, calling conventions, pinned native code hashes and
no-unload lifetime are unchanged from [v1.20.0](native-aa-activation.md).

All analysis uses host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`,
and the pinned decoded comparison
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Private disassembly and scalar observations stay under ignored `local/`.

## Verification and limits

Release `/W4 /WX` builds passed. All 35 focused native checks passed, with
the expanded activation fixture additionally rerun after adding secondary
display, nested-call and admitted-TAA exception coverage. Four saved AA choices
now start with null/stale context views; the simulated native body binds its
view later. Checks also cover other/null bound views, unchanged shared settings,
competing AA suppression, other-thread isolation and stop inside admitted TAA.
All 43 Python checks passed: activation 14, full review 13, startup review 16.
Two startup-test invocations used incorrect fixture arguments and ran no tests;
their error logs are preserved. The correct existing fixture passed all 16.

The qualified GPU resolve, SDK integration, request retirement and retention
are unchanged. `exclusive_mode=false`: unqualified/publication-blocked GPU
callbacks still use the diagnostic native path. Final unfiltered suspension
and latched-failure integration remain pending. No UI/configuration-menu work.
The correction is ready for live validation, not yet proven in-game.
