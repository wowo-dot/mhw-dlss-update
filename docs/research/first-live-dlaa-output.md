# First verified DLAA evaluations on the live game device

2026-09-25, v1.13.0, PID 47780, creation `134348356816911874`.
Three private, native-resolution DLAA outputs from the running game passed
the frozen full reviewer. The original TAA scene remained presented.

The startup transaction again passed: four code gates became ready after
469 ms, public project-ID initialization succeeded before the unchanged
native API-18 call, and capability values remained `(1,0,1)`. The startup
hook disabled before the native input recorder was armed. The loaded diagnostic
and signed modern provider were independently matched by path and SHA-256.

The input capture selected consecutive frames **17950–17952** at 2560x1440,
adapter LUID `(0,97816)`, D3D12 swap-chain format 87. All ten capture hooks
disabled with zero pending callbacks and zero faults. The independent GPU
conversion review compared **77,414,400 float values**, all exactly matching
the reference conversions of captured color, depth and motion.

The evaluator accepted the retained startup ownership on the same device and
provider. It made **zero additional Init calls**, queried capabilities again,
created one DLAA feature and successfully evaluated all three private input
sets. Four private submission fences completed and their recordings retired.
All input states were restored, ReleaseFeature and DestroyParameters succeeded,
and borrowed-device Shutdown remained deferred until normal process exit.

## Output evidence and limits

The full reviewer verified all output hashes and **33,177,600 RGB values**:
finite, overwritten from the sentinel, nonconstant and distinct from input.

| Frame | Changed RGB values / 11,059,200 | Mean absolute RGB difference |
| --- | ---: | ---: |
| 17950 | 11,058,817 | 0.0012392292 |
| 17951 | 11,058,842 | 0.0012289864 |
| 17952 | 11,058,526 | 0.0012143995 |

These differences establish written SDK output, not improved visual quality.
The mode is native-resolution DLAA, requested preset K, with HDR, inverted
depth, nondilated input-resolution motion and automatic exposure. Each of
the three evaluations requests reset and supplies jitter `(0,0)`, exposure
scale 1 and a fixed 16.666666 ms control interval. This run does not establish
continuous history, final jitter/exposure calibration or on-screen replacement.
Game device debug settings were unchanged; GPU debug-validation claims apply
to the isolated fixtures, not this live process.

The game remained responsive, and the user confirmed normal exit. The exact
previous DLL was restored at **21:53:36 +03**. Graphics settings, barebones
profile, mod list and diagnostic mode hashes were unchanged. No next diagnostic
is installed. Original game assets were not modified.

## Immutable archive

`local/dlaa-startup-live-20260925/v1.13.0/` contains 279 sealed files,
including 244 prelaunch files and 29 verified runtime artifacts. The preparation
and runtime contracts are documented in [startup-owned live DLAA](../runtime/startup-live-dlaa.md).

- Host SHA-256: `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- Diagnostic SHA-256: `737ea5c062b5ec93c076a19508a1528a006231c3d63499d98beb52368498cac3`.
- Capture log SHA-256: `59d336f01f85d9dbef5a47ba6dac035d0c4411840ba8e7f4289486d079dc115f`.
- Archive SHA-256: `dbaaa17185b546464d75f2919f4a18f015bd4abb77f669cbe7cf283cce1da339`.

Next: bounded continuous history with correctly paired projection jitter,
followed by controlled presentation of the private output. Keep the working
startup ownership and verified input conversion; neither needs rediscovery.
