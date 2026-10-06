# v1.25.25 broad DLSS regression, 2026-10-01

The 29-minute run completed with clean shutdown and no runtime fault, while
reproducing visible compatibility defects. This is a useful failure capture,
not a claim that DLSS is ready across all scenes. The user confirmed normal
appearance after stopping and a normal game exit. The current DLL remains
installed.

## Configuration and evidence

DX12, windowed 2560x1440 output, 1280x720 scene rendering; native AA, native DLSS
and MotionBlur Off. The barebonesdlss MO2 profile includes Stracker's Loader.
The renderer binary was unchanged from the successful targeted cutscene test.
PID 17352, creation 134352828027431998. DLL SHA-256:
`e84e443bfd8a32f62b892681a6e0949c6babd1912378999158d7c0d444bc117c`.
Host SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Startup verified D3D12, swap-chain format 87 and adapter LUID 0:93877.

Private evidence is under `local/dlss-long-run-20261001`: original build-time
sources, separately frozen host collector, prelaunch/runtime provenance,
timestamped status timeline, checkpoints, reports and the two original PNGs.
Clipboard image dimensions include window edges and do not define the
swap-chain extent. The Steam recording was requested for preservation; no
exported replay from this run has been supplied yet.

The host collector gained an optional 1 GiB capacity and streamed final hashing
before this test; its previous 128 MiB default remains unchanged. Twenty-three
collector tests passed, including a simulated full hour above the old limit.
No C++ renderer change or binary deployment occurred for this run.

## Observed defects

| Report | Evidence | What remains uncertain |
| --- | --- | --- |
| Gallery cutscene, supplied title `trust hunbting companion` | Sustained suspension; graph reason 6/detail 4. A non-haze stage-15/order-26005 pass, type RVA `0x3526650`, connects the motion output to a different grading input. Leaving Gallery restored DLSS. | Exact display title and shader/auxiliary-coordinate contract have not been independently verified. |
| Wingdrake travel | Controlled repeat showed a steady baseline, suspension around 02:19:57 local, recovery by 02:20:06. The report interval added 969 skips and 969 overlay rejections. Rejected graph contains order-4100 overlay, type RVA `0x3544910`, reason 17/detail 0/qualification 0. | The exact failed command/guard remains unresolved. Some intermittent rejections continued after travel. |
| Safi'jiiva attacks, Alatreon frost attack and nova | User-reported skips coincide with further order-4100 overlay rejections. The nova's retained graph also includes the supported order-20000 haze pass. Reconstruction subsequently resumed. | Exact attack-frame alignment is not proven. A shared rejection signature does not identify the same sub-guard in every case. |
| Occluded hunter silhouette | Two user screenshots show a small white silhouette toward the upper-left while the HUD spans the view. Run dimensions confirm a 2:1 output/render relationship. | Render-to-output coordinate/extent mismatch is a hypothesis. Its exact draw and any relationship to order-4100 rejections are unproven. |

The timestamps above are Europe/Istanbul (+03:00). The host timeline observes
status approximately every five seconds; it is not a frame-exact video capture.
An instantaneous `waiting=false` checkpoint does not imply the preceding
interval had no skips. The timeline retained intermittent suspensions between
otherwise successful checkpoints.

## Limits of the read-only draw capture

The two-minute wingdrake reader performed 457 bounded attempts, 5,796 reads and
523,907 bytes of process reads. It obtained no stable detailed draw snapshot.
Seventeen attempts reached the overlay but failed the reader's requirement for
one to eight opcode-0 draw packets. Actual opcode/count values were not saved
before that check. The remaining attempts saw no active overlay, a changing
primary view, or an invalid transient filter count. No process writes,
suspension or remote execution occurred.

This limitation must not be presented as proof of indexed drawing, a stride
failure, or a captured shader. The native graph rejection is independently
preserved. Several native branches can emit reason 17 with qualification zero.

## Final verification

- 40,178 successful DLSS evaluations and 11,751 skipped/unfiltered attempts.
- Zero runtime/stream faults and zero request collisions; all 80 recorded
  suspension periods resumed.
- 9,301 overlay rejections; 4,476 admitted overlay draws completed, including
  56 draws with the previously verified 28-byte layout.
- All 51,929 GPU signals completed. All 51,928 private recordings and 47,336
  downstream recording records were released; none remained live.
- All 81,817 jitter modifications restored, full render scale restored, all
  hooks disabled. Read-only live comparison verified 38 complete native
  function spans and 18 hook heads restored before normal exit.
- Frozen full review accepted lifecycle/telemetry. Its temporal-quality and
  full-transition-stability claims remain false.
- Collector ended at the requested stop after 1,758.891 seconds, with 351
  snapshots/events and 55,989,685 bytes of timeline. Nine retried status reads
  caused no collector fault; no game termination was performed.

The full-run deltas include multiple transitions and effects; do not assign
every skipped frame to the last named attack. The final dispatch counters
contain 11,750 readiness/input skips and one state-admission skip. Detailed
depth/readiness counters are preserved for later analysis, including 54
rejected consumer-depth attempts. These do not supersede the visible issues.

## Next implementation work

First capture the generic rejected overlay command sequence before filtering
by opcode or draw count. A bounded receipt at the existing native preparation
hook can retain command kinds/counts and the exact rejecting branch without
depending on a brief external polling window. Preserve ownership, ordering,
resource-state and shader validation; do not remove the rejection blindly.

Then extend the shared downstream contract to the Gallery's intermediate color
pass once its inputs and coordinate domain are established, and trace the
hunter silhouette's render/output extent separately. The diagnostic selectors
above identify evidence, not monster-specific fixes. Retest the reported
transitions after implementing generic handling. Keep the current renderer
installed until the next prepared replacement is ready.
