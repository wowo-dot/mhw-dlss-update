# DLSS panning: distance-only history comparison (v1.25.12)

The v1.25.7 isolated pan added 241 displacement resets while SDK evaluations
continued and the skip counter stayed unchanged. The user reported brief visible
skips. The successful v1.25.11 sauna run still recorded 280 displacement resets;
it had only two pipeline warmup skips. These are observations, not proof that
every visible defect comes from history resets.

The current threshold is 20 game units per evaluated frame, independent of time.
Orbiting the third-person camera moves its world-space position. A smooth orbit
can cross that threshold, especially at the diagnostic build's low frame rate.

v1.25.12 changes one decision in the scaled runtime: when the complete reset
mask is camera_displacement alone (1024), keep history and count the reset that
the control policy would have requested. First evaluation, frame/epoch gaps,
camera/view/depth identity, viewport, projection, interval and large rotation
reasons are preserved, including when combined with displacement. Invalid
matrices still reject admission. DLAA retains its previous policy. No hooks,
resource handling, depth timing or downstream/sauna shaders change.

This is explicitly a diagnostic comparison. A pure translation teleport also
matches the removed rule; this is not a finished camera-cut detector. Do not
interpret clean panning as proof of correct cuts, travel or all transitions.

Telemetry schema 3 uses policy distance-only-comparison-v1, diagnostic_only,
control_resets and displacement_only_resets_suppressed, with bounded first/latest
typed examples. Native tests check all retained reasons and matrix rejection;
an independent Python solve recomputes decisions and checks accounting. Real
bounded examples are replayed with all identities, time and projection bits.
Synthetic parser tests are marked as such and provide no runtime evidence.

Evidence: local/scaled-pan-history-20260930. Prior sealed evidence:
local/scaled-haze-queue-completion-20260930, archive SHA-256
2e3777d5f2cc26172db5fd2ef4d09856c542f915589e6f43aece7c7b10880190.

Live comparison: same bare profile, DX12, 2560x1440 windowed, Performance
1280x720, native AA/DLSS/MotionBlur Off. Load outside sauna, keep menus closed
and camera still for the baseline; then slow and fast pan without traveling.
Compare actual resets, suppressed control resets, skips and output/depth
rejections against the visible result. Stop after this focused test and verify
GPU completion, full-scale restoration, jitter restoration and native bytes.

The build remains unarmed until the normal arm event. Preserve the exact current
DLL and output sidecar before installation. Keep the latest DLL installed as
requested. Manual removal of this experiment, only after the game closes:
run local/scaled-pan-history-20260930/deploy.ps1 -Action Restore to recover the
preserved previous DLL and sidecar; it does not alter game assets or settings.

## Live result, 2026-09-30

PID 45600, creation 134352014002467634, DLL SHA-256
91b777dedbf4ad4f502ac5c090d3cbcea023ffd29f33e0fb8896505970a3ebf7.
Startup passed on RTX 5090, LUID [0,94196], D3D12, windowed 2560x1440,
swapchain format 87; Performance input 1280x720. Native AA/DLSS stayed Off.

| Checkpoint | DLSS evaluations | Actual resets | Control resets | Suppressed distance-only resets | Skips |
| --- | ---: | ---: | ---: | ---: | ---: |
| Still baseline | 1,154 | 1 | 1 | 0 | 1 |
| Slow/fast panning report | 2,763 | 2 | 537 | 535 | 13 |
| Intentional stop | 3,125 | 3 | 780 | 777 | 22 |

User reported **clean while panning**, then **normal scene and normal exit**.
All captures and history examples passed independent review. No runtime or SDK
fault occurred. The three actual resets were first evaluation plus two combined
resets involving frame/epoch gaps, interval and displacement; one also included
the rotation threshold. The diagnostic did not suppress these combined reasons.

The 22 skipped evaluations comprise one initial output-pipeline warmup and 21
consumer-depth admission rejections, all reason 5. This code covers the required
writer/consumer/next-use schedule interval and depth-use representation. The
aggregate reason does not distinguish its individual branches. Twelve occurred
by the panning checkpoint; nine more preceded stop. They are separate from
history resets, remain unresolved, and did not produce a reported visible defect
in this test. Every one of 3,147 published output plans built successfully.

Cleanup passed: all 3,126 GPU signals completed, all 3,148 jitter changes restored,
full scale restored, all hooks disabled. All 3,125 route records released, none
live; 9,375 downstream draws completed. Live read-only verification matched 38
complete code spans and 17 native hook heads before exit. Collector archived 22
events over 112.672 seconds, with seven status-read retries and no log-read
failures. The full capture reviewer accepted the run. No objective pixel-quality
readback was performed; visual quality is the user's report.

This supports the fixed per-frame displacement threshold as a cause of the
earlier pan flicker, but the comparison used different runs, not identical frame
playback. It does not establish that every earlier skip shared this cause.
Next: replace the distance-only heuristic with a justified camera-cut policy
that preserves normal movement, then validate actual cuts. Do not promote this
diagnostic ablation to a final policy or silently relax the separate depth guard.

The game is closed. Exact current DLL and settings remain installed as requested;
the run is sealed by archive-final.json. Use a fresh evidence directory and
preserve this slot/sidecar before a subsequent launch or build deployment.
