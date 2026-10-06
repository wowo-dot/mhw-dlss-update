# v1.25.7 output callback/submission and filter connection diagnostic

Load the same save with native AA/DLSS Off and wait for arming. First pan without
menus/sauna entry; then test sauna entry and recovery as a separate checkpoint.
New bounded diagnostics distinguish callback identity/order and changed links.
No rendering/admission change is claimed. Keep latest DLL installed after exit.
See ../research/scaled-output-correlations.md; evidence local/continuous-dlss-20260929/v1.25.7.

# v1.25.6 output-readiness diagnostic

Load the same save with native AA/DLSS Off and wait for arming. After the quiet
baseline, repeat brief menu openings at the original and changed camera angles.
The new `scaled_route.readiness` channel identifies the exact rejecting condition;
this revision does not relax admission or claim the remaining flashes are fixed.
See `../research/scaled-output-readiness.md`. Keep the latest DLL installed.
Evidence and exact prior DLL/sidecar backup: local/continuous-dlss-20260929/v1.25.6.

# v1.25.5 consumer-point depth correction

The next test replaces the private depth copy's CPU-order dependency with a
fresh schedule/state check at reconstruction. See
`../research/scaled-consumer-depth-copy.md`. Offline checks passed; the visible
menu-skipping fix is pending live verification.

Use the existing MO2 entry, bare profile and unchanged 2560x1440 window with
native AA/DLSS/MotionBlur Off. Load a save, wait for arming and establish a quiet
baseline. Then open the reproducing menu, inspect consumer-depth counters, and
close it to check recovery. Stop intentionally and verify GPU drainage, full
scale, jitter restoration and native hook bytes. Keep the latest DLL installed.
Evidence, prior DLL/sidecar backup and optional restore script are in
`local/continuous-dlss-20260929/v1.25.5`.

# v1.25.3 focused menu-transition capture

The next run diagnoses the brief menu-opening skips from v1.25.2. Use the same
MO2 entry and settings below. Load a save and wait for arming; after a quiet
baseline, open/close the reproducing menu a few times in the same area.
Do not begin broader travel testing yet. See
`../research/scaled-menu-depth-admission.md` for the bounded trace schema.

Evidence and deployment: `local/continuous-dlss-20260929/v1.25.3`. This revision
adds depth-stage diagnostics to the working continuous renderer. No new hook,
resource-admission change, or SDK parameter change is introduced. Keep it
installed after the run. Its deployment directory contains the exact v1.25.2
DLL/sidecar backup and an explicit restore command, if later requested.

# v1.25.2 continuous Performance DLSS test

The first run is complete: 6,901 live DLSS frames and verified native hook
restoration. Menu opening caused brief missing-depth-copy suspensions that
recovered. See `../research/first-continuous-dlss-result.md`. Keep the current
DLL installed; the next targeted test should diagnose that menu transition
before continuing broader area/focus checks.

Use the existing barebonesdlss MO2 profile and the
`Monster Hunter: World - DLSS 1.2.110 test` launch entry. Keep DX12 enabled,
native AA/DLSS/MotionBlur Off and the 2560x1440 window unchanged. Run outside
public multiplayer. Load a save and report readiness; gameplay hooks wait
for the explicit arm event. Do not enable the old in-game DLSS setting.

The DLL initializes the modern SDK at startup and waits up to 30 minutes for
arming. Once armed, it renders at 1280x720 and reconstructs 2560x1440 continuously
with retained history. There is no frame-count limit. A pause while pipelines
are prepared is possible. Start with a still scene and slow camera movement;
verify live counters before moving to area changes and longer play.

Arm with `tools/arm_continuous_dlaa.py`, the exact process-generation input log,
installed DLL SHA-256 and a new arm receipt. Use its stop action to end the run.
`tools/collect_continuous_session.py` can save timestamped checkpoints; review
the stopped input and startup logs with `tools/review_continuous_dlaa.py`.
The versioned reviewer requires one expanded copy and three output effects for
every successful DLSS frame, full scale restoration, hook closure, and all the
existing temporal/GPU ownership checks. Status contains `scaled_route` and
`resolution` alongside shared continuous telemetry. No live image readback is
claimed by the reviewer.

Evidence directory: `local/continuous-dlss-20260929/v1.25.2`.
This revision corrects a byte-width error in the final-source capture and
the activation hook shutdown race/accounting. Confirm `final_captured`,
`built` and SDK frames advance before beginning the visual checks. The
previous v1.25.0 and v1.25.1 runs admitted no DLSS frame and are archived.
After stop, verify native hook bytes as well as telemetry; v1.25.1 incorrectly
reported eight activation hooks disabled after a queueing failure.
The diagnostic DLL replaces only the existing MO2 dlssTEST probe slot. Its
`.output-dir` sidecar points to this directory; `sdk-runtime-dir.txt` points
to the existing local modern SDK. Logs, snapshots and backups remain inside
the repository. Game settings, executable and original assets are not changed.

The deployment script verifies a closed game, previous DLL identity, source
snapshot identity and unchanged configurations before writing. It preserves
`previous-slot.dll.disabled` and the exact previous sidecar. With the game
closed, `deploy.ps1 -Action Restore` restores that prior slot and sidecar;
it does not alter game settings. Do not restore automatically: the user's
current preference is to keep the latest diagnostic DLL installed.
