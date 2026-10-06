# Ordinary TAA native recording observer v1.2.0

Runtime result: [run 53044](../research/ordinary-taa-consumer-result.md)
passed and is closed, with the prior DLL restored and settings unchanged.
All three matched native recordings establish GBuffer4 state `0xC0` before
Dispatch. Submission/completion and pixel readback remain unverified.

This observer measures a prerequisite for motion-buffer readback. The accepted
v1.1.0 result identifies the shader and CPU bindings; it does not establish
the resource state at a native dispatch or prove submission/completion.
v1.2.0 associates those requests with the native command-list recording path.
It adds no copy, transition, Signal, submission, jitter write or SDK call.

## Scope and ABI

The same three requests and two-second window are retained. Four hooks run
only after explicit arming:

| Target | ABI and admission |
| --- | --- |
| Producer RVA `23C9980` | Existing six-argument Win64 wrapper; only ordinary-TAA return RVA `2392F31` |
| Consumer RVA `259CC20` | Win64 RCX executor, RDX backend context, R8 packet; matches one published packet receipt |
| ID3D12GraphicsCommandList::ResourceBarrier, slot 26 | COM Win64; only inside an admitted consumer on that thread and its actual command list |
| ID3D12GraphicsCommandList::Dispatch, slot 14 | COM Win64; same scope, exact native return RVA `259CFCA` and matching group counts |

The native consumer wrapper preserves volatile GP/vector argument and return
registers using the existing forwarding helper. All original calls run,
including rejected observations; exceptions propagate through cleanup.
All 48 existing code gates remain required. The consumer's complete
1283-byte span was already gated in v1.1.0 and covers the Dispatch call site.
Host version/hash and span hashes are in the
[shader map](../research/ordinary-taa-shader-map.md). API targets are discovered
from an owned, closed, unsubmitted DIRECT command list and restricted to the
system D3D12 implementation. Each observed engine list must match those targets,
support the native interface, belong to the same device, and be DIRECT.

## Association and lifetime

Producer receipts are published after the immutable packet/shader checks.
The consumer independently rereads the packet and shader recipe on entry,
at the native Dispatch, and on return. Backend-context `+8` must identify
the backend. Executor `+8` identifies the recording object; recording `+20`
is the command list, and `+158` must identify the admitted packet at Dispatch.
No current renderer frame sampled on a worker substitutes for the originating
producer's frame. An observation that races before publication is not matched.

Only one consumption is accepted for each distinct published packet address.
Duplicate/reused packet addresses, nested consumers, missing/duplicate dispatch,
changed sources, wrong lists/callers/groups, exceptions or expiration reject
the run. Three completed consumers close admission immediately. Same-list
overlap is rejected; independent consumers can run on different threads/lists.

Seven native resource pins per request retain the original input identities.
One additional command-list reference per consumer prevents address reuse.
Hooks disable and callbacks drain before references release. Unowned engine
pointers are read only inside admitted callbacks. The DLL/trampolines remain
pinned until process exit. There are no potentially in-flight observer GPU
allocations because this version records no GPU commands of its own.

## Barrier interpretation

Each consumer records at most 16 barrier calls and 128 barrier entries total.
Call arguments are checked again after the original API returns. The observer
derives each named resource's state immediately before the native Dispatch,
using only barriers recorded within that consumer. A full, non-split,
all-subresource transition can establish a state in this scope. Split or
partial transitions, relevant aliasing, or conflicting transitions make it
unknown. UAV barriers do not establish a transition state.

The offline reviewer independently derives the same state from the journal.
No transition is a legitimate unknown result, not a failed observation and
not an assumed shader-resource state. A known local state still does not
prove queue ordering, implicit state behavior, submission or GPU completion.
The reviewer always leaves readback eligibility, saved motion pixels and
GPU-submission verification false. A later pixel-copy probe must establish
the missing execution/lifetime guarantees before it can save pixels.

## Validation and runtime

31 native tests pass, including 15 consumer cases using real WARP resources
and command-list identities with forwarding stubs. No owned fixture GPU work
is submitted. The consumer cases also run through the real assembly entry,
including exception propagation and the original native return. Fourteen
Python tests cover both accepted and damaged captures, barrier state derivation,
shader-file integrity and historical review compatibility.

With the game closed, deploy through the ignored lifecycle script, preserving
the exact previous slot and sidecar. Current preparation/deployment/rollback:
`local/temporal-consumer-20260925/v1.2.0/`. Its receipt is authoritative.
Launch the existing bare-profile test entry, keep DLSS Off/TAA On, load a save
and leave the camera still. Arm once using `tools/arm_temporal_inputs.py` and
the prepared DLL hash. Review with `tools/review_temporal_inputs.py`, preserve
either result, then restore the prior slot after normal game exit.
