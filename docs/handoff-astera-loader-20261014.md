# Handoff to Astera Loader: no input window when the presentation owner hands the game a composition swap chain (2026-10-14)

From the MHW DLSS session. Found by the user: CMFM's mouse wheel does nothing
while the DLSS mod is loaded, and works without it.

## Summary

Astera Loader 0.2.0 (commit `c6e0f6f600a8`) learns the game window only from
`GetHwnd` on the swap chain the game receives. Since DLSS mod v0.27.4, the
game receives a **composition** swap chain: the presentation owner's stand-in
for live frame-generation switching. Composition chains have no HWND (DXGI
returns `DXGI_ERROR_INVALID_CALL`), so:

- `Hub::describe` leaves `current_.window` null;
- `Host::dispatch(AL_PHASE_SWAPCHAIN_CREATED)` calls
  `input.attach_window(nullptr)`, which returns at once;
- the game window is never subclassed, and `Native::State::call` stays null;
- `Native::begin_frame` never sets the focus, so `Service::focused_` stays
  false.

As a result, **all of `al.input` is dead for every plugin**: hotkeys,
`key_down`, pad state, captures, and window messages (CMFM's `WM_MOUSEWHEEL`
through its message callback). This happens on every RTX 40/50 system running
the DLSS mod. RTX 20/30 cards don't get the stand-in and are probably fine.

## Evidence (MO2 `AsteraLoader/AsteraLoader/logs`)

| Run | DLSS mod | `input_window_subclassed` | `input_state=3 … messages_consumed` |
|---|---|---|---|
| `core.43568.log` (and the 3 runs before it) | loaded (0.28.3) | absent | 0 |
| `core.44232.log`, `core.51952.log` | not loaded | present | 43 |

The DLSS mod's own log for pid 43568 (`local/fg-game-20261006/game-139`) shows
`"identity":true`: the game got the composition stand-in. AL's log for that run
shows the swap-chain detours installed (`swapchain:8 … installed=yes` at
4.85 s), so `swapchain_created` did run; only the window was missing.

## Where in AL

- `src/core/graphics/layers.cpp`: `create_swapchain_for_hwnd(…, HWND window, …)`
  receives the window the game asked for, but calls
  `hub->swapchain_created(*out, queue)` without it. `create_swapchain` has it
  too, in `desc->OutputWindow`.
- `src/core/graphics/hub.cpp`: `Hub::describe` takes the window only from
  `IDXGISwapChain1::GetHwnd`. `Hub::swapchain_created` (`hub.hpp:110`)
  dispatches the phase with that `current_`.
- `src/core/plugins/host.cpp:835-837`: `attach_window(chain->window)`.
- `src/core/input/native.cpp`: `attach_window` returns on a null pointer before
  `user32()`, so `begin_frame`'s focus check never runs.

## Proposed fix

1. Pass the creation window through: `swapchain_created(void* swapchain, void*
   queue, void* window)`. `create_swapchain_for_hwnd` passes `window`, and
   `create_swapchain` passes `desc ? desc->OutputWindow : nullptr`.
2. In `describe`: if `GetHwnd` fails or returns null, use the creation window.
   Prefer the chain's own HWND when it has one, so the plain case is
   unchanged.
3. Optional: log the source, e.g. `swapchain_window=chain|creation|none`.
   That makes this visible next time.

This is the game's own window, the one it passed to `CreateSwapChainForHwnd`,
so `attach_window`'s checks (same process, top-level `GA_ROOT`) still apply
unchanged.

Also consider `al_d3d12_swapchain_t.window` for plugins: with this fix it
names the game window for a composition chain too. Document that it is the
window the game created its chain for.

## Tests

`tests/test_d3d12_hub.cpp` already drives `swapchain_created`. Add these cases:
- **Composition chain:** `GetHwnd` fails and the creation window is given, so
  `current_.window` is the creation window.
- **Ordinary HWND chain:** unchanged.
- **Neither:** the window stays null, as before.

If a fixture covers `attach_window` or `begin_frame`, assert that focus can
become true after a composition-chain creation.

## Check in game

1. Run AL with the fix and DLSS mod 0.28.4 (installed in MO2 as run game-141,
   Quality + 2x).
2. The core log should show `input_window_subclassed`, and `messages_consumed`
   above 0 at exit.
3. CMFM's category list should scroll with the mouse wheel.

## Meanwhile

- The DLSS mod's `frame_generation_plugin = off` avoids the composition
  stand-in, which restores AL input but loses frame generation.
- The closed beta (2026-10-15) lists it as a known issue.
- The DLSS mod's release needs an AL release with this fix (or the release
  notes must name the AL version).
