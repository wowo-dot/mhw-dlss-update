# Recorded gameplay stopped at output-destination capacity

Run 51280 / creation 134349028442589442, 2026-09-26, used the unchanged v1.18.3
DLL in Windowed 2560x1440 DX12 with native DLSS Off/TAA On. The intended 30–60
minute test stopped after about **11 minutes 16 seconds** and 45,964 successful
DLAA evaluations. The user noticed increased FPS and supplied the Steam replay.

The diagnostic recorded fault 14 with failure string `presentation destination
capacity`. `Output::bind` in `src/temporal_present/output.cpp` retains up to 16
distinct native destinations in continuous mode. A new destination with that
array full throws before this call begins SDK evaluation. `dispatch_after`
turns that failure into fault 14 and stops admission. Native TAA had already
forwarded. This is a prototype retention limit, not a GPU-memory-exhaustion or
vendor Evaluate error. The last Evaluate result remained success.

All 49,801 published TAA requests matched their consumers. There were no request
collisions, changed/multiple matching packets, publication-blocked consumers or
reentrancy. The original fault 6 remains unresolved and did not recur here.

## Replay correlation

The provided export lasts 952.049 seconds. Visual samples show hub equipment and
quest menus, the hunt, returns/loading, and another quest departure near the stop.
Steam's matching source recording is `bg_582010_20260926_132730`; its separate
timeline epoch is 13:27:24 UTC. No account identifiers are included in evidence.

The export is offset from the source by **5.080 seconds**. Normalized audio
correlations at export 100 and 760 seconds scored 0.9706 and 0.9762 and agreed
within 0.000125 seconds. This resolves relative export/source timing. Absolute
wall-clock alignment uses the recording directory's UTC time and is approximate;
five-second diagnostic snapshots are also observations rather than exact frames.

Last healthy observation was 16:40:44.892 local, first fault observation
16:40:49.929. Their nominal export positions are 13:09.812–13:14.849. The inspected
sequence is departure animation at 13:09–13:11, fade at 13:12, loading at
13:13–13:15, and the new quest at 13:16. Thus the fault corresponds to another
hub-to-quest departure/loading transition. Do not assign an exact failure frame.

Tracked resource totals rose from 6 to 7, 10, 13, 16, 19 and finally 22 during
the run. This registry includes different resource roles and is not the output
destination count. Fifteen output identities appear in the sparse snapshots;
the exact source guard establishes that the 16-entry destination list was full,
even though not every short-lived identity appears in a saved snapshot.

## Closure, evidence and next step

The standard reviewer rejects the unhealthy stop; this run remains failed.
Separate closure review passed: all 45,980 GPU signals/depth copies completed,
all 50,294 scoped jitter injections restored, zero pending callbacks/private
recording, all hooks disabled. Final totals include 13,468 history resets,
3,837 skips, 21 lists and 22 pinned resources. These were not independently
profiled for quality or performance. Native TAA still runs.

The external collector preserved 136 snapshots totaling 1,953,385 bytes and ended
on native terminal failure, without hitting its duration/storage limit or sending
a stop request. Every saved snapshot passed request/history metadata review.
The game was observed closed after the user supplied the export; no explicit
normal-versus-crash exit classification was supplied. Exact previous DLL
`689bef7c...` was restored at 16:50:07 +03, sidecar removed, all four configurations
unchanged. The user's windowed setting remains preserved.

Evidence: `local/dlaa-soak-20260926/v1.18.3/` (ignored).
330 local files plus one external-video identity, final archive SHA-256
`78b6a6fbeaf3bcae9ea3a770faeb5ce0e145c3cc952b88c850b50c0151f6aaca`.
All 248 prelaunch files / 215 frozen sources reverified. Final native log
`5f075568aae529bc9190f24011cbbdad0adadea7214abf1f5da708d9fe7e6a85`.
Timeline `8d4942fcd588ee65b29a56e6cd9a986b9e231a0efadf20aca0749c6de8caf8c4`.
Export SHA-256 `78d3d010282243306f2b012fbe9c3d4eef785ee14753cd9806a6f295d98351bd`;
the original 3.76 GB video remains in the user's Videos directory, not copied
into the repository. Thumbnails/metadata are local ignored research artifacts.

Next implement and verify safe native output retirement/reuse across transitions.
Coordinate the output handoff's references with runtime pins and completed GPU
submissions; clearing one array while another retains all old resources is not a
complete lifetime solution. Validate more than 16 distinct outputs, multiple
frames in flight and queue changes before repeating the long session. Increasing
the array alone would postpone exhaustion. The original request lifetime issue
and qualified-frame native TAA bypass remain on the agreed plan; no fix is
claimed by this diagnosis.
