# Independent-AA projection/bindings v0.6.1: completion before unhook

This repeats the v0.6.0 projection/binding capture with a corrected shutdown
order. Input selection, jitter, native code gates, the 25 hook targets and their
Windows x64 ABIs remain those documented in the
[projection/binding contract](independent-aa-projection-bindings.md). No new hook,
SDK evaluation, output handoff, AA option write or fallback is added. Keep native
AA Off / DLSS Off, Windowed 2560x1440, DX12 On, bare offline profile.

Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded analysis image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.

## Reason for the change

[v0.6.0](../research/independent-aa-projection-bindings-result.md) verified all
eight projection and binding receipts and restored six jitter scopes. All eight
selected recordings retired by 407ms, but ordinary rendering traffic filled
the 4,096-event journal at 516ms while the worker was removing scheduling/jitter
hooks before its completion loop. No completed fence values or images were saved.
The failure is preserved as a rejected capture, not relabeled as success.

## Shutdown order and lifetime

The worker first closes projection and scheduler admission without removing
hooks. It polls deferred completion while any scheduler calls drain, then waits
for all eight selected requests, recorded fence completions and successful Reset
retirements. Existing deadlines and failure propagation remain in force.

Before any MinHook removal, `gpu_trace::close_admission()`:

1. Records whether all selected completions/retirements were already observed.
2. Closes selected-barrier admission and drains any admitted barrier lease.
3. Stops binding observation and journal admission; drains existing callbacks.
4. Records the fixed journal event count and closure timestamp.

Only then does the worker disable scheduling, projection, consumer, barrier,
binding and submission hooks. The close operation is idempotent; the GPU and
deferred finish paths also invoke it before their own hook removal. Original
APIs continue forwarding during this interval, while new calls cannot enlarge
the closed journal. Objects are still retained until the existing GPU-completion,
recording-retirement and callback-drain requirements authorize release.

An early cutoff or existing failure stays a failed capture. Post-cutoff Reset
calls cannot manufacture a missing retirement. The 4,096-event cap, two-second
recording deadline, resource-state checks and partial-work retention are unchanged.
No source/image saving is allowed just because observation has been closed.

Additional evidence records closure-request/closed timestamps, whether completion
preceded cutoff, event count at closure, the journal cutoff and the first outer
and GPU hook-removal timestamps. The v0.6.1 reviewer requires all eight completed
fence samples and retirements before closure request, closure before removal,
and final event count equal to the recorded closed count. Callbacks admitted
before cutoff may finish while closing, but must drain before closure completes.

## Verification

Release /W4 /WX build. All 48 relevant native checks and 43 AA Python tests pass.
Six new real D3D12 WARP cases exercise the changed lifetime:

- 2,500 Reset/Close pairs plus 5,000 resource transitions and binding setters
  after cutoff leave the event count unchanged with hooks still installed.
- A real queue Wait blocks GPU completion; observation remains open until the
  test releases the gate and completed fence values become observable.
- Removal delayed more than 2.1 seconds after valid closure still saves all nine
  correct images; a closed capture does not expire due to unhook latency.
- An admitted Close callback held across cutoff must drain before closure returns.
- Cutoff before the last retirement rejects saving even after later Reset traffic.
- The same extra Reset/Close traffic before cutoff hits the unchanged event cap
  and remains a rejected capture with retained image references.

The native fixtures check exact pixels and D3D12 debug errors. Saved real fixture
logs passed the independent resource-state, completion, binding and new shutdown
reviews for all five positive binding/readback scenarios. Synthetic corruption
tests reject early removal, late completion/retirement, post-cutoff events and
altered closed counts. The game capture subsequently passed these scoped checks:
eight completions/retirements, nine images, six restored jitter scopes and a
2,831-event journal closed before hook removal. See the
[v0.6.1 result](../research/independent-aa-projection-closure-result.md).

## Session and rollback

Ignored session: `local/independent-aa-projection-closure-20260926/v0.6.1/`.
The usual MO2 test entry and profile are unchanged. Load a save and keep the
camera/character still; a pose can reduce idle animation. Startup reserves hook
storage and waits for explicit arming. Frozen `arm.py` verifies all deployment,
source, backup and configuration hashes before invoking the version-aware tool.

Header: version 0.6.1, `independent-aa-projection-bindings`,
`shutdown_order: drain-before-disable-v1`. Process-generation-scoped events remain
`Local\\MhwAaProjection-v1-PID-CREATION-1` (arm) and suffix `-0` (stop).
Use frozen `source/tools/review_aa_projection.py` for the result.

The exact existing test-slot DLL is saved as `previous.dll`. After normal game
exit, this session's `restore.py` verifies test/backup hashes, restores that DLL
and removes only its matching output-directory sidecar. It reports configuration
hashes without overwriting user settings. Previous archives and original game
executable/assets remain untouched. The accepted capture is now closed: the user
exited normally, the exact previous DLL was restored, its matching output sidecar
was removed, and all four configuration hashes remained unchanged. No next
test build is installed or armed.
