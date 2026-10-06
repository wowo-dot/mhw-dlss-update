# One-hour output-retirement soak passed — v1.18.4

The full 60-minute run completed without a runtime fault. The user reported no
visible problems, confirmed area changes through loading screens, and exited
normally. Both the final native log and all 719 saved status snapshots pass the
frozen independent reviewer. The collector stopped the diagnostic at its planned
duration limit; it did not terminate the game.

| Check | Result |
|---|---:|
| Successful DLAA evaluations | 296,528 |
| GPU recordings created/submitted/completed/reset/released | 297,110 |
| Peak simultaneously retained GPU recordings | 2 |
| Remaining GPU recording references at stop | 0 |
| Scoped jitter injections/restorations | 312,038 / 312,038 |
| Runtime faults / request collisions | 0 / 0 |
| Native-output pointers observed in sampled status | 32 |
| Native fallback skips / history resets | 13,189 / 42,961 |
| Final pending callbacks / private recordings | 0 / 0 |

All hooks disabled cleanly. Thirty-two distinct sampled output pointers exceed
the former 16-entry destination limit. The references no longer accumulate across
the whole session: all 297,110 injected recordings were safely released under
the Reset-and-completed-fence policy, with a peak of two live entries.
This is direct in-game evidence for the ownership fix described in
[the implementation contract](../runtime/continuous-output-retirement.md).

## Session and evidence

- Process 20320, creation 134349063362149844, version 1.18.4.
- Arm tick 17001843, approximately 17:27:10.024 +03 on 2026-09-26.
- Duration stop requested at tick 20601953 (110 ms after its 60-minute deadline).
- Collector 3840 finished at 18:27:11.150 +03. No fault observed; one partial
  status read retried, no log-read retries. Largest snapshot gap 5,094 ms.
- 10,785,522-byte timeline; 719 snapshots, all verified. File-observation times
  are not exact video timestamps or an inventory of every output allocation.
- Windowed 2560x1440, DX12, swap-chain format 87, adapter LUID [0,98931].
  Native DLSS Off, TAA On, bare offline profile.
- User retained the Steam recording; no video export was supplied or reviewed
  for this run. The visual result is the user's report, not pixel analysis.

Evidence root: `local/dlaa-output-retirement-20260926/v1.18.4/` (ignored),
**323 files** in the final archive. All 296 prelaunch files / 221 frozen source
identities reverified before sealing.

| Identity | SHA-256 |
|---|---|
| Diagnostic DLL | `044bede4ad9efca0d74e952988b4e06d22a708e52335ccc8e9eced3fac14420c` |
| Prelaunch manifest | `6bfde9d707aa0bade3450f31a46842d265433a6215a95c3b778f90514060e4d8` |
| Final native log | `776405bd7c1fd4896e0be3545fe3baffcae7f8078d56d5b77c1f432444482b23` |
| Startup log | `d05b2dd56ab5c6cbdf57540ff47b7bfc51e2a95daf07f7b2361258a55223e4b8` |
| Timeline | `540d7a478f87c0297474ce483fd1e6e6d97db7dbda7d9981288a69f7fe25f365` |
| Final archive manifest | `51a68aa6900c14e9eae909f7bfdf0c3470e03da16cd6f9d399acd5397b6f6981` |

Game closed before rollback. At **18:31:40 +03**, restored exact previous DLL
`689bef7cf00ff71e4518f452f0467edf6c3d877f2a6dd090553388d8b5f288cc` and removed
the test sidecar. Graphics, modlist, profile and mode hashes are unchanged. No
original game assets changed. Game and collector are stopped; no new test installed.

## Remaining scope

Native TAA still forwards once and skipped/unqualified frames use native rendering.
The agreed next feature work is safe native TAA bypass on replacement frames,
retaining fallback and diagnostic coverage, then DLSS upscaling and FG/MFG.
The earlier request-slot collision did not recur; this run does not establish its
cause or an independent fix.

There were 42,961 history resets. Their reason counters overlap: projection
35,624, camera displacement 9,297, frame/epoch gap 1,436 each, interval 193,
view-other 64, initial reset one. These counts do not measure image quality or
prove unnecessary resets; retain them for later targeted history/performance work.
No FPS benchmark, objective quality improvement, general hot unload or coverage
of every device/resolution/swap-chain recreation path is claimed.
