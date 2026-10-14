# To CMFM: the mod list shrinks to its first row count (2026-10-14)

For the CMFM session (`D:\mh world modding\CMFM`). Found in the closed beta
(bundle 0.28.4 with cmfm.dll 0.1.0-dev, SHA-256 `edce63cc…dcd0`, the same
file as MO2's "CMFM (dev)"). The DLSS repository is read-only from CMFM, as
CMFM's is from here.

## What the player sees

With the beta's two mods (CMFM's Overview and DLSS, three pages), the Mods
page's list on the left shows only **two rows** at a time, though its layout
holds 11. Opening DLSS's dropdown makes four rows, so Settings and Status sit
below the window and the list needs scrolling. The panel on the right is fine.

## Cause (confirmed)

- `show_mod_list()` (src/native/mods_page.cpp) takes the window from the
  list's `DuplicateNum` (`+0x298`, `game::kListDuplicateNum`), then calls
  `gui_set_row_count(list, count)` with `count = min(rows, window)`.
- `setRowCount` (RVA 0xB7FED0) calls `rebuildRows` (RVA 0xB7FEF0), which
  **stores the count into `DuplicateNum`**: `*(DWORD *)(list + 664) = count`
  (clean database `local\ida-clean-20261011\mhw.i64` in the DLSS repo, image
  `a78ea69b…1d80`). native-menu.md says it "makes `count` new ones" but not
  that the field changes.
- So the first fill with 2 rows sets `DuplicateNum` to 2, and every later
  read (`show_mod_list`, `list_visible`, `drag_scrollbars`,
  `update_scrollbars`) sees a 2-row window.
- The beta's cmfm.log agrees: `Mods page: mod list on (2 mods, 3 pages, 11 rows
  visible; ...)`, the value read before the first fill.
- With 19 or 20 mods (the dev profile with the demo pages) the count is
  always 11, so `DuplicateNum` never changed. That is why it never showed
  up in testing.

## Suggested fix

Keep the layout's window size apart from the live field:
- read `DuplicateNum` once, when the page finds the mod list (the "mod list
  on" branch);
- if the unit and list survive between openings, keep the value per list
  instance;
- use that saved size in the five places that read `kListDuplicateNum` for
  the window (`mods_page.cpp` lines 773, 825, 1179, 1270 and 1893 at
  `2d7ddb1`).

The check at line 1815 (`== 0`: the layout has no mod list) can stay on the
live field. A test with one mod, then two mods with an open dropdown, then
more than 11 rows covers it.

## Also seen in the same sessions (not CMFM)

- Every exit under Astera Loader's stracker_compat crashed in
  `FextyMod.dll+0x179b` (AV reading `0x…fe00`, FextyMod's own thread,
  phase exiting). That is for the Astera Loader session.
- The DLSS mod's own beta bug (MO2 overwrite folder, fixed in 0.28.5) is in
  the DLSS repository.
