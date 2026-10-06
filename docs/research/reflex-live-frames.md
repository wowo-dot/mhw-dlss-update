# First live Reflex frame integration, 2026-10-02

Probe v0.3.1 connects the accepted native frame map to Streamline 2.12.0
FrameSession and the accepted native presentation route. The first loaded-save
game test has [passed](reflex-live-frame-result.md). Reflex remains Off: this verifies real token/sleep/marker calls,
not low-latency scheduling, a latency benefit, or frame generation.

## Executable and hook contract

Retail version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded private audit identity remains
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
These offsets are a qualified diagnostic for this executable, not portable
signatures. Full native functions must match immediately before hook creation
and again after restoration. Early reservation allocates reachable private
space without reading or patching the native function bodies.

| Target RVA | Bytes | Full SHA-256 | Operation |
| --- | --- | --- | --- |
| `1af2b60` | 4864 | `9f6f5b3ac1a521ddd59351e41cc66c17dfc453ccd1b969afd4d59787bfef795d` | New token, sleep, SimulationStart before observed input refresh |
| `229a080` | 1428 | `961cf9969c28abc6b5b63ba9fb476978555eb188fdc173940d7ed81fc566a4e0` | SimulationEnd and RenderSubmitStart at recording entry |
| `23a7f50` | 69 | `1183e1807b99ec2ada753fa147af8c59b1f99f29bbbf6176ed7479fd48e09a46` | RenderSubmitEnd after game-origin submissions return |

All three use the observed Win64 member-call convention, this in RCX, no
stack arguments. Existing tested assembly preserves volatile GPRs, flags,
XMM0 through XMM5, return values, native exceptions and LastError. Native calls
always forward. Engine return completes the frame after PresentEnd; it does
not mark simulation or submission completion. The mapping is qualified for
the observed worker branch. The late-worker interpretation is an inference
from native control flow, not dynamic identification of every worker callback;
see [timing result](reflex-input-result.md).

Engine, renderer, backend and SDK-attached device identities are bounded-read
validated on each tracked edge. Epoch/bank are read twice. A token belongs to
one engine thread; recording advances epoch by one and toggles bank, with
temporal unchanged or incremented once. Recording/submission/Present must
share the advanced stamp. An arm partway through a frame waits for the next
engine entry. No input values are captured.

Presentation's observer wraps each actual native non-TEST Present transaction
once, outside proxy recursion. Present and Present1 have the same marker pair.
TEST does not create markers. Native HRESULT and LastError remain authoritative.
The host retains native interfaces; SDK proxies and dispatch/trampoline storage
are private and process-lived. No vendor teardown runs under DllMain.

## Startup, stop and evidence

The early startup path remains the accepted Off device/presentation path:
two public creation hooks, twelve mandatory COM route hooks, native-adapter
parent factory qualification before queues/swap-chain creation, only common,
PCL and Reflex plugins, OTA disabled. Three additional native frame hooks are
installed after explicit arm. Required early trampoline space is reserved
before public hooks are created, with a versioned allocation receipt recording
policy, status, address and failure reason. The runtime dependency remains pinned to
Streamline 2.12.0, commit `e8aaa6eaac968711fb62473d4ae8256dde20919b`.

Armed capture lasts thirty seconds. Stop requests finish the current frame
without starting another token. A stalled frame times out after five seconds
and cannot pass strict review. Disable cancels all queued native activations,
disables heads, drains callbacks and verifies both original heads and full
native functions. Presentation then stops before driver-state sampling and SDK
shutdown. Interface pins release only after shutdown. Failures disable the
integration and preserve original rendering; no invented closing markers are
used to turn partial frames into successful evidence.

Only the first eight frames retain six bounded stage samples, plus five-second
heartbeats, SDK return/count totals and restoration records. Acceptance requires
one token and sleep and six successful markers per completed frame, one owner,
valid matching stamps, zero faults/pending calls, all seventeen original heads
and five full native functions independently restored, SDK shutdown and pin
release. Driver reports are capped at sixty-four and must have ordered complete
timestamps when present. Off may produce no reports; On/Boost and driver-backed
latency evidence are later tests. A normal-looking scene alone is insufficient.

The owned fixture has passed 144 tokens, 144 sleeps and 864 markers across both
Present paths, two resizes and same-window swap-chain recreation, with TEST
excluded, callback LastError preserved, native rendering after stop and zero
final device references. Native boundary ABI/frame-policy/ownership and strict
log-review tests passed. Ignored receipts and binaries live under
`local/reflex-frames-20261002/`. The earlier input and presentation archives
and their exact source freezes remain separate and unchanged.

## Rejected v0.3.0 startup and correction

The first live startup rejected `live-frame-startup-reservation` before any
MinHook initialization, hook creation or SDK load. Query/VM_READ inspection of
the exact owned PoolState found all fields zero: allocation had never been
attempted. The rejected check was therefore the earlier native-code gate.
After save load, all three complete frame functions and both complete native
creation functions matched their trusted hashes. The exact earlier mismatch
or memory-protection state was not recorded; do not attribute it to a specific
loader transform. The observer's startup metadata initially showed zero bytes;
the start/failure records became readable on the next read.

The correction uses the established allocation-only early reservation pattern
from the accepted SDK-free frame observer. Disk/PE host identity and address
range checks precede allocation. Full native verification remains mandatory
when the frame hooks are actually installed. This does not relax the hook gate
or alter the game's original loading/protection logic.

Independent inspection covered 212 bounded reads and 24,508 bytes, with zero
writes or remote execution, and found no Streamline modules. API export heads
were observed but no restoration comparison against startup originals was
possible: the probe had not created hooks or retained original heads. One DXGI
head differed from its unrelocated disk bytes; no attribution or restoration
claim is made from that comparison. The user confirmed normal scene and exit.
The rejected run and exact v0.3.0 source freeze are preserved separately.

## First runtime procedure and removal

Use the existing MO2 profile, solo/offline, DX12, native AA/DLSS Off, fixed window
size. Launch normally, load a save and keep the game open. SDK startup remains
Off and waits up to ten minutes for arm; the frame capture itself is thirty
seconds. Unique log name is `reflex-frames-<pid>-<creation>.jsonl`. The controller
`tools/reflex_frames.py arm --log <log> --output <fresh-receipt>` verifies PID,
creation time and retail disk hash with query-only access, then exclusively
creates `<log>.arm`. The equivalent stop command creates `<log>.stop`. No
external process writes or remote execution are used for control.

After stop, use `tools/reflex_frames.py review` and the ignored independent
`local/reflex-frames-20261002/check_live02.py` for v0.3.1 while the game is still
open (the earlier `check_live.py` remains scoped to the rejected v0.3.0 run). Then
exit normally and archive the immutable run with post-exit settings hashes.
This initial diagnostic leaves resolution, jitter and reconstruction untouched.
Menus/loading/focus/recreation and alternative scheduling remain integration
coverage to establish after the first successful live sequence.

Deployment uses only the already authorized MO2 diagnostic slot. Its exact
previous DLL and output/runtime sidecars are backed up in `game-frames-01/`.
With the game closed, restore `previous-slot.dll.disabled` to the deployment's
recorded destination and `output-sidecar.before.bin` to its output sidecar to
remove this test. The runtime sidecar remains unchanged; its backup is retained.
No original executable or game asset is changed. Keep this test installed
between sessions unless the user requests removal or a replacement test.
