# Independent-AA projection/bindings v0.6.0 result

2026-09-26, PID 51352 / creation 134349228791640896. **Partial capture:**
eight projection/request associations and eight native binding snapshots passed
their independent reviews. Six scoped jitter injections were restored. The
submission journal filled before its first successful completion poll, so GPU
completion and image saving were not certified. Full review correctly rejects
the run (`clean closure`). No independent-DLAA or pixel-jitter claim.

The user exited normally. Exact previous DLL restored at 22:04:28 +03; matching
output-directory sidecar removed. Graphics, profile modlist/settings and separate
mode sidecar hashes unchanged. Game closed; no next build installed or armed.

## Confirmed observations

Source frames 15322-15329, with native AA and DLSS Off, Windowed 2560x1440,
DX12, device 354444192, adapter LUID 98931:0, swap-chain format 87. Build SHA-256
`e22e17b580e1fa879f3744417c0bbe928e94351979737a361b16f80e6e6298fc`.
The host/decoded-image identities and code gates are documented in the
[runtime contract](../runtime/independent-aa-projection-bindings.md).

- All eight projections match their owned scheduler requests by frame, epoch,
  bank, context, view and async mode. Their matrix and view-projection relations
  pass the frozen independent reviewer. Six distinct stimuli, all restored to
  zero; unchanged baseline/final controls. Projection fault/pending counts zero.
- Each qualified input read point retrieves the same immutable source frame,
  projection epoch and jitter bits. The later global renderer counter is not
  used to label the source. Actual raster response is still unmeasured.
- All eight native binding snapshots have known valid state on their matching
  Reset generation. Pipeline 4666381424 and compute root 357103360 are stable;
  both compute/graphics arguments are recorded and unchanged after input copies.
  Binding observer fault/pending counts zero, 21 tracked command lists. This is
  an observed binding set, not a separate shader-cache or restoration proof.
- The runtime recorded all nine input copies and resource-state restorations.
  Eight native submissions each recorded one successful Signal call and a later
  Reset retiring that recording. No completed fence value was sampled into the
  capture. Nine image buffers therefore remained unmapped/unsaved and retained.
- All 25 hooks disabled and drained, all eight owned descriptors detached.
  The journal released 534 of 608 pins and retained the remaining 74 together
  with relevant input/readback references until process exit. This is deliberate
  retention after incomplete completion evidence, not an unsafe early release.

## Recorder failure and next change

Submission fault 2 corresponds here to the event budget: exactly 4,096 events,
only 21 lists, 44 allocators and one queue. Other registry budgets were not full.
The GPU state observer itself has fault zero; outer fault 6/deferred fault 8
propagate the journal failure. This is a failed diagnostic closure, not evidence
that the game's GPU work failed or that a fence remained incomplete afterward.

Relative to the journal start at tick 33526046:

| Observation | Milliseconds |
| --- | ---: |
| First selected projection | 313 |
| First selected GPU marker | 329 |
| Last selected projection, Execute return and retirement | 407 |
| Last recorded event; journal reaches capacity | 516 |
| Journal stop recorded | 829 |

Source inspection shows the worker first stops/disables the scheduling hook and
the added jitter hook, then enters the deferred completion loop. Native journal
traffic continues during those hook-removal operations. The timeline is
consistent with that closure order delaying the first poll until after capacity
exhaustion; individual hook-removal durations were not instrumented.

Next implementation should stop projection/scheduling admission, collect the
selected request completions and retirements while the hooks remain installed,
then close journal admission before disabling hooks. Preserve bounded deadlines,
fault handling and partial-work retention. Test excess post-capture traffic and
shutdown ordering explicitly. Merely raising the event cap leaves the underlying
ordering issue. No source or test changes were made during this run/closure.

The eventual rerun should recover the images alongside the already verified
association/binding evidence. SDK conversion/evaluation, output handoff, other-AA
suppression and production failure latching remain integration work.

## Evidence

Ignored archive: `local/independent-aa-projection-20260926/v0.6.0/`.
351 frozen sources and 376 prelaunch files; prelaunch SHA-256
`23bc2f61dac409eeb30b90ee7bd19d97ad7eb9fe0aabc7ea6b901233d3428e41`.
Capture 1,064,243 bytes, SHA-256
`a7937f60ac72918497d4c6e8d9245ba9e682b419263ac03727750199e2b53709`.
`review-partial-51352.json` records explicit failed overall acceptance and the
successful scoped checks. No raw image files were saved. Preserve the original
failed capture; do not synthesize completion observations or mark it accepted.
