# Native prepared color-chain capture: v1.25.16

2026-09-30. This is the next diagnostic for the shared output migration described
in [the renderer boundary audit](shared-renderer-boundary.md). It includes the
v1.25.15 scheduled depth read-interval correction. It does **not** replace the
existing motion/haze/grading/final adapters or remove the executed-draw guard.

## Why this observation is needed

The native scheduler assigns the entire queue's inputs and outputs before it
calls individual units. A unit may prepare work at a later sort key; the final
screen draw may already have been enqueued before post-filter callbacks run.
Therefore callback order, prepared-packet insertion order and GPU execution
order are three different things. The current output prototype conflates some
of their ownership requirements. Adding another shader adapter will not fix
that structural constraint.

The queue's descriptor contract provides a way to inspect every participating
filter without recognizing its vtable or shader. This capture adds the missing
dimension and prepared-command evidence to those links. It is a diagnostic
foundation for selecting a common handoff, not an implementation of that handoff.

## Scope and data

`prepared_queue.hpp` is invoked from the existing, admitted outer postqueue
completion callback, before the stream route's effect-specific admission checks.
It does not require a published temporal plan or a recognized motion unit.
The current activation policy still supplies the admitted TAA unit; this is not
an observer for every arbitrary renderer/view in the application.

One capture is attempted on the first completion and every 128 completions
afterward. It retains the first sixteen successful captures plus the latest.
Status logging preserves later checkpoints. Each attempt reads the following
bounded fields twice and publishes only matching results:

- Frame/epoch, renderer bank/async byte, context, primary view and queue identity.
- Current context rectangle and the view's three scale values, stored as bits.
- The two scratch owners and up to sixteen native descriptors: phase/order,
  unit/type, the three native flags, and input/output engine metadata.
- Owner/resource identity, allocation width/height and format metadata. Failed
  owner reads are explicitly marked unknown, retaining only the owner address.
- A scan of at most 65,535 prepared queue entries, in blocks of 256. For each
  descriptor's key, record the complete opcode histogram and the first two draw
  packet/attachment/rectangle observations. Record final phase-16/key-0 draws
  separately. An empty command count is valid evidence for a setup descriptor.

The chain-break count is only a recomputed adjacency diagnostic from the TAA
descriptor onward. Scratch A -> B -> A is permitted; pointer reuse is not itself
a broken link. A skipped-callback endpoint can still describe a color edge and
have matching GPU work prepared by another callback. Unknown types, flag
combinations, disconnected links and unreadable owner metadata remain visible.
They do not become exceptions that qualify the existing DLSS route.

The snapshot is not a GPU lifetime/state proof. Engine allocation dimensions
are not `GetDesc`; matching sort keys do not prove texture binding or submission;
the first two draws are explicitly a prefix; equal aggregate observations do
not prove every unretained packet immutable. No shader semantics or color-space
contract is inferred from the graph. This evidence must not be used to authorize
full-size draws, copy stale depth, or move reconstruction after grading by itself.

## Hooks, ownership and failure behavior

No hooks, COM references, GPU copies, shader edits or state mutations were added.
The existing Windows x64 `0x24f1fc0` wrapper preserves incoming manager/context/
view across the original call and reports only a successfully returned owned
queue. The existing stream-route mutex serializes capture and status output.
The recorder stores values only and never dereferences retained addresses later.
Capture failure increments an identity/bounds/descriptor/packet/change counter;
it has no effect on renderer admission or original native rendering.

The scheduler's pinned 835-byte SHA-256 is
`1ec618caf8d1d455cc12fc12c57889b73e1073074faf04e82364fba10d01e190`.
Tested host SHA-256 is
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`,
version 0.0.0.0; decoded executable SHA-256 is
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
All existing code gates and runtime fail-closed checks remain required.

## Offline verification

Both DLL targets and the new fixture build with `/W4 /WX`. Forty-seven relevant
native CTest targets passed. The new owned-memory fixture covers unfamiliar
units, a no-callback endpoint, scratch reuse, reduced active rectangles in full
allocations, final commands enqueued first, malformed pointers/counts/scales,
torn frames, missing owner metadata, and bounded sampling/retention. It emits
the real C++ schema for independent Python review.

Forty-six Python tests passed with their required fixtures, and eight integrated
schema/receipt corruptions were rejected. The sealed v1.25.14 log remains
accepted. An explicitly synthetic v1.25.16 complete log verifies new-version
integration and is **not** live game evidence. Initial test invocation omitted
the existing review/depth fixture arguments; those commands were corrected and
rerun without skipped tests. The new CMake fixture initially needed the shared
include directories; the corrected build has no project compiler warnings.
The existing third-party MinHook CMake deprecation notice is unchanged.

The earlier v1.25.15 WARP/NVIDIA depth tests still apply to unchanged depth code;
no new GPU test is claimed for this read-only CPU observer.

## Next live checkpoint and installation

Use the same bare offline MO2 test entry, DX12, windowed 2560x1440, native AA,
native DLSS and MotionBlur Off. Start at the main menu. Verify the new startup
identity, arm and start the collector; inspect depth admission and the prepared
queue before asking the user to load a save. Capture an ordinary loaded scene
for comparison. Do not expand this into a per-effect test list yet.

Evidence/build/source freeze: `local/prepared-color-chain-20260930/`.
The deployment script preserves the exact previous DLL and sidecar, redirects
output to this fresh root, and verifies configuration identities. Removal of
this test means running that directory's `deploy.ps1 -Action Restore` while the
game is closed; it restores the saved slot and sidecar. Do not launch afterward
into the restored sidecar's sealed archive without preparing another fresh root.
The user's preference is to keep the latest diagnostic DLL installed on normal
test completion. Runtime behavior of v1.25.16 is still unverified at preparation.
