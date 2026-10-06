# Five-sample depth sequence: history capacity rejection

Run `49240`, process creation FILETIME `134347612610388148`, used v0.9.0 on
2026-09-25. This run is **rejected** and saved no depth images. The independent
archived reviewer rejects it as `unclean shutdown`; that describes the recorder's
fatal status, not a game crash. The user subsequently exited the game normally.

The executable was version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
The loaded diagnostic DLL was SHA-256
`3B5B9BE399E3833787E81D70FBE99BC997111F5E5A28B18F9E57370715FA6046`.
The existing barebones profile used DX12, TAA On and DLSS Off at 2560×1440.

## Confirmed observations

The archived CPU stimulus validator accepts five source samples at frames
10926, 10927, 10929, 10930 and 10931. Stages 1–3 each injected a distinct native
jitter pair and restored camera `+38` to zero before publishing the source.
There were three injections and three restorations, one untouched primary-draw
wait and no recovery deferrals. The first draw returned at core order 36;
stage 2 began at order 37. View matrices are bit-identical across all samples,
and the baseline/recovery constant payload hashes match exactly.

All five slots issued their bounded copies and one successful private Signal
each. Every private fence was observed at value 1. Their original request
stamps match the five CPU sources. These are partial observations; the complete
GPU sequence is not accepted. No slot recorded retirement or mapped its buffer,
and no pixel file exists. Issued allocations were quarantined and retained
until process exit.

The first provenance error is `Error::capacity` (1): history reservation 4097
exceeded the fixed 4096-entry buffer. The retained history spans 235 ms,
well within the two-second window. Other buffers did not reach their bounds:
9734 barriers, 198 copies, 266 submitted list references and 72 packet
receipts/consumers. Resource-barrier API calls account for 2551 history entries;
2413 entries were already present before the baseline selection. History
deliberately includes the pre-selection interval needed for state reconstruction.

The retained log contains a successful later Reset for every readback list:

| Stage | Request frame | Copy submission | Later Reset interval |
| --- | ---: | --- | --- |
| Baseline 0 | 10926 | 5605–5608 | 5721–5722 |
| Stimulus 1 | 10927 | 6113–6116 | 6233–6234 |
| Stimulus 2 | 10929 | 7127–7130 | 7245–7246 |
| Stimulus 3 | 10930 | 7634–7637 | 7752–7753 |
| Recovery 4 | 10931 | 8140–8143 | 8258–8259 |

Those Reset records do not retroactively establish accepted retirement after
history overflow. The worker's `poll_watch()` polls the private fences, then
calls retirement validation only while provenance is healthy. In production,
that worker polling resumes after the core CPU sequence completes; the core
recording loop polls only its deadline. The capture log does not record every
worker scheduling opportunity, so it cannot establish exactly which earlier
retirement opportunity was missed. The confirmed failure is capacity exhaustion.

Core completion reports zero faults, drops and pending calls, and all three
mutations restored. Provenance drained, all 396 observer copy references were
released, and all hooks disabled. The fatal provenance status still rejects
the capture as a whole. There is no pixel displacement measurement from this run.

## Closure and next change

The previous slot DLL was restored exactly to SHA-256
`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
The test output sidecar was removed, and all four configuration hashes remained
unchanged. All 87 archived sources verified. The local archive seals 214
evidence files, including the rejected trace, analysis, source identities,
build/test receipts and rollback receipts, under
`local/projection-binding-20260923/v0.9.0/`. Private shader bytes remain ignored.

Next, size the bounded history for the five-slot workload and verify its
capacity and retirement behavior offline before another launch. An 8192-entry
history is a candidate, not a validated fix; account for the larger static
storage and stack scratch arrays, and update versioned reviewer expectations.
Keep the two-second, three-injection and five-copy/Signal limits, with all
request/state/completion/retirement checks intact. Do not overwrite old history,
discard the pre-selection interval or relax review to accept this run.

The accepted [v0.8.4 triplet](depth-triplet-result.md) remains the latest pixel
evidence. This run adds evidence for bounded three-sample CPU injection and
restoration, not multi-sample raster validation, sustained jitter or working DLAA.
