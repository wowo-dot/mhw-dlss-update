# DLSS broad play test, 2026-10-02

The v1.25.42 test ran for 43.14 minutes and completed 44,351 DLSS evaluations
without a runtime fault or permanent shutdown. The user reported a solid image,
brief skips during area changes, and the previously known undersized hunter
silhouette. This supports continued development, with compatibility and diagnostic
follow-ups preserved below. It does not establish uninterrupted reconstruction.

The trace recorded 2,061 skip attempts, including 2,052 output-plan rejections.
All 21 suspension periods recovered. The rejected commands fall into two recorded
shapes: dependency discovery (1,189 observations) and native depth-of-field
qualification (863). These are attempts rather than displayed-frame counts.
The first recorded instances were around 02:55:35 and 02:59:59 local time;
the last retained graph rejection was around 03:18:24. Exact actions remain
uncorrelated. No new skips appeared between the 03:20:39 checkpoint observation
and the intentional stop, which added one final stop-time attempt.

The supplied Steam export is 15 minutes 12.74 seconds at 2560x1440. It contains
startup, Gallery navigation and multiple cutscenes. Sampled Gallery screens show
The Voyage Begins, The Beast Descends and Commence the Lure Operation. The
overview images do not show an obvious recurrence of the dark rectangle, but
thumbnail sampling cannot establish DLSS activity or temporal quality. The export
does not cover the entire 43-minute run. Same-seek comparisons against the Steam
background recording did not establish exact alignment; filename time is not
treated as a video clock anchor. Preserve the original export for targeted review.

The intentional stop completed all 46,277 main GPU signals. It released 46,276
main recordings, 2,057 unfiltered recordings and 45,913 output recordings, restored
all 144,957 jitter changes and full render scale, and disabled the hooks. Read-only
live checks matched 40 complete native code spans and 18 hook heads. The user
confirmed a normal scene and normal exit. The DLL remains installed; the four
recorded configuration files remain unchanged.

Final component checks pass in all 14 channels with a separate, tested correction
to the diagnostic reader. The original frozen reader incorrectly expected no key
or packet for a discovery failure; v1.25.42 deliberately records the failed draw.
The correction accepts that bounded native draw identity while retaining the
historical form, and rejects malformed receipts. Five new tests and 18 existing
diagnostic tests pass. The renderer DLL was neither rebuilt nor replaced.

The complete review remains incomplete. After the diagnostic-reader correction,
the full native log fails the state rejection-stage check: two temporal-rejection
samples contain missing local transition proofs for all three roles. The raw
barrier proofs replay correctly, but their semantic classification needs analysis.
Among 506 timeline snapshots, one also fails the unfiltered completion-order check.
Both discrepancies are saved without weakening those checks. Final GPU retirement,
native restoration and zero runtime fault are independently verified; these do
not resolve the intermediate diagnostic discrepancies.

Evidence is in `local/dlss-broad-run-20261002`, PID8968,
creation134353722167009226. Original frozen failures, corrected reader source,
terminal checks, complete timeline and recording identity are preserved separately.
The known hunter silhouette remains the next visible rendering issue. Keep the
recovered dependency/DOF interruptions and diagnostic consistency checks in the
follow-up queue. A fresh output directory is required before another test launch.
