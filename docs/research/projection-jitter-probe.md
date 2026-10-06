# v0.13.0: observe camera jitter and projection constants

Updated 2026-09-23. Run 41776 completed the existing one-item diagnostic and
clean exit, but rejected the optional projection snapshot at checkpoint 2.
The [v0.13.1 type correction](camera-type-probe.md) is the next diagnostic.
This extends the accepted
[v0.12.0 publication receipt](publication-receipt-probe.md) with an optional,
bounded CPU observation at the existing producer hook. It does not invoke
modern DLSS, add another hook, or issue GPU commands.

## Run 41776: existing capture accepted, projection observation rejected

The exact DLL and all 21 preflight spans passed. The user enabled once, reported
a brief freeze, then turned Off/applied/exited normally after capture was saved.
One producer, four preparations, one correction and one consumer/Evaluate
completed without guard rejection. The source/publication frame and epoch were
11738/bank 0; preparation and Evaluate observed 11739/bank 1 with the same
descriptor/payload/resource receipt. All 17 parameter queries and inspection
completed. Reset 246, Close 387, submission 406, Signal 408 and completed query
615 at value 11763 form the retained completion chain (15 ms after Evaluate).
The recorder drained 615 records with flags 0.

The optional projection read stopped after 14 scalar reads at checkpoint 2.
Its epoch 11738 agrees with the source, and the buffer/parameter IDs are
initialized. Neither matrix, jitter pair nor viewport was read; their zeroed
output arrays are unpopulated fields, not observed zero jitter. The record did
not retain its vtable or individual rejection reason, so the specific failed
condition cannot be established from this capture alone. Offline follow-up
found a base-only type check that excludes a verified derived camera class.
The correction and its uncertainty are documented in the linked note.

Release and both Shutdowns succeeded, cleanup cleared, and saved settings match
original Off/TAA. The deployed DLL/sidecar and all 131 staged source files were
verified unchanged before subsequent edits. Trace duration was 117593 ms.
`local/temporal-inputs-20260923/acceptance-41776.json` indexes the private evidence.
Capture SHA-256 is
`934DDB8A83B4757599AFC7CE5AA2D6BE011E5E84C9A993432B9EDE1B5FD4005A`;
journal SHA-256 is
`8D693CCCA5DC76521BC3CE53CF98CA7C8F9EEBAE008661934B82D773AB1035E0`.
GPU/temporal correctness remains unqualified; no unchanged rerun is needed.

## Confirmed offline trace

Analysis is pinned to executable version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
The private decoded analysis image has SHA-256
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
All addresses below are RVAs in that exact image, not portable signatures.

The paths selecting `fJitterOffset` and camera projection jitter are different:

- Dispatch at `0x23C6C5D` selects the 18-entry table through `0x227F7B0`
  for mode 1, and the 20-entry table through `0x227DB60` for mode 2.
- The TAA path at `0x23931E0`, under its view/layer gates, selects the
  20-entry table and calls the camera setter at `0x228F390`.
- Renderer publication at `0x229A080`, when view flag `0x800000` is set,
  selects the 20-entry table for mode 1 and the 18-entry table for mode 2.
  Other modes clear jitter. With that flag clear, a stale camera epoch also
  clears jitter.

The TAA gates require neither view flag `0x40000` nor `0x800000`, nonzero
layer float `+0x180`, layer type `+0x0C == 8`, flags `+0x14 & 0x12 == 0x12`,
and enabled byte `+0x184`. The camera setter copies two floats to camera
object `+0x38/+0x3C` and stamps `+0x40` with the low 32 bits of renderer epoch
`+0x1F208`. The camera object is reached through global `0x51C4400`; its
constructor installs vtable `0x350D188`.

Constructor `0x227FF00` builds the 20-entry table from base-2/base-3 radical
inverses at indices 20 through 39, minus 0.5. The 18-entry table is built from
fixed constants. The latter reproduces run 4768's captured candidate bits,
but that does not qualify the candidate as camera jitter. At that run's counter
10791, the reconstructed 20-entry candidate would be approximately
`[0.46875, -0.0432098508]`. This is an offline candidate, not a live observation.
That run's unresolved `fJitterOffset` ID was `0xFFFFFFFF`.

View setup `0x228F3B0` constructs row-vector translation matrix `J` with
`J[12] = stored_jitter_x / viewport_width` and
`J[13] = stored_jitter_y / viewport_height`. Constant publication `0x23D0E70`
then produces:

```text
fProj         = P * J * D
fNoJitterProj = P * D
D = identity, except D[10] = -1 and D[14] = 1
```

The multiply helper is `0x28E280`; the matrix copy helper is `0x1FAD4C0`.
The latter decodes columns/rows from parameter-ID bits 10/8 and a float-word
offset from bits 12..21, writing destination rows with stride four floats.
Cached IDs are `CBViewProjection=0x4BE75F4`, `fProj=0x4BE7604`, and
`fNoJitterProj=0x4BE763C`. For a recording context, the metadata table is
at `+0x2B18`, selected metadata word count at `+0x10`, and the tagged CPU
constant pointer at `+0x710 + 8 * (buffer_id & 0xFFF)`. The low four pointer
bits are tags. Viewport floats are at context `+0x230/+0x234`.

Conditional inference: if this matrix reaches the rasterizer with the matching
viewport, the pixel displacement is `(stored_x / 2, -stored_y / 2)`.
Actual shader execution, raster viewport, GPU contents, and modern NGX jitter
units are not established by this CPU trace. Neither candidate table should
be passed to a modern provider on this evidence alone.

## Observation contract

`projection_snapshot.cpp` uses the existing producer's renderer, recording
context, main view, counter, epoch, command bank, and mode. It checks those
anchors before and after reading camera jitter, the 20-entry candidate,
viewport, and both CPU matrices. It also rechecks camera type/stamp, cached
IDs, metadata, tagged pointer and matrix layout. It copies scalar values only;
no foreign pointer is retained for later dereferencing.

The reader permits at most 192 reads of at most eight bytes. A complete owned
fixture uses 110 reads. Nonfinite values, invalid viewport, mismatched epochs,
overflow, invalid matrix shapes, overlapping ranges, or changed anchors reject
the observation. Checkpoints 1..4 identify progress; 100 means a valid snapshot.
Validity establishes stable CPU data, not GPU use. Matrix agreement is a
separate result, checked against the relation above with float rounding bounds.
A stable mismatch is preserved as evidence.

Projection observation is optional: unavailable data does not discard the
existing publication receipt or disable its already bounded diagnostic. The
outer producer guard still applies. The one-producer/one-Evaluate limits,
native TAA fallback, completion stop, and cleanup are unchanged.

Eight additional exact-code gates validate the setter, TAA path, view setup,
constant publication/allocation, multiply/copy helpers, and table constructor.
The publication routine already had a gate. Runtime preflight now compares
21 spans across host and bridge: 17 host spans totaling 13,642 bytes and four
bridge spans totaling 221 bytes. The existing hook ABI and ownership contracts
remain those in the linked publication/depth notes.

Trace version is `0.13.0`; mailbox and journal producer IDs are 1300.
Frame JSON schema 2 gains `producer.projection`; the binary mailbox schema 1
and journal schema 2 are unchanged. Both `shader_execution_verified` and
`raster_jitter_verified` remain false. The Python reviewer independently checks
matrix layout, anchors, finite raw bits, and the projection relation.

## Verification and private evidence

Private artifacts are under `local/temporal-inputs-20260923/` and remain
untracked. `projection-contract.json` records nine hashed spans and 33
instruction checks. Owned-memory replay of the original leaf machine code
passes 512 multiply cases, 64 matrix-copy cases, and 24 composed projection
cases; no game process was opened for those replays.

All **45 native tests** and **48 Python tests** pass. Reader tests cover each
of 110 failed-read positions and 55 changed rereads, layout/epoch/type failures,
perspective matrices, wrong signs and stable mismatches. The real hook fixture's
JSON also passes the independent Python projection review. A missing-projection
hook fixture still completes the existing one-item diagnostic. The previous
run 4768 replays with its original flags 0 and `gpu_safe=false`.

The final DLL passes CFG checks for 126 targets and 18 distinct provider
wrappers, helper assembly/unwind checks, and the real D3D12 completion fixture.
The accepted build logs contain no compiler warnings or errors.

## Deployment and next controlled run

The prepared v0.13.0 DLL is 770,560 bytes, SHA-256
`7F0F1DAA17C394D718D85F8A77BFAE160C09EDF3928BCF456083266E19A3530B`.
The staging script and resulting `staging-0130.json` record exact deployment
and archive the source, DLL, test detail and saved settings before replacement.
The destination is the existing MO2 `dlssTEST` mod, never the physical game
directory. Preserve the exact deployed v0.12.0 DLL as
`mhw_dlss_bridge_probe.v0.12.0.dll.disabled`, SHA-256
`CF3741D4446FD7F0CC334CFB78AA374CBFE45FB30E1D4BEA24FBEC23F8905043`.
Preserve the mode sidecar as `mhw_dlss_bridge_probe.mode.before-0130.disabled`;
its existing `native-depth-probe` content is unchanged. Older backups remain.

Saved settings remain DLSS Off, TAA, DX12 On, FullScreen, 2560x1440; SHA-256
`02BAB1311AD739420182D2FF5DB8A6ED9536235E80B51D2B736A6232633CB235`.
Mode SHA-256 is
`E94AC2A93344FD2069AA07C24FC58A005F59F0FCB68882C07B9C0CFD562154A6`.

1. With the game closed, run the checked local staging script and verify its
   manifest, deployment, backups and source archive.
2. Launch the same MO2 entry, **Monster Hunter: World - DLSS 1.2.110 test**,
   offline. Leave DLSS Off at the main menu and wait for read-only preflight.
   No MO2 restart or external recorder is needed.
3. After preflight, enable DLSS once, close its popup, and leave Options to
   apply. Stay at the main menu and reply from the phone; do not load a save
   or toggle again. No intentional terminal crash is part of this diagnostic.
4. Save both channels while the process stays open. Review projection validity,
   camera/table selection and matrix relation alongside the publication,
   Evaluate and completion evidence. Keep GPU/temporal qualification false.
5. Once evidence is saved, turn Off, leave Options to apply, then exit normally.
   If unavailable, leave the setting alone and exit. Verify cleanup and settings.

```powershell
python local/temporal-inputs-20260923/preflight_0130.py --pid <pid>
python tools/save_capture.py --pid <pid> --output-directory local/temporal-inputs-20260923
python tools/review_submission.py --receipt local/temporal-inputs-20260923/channels-<pid>.json --output local/temporal-inputs-20260923/review-<pid>.json
```

To roll back with the game closed, restore the exact v0.12.0 backup over the
active bridge DLL and verify its hash; the mode file need not change. To remove
the experiment, disable its MO2 Force Load entry and `dlssTEST` mod. Preserve
evidence and backups. GPU input readback, motion/depth conventions, actual
shader jitter and temporal image alignment remain subsequent work.
