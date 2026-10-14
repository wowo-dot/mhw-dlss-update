# The CMFM menu, reworked (v0.28.0, 2026-10-14)

The user left the design to Claude, with these rules from earlier
conversations:
- a row that cannot change is **greyed with the reason, not hidden**;
- frame generation cannot be set On while DLSS is Off (game-129's "outdated"
  menu);
- the menu offers **what this card supports** (`card-capabilities.md`);
- the Status page should not be cramped.

## Settings page

| Section | Row | Choices | Greyed when (`menu_model::lock_reason`) |
|---|---|---|---|
| Upscaling | DLSS | Off / On | the card has no DLSS (not NVIDIA) |
| | Mode | DLAA, Quality, Balanced, Performance | no DLSS; DLSS Off ("Turn DLSS on first.") |
| Frame generation | Frame generation | Off / On | `frame_generation_plugin = off`; the card, driver or Windows has none (their reason); DLSS Off |
| | Multiplier | the card's: 2x to its DLSS-G limit (`multiplier_choices`) | as above; frame generation Off; Dynamic pacing; one choice only (RTX 40: "2x at most") |
| | Pacing | Fixed, Auto pause, Dynamic (only where the card supports it) | as above |
| | Target frame rate | Display refresh, 60 to 540 fps | as above; pacing not Dynamic |
| Latency | Reflex | On, Boost, Off | no Reflex; Off refused while DLSS-G is loaded |

- **DLSS is a switch of its own.** Mode no longer holds "Off". The ini keeps
  one key: `dlss_mode = off` or the mode. The mode is kept in memory while Off.
- **Pacing replaces two rows:** the "Auto pause" switch, and "Dynamic" as a
  multiplier.
  - In the ini: `frame_generation = dynamic` for Dynamic, and
    `frame_generation_auto` for Auto pause.
  - Choosing Dynamic keeps the Auto pause setting for the way back.
- **Validation also guards CMFM's "Reset to defaults"**, which sets locked rows
  too:
  - frame generation On is refused while its lock has a reason;
  - a multiplier or Dynamic above a limit DLSS-G reports later is refused.
- Everything applies at once, as before.

## Status page

The value box holds about 25 to 30 characters before the game shrinks the
text (CMFM: AutoScale). Every line is now one fact in a few words; the help
line explains.

- **Now** (live): Mod, DLSS (the mode or Off), Resolution
  ("1696x954 to 2560x1440", or "The game's own"), Frame generation
  ("2x, generating", "Dynamic, showing 3x", "Paused (DLSS is off)"),
  Generated frames, Reflex.
  - The Frame generation help says DLSS-G generates only while the game
    window has focus (game-135).
- **System:** Graphics card, Video memory (whole GB rounded up: DXGI's
  32,187 MB is "32 GB"), Driver, and Frame generation support:
  - "Up to 6x, Dynamic" or "Up to 2x";
  - "No: needs an RTX 40 or 50" or "No: needs driver x";
  - "Not loaded (mhw_dlss.ini)".

## When the pages are added

CMFM keeps a page's choices as added (no update or removal), so the pages
wait for the card:
- `menu::card_ready()` comes from the end of `attach_device`, when the game
  creates its device, seconds before its title menu;
- otherwise, after 20 s, the pages are added with what is known (the full
  lists).

The log line says when they were added and what was offered. Non-NVIDIA cards
now report DLSS as unsupported from DXGI's vendor id, so the DLSS row is
greyed with its reason.
