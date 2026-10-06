# Shared renderer boundary work: v1.25.15, offline only

2026-09-30. The user asked to stop extending DLSS by chasing individual screen
effects. This revision implements the first shared resource-state correction and
records the separate output migration constraints. It does **not** claim that
the effect-specific output route has been replaced. Nothing was installed or
armed; the MO2 slot still contains v1.25.14.

## Evidence and scope

The last sealed run is `local/main-menu-variant-20260930`, PID 35100. Its archive
SHA-256 is `537bf5f6546bf231d0984fc4230d07453579fb9ba7e1e8d8440cd2adff22fc2d`.
See [the main-menu and rain result](main-menu-rain-depth-result.md).

There are two independent issues:

1. Depth admission assumed a writer followed by one pixel-only read, with DLSS
   strictly between them. The main menu used a combined pixel/compute read;
   the rain-reported scene reached DLSS inside an already-started read span.
   Rain itself was not established as the cause.
2. The output route cannot claim a plan once a downstream draw has been recorded.
   Loading captured the final draw recording first on the CPU while its command
   list was submitted after the temporal list. GPU order was suitable, but the
   opportunity to alter the recorded draw had already passed.

The first issue can be corrected from the existing pinned planner contract.
The second requires changing preparation and ownership, not deleting its guard.

## Implemented depth interval proof

`depth_consumer_admission.hpp` now interprets the complete, bounded sequence of
resource-use spans following the selected depth writer. There are no scene,
weather, menu, effect-type, or shader-name exceptions.

- Preserve the exact writer, owner/backing/resource, rectangle, epoch, complete
  span chain, unique schedule identities, double capture, and local API checks.
- The selected writer remains the qualified kind-8 nested draw with native
  state 12 -> 256 and its sole last-use ordinal at the outer writer.
- Locate every subsequent span's first scheduled use. Require unique identity,
  first use after the previous span's last use, and last use at/after first use.
- Read every state-node list from writer through reconstruction, including the
  existing qualified nested-copy layout. Apply transitions in actual node order.
- Each read transition must match the next planner span exactly. Its node must
  occur after the previous span's last use and no later than its own first use.
  This handles both native hoisting and reads that already began before DLSS.
- Every span that has begun by the consumer must have its transition accounted
  for. Missing, duplicated, reordered, aliased, writable or unsupported changes
  reject the copy. Future writes are permitted only beyond the consumer, with
  no transition into that write in the inspected interval.
- Native shader-read bits 4, 8 and 12 map to D3D12 states 64, 128 and 192 through
  the already-pinned converter. State 4 remains valid if the depth is still a
  render target. No other D3D state is accepted by the copy helper.

The copy restores the exact derived state after COPY_SOURCE/CopyResource.
Destination remains COPY_DEST. Existing command-list generation pins, native
schedule-order assumptions, completion fences and reset retirement remain.
No new hook, global scale write, wait between recording workers, or stale depth
reuse was added. Unscaled DLAA keeps its existing writer-copy runtime path.

Bounds remain 32,768 schedule entries, 16 spans/nodes, 256 entries between writer
and consumer, and 128 state nodes per packet list. The arbitrary requirement
that the first later reader be within 128 entries *after* the consumer is gone;
that reader was already located by the complete schedule scan. Native packet
layout qualification remains bounded; this is not universal D3D state tracking.

## Versioned proof receipts

`consumer_depth` schema 2 uses `scheduled-read-interval-depth-copy-v2`.
`read_state_copies` replaces the old `hoisted` counter because a valid read may
now begin normally before the consumer. Each retained receipt includes applied
transitions `[first_use, last_use, transition_index, native_before, native_after]`.
The reviewer independently checks the chain, schedule bounds, derived state,
counter coverage and immutable receipts. v1.25.15 requires schema 2; old logs
retain schema 1 and their original admission interpretation.

## Offline validation

- Both runtime DLLs and depth fixtures build with project `/W4 /WX`.
- 48 selected native CTest cases passed. The depth fixture includes 60 schedule
  permutations plus positive/negative multi-span, state, identity and API cases.
- 35 Python tests passed, plus eight deliberately corrupt integrated receipts.
  The sealed v1.25.14 log is still accepted. Explicitly synthetic v1.25.15 logs
  exercise version/schema integration; they are not live reconstruction evidence.
- WARP and RTX 5090 each passed 64 frames, 524,288 exact float-depth comparisons,
  16 frames in each of four states, seven fence-ordered queue switches, and
  changing native sources. Consumer recording precedes writer recording on the
  CPU; submission places the writer first. All pins retire. D3D12 debug and GPU
  validation report zero warnings/errors.

These checks establish the implementation under the fixture contracts, not
main-menu/rain visual correctness or the game's full interval in a future run.
An initial local reviewer test assumed the menu-first archive retained a
successful read receipt; it did not. The fixture now explicitly supplies a
native synthetic receipt instead of claiming the archive contains one. A
version-list edit syntax error was corrected before the passing review/build.

## Output boundary audit and remaining implementation

The native scheduler at RVA 0x24f1fc0 first assigns inputs/outputs to the whole
filter queue, then invokes unit callbacks. The queue has two scratch owners and
can reuse them. CPU callback sequence is not the sorted GPU packet sequence.
The current route recognizes motion, optional haze, grading and final output,
with private geometry/PSO adapters and a single active recording. That remains
the installed prototype's constraint.

The TAA output helper 0x23917a0 and scheduler copy helper 0x24f1b20 derive copy
rectangles from context +0x5c..+0x68. The broader camera builder at 0x23c3790
starts with a rectangle scaled by view +0x6b8 and later restores the original
rectangle at 0x23c7839 for the final screen path. At 0x23c7a49..0x23c7b08 it
selects a copy or scaling quad using allocation dimensions and view scale.
Those observations identify actual native boundaries, but do not establish that
moving modern temporal reconstruction to the final screen quad is correct.

The existing shader adapters demonstrate another requirement: motion and haze
read reduced depth/motion/noise as well as color. Enlarging all coordinates or
globally changing a rectangle after TAA would misdescribe some inputs and leave
already-prepared commands untouched. Reusing the native final resize alone
would also move reconstruction past native post-processing without validating
the resulting color/exposure/motion contract.

The next output work must therefore establish the render/output dimension
domains during native preparation, before worker recording. It must deliver
reconstructed color at a common post-processing input while native auxiliary
inputs retain correct coordinates, and retain each participating recording
until submission/completion/reset. Admission must be fixed before any consumer
can record, independent of which CPU worker runs first. Unknown effects should
flow through that native contract; the acceptance test must not be another
effect-name whitelist. The native legacy sizing branch is a candidate to study,
not a reason to enable the old provider or fabricate its wrapper.

This output migration is **not implemented yet**. Existing output rejection and
unfiltered recovery remain. The next live run should wait until a fresh capture
root and the intended boundary experiment are prepared; do not launch into the
sealed v1.25.14 evidence folder.

## Identity and local artifacts

Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
decoded executable SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Both were checked before offline inspection. Existing hook ABI and lifetime
contracts are unchanged. The converter at 0x259ba50 remains pinned to 495 bytes,
SHA-256 `f560506e570fff4dd21df27e5d68db204420e82e3eed1bb5aae2cc3ad1441ee5`.

Evidence: `local/renderer-boundary-20260930/`. Private disassembly, manifests,
before-edit snapshots, tests, built v1.25.15 DLL and source freeze stay ignored.
Installed v1.25.14 hash remains
`48567ef9214b302e94f5936507a3a7bd79faeec306e80ce6958eea8d93368e69`.
No deployment, profile/configuration change or runtime mutation occurred.
