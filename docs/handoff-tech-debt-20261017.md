# Handoff: tech-debt pass (2026-10-17)

From the bug-hunting session (tester and player reports) to the main session.
This session keeps the reports; the main session does the tech debt.

## The goal (the user's words, paraphrased)

Pay the tech debt: review, clean up, and move our own game hooks from MinHook
to Astera Loader's `al.hooks` properly. Confirm scope and a token budget with
the user before any large multi-agent run (the 2026-10-17 review spent ~22M
tokens on one crash; not what was wanted).

## State of main (local, not pushed)

- `7af6965` presentation: no game call reaches the identity chain itself
  (tifa-6 crash fix; not yet run in game; DLL `7beafabd` in the MO2 mod
  "MHW DLSS 0.28.6 (test)").
- `5c08baf` the review report `docs/research/presentation-review-20261017.md`
  (D1-D18, P1-P11, coverage gaps; status note at the top says what is fixed).
- `b46a89c`, `79e4b1b`, `e7dd072`, `d957986`: the rest of 0.28.6 (startup
  diagnostics, MinHook vendored with AL's PSS thread adapter, silhouette).
- 0.28.6 is not tagged: waiting for the in-game test of `7af6965` and the
  silhouette check.

## MinHook in the shipped DLL (mhw_dlss_alpha), 2026-10-17

Already on AL: every DXGI/D3D12 presentation hook (`al.d3d12`,
`presentation.cpp`), the frame boundary hooks (`al.hooks`, `live_frames.cpp`),
the DLSS runtime's D3D12 API hooks.

Still MinHook (game-internal functions):

| File | What |
|---|---|
| `src/ngx_startup/startup.cpp` | the game's NGX init (exe+0x273F230) and its DLSS 1 builder (exe+0x23842A0); the hooks whose removal failed for tifa-2/4 |
| `src/continuous_dlaa/activation.cpp` | DLSS activation |
| `src/continuous_dlaa/game_options.cpp` | the game's own DLSS option rules |
| `src/render_resolution/runtime.cpp` | render resolution |
| `src/continuous_dlaa/runtime.cpp` | its game-internal hooks (API ones are on AL) |
| `src/scaled_temporal/stream_route.cpp`, `src/temporal_present/observer.cpp` | partly |
| `src/projection_binding/recorder.cpp`, `provenance.cpp`, `trampoline_pool.cpp` | most MinHook-specific (asm entry stubs, trampoline checks); last |

`al.hooks` 1.0 (`third_party/al/al_plugin.h`): any game function by pattern
or address+pattern, chained with other plugins, active at PRE_MAIN if
registered early, removal by `set_enabled` (no thread freezing on our side).
Keep our SHA code gates before registering. Research DLLs and fixtures run
without AL and keep MinHook; only the shipped DLL drops it.

Proposed order (not yet confirmed by the user): NGX startup, then game
options and render resolution, then the DLSS runtime/stream/observer leftovers,
then the projection recorder, then MinHook out of the alpha's link.

## Cleanup candidates seen on the way

- Three "module + offset" formatters (`frame_generation/crash_log.cpp`,
  `perf/sampler.cpp`, `reflex/presentation_probe.cpp` `code_site`).
- `presentation_probe.cpp` is ~1,850 lines (worker, switch, DLSS mode/scene
  changes, logging); worth splitting.
- Research-only paths compiled into the alpha.
- Review backlog items that are debt rather than bugs: D7 leftovers
  (diagnostics), D13-D18 (NGX startup and temporal coexistence).
