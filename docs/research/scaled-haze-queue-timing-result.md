# Haze route queue timing: v1.25.10 result and v1.25.11 correction

## Confirmed runtime result

The v1.25.10 test (PID 26224, creation 134351976947449200) was healthy
outside the sauna, suspended reconstruction inside it, and recovered on exit.
The user confirmed the final scene was normal and the game closed normally.
Evidence: `local/scaled-haze-route-20260930`.

Final counters: 2,558 DLSS evaluations, zero runtime faults, 12,107 GPU signals
completed, 12,192 jitter changes restored, and full render scale restored.
All 38 code gates and 17 installed native hook heads matched their original
bytes after intentional stop. The reviewer accepted cleanup and telemetry;
this does not establish a working haze adapter. Haze completion was zero.

There were 9,548 rejected output plans. In the sauna, rejection 15 meant a
required draw was absent from the queue. The graph itself had the expected
motion -> haze -> grade link. Auxiliary texture qualification was never reached,
so the zero auxiliary descriptors are not evidence of incompatible textures.
Three same-frame haze recipes were captured successfully at frames
16717, 16777 and 16837. Haze output reused the earlier copy destination.

## Native ordering and cause

The decoded, hash-verified outer postqueue function at RVA 0x24f1fc0 sorts its
filters, assigns their ping-pong resources, then calls each enabled filter's
vtable +0x60 Execute. The call returns at RVA 0x24f228b; the loop continues
until RVA 0x24f22ec and the function returns at RVA 0x24f2302. Motion order 100
therefore returns before haze order 20000 has appended its draw.

v1.25.10 built the output plan at motion return. That is too early for haze.
`postqueue-order.asm` records the disassembly. The bounded live queue survey
caught only early partial queues; it is not proof of a completed draw queue.

Host SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded image SHA-256:
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Outer postqueue gate: 835 bytes, SHA-256
`1ec618caf8d1d455cc12fc12c57889b73e1073074faf04e82364fba10d01e190`.

## Correction

v1.25.11 reuses the existing activation hook for the outer postqueue. After a
successful native return, it notifies the scaled route only for the admitted
outer queue, while the owned camera context is still active. The x64 Windows
ABI has manager/context/view in RCX/RDX/R8. Save these before forwarding:
the forwarder replaces volatile registers in its Frame with native return state.

Motion return now records a receipt without building or rejecting the partial
queue. Completion requires matching frame/epoch/context/view/TAA unit, current
manager, the same thread, and one successfully returned motion Execute.
Duplicates, foreign callbacks, stale slot reuse, exceptions and stopped
recorders cannot build a new plan. Existing graph/resource/shader checks and
GPU fence-plus-Reset retention still apply. No new hook or relaxed graph check.

Regression coverage exercises incremental queue construction for ordinary and
haze paths, the native activation ASM callback, nested/exception/stop behavior,
and aliased haze-output resources under D3D12 GPU validation. Results are stored
in `local/scaled-haze-queue-completion-20260930`:

- 47 native CTest cases passed, including the production activation ASM and
  continuous runtime lifetime/failure tests.
- Ordinary GPU route: 96 frames and 288 effects. Alternating ordinary/haze route
  with aliased haze output: 96 frames and 336 effects, including 48 haze frames.
  Each run checked 786,432 expanded-copy pixels, varying vertex resources and
  complete retirement, with zero GPU validation warnings/errors.
- Partial-queue regression passed for ordinary, haze and aliased haze graphs;
  14 existing malformed graph cases and 10 haze cases remained rejected.
- 23 Python tests passed, including 13 corruption checks against the actual
  v1.25.10 capture. A separately marked synthetic v1.25.11 replay passed and
  rejected four missing telemetry fields/schema changes.
- Release builds passed /W4 /WX. CMake reported the existing third-party MinHook
  minimum-version deprecation. An initial enlarged fixture exhausted its stack;
  heap allocation for saved test data corrected it before the successful runs.

The subsequent runtime result is recorded below.

Keep DX12, AA/DLSS Off, the bare profile and the current window dimensions.
Preserve the exact previous DLL and sidecar before deployment, leave the latest
DLL installed afterward, and redirect captures to the new evidence root.
The local deployment script supports explicit rollback while the game is closed.

## v1.25.11 live result: sauna route completed

PID 46780, creation 134351995074220844, used DLL SHA-256
`194da1b7f5e9ae144603555fbdcc5d2c6266a8a4596aaf90b905547057762cb4`.
The user reported a clean scene inside the sauna and thought it remained clean
after the requested walking, panning, exit and re-entry sequence. The user later
confirmed the game was closed. No video or pixel comparison was captured here.

The run completed 3,843 DLSS evaluations: 2,203 completed the four-effect haze
route and 1,640 completed the ordinary route. All 3,845 published plans built;
there were zero plan, binding or resource admission rejections and zero faults.
The two unfiltered frames were the first ordinary-pipeline warmup and the first
haze-pipeline warmup (readiness reason 9, missing masks 7 and 8 respectively).
There were no additional skipped evaluations during the tested transitions.
The 13,732 replaced downstream effects equal 3 * 3,843 + 2,203.

Live auxiliary resource descriptions matched the qualified binding recipe:

| Input | Actual D3D12 resource |
| --- | --- |
| Depth | Texture2D, 2560 x 1440, R32_FLOAT, one mip/sample |
| Volume | Texture3D, 32 x 32 x 32, R8G8B8A8_UNORM, one mip/sample |
| Filter | Texture2D, 1 x 1, R8G8B8A8_UNORM, one mip/sample |

The native haze PSO/root/bindings, expanded draw and ordinary/haze route lifetime
checks passed during actual execution. Three bounded same-frame recipes were
also captured without rejection. This verifies the tested route, not every haze
technique, unrelated scene or independently read-back auxiliary GPU descriptors.

Intentional stop completed all 3,845 reconstruction/depth GPU signals; every
scaled-route recording was released (3,843 retained/released, zero live).
All 3,846 jitter changes were restored, full render scale returned, and runtime
hook cleanup passed. A separate read-only process check matched all 38 complete
code gates and 17 native hook heads. The full reviewer accepted the trace.
Collector duration was 183,406 ms, with 36 snapshots/events and one retried
status-file read. Latest DLL and graphics/profile settings remain unchanged.

## Remaining panning investigation

This run retained temporal history for 3,562 evaluations and reset it 281 times:
one first evaluation and 280 camera-displacement reasons. Some reasons overlap
with a frame gap or large view rotation. The fixed >20-unit per-frame camera
distance rule is unchanged by the haze fix. Twelve unique bounded saved matrix
pairs were independently recomputed; ten had displacement as their only reset
reason (20.29 to 91.95 units over 29.63 to 57.98 ms). These are selected examples,
not a distribution of every frame, and this clean sauna report does not by itself
establish visible history instability.

Earlier isolated panning in v1.25.7 did report brief visible skips while SDK
evaluations continued, with 241 new displacement resets and no new output/depth
rejections. That evidence supports a controlled history-policy comparison as
the next test. Do not claim the visible cause or choose a new threshold solely
from the sauna samples. Preserve frame/resource/projection discontinuity checks
and distinguish this from the separately observed early CPU draw recording and
consumer-depth interval rejections. Performance cleanup remains deferred.
