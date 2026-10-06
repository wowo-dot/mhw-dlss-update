# Camera panning exposes a false cut detector — v1.18.1

Run 10484 on 2026-09-26 identifies the cause of this run's repeated history
resets: every noninitial reset came from the view-matrix translation threshold.
The stationary checkpoint retained history across 4,055 evaluations after its
initial reset. During user-confirmed camera panning, the translation check fired
repeatedly while every other reset-reason count stayed zero.

The typed matrix examples explain why. Reconstructing camera centers from the
two saved noninitial reset examples gives displacements of 1.496 and 3.132 world
units. The corresponding largest view-translation component changes are 23.898
and 47.952, exceeding the threshold of 20. Relative rotations are approximately
0.214 and 0.452 degrees. A view matrix's translation includes camera orientation;
it is not a direct measure of camera displacement. The diagnostic preserved the
previous policy, so this observation was not produced by changing thresholds.

## Captures and shutdown

| Checkpoint | DLAA evaluations | Resets | Translation resets |
| --- | ---: | ---: | ---: |
| Requested stationary phase | 4,056 | 1 | 0 |
| User-confirmed panning checkpoint | 12,104 | 2,343 | 2,342 |
| Final stopped log | 12,244 | 2,412 | 2,411 |

The checkpoint interval includes time before the user began panning; it is not
a precisely timed motion-only performance sample. The final log reports 9,832
retained-history evaluations, a longest retained streak of 7,495, and one skipped
attempt. Frame/epoch gaps, camera/view/depth identity, viewport/projection,
long intervals and the nontranslation matrix check caused zero resets.

All 12,244 private GPU signals completed. All 12,245 jitter injections were
restored; all hooks disabled with zero faults and pending callbacks. The process
remained responsive after stopping. The user confirmed normal exit, and exact
rollback completed at 13:41:15 +03. The prior DLL is restored, the output sidecar
removed, and all four configurations remain unchanged (native DLSS Off/TAA On).
The game is closed and no next diagnostic is installed.

## Independent checks and limits

The reviewer reconstructs the saved reason and element masks using float32
operations and reconciles the history counts with the SDK totals. Offline camera
analysis solves `R^T C = -t` for the stored affine row-vector view matrix. It checks
finite values, affine layout, near-orthonormal rotation, determinant near one and
camera-center reconstruction residual before interpreting displacement.

These are two sparse matrix examples, not a complete motion trace. They explain
the confirmed reset reason in this run; the older v1.18.0 run had no reason
attribution. This test does not establish a new camera-cut threshold, improved
live image quality, or performance changes. No replacement policy has been
implemented yet. The original native TAA pass still forwards.

## Next implementation

Replace direct view-translation differences with displacement between validated
recovered camera positions. Preserve frame continuity, resource identities,
projection/viewport, long-gap and rotation/cut guards. Invalid or nonrigid matrices
must fail closed rather than silently retain history. Test fixed-position
rotation, ordinary orbiting, real translation jumps and malformed matrices
offline; then compare stationary/panning retention and exercise actual cuts.

## Evidence

Evidence root: `local/dlaa-history-reasons-20260926/v1.18.1/` (ignored).
The frozen source/reviewer set contains 201 files. All 219 prelaunch files were
reverified before arming and final archival. The final manifest covers 237 files.

- Process: 10484 / creation 134348924844891819.
- Host version 0.0.0.0, SHA-256:
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- Test DLL:
  `150a47a33b3434afd37b23d3c45c488d181d431882e97e5a68c217e951f517aa`.
- Startup log:
  `aedba0695e9b91e2571eba199cd6651559c600891ca8fd64d5a3349dff06f759`.
- Continuous log:
  `4cefd32a326679083b0cf7c5bed6114d744c08f049b0472d25fb662a89d07ed7`.
- Prelaunch manifest:
  `a84bab9cfc7dca61e4b6539e2e14eab4dd1eccadeb59f99e81be21fd880b42fe`.
- Final archive manifest:
  `ad6ecada8bede412ecd1d73a4895a0dfa4f13d6a3ccb44deeae21b54f4f88782`.

DX12, 2560x1440, swap-chain format 87, adapter LUID [0,98931].
Arm-to-stop requests span 132.219 seconds. The initial MO2 launch used an invalid
named-instance argument; the existing Portable MHW instance was selected and
the correct diagnostic entry subsequently verified in the loaded process.
