# To Astera Loader: start the game without Steam's launch options (2026-10-14)

For the Astera Loader session (`D:\mh world modding\AsteraLoader`). This is a
request from the DLSS mod's closed beta, not a bug in AL. AL's repository is
read-only from here, as `dlssupdate` is from there.

## What happened

Tester flejmzi (closed-beta bundle 0.28.5: AL 0.2.1-dev 86cdb81) did a manual
install, without MO2, and started `AsteraLoader.exe` directly:
- the launcher started the game and loaded `AsteraLoader.dll`;
- the game exited with **code 53** inside SteamStub, 0.25 s and 0.78 s in,
  before PRE_MAIN, in two runs;
- Steam then started the game again without AL, so neither CMFM nor the DLSS
  mod loaded.

This is AL's known behaviour (release-checklist.md, manual-release-1: "AL
leaves that check alone"). With Steam's launch options
(`"<game folder>\AsteraLoader.exe" %command%`) it works; the tester confirmed
it.

The DLSS bundle's README had said to start `AsteraLoader.exe` directly. It
now says what AL's README says.

## The request

`AsteraLoader.exe` started on its own (double-click, a desktop shortcut,
another launcher) should start the game with AL, as it does under MO2. The
launch-options step is easy to miss, and players expect a loader's .exe to
work by itself.

The user asked for this to be noted; whether and how to do it is AL's
decision. Constraints from this project's rules:
- Steam's ownership check must still run. Nothing in SteamStub or the game's
  Steam check is patched or skipped.
- Steam must be running, as for any Steam launch.

## What is known and what is not

**Confirmed:**
- the exit code 53;
- that MO2 launches of the same exe work;
- that Steam launches with `%command%` work.

**Not checked yet:** why MO2 works. One hypothesis is that MO2 starts the
game with the Steam app id in its environment (as Steam does), so SteamStub
sees a Steam launch and does not restart the game through Steam. If so, the
launcher could give the game the same environment Steam gives it (app id
582010). The other option is for the launcher to ask Steam to start the game
with AL.

Logs: `dlssupdate/local/beta-reports/flejmzi-1/` (private, not in git).
