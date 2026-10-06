# Live SDK capability failure and initialization-order controls

2026-09-25. Live v1.11.0 run 52904 completed input capture and conversion,
then failed closed at the public SDK capability gate. No feature was created
or evaluated. The user closed normally; the exact previous MO2 DLL was
restored at 20:19:24 +03, the output sidecar removed and all four configuration
hashes remained unchanged. No replacement diagnostic is installed.

## Live evidence

Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Process creation FILETIME `134348297109914486`; DLL SHA-256
`668ae28a421d01691563d1d519445c1b015fb0b2892ef84d78a50e308ef95e44`.
Frames 29787–29789, 2560x1440, adapter LUID 0:97816.

- Native shader, consumer, producer submission, depth routes and raw pixels
  pass their frozen reviewers. All hooks disabled, pending/faults zero.
- All three private conversion fences complete and recordings retire. Nine
  converted files contain 77,414,400 float values matching the independent
  CPU reference exactly; depth is also byte-identical to its raw capture.
- SDK Init, GetCapabilityParameters and all three parameter Get calls return
  success. The value predicate `(Available == 1, NeedsUpdatedDriver == 0,
  FeatureInitResult == 1)` fails. **The original logger omitted these values**;
  successful Get calls do not imply successful capabilities.
- The subsequent module snapshot contains driver `nvngx.dll` / `_nvngx.dll`
  but neither legacy nor modern `nvngx_dlss.dll`. This is a snapshot after
  failure, not proof that a feature module was never transiently loaded.

The frozen full reviewer rejects the run at `stopped`. A supplemental review
calls the existing input reviewers and a documented in-memory conversion-only
view: the shared `owned.failure` field was set by the later SDK exception,
despite all conversion artifacts having completed. Only that later failure
is cleared in the temporary view. Original logs remain untouched, and both
reviews retain `accepted=false`, `sdk_evaluation_verified=false`,
`dlaa_ready=false`. The SDK failure log exists independently even though the
outer failure path did not populate its artifact metadata.

Evidence: `local/dlaa-live-20260925/v1.11.0/`. Sealed
`archive-52904.json`: 260 files, SHA-256
`e1ba2c7075d92d4a23bf5b40ebc31a3c32dd49dc88bd5277ac8f7c8cbf2bec9c`.
All 228 prelaunch files and 140 frozen sources verified before sealing.

## Isolated public API controls

`mhw_ngx_capability_probe` creates its own D3D12 device and uses only the
public project-ID SDK initialization/capability APIs. It creates no feature,
submits no GPU work and never opens the game process. It uses our project GUID,
CUSTOM engine `0.1.0`, API 21 and the pinned signed 310.7.0.0 provider. The
logging callback counts messages without retaining SDK text or private paths.

Each row below ran in a separate process. All completed API/Get calls returned
success, including the capability queries that reported a failed feature init.

| First initialization | Later initialization | Later capabilities (available, update, init) | Modern module |
| --- | --- | --- | --- |
| Modern directory | None | `(1, 0, 1)` | Loaded |
| No additional directory | Modern directory | `(0, 0, 0xBAD00004)` | Absent |
| Modern directory | Modern directory | `(1, 0, 1)` | Loaded |
| Modern directory | No additional directory | `(1, 0, 1)` | Loaded |
| No additional directory, then owned-device shutdown | Modern directory | `(1, 0, 1)` | Loaded |

`0xBAD00004` is the pinned public SDK's `FAIL_FeatureNotFound`. The no-directory
first query already reports that result. Merely repeating Init with the right
directory does not change it; shutting down the fixture's exclusively owned
SDK/device initialization does. This is evidence of initialization/discovery
state persisting across repeated Init calls in this fixture.

The separate legacy-directory-first control exits during its first Init with
`0x80000003`, before a return result is logged. Its two-line log and original
executable/source are preserved. No crash-cause or successful coexistence
claim is made for that control; it was not retried or bypassed.

These results reproduce the live run's observable pattern but **do not prove
its exact cause**, since its actual capability values were not recorded and
the fixture does not reproduce the game's older API initialization sequence.
The fixture shutdown is not a recovery policy for the borrowed game device.

## Diagnostic correction and verification

`src/dlss_sdk/live.cpp` now emits SDK log version 2. Each capability query logs
both its API return and its signed/unsigned value before checking the unchanged
support predicate. The failure record includes the callback message count.
The reviewer requires successful values in v2 while retaining v1 compatibility.

Release builds pass `/W4 /WX`. The shared-code hardware fixture still produces
three reset-per-frame DLAA outputs, byte-identical to the preceding fixture,
with zero D3D12/GPU validation warnings or errors. Four invalid-input fixtures
reject before SDK Init. Eleven reviewer tests pass against each of the old and
new log versions, including missing/invalid capability values. Three focused
CTest entries pass (`dlss_sdk_contract`, `temporal_gpu_contract`,
`temporal_gpu_typed_warp`). No new game DLL was deployed.

Evidence: `local/ngx-capability-20260925/v1/`. Sealed `archive.json`:
35 files, SHA-256
`2b7d2d0f1c86eb348715a697548a18baa77bc7b7dde70695b379b66b5f863a9d`.
The original and follow-up probe executables/sources are separately preserved.

## Next decision

Map the game's initial NGX core setup and provider discovery timing before
another capture. Reuse existing hash-gated observations where applicable, but
distinguish the public SDK Init ABI from similarly named feature-provider
exports in the earlier bridge research. Keep real support checks and the
project's public identifier. Do not shut down SDK state owned by the game.

A next live diagnostic should report exact capability values and initialization
ordering early, without repeating resource discovery. Before deployment, give
it a new capture version and separate SDK failures from conversion failures;
preserve failure-log identity in the outer receipt. The three input sets already
pass. Live SDK evaluation, continuous history/jitter and output presentation
remain unfinished.
