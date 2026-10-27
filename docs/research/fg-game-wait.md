# The game's wait while DLSS-G is loaded (0.32.0, 2026-10-27)

## The problem

Whenever DLSS-G was loaded but not generating (NVIDIA's Auto mode paused,
Dynamic at 1x), MHW rendered at about 107 to 150 fps, below DLSS-G at 2x and
far below DLSS-G unloaded (about 260 to 280 fps). DLAA 1440p, RTX 5090, the
hub. v0.27 had seen the same state ("Off with DLSS-G loaded", 109-111 fps,
`fg-live-toggle.md`): the CPU sampler put about 55 % of the game thread in its
per-frame wait on the swap chain's latency waitable (`sub_1423ADE30`,
`WaitForSingleObjectEx(cD3DRender+0x10F0)`), 8 % while generating.

NVIDIA's DLSS-G guide (Streamline 2.12, 18.0): a loaded DLSS-G keeps its own
present queue and makes the game render off screen, so not generating still
costs a copy and a queue sync; NVIDIA's own answer is to unload it, which Auto
and Dynamic cannot do (they must stay loaded to decide).

## The measurement

`waitable_ab` (dev.14 to dev.16): while DLSS-G paces the chain, the game's
waitable slot alternated every 10 s between the chain's waitable ("chain") and
an always-set event ("free"; Reflex's sleep alone then paces the game). Per
phase: the mod's own present intervals and, from dev.15, PresentMon 2.6.0
(1.x metrics, `--track_pc_latency`, `--qpc_time`) lined up through a QPC/tick
pair in each frame heartbeat.

| Run | State | chain | free |
|---|---|---|---|
| 3224 (PresentMon) | Fixed 2x, DLAA | 181 fps, PCL 14.2 ms | 181 fps, PCL 14.2 ms |
| 3224 | Fixed 2x, Performance CNN | 189 fps, PCL 12.4 ms | 189 fps, PCL 12.4 ms |
| 3224 | Dynamic target 165, at 1x | 107 fps, PCL 15.3 ms | 176 fps, PCL 13.3 ms |
| 35184 (cpuopt) | Auto, paused | 151 fps | 247 fps |
| 35184 | Dynamic target 60, at 1x | 148-151 fps | 243-248 fps |
| 35184 | FG off (DLSS-G unloaded) | 262 fps | |

- Generating, the wait makes no difference to the frame rate or to PC
  Latency.
- Not generating, the free wait lifts the game by 65 % and lowers PC Latency.
  With cpuopt it runs within 6 % of DLSS-G unloaded: what remains is the
  off-screen copy and queue sync the guide describes.
- The game's Present into Streamline's proxy takes about 60 us at 2x and
  about 500 us at 1x: not the limit.

## The change (0.32.0)

`reflex::GameWait` (ini `fg_wait`, diagnostic): `free` by default. While
DLSS-G paces the chain, the game's waitable slot holds an always-set event;
the chain's own waitable is put back before every chain switch (the switch
compares and replaces it there), and whenever DLSS-G does not pace the chain.
`chain` keeps the behaviour before 0.32.0; `ab` alternates every 10 s. The
frame heartbeat logs `game_wait`: the phase, flips, and per phase the presents
timed with their intervals and the Present calls' time.

Auto mode stays: paused it now runs faster than generating's base rate.

## Generating: DLSS-G's work on the game's path (dev.18, run 37180)

With DLSS-G loaded and generating, the game renders at about 190 fps in the
hub against about 270 unloaded (3.7 to 5.2 ms a frame). The driver sets no
frame limit (its sleep interval, logged since dev.16, stays at 3.3-4.1 ms),
the GPU load barely matters (DLAA and Performance CNN alike), 6x costs about
what 2x does, and cpuopt lifts it to about 200. In the event hub the game is
CPU-bound at about 180 fps even with DLSS-G unloaded; run 58748's "leftover"
was that, not a limit left behind.

The CPU sampler (FG off against 2x, three clean 20 s windows each, module
level only for NVIDIA's DLLs):

- the game's Present thread spends 49 % of its samples in ntdll (waiting),
  against 29 %; the game's own code 35 against 38 %; this mod's DLL 0.8
  against 1.8 %; Streamline's DLLs do not register on that thread;
- its innermost game frame is `cD3DRender::present12` (0x23a9ce0) in 41.5 %
  of the samples, against 8.3 %.

`present12`, before it presents, waits on the frame fence (cD3DRender +0x168)
until the previous frame's value: slot `(index + 2) % 3` of the three at
+0x1F0, which `endFrame12` (0x23a7ee0) fills right after its
`mCmdQueue->Signal(mFence.Get(), v)`; the index (+0x1E8) is incremented by
0x23a2b50. So MHW keeps one frame in flight. Streamline's DLSS-G header
(2.12): on D3D12 the client's presenting queue "is blocked until DLSSG
workload execution completes" (`DLSSGQueueParallelismMode`; the
non-blocking mode is Vulkan only). The hypothesis: each real frame carries
DLSS-G's workload on the game's critical path, and one more frame in flight
would hide it. PresentMon's fgcap2 (dev.13/14, before the free wait) seemed
to agree: at Performance CNN and 2x the GPU was busy 3.7 ms of each 5.7 ms
real frame. The experiment below disproved it.

## The experiment: one more frame in flight (dev.19, `fg_queue`, removed)

What a deeper queue would touch (clean image, IDA):

- Main:Update (0x1af2b60) begins the frame (sub_142293280: the frame
  counter, `frame % 3`, `frame & 3`, the parity, then the backend's begin
  sub_1423A2B50) and ends it with endFrame and present (sub_1422980B0) on the
  same thread, so the next frame begins only after the present's wait.
- The backend's begin increments the index and resets the command
  allocators for `index % 3` (`MAX_INFLIGHT_COUNT`, three sets);
  `mTempDynamicBuffer` is three frames' worth (sRender +0x1F420, picked by
  `frame % 3`); the per-frame constant arena has three slots and, in mode 2
  (this machine: one resource for the CPU and the GPU), no copy; the
  deferred releases cycle four lists.
- With the present waiting for the frame before its last, the next frame
  reuses the slots of the frame three back, which that wait has completed.
  Arena modes 0 and 1 copy at the next frame begin, which a deeper queue
  would race: refused.
- Not yet checked: per-frame CPU writes in two-deep (parity) buffers the GPU
  reads, and CPU reads of GPU results that expect the last frame complete
  (the frame counter has about 80 readers).

`reflex::GameQueue` (ini `fg_queue`, diagnostic, `game` by default): `deep`
makes the next present wait for the frame before its last while DLSS-G paces
the chain, by replacing, at the game's Present, the fence value the next
present waits on (this frame's, `values[index % 3]`) with the last frame's.
Only when the index follows on by one and the value grew (`queue_follows`),
so the wait never reaches further back. Gated: endFrame12's store, present12's
wait, the index increment, the endFrame/present pair, Main:Update's two calls.
`ab` alternated every 10 s; the frame heartbeat logged `game_queue` (phase,
rewrites, refusals, present intervals per phase).

Run 56996 (dev.19, hub, standing still, 2x, PresentMon 2.6.0 session
`20261027-fgqueue`; per 20 s window and phase, 0 refusals):

| Mode | game's wait | deep | GPU busy per real frame |
|---|---|---|---|
| DLAA | 194 fps, PCL 15.4 ms | 194 fps, PCL 15.4 ms | 5.3 ms (frame 5.15 ms) |
| Performance CNN | 285 fps, PCL 11.1 ms | 285 fps, PCL 11.1 ms | 3.65 ms (frame 3.5 ms) |

No difference in either mode: since the free wait (dev.17) the generating
game is GPU-bound, the GPU busy for the whole real frame. At DLAA frame
generation adds about 1.8 ms of GPU work per real frame (5.3 against about
3.5 ms unloaded, fgcap2): DLSS-G's own work plus the mod's input copies for
it. At Performance CNN the base rate (285 fps) is the unloaded one. The code
was removed again (an unaudited path for no gain); what is left to trim is
the mod's own GPU work (its copies), which helps DLSS and frame generation
alike.

## Frame pacing while generating (2026-10-27)

The 0.31.0/0.40.0 comparison (dlss-own-work.md) showed the same frame rate
with frame generation 2x but a lower 1 % low in 0.40.0 (260 against 280).
Checked with `fg_wait = ab` (free and chain every 10 s, one launch), each
PresentMon frame sorted into its phase by the frame heartbeat's QPC/tick
clock pair, 600 ms around each flip left out (sessions `20261027-fgwait-ab`
and `20261027-031-repeat`; hub, standing still, DLAA, Transformer K, 5090 at
1440p, 0.40.0-rc.2):

| Run | 1 % low, mean of 10 s blocks | Display latency | PC Latency |
|---|---|---|---|
| 0.40, DLSS-G 310.9.16, chain | ~259 (8 blocks, 249-266) | 6.1-6.4 ms | 14.3-16.0 ms |
| 0.40, DLSS-G 310.9.16, free | ~263 (7 blocks, 260-267) | 6.1-6.3 ms | 14.4-15.6 ms |
| 0.40, DLSS-G 310.7, chain and free | ~268 (35 blocks, 260-276) | 5.3-6.3 ms | 12.5-15.4 ms |
| 0.31.0 (310.7), first run | ~280 (6 slices, 279-281) | 6.7-6.9 ms | not reported |
| 0.31.0 (310.7), repeat | ~273 (15 slices, 268-279) | 6.7-7.0 ms | not reported |

- The free wait doesn't change pacing while DLSS-G generates (chain and free
  equal in both A/B runs), so `fg_wait = free` stays the default.
- DLSS-G 310.7 against 310.9.16 (one run each): 268 against 261, inside the
  run-to-run spread of the same 0.31.0 build (280 and 273). 0.40.0 keeps
  310.9.16.
- The remaining gap to 0.31.0 (2-4 %, about 0.1 ms at the 99th percentile of
  ~2.6 ms display intervals) is within that spread; not followed further.
