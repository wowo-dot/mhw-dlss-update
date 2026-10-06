# First bounded live DLAA history and paired jitter

Accepted 2026-09-25, v1.14.0, PID 49200, creation FILETIME
`134348374443719676`. The frozen reviewer accepts three live private DLAA
outputs with history retained after the first evaluation and projection
jitter bound to the exact input frames. Native TAA remains presented.
The [experiment contract](../runtime/retained-history-dlaa.md) describes
hook ABI, mutation/restoration, resource ownership, limits and offline tests.

## Observations

Startup code readiness took 453 ms (29 polls, mask 0 to 15). Public project-ID
Init and the unchanged native API-18 Init succeeded; capability values remained
`(1,0,1)`. The early hook disabled. Before arming, the exact process generation,
loaded DLL/provider paths and hashes, four configurations, all 244 prelaunch
files and pinned dependencies were verified.

The two-second recorder selected frames/epochs **23398–23400** at 2560x1440,
D3D12, adapter LUID `(0,97816)`, swap-chain format 87. The selector admitted
its first preview. Projection and TAA receipts match exactly; one scoped
middle-frame injection and one restoration completed. All eleven capture
hooks disabled and drained with zero faults. The game remained responsive.

All **77,414,400** converted input floats exactly match independent reference
conversion of the raw color, depth and motion images. Their copies, private
conversion, fences, recording retirement and state restoration pass review.
The evaluator reused the retained startup initialization with zero extra
Init calls. One CreateFeature, three EvaluateFeature calls, four completed
private submissions, ReleaseFeature and DestroyParameters succeeded.
Borrowed-device Shutdown remains deferred until normal process exit.

| Frame | Reset | DLAA jitter, input pixels | Output RGB values checked |
| --- | --- | --- | ---: |
| 23398 | true | (0, 0) | 11,059,200 |
| 23399 | false | (+0.1953125, +0.169753075) | 11,059,200 |
| 23400 | false | (0, 0) | 11,059,200 |

The middle native pair is `(0.390625, -0.3395061493)`. The SDK receives its
recorded raster displacement, with Y converted to downward-positive pixels.
Every output RGB value is finite, overwritten, nonconstant and distinct from
input. All three full image hashes pass. Mean absolute RGB differences from
input are 0.0013072338, 0.0023024289 and 0.0019496255 respectively; these are
write/difference evidence, not a visual quality score.

## Limits and next decision

This proves the bounded live handoff with reset true/false/false and exact
projection-jitter metadata. It does not yet establish a sustained production
sequence, improved antialiasing, camera-cut/recreation stability or on-screen
DLAA. Time is still an explicitly fixed diagnostic 16.666666 ms. GPU debug
validation applied to isolated fixtures, not the game device. The SDK uses
native-resolution DLAA, requested preset K, flags 75 and motion scale (1,1).

Next: inspect these retained-history images, then extend bounded accumulation
and controlled output presentation. Actual timing, continuous jitter and
reset/recreation handling remain integration work. Preserve the proven
startup ownership, exact input conversion and paired projection path.

## Closure and immutable archive

The user confirmed normal exit. The exact prior DLL was restored at
**22:23:10 +03**, its output sidecar removed, and all four configuration hashes
remained unchanged. No next diagnostic is installed.

`local/dlaa-history-live-20260925/v1.14.0/` contains **279 sealed files**,
including 244 prelaunch files and 30 verified runtime artifacts.

- Host SHA-256: `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- Diagnostic SHA-256: `92f7b14ec2dfe0c89410053271a7bb768e6bd390ded3f2f7a40d6a9ec2a8a2b5`.
- Capture log SHA-256: `ce36862d0c4649c6363f5395c18123c1f2760e03c746410aad6cb54543a4269c`.
- SDK log SHA-256: `575d0880fbb9e6e14a2535a9dbd69b4a3bc1faaeb4a21ad2b29b33a5032e6b0a`.
- Archive SHA-256: `9ba9715e7df45595b33b5145bfe957a8c18ae87f37004c9b86d5863aff2cb87c`.
