# Bounded native jitter depth sequence v0.9.0

Runtime outcome: [run 49240](../research/depth-sequence-capacity-result.md)
reached all five slots but exhausted the 4096-entry history before retirement
validation. No pixels were saved; the run is rejected and the previous DLL has
been restored. The following describes the original v0.9.0 protocol.

This diagnostic extends the accepted [v0.8.4 triplet](../research/depth-triplet-result.md)
to five independently stamped CPU/depth samples: zero baseline, three scoped
native jitter samples, and zero recovery. The preceding capture measured the
expected response on static depth interiors for one sample. This version tests
multiple samples in one bounded window; no provider or sustained mode is added.

## Mutation and lifetime contract

The sole data mutation still surrounds native helper RVA `228F3B0`, Windows x64
RCX = camera `+50`, RDX = renderer `+710`, AL = native result, accepted return
site `228E834`. The tested executable is version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
Native byte gates, calling conventions and borrowed ownership assumptions from
the [triplet](projection-depth-triplet.md) remain unchanged: 41 hooks, seven
startup gates, 39 core gates and 45 distinct code spans. No new hook is added.

Each admitted call requires the existing typed camera, frame/epoch, view,
provider, arena and matrix checks. Samples have increasing frame/epoch values,
stable camera/context/thread/viewport anchors and their own immutable source
copies. The native pair at `table+E6A0+8*(frame%20)` must be finite, nonzero and
within ±0.5. All three injected pairs must differ. The sequence does not select
a favorable pair or rewrite the native table.

For stages 1, 2 and 3, compare/exchange camera `+38` from zero to that call's
native pair; forward once; then restore to zero in `__finally`. Restoration
runs even on exceptions, explicit stop or bookkeeping failure. A competing value
is preserved and rejects the run. Baseline stage 0 and recovery stage 4 do not
mutate the field. Each source is published only after successful restoration.

Stage 1 retains the complete publication/consumption/root-CBV/native-draw chain.
Stages 2 and 3 cannot start until that draw has returned, passed its existing
stability checks and released its protected source arena. Up to 64 unreserved
primary-helper entries may pass through untouched while waiting; the next ends
capture. These waits are recorded separately as `draw_waits`. Accounting also
stops when admission closes. Critical contention remains fatal for every
mutation and admitted exit. Only the final, unreserved zero-recovery entry may
defer on contention, under the existing 64-deferral budget.

The selected source and draw remain stage 1; later sources cannot replace them.
Overall completion requires that draw plus all five CPU samples. Because the
first draw and recovery may arrive asynchronously, this is a bounded sequence
of eligible calls, not a promise of five consecutive display frames. The exact
frame gaps are logged. The capture window remains two seconds, with at most
three injections/restorations and no rearm.

## GPU and evidence contract

Five private READBACK buffers/fences are allocated before arming. Each slot
admits at most one copy and one Signal and retains the same original-request
frame/epoch/bank/async/context/thread checks. Total allocation is at most
5×64 MiB; five 2560×1440 pixel payloads total 73,728,000 bytes. Existing history,
packet, barrier and copy capacities remain bounded and unchanged. Exhaustion
rejects the sequence; it does not overwrite earlier evidence.

Stage 1 keeps its before-Signal draw-return, selected-submission, depth-view and
same-queue checks. Stages 0, 2, 3 and 4 are request-associated readbacks with
`site.request_control=true` and marker 0. This denotes their narrower binding
evidence; stages 2 and 3 are jittered CPU samples, not zero-jitter controls.
Before Map, each must match the selected pinned depth/canonical identity and
surviving writable view. Private completion and command-list retirement are
required for every slot. Issued buffers remain retained until process exit.

Header version `0.9.0` names `projection-jitter-depth-sequence`, three jitter
writes/injections, five CPU samples, five copies/Signals, and both deferral
limits. Startup requires `sequence_readback=true`, `readback_slots=5`, plus the
existing frame/triplet readiness. The main `-depth.f32` file is stage 1; four
`depth_control` records use `-depth-control-{0,2,3,4}.f32`. That filename/schema
retains the request-control designation, not a claim about zero jitter.

The independent reviewer verifies all five CPU samples and restorations, distinct
native pairs, later samples after the primary draw return, all resource/request
associations, ordered non-overlapping queue submissions, one DSV lifetime and
distinct private allocations. Missing middle/final samples cannot be replaced
with later unrelated copies. It reports five request frames and predicts each
injected pixel shift using `(jx/2,-jy/2)`. Broad raster/full-scene flags remain
false until separate pixel analysis; no control draw bindings are fabricated.

## Verification and operation

Native core fixtures exercise the five-sample chain, final recovery deferral,
draw-wait exhaustion and post-stop behavior, repeated/zero samples, later-call
exception, competing write, provider change, explicit stop and changed recovery.
GPU fixtures route five distinct clear values through real D3D12 readbacks and
reject missing baseline, wrong middle/final request stamps, changed view and
duplicate consumption. Clear values test routing, not raster displacement.
Python checks damage the complete sequence review and separately review actual
owned CPU hook output; full core/GPU composition remains labeled synthetic.

Use the existing `barebonesdlss` profile and **Monster Hunter: World - DLSS
1.2.110 test** entry, preferably offline. Keep DLSS Off, TAA On and DX12 On.
Load a save, keep the camera still and report ready from the phone. Arm only the
verified process generation; no DLSS menu toggle is needed. Preserve the
capture and all five images, exit normally, then restore the previous slot.

Local preparation, deployment and prelaunch receipts are under
`local/projection-binding-20260923/v0.9.0/`. The deployment script preserves the
exact previous destination DLL and output-sidecar state. With the game closed,
`deploy.ps1 -Action Restore` restores them and verifies all four configuration
hashes. Original game executable/assets remain untouched. No v0.9.0 game result
is claimed at preparation time.
