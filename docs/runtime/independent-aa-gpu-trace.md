# Independent-AA GPU trace v0.4.0

This extends the accepted [v0.3.0 deferred marker](independent-aa-deferred-marker.md)
with resource-state observations, command-list recording generations, submission,
private fence completion and retirement. Eight immutable requests still enter
through the owned phase 15/order 2 pass while native AA and DLSS are Off. No DLAA
evaluation, jitter, AA setting change or private image command is recorded.
The diagnostic does add up to eight queue Signals to its own fences.

## Native and D3D12 hook contracts

Supported executable version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded analysis image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
The scheduler and deferred consumer retain all v0.3.0 full-byte gates, ABI
forwarding, exact packet identities, parameter-table checks and queue restoration.
Original game executable/assets are never modified.

Six hooks are active only during the bounded recording:

| Target | Windows x64 arguments | Observation |
| --- | --- | --- |
| Game RVA `24f1fc0` | RCX manager, RDX frontend context, R8 view | Owned descriptor scheduling and removal |
| Game RVA `259e1e0` | RCX executor, RDX backend context, R8 packet | Marker after native preparation and barrier flush |
| GraphicsCommandList::Reset, slot 10 | RCX list, RDX allocator, R8 initial PSO | New recording generation; HRESULT in EAX |
| GraphicsCommandList::Close, slot 9 | RCX list | Successful close; HRESULT in EAX |
| CommandQueue::ExecuteCommandLists, slot 10 | RCX queue, EDX count, R8 list array | Exact submitted batch and subsequent private Signal |
| GraphicsCommandList::ResourceBarrier, slot 26 | RCX list, EDX count, R8 barrier array | Relevant transitions, UAV and alias barriers |

COM targets are discovered from objects on the identified device and validated
as system methods. A private, never-submitted list discovers ResourceBarrier;
it is closed and released before capture. No game vtable is modified. Every
hook forwards the original exactly once. Native callbacks and trampoline storage
remain pinned for process lifetime even after the hooks are disabled.

## State and lifetime rules

Before observation begins, the three resource identities are pinned and checked
against the discovered device: full-size 2560x1440 Texture2D, one mip, array
slice, sample and format plane; distinct R11G11B10_FLOAT color (26), R32_FLOAT
depth (41), and R32_UINT packed motion (42). Each published request must still
match those identities. The owner chains and post-consumption checks retain the
v0.3.0 contract.

State is tracked separately for each command list and observed Reset generation.
Reset starts all three states as unknown. A full-image explicit transition
anchors a state; subsequent StateBefore values must agree with tracked state.
Only ALL_SUBRESOURCES or subresource zero are accepted for these single-plane,
single-subresource images. Split transitions, aliases, unsupported barriers,
conflicts, missing Reset or stale generations cannot qualify an input. Relevant
barriers are checked before and after native forwarding and share the same
ordered journal as Reset, Close, the marker and ExecuteCommandLists.

The marker records its immutable source frame and packet identity after the
empty native packet has flushed pending barriers. The global renderer counter
remains a separate observation. Each accepted sample requires a completed
Reset -> marker -> Close -> Execute chain, exactly one private fence Signal,
observed fence value 1, and a later Reset retiring that recording. These facts
are reconstructed independently from the captured API events by the reviewer.

Resource references remain held through completion, retirement and hook drain.
Incomplete traces retain sampled references for process lifetime. A failed
Signal does not imply GPU completion or permit early release. Empty packet,
descriptor and trampoline storage is never recycled during this diagnostic.

Limits: eight requests, 128 lists, 4096 journal events, 2048 selected barrier
calls and 8192 selected barriers; at most 1024 barriers per observed API call.
The submission window is two seconds, including 150 ms of Reset warmup. Partial
or overflowing evidence fails qualification and disables the hooks.

Recorded states describe this exact recording point. They do not by themselves
prove pixel freshness, temporal alignment, or cross-queue input writes. No
resource read, transition or DLAA evaluation is authorized solely by its format
or a CPU owner pointer. The inherited `deferred` report retains its CPU-only
`gpu_states_verified:false`; the separate `gpu_trace` report and independent
review supply the new, scoped state/completion evidence.

## Offline verification

Release builds use /W4 /WX. The state-contract test checks unknown/reset,
transition conflicts, split barriers, subresources, ordering and alias handling.
A real D3D12 WARP fixture hooks Reset/Close/Execute/ResourceBarrier and exercises
eight recordings with GPU Signals and retirement. Its positive case passes an
independent reconstruction. Missing transition after Reset, missing retirement,
and failed Signal cases are rejected. All four fixtures also check the D3D12
debug InfoQueue for errors and corruption.

Eighteen native checks passed: the five new state/trace cases and thirteen
existing submission cases. All 33 AA Python checks passed, including altered
barriers, generations, states, fence chains, identities and closure evidence.
This is offline validation; the game result remains pending before the first run.

## Runtime and rollback

Use the existing bare offline MO2 test entry, DX12 On, Windowed 2560x1440,
native AA Off and DLSS Off. Load a save; no camera movement or setting toggle
is required. Startup reserves hook storage and waits up to 30 minutes for an
explicit process-generation-specific arm event. Nothing is hooked before arming.

Versioned evidence: `local/independent-aa-gpu-20260926/v0.4.0/`. Its `arm.py`
verifies the sealed files, deployed DLL, sidecar and configurations before
invoking frozen `tools/arm_aa_gpu.py`. Events are
`Local\\MhwAaGpu-v1-PID-CREATION-1` (arm) and suffix `-0` (stop). The startup log
must identify version 0.4.0 and experiment `independent-aa-gpu-trace`.

Save the capture and run frozen `tools/review_aa_gpu.py` before asking for a
normal game exit. Success requires eight complete requests, unchanged native
bindings and queue, independently reconstructed states, completed/retired GPU
records, released diagnostic pins, zero fault and all six hooks disabled.
Unknown states are useful partial evidence, never an accepted GPU qualification.

The versioned deployment retains the exact previous DLL and all four configuration
snapshots. The only added installation file is the DLL's `.output-dir` sidecar.
After verifying the game is closed, run the versioned `restore.py`: it checks
the deployed DLL, backup and sidecar hashes, restores the exact previous DLL,
and removes only the matching sidecar. It reports configuration hashes without
overwriting user settings. Keep all generated binaries and evidence untracked.
