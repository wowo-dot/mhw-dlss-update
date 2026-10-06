# Independent-AA input readback v0.5.0

This diagnostic captures color, depth and packed motion through the independent
AA-Off route. It resolves the v0.4.0 motion-state gap at a native explicit
transition rather than guessing a state from another command list. Eight owned
requests establish the entry; receipts 0, 3 and 7 each copy three full-size
images. No SDK, jitter, native AA setting change or output replacement occurs.

## Entry and state contract

Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded analysis image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
All six hook targets, Windows x64 ABIs, native code gates and ownership rules
remain those of [v0.4.0](independent-aa-gpu-trace.md). There are no new hooks,
game vtable patches or original asset changes.

The owned phase 15/order 2 packet marks the request on a known Reset generation.
At that exact marker, motion may still have unknown local state. The diagnostic
waits for the **first relevant ResourceBarrier call after that marker on that
same command-list generation**. Its selected payload must contain exactly the
two full-image color/motion transitions RENDER_TARGET (4) ->
PIXEL_SHADER_RESOURCE (128), with no split flags. Missing or mismatched payloads
reject the request; the diagnostic does not keep searching for a later match.

After that native call returns with unchanged arguments, both declared states
are known. Depth must already have a consistent explicit transition chain on
this same recording ending at PIXEL_SHADER_RESOURCE. All three current states
must be 128, known, with no aliases, conflicts or unsupported transitions.
The marker's original state snapshot stays in the report; a separate read-point
snapshot records the later qualified states. This does not retroactively qualify
the earlier marker or borrow state from an earlier frame.

The saved v0.4.0 trace satisfies this gate at all eight requests. This is a
specific native transition boundary following our independent packet, not a
claim that every image is temporally aligned with the packet's immutable source
frame. Pixel content and that relationship remain subjects for live review.

## Copies and lifetime

Resource identities remain pinned from preparation through submission completion
and recording retirement. Each input is a distinct, single-plane, single-mip,
single-slice, single-sample 2560x1440 Texture2D on the identified device:
color R11G11B10_FLOAT (26), depth R32_FLOAT (41), packed motion R32_UINT (42).
Before hooks are installed, nine private READBACK buffers are allocated, each
14,745,600 bytes with a 10,240-byte row pitch. Total staging is 132,710,400 bytes.

For each selected receipt, immediately after native ResourceBarrier returns and
before its intercepted call returns to the game, each input follows:

1. Explicit native state 128 -> COPY_SOURCE (2048).
2. CopyTextureRegion into that input's private readback buffer.
3. Restore 2048 -> 128, including scoped restoration after a copy exception.

The first transition attempt commits that buffer to retention. Uncertain calls
are never retried. The original ResourceBarrier trampoline issues private
transitions so they cannot recursively become native observations. The copies
change no root signature, PSO, descriptors or other native pipeline bindings.
The source's content is never written. Successful scope is nine copies and
eighteen transitions, 27 private image commands, plus eight private fence Signals.

The existing submission journal holds its recording lease across the original
barrier and all copies/restoration. Its linked event interval must lie after
the owned marker and before Close, on the same exact Reset generation. Each
sample still requires Close -> Execute -> private Signal -> completed value 1
and a later Reset retiring the recording. Only after all eight complete and
hooks drain can the worker map/save the selected images and release references.
Partial issued work retains its private buffers and source pins for process
lifetime. A failed Map/save remains a failed capture even if GPU completion passed.

No native resource state is permanently changed. A readback does not remove
native TAA, perform temporal reconstruction or prove DLAA quality. Native AA and
DLSS remain Off for this run; the historical continuous DLAA path is not armed.

## Verification

Release build uses /W4 /WX. Ten native tests pass: five prior state/GPU-trace
cases and five new real D3D12 WARP cases. The positive fixture uploads known
pixels, records the producer after the marker on the CPU but submits it first
on the GPU, captures all nine images and checks every pixel. All three inputs
are restored correctly; the D3D12 debug InfoQueue reports no errors/corruption.
Missing depth state, separated/wrong transition pair, missing final retirement
and failed Signal cases reject capture/saving. All 37 AA Python checks pass.

The independent reviewer reconstructs the marker and later read-point states
from captured native events, verifies the first matching call, generation,
complete fence/retirement chain, all nine footprint/format/state records and
the saved file hashes. Raw readbacks are not accepted on a visual result alone.
Pixel freshness and cross-queue input writers remain explicitly unverified.

## Runtime and exact rollback

Ignored evidence: `local/independent-aa-inputs-20260926/v0.5.0/`.
Use the existing MO2 test entry with the bare offline profile, DX12 On,
Windowed 2560x1440, native AA Off and DLSS Off. Load a save and keep the camera
still for the short capture. Nothing is hooked before explicit arming; startup
reserves hook storage and waits up to 30 minutes.

The sealed version's `arm.py` verifies source/artifact/deployment/configuration
hashes, then invokes frozen `tools/arm_aa_inputs.py`. Event names are
`Local\\MhwAaInputs-v1-PID-CREATION-1` (arm) and suffix `-0` (stop).
The header identifies version 0.5.0, experiment `independent-aa-input-readback`,
27 private image commands and receipts `[0,3,7]`.

The existing two-second submission window remains bounded. After completion,
the worker saves `aa-input-PID-CREATION-RECEIPT-ROLE.bin` (roles 0=color,
1=depth, 2=packed motion) and writes the final JSONL with `input_readbacks`.
Use frozen `tools/review_aa_inputs.py` with the log and exact DLL hash. Save the
log and review before requesting a normal game exit. Do not interpret a stopped
partial capture as a successful input validation.

The prior DLL and all four configurations are backed up byte-for-byte. The only
new installation file is `.output-dir`; generated images/logs stay in this repo.
After the game is closed, the versioned `restore.py` checks all slot/backup/
sidecar hashes, restores the exact prior DLL and removes only the test sidecar.
It reports configuration changes without overwriting user settings.
