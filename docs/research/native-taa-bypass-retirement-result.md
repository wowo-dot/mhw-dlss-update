# Native TAA bypass: validated request retirement live result

The v1.19.1 focused run passed on 2026-09-27. The already-published request
overlap that caused the previous run's slot collision occurred again, was
validated and retired after native return, and DLAA continued through later
area and sauna checks. This is evidence for the repaired request-lifetime case,
not complete renderer-wide exclusive-AA coverage.

## Run and observed behavior

The tested game executable was version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
PID 47596, creation FILETIME 134349394655214204; DX12, bare profile, Windowed
2560x1440. Native AA was temporarily TAA to activate the existing preparation
stage, with native DLSS Off. Armed duration was 188.015 seconds.

| Checkpoint | Successful DLAA replacements | Validated native retirements | Faults | User report |
| --- | ---: | ---: | ---: | --- |
| Baseline | 1,712 | 0 | 0 | Save loaded |
| First area return | 5,950 | 1 | 0 | Stayed active and clean |
| Sauna walk/pan, leave and re-enter | 10,246 | 1 | 0 | Stayed active and clean throughout |
| Second area return | 14,608 | 1 | 0 | Stayed active and clean |
| Deliberate diagnostic stop | 15,761 | 1 | 0 | Normal exit confirmed afterward |

The area prompts requested loading-screen transitions followed by walking and
panning; the sauna prompt requested about twenty seconds inside followed by
leaving and re-entering. Actions and visual quality are user reports, separate
from the saved telemetry checkpoints. No video or live pixel readback was used.

The assistant explicitly stopped the diagnostic after the last successful
checkpoint. The user's subsequent report that DLAA had stopped was this
intentional shutdown, not a new feature failure. The final event records
`stopped`, `armed=true`, `healthy=true`.

## Repair exercised in-game

The [validated-native-return correction](native-request-forward-retirement.md)
observed 21 TAA consumers blocked by active publication. Twenty had no matching
published request and forwarded native rendering without inventing retirement.
One had a uniquely validated published request: frame/epoch 23341, sequence
17457, with one active publisher and one native bypass. Its packet and sequence
remained stable across native execution, permitting retirement after return.

Started and retired counts were both one; pending, failed, invalid and ambiguous
counts were zero. Final SDK game frame 36270 was 12,929 frames after the retired
request, beyond the 32-frame request-slot reuse. No request collision occurred.
All 15,911 publications are accounted for by 15,910 normally matched TAA
requests plus the one validated forwarded retirement.

The previous v1.19.0 run instead retained its request across a blocked callback
and collided exactly 32 frames later; see the
[first bypass result](native-taa-bypass-first-result.md). This run supports the
specific lifetime fix. It does not establish that every possible publication and
consumer interleaving has been exercised.

## Final verification and limits

- 15,761 claimed resolves, DLAA evaluations, reconstructions and SDK-command
  receipts; zero faults, resolve failures or shader rejections.
- 15,072 retained-history evaluations and 689 history resets; longest retained
  streak 3,162 evaluations.
- All 15,798 GPU recordings submitted, completed, Reset-retired and released;
  zero remaining resource references, peak two live recordings.
- All 17,397 jitter injections restored; no pending callbacks or private
  recording, and all hooks disabled.
- Final SDK/history/request/retention/resolve/forwarded-request review accepted.

The scope remains `exclusive_mode=false`. The 149 unclaimed resolve candidates
and the one validated publication-blocked callback used native forwarding.
Unmatched callbacks also continued native rendering; they are not automatically
equivalent to qualified main-scene AA resolves. Whole-renderer exclusive-AA
ownership and activation independent of the player's AA selection are not yet
implemented. This was a focused test, without a new long soak or dedicated
focus, resize, device recreation or cutscene test.

The build and implementation checks preceded this live run: Release `/W4 /WX`
clean, 34 focused native checks and 99 Python checks passed. These include actual
native-hook fixtures for overlap, return validation, stale tokens and slot reuse.

## Archive and restoration

Ignored evidence root: `local/native-taa-bypass-20260927/v1.19.1/`.
The finalizer rechecked all 346 prelaunch files, 290 repository source identities,
the strict final review and phase reviews, then sealed 371 files. Key evidence:
`final-review-47596.json`, `run-result.json`, `user-reports-47596.json`,
`restored.json`, and `archive-final.json`.

| Identity | SHA-256 |
| --- | --- |
| Tested v1.19.1 DLL | `fd5000ead97248c29062bd58a150d42009b8f9e1fe91a70965a0ce7889cf93ae` |
| Final input log | `494ff5fa2facc8ccaea1ca00970de4464a48ee0472a009ce0975f645ebe4aa43` |
| Startup log | `86cea04aee1336b8fa47075478f02b5414803af35a844d7965d98986f9872868` |
| Prelaunch manifest | `75bb4c3f83619ef1ab1025dca0ae9929009a24f3d2c619d3bcadfaee2a100208` |
| Final archive manifest | `7a0875b1d5e7f8c243f600ee7393e0b16d416bbb393fd72c219e6d61b2ad6829` |

After normal game exit, `deploy.ps1 -Action Restore` verified process absence
and restored the exact prior diagnostic DLL (SHA-256
`689bef7cf00ff71e4518f452f0467edf6c3d877f2a6dd090553388d8b5f288cc`).
The temporary output-directory sidecar was removed. All four configuration
identities match their pre-test values: graphics, bare-profile mod list, profile
settings and diagnostic mode. Graphics returned to native AA Off and DLSS Off,
retaining Windowed 2560x1440 and DX12. No next build is installed or armed.

Next work remains renderer-level AA ownership using the existing temporal stage:
activation independent of player AA selection, suppression of other AA, one
jitter owner, correct output selection, and agreed failure/suspension behavior.
CMFM and menu integration remain deferred. Resolution-changing DLSS follows.
