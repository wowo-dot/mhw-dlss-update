# First continuous live Performance DLSS output

## Run and demonstrated behavior

2026-09-29, v1.25.2, PID 21704, creation 134351205854423564. The user
reported a loaded save, then a clean still/moving scene with normal HUD/text.
The test rendered 1280x720 into 2560x1440 with native AA and legacy DLSS Off.
The installed DLL SHA-256 was
`3b6d3174215ee62343af6e9da4a5033c10fbfb18743bc145227aa313f2d36627`.
Host, provider, adapter and code identities remain those of
`continuous-scaled-output.md`; no new native target was introduced.

The run completed 6,901 successful SDK evaluations, 6,901 expanded copies
and 20,703 downstream effect draws. All 7,133 published plans were built;
there were no unqualified plans or final-quad rejections. No runtime,
SDK, activation or stream-route fault was recorded. Retained temporal history
was used. This establishes continuous live reconstruction/output delivery,
not completed transition coverage or artifact-free image quality.

## Menu-opening skips remain

The user subsequently reported brief skipped reconstruction frames and
identified opening menus as a reproducible trigger. A video clip is pending.
There were 232 unfiltered frames, grouped into 22 suspension periods; all
22 resumed. Of those frames, 230 failed the current-frame depth-copy lease
check, and two failed the later stream-route readiness check. Saved missing
lease samples contain a zero list/generation/frame/depth tuple. Native depth
destination identity itself was accepted, and the downstream plans remained
qualified. Thus this is missing private depth-copy admission, not a permanent
SDK failure or a repeat of the final-source flag-width defect.

The trace does not yet establish which earlier depth condition fails during
menu opening: producer publication, request-to-writer matching, draw admission,
resource state, or lifetime selection. Do not weaken those checks or reuse an
old depth copy without frame/state evidence. Next diagnostic should attribute
those stages for a controlled menu open/close, separating native writer absence
from a recorder rejection. Area-load/focus tests were paused when the user
reported this symptom and are not credited to this run.

The history trace also recorded 262 resets, mostly camera-displacement events.
They are a separate observation and not proven to cause the reported skips.

## Stop and verification

An explicit stop was announced and sent at tick 34001750. It completed healthy:
all 6,903 primary GPU signals completed, all 7,134 jitter changes were restored,
all 6,901 stream recordings were released, full scale returned, and all hook
groups reported disabled. Read-only process checks verified all 35 native code
gates and all 17 native hook entry points restored, including the eight
activation hooks left installed by v1.25.1. COM hook removal is covered by
runtime receipts, not those 17 native byte comparisons. The user confirmed a
normal scene and normal game exit. Keep the current diagnostic DLL installed.

The full reviewer initially rejected the run because it inherited DLAA's
assumption that dispatch reason 7 means only a rejected depth lease. Scaled
v1.25.0..2 also use that code for stream readiness after a successful lease.
The version-specific review now partitions these counts: rejected leases plus
additional scaled-readiness skips, bounded by passed leases not evaluated.
It retains exact equality for DLAA logs and rejects undercounts, excess counts
and rollback. Fifteen depth tests and 57 broader reviewer tests passed. The
corrected full review accepts runtime/GPU ownership telemetry while explicitly
leaving pixel readback, temporal quality and full transition stability unverified.
No native binary was changed to make that review pass.

Evidence: `local/continuous-dlss-20260929/v1.25.2`, including the raw log,
timestamped collector, baseline/movement/intermittent snapshots, user report,
stop receipt, native-code restoration checks, corrected review and its tools.
