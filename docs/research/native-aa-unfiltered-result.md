# v1.21.0 — live unfiltered suspension and recovery passed

Closed, restored and archived 2026-09-27 04:42 +03. Game PID 49076,
creation 134349464479679036; executable 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Native AA/DLSS stayed Off, DX12, Windowed 2560x1440, bare MO2 profile.
[Implementation and remaining scope](native-aa-unfiltered.md).

## Confirmed results

The 347.954-second armed run produced 25,008 successful DLAA replacements and
967 unfiltered copies. All 190 suspension periods resumed successfully, with
no runtime/activation faults, request collisions or SDK failure latch.
The copy reasons were scene-depth identity mismatch (80 frames) and an
unavailable/mismatched depth lease (887 frames). These copies replaced the
verified native TAA dispatch with current color; they were not native TAA.

All 25,091 SDK/depth recordings and all 967 independent copy recordings were
submitted, GPU-completed, reset, and released. Both live resource-pin counts
were zero. Both lifetime trackers peaked at two live recordings. All 26,438
projection scopes were restored; 1,049 published zero jitter during waiting.
Scopes, zero-jitter projections, suspension periods and copied frames count
different events and need not have equal totals.

All phase reviews passed. User confirmed clean still/moving scene, sauna
walking/panning/reentry, area loading and a stationary windowed focus cycle.

| Checkpoint | DLAA frames | Unfiltered frames | Recovered suspension periods | Native candidates forwarded |
| --- | ---: | ---: | ---: | ---: |
| Baseline | 2,411 | 2 | 2 | 2 |
| Walk/pan | 6,458 | 17 | 17 | 2 |
| Sauna/reentry | 10,455 | 32 | 26 | 2 |
| Area load | 14,873 | 573 | 78 | 4 |
| Focus cycle | 24,150 | 967 | 190 | 4 |
| Intentional stop | 25,008 | 967 | 190 | 4 |

The native owner inserted 25,980 missing TAA postqueue entries and ran the same
number of scoped preparations/comparison overrides. All 26,438 camera scopes
observed saved AA Off. There were 458 transient queue rejections: 457 grouped
registry failures and one native gate. Their grouping does not identify the
specific missing registry condition. Evaluation continued after transitions.

The assistant announced the intentional stop before signaling it. Final log
review accepted healthy shutdown with all hooks disabled. User confirmed normal
exit; process absence, exact baseline DLL restoration, sidecar removal, and all
four unchanged configuration hashes were verified. No game assets changed.

## Remaining work and limits

Four qualified-source candidates still forwarded native TAA after failing the
raw-color/output resource-state proof (reason 6). Two occurred before baseline
and two by the area-load checkpoint; none accumulated afterward. The current
counter cannot identify which resource/state failed. Add narrowly scoped state
evidence or derive the missing state path before changing its proof. Do not
relax state validation just to eliminate that counter.

Whole-renderer exclusive AA is still unverified: rejected producer publication
and unmatched consumers can also fall outside this candidate counter. Other
saved AA choices, resolution/device changes and latched real SDK failure were
not exercised by this run. Latch behavior currently has offline fixture
coverage. The clean appearance is the user's observation, not live pixel
readback or exhaustive temporal-quality evidence.

The prior v1.20.2 run had 1,172 native-forwarded candidates; runs differ in
duration and scene activity, so this is not a controlled frame-for-frame
comparison. The direct new evidence is that 967 verified copies executed and
all suspension periods recovered in this run.

Next: identify and cover the remaining native-output state cases while retaining
the validated postqueue, DLAA and unfiltered lifetime paths. CMFM/UI remains
deferred; resolution-changing DLSS follows completion of this AA ownership work.

## Evidence

Root: `local/native-aa-unfiltered-20260927/v1.21.0/` (ignored, immutable archive).
377-file final archive SHA-256:
`baae40911fbd60c6593c9841a1bd77c44b0c5c084cb3da011b577513d1575243`.
Final log SHA-256:
`035af88dc6ea83085616b5339b8847f9ac1fbe73163e2e4c1b425d6badcfa385`.
Test DLL SHA-256:
`43690e5830661be4eac4e8dd5d88687d09479749837f010975f57f7e1a6359cf`.
Baseline DLL SHA-256 restored:
`689bef7cf00ff71e4518f452f0467edf6c3d877f2a6dd090553388d8b5f288cc`.

`final-review-49076.json`, `run-result.json`, phase statuses/reviews,
`user-observations.jsonl`, `normal-exit.json` and `restored.json` retain the
receipts. The sealed package includes 306 frozen source files, the DLL and
fixtures, and 37 native / 108 Python passing checks.
