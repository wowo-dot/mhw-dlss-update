# Read-only depth-view lifetime and earlier-barrier capture

Design dated 2026-09-24, based on the verified
[depth-view/state map](../research/depth-view-state-map.md) and accepted
[run 11804](../research/projection-attachment-result.md). Implemented as
[v0.4.0](depth-view-state-recorder.md) on 2026-09-24. This document preserves
the design requirements; the implementation protocol and versioned receipts
record verification and deployment. [Run 1544](../research/depth-view-state-result.md)
accepts the view provenance; cross-list state and GPU evidence remain incomplete.

## Required evidence

Connect the selected depth view's actual creation descriptor and lifetime to
the existing projection source/binding/draw chain. Capture earlier resource
declarations in the same actual command-list recording generation. Keep
descriptor provenance, local recorded state and eventual GPU execution as
separate acceptance fields. DLSS stays Off/TAA On on the bare profile; no
camera writes, descriptor writes, resource transitions, copies or vendor calls
are added by this diagnostic.

## Two observation lifetimes

The descriptor watcher must start before view creation, independently of the
later two-second frame capture. Observe only after the pinned host/code gates,
actual DX12 device and system method targets have passed. Record its start
explicitly. Installation timing must be tested; a view that predates the
watcher stays unqualified. Do not force view recreation, change resolution or
reconstruct a descriptor and label it observed.

Retain bounded native constructor/destructor records and actual
`CreateDepthStencilView` arguments/return. Associate the 24-byte descriptor,
device, resource, CPU handle, native view and generation. Validate and pin
tracked resources outside bookkeeping locks where needed to prevent pointer
reuse from certifying a different object. Bound retained resources and release
them on retirement or clean shutdown. Use an explicit maximum duration and
capacity; overflow or an incomplete lifetime ends qualification, not silent
eviction. Choose the final numeric bounds after fixture/footprint measurement
and record them in the versioned start event.

All DSV overwrites must be covered. A creation outside the mapped constructor
invalidates an old association at that destination. Observe DSV descriptor
copies; initially reject/invalidate affected associations rather than trying
to propagate arbitrary copy provenance. Decode bounded destination ranges with
the actual device's DSV stride, including omitted range-size arrays. Unreadable,
overflowing or unsupported writes that might overlap retained handles end the
descriptor qualification. Heap/device replacement also invalidates history.

Once the user confirms a loaded save, arm the short command recorder. Record
actual Reset, Close, ResourceBarrier, ClearDepthStencilView and ExecuteBundle
calls from the start of this window, before choosing the projection source.
Keep list identity, entry/return order, thread, HRESULT where applicable and
bounded argument copies. A successful Reset establishes a new recording
generation; native `259BCC0` does not. If the selected list's generation began
before observation, preserve the capture as partial rather than inventing its
prefix or silently choosing a different draw after failure.

Capture the existing source/publication/consumption/root-CBV/draw chain and all
three resource checks unchanged. Stop at the selected native draw's return,
close admission, drain callbacks, disable hooks, release retained references
and write the immutable evidence. The longer descriptor watch does not extend
the expensive frame trace or the current single-draw bounds.

## Acceptance

Require a completed observed creation for the exact bound handle and native
view generation, with no intervening destruction or unqualified overwrite.
Recheck owner/backend/resource anchors at binding and draw boundaries. Preserve
the actual API descriptor separately from native recipe reconstruction.
Initially qualify only a supported 2D, single-sample DSV whose format, flags
and selected subresource fit the validated resource. An unsupported descriptor
is evidence, not permission to substitute mip zero or clear read-only flags.

For local state qualification require the actual successful Reset and a
complete, ordered history through the matched draw. Replay matching transition
subresources and detect continuity mismatches, unresolved split barriers,
relevant aliasing, bundles and failed/overlapping control calls. A clear is
corroborating API evidence, not a replacement for the missing state history.
Absent a relevant declaration, state remains unknown. The first captured
declaration's incoming state is a game assertion, not proof of older GPU work.

Cross-list declarations must not be merged by CPU arrival time. This step does
not add queue submission or fence claims; those remain necessary before a
later CPU readback can be accepted. It also does not retroactively certify the
cached PSO's creation descriptor or demonstrate raster jitter.

## Implementation checks before deployment

Use the existing single-original-call ABI/exception/LastError preservation and
bounded callback admission. Do not hold locks across original calls or COM.
For the wider history, prefer independently reserved immutable event slots
over routing every rendering thread through the draw recorder's short lock;
the existing barrier journal is a reference, not automatically compatible code.

Fixtures must exercise observed creation, pre-watcher views, descriptor
overwrite/copy, native handle reuse, resource identity changes, device/heap
replacement, lifetime overlap, reference balance and capacity rejection.
State replay must distinguish cache setup from real Reset, reset failure,
different list generations, per-subresource transitions, split pairs, aliasing,
bundles, missing prefixes and out-of-order callbacks. Include real owned SDK
resource/descriptor calls as well as synthetic fixtures. Keep old accepted
reviews unchanged and rejected runs rejected.

Only after implementation/build/review checks pass, archive the exact new
DLL/sources and preserve the current slot before deployment. The eventual
runtime instructions can remain one launch and one save load if watcher
timing proves sufficient. Any missing early creation must be reported plainly;
do not ask for repeated identical runs without a changed observation strategy.
