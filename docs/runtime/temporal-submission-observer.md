# Ordinary TAA submission observer v1.3.0

Runtime result: [run 14268](../research/ordinary-taa-submission-result.md)
passed all three submission/completion/retirement checks and is closed.
The previous DLL is restored and settings are unchanged. The procedure below
describes that completed experiment; the next change is a bounded pixel copy.

This extends the accepted [native recording observer](temporal-consumer-observer.md)
with command-list generation, submission, private fence completion and retirement
evidence. The original three TAA requests, shader and local barrier observations
remain unchanged.

## Scope and ABI

The seven hooks consist of the four v1.2.0 targets plus:

| Target | Native ABI |
| --- | --- |
| ID3D12GraphicsCommandList::Reset, slot 10 | Win64 COM; list, allocator, initial PSO; HRESULT |
| ID3D12GraphicsCommandList::Close, slot 9 | Win64 COM; list; HRESULT |
| ID3D12CommandQueue::ExecuteCommandLists, slot 10 | Win64 COM; queue, count, list array; void |

Methods are discovered from owned D3D12 objects and restricted to the system
implementation. Only DIRECT lists/queues belonging to the discovered device
are tracked. Existing 48 executable code gates still apply; host version
`0.0.0.0`, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
All original calls forward, with native HRESULTs and exception propagation
preserved. Recorder failures stop observation, never suppress the game call.

After arming, lifecycle observation starts immediately; producer admission
waits 150 ms within the existing two-second window. That warmup gives the
tracker a chance to observe a Reset. It does not substitute for one: a matched
TAA Dispatch requires an actual successful Reset/allocator association first.

The journal has 4096 events, 128 lists, 128 allocators, eight queues, at most
128 pointers per submission, and 8192 submitted pointers total. Overflow,
unreadable or changed arguments, failed calls, overlapping list operations,
changed generation, duplicate submission or deadline expiry reject the run.
Worker polling cannot turn a completion arriving after the deadline into an
accepted result. Event and completion timestamps let the reviewer check this.

## Generation and completion

Inside each matched Dispatch wrapper, an independent marker carries the
original receipt index, frame and packet. It belongs to the latest successful
Reset for that pinned list, before Close. The list cannot be busy, already
closed or missing its allocator. Original Dispatch completion closes the marker.

The tracker then requires a successful Close after that marker and exactly one
ExecuteCommandLists batch containing that list generation. Batch pointers are
reread after the original Execute returns. Only then does the observer enqueue
one private Signal on the actual DIRECT queue, using a distinct fence created
at value zero for each of the three receipts. Each is signaled once, to one;
there are at most three private Signals. No private fence is published to the
game, and there are no CPU Signal or queue Wait calls on it.

The worker polls those fences without blocking a render thread. A completed
value of one and a subsequent successful Reset retiring the recorded list
generation are both required. An intervening or overlapping resubmission is
rejected. Reset may precede the worker's completion poll; neither observation
alone is treated as sufficient.

The offline reviewer independently replays the captured Reset/Dispatch/Close/
Execute/Reset events, checks frame/packet/list/thread identities against the
native consumer, and verifies each private signal/completion ordering. A shared
list pointer across frames is never sufficient to identify its generation.

## Ownership and rendering

QI references retain tracked list, queue and allocator identities while hooks
are active. The existing observer independently retains its 21 resource and
three consumer-list references. Brief locks protect bookkeeping; no hooked
original method or private queue Signal runs under that lock. Admission closes,
hooks disable and pending callbacks drain before journal references release.

Private fences and all journal-owned references are retained until process exit
if any Signal was attempted without observed completion, including a failed or
uncertain Signal. Successful runs can release them after shutdown: this version
records no observer commands into game lists, so there is no copy allocation
that a later resubmission could access. The DLL and trampolines stay pinned.

This version performs **no copy, resource transition, output replacement,
jitter write or vendor SDK call**. The header's `gpu_commands=0` means no
observer command-list work; the separate `private_signal_limit=3` accounts for
the completion markers. The observer adds no ExecuteCommandLists of its own.
Even an accepted run leaves motion pixels, typed views, readback eligibility
and DLAA readiness unverified.

## Verification and procedure

The Release build uses `/W4 /WX`. Forty-three native cases pass, including
12 submission cases using real WARP identities and empty-list GPU submissions;
one uses the installed Reset/Close/Execute detours. Tests cover two queues,
duplicate submission, missing reset/close, close failure, premature reset,
missing retirement, failed/unobserved signals and late completion. Existing
consumer assembly/forwarding and shader tests remain covered. Twenty-two
Python tests independently review the real recorder fixture and corrupt its
ordering, generation, batch, private fence and closure evidence.

Local preparation, deployment and exact rollback receipts are under
`local/temporal-submission-20260925/v1.3.0/`. That initial deployment was restored
before launch when the user ended the session. The verified, unchanged build's
resumed deployment/rollback receipts are under
`local/temporal-submission-20260925/v1.3.0-resume-20260925/`.
Deploy only while the game is
closed, preserving the exact prior slot and sidecar. Use the existing bare
profile and test entry, DLSS Off/TAA On, load a save and keep the camera still.
Arm once with `tools/arm_temporal_inputs.py`; review with
`tools/review_temporal_inputs.py`. Save either result and restore the prior DLL
after normal exit. No profile, graphics or mode-setting change is needed.

After successful runtime evidence, implement a separately bounded R32_UINT
pixel copy with format/footprint validation, state restoration and retention
appropriate to issued GPU copy resources. Then compare stationary and moving
camera controls before reconstruction integration.
