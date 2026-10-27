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

## Not explained yet

With DLSS-G loaded and generating, the game renders at about 180-190 fps in
the hub against about 270 unloaded (about 1.5 ms a frame). The driver sets no
frame limit (its sleep interval, logged since dev.16, stays at 3.3-4.1 ms),
the GPU load barely matters (DLAA and Performance CNN alike), and cpuopt lifts
it to about 200: mostly CPU time on the frame's path. A profiling run follows.
In the event hub the game is CPU-bound at about 180 fps even with DLSS-G
unloaded; run 58748's "leftover" was that, not a limit left behind.
