# Independent-AA projection and bindings v0.6.0

This build connects the proven AA-Off entry to the existing scoped projection
jitter recorder and native GPU binding observer. Eight consecutive projection
receipts must match eight owned requests and their qualified GPU read points.
The first and last frames are unchanged controls; the middle six use distinct
native-table jitter samples. Receipts 0/3/7 save color, depth and packed motion.
There is no SDK evaluation, output handoff, native AA setting write or TAA fallback.

Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded analysis image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
The six scheduling/submission hooks, phase 15/order 2 packet, resource states,
copy sizes, code gates and completion/retirement rules remain those documented
for [v0.5.0](independent-aa-input-readback.md).

## Projection identity and scope

The added native hook is the Windows x64 projection helper at RVA `0x228f3b0`,
caller `0x228e834`, with the existing integer/vector-preserving ASM wrapper.
The 2,000-byte code gate is
`df1bf81584401fe529bc5fbbc3e19010e8959c2d13745eca6f87897183148373`.
RCX must be the active camera +0x50 and RDX the renderer +0x710. Camera type,
frame/epoch stamp, primary view, renderer bank, async mode, provider/device,
native-DLSS absence and viewport are validated by the existing jitter module.

An initial unchanged baseline is selected only if the next native table sample
has both components at least 0.3125 in magnitude, bounded by 0.5. Selection is
limited to 20 eligible projections. Six subsequent candidate pairs must be
distinct. The camera +0x38 pair is exchanged from zero to the candidate only
inside the native helper call, then restored by compare/exchange in `__finally`,
including exceptions and stop requests. No persistent camera or option edit.
The capture is bounded to eight projections and the existing two-second window.

Before an owned pass can be inserted, its immutable PostQueue frame, epoch,
bank, view and context plus the same-invocation async byte must match the
completed projection receipt. No request is admitted before the first selected
projection. A mismatch stops the capture instead of skipping to a later frame.
The returned projection constants are copied twice, matrix identities checked,
and projection/view-projection jitter relations independently reviewed.

At the GPU read point, the request's immutable receipt index retrieves the
already acknowledged projection. Its frame must equal the request's source
frame. The renderer's later global counter is not substituted. Logs retain the
projection epoch and exact jitter bits beside the Reset generation and packet.
This verifies CPU receipt association; actual raster shifts still require pixel
analysis. It does not itself establish DLAA history or image quality.

## GPU binding observation

Eighteen existing Windows x64 COM hooks observe command-list slots
11, 25, 27, 28, 29-42: ClearState, PSO, bundles, descriptor heaps, both root
signatures, tables, constants and CBV/SRV/UAV addresses. Targets are resolved
from an owned DIRECT list and validated as system methods with no aliases.
Together with the projection hook and previous six hooks, the total is 25.

The existing submission Reset hook supplies the successful Reset generation and
initial PSO; it is not hooked a second time. The observer starts before the
submission warmup. All admitted lists remain pinned by that journal. Each input
read point requires a known valid binding set with PSO and compute root on the
same Reset generation. Both compute and graphics arguments and shared heaps are
saved. Expected root/PSO fields identify this observed state, not a separate
shader-cache proof. Incomplete state is logged diagnostically and rejects work.

The nine input copies alter no pipeline bindings; the mirror must remain exactly
unchanged afterward. This run establishes the binding set a later compute pass
must restore. It does not exercise SDK state changes or claim restoration of
arbitrary vendor behavior. Hooks drain before journal references are released;
all existing copy completion and recording-retirement requirements still apply.

## Verification and next decision

Release /W4 /WX build. 38 native checks pass: 26 eight-receipt scoped jitter and
association tests, seven real D3D12 WARP binding/readback cases, and five prior
readback cases. The WARP fixture checks exact native compute/graphics constants,
PSO/root identities, all copied pixels and a clean D3D12 debug queue. Missing
compute root or unobserved Reset generation rejects the new path; stale frame,
epoch, bank, async, context and view reject association. Scoped exception,
competing-write, reentrancy and missing-ack cases remain covered.

All 41 AA Python tests pass. The new reviewer checks exact version/scope, full
queue/packet/GPU chains, projection matrices and identities, binding generations
and root arguments, and nine image hashes. A passing runtime capture would allow
connecting private conversion/DLAA recording at this entry. Output handoff,
other-AA suppression and production failure latching remain separate work.

## Test and rollback

Ignored session: `local/independent-aa-projection-20260926/v0.6.0/`.
Use the existing MO2 test entry, bare offline profile, DX12 On, Windowed
2560x1440, native AA Off and DLSS Off. Load a save and keep the camera/character
still; a pose can reduce idle animation. Nothing is hooked before arming.
The sealed `arm.py` verifies artifacts/configuration before calling frozen
`tools/arm_aa_projection.py`. Events:
`Local\\MhwAaProjection-v1-PID-CREATION-1` (arm), suffix `-0` (stop).

Header: version 0.6.0, experiment `independent-aa-projection-bindings`, eight
projection/binding receipts, six stimuli, twelve scoped jitter writes, 27 private
image commands and eight Signals. Requested counts are budgets, not proof of
successful execution. The final reviewer supplies that proof for a complete run.

The exact existing MO2 DLL is copied to `previous.dll` before installation.
After normal game exit, run this session's `restore.py`: it verifies test/backup
hashes, restores the original slot and removes only the matching `.output-dir`
sidecar. It reports configuration hashes without rewriting user settings. The
separate mode sidecar remains unchanged. Original executable/assets are untouched.
