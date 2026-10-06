# Broad run: depth rejection, effect rectangle and output fault

2026-09-30, v1.25.19. Evidence: `local/depth-interval-detail-20260930`.
Follow-up to [depth-interval-diagnostics.md](depth-interval-diagnostics.md).

The run failed after 11,092 successful DLSS evaluations and 15,954 spatial
fallback frames. The user requested continued testing beyond the smithy;
recording continued through rain and Fatalis's nova until an actual output-path
fault ended the test. There was no assistant/collector stop request, timer
expiry or recording-size limit. The collector ran about 16 minutes 18 seconds.

The selected `barebonesdlss` profile had Stracker's Loader enabled by the user.
Its loaded modules and hashes are recorded separately from the earlier bare
installation baseline. Activation of additional gameplay modifications was not
independently verified. Native AA/DLSS/MotionBlur remained Off, with a
2560x1440 window and 1280x720 prototype render extent. Preserve the user's
Stracker configuration; do not restore the original bare modlist.

## Depth rejection shared by smithy and rain

The user saw the sustained DLSS-off appearance in both locations. There were
15,956 reason-6 depth interval failures and two reason-5 schedule rejections.
The added bounded receipts identify opcode-10 nested packet state-list reads,
usually site 23 (backing resource), occasionally site 21 (next-node pointer).
Invalid partial node data is not evidence of a valid resource or state.

The pinned native opcode-10 executor processes its own barriers, then chooses
between a direct copy and a child draw. Only the latter processes the child's
state list. Our reader currently traverses the child unconditionally. An unused
child state-head field is a plausible common cause; this has not been proven
against an exact failing parent snapshot. External bounded reads did not
capture the needed opcode-10 packet and must not be used to grant admission.

A correction must mirror the native branch conditions and preserve parent
transitions, qualified writer-child requirements, ordering and resource-state
checks. Ignoring arbitrary invalid child pointers would not establish safety.
The pinned disassembly, hashes and branch observations are in the local run's
`copy-branch-investigation.md` and accompanying artifacts.

## Nova/cutscene rectangle report

The user supplied a nova screenshot and reported a similar artifact during an
earlier cutscene. They described it as a 16:9, 720p-sized rectangle/bar that
appeared only during those effects. This suggests checking whether an effect's
viewport, scissor, copy rectangle or sampling extent remains at render size
when its destination is output size. It is a hypothesis, not an identified pass
or proven root cause. The exact screenshot frame is unknown; report-time
status snapshots are explicitly not frame-exact correlations. The user was
asked to preserve Steam recording; no exported path has yet been provided.

The existing depth rejection was already present at the nova lead-in. Keep it
separate from the rectangle report and the later terminal failure.

## Output fault and shutdown

The collector first observed runtime fault 46 at 08:43:45.325 local time.
The original stream fault is 6, `stream draw order or state`. The combined
guard in `record_effect` covers recording identity, destination resource/state,
raster target and pipeline/root identity; current evidence does not identify
which predicate failed. Do not attribute it specifically to the nova or the
reported bar without further evidence.

The stopped log is unhealthy, and the frozen full reviewer rejects `clean
stop`. This result is preserved. Independently, the separate shader accounting
and bounded depth receipts validate across 18 native status records and 195
collector snapshots. Both shader channels end at 27,051 accepted checks, and
dispatch checks match dispatch acceptance; the previous accounting discrepancy
did not recur in these retained samples.

Fault shutdown restored full render scale and all 27,503 jitter changes.
All 27,048 private GPU signals completed. Independent read-only inspection
verified all 38 pinned function spans and 18 native hook heads restored.
One private and two downstream recording records remained pinned until process
exit; they were not reported as released. The user closed the game, and process
absence was verified. Cleanup verification does not make the run accepted.

All 570 prelaunch files and 540 frozen sources were checked before adding this
result note. The installed v1.25.19 DLL, sidecar, exact previous-DLL/sidecar
backups and runtime configuration hashes remain unchanged. No deployment or
live code mutation was performed during this investigation. The evidence root
is sealed by its final file manifest. Prepare a fresh root before relaunching.

Next work is to establish the native child-execution rule for depth admission,
isolate the failed output-state predicate, and trace effect extent propagation.
No new test build is prepared. Last fully accepted live version remains v1.25.16.
