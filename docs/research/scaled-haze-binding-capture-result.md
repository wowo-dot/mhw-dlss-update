# Haze enqueue capture: binding metadata correction required

2026-09-30, v1.25.8, PID 38772 / creation 134351943753041544.
Evidence: `local/scaled-haze-adapter-20260930`.

The live run produced 2,538 DLSS evaluations with zero runtime faults. Its
6,878 unfiltered frames comprise 6,877 missing haze plans and one warmup.
Both suspension periods recovered. The user reported normal recovery after
leaving the sauna and a normal scene after intentional stop, then exited.
All 9,416 reconstruction/depth and 6,878 unfiltered GPU recordings retired and
released; 9,417 jitter changes restored. Full render scale returned. Independent
read-only checks matched all 38 gated code spans and 17 native hook heads.
The full telemetry reviewer accepted cleanup, not a complete haze capture.

## Confirmed capture failure

All 16 bounded enqueue attempts reached phase 5 (bindings) and rejected with
reason 8. No complete same-frame recipe was retained. Native queue append,
geometry/layout, and exact VS/PS identity checks precede this phase.

A separate bounded read-only retained-packet inspection found the exact haze
pixel shader and two equal binding snapshots. These are retained CPU values,
not proof of active queue membership, frame alignment, GPU descriptors or
resource states. See `haze-binding-diagnostic-38772-02.json`; the first stricter
inspection rejected the same malformed range interpretation.

The shader-stage blocks are 12 bytes each. The native per-stage binding loop
at RVA 0x259f330 reads only the first u16 count and u16 start (0x259f35d,
0x259f370, 0x259f578..587). It does not interpret the remaining eight bytes as
two more count/start pairs. v1.25.8 did, incorrectly rejecting legitimate opaque
metadata: VS halfwords [0,0,0,65280,65535,1], PS [9,0,4,4,255,0].
The native graphics dispatcher also tests flags at set+0x10 and set+0x1c.

The observed PS range contains nine words: two samplers, four texture SRVs,
and three constant-buffer entries. SRVs resolve to t0 color 2560x1440,
t1 depth 2560x1440, t2 volume 32x32, t3 fallback filter 1x1. These dimensions
are engine metadata only. All four source CPU descriptor handles were nonzero.

## Next correction

Parse one bounded count/start pair per stage, preserve the other eight bytes
as uninterpreted metadata and include them in the repeat check. Keep all
texture/register/named-owner and same-frame checks. Cover real sentinel
metadata, nonzero range starts, and invalid live ranges in the synthetic
fixture. Repeat the sauna capture before integrating the optional haze pass.
The tested offline shader adapter remains disabled in live rendering.

The 639 camera-displacement history resets are a separate observation; this
run was not a controlled panning experiment. CPU ordering, history policy and
performance cleanup remain separate work.

## v1.25.9 correction prepared

The in-process recorder now reads exactly one u16 count/start pair per stage,
with at most 128 words and a 4096-word end bound. An empty range does not
traverse its unused start. The following eight bytes are preserved as opaque
metadata and compared during the same-frame stability check. Serialized
haze schema 2 / `bounded-haze-enqueue-v2` separates `stage_bindings` from the
historical incorrect three-range schema. The external reader is corrected too.
The v1.25.8 reader remains available in the sealed archive. The current log
reviewer supports historical schema 1 but requires schema 2 for v1.25.9.

Native synthetic capture passes three recipes and ten malformed cases, plus
foreign-caller and capacity checks. New assertions cover the observed VS/PS
sentinel metadata, an empty range with unused start 65535, a nonzero live start,
and oversized/out-of-bounds live ranges. The existing GPU route passes
96 frames / 288 effects / 786,432 exact pixels with validation and retirement.
Seven external-reader, five haze-reviewer (16 corruption subcases), and
13 existing log-review tests pass. The actual 2,538-frame v1.25.8 log still
replays successfully; a labeled synthetic v1.25.9 replay accepts the new
capture and rejects missing or old-schema channels. The corrected targets
build with /W4 /WX and no project diagnostics. The initial build invocation
used a nonexistent fixture target name; the corrected target builds cleanly.

Evidence/build/deployment: `local/scaled-haze-bindings-20260930`.
No new hooks, graph admission changes, or live haze shader use. Runtime
validation remains pending. Use the same MO2 entry with native AA/DLSS Off,
windowed 2560x1440, and load outside the sauna before arming. Keep the latest
DLL installed. `deploy.ps1 -Action Restore` restores the exact backed-up prior
DLL and output-sidecar only if explicitly wanted; no settings are changed.
