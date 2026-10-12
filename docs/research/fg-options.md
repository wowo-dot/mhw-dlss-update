# Frame generation options (v0.21.0)

Roadmap step 2, "DLSS wholesale", after the Reflex options. Frame generation
offered a fixed multiplier only (2x, 3x, 4x, 6x). Streamline 2.12 offers two
more modes; both are now options. Sources: the Streamline headers we build
against (`sl_dlss_g.h`, pinned) and NVIDIA's DLSS-G programming guide in the
Streamline repository (sections 6.3, 6.5, 14.2, 22.5). Nothing here comes from
NVIDIA's binaries.

## What Streamline offers

| `DLSSGMode` | What it does | Needs |
|---|---|---|
| `eOn` | The fixed count in `numFramesToGenerate` | RTX 40 for 2x, RTX 50 for more |
| `eAuto` (legacy) | The same, but DLSS-G pauses generation when the game would run faster without it | as `eOn` |
| `eDynamic` | DLSS-G picks the count to reach `dynamicTargetFrameRate` (0: the display's refresh rate); `numFramesToGenerate` is ignored | `DLSSGState::bIsDynamicMFGSupported` (multi frame support, driver 595.41 or newer, D3D12) |

- The guide offers Auto as the fallback where Dynamic is not supported.
- V-Sync is not supported with Dynamic (guide 22.5), and the debug overlay
  shows Auto as V-Sync disabled (22.8).

## Not taken: full-screen menu detection

`DLSSGFlags::eEnableFullscreenMenuDetection` works only with a
`kBufferTypeUIColorAndAlpha` buffer (guide 6.5): the UI alone, with alpha.
- **What we tag:** HUD-less colour (the scene before the UI), not a UI layer.
- **Why we can't make one cheaply:** MHW draws its UI straight onto the final
  image, and the UI's alpha cannot be recovered from the difference.
- **What it would take:** sending the game's UI pass into a transparent
  target of our own and compositing it back. The same buffer would also allow
  UI recomposition (guide 6.6).
- **The guide prefers turning frame generation off in menus by hand.** For
  MHW that means finding the game's own full-screen-menu state.

Decided with the user on 2026-10-13: after the release.

## The options

| ini | Menu | Session (`game_session.hpp` `Mode`) |
|---|---|---|
| `frame_generation = 2x` to `6x` | Multiplier 2x to 6x | `fixed`: `eOn` |
| the same, plus `frame_generation_auto = on` | Auto pause: On | `automatic`: `eAuto` |
| `frame_generation = dynamic` | Multiplier: Dynamic | `dynamic`: `eDynamic` |
| `dynamic_target = display` or 30 to 1000 | Target frame rate | the target (0 for display) |

- **One ini key for the multiplier:** `frame_generation` holds off, a
  multiplier or `dynamic`, as before.
  - The menu keeps the last fixed multiplier in memory while Dynamic is
    chosen.
  - Starting from `dynamic` in the ini, that multiplier is 2x.
- **Auto pause:** applies to the fixed multipliers only. With Dynamic it is
  kept but unused, and the menu locks the row.
- **Target frame rate:** used only by Dynamic, and locked otherwise.
  - The choices are "Display refresh" and 60 to 540 fps.
  - An ini value outside the list joins it, in order, when the page is added
    at start.
  - A hand edit to a value outside the list while the game runs applies from
    the next start; the menu logs that.
- **Live:** all three apply at once while DLSS-G is loaded. Turning frame
  generation itself on or off still needs a restart.

## How the session applies them

- **The worker:** passes the ini's multiplier and mode at arm time.
- **Each tagged Present:** `wanted_options()` builds the options from the
  requested multiplier, mode and target, and calls `slDLSSGSetOptions` only
  when they differ from the applied ones.
  - Changes of mode or target count as `mode.changes`; count changes still
    count as `generated_changes`.
- **Dynamic fails closed, in two ways:**
  - it is used only while the last `DLSSGState` reports
    `bIsDynamicMFGSupported`; otherwise the fixed multiplier runs, counted in
    `dynamic_fallbacks`;
  - if DLSS-G refuses `eDynamic` anyway, `dynamic_refused` is set, and the
    fixed multiplier runs from the next Present on.
- **Auto's pause:** can only be inferred. `numFramesActuallyPresented` stays
  at 1 while On. The session counts Presents in a row without a generated
  frame (`single_streak`), and the status line reads 30 or more as paused.

## Logs

- `alpha_config`: `dynamic`, `dynamic_target`, `frame_generation_auto`.
- `fg_armed`: `dynamic_supported`, from the state.
- `fg_status` `session`:
  - `mode`: requested, target, applied (`DLSSGMode`), applied target,
    changes, `dynamic_fallbacks`, `dynamic_refused`, `single_streak`;
  - `state.dynamic_supported`.

## Status page

- **Dynamic, applied:** "Dynamic (display refresh), showing 3x" or
  "Dynamic (240 fps), showing 3x". The count shown is the frames presented
  for the last rendered frame.
- **Dynamic asked for but not running:** the fixed line, plus "(Dynamic is
  not available here)", "(DLSS refused Dynamic; see the log)" or "(switching
  to Dynamic)".
- **Auto:** "2x Auto, generating", or "2x Auto, paused (the game runs faster
  without it)".

## Tests

- **`alpha_config_tests`:** `dynamic` in any case; the last line wins; off
  after dynamic; the target bounds and bad values; round trips; Auto values;
  appended keys read back.
- **`alpha_menu_tests`:** the Dynamic choice index, the mode order, the
  support rule and its reason, target lists with an ini value, and the status
  lines above.

## In game (not yet seen)

To confirm on the RTX 50:
- `fg_armed` `dynamic_supported`;
- a live 2x → Dynamic → 2x switch (`mode.changes`, applied 3 then 1, status
  "showing Nx");
- Dynamic with a 120 fps target, then display refresh;
- Auto pause on with 2x, to see whether it ever pauses in MHW.
