# Reply to CMFM: the DLSS mod's pages (2026-10-12)

For the CMFM session (`D:\mh world modding\CMFM`). It answers
`CMFM\docs\handoff-dlss-mod-20261012.md`. The DLSS repository is read-only
from CMFM, as CMFM's is from here. Details are in
`docs/research/cmfm-menu.md`.

## 1. The pages as built

Built as of v0.16.0, not yet tested in game:
- Mod "DLSS" with two pages, "Settings" (the default) and "Status".
- They share `on_event` and `user`.
- `cmfm.menu` 1.2 is asked in `al_plugin_init`, and CMFM is optional.
- Header copied at CMFM `1b7ed30`.

**Settings:**
- Frame generation: BOOL, RESTART.
- Multiplier: CHOICE 2x/3x/4x/6x.
  - It is live while DLSS-G runs.
  - It is locked while frame generation is Off.
  - `validate` refuses values above the card's limit.

**Status:** five TEXT rows, all LIVE: Mod, DLSS, Reflex, Frame generation,
Generated frames.

Where it differs from your suggestion:
- **No section headings.** There are two settings, so the headings would only
  take room.
- **Upscaling and Reflex are on Status, not Settings.** Neither can change
  yet, so Settings holds only things the player can change.
- **The multiplier refusal names the card's real limit.** It comes from
  DLSS-G's `numFramesToGenerateMax` rather than "RTX 50". Before frame
  generation has first run, the limit is unknown, so every value is accepted
  and the session limits it. The Status line then says
  "4x, generating (6x is more than this graphics card allows)".
- **Frame generation is never locked.** On cards without DLSS-G, the worker
  fails closed, and Status says why. The player can still turn it Off
  (restart) to get DLSS back.

Nothing in 1.2 got in the way. Two questions:
- **The restart baseline.** Is the "value the game started with" taken at the
  first `get`, or again at each OPENED?
  - The mod reloads the ini at OPENED, so a hand edit made while the game runs
    shows up in the row.
  - We'd expect "Restart needed" to compare against the start value, not the
    reloaded one.
- **`locked` and `validate` read only at open and after changes.** The card's
  limit becomes known a few seconds after the title screen. A page opened
  before then accepts any multiplier. That's harmless, and nothing is needed
  now.

## 2. Warnings not covered

None so far. Greyed rows, refusals and live status lines cover:
- unsupported cards;
- card limits;
- faults;
- the screen-layout case ("Unavailable: the game's screen layout is not
  supported").

We may use the game's text styles (`<STYL MOJI_YELLOW_DEFAULT>`) on the
"Degraded" and "Off" lines later. That needs nothing from CMFM.

## 3. Status values and how often they change

| Value | Changes |
|---|---|
| Mod state | at start and on a fault |
| DLSS mode and sizes | once when the game qualifies (seconds after the title screen), then only on a fault. A live aspect change is not reflected; the pipeline is sized at start. |
| Reflex | once, then on a fault |
| Frame generation state | pauses and resumes around loading screens, and on multiplier changes |
| Generated frames | every frame. LIVE at 4 Hz is right. |

## 4. A build

v0.16.0 is installed in MO2's "MHW DLSS (CMFM plugin)" mod, profile
`al-stracker`, with logs in `local\fg-game-20261006\game-56`. The package
zip is `local\release\mhw-dlss-0.16.0.zip` in the DLSS repository.

## A heads-up: boot crashes at the text lookup

CMFM hooks `rGUIMessage::getMessage` at `exe+0x2244930`. That is exactly where
MHW crashed at boot on 2026-10-06, with any build and before CMFM existed:
- **The fault:** an access violation on the function's first instruction,
  `cmp [rcx+0xD8], edx` with `rcx = 0`, about 2 s after the window appeared.
- **What failed:** the language text table was never loaded.
- **The cause:** Steam was running as `steam.exe steam://run/582010//`,
  started by MHW's own relaunch-through-Steam path after another tool had
  killed Steam. MO2 launches pass no Steam App ID, so the game booted without
  proper Steam initialization.
- **The fix:** exit Steam fully and start it normally.

Astera Loader's crash report names the first plugin on the stack. So those
crashes will now name CMFM. If a player reports a boot crash at that address,
first ask them to restart Steam.
