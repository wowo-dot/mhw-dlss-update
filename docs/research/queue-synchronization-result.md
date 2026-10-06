# Queue fence observation — run 21900

2026-09-24, [recorder v0.6.0](../runtime/projection-sync-recorder.md).
The selected draw now has a validated view lifetime, an observed submitted
recording, a depth-write declaration across adjacent DIRECT submissions, and
a following graphics-fence signal whose value is reached by a native read.
This is a bounded completion observation, not complete resource-access or GPU
content validation. The game exited normally and the previous DLL is restored.

## Capture identity and selected work

PID `21900`, creation `134347341552010815`, executable version `0.0.0.0`.
Executable SHA-256:
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
DLL SHA-256:
`0FA8E794087CECE79B2109D359F39B3D332FC48D8659298FDB6C9626DC04B704`.
Five startup and 39 core gates passed, covering 43 distinct spans. The 37
hook targets comprise 12 native functions and 25 APIs. No jitter writes or
rendering commands were added.

Frame/epoch `14548`, root parameter 9 / native VS b0, indexed draw
`(2520,1,2586,0,0)`. The depth-only attachment is 2560×1440 R32_TYPELESS,
one mip/slice/sample, flags ALLOW_DEPTH_STENCIL. Actual DSV descriptor
`[40,3,0,0,0,0]` identifies writable D32_FLOAT/TEXTURE2D/mip 0; native recipe
`[2,0,1,17,0]` is separate evidence. Resource interface `358065648`, canonical
identity `358065600`, device `207717088`; all three resource-query phases agree.

The selected list `357761488` resets at `4947/4948`, binds depth at
`5253/5254`, records the selected draw at `5255` (native return `5256`),
closes at `5259/5260` and submits at `5291/5292`, first in seven lists.
DIRECT queue `207706368`, canonical identity `207706320`, has description
`[0,0,0,1]`. Its preceding submission `5283/5284` contains a complete
Reset `4931` to Close `5251` generation with the selected depth resource's
`0x800 → 0x10` transition and matching clear. Replay establishes the narrower
`same_queue_depth_state_declared=true`; the older single-batch/global fields
remain false.

## Observed handoff and completion

| Recorded call | Entry / return order | Fence / value |
| --- | --- | --- |
| Previous DIRECT Execute | 5283 / 5284 | — |
| DIRECT Signal | 5285 / 5286 | 205263888 / 22863 |
| COMPUTE Wait | 5287 / 5288 | 205263888 / 22863 |
| COMPUTE Execute | 5289 / 5290 | — |
| Selected DIRECT Execute | 5291 / 5292 | — |
| COMPUTE Signal | 5293 / 5294 | 205263888 / 22864 |
| DIRECT Wait | 5295 / 5296 | 205263888 / 22864 |
| Later DIRECT Execute | 5297 / 5300 | — |
| Wait inside that later Execute interval | 5298 / 5299 | 1296111040 / 12734 |
| DIRECT Wait for COPY fence | 5301 / 5302 | 205260688 / 16273 |
| Qualifying DIRECT Signal | 5303 / 5304 | 205261888 / 14566 |
| Immediate native completion read | 5305 / 5306 | 205261888 / 14565 |
| Later native completion read | 5771 / 5772 | 205261888 / 14566 |

The completion fence has canonical identity `205261840`, owning device
`207717088`, and creation flags zero. All 14 observed writes to this fence
are successful, nonoverlapping, monotonically increasing DIRECT-queue signals
(`14554…14567`); no observed CPU or other-queue writer targets it. Its native
reads are monotonic and nonoverlapping. The last below-target baseline before
the qualifying Signal is order `4837`, value `14564`. The final read reaches
the selected Signal's exact value. These are observations within the journal,
not a reconstruction of the fence's entire lifetime.

The graphics/compute fence traffic matches the prior static map. The selected
DIRECT batch precedes the COMPUTE-to-DIRECT Wait, so this does not establish
that compute finished before the selected draw. The whole capture contains
55 exact-value Signal/Wait correlations and 70 unmatched waits. CPU-written
fences and a shared fence also appear. Unmatched waits are retained, not
treated as absent dependencies; complete cross-queue access safety is unknown.

## Offline reviewer correction

The original review rejected completion with `failed/overlapping queue operation`.
Its blanket interval rule also rejected a successful Wait strictly inside a
later Execute on the same queue and thread. Here both calls return before
the qualifying Signal, and both begin after the selected Execute returns.
The Wait caller is outside the game image; no module attribution is claimed.

The follow-up permits only this strictly contained, same-thread Wait case in
a later Execute. It does not infer ordering inside that batch. Overlapping
threads, crossing intervals, overlap with the selected Execute or qualifying
Signal, nested signals/submissions, failed waits, competing fence writers and
rewound values remain rejected. Eighty-three synchronization tests pass,
including synthetic nested-interval cases and composition with owned SDK
fixtures. All 16 existing owned fixture reviews are unchanged, as are five
accepted prior game reviews; two rejected game captures remain rejected.
The binary and captured data did not change, and no second game run was needed.

This narrow interpretation uses the documented queue semantics:
[Signal](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-signal)
adds a GPU-side fence update, while
[Wait](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-wait)
queues a GPU wait rather than establishing completion when the CPU call returns.
Separate [Execute calls](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-executecommandlists)
preserve queue workload order. These API contracts do not certify resource
usage or the provenance of every writer outside our recorded window.

## Archive and next decision

The journal contains 55 view events, 2,829 history calls, 7,048 barriers and
211 list entries in 113 submissions (85 DIRECT, 14 COPY, 14 COMPUTE). It pins
23 lists, three queues and 11 fences, then releases all references. Entry
ticks span 156 ms; this is not a GPU duration. Both channels drain cleanly,
with zero errors/faults/drops/pending and hooks disabled.

Archive: `local/projection-binding-20260923/v0.6.0/`. The original
`review-21900.json` reproduces exactly with the preserved build-time reviewer
(`review-21900-archived-replay.json`). `review-21900-followup.json` records the
corrected bounded completion observation. `synchronization-history-21900.json`,
`acceptance-21900.json` and `archive-21900.json` seal the run. The 52 build-time
source snapshots remain unchanged; follow-up code and notes are separately
snapshotted. Preparation passed 116 native and 276 Python tests; the follow-up
changed only offline review logic and passed its 83-test suite.

The prior DLL hash is restored:
`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
The temporary output sidecar is removed and all four configuration hashes
are unchanged. Main log SHA-256:
`7910AA4A27F267BE5EF021C1C454F1293C726E61CC50258C2BF954BF94C86100`.
Companion SHA-256:
`001A2B9ACB70B747BE593362D83D0C7B42CFEFEE2B605C9B80649E31FA48D082`.

Next: audit depth accesses and identify a stable per-frame readback point.
Full cross-queue resource access coverage, state at GPU execution, contents,
executing PSO bytecode, raster displacement and working DLAA/DLSS remain
unverified. Keep the broader GPU safety fields false. Do not repeat this
unchanged capture merely to address the already-resolved interval rejection.
