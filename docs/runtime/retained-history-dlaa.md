# Bounded retained-history DLAA — v1.14.0

Completed: [accepted live run 49200](../research/first-live-dlaa-history.md).
Three history-retaining outputs pass the frozen full review. Normal exit,
exact previous-DLL restoration and unchanged configurations are verified.
The following describes the frozen experiment.

This follows the [first live private outputs](../research/first-live-dlaa-output.md).
The next live experiment combines the existing baseline/stimulus/recovery
projection recorder with matched color, depth and motion copies, then evaluates
the three adjacent frames on one private DLAA feature with reset **true, false,
false**. DLSS remains Off, TAA remains On, and SDK output is saved offscreen.

## Projection and history contract

The projection helper is the previously validated Windows x64 hook at
`+0x228F3B0`, called from `+0x228E834`, with RCX=camera+0x50,
RDX=renderer+0x710 and AL return. Its 2,000-byte hash is
`df1bf81584401fe529bc5fbbc3e19010e8959c2d13745eca6f87897183148373`.
Tested executable version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
The [existing selection and lifetime rules](temporal-jitter-selection.md)
apply: at most 20 next-entry previews, both native scalar magnitudes at least
0.3125 and at most 0.5, one compare-exchange injection on the middle frame,
unconditional scoped restoration including exceptional unwind, and exact
projection/TAA frame, epoch, bank, context, view and async association.

The native matrix delta is `(jx / width, jy / height)` in clip coordinates.
D3D viewport conversion therefore gives the DLAA input-pixel offset
`(+jx / 2, -jy / 2)`. The NVIDIA guide defines jitter in input pixels in the
same upper-left-origin coordinate system as motion (+X right, +Y down).
[Pinned public SDK guide, sections 3.6 and 3.7.3](https://github.com/NVIDIA/DLSS/blob/a291cc7d2cc642a51566f3dfd5376f635cd1b284/doc/DLSS_Programming_Guide_Release.pdf).
The mapping is derived from the recorded matrices; the earlier
[color/motion stimulus](../research/ordinary-taa-strong-jitter-result.md)
supports its direction and jitter-excluded static motion. Current-frame live
DLAA quality remains to be checked. MVJittered stays false, scale stays (1,1).

Only disabled, drained, completely matched and restored projection receipts
can supply history metadata. The evaluator rejects frame/epoch gaps, wrapping
counters, nonfinite jitter and out-of-range offsets before SDK work. Three
samples are a transport/retention check, not a production jitter sequence.
Time remains the explicitly logged **fixed diagnostic 16.666666 ms**, not a
measurement of game frame intervals. Continuous jitter, actual timing and
scene-change resets remain future integration work.

## Ownership and bounded work

The startup transaction, retained provider/device ownership, resource formats,
copies, GPU state restoration, fences and process-lifetime retention are
unchanged from [v1.13](startup-live-dlaa.md). There are eleven armed native
hooks instead of ten. The startup hook is already disabled before arming.
The extra projection target is the origin of the existing reserved near-code
pool. No new allocator fallback or startup timing change is introduced.

All native hooks disable and drain before SDK evaluation. One CreateFeature,
three EvaluateFeature calls, one ReleaseFeature and DestroyParameters remain
the limits. There is no repeated SDK Init or borrowed-device Shutdown.
SDK log version 4 adds one `sdk_temporal` record immediately before each
EvaluateFeature. Review binds its frame, epoch, reset, jitter and fixed time
policy to the independently reviewed native projection/input receipts.
Historical versions retain their original scope restrictions.

## Offline verification

Release builds pass with /W4 /WX. Forty-three native tests pass, including
pure history admission, existing jitter exception/restoration cases, combined
jitter/depth/owned-copy success and rejection cases, and SDK-free controls.
The combined WARP fixture uses GPU validation. Its initial sequence staged
the frame epoch after the depth request; correcting the fixture to stage
projection before depth production resolved all four initial failures without
relaxing any production guard. Both failed and corrected logs are retained.

Four RTX SDK fixtures pass with zero debug errors/warnings: reset control,
retained history with synthetic nonzero jitter, retained history with zero
jitter, and a frame gap rejected before evaluation. They replay archived
unjittered input pixels, so their temporal metadata is explicitly synthetic.
Frame zero is byte-identical to reset control; later frames change even in
the zero-jitter history control. This establishes retention, not quality.
Full artifact review checks 33,177,600 RGB values and rejects a rehashed NaN.
Three groups of 14 SDK reviewer tests cover reset/history and wrong-sign,
wrong-scale, frame-binding, ordering, capability and ownership failures.
Six combined native reviewer tests, seven startup-link tests, 15 startup
reviewer tests and 61 historical capture tests also pass (historical-only
skips are recorded). An old scene reviewer prohibition is made explicit by
version; only v1.14 admits independently verified paired jitter with depth.

## Run and rollback

Evidence: `local/dlaa-history-live-20260925/v1.14.0/`. The exact previous MO2
slot DLL and sidecar are preserved by the versioned deployment script.
Launch the existing barebones entry with DX12 On, DLSS Off and TAA On. Load
a save, keep camera still and preferably hold the character in a stationary
pose. Reply from the phone. The agent validates startup/process identity and
arms the two-second capture; no toggle is needed. A brief pause is possible.

After the evidence is saved, exit normally with DLSS still Off. Run the local
`deploy.ps1 -Action Restore` only after process exit; it verifies exact hashes,
restores the prior slot DLL and removes/restores its sidecar. Archive this run
before preparing another version. No original executable or game asset is
modified; the MO2 profile and graphics settings are unchanged.
