# To CMFM: the kept Mods page shifts the game's GUI unit list (2026-10-15)

For the CMFM session (`D:\mh world modding\CMFM`). A player's speedrun tool
(IN-Q, a Cheat Engine table) crashed when it applied an item loadout in a
quest, but only with our mods installed. The cause is confirmed in the user's
game, and it is CMFM's page unit. The DLSS repository is read-only from CMFM,
as CMFM's is from here.

## What IN-Q does

IN-Q forges a request in the game's GUI request queue: count at
`sMhGUI+0x8F60`, 0x80-byte entries from `+0x4EE0`. The request is
uGUIBoxItemMySet's apply-loadout callback (vtable `exe+0x3460708`), bound to
the unit `[[sMhGUI+0x278]+0x170]`:
- `sMhGUI+0x268` is the count of the game's registered GUI units and
  `sMhGUI+0x278` their array (as read by sub_141ADEBE0);
- so IN-Q takes **slot 46** and assumes it is the item box's Item Loadouts
  unit.

## What we measured (Frida, read only, the user's game, in a quest, Mods never opened)

- 165 units in total.
- Slot 18 is CMFM's `uGUICmfmPage` (vtable in cmfm.dll). cmfm.log shows it
  was made during startup, before the title row: "Mods page: CMFM's page
  made (unit …, page 1, kept for the session)".
- Slot 19 is uGUIHudRadar, slot 46 uGUIBoxItemSell, slot 47
  uGUIBoxItemMySet.

Every game unit made after CMFM's page sits one slot later than in the
unmodded game. IN-Q's slot 46 is the Sell screen, and the loadout callback
then reads a field that is not an animation instance and crashes
(`cGUIInstAnimation::getPlayingSequenceId`, the field at +0x1C0 = 2).

## The request

The unit list should look as the game made it while the player is not in
Mods. Then tools and mods that index it, fragile as that is, keep working:
- make the page unit when the Mods page opens, not at startup;
- release it when the page closes, instead of keeping it for the session.

The user suggested this ("only add itself when the menu is opening").

## To check while doing it

- Whether the game compacts the unit array when a unit is removed. If it
  leaves a hole, the slots stay shifted after the first visit to Mods.
- That opening Mods from the title menu works the same way. At the title,
  most game units (HUD, item box) do not exist yet.

The run: `dlssupdate/local/reports/kong1912-1/runs/20261015-guilist/` (snapshot,
resolver, notes; private, not in git).
