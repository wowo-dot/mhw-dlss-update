# The mod's pages in CMFM's Mods menu (v0.16.0, 2026-10-12)

CMFM is an Astera Loader plugin that adds a **Mods** entry below Options to
the game's own menus. The game draws that page itself; CMFM draws nothing.
From v0.16.0 the mod puts two pages there: **DLSS > Settings** and
**DLSS > Status**. This follows CMFM's handoff
`D:\mh world modding\CMFM\docs\handoff-dlss-mod-20261012.md`.

## Interface

- **Header:** `third_party/cmfm/cmfm.h`, `cmfm.menu` 1.2, copied unchanged.
  The CMFM commit is recorded in `third_party/cmfm/README.md`.
- **Optional:** in `al_plugin_init`, once the worker has read the ini, the mod
  asks `get_interface("cmfm.menu", 1.2)`.
  - NULL means CMFM is missing or older. The mod logs one line and runs as
    before.
  - CMFM is not in the mod's dependencies.
- **Code:**
  - `src/alpha/menu.cpp`: the pages, the callbacks and the ini write.
  - `src/alpha/menu_model.hpp`: the settings model.
  - `src/alpha/status.{hpp,cpp}`: the status lines.
  - `src/alpha/config.hpp`: the ini edit (`with_frame_generation`).
  - The pure parts are tested in `tests/alpha_menu_tests.cpp`.

## Settings page

| Row | Type | Values | Behaviour |
|---|---|---|---|
| Frame generation | BOOL, RESTART | Off / On, default On | DLSS-G loads only at start. CMFM marks a change as "Restart needed". |
| Multiplier | CHOICE | 2x, 3x, 4x, 6x, default 2x | see below |

How the Multiplier row behaves:
- It applies at once (`frame_generation::game::multiplier`, an atomic store) when DLSS-G was loaded at start. Otherwise it applies from the next start.
- It is locked while Frame generation is Off ("Turn frame generation on first.").
- `validate` refuses a multiplier above the card's limit. The limit is DLSS-G's `numFramesToGenerateMax`, mirrored lock-free by the session.
  - Before frame generation has first run, the limit is unknown. Every value is then accepted, and the session limits it.
  - The refusal says, for example, "This graphics card shows 2x at most; 3x and up need an RTX 50."

**One ini key holds both rows.** Off writes `frame_generation = off` and On
writes the multiplier. While frame generation is Off, the multiplier is kept in
memory, so turning it back On restores it.

**Upscaling and Reflex are not on Settings.** Neither can change today, so they
appear only on Status.

## Status page

All rows are TEXT with `CMFM_FLAG_LIVE`, so CMFM reads them about 4 times a
second while they are shown.

| Row | Example | Source | Changes |
|---|---|---|---|
| Mod | `Active`, `Degraded: <reason>`, `Disabled: <reason>` | the Astera Loader status the mod sets, then the stages below | at start and on a fault |
| DLSS | `Performance, 1280x720 to 2560x1440`, `Starting`, `Off: <reason>`, `Stopped; the game renders without DLSS` | worker: stage and sizes after `temporal::arm`; live `continuous_dlaa::healthy()` | once when the game qualifies, then on a fault |
| Reflex | `On (low latency)`, `Starting`, `Off: <reason>` | worker: after the live frame hooks are installed | once, then on a fault |
| Frame generation | `2x, generating`, `2x, paused`, `4x, generating (6x is more than this graphics card allows)`, `Unavailable: <reason>`, `Off` | worker stage, plus the session's lock-free `status()` | pauses and resumes around loading screens, and on multiplier changes |
| Generated frames | `12,345` | session: sum of `numFramesActuallyPresented - 1` per Present | every frame |

How the rows are read:
- **Reasons.** Reasons from the log are turned into words for the player when
  they are known (`status::reason_text`). Any other reason reads
  "see the mod's log (<reason>)".
- **Threads.** CMFM calls back on the game's menu thread.
  - `get_text` reads a status snapshot under a short mutex and the session's
    atomics. It takes no render or session lock.
  - `validate` reads the session's atomics.

## Writing the ini

**When it is written:** at `CMFM_EVENT_CLOSED`, if a value changed. This
follows CMFM's advice, so the file is written once per visit.

**How it is written:**
1. The file is read again, so hand edits made to other lines meanwhile stay.
2. Only the value of the last `frame_generation` line changes. Its key,
   spacing, comment and line ending stay, as does every other byte. Without
   such a line, one is added.
3. The edited text must read back (`parse_config`) as the intended value, or
   nothing is written.
4. The text is written where the file really is, using CMFM's approach. The
   file is opened, and `GetFinalPathNameByHandleW` gives its real path. The new
   text goes to `<real>.new`, is flushed, then moved over the file with
   `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`.
   - Under Mod Organizer 2 the real path is the mod's own folder. A new file
     created through the game folder's path would land in MO2's overwrite
     folder instead.

**When it is read:** at `CMFM_EVENT_OPENED`, the ini is read again, unless the
menu holds a change not yet saved. This way the rows follow edits made by hand
while the game runs.

**Logging:** every change and save goes to Astera Loader's plugin log
(`AsteraLoader\logs\plugins\mhw_dlss.log`). The mod's own JSON log belongs to
the worker thread.

## To check in game (v0.16.0)

These points are open until the in-game test.
- `cmfm.log` shows both pages added, and Astera Loader's plugin log says
  "menu: 2 of 2 pages added to CMFM".
- Settings:
  - Multiplier 2x → 3x → 4x applies at once. The Status line follows, and so
    does the log's `fg_status` multiplier.
  - Frame generation Off shows "Restart needed" and locks Multiplier.
  - After closing, `mhw_dlss.ini` in the MO2 mod folder (not Overwrite) has
    `frame_generation = off` with its comment intact. The next start runs
    without DLSS-G.
- Status:
  - The lines change while the page is open.
  - The generated-frames counter grows.
- Stability: menu open and closed, alt-tab, loading screens, Reset to defaults.
