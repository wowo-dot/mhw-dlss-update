# First continuous DLAA prototype run — v1.18.0

On 2026-09-26, run 18336 completed 20,802 full-resolution DLAA evaluations and
native output handoffs at 2560×1440. The user reported, “It looks clean asf 😍”,
then ended the test and confirmed normal game exit. The continuous telemetry
review passes. All private GPU submissions completed, all scoped jitter writes
were restored and all hooks were disabled with zero faults or pending callbacks.
The arm-to-stop request interval was 237.296 seconds.

This is the first successful continuous prototype. The visual assessment is the
user's report; this build performs no per-frame GPU pixel readback. Earlier
one-frame live capture and offline streaming fixtures independently verified
packing and bindings. They do not establish this run's temporal image quality
or complete lifecycle stability.

## Terminal evidence

| Measurement | Result |
| --- | ---: |
| DLAA evaluations / depth copies | 20,802 / 20,802 |
| Private GPU signals / completed | 20,802 / 20,802 |
| Jitter injections / restorations | 20,876 / 20,876 |
| Skipped attempts | 73 |
| History resets | 4,566 |
| Tracked command lists / resources | 21 / 6 |
| Faults / pending callbacks | 0 / 0 |
| Pending private recording | None |
| Hooks | Disabled |

Startup modern initialization and capability review passed for the same process
generation. The final SDK status reports successful Create/Evaluate, output copy
and binding restoration, DLAA mode 5, preset K (11), flags 75, unit motion scale
and exposure. Swap-chain format is 87; adapter LUID is [0,97816]. The prototype
retains its bounded resources and SDK feature until process exit by design.

The 4,566 history resets (about 22% of evaluations) warrant investigation.
View-matrix jump thresholds or exact projection comparisons may overreact during
movement, but reset reasons are not present in this sparse log. A planned bounded
read of retained history did not complete before game exit; it produced no live
history evidence. Static symbol resolution alone does not establish a cause.
Add reset-reason accounting before changing policy, then exercise camera cuts,
menus/cutscenes, alt-tab and resource re-creation. No next test is installed.

## Preservation and rollback

The game process was observed closed and the user confirmed normal exit.
Exact rollback completed at 04:49:44 +03. The previous diagnostic DLL is restored,
the output sidecar removed, and all four configurations match their prelaunch
hashes. Native DLSS remains Off, TAA On, with the bare profile unchanged.

Evidence root: `local/dlaa-continuous-20260926/v1.18.0/` (ignored).
The final manifest covers 227 local files and 48 referenced sibling offline-check
files. All 207 prelaunch entries, including 196 frozen source files, were verified
unchanged. The post-run reviewer is frozen separately; its 13 corruption checks
cover identity, SDK parameters/handoff, faults, incomplete work, unclosed hooks,
unrestored jitter, counter rollback and truncated logs.

- Process: 18336 / creation 134348604138185436.
- Host version 0.0.0.0, SHA-256:
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- Tested DLL:
  `cbd768404b2d752f39256282dfaa27abb3b6e8f10d6eefa3dfee0d48580782e6`.
- Continuous log:
  `ed374955a767c2c6c7af71a7e6a291b716a58b7d2a98d4dd198b7cd3280bda84`.
- Startup log:
  `dbe08fa8c23b3106061b5bc975647d359b6a4cc49e4821da4096c12fb985f06c`.
- Prelaunch manifest:
  `9817f52d4d5b50d94e5d92a4410aed6764073a2c9f8d5c3cb59e7472049aebf3`.
- Final archive manifest:
  `e8f68af76572ec0db4119aebb8a7b9ec976d969f0f0c152f53480af7850685f0`.
- Restored DLL:
  `689bef7cf00ff71e4518f452f0467edf6c3d877f2a6dd090553388d8b5f288cc`.

Review the stopped local log with `tools/review_continuous_dlaa.py`, supplying
the matching startup log, tested DLL hash and a new output path. Its acceptance
explicitly distinguishes verified continuous telemetry from unverified live
pixel readback, temporal quality and full transition stability.
