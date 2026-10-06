# Recorded 30–60 minute gameplay test, unchanged v1.18.3

The user proposed ordinary play with a complete Steam recording to expose
intermittent problems and correlate them with gameplay. Use the same diagnostic
DLL as the [successful short comparison](../research/continuous-request-lifetime-result.md).
No rendering/hook/lifetime change is part of this test. The earlier collision
remains unresolved; this is broader reproduction coverage before selecting a fix.

## Collection and timing

`tools/collect_continuous_session.py` runs outside the game. It reads the existing
five-second latest-status file every second and preserves changed, complete
snapshots in a fresh `timeline.jsonl`. Each record includes UTC observation
start/end, Windows boot ticks, elapsed time from the arm receipt, a raw-file hash
and the typed native status. Native log and status reads are capped at 1 MiB.
No process memory reads/writes or new hooks are introduced by the collector.

Match video using the wall clock or elapsed time from the recorded arm time.
These are file observation times, not exact rendered-frame timestamps; native
snapshots can already be about five seconds old. Request-collision/bypass ticks
use the same boot clock and provide a closer event marker when available. System
clock adjustments do not change the monotonic elapsed-time limit.

The collector keeps a fresh create-only directory and a maximum 128 MiB timeline.
Partial writes are retried; repeated identical snapshots are not duplicated.
It notes 30 seconds without a changed status as stale telemetry, not a proven
game hang. Native terminal status, faults and partial final logs after process
exit are preserved. Full runtime review remains separate; a successful collector
exit does not imply the rendering test passed.

At 60 minutes from arm, a filled timeline, or an explicit `stop.request` file in
the collection directory, send the existing generation-qualified normal stop
event once. Wait up to 60 seconds for native closure and record an incomplete
result if it never arrives. Never terminate or restart the game. A collector
exception also attempts the normal stop event. A native diagnostic fault stops
DLAA under its existing policy; later normal-looking gameplay may be native TAA.

The helper is launched hidden with its command/output paths recorded locally.
It does not require an active chat turn. It writes evidence; it does not promise
live notifications. The user may reply from the desktop. Keep the same Windowed
2560x1440 bare offline profile with native DLSS Off/TAA On for comparability.

## Procedure

1. Verify the previous archive and exact DLL, preserve the destination DLL and
   sidecar, deploy to the usual MO2 slot, freeze sources and seal preparation.
2. User starts/preserves a complete Steam recording, launches the usual MO2 test
   entry and loads a save. Verify startup/provider/process generation, arm, then
   start the collector for that exact generation. Save a healthy baseline and
   record the local clock/arm offset before telling the user to begin the session.
3. User plays normally for 30–60 minutes: movement, combat, menus and travel can
   happen naturally. No scripted sequence or phone replies are required. Keep
   resolution/window size and the diagnostic profile unchanged for this run.
4. If an image problem, crash or prolonged freeze occurs, preserve the recording
   and note its approximate time. Do not automatically relaunch after a crash;
   collect existing evidence first. If everything looks normal, report when done
   before exiting so the diagnostic can drain and stop cleanly.
5. Signal normal stop if still active, verify final fault/SDK/request histories,
   GPU completion, jitter restoration and hook closure. Then normal game exit,
   exact DLL/sidecar restoration, configuration comparison and final archive.

A clean hour improves coverage, but does not prove all lifecycle paths or fix the
old fault. Resource tracking remains bounded and process-lifetime retained, so a
long session can legitimately reveal a capacity limit. Native TAA still runs.
Recording and prototype overhead mean this session is not an FPS benchmark.

Evidence root: `local/dlaa-soak-20260926/v1.18.3/` (ignored). Use that root's
`deploy.ps1 -Action Restore` with the game closed for rollback. Previous
request-lifetime diagnostic evidence remains immutable.
