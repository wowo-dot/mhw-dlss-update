# Depth-copy request frame and pixels: v0.8.1 run 43440

Accepted 2026-09-24 after a loaded-save capture with DX12 On, DLSS Off and TAA On.
The unchanged deployed reviewer accepts the complete capture. The original copy
request has frame/epoch **14826**, while the selected projection draw has
frame/epoch **14825**. This establishes the request stamp and its actual native
copy/readback chain; all pixel-producing draws are not yet covered.

The [v0.8.1 recorder](../runtime/projection-depth-frame.md) adds no jitter and
uses the previous one-readback-copy / one-private-Signal budget. Executable
version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
DLL SHA-256
`1E33E9D990CE2BB24F8138C90E574879C8DF9F58FDCB3900F82DE143A29B864B`.
Process generation: PID 43440, creation FILETIME 134347553216060126.

## Confirmed chain

| Observation | Recorded evidence |
| --- | --- |
| Original producer | Orders 5514–5528, frame/epoch 14826, bank 0, async mode 1, primary frontend context |
| Queued packet | Pointer 3221178080, opcode 10, source kind 8, full 2560×1440 rectangle, queue count 50→52 |
| Selected draw marker | Order 5563, projection frame/epoch 14825; submitted at 5599 |
| DX12 consumer | Orders 5905–5973, receipt 48, unchanged packet fields |
| Native depth copy | Orders 5931–5962; source 1350021552, canonical identity 1350021504, destination 5260535952 |
| Added readback | Orders 5965–5966; source subresource zero, full extent, private READBACK buffer |
| Recording | Reset 5721, adjacent barrier 5925, Close 6091, command list 357722784 |
| Submission | DIRECT Execute 6109–6112, batch index 1; queue 207599232 |
| Private completion | Signal 6113–6114 to value 1; completion observed and successful later Reset 6227–6228 before Map |

The producer returns before the older selected draw is recorded. The selected
consumer runs on a different thread. This is direct evidence of overlapping
frontend construction and worker recording, and explains why a global frame
sample taken at copy execution would not be a reliable original request stamp.
The CPU orders above do not imply a GPU draw execution order by themselves.

Fifty-six producer receipts cover frames 14815–14828, four per frame. Fifty-two
copies link to matching receipts across frames 14815–14827. Four consumers have
no captured producer and four final producer receipts have no captured consumer;
these remain unmatched. The selected readback has a complete unique chain.

For frame 14826, three copies declare local source state `0x840`, while the
selected copy declares pure COPY_SOURCE `0x800`. The two destinations and four
stages repeat the earlier structural pattern. CPU recording order still differs
from submission-array order: copy 6000 is in batch index 0 of submission 6109,
while selected copy 5931 is in index 1. The original request stamp now disambiguates
their frame, but does not by itself identify every contributing depth draw.

## Pixels, shutdown and scope

The capture saves 14,745,600 bytes: 3,686,400 finite depth values at 2560×1440,
8,548 zeros, no nonfinite or out-of-range values. Minimum is 0 and maximum is
0.06881000101566315. Pixel SHA-256:
`8f63aaaa761bc531258a9c3f67bb776a9972d95f62f70300aad16290b1d20f6c`.
The scientific preview shows coherent character/weapon, floor, columns, stairs,
railings and a hanging fixture. Its logarithmic color scale is not world distance,
and visual similarity is not proof of frame alignment or final-scene coverage.

The native journal has 55 watch events, 3,169 history calls, 7,589 barriers,
116 submission calls, 214 submitted list entries and 154 copies. All 308 observer
copy references release after drain. The one-shot readback's private references
remain retained until process exit, as designed. There are zero faults, drops or
pending calls; all hooks disable and the recorder stops without a fatal error.

The user exited normally. The exact prior v0.15 DLL was restored, its output
sidecar removed, and all four configuration identities match. All 73 archived
build sources verify. Local directory `local/projection-binding-20260923/v0.8.1/`
contains `review-43440.json`, `packet-analysis-43440.json`,
`acceptance-43440.json`, `archive-43440.json`, `restored.json`, raw depth and
`depth-preview-43440.png`. This runtime cycle is closed.

## Next decision

No unchanged v0.8.1 rerun is needed. Next, associate the bounded projection
stimulus with the same depth-copy request stage and frame, then qualify an
aligned baseline/jitter/restoration pixel comparison. The +1 frame difference
observed here is a result, not a constant to encode into the implementation.
Complete pixel-frame origin, raster jitter, final-scene depth and working DLAA
remain unverified.
