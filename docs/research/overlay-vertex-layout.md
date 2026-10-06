# Native overlay vertex-layout support

## Evidence and scope

v1.25.24 established a 28-byte stride at the cutscene's failed vertex gate.
It did not capture the complete input layout. The preserved 644-byte vertex
shader (`730b7d23bcdca40477ed973c29c8fff8de7ab7244b05d142d5f9458fbfec5fdd`)
reads POSITION0.xy and TEXCOORD0.xy and passes them through, supplying constant
clip Z/W. v1.25.25 decodes those attributes from the native input layout instead
of assuming a 16-byte position/UV record. No effect names select this behavior.
Live cutscene correctness remains unverified until the next run.

## Contracts

Only the existing qualified in-place overlay route gains layout support.
The other native effects retain their 16-byte contract. Overlays require the
same pinned VS, eligible UV-only pixel shader, target, queue order, attachment,
shader/binding identity, full-clip triangle strip and inherited-state contract.
The native PSO, vertex resource, input layout, constants, UVs, texture bindings
and blending remain intact; only viewport/scissor expand to output dimensions.

The decoder uses bounded semantic-name copies and at most 32 descriptors. Its
two consumed attributes must be float32 XY/XYZ/XYZW in slot zero, per vertex,
with zero instance step. It reads XY at their declared offsets, not the first
four floats. Both attributes must fit the 16..255-byte stride, have four-byte
aligned offsets and nonoverlapping XY storage. Unused attributes are retained.
Append offsets are supported only after a known format with an already
four-byte-aligned end; unresolved appends and unsupported consumed formats
reject. Finite XY/UV and exact full-screen strip coverage are still mandatory.

The bounded CPU read contains four complete records (maximum 1,020 bytes).
Offset plus first vertex times stride plus record bytes must fit the existing
32 MiB arena window. Draw time rechecks the exact qualified stride and byte
offset against the native GPU vertex view and verifies the entire drawn range
fits. COM identity, device, buffer description, GPU address, backend ownership,
Reset and GPU-completion lifetime checks remain in force. This adds no hooks,
calling conventions, game writes, shader substitution or resource-state guess.
Existing native hook contracts apply to executable SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.

## Telemetry

`diagnostics.vertex_policy = native-overlay-layout-v1` distinguishes the new
stride gate from historical packed-only receipts. Check 3 predicate 2 now means
stride below 16 for overlays; other stages still require exactly 16. Layout or
coverage rejection uses check 5; bounded vertex reads use check 4.

`overlays.extended_draws` counts recorded overlay draws with non-16-byte stride.
`extended_layout` is null until one is recorded, then contains the latest
`[stride, input_count, position_format, position_offset, uv_format, uv_offset]`.
The existing route mutex protects both. These receipts prove the recording
path; SDK evaluation and GPU completion must be checked separately. They do not
claim the reconstructed side of predication executed on every recorded draw.
Review accepts historical receipts and requires the new fields for v1.25.25.

## Offline verification

Release builds passed `/W4 /WX`. All 56 focused native checks and 41 Python
tests passed. New checks cover reordered and padded 28-byte records, unused
attributes, explicit/append offsets, invalid formats/slots/instances, finite
values, coverage, nonzero first vertex, CPU bounds, native GPU bounds and
stride/offset changes. Graph fixtures exercise actual production discovery.

WARP and RTX 5090 pixel tests compare expanded overlays against independent
full-size native draws using the captured vignette shaders. Every pixel matches;
the half-size negative control differs, and native raster state is restored.
Continuous fixtures passed 384 frames across WARP/RTX 5090, including native
and reconstructed predication choices, four command lists, all 24 CPU recording
orders, rotating vertex resources, GPU validation and release after completion
plus Reset. The packed layout regression also passed.

An initial padded continuous fixture failed because its buffer-rotation helper
still allocated/uploaded 16-byte records after frame one. GPU validation caught
the short resource. The helper now uses the same layout for initial and rotated
buffers; failure logs and successful reruns are preserved. This was a fixture
error, not evidence of a game failure.

Historical v1.25.24 full review passes. A synthetic v1.25.25 schema example passes
and five corrupted examples reject; these are explicitly not live evidence.
Evidence, source snapshot and exact deployment backups live under
`local/overlay-layout-support-20261001`.

## Next runtime test

Arm at the main menu, load a save and replay the affected cutscene once. Check
SDK evaluations, overlay receipts/rejections and the image during the cutscene,
then verify normal-scene recovery. Announce intentional stop, verify cleanup
and native bytes, then request normal exit. No extended roaming is needed.
Keep the current DLL installed afterward. Settings/profile stay unchanged.

## Live result, 2026-10-01

PID 13240 / creation 134352812940432461 loaded v1.25.25, DLL SHA-256
`e84e443bfd8a32f62b892681a6e0949c6babd1912378999158d7c0d444bc117c`.
Startup, frozen sources, runtime module provenance and settings passed checks.
The user reported no rectangle during the affected cutscene and active, clean
DLSS while walking/panning afterward in Castle Schrade.

The live overlay used stride 28 with four input descriptors: float2 POSITION0
at offset 0 and float2 TEXCOORD0 at offset 8. This differs from the deliberately
reordered/padded offline fixtures and was decoded from the native layout.
800 recorded overlay draws used that layout. The other descriptors were not
captured by this receipt; they remain native and are unused by the pinned VS.
DLSS evaluations continued, with zero overlay rejections or runtime faults.

The intentional stop finished at 6,346 DLSS frames. All 3,148 overlay draws
completed; all 6,353 GPU signals completed; 6,352 private and 6,408 downstream
recordings released with none live. All 6,817 jitter changes were restored,
full resolution returned, and read-only live checks verified 38 native function
spans and 18 hook heads. Frozen full review accepted the run. The user confirmed
normal appearance and normal exit; process absence and unchanged settings were
verified. The current DLL remains installed.

This targeted test passed. Seven total skips, five successfully resumed
suspension periods and two reason-6/detail-4 graph refusals remain in the log;
zero overlay refusals does not imply universally skip-free rendering.
Evidence is sealed in `local/overlay-layout-support-20261001`.
