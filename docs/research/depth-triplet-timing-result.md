# Triplet draw-return timing failure and correction

## Observed v0.8.3 result

Run 14072 on 2026-09-25 is rejected. No depth pixels were mapped or saved.
The unchanged archived reviewer reports `unclean shutdown`, referring to the
recorder's fatal stop; the user subsequently closed the game normally.

The scoped CPU stimulus completed baseline/stimulus/recovery frames 12056,
12057 and 12059, with one recovery admission deferred. The single camera-field
injection was restored, and all three returned camera samples were zero.
Baseline and stimulus selections matched their original native copy requests.

The selected draw returned at core order 36, before CPU recovery at core order
39. However, `complete_if_ready()` published the provenance draw-return marker
only after recovery. The provenance journal shows:

| Event | Provenance order |
| --- | ---: |
| Selected draw marker | 5751 |
| Selected command-list submission | 5786–5787 |
| Stimulus copy-list submission | 5792–5795 |
| Delayed draw-return marker | 5796 |

Consequently, the stimulus copy submission encountered `draw_return == 0`.
Its before-Signal validation stopped provenance with `Error::changed`.
Recovery then failed to publish its selection, producing the secondary core
identity fault. This is recorder ordering, not evidence of missing game jitter.

The baseline had one private Signal; the stimulus had none; recovery issued no
copy. Issued readbacks remained quarantined until process exit. All 264 observer
copy references released, hooks disabled and pending calls drained to zero.
The exact previous v0.15 slot DLL and absent output sidecar were restored;
graphics, profile, modlist and bridge-mode hashes remained unchanged.

Local evidence is in `local/projection-binding-20260923/v0.8.3/`, including
`rejection-14072.json`, `archive-14072.json`, the raw journals and 82 verified
archived sources. The executable is version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.

## v0.8.4 correction

Publish `provenance::end_frame()` directly after the selected native draw returns
and its existing stability checks pass. A faulted draw does not publish it.
Overall core completion still requires all three CPU samples. Recovery cannot
publish a second draw-return marker. Copy submission still requires the actual
draw return, selected submission, matching depth/view and queue order before
issuing its private Signal.

Core hook fixtures reproduce the original failure: draw-first and deferred
recovery both failed before this change; recovery-first already passed. All
three pass after the correction, with assertions for no early marker, one marker
at draw return and no repeat at recovery. Exception and invalid-root fixtures
require that no marker be published. Fixture instrumentation is compiled only
under `MHW_BINDING_FIXTURE` and is absent from the diagnostic DLL.

The experiment retains one injection, three copies/Signals, the two-second
window, 64 recovery deferrals and all previous hook, ABI, lifetime and byte-gate
contracts documented in the [triplet protocol](../runtime/projection-depth-triplet.md).
The failed capture remains rejected. Three game depth images and measured raster
displacement remain pending; this correction does not establish working DLAA.
