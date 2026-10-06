# Continuous history-reset diagnosis (v1.18.1)

This build keeps v1.18.0 rendering and reset decisions unchanged. It adds bounded
reason accounting after successful DLAA evaluations, under the existing runtime
lock. No new hook, GPU pass, image readback or per-frame disk write is added.
The [continuous prototype contract](continuous-dlaa.md) still applies, including
the same executable hash, x64 calling conventions, resource ownership and
process-lifetime retention. Native DLSS stays Off and TAA On.

## Evidence sought

The previous live run recorded 4,566 resets across 20,802 evaluations without
reason attribution. This does not establish that the view-matrix heuristic was
responsible. A new synthetic test demonstrates one weakness: a fixed camera at
(10000,0,0), yawed by 0.01 radians, changes the view translation by about 100 and
crosses the current threshold of 20. Thus view translation is not itself camera
displacement. Actual game examples must determine whether to revise that check.

Each continuous status contains `history` schema 1, policy `v1.18.0-unchanged`:

- Total evaluations, resets and retained-history evaluations; current and longest
  streak of retained-history evaluations.
- Overlapping counts for first evaluation, frame/epoch gaps, camera/view/depth
  identity, viewport, projection, long interval, nontranslation view elements,
  and translation view elements (indices 12–14).
- The first example for each of the eleven reasons and the latest reset example.
  An example contains the previous/current typed history stamps and exact view
  and projection float bits, plus reason and changed-element masks. It contains
  no arbitrary memory dump. First evaluation reports only the first reason.

Storage is below 8 KiB and does not grow with frame count. Examples update in
memory; existing five-second status and sparse log publication are retained.
Counts can overlap, so their sum is not the reset total. The policy still uses
the existing 250 ms interval, 0.5 nontranslation and 20 translation thresholds.
The SDK's input jitter, output replacement and queue synchronization are unchanged.

`tools/continuous_history.py` independently reconstructs every saved example's
reason and element masks with float32 subtraction. The continuous reviewer
requires this evidence for v1.18.1 and cross-checks counts against SDK totals.
The old v1.18.0 reviewer path remains supported.

## Offline verification

The Release build uses /W4 /WX. Ten continuous tests pass, including 1000 scoped
projection callbacks and 1000 WARP submissions. History tests cover every reason,
overlap and strict thresholds, first/invalid frames, bounded storage, the fixed
camera rotation example, and 100,000 comparisons against the pre-instrumentation
policy. Twelve Python checks accept native C++ telemetry and reject corrupt
matrices, reasons, counts, timestamps and examples. Thirteen earlier continuous
reviewer checks and fifteen startup reviewer checks pass.

The earlier 240/600-evaluation GPU fixtures remain evidence for unchanged GPU
code; they were not rerun for this telemetry-only change. Live run 10484 now
attributes all 2,411 noninitial resets to the view-translation check; stationary
history was retained. [Result, matrix analysis and rollback](../research/continuous-history-reset-result.md).
The replacement policy remains pending.

## Runtime procedure and rollback

Evidence root: `local/dlaa-history-reasons-20260926/v1.18.1/` (ignored).
The local deployment script preserves the exact prior diagnostic DLL and sidecar,
checks all four configurations, freezes tested sources and writes a receipt.
Use this root's `deploy.ps1 -Action Restore` with the game closed for rollback.

Launch the usual MO2 test entry in the bare offline profile. Leave native DLSS
Off/TAA On, load a save and keep the camera still. After startup verification,
arm with `tools/arm_continuous_dlaa.py` for this exact process generation. Save a
stationary status after about 20 seconds; then have the user pan for about 20
seconds and save another status. Read per-reason deltas and examples before
proposing any policy change. Use the stop event, verify closure, then close
normally and restore. Continuous admission still has no frame/time cutoff.

The user reported roughly 80 FPS versus 200–240 without this prototype. That is
an unprofiled comparison; performance work remains separate from this diagnosis.
