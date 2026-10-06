# Native AA-Off postqueue result — v1.20.2

Closed and restored 2026-09-27 04:06 +03. The 228.578-second run passed the
continuous reviewer: **15,903 qualified DLAA replacements with native AA Off**.
User reports were clean for the still scene, walking/panning, sauna walking and
reentry, an area loading screen, and a windowed Alt-Tab cycle. The assistant
announced and intentionally stopped the diagnostic after the final checkpoint.

PID 56980 / creation 134349443685469575. Host version 0.0.0.0 SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Installed/loaded DLL SHA-256
`f6a56c8ea444f1bd7bb4bef621b11fd02591048b6d154cef6fa14b76513795c9`.
Exact module paths/process/startup, 326 sealed prelaunch files, 300 frozen sources
and four configurations were verified before arming.

## Observed behavior

| Checkpoint | DLAA replacements | User observation |
| --- | ---: | --- |
| Baseline | 1,226 | Save loaded |
| Walk/pan | 4,510 | Still and moving scene clean |
| Sauna/reentry | 7,723 | Stayed active and clean throughout |
| Area loading | 12,093 | Stayed active and clean |
| Windowed focus cycle | 15,162 | Returned clean |
| Intentional stop | 15,903 | Normal exit later confirmed |

All 17,655 camera scopes observed the native AA option Off. The native queue
received 17,076 TAA registrations and invoked all 17,076 preparations; both
comparison overrides ran 17,076 times. This exercises insertion before native
sorting and main-context preparation. It does not require relaxing worker-context
identity or writing the saved AA choice/native layer enable flags.

There were 579 rejected queue scopes (578 in the registry category, one native
layer-filter rejection). Their count increased between the sauna and area-load
checkpoints and remained unchanged through the focus checkpoint. DLAA evaluations
continued afterward with zero faults/collisions. Grouped reason counters do not
identify the precise absent/invalid registry condition; no stronger claim about
individual loading frames is supported by this capture.

Cleanup passed: all 15,925 GPU recordings were submitted, completed, retired and
released; live recordings and resource references were zero, all 17,655 jitter
changes restored, callback counts/scopes drained, both hook sets disabled.
History recorded 1,523 resets and 14,380 retained evaluations. These telemetry
checks complement the user's visual observations; no new pixel readback was taken.

## Remaining scope

This is still a qualified diagnostic replacement (`exclusive_mode=false`).
**1,172 unclaimed resolve candidates forwarded native TAA**, so this run does not
prove the final requirement that native TAA never computes while modern AA owns
rendering. No claimed SDK evaluation failed. Finish unfiltered suspension and
latched-failure behavior without silent native-AA fallback, preserving the working
postqueue preparation and GPU path. Existing-entry reuse and removal of competing
AA currently have owned-fixture coverage only; other saved AA choices still need
live validation. CMFM/UI, scaling DLSS and frame generation remain later work.

## Archive and restoration

Evidence: `local/native-aa-ownership-20260927/v1.20.2/`.
353-file final archive SHA-256:
`aefcf378556f4f0f100ade49260b43c8c716e11b635829eca065692a2e566bf4`.
Final log SHA-256:
`087049f1074ac6c589e7fc8e3d155babdee54c4ec40091e24d7b7196efbe8da8`.
The user confirmed normal exit; process absence was independently checked. The
exact prior slot DLL was restored (SHA-256
`689bef7cf00ff71e4518f452f0467edf6c3d877f2a6dd090553388d8b5f288cc`), and the
output-dir sidecar removed to match its previous absence. No settings were written;
all four configuration hashes stayed unchanged. Native AA/DLSS Off, Windowed
2560x1440, DX12. No next build is installed and no further live action is pending.
