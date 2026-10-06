# Accepted live Reflex Off frame integration, 2026-10-02

The first real game token/sleep/marker integration passed in a loaded save.
This accepts the observed Off-mode frame sequence and shutdown path. It does
not establish low-latency scheduling, a performance improvement, On/Boost
behavior, frame generation or coverage of every engine scheduling branch.

## Identity and startup

Retail version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Probe v0.3.1, SHA-256
`609b70fcfb0dbf1f3cb62ef3dd7a6c5e95de8fd6cb14123c2199abf8f77b3b0f`.
PID 52044 / creation 134354122025232700. Streamline 2.12.0, Reflex Off, DX12,
2560x1440, three buffers, DXGI format 87, windowed, adapter LUID 93911:0,
feature level 49152. Native AA/DLSS remained Off; reconstruction was not active.

The corrected early allocation-only reservation passed: dynamic-code policy
available with flags zero, MinHook status zero, no allocation error, private
pool reachable from all three native targets. Full native function hashes
qualified immediately before installation. SDK initialization followed native
main-device creation and preceded native queues/swap-chain creation. The
factory came from the qualified native adapter parent and local backend owner;
the backend global was not yet published. The host retained native interfaces.

The first directory metadata query showed zero log bytes. A fresh read returned
the complete startup/ready records before arm. No extra startup rejection or
SDK failure occurred in this run. The rejected v0.3.0 startup, its original
binary/source and its read-only diagnosis remain separate and unchanged.

## Real frame sequence

The thirty-second armed interval completed 5,949 frames, with exactly 5,949
successful integration-owned frame-token requests, 5,949 SDK sleep calls and
35,694 successful marker calls. There were zero runtime faults, pending
callbacks or incomplete final frames. One owner thread, 12536, handled all
tracked edges. Sleep was called while Reflex remained Off; this does not claim
that the driver imposed low-latency pacing.

The first eight frames each retained six bounded stage samples. Their SDK
results were all zero, marker totals followed 1/3/4/5/6/6, and token/sleep
counts matched the integration frame ID. Engine entry preceded epoch/bank
advancement; recording advanced epoch once and toggled bank. Submission and
both Present edges retained that recording stamp. QPC order and thread identity
matched throughout. The selected mapping is:

| Operation | Native boundary |
| --- | --- |
| Token, sleep, SimulationStart | Engine `1af2b60` entry, before observed input refresh |
| SimulationEnd, RenderSubmitStart | Recording `229a080` entry |
| RenderSubmitEnd | Backend-submit `23a7f50` return |
| PresentStart, PresentEnd | One actual non-TEST native swap-chain Present transaction |
| Complete frame | Engine return after PresentEnd |

The presentation route recorded 19,715 matched routed/native presentations,
five queues, one swap chain and four resizes over its longer startup-to-stop
lifetime. These are not the thirty-second frame count. Forty-five observer
Present calls had no active tracked frame and were counted separately, with
no fabricated tokens or markers. This capture did not exercise TEST Presents.

Driver state reported low-latency availability and latency-report availability,
while mode remained Off. All 64 retained reports had ordered, complete six-edge
timestamps. Their unique IDs 5,886 through 5,949 exactly matched the last 64
integration-owned tokens. This independently corroborates frame identity and
marker submission. No comparison against On/Boost or native latency was made.

The pending-worker interpretation remains the inference documented in the
[input timing result](reflex-input-result.md). This run confirms calls at the
selected native boundaries; it does not dynamically identify every worker or
prove alternative scheduling branches use the same simulation boundary.

## Shutdown and independent verification

Stop was intentional at the thirty-second ceiling. The current frame completed;
native hooks were disabled, pending activation canceled and callbacks drained.
The frame stop reported all native bytes restored. Presentation then stopped,
driver state was sampled, SDK shutdown passed and private interface pins were
released. The SDK log contained no warning/error/fatal entries and reported
zero delayed destroy resources.

Strict log review passed. The independent query/VM_READ checker verified all
17 original hook heads, all five complete native game functions, the retained
native graphics identities and the native host swap-chain interface. It used
355 bounded reads, 25,592 bytes, zero process writes and no remote execution.
The user confirmed normal rendering and normal exit. Keep v0.3.1 installed as
requested; no previous DLL restoration is needed.

Log SHA-256:
`9f79a2914837a54b5e108f403efb89321900c63c7bd89118aceab65422397a53`.
Ignored evidence is under `local/reflex-frames-20261002/game-frames-02/` and its
separate post-exit archive: deployment, arm receipt, complete log, strict review,
independent restoration, SDK review and frame alignment. The exact 315-file
`source-frames-0.3.1` freeze is preserved. Current documentation has a later
checker-link correction; the frozen implementation remains unchanged.

Next: run real Reflex On through the same mapping, verify the reported mode,
driver marker reports and stable behavior, then On+Boost and transition/focus
coverage. Keep the working reconstruction milestone separate during this
initial Reflex verification; combine the features after their standalone
contracts pass. Ordinary frame generation and MFG follow Reflex acceptance.
