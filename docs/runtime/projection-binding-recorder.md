# Read-only projection binding recorder, v0.1.3

This describes the completed read-only versions. The subsequent
[v0.2.0 stimulus protocol](projection-binding-stimulus.md) retains this hook and
binding contract but adds one scoped mutation and three CPU observations.

Prepared 2026-09-23 following the accepted CPU experiment in run 17072 and
the [offline map](../research/projection-upload-binding.md). The first v0.1.0
run passed live gates and recorded source/publication, but stopped on recorder
contention before backend consumption. See [run 33032](../research/projection-binding-first-run.md).
The v0.1.1 run failed before installing any hook because trampoline allocation
failed. See [run 23832 and the v0.1.2 reservation](../research/projection-binding-allocation.md).
The v0.1.2 [run 6444](../research/projection-binding-result.md) subsequently
accepted the source-to-root-CBV/draw chain with clean shutdown. Its previous
DLL was restored. The [v0.1.3 run 31912](../research/view-projection-contract.md#run-31912-zero-jitter-contract-accepted)
then accepted the zero-jitter matrices and native root-layout/register
correlation. Its previous DLL is restored and all configurations are unchanged.
This probe does not implement DLAA or DLSS.

## Scope and evidence levels

With the bare profile, DLSS Off, TAA and DX12, retain one eligible loaded-scene
projection result, its exact kind-11 packet entry, the D3D12 backend slot, and
the first native graphics draw using that slot. Read-only checks require
stable source bytes, valid arena ranges, the actual resource GPU addresses,
and no provider wrapper or resolution-changing sizing flag. Raw renderer
`mUseDLSS == 1` remains permitted. No camera, TAA or projection field is written.

Twenty hooks forward the original calls once. Ten are native: producer
`228F3B0`, publication `23D2A80`, consumer `259E1E0`, state `259E670`, five
draw parents `259D670/740/820/930/A10`, and arena rollover `23A2B50` (RVAs).
Ten observe D3D12 methods: graphics root CBV/table/signature, PSO, viewport,
draw/indexed/indirect, device CreateConstantBufferView and list Reset. API
events are recorded only within the selected draw on its owning thread.
Concurrent recording/reset of that same command list is outside this scoped
observer's supported ownership assumption. It is not a full command-list trace.

Native x64 arguments and RAX/XMM return values are preserved by ASM forwarding;
native returns other than the producer's bool AL are not assigned invented
C++ types. EFLAGS are volatile under the ABI. Typed API stack arguments are
copied by count. SEH cleanup preserves exceptions and LastError. A conservative
decoded-prefix gate rejects relocated CALL instructions because MinHook's
anonymous trampoline does not carry unwind metadata.

All 36 mapped code spans, camera vtables and the exact executable SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`
are checked live before hooks. The mapped image timestamp is `66D13CB6`,
size `55D3000`, file version `0.0.0.0`. API discovery uses an owned, empty
command list on the engine device, never submits it, and accepts only executable
System32 D3D12 implementation targets. Each admitted API object must expose
that exact target. Owned COM references cover discovery; render callbacks only
read borrowed objects within the native call that owns them. The actual device
LUID, swap-chain description, buffer resource descriptions and addresses are logged.

The allocation arena and consumed arena are different concepts. Ranges and
used cursors establish containment, while the backend delta must independently
match the consumed GPU address. The rollover hook rejects reuse of the selected
arena, including identical-byte reuse. Constants are capped at 4096 bytes;
five deduplicated shader containers at 256 KiB each are copied and rechecked
in scope. Source matrices must also match their copied buffer bytes. Shader
descriptors are revalidated after native state setup.

The first completed native draw ends admission; a two-second deadline, visit
bound, capacity checks or invalid observation can end it earlier. Callbacks
use a combined admission/in-flight gate and never hold the bookkeeping lock
over an original call. In v0.1.1, immutable source/publication/consumption keys
are published through atomic phase transitions and filter unrelated callbacks
before lock admission. The arena lifetime guard uses atomic overlap/reuse state
and never enters the capture lock. Relevant lock contention still invalidates
the capture. The first contender's hook site/thread is retained, along with an
owner sample when its value is stable around the failed acquisition. Zero owner
means unknown; the sample is not an exact lock-acquisition history. Site encoding
is `2 * (Kind + 1) + exiting`, where `exiting` is 0 at entry or 1 after return.
After draining, only owned hooks are disabled. Trampolines and the DLL remain
pinned until process exit so an already-entered forwarding path cannot use
freed code. Shader files are written afterward with CREATE_NEW, under ignored
local evidence only. Nothing from the game is distributed in source control.

Independent review distinguishes:

- `root-cbv-call-chain-observed`: the matched GPU address was passed to an
  actual root-CBV API and remained bound through an observed draw call.
- `partial-binding-evidence`: a draw and shaders were captured, but an actual
  usable root-CBV bind was not observed. Cached addresses alone do not count.
  Descriptor creation is retained but requires later root-layout/table mapping.

Neither result proves command submission, GPU constant contents, shader
execution or use of that constant slot, raster displacement, or temporal
reconstruction. Captured VS/PS/GS/HS/DS bytecode is a bounded candidate set from
native graphics state; PSO identity may still be a native cache observation.

## Runtime procedure

1. Launch the existing MO2 test entry with `barebonesdlss`, no enabled mods and
   Stracker off. Prefer offline/private gameplay. Leave DLSS Off and TAA on.
2. Confirm the new process generation and matching DLL hash. Require the
   `trampoline_reservation` and `ready` records: a 4 KiB block is reserved,
   with 62 remaining slots reachable from all ten native targets. No hooks
   are installed at this point. Load a save and stay
   in a normal scene. The recorder can wait up to 30 minutes for its arm event.
3. Use `tools/arm_projection_binding.py --log <absolute JSONL> --action arm
   --output <new receipt>`. The helper validates PID plus creation time and
   signals only that recorder's event. A sent request is not an acknowledgment.
4. Require `armed`, then `capture_complete` and `stopped`. Inspect any failure
   before requesting another run. Do not toggle DLSS or change display mode.
5. Run `tools/review_projection_binding.py <log> --output <new review.json>`.
   Inspect local shader blobs only after their hashes and container bounds pass.
6. Ask for normal game exit. Restore with the archived deployment helper, then
   verify the previous DLL, sidecar and unchanged settings/profile/mode hashes.

## Deployment and removal

Machine-local slot: `D:/MO2/MHW/mods/dlssTEST/mhw_dlss_bridge_probe.dll`.
The helper and receipts are under ignored
`local/projection-binding-20260923/v0.1.3/`. `deploy.ps1 -Action Install`
requires the game closed and the old v0.15 DLL hash
`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
It preserves the exact previous slot and any existing `.output-dir`, snapshots
four configuration files without editing them, archives sources and the new DLL,
and verifies deployment. `deploy.ps1 -Action Restore` refuses unexpected changed
files, restores the old DLL and restores/removes the sidecar as recorded.
No game executable or original asset is patched on disk.

## Verification

Release `/W4 /WX` build, native owned-memory fixtures and an independent Python
review suite cover ABI forwarding, return registers and LastError, nested SEH,
pending-stop drain, pointer/packet mismatch, arena reuse/overflow, post-deadline
forwarding, unsafe trampoline prefixes, shader hashes, root binding chronology,
cached-binding evidence, and generation-bound control. Owned fixture captures
are explicitly marked and cannot be armed or accepted as game evidence.
v0.1.2 adds reservation-without-patching and paired address-space-pressure
fixtures: fresh allocation fails, while a reserved pool allows full capture.
There are 18 native and 23 Python tests. Reservation uses the private buffer API
of pinned, unmodified MinHook v1.3.4; its geometry is version-specific. The
worker checks dynamic-code policy and fails closed if prohibited or unavailable;
it never changes policy. The reserved slot and block live until process exit.
Ten API hooks in system modules still allocate through MinHook normally.
Exact counts, hashes and retained test output are in `preparation.json` beside
the deployment receipt. The actual game's live gates passed in v0.1.0 run 33032;
every arm still rechecks them. v0.1.2 run 6444 passed fresh gates and accepted
the recorded chain, with zero drops/faults, two shaders and a root-CBV bind at
parameter 9. This does not qualify shader execution or raster jitter.

v0.1.3 retains the same hook targets and limits, adds three dynamically located
matrices, and revalidates root selection after state setup and draw. Seven extra
code gates cover the mapped root constructors/serialization route. It passes
24 native and 34 Python tests; a separate twelve-case static builder replay
supports the layout interpretation. Run 31912 passed all 36 live gates and
accepted the zero-jitter matrices, compact root selection and VS b0 binding
correlation, with the same shader containers as run 6444. The archive includes
an acceptance replay and verified rollback. This completes this read-only
baseline; a nonzero-jitter follow-up needs its own implementation and protocol.
