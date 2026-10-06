# Scoped projection-consumer experiment

Updated 2026-09-23: run 17072 accepts a three-call CPU baseline/stimulus/recovery
experiment using v1.0.2. The game exited normally, the previous DLL is restored,
and the output sidecar is removed. [Track run 42104](jitter-track-writer-result.md)
captured shared one-key, raw-zero data driving two TAA2 jitter-enable writes.
The resource file and active raster owner remain unidentified. Repeating the
same writer observation is not needed to test the downstream projection path.

The bounded test supplies one native nonzero sample directly while the
projection consumer executes, restoring the camera field immediately afterward.
This isolates whether the mapped CPU projection path applies the expected
offset. It does not force the scheduler value or establish that the original
TAA callback has been activated. The [runtime contract](../runtime/scoped-camera-jitter.md)
contains the ABI, guards, lifetime assumptions, limits, rollback and protocol.

Confirmed static path: camera virtual slot `+0x48` resolves to `0x228E770` for
both accepted types. Its eight-entry loop passes `camera+0x50+index*0x1A0` to
`0x228F3B0`; that helper reads jitter from the separate global camera object.
The probe selects entry zero/display zero, verifies the exact caller, and
observes baseline, stimulus and recovery on increasing CPU frames/epochs.
The complete function, its caller, six projection/sample dependencies, the
provider-selection block and both virtual slots are pinned. The offline map
checks 23 instructions and confirms no call inside the relocated hook prefix.

The accepted result changes CPU constants by the mapped jitter/viewport
relation and recovers to a zero-jitter relation. It leaves GPU/raster consumption, motion vectors,
history handling, stable sustained jitter and modern reconstruction unresolved.
It is a one-call experiment, not a working DLAA implementation. Its field
restoration cannot rewind any image/history affected by that call.

The subsequent [upload/binding map](projection-upload-binding.md) now follows
these constants through native publication, three-arena transfer, D3D12
binding and draw entry points, and locates graphics shader bytecode storage.
It selects a read-only correlation capture; it does not upgrade this CPU result
to GPU or raster proof. No follow-up observer has been deployed.

## Run 17072: CPU stimulus and recovery accepted

Read-only preflight verifies the v1.0.2 DLL, 15 code spans, both virtual slots,
135 loaded modules and unchanged saved Off/TAA/profile settings. After the user
reported a loaded save, the single arm installed/enabled the hook with `MH_OK`.
The DLL's nine code gates passed. All three selected calls returned true to
`0x228E834` on one thread, using the same derived camera and embedded entry zero.

| Stage | CPU frame / epoch | Bank | Stored jitter during call | Projection result |
| --- | ---: | ---: | --- | --- |
| Baseline | 13406 | 0 | `[0, 0]` | Jittered/plain matrices bit-identical |
| Stimulus | 13407 | 1 | `[0.34375, -0.48765432834625244]` | Expected nonzero relation |
| Recovery | 13408 | 0 | `[0, 0]` | Jittered/plain matrices bit-identical |

All admission checkpoints are 100; all projection snapshots are valid at
checkpoint 100 with 110 scalar reads. Viewport is 2560x1440. The stimulus sample
is the exact current 20-entry table value, index `13407 % 20 = 7`. The existing
268-word constant block contains projection at word offset 32 and unjittered
projection at offset 200, using buffer slot 11. Its CPU storage address changes
across frames; these addresses are not assumed to be GPU addresses.

Only matrix indices 8 and 9 differ in the stimulus, by
`-0.00013427734666038305` and `+0.0003386488533578813`. Independent float review
checks all 16 coefficients against the mapped jitter/viewport relation; maximum
absolute residual is about `1.43e-11`. Baseline and recovery are bit-identical
to their unjittered counterparts. Before/after provider snapshots remain equal
throughout: raw `mUseDLSS=1`, sizing flags zero, nonnull backend, absent wrapper.

The terminal records exactly three visits/records, one injection, one successful
restoration, zero drops and zero pending calls. Each call exits with zero camera
jitter. It stops as complete, without fatal error, and disables its hook. The
user reports normal exit; process absence was required for rollback. The exact
previous v0.15 DLL is restored and the new sidecar removed. Four configuration
files retain their original hashes. No additional rendering/provider experiment
was installed after the run.

The original binary, 16 source copies, preflight, arm receipt, capture,
independent review and restoration are retained under ignored
`local/jitter-activation-20260923/provider-gate-1.0.2/`. The archived reviewer
replays successfully through `accept_17072.py`; `acceptance-17072.json` records
CPU acceptance with GPU qualification false. The unchanged build's 16 native
and 20 Python checks apply. Capture SHA-256:
`9FA0D43AC1E6F7366EBFE9C29D554E0C6A580E80B21109FD630DA35EAF582521`.

This proves the sampled native CPU projection code responds to one supplied
nonzero jitter pair, with immediate camera-field restoration and subsequent
zero-jitter recovery. It does not establish that the rendered image used those
constants, validate motion vectors, enable the original TAA jitter callback,
undo temporal history, or demonstrate sustained jitter or modern DLAA/DLSS.

Next: map this constant slot's upload and binding path, retaining source
frame/epoch and buffer identity. A GPU address or binding alone will not prove
shader consumption; inspect the consuming shader/transform and raster behavior
before treating the sample as a valid temporal input. Motion-vector sign/scale
and frame alignment remain a later validation gate. No unchanged CPU rerun or
new launch is needed yet.

## Run 25448: installation failed, no stimulus

Read-only preflight matched the loaded v1.0.0 DLL, 14 code spans, both camera
virtual slots, 135 loaded modules and unchanged Off/TAA/profile settings. The
user reported a loaded save before the one arm request. The DLL passed its
identity/code gates but returned `hook-install` before `armed`. It retained
zero visits, records, injections and restorations, with no pending call.
The terminal reported fatal capture failure, successful hook disabling and
pinned trampoline storage. This is no evidence about nonzero jitter behavior.

The user exited normally. Process absence was required to restore the exact
previous v0.15 DLL and remove the output sidecar. Four configurations remain
unchanged. Archive verification replays the original reviewer/source copies;
`acceptance-25448.json` accepts the failed run's integrity, not the experiment.
Capture SHA-256:
`ED5FB3C7772C06BB0020ED3572D94EED9FE52323AFE20EB531E6E6C52621AFE6`.

Version 1.0.0 discarded the failing MinHook operation/status, so the precise
cause cannot be recovered from its log. The v1.0.1 follow-up records those
details, retains them during review and uses an owned fixture with the same
stack-prologue shape. Hook installation and exception restoration pass there;
that narrows the concern but does not explain the live failure. There is no
automatic retry, expanded jitter budget or claimed fix to an identified cause.

That fixture/reviewer had 29 passing cases. The original capture,
v1.0.0 binary/sources and rollback receipts remain under ignored
`local/jitter-activation-20260923/`; the new build, sources and deployment are
separate under `install-diagnostics-1.0.1/`.

## Run 5152: incorrect raw-switch exclusion

Version 1.0.1 passed read-only preflight (14 code spans, both virtual slots,
135 modules) and installed/enabled the hook with `MH_OK`. After the user
reported a loaded save, it captured one baseline call returning true to
`0x228E834`, with projection entry `camera+0x50`, frame/epoch 14903 and derived
camera vtable `0x33F82A8`. Admission rejection 1 prevented matrix capture and
all jitter injection. One visit, zero drops and no pending call were recorded;
the hook disabled cleanly. This does not resolve the prior installation failure.

Three later bounded read-only snapshots observed the same camera/renderer,
active entry zero, zero camera jitter, current stamp, legal view/bank/mode and
finite native candidate samples. Raw `mUseDLSS` was 1 in all three. A separate
three-snapshot check found sizing flags zero and an absent wrapper with a
stable nonnull backend. Saved settings remained Off/TAA. The older bare-profile
run 2632 had also observed `mUseDLSS=1` with no wrapper; the
[earlier renderer analysis](taa-activation-depth-crash.md) explicitly cautions
against equating this byte with the menu setting or a live handle.

The new probe had incorrectly added `mUseDLSS == 0` as an exclusion condition.
That is a confirmed implementation error for the observed Off/TAA state. The
rejected call did not retain the raw byte when admission failed, so later
snapshots cannot prove that exact call's failing field. In particular, its
serialized `projection_active=0` and `use_dlss=0` are default placeholders from
fields assigned only after full admission, not observations that they were off.

The game exited normally; the previous DLL was restored, the output sidecar
removed, and four unchanged configurations verified. Original sources/reviewer
and binary are retained. `install-diagnostics-1.0.1/acceptance-5152.json`
verifies the rejected run's integrity. Capture SHA-256:
`8F954BA397275CB37C0937A3EEC5C8BFF6759FF73A60982E86AD6B5AD0BFC33C`.

Version 1.0.2 retains the raw switch but requires sizing bit 0 clear and no NGX
wrapper, with a nonnull backend and unchanged provider snapshots before/after
the original call. A ninth live code gate pins the actual provider selector
at `0x2392A18..0x2392A56`; seven added instruction checks confirm its renderer
switch, sizing, backend and wrapper checks. The original one-stimulus budget
and restoration are unchanged. Admission checkpoints and partial values now
survive rejection instead of becoming uninformative zero defaults.

The corrected build passes 16 native and 20 Python cases, including the raw
switch true with no wrapper, sizing/wrapper exclusion, and restoration after a
wrapper appears during the stimulus. It is installed from the separate ignored
`provider-gate-1.0.2/` directory. The same Off/TAA menu-then-save protocol applies;
GPU/raster, motion and nonzero projection behavior remain unqualified.
