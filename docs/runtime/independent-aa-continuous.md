# Independent AA-Off continuous DLAA, v0.8.0

First live run: [partial result](../research/independent-aa-continuous-result.md).
4,196 handoffs completed before queue admission fault 8 during walking/panning.
The run is closed and restored; continuous acceptance and area transitions
remain unverified on this independent route.

This prototype connects the verified independent AA-Off read point to displayed
native color on every eligible frame. Native AA and native DLSS must stay Off.
The renderer receives packed DLAA output through an in-place copy and then
continues its normal post chain. No native TAA producer, TAA resolve, or TAA depth
copy hook is used. Other-AA suppression and general mode selection remain later
integration; this version does not edit the game's AA options.

## Host, targets and scope

Tested host identity remains version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
The startup transaction and all native code gates are inherited from the
[accepted private evaluation](independent-aa-sdk.md). `discover` verifies its
existing code spans, including the empty packet consumer, before hooks install.
Additional scheduler, native queue, marker enqueue and projection spans are
checked explicitly in `stream_runtime.cpp`.

Windows x64 hooks: post scheduler RVA 0x24f1fc0 (caller 0x228e92d), empty native
parameter consumer 0x259e1e0, projection helper 0x228f3b0 (caller 0x228e834), and
the discovered DIRECT-list Reset, Close, ExecuteCommandLists and ResourceBarrier
methods. Register forwarding uses the previously tested assembly wrappers.
The existing 18-hook binding observer provides exact compute/graphics restoration
checks. The separate startup hook is removed before readiness.

Only the verified four-node AA-Off post queue and 2560x1440 layout are accepted.
The owned descriptor is still phase 15/order 2/in-place, and is detached with the
exact original queue restored after each synchronous scheduler call. The owned
empty packet is a separate stable allocation. Neither game executable nor
original assets are modified.

## Continuous ownership and ordering

Thirty-two reusable records hold source projection, frame/epoch, three native
resource pins and one immutable empty marker each. Before jitter, native AA must
be Off, camera/provider/extent valid and resources available. Projection jitter
is restored even when native execution fails or stop arrives. The post scheduler
associates its immutable source frame with that projection and publishes the
marker. The marker's consumer must return without changing native parameters.

The next relevant native barrier must be the previously verified paired
RT-to-PS-SRV transition for color and motion. State is tracked within the exact
successful Reset generation; depth must independently be known PS-SRV too.
Per-list state is bounded to 512 resources; the pinned list registry is bounded
to 128. Split barriers, state conflicts, unexpected aliasing, duplicate markers,
unknown binding generations and resource changes reject the invocation.

All three inputs are copied from PS-SRV (128) into private raw textures and their
native states restored. One persistent full-size DLAA feature performs the
existing conversion/evaluation/packing. The actual source projection determines
SDK jitter `(rawX/2,-rawY/2)`. The existing camera/history policy supplies measured
frame time and resets on discontinuities. Before writing native color, the exact
binding mirror must match its pre-SDK snapshot. Only after successful evaluation
and restoration does the code copy packed COPY_SOURCE output into native color
through PS-SRV -> COPY_DEST -> PS-SRV. Depth and motion are not modified.

One private recording may await submission at a time. GPU executions are ordered
on the native DIRECT queue; a queue change inserts a Wait for the prior private
fence value. Reusing feature descriptors does not require a CPU fence wait each
frame. Every marker and native resource remains pinned until its producer and
consumer returned, its selected recording successfully reset, and its submission
fence completed. Signal return and Reset retirement are independent receipts:
Reset may happen before Signal returns, but cannot authorize early recycling.
Duplicate submissions or uncertain execution freeze retained references.

Native command-list calls are assumed to obey D3D12's requirement for serialized
recording on a single list; the mod serializes its metadata and private work.
Resource identities are pinned while used. No resource identity is inferred
across unrelated Reset generations. The former bounded capture's detailed
cross-queue producer-freshness and measured raster-response limitations are not
reclassified as proven by this integration.

## Stop and failure

Actual errors latch admission off; subsequent frames use the game's AA-Off path.
There is no automatic retry or silent TAA activation. Failure before the native
handoff leaves native color unchanged; it cannot undo raster jitter already used
in that frame. Later projections receive no mod jitter after a latched fault.

Stop first closes new projection admission, allows issued packets/submissions
to drain for a bounded interval, then removes hooks and drains callbacks. Clean
GPU/Reset receipts release native pins. SDK private resources, the list registry
and uncertain references remain pinned until process teardown; no unsafe unload
or game-device NGX Shutdown is attempted. Diagnostics expose pending GPU/reset/
submission counts, active state, fault code, native handoffs, history and bounded
record usage. A five-second latest-status file and sampled durable log replace
per-frame image captures. There is no frame limit.

## Verification and runtime procedure

Release builds use /W4 /WX. Owned GPU fixtures test 600 full-resolution handoffs
and 600 small-resolution handoffs, comparing every output pixel with packed SDK
output, preserving depth/motion, and checking downstream compute bindings.
Failure fixtures cover invalid bindings, conversion failure, failed Evaluate,
failure after the private handoff, failed binding-mirror validation and missing
retirement. All final fixtures require zero GPU validation errors/warnings.

The real submission callbacks are tested with 1,000 WARP submissions alternating
between two queues, premature Reset, pending GPU work after Reset, duplicate
submission and Reset before Signal return. A separate lifetime test exercises
10,000 reuse cycles. Python review tests corrupt closure, history, startup and
SDK evidence and require rejection. Initial runtime-test setup omitted the
existing debug-layer allowlist macro; its rejected list registration and failing
outputs are preserved. Only the test target enables that macro; production
discovery gates are unchanged.

Session: `local/independent-aa-stream-20260926/v0.8.0/`.
Use the bare offline MO2 profile and usual **Monster Hunter: World - DLSS 1.2.110
test** entry, with AA Off, DLSS Off, Windowed 2560x1440 and DX12 On. Load a save,
then use the sealed `arm.py` after startup ownership passes. Event namespace:
`Local\\MhwAaStream-v1-PID-CREATION-1` (arm), suffix `-0` (stop).
No game setting toggle is needed to activate this test.

The exact prior MO2 DLL is backed up as `previous.dll`. After normal game exit,
`restore.py` restores that exact file and removes only the matching output
sidecar. Configurations and runtime hashes are recorded before installation.
Do not edit frozen source/tools during a run. `review_aa_stream.py` requires a
clean stop, completed GPU work and released marker lifetimes; its success is
explicitly not a visual-quality, resize or other-AA-suppression verdict.
