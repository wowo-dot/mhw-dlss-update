# The game's AA option under DLSS (v0.27.6, 2026-10-14)

Question (the user, before the menu rework): how does the game's own DLSS
force the anti-aliasing option, and can this mod's DLSS do the same?

Static reading of the clean database (`local/ida-clean-20261011/mhw.i64`,
image `a78ea69b…`); the game's English texts from
`common/text/steam_text_eng.gmd` (the user's extract). Nothing ran in game
for this note.

## What the game does (confirmed, static)

- **Saved choices stay.** `graphics_option.ini` holds the player's choices
  (`Anti-Aliasing` = Off / TAA / FXAA / TAA+FXAA, `ResolutionScaling` =
  Low / Mid / High / Variable (resolution) / Variable (frame rate)). DLSS never
  rewrites them (one exception: DLSS and FidelityFX CAS both on at start
  turns both off, `0x43e220`).
- **The effective value is substituted where it is applied.**
  - `0x43dcb0`, the anti-aliasing apply: while "DLSS in effect" (`0x4431f0`:
    ini DLSS on, the game's NGX context, its optimal-settings query), the
    value becomes **TAA** or **TAA+FXAA** from the DLSS mode, because the
    game's DLSS 1 runs inside `uTemporalAAFilter2` (`native-dlss-path.md`).
    It then writes the renderer table's TAA/FXAA bytes (+0xe9e4/+0xe9e5)
    through `0x2287a00`, its only writer, and walks the unit list
    (`[0x51238c8]+0x15d0`) to set bit 0x2 of each `uTemporalAAFilter2` and
    `uFXAAFilter` unit.
  - The units also follow the bytes themselves every frame (vtable slot 10:
    `0x2393260` TAA2, `0x2394140` FXAA).
  - `0x43eeb0`, the resolution-scaling apply: under DLSS (or CAS) the value
    becomes **High** (scale 1, no variable resolution: `0x229bf30`,
    `0x229bde0` writes renderer +0x1f0).
- **The graphics menu (`uGUIGraphicsOption`, items 15-36) locks two rows**
  while DLSS is in effect: item 15 (Resolution Scaling) and 22
  (Anti-Aliasing). The check is inlined in three places:
  - `0x53f580` (a row takes input; used by the row build and the input
    handlers);
  - `0x53f440` (page refresh: `0xb72f00(row object, 9)`, the greyed look);
  - `0x540720` (restore defaults: locked rows keep their values).

  The row build `0x53f190` swaps the help text: 10552 for item 15, and for
  item 22 one of 10553/10554/10555 by the DLSS mode. The menu's ids are
  10000 + the index in `steam_text`:
  - 552: "If NVIDIA DLSS is on, this will automatically be set to High."
  - 553: "If NVIDIA DLSS is on, this option will be unavailable."
  - 554/555: "... automatically be set to TAA" / "TAA + FXAA".
- Text 562 also says the Dynamic Range option applies 64-bit under DLSS. That
  is not done through `0x43e130` (the Dynamic Range apply, renderer +476
  bit 0x8000); where it happens was not found. Not used here.

## What this mod does (v0.27.6, `src/continuous_dlaa/game_options.cpp`)

The same rules for this mod's DLSS, with **Off** instead of TAA.

- This mod runs the TAA2 unit itself as DLSS's input pass (activation forces
  it into the queue and passes its table gate). Every test so far ran with
  the game's AA Off (`native_choices` [n,0,0,0]); that is the tested state.
- **Hooks (MinHook, enabled at install, pass-through until taken):**
  - `0x43dcb0` applies Off and records the player's value;
  - `0x43eeb0` applies High and records the player's value;
  - `0x53f580` locks rows 15 and 22;
  - `0x53f440` greys them after the page's own pass;
  - `0x53f190` passes the game's help ids 10552 and 10553 (the player's
    language);
  - `0x540720` keeps the two rows on restore defaults, without recording the
    defaults as the player's choice.
- **Taken** (`take_over`, worker):
  - at the DLSS install, before `render_resolution::install` (its start-up
    check needs the game's scale at 1: `full_scale()` waits up to 2 s);
  - when leaving the player's Off, at the rebuild's step 4.

  The anti-aliasing goes through the game's setter `0x2287a00` only (the
  units follow on their next update; the game's unit-list walk stays on the
  game's thread). Resolution scaling goes through the game's apply
  `0x43eeb0`.
- **Given back** (`give_back`): at the player's Off, a fault, or a stop.
  Before it, `render_resolution::released()` (the render thread restored its
  owned scale) waits up to 1 s, so the game's scale write cannot race that
  restore. Then the player's values go through the same two functions.
- **The player's values:** the saved ones at install (`0x4413e0`,
  `0x442120`), then every value the game applies (its menu, presets,
  start-up).
- **If the code differs (gates):** nothing is hooked, and DLSS runs as in
  v0.27.5, over the player's settings.

Gates: 12 new (201 in all). Log: `dlss_summary.runtime.game_options` holds:
- saved and player values, the table bytes, the variable-resolution byte and
  scale;
- take-overs and give-backs, applies and forced counts;
- rows locked, greyed and helps, defaults kept;
- the game's own DLSS context;

and `native_choices`.

Fixes on the way:
- Resolution Scaling Low/Mid at start no longer faults render_resolution
  (fault 5, "existing reduced rendering");
- Variable modes no longer run the game's dynamic resolution under the
  override.

## Found on the way: the game's own DLSS 1 context

- `cD3DRender::initForD3D12` creates the game's NGX context (`0x23842a0`) on
  any NVIDIA adapter with Windows 10 1709 or newer (+0x2da). It keeps the
  context only if `SuperSampling.Available` and
  `ImageSignalProcessing.Available` (DLISP); otherwise
  `0x51c6c28` = 1/2/3.
- The 5090 has no context (the reason is logged from v0.27.6 as
  `game_dlss.refusal`).
- This mod refuses to run beside that context: `render_resolution` check 11
  and activation's `primary()`. A card where the game's DLSS 1 still starts
  (possibly RTX 20/30) would never arm.
- **Open, before release:** find out which cards get the context. If any do,
  run beside it with the game's DLSS mode off: its menu row locked, or the
  context ignored while renderer +168 is 0.

## Not covered

- FidelityFX CAS with this mod's DLSS (the game makes them exclusive):
  covered from v0.30.1, `game-cas-option.md`.
- Dynamic Range 64-bit with this mod (raw copies are R11G11B10).
- In game: the locked rows, help texts, Off and back, and Low/Mid/Variable or
  TAA/FXAA saved at start. That is the next run.
