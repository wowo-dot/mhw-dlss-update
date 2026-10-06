# Reflex input and submission timing result, 2026-10-02

The SDK-free loaded-save observer passed. This establishes the selected input
refresh and frame/submission order in this capture and qualifies the next Reflex
integration test. No SDK, sleeps, latency markers or reconstruction ran.

## Identity and cleanup

Retail executable version 0.0.0.0, SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Probe v0.2.0, SHA-256:
`ecd79670bce69cb6a97394f3793a77d91b2ef952f9c95aac1ab4796f5d2c0719`.
PID 37292 / creation 134354079371033345. DX12, 2560x1440, three buffers,
DXGI format 87, adapter LUID 93911:0. Native AA/DLSS remained Off.

The fifteen-second trace retained 53,520 complete call pairs: twelve boundaries
occurred 2,631 times each, plus 21,948 ExecuteCommandLists calls. All ran on
thread 2152, all input/engine owners qualified, and all frame stamps and engine
contexts were valid. The engine worker count was 31. There were zero dropped
records, exceptions or outstanding callbacks. Lifecycle review passed.

The external checker independently compared all thirteen original 32-byte
heads and eleven full native functions. All matched; 203 bounded reads covered
18,322 bytes, with no process writes or remote execution. The user confirmed
normal rendering and normal exit. Keep the observer installed as requested.

The first startup-file metadata read reported zero bytes. Read-only inspection
found the observer thread and both controls alive, with all native gates intact.
The start/ready records subsequently became readable and validated before arm.
No startup correction was installed; the cause of that visibility delay was
not established. Preserve this observation without attributing it to the SDK,
which was not loaded.

## Confirmed frame order

Independent order analysis accepted 2,630 complete engine intervals; the final
partial capture interval is excluded. Each complete interval had this sequence:

1. Engine entry uses the preceding epoch, temporal counter and bank.
2. Bank advancement increments epoch/temporal and toggles the bank.
3. Qualified keyboard refresh completes, then mouse refresh, then input-device
   refresh. The base device poll is nested inside input-device refresh.
4. The synchronous job batch completes before command recording begins.
5. Recording/publication uses the advanced frame stamp.
6. The backend-submit wrapper makes five game-origin submissions and returns.
7. Native Present contains one actual API Present, then engine update returns.
8. The separately observed renderer maintenance call occurs after that return,
   before the next engine update. It is not the rendering completion boundary.

The 8,789 additional submissions after backend-submit return in these intervals
all originate outside the game's image, inside API Present. Do not move the
game's RenderSubmitEnd marker into driver/overlay submissions on that basis.
The entire trace includes 8,793 such calls, including partial capture edges.

The intended adapter mapping is:

| Marker operation | Qualified boundary |
| --- | --- |
| New integration-owned frame token, sleep, SimulationStart | Engine entry `1af2b60`, before observed input refresh |
| SimulationEnd and RenderSubmitStart | Recording entry `229a080`, caller return `1af3955` in this worker branch |
| RenderSubmitEnd | Backend-submit `23a7f50` return, called from `22980b0` |
| PresentStart / PresentEnd | Actual non-TEST game swap-chain Present through the preserved presentation route |
| Frame completion | Engine return after PresentEnd; never use it for SimulationEnd |

An offline replay separately checked the actual stamps at recording entry,
backend-submit return and both Present edges, then fed all 2,630 complete rows
through the existing C++ FramePolicy fixture. It accepted every row. This is
policy compatibility, not proof that SDK calls ran or latency improved.

## Pending workers: observation and inference

At recording entry, 363 complete intervals had one or more pending worker bits;
2,267 had none. All pending bits were clear at recording return. A rule requiring
every worker to be idle at recording entry would reject otherwise normal frames.

The full hash-qualified engine update explicitly waits on all four earlier
worker completion flags before its final scheduling block. That block assigns
only callback `221a890` to the four queues, then calls recording entry. The
private decoded audit shows `221a890 -> 221bf40`; the latter sorts/relinks job
lists, updates scheduling cost metadata, and prepares later job descriptors.
It does not invoke those prepared job callbacks in this path.

From that control flow and the live pending flags, the overlapping work is
inferred to be late scheduling maintenance, rather than current-frame game
simulation. The two live routing-table samples were empty at recording entry,
both on intervals with no pending flags. They therefore do not dynamically
confirm the callback identities and must not be represented as such.

The mapping is qualified for this executable and observed worker branch.
Sequential scheduling, menus/cutscenes, frame pauses and owner/recreation
transitions still need verification with the real integration. Keep native
calls running and disable Reflex if its required identities/order fail.
No input values were recorded, and no latency benefit is claimed yet.

## Preserved evidence and next implementation

Ignored evidence lives under `local/reflex-input-20261002/game-input-01/`:
process log, deployment/source provenance, arm receipt, lifecycle review,
independent restoration, ordering analysis and C++ policy replay. A separate
archive records hashes after normal exit. Native code/disassembly and runtime
binaries stay local and untracked. The exact 310-file observer source freeze
and the earlier accepted persistent presentation archive remain preserved.

Log SHA-256:
`cc750dc84fb3e0c6bc3e7beef490ddfeeeafe4ae5778cdee8bcbcb0775dae8fa`.

Next, connect FrameSession to the accepted persistent presentation route using
this map. First verify live tokens/sleep calls and six ordered markers in Reflex
Off compatibility mode; then test On/Boost, loading/focus transitions and driver
latency reports. Ordinary frame generation and MFG follow Reflex verification.
