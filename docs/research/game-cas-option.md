# The game's FidelityFX CAS option under DLSS (v0.30.1, 2026-10-19)

Question (jgb2003-2, 2026-10-18): with FidelityFX CAS On, the mod stayed at
"Starting" for the whole session. Its log waited on `native sizing admission
(check 16)`: the renderer's content scale was not 1.

Static reading of the clean database (`local/ida-clean-20261011/mhw.i64`,
image `a78ea69b…`). Nothing ran in game for this note yet.

## What the game does (confirmed, static)

- **The option.** `graphics_option.ini` keys `FidelityFX CAS` (On/Off) and
  `CAS_Sharpness`. `0x441520` reads both, packed in one register: the flag in
  the low byte, the sharpness (float) in the high dword. `0x441620` is the
  same read on the game's ini table.
- **The apply, `0x43de50`.** It takes a pointer to that 8-byte value and:
  - writes renderer +0xBC (on) and +0xC0 (sharpness);
  - calls `0x229bd20(renderer, on ? 0.8 : 1.0)`, which stores the factor at
    +0xC4, sets +0x200 (the desired content scale) to
    clamp(factor x +0xB0, 0.5, 1) and flags a scale update (+0x1F548 |=
    0x2040);
  - reads `ResolutionScaling` from the ini again and applies it
    (`0x43eeb0`, which applies High under CAS).

  So CAS renders the scene at 80 % and sharpens it back up. +0xB0 is the
  game's DLSS 1 optimal scale, 1 without it.
- **Who applies it.** Only two callers, plus the start-up:
  - `0x1e8ab40`, the display window's apply, when the CAS value changed
    (window +30525 vs +30524);
  - `0x1e8d890`, the display window's row set-up: it applies the saved CAS
    each time the window builds its rows;
  - `0x43e220`, the start-up apply (called once, from `0x1aec7d0`). It writes
    +0xBC and calls `0x229bd20` itself, not through `0x43de50`.

  Nothing else writes +0xBC. It is read by the render side (`0x2293f10`,
  `0x2296f10`, `0x23c3790`, `0x23ec920`).
- **CAS and the game's DLSS are exclusive.**
  - At start, both saved On turns both off and rewrites the ini (`0x43e220`).
  - In the display window, the value change `0x1e89ea0`:
    - turning DLSS (row 2) On shows message 557 once and sets CAS (row 13)
      Off;
    - turning CAS On shows message 766 once.
  - The resolution-scaling row (graphics menu item 15) is locked under either
    (`0x53f580`). With CAS saved On, the row build `0x53f190` swaps its help
    for 10761 (item 15) and 10252 (item 22).
- **The display window's rows.** `0xeeae80` decides whether a row takes input.
  Its callers are in three menus: `0x1e8d200` and `0x1e8e600` (the display
  window), `0x1e806b0` and `0x1e82170`, and `0xaccca0`. Its descriptor holds
  the kind at +0 (2 = the display options) and the index at +20.
  - Kind 2 row 3 (DLSS mode) follows the DLSS ini flag.
  - Row 14 (CAS sharpness) follows the CAS ini flag; row 13 (CAS) is always
    available.
  - The callers grey a row it refuses (`0xb72f00(row, 9)`), as the sharpness
    row is greyed with CAS Off.

## Why the mod waited

`render_resolution`'s snapshot requires the content scale at 1 in the
renderer (+0x200, +0x204) and the view (+0x6b4): check 16. With CAS On the
desired content scale is 0.8, so `ready()` refused every attempt. It ran
before `install()`, which is where game_options takes the game's options.

## What this mod does (v0.30.1)

The game's own rule, as for anti-aliasing and resolution scaling
(`game-aa-option.md`): CAS Off while this mod's DLSS runs, the player's choice
kept and given back.

- **game_options** (`src/continuous_dlaa/game_options.cpp`, two more hooks):
  - `0x43de50`: every value applied is the player's; while taken it applies
    Off with the player's sharpness;
  - `0xeeae80`: while taken, kind 2 rows 13 and 14 take no input, so the game
    greys them. Their help text stays the game's own (the display window has
    no "unavailable" text for them);
  - taken: AA Off, CAS Off, then resolution scaling High. CAS's apply
    re-applies the scaling from the ini, so High comes last;
  - given back: AA, CAS, then the player's scaling (read before CAS's apply
    records the ini's);
  - the saved CAS is read at install (`0x441520`). The log's
    `game_options.cas` shows the saved and player values, the renderer's
    +0xBC/+0xC0/+0xC4, and the applies and forced counts; `rows` gains
    `display_locked`;
  - with CAS saved On, the graphics menu's locked rows get the DLSS help back
    after the game's build (row +24).
- **render_resolution:**
  - `ready()` admits CAS's content scale (check 16 with +0xBC on). `install()`
    takes CAS before render_resolution installs, and `full_scale()` still
    requires the content scale at 1;
  - `install()` admits it only when the player's DLSS is Off at start
    (yielded);
  - an unqualified visit faults (4) only while the override is requested.
    Yielded or stopped, the game's own CAS is its business;
  - `full_content_visits()` counts visits that read the content scale at 1;
    `restorations()` counts visits that saw the game's own scale whole.
- **continuous_dlaa:**
  - from the player's Off, after `take_over()`, the rebuild waits for two
    visits at full content scale before the override can own a frame (the
    scale update reaches the render thread on its next update). Otherwise it
    gives the options back and stays Off (`fidelityfx-cas`);
  - `give_back_options()` waits for one fresh restoration visit before giving
    CAS back. CAS's content scale refuses those visits from then on, and
    `finish()` needs one (fault 47 otherwise at exit);
  - if game_options cannot install (the game's code differs) and CAS is on,
    the DLSS install stops on `fidelityfx-cas`. Status reads "turn
    FidelityFX CAS off in the game's display settings".

Gates: 4 new (`0x43de50`, `0x441520`, `0x229bd20`, `0xeeae80`), 113 in all,
matched on the clean image.

## To check in game

- CAS On saved, DLSS On at start: arms; `game_options.cas` saved [1,…],
  renderer [0,…,1]; the display window's CAS rows greyed.
- DLSS Off in the menu: CAS back (renderer [1,…,0.8]), the image is the game's
  CAS. DLSS On again: armed, no fault 4.
- CAS On saved, DLSS Off at start, then On.
- Exit with CAS saved On: no fault 47, and `resolution.restored` true.
- The graphics menu's Resolution Scaling help with CAS saved On.
