# Ordinary TAA input observer

The current extension is the [v1.2.0 native recording observer](temporal-consumer-observer.md).
It retains the shader/request contract below and adds scoped consumer/API
observations. v1.1.0 has an accepted, archived runtime result; its preparation
details below are historical.

Current preparation: **v1.1.0**, adding a bounded compute-shader and binding
recipe capture to the accepted v1.0.0 observer. The single hook, three-request
limit, two-second window and zero jitter/GPU-work budgets are unchanged.
See the [shader map](../research/ordinary-taa-shader-map.md) for the five
additional live code gates and pointer layout.

The recorder retains one shader (at most 256 KiB), reads at most 64 owners
and 128 binding instructions per request, and requires an SM5.0 compute
container. It rechecks metadata and bytes before and after forwarding the
original call. A shader change, unreadable memory or changed binding stops
the observer while original rendering continues. After closure it writes
`inputs-<pid>-<creation>.cs.dxbc` with exclusive creation and records its SHA-256.
The reviewer requires that exact artifact and hash for v1.1.0 acceptance.
The historical v1.0.0 review remains supported without shader evidence.

Native verification covers the existing forwarding/resource-lifetime cases
plus shader mutation, recipe mutation, wrong shader stage, unreadable bytecode,
shader-size overflow and an out-of-range owner index. Contract tests reject
319 missing shader-metadata bytes in addition to the original 620 cases.
An independently compiled owned compute shader is captured, saved and
disassembled with the system D3D compiler. Eleven Python tests cover review,
arming, corrupt artifacts/recipes and historical review compatibility.

Use `tools/disassemble_temporal_shader.py` only on the private artifact;
keep shader bytes and disassembly in the ignored local evidence directory.
This tool performs no GPU work. CPU descriptor handles remain metadata:
typed-view, GPU-consumption and motion-content verification stay false.

Current preparation/deployment/rollback directory:
`local/temporal-shaders-20260925/v1.1.0/`. The deployment receipt determines
whether it currently occupies the MO2 test slot.

## Shared observer contract

This standalone diagnostic follows the accepted depth sequence. It installs
one hook at the reviewed compute request producer and admits only the ordinary
TAA return address. It does not install the projection recorder's hooks,
inject jitter, change settings, submit GPU work or call a vendor SDK.
Shared source supplies identity checks, startup trampoline reservation,
read helpers and native forwarding. The original producer always runs.

Prerequisites: pinned executable, DX12, DLSS Off, no live legacy provider,
and the bare MO2 test profile. Output stays in an explicitly configured
repository directory. The worker reserves trampolines at startup and waits
up to 30 minutes for a process-generation-specific arm event. Capture lasts
at most two seconds and records at most three distinct ordinary TAA requests.

The snapshot checks the primary view, layer vtable, renderer/backend/device,
frame/epoch/bank/async stamp, named binding IDs and source-owner relationships.
Seven resource references per request retain native identity through closure.
Each QueryInterface and GetDevice result must agree with the engine pointers;
GetDesc records the actual format/dimensions instead of assuming the old
DLSS input formats. No role is accepted as motion based on its format alone.

On producer return, the snapshot must be unchanged, queue count must increase
by exactly one, and the last packet must match the captured state and dispatch
dimensions. Queue address/count/packet are rechecked before publishing the
receipt. Exceptions propagate through `__finally` cleanup. Missing inputs,
changed sources, reentrancy, repeated frames or expiration stop observation.
Resources release only after admitted callbacks drain. The DLL and trampolines
remain pinned for process lifetime; the hook is disabled at capture closure.

Validation includes bounded-read/source/packet rejection, three-slot history
rotation, an assembly fixture for RBX, six arguments and the native return,
plus real D3D12 WARP resource tests for completion, changed stamps, bad opcode,
exceptions, foreign callers, timeout, a stalled original, reentrancy and
duplicate frames. The success case forwards a fourth call without capturing
it and releases all 21 retained references. Pure Python tests exercise the
reviewer and armer against the owned-resource fixture and damaged records.

The reviewer accepts only three CPU request observations. Its output always
keeps GPU consumption, motion contents and DLAA readiness false. See the
[static input map](../research/ordinary-taa-input-map.md) for offsets and limits.

## Local runtime procedure

1. With the game closed, use the ignored deployment script and preserve the
   exact previous MO2 DLL and output sidecar. The preparation receipt binds
   the tested DLL and archived sources by SHA-256.
2. Launch the existing MO2 DLSS test entry in the bare profile. Leave DLSS Off
   and TAA On, load a save, and stay in the scene.
3. Verify the new `inputs-<pid>-<creation>.jsonl` reports ready. Use
   `tools/arm_temporal_inputs.py` with that log and the prepared DLL hash.
4. Require armed/capture/stopped records; review with
   `tools/review_temporal_inputs.py`. Preserve a rejection rather than relaxing
   the predicate to make the run pass.
5. After normal game exit, run the deployment script's Restore action and
   verify the original DLL/sidecar and unchanged settings. Archive evidence.

No game executable or original asset is modified. The current local slot and
rollback instructions are in `local/temporal-bindings-20260925/v1.0.0/`.
