# Native AA preparation scheduling — v1.20.1 result

The entry-order correction ran in-game: **2,739 camera scopes were admitted with
the native AA choice Off**. The intended TAA2 unit stage was never admitted,
and there were zero temporal publications and zero DLAA evaluations. This is a
scheduling coverage failure, not a successful rendering test or an SDK failure.

PID 45428 / creation 134349427329869354, host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
DLL SHA-256:
`191b54e007c75fc2573ceb12282be4faf8719461de39aaa46458c4b66c8f68d9`.
The installed/loaded DLL, modern provider, startup, 325 prelaunch files,
299 source files and all four configurations were verified before arming.

The assistant stopped the run deliberately after 25.891 seconds. Runtime and
activation fault counters stayed zero. All 2,739 jitter changes were restored,
both hook sets disabled, scope/callback counters drained and no private GPU work
was submitted. The full reviewer correctly rejected the zero-frame run. The
user confirmed normal exit. Baseline DLL and sidecar restoration succeeded;
AA/DLSS Off and all four configurations remained byte-identical.

Evidence: `local/native-aa-ownership-20260927/v1.20.1/`.
356-file archive SHA-256:
`5f6a5f9bdf693e7687a89d34557887418b3c84a2412649404142f5d400fe6e2c`.
Final log SHA-256:
`7854bc7a02efe83eb47727a036c7bf4e66a5b18be887bbcb099403805adbc728`.

## Confirmed observations

A bounded post-stop VM_READ inspection identified the live unit manager's
vtable at RVA `0x33fcde8` and its render method at `0x221a050`. The 64-byte
entry fingerprint is saved. The 242-node registered-unit list contained TAA2
with state 8, flags `0x11`, mask 1 and display mask 255; its Execute pointer
was the expected `0x2392150`. FXAA was also registered but disabled. This was
an asynchronous structural snapshot, not a callback register capture.

Offline review of the pinned decoded image exposed two missing cases:

- In the native manager's uncached parallel path, the loop at
  `0x221a730..0x221a78c` checks `(unit.flags & 0x12) == 0x12` **before**
  publishing the unit job. With observed TAA2 flags `0x11`, that path excludes
  the unit before the hooked generic dispatcher at `0x2228de0` can run. Other
  paths use cached job arrays or serial traversal; these need separate coverage.
- The manager calls `0x23cd870` to initialize worker contexts and copy the
  parent's context fields into slots spaced `0x1eaf0` apart. Job mode 1 in
  `0x225f620` passes that slot's context in RDX to the job's callback. The
  submit/wait function `0x225e840` lets the caller participate and waits for
  workers before returning; the manager then merges their output at
  `0x23cac60`. A worker's TLS and context pointer differ from the outer
  camera-render thread/context required by v1.20.1.

The zero-qualified-visit counter does not distinguish an earlier prefilter from
a later scope rejection. The run did not record the active manager branch,
worker count or per-worker callback arguments, so it does not prove which
path handled a specific frame. It does prove the entry predicate is now being
exercised while downstream activation remains absent.

## Next implementation boundary

Activation must cover native job admission and its worker-context lifetime,
while preserving the manager's type/render/category/mask/display filtering and
normal synchronization. Merely relaxing the generic dispatcher's flags or
letting arbitrary threads share the parent scope is insufficient. The cached
and uncached paths both need review before changing them. Keep the actual
native temporal preparation and queue merge, and retain the existing qualified
DLAA resolve and validated request retirement.

Add owned fixtures that reproduce prefilter-before-callback ordering and
parent-to-worker context copying, including stop/exception/drain and unrelated
workers. Record bounded admission/rejection reasons so another zero-frame run
identifies the missing boundary without relying on inference. Do not substitute
a global single-thread renderer mode for proper scope propagation.

No new DLL is prepared or installed after this run. Final exclusive AA,
unfiltered suspension and latched failure remain pending; CMFM/UI work is
deferred. Keep the game closed until the next diagnostic is prepared.
