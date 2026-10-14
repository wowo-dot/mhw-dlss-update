# To CMFM: D-pad Down moves the mod list up (2026-10-14)

For the CMFM session (`D:\mh world modding\CMFM`). Reported in the closed
beta (bundle 0.28.5 with CMFM 0.1.0-beta.2, cmfm.dll `8fb5d064…f34e`): in
the Mods page, **D-pad Down moves the mod list's highlight up**. Up works.
Keyboard Down has not been checked yet. The DLSS repository is read-only
from CMFM, as CMFM's is from here.

## Likely cause (from the code at 25a5126, not yet checked in game)

It is the same family as the window bug: it only shows with few pages.

1. **The category list is sized to the pages.** In split mode the page sets
   the hidden category list to `used = min(entries, kTitleViewTabs)` rows
   (`gui_set_row_count(categories, used)`, mods_page.cpp ~1870).
2. **The parked cursor ends up on the last row.** The cursor is parked on
   `kSplitPanel = 2` (`keep_panel_cursor`). The beta has 3 pages (CMFM's
   Overview, DLSS Settings, DLSS Status), so `used` is 3, and the user's
   cmfm.log says `Mods page: on (3 categories, 27 rows)`.
3. **Down wraps and reads as Up.** The move hook (~line 448) takes the
   direction from `row > old` on the category list. Down from the last row
   wraps to row 0, so `row > old` is false, and the move becomes Up.
4. **Why it never showed before.** With the dev profile's 23-25 pages the
   list had 5 rows, so row 2 was in the middle.

**Related edge:** with fewer than 3 pages (CMFM alone, or one mod with one
page), `used` is below 3 and row 2 does not exist; `keep_panel_cursor` then
writes the cursor field directly. Worth a check.

## Suggested fix

Take the direction from the input, not from the row comparison. Either:
- use the game's step functions, which CMFM already detours for the panel
  (`step_down_detour` and `step_up_detour`);
- or count a wrap (`old == last && row == 0`) as Down and the reverse as
  Up;
- or keep the category list at least `kSplitPanel + 2` rows in split mode,
  so the parked cursor never sits on an edge.

A test with 3 pages, then 1 page, then more than 5 pages covers it, with
the D-pad and the keyboard.
