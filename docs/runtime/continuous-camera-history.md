# Camera-displacement history policy (v1.18.2)

This build replaces the false translation-cut heuristic identified in
[v1.18.1 run 10484](../research/continuous-history-reset-result.md). It computes
camera position from the view matrix and compares positions across successful
evaluations. All GPU rendering, hooks, native ABI, resource checks, synchronization
and process-lifetime retention follow the [continuous contract](continuous-dlaa.md).
Native DLSS stays Off and TAA On. Performance optimization remains separate.

## Policy and admission

For a row-vector affine view, `C*R+t=0`. The native implementation solves
`R^T*C=-t` in double precision using cofactors, including division by the actual
determinant. It does not assume the rounded float matrix has an exact transpose
inverse. Input matrices must be finite and affine, with rotation orthonormality
and determinant errors at most 1e-4. The reconstructed position must be finite
and satisfy each translation equation within `1e-9*(1+abs(t_component))`.
Both consecutive matrices are checked; only the empty initial predecessor is
exempt. Invalid data rejects admission and leaves the native renderer in place.

The translation-cut criterion is Euclidean camera displacement **greater than
20 world units**. This reuses the earlier numeric threshold on the correct
quantity; it remains a heuristic requiring live validation. Unlike the old
per-component test, it also detects a diagonal displacement whose length exceeds
20. First evaluation, frame/epoch gaps, camera/view/depth identities, viewport,
exact projection bits, long intervals and nontranslation view-element jumps
retain their existing reset rules. The rotation criterion remains an element
change greater than 0.5, and the long-interval threshold remains 250 ms.

## Bounded telemetry

History schema 2, policy `camera-displacement-v1`, renames reason index 10 to
`camera_displacement`. Reset examples include recovered previous/next positions
and their distance, serialized at double precision. The old view-element jump
mask remains diagnostic, including translation indices 12–14.

`translation_resets_avoided` counts retained evaluations where the old translation
check would have fired. Its first and latest examples prove that the legacy
translation mask was nonzero while the new policy retained history. They do not
claim that every omitted reset represents improved image quality. Total storage
remains below 8 KiB; the five-second/sparse logging schedule is unchanged.

The Python reviewer independently recovers position using pivoted elimination,
recomputes all masks/decisions, checks the avoided-reset examples and matches
history totals to SDK counters. It enforces each DLL version's expected history
policy while continuing to support v1.18.0 and v1.18.1 evidence.

## Verification

- Release /W4 /WX build and all ten continuous native tests pass.
- 100,000 generated camera positions in the range ±100,000 world units exercise
  small yaw/pitch changes and actual 30-unit jumps. Fixed distant rotation,
  ordinary orbiting, strict distance boundaries, diagonal jumps, all other reset
  reasons and combined reasons are also checked.
- Singular, scaled, reflected, sheared, projective and nonfinite matrices reject
  admission. Invalid previous matrices are checked as well.
- Three saved live matrix pairs replay through the compiled C++ policy: all
  retain history, with independently confirmed displacements of approximately
  3.132, 2.056 and 1.496 units. Their old translation masks are unchanged.
- Seventeen history reviewer checks pass, plus thirteen continuous reviewer
  checks on each prior version and fifteen startup reviewer checks (68 targeted
  checks total, in addition to the three-pair replay).

No GPU-path source changed; the earlier streaming GPU fixtures remain applicable
evidence for that code. The offline replay contains only the previously saved
typed matrices.

Live stationary/panning run 52584 on 2026-09-26 passed verification: 10,002
evaluations, one initial reset, 10,001 retained-history evaluations, and 3,473
legacy translation resets avoided. The user reported a clean scene. All GPU
signals completed, all jitter injections restored and hooks disabled with zero
faults/pending. Normal exit and exact rollback are complete. See the
[run result](../research/continuous-camera-history-result.md). Genuine cuts,
full transition stability and objective temporal-quality measurements remain
pending; this run made no performance claim.

## Test and rollback

Evidence root: `local/dlaa-camera-history-20260926/v1.18.2/` (ignored).
Deploy using that root's script with the game closed. The previous diagnostic
DLL and sidecar are preserved; all four configurations are checked unchanged.
Restore with the same script's `-Action Restore` after normal game exit.

Load a save in the bare offline profile with native DLSS Off/TAA On and keep the
camera still. Verify startup and exact DLL identity, then arm this process
generation with `tools/arm_continuous_dlaa.py`. Save a stationary checkpoint,
then cue slow panning. Check retained-history streaks, per-reason counts and
avoided-reset examples. Stop/verify closure and collect the user's visual report
before normal exit/rollback. Camera cuts, menus, alt-tab and resource re-creation
remain subsequent validation; no automatic in-game toggle or resolution change
is part of this comparison.
