# Prepared frame output: v1.25.17

2026-09-30. This revision addresses CPU recording order in the continuous DLSS
output route. It is an implementation to test, not a claim that all visible
skips or unknown post-processing paths are fixed. The previous accepted live
run remains v1.25.16 in `local/prepared-color-chain-20260930`.

## Change and evidence

The previous capture rejected frames when grading or the final draw had already
recorded on the CPU, even though their lists were submitted after the temporal
list. The new route prepares its packet/resource contract after the complete
native post queue returns, before the recording workers execute those packets.
It retains the native motion, optional haze, grading and final-screen adapters.
It does not add an effect exception or replace that qualification with a general
unknown-shader policy. Unknown color chains still keep the original path.

Each admitted frame has an upload-buffer predicate, initially zero. Downstream
copies and draws record both the original-size and full-size alternatives under
opposite predicates. Only the selected alternative performs GPU image work.
The temporal callback selects full output only after writing reconstruction or
full-size spatial recovery. A forwarded/unclaimed resolve leaves the original
alternative selected. The value is sealed once, before any participating GPU
submission, and is never recycled while a recording can still reference it.

This separates CPU recording order from GPU dependency order without waiting
for another recording worker. Barriers and binding restoration remain
unconditional. Predication is scoped to the substituted draw/copy; an existing
native predicate is rejected instead of overwritten. API-derived Reset,
ClearState, SetPredication and bundle invalidation establish that state.

Missing depth/history can produce a full-size spatial frame when the current
color/output contract is valid. A busy private-input recording instead uses the
existing unfiltered original-size path, and every downstream pass follows the
same choice. This does not reuse stale depth or temporal history, and spatial
recovery is not reported as successful DLSS. The input-validation and shader
identity checks remain. A hard invariant/SDK failure still stops the experiment.

## Ordering, ownership and stop

`FrameHandoff` records the logical resolve, copy and effect nodes against exact
packet identities and list generations. Different lists may record in any CPU
order; nodes within one recording must remain in logical order. Submission must
consume a prefix of the prepared GPU order, on the same DIRECT queue for that
frame. Split submissions are supported. This model does not prove native
barriers or shader bindings; those remain separate qualification contracts.

The runtime now fences downstream-only batches as well as the private temporal
recording. Every participating recording pins the resources, selector, native
PSO/root/VB and descriptor heaps it uses until both successful Reset and GPU
completion. A failed submission check seals undecided selectors before native
Execute is forwarded, latches a failure, and does not claim completion.

Resource-state observations are scoped to the exact temporal/copy/draw native
packet callback. Unrelated earlier frame work cannot populate a prepared
frame's recording registry merely by touching a shared scratch texture. A haze
read may inherit the unchanged native SRV state from an earlier GPU list;
contradictory local transitions still reject it. GPU ordering and original
barriers continue to establish dependencies.

Stop blocks new admission, drains already-prepared frames, restores render
scale, and disables all hook owners. Selectors and referenced objects remain
pinned until the same completion/reset rules permit retirement. No wait was
added between game recording workers. Unscaled DLAA retains its existing route;
the shared observer defaults and submission behavior are regression-tested.

## New hooks and ABI

Tested host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded executable SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.

- Native copy executor RVA `0x259c650`, Windows x64: RCX executor, RDX
  renderer context, R8 copy packet. Executor +8 selects recording state;
  state +0x20 is the list, +0x158 is the active packet. The native function
  sets +0x158 before converting state and issuing CopyTextureRegion. Its
  1,214-byte gate is SHA-256
  `299ede85b4e2bbfb25f0ae2c20e003317efc2809c591eb46677ec154495daf88`.
  The existing register-preserving forwarding wrapper remains responsible for
  native arguments, return values and SEH cleanup. The hook only recognizes
  an exact prepared copy packet. It does not own the native packet memory.
- `ID3D12GraphicsCommandList::SetPredication`, vtable slot **55**, Windows
  COM x64, observed after forwarding. Resource/offset/operation are tracked per
  successful Reset generation. The owning runtime pins admitted command lists.
  This hook is optional and only enabled for the new scaled route.

The first isolated predicate-state test caught an incorrect slot 56 selection;
it was corrected to 55 before deployment. The test checks a non-null predicate,
refusal to replace it, ClearState, and restored disabled state after our copies
and draws. An earlier mixed-fixture failure was its bootstrap fence value, not
a game result. Initial compiler/fixture failures remain in local evidence.

Microsoft documents GPU-time predicate sampling and support for predicated
draws and texture copies in [Predication](https://learn.microsoft.com/en-us/windows/win32/direct3d12/predication).
[ClearState](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-clearstate)
disables predication. Upload selectors use GENERIC_READ, including the required
PREDICATION/INDIRECT_ARGUMENT read-state bit.

## Verification and next runtime acceptance

Evidence and receipts live in `local/frame-output-20260930`. The deployment
receipt, when present, is authoritative for the installed hash. Private shaders,
disassembly, binaries, source freezes and captures remain ignored.

Offline checks include the 1,631 pure recording partitions/permutations;
synthetic WARP/5090 output and lifetime checks; production effect routing with
all six CPU list orders, single/split GPU batches, original/full/spatial choices,
stop, alternating haze and aliased scratch targets; and actual runtime
submission callbacks. Readbacks check nonconstant final output against the
in-order reference, exact copied pixels, and untouched padding in the native
branch. The fixtures do not evaluate NGX or run the game.

An intermittent RTX 5090 GPU-validation warning was isolated to the fixture's
1x1 auxiliary texture upload. The same warning reproduced with a minimal shader,
without any hooks, predication, game shaders in the pixel stage, or runtime
submission callback. Adding `ID3D12DebugCommandList::AssertResourceState` after
the explicit upload transitions verified the expected read states; the
instrumented isolation runs and complete WARP/5090 route matrix passed. The
exact validation discrepancy is unresolved. Preserve the failing isolation logs
rather than interpreting this as a game-renderer bug or filtering the warning.
The assertions are fixture-only; no debug API or extra barrier was added to the
game.

The route matrix covers 1,152 frames (96 per case on both backends) plus six
pre-submission rejection cases. Submission also requires a successful Close
receipt for every participating recording, including downstream-only batches.
The negative fixture removes that receipt while keeping the actual native list
closed, verifying rejection without submitting an invalid D3D12 list.

The JSON reviewers distinguish prepared frames, reconstructed/spatial/native
choices, downstream-only signals and multiple retained recordings. Route and
readiness schema 3 apply to v1.25.17 and v1.25.18. Older captures retain their original
requirements. Integrated synthetic receipts exercise the new accounting and
deliberate corruption rejection; they are not live evidence.

The next run starts at the main menu, with native AA/DLSS/MotionBlur Off and the
existing window size. Check startup provenance, arm, verify successful frames,
then load a save. Compare early-draw and multi-list counts with choices, all
submissions/completions, panning/menu transitions and one area load. A remaining
skip must be classified as output timing, missing temporal input, or an
unqualified graph. Do not claim universal effects support or visual stability
from the offline fixture results.

To remove this test installation while the game is closed, use its local
`deploy.ps1 -Action Restore`. The exact previous DLL and sidecar are preserved.
Restoring that sidecar points to sealed prior evidence; prepare a fresh capture
destination before another launch. No game executable/assets or settings are
modified by installation. Keep the latest DLL installed after the test unless
the user changes that preference.
