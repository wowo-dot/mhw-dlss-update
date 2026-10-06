# Ordinary TAA submission and completion result — v1.3.0 run 14268

Accepted and closed on 2026-09-25. Three consecutive TAA request frames match
their native Dispatch recordings, distinct command-list generations, actual
DIRECT-queue submissions, completed private fences and retirement resets.
The unchanged archived reviewer accepts all three. This establishes the
execution/lifetime observations needed to design the next bounded pixel copy;
this observer saved no motion pixels and changed no rendering commands.

## Evidence and identity

- Host version `0.0.0.0`, SHA-256
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- Probe SHA-256
  `8a61d013c1cfdab11c43960ce5e463589db48a0373ae4b1c0738e109d174311b`.
- PID 14268, creation identity `134348048780327816`.
- Private evidence: `local/temporal-submission-20260925/v1.3.0-resume-20260925/`.
- Log `inputs-14268-134348048780327816.jsonl`, SHA-256
  `2047c5e5fbd1539db646cb20d0efaee36c778c386819b4fe3e5e9e8728c70127`.
- D3D12, adapter LUID high/low `0/97816`, swap-chain format 87,
  2560×1440. The bare profile retained DLSS Off and TAA On.

The resumed deployment used the exact archived DLL that passed 43 native and
22 Python tests. All 117 prelaunch files, 132 original preparation-archive
files, and 106 archived source files verify. The frozen reviewer independently
replays the full journal; separate analysis checks the selected generations,
allocator changes, motion transitions and shader identity.

## Confirmed observations

Frames/epochs are 26815, 26816 and 26817, banks 1/0/1, async flag 1.
Producer thread 5416 publishes their distinct packet receipts. Consumer thread
4520 records Dispatch groups 160×90×1 on the same native DIRECT list. The three
requests belong to distinct observed successful Reset generations, using three
different allocator identities, and are submitted on the same DIRECT queue.
Each selected submission contains two command-list pointers, including the
selected list exactly once.

The table contains journal ordering labels, not GPU timestamps:

| Frame | Reset | Dispatch | Close | Execute | Private Signal | Completion poll | Retirement Reset |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 26815 | 2331 | 2355 | 2357 | 2363 | 2365 | 2435 | 2409 |
| 26816 | 2409 | 2433 | 2436 | 2442 | 2444 | 2584 | 2488 |
| 26817 | 2488 | 2512 | 2514 | 2520 | 2522 | 2727 | 2560 |

Each private fence starts at zero and receives one observer queue Signal to
one after its original Execute returns. Each Signal succeeds and its fence is
observed complete at one. Later successful resets retire the selected list
generations, with no intervening or overlapping resubmission. Retirement is
observed before the worker's completion poll in these samples; acceptance
requires both observations and does not infer GPU completion from Reset alone.
All three completion polls finish by 203 ms after observation begins, within
the two-second bound (including the initial 150 ms of lifecycle warmup).

Each consumer again records GBuffer4 transitioning from `0x4` to `0xC0`
(RENDER_TARGET to combined pixel/non-pixel shader resource) before Dispatch.
Its resource is R32_UINT, 2560×1440, one mip/slice/sample. The shader is byte-
identical to run 4376: 15,492 bytes, SHA-256
`2d7b26c742c27db2c83546c6cf6a1dfda47d7cfd4336e69f8c2302de5802b0d1`.
The [static motion decode](ordinary-taa-shader-result.md) remains applicable
to this captured recipe, but actual pixel values have not been measured.

## Closure and next recorder

All observers report zero faults and pending callbacks, and all seven hooks
are disabled. The 21 resource references, three consumer-list references and
637 cumulative lifecycle reference acquisitions all have matching releases.
The lifecycle count is not a count of unique tracked objects; the journal
contains 21 admitted DIRECT lists, 44 allocators and one queue. Private fence
completion is established and no lifecycle references are quarantined.

The journal contains 3263 events: 1336 Reset, 1337 Close, 587 Execute and three
Dispatch markers. It stores 1252 submitted list pointers, with a maximum batch
of seven. All bounds are respected. However, 1904 events begin after the final
completion poll while hooks are being removed; the last event ends at 469 ms.
For the copy probe, stop lifecycle admission once all completion/retirement
witnesses are fixed, then disable hooks and drain already admitted callbacks.
This saves journal capacity without weakening the required observations.

The user confirmed normal exit. The prior DLL was restored exactly (SHA-256
`689bef7cf00ff71e4518f452f0467edf6c3d877f2a6dd090553388d8b5f288cc`),
the temporary output sidecar was removed, and all four graphics/profile/mode
configuration hashes stayed unchanged. No new test is installed or armed.

Next: insert a bounded R32_UINT motion-buffer copy at the matched native site,
with validated format/footprint, a transition to COPY_SOURCE, restoration to
the observed state, and completion/retirement checks before Map. Unlike this
fence-only observer, issued copy allocations need retention appropriate to
possible resubmission. Then compare stationary and deliberate camera-motion
controls, and resolve typed views, jitter inclusion and producer coverage.
Submission/completion/retirement verification is now true; readback eligibility,
motion contents and working DLAA remain unverified. No vendor SDK call or
output replacement occurred in this run.
