# Depth pixels paired with ordinary TAA inputs

Captured and closed 2026-09-25, recorder **1.9.0**:
[accepted run 25300](../research/ordinary-taa-depth-pixels-result.md). All three
depth/color/motion sets verify; finite depth and zero-offset edge agreement
support spatial correspondence in the stationary scene. Temporal/subpixel
calibration remains pending. Previous DLL restored; configurations unchanged;
no next test installed or armed. The same recorder can serve a controlled pan.

This extends
[run 45404](../research/ordinary-taa-depth-routes-result.md) with three bounded
depth readbacks at the matched native copy, alongside existing color/motion.

## Hook and ownership contract

The ten hooks, Windows x64 calling conventions, native forwarding and 51 live
code spans are unchanged from [depth-copy routes](temporal-depth-routes.md).
Tested host 0.0.0.0 SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded-image SHA-256:
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Preparation checks both identities; runtime gates the host and native code.

Three readback allocations are prepared on the worker before arming. The CPU
TAA producer publishes each selected frame/stamp/packet with release/acquire
synchronization. Following a returned native CopyResource at image+259E100,
the copy callback requires this exact selected frame, view, candidate owner
and native resource. Observed native barriers must put the original source in
pure COPY_SOURCE (0x800) and destination in COPY_DEST (0x400), distinguishing
the later copy from the two earlier 0x840-source copies seen in run 45404.
Missing/split/subresource/alias/conflicting state evidence cannot qualify it.

The destination must match full-resolution R32_FLOAT/ALLOW_RENDER_TARGET,
single mip/slice/sample, the same verified D3D12 device and canonical resource
identity. The existing readback implementation records COPY_DEST -> COPY_SOURCE,
one CopyTextureRegion into the private buffer, and COPY_SOURCE -> COPY_DEST.
Observer barriers use the original API trampoline, preserving the distinction
between native barrier evidence and observer commands. No resource binding,
descriptor or shader change is made.

Only one copy attempt is allowed for each selected frame. A second eligible
copy rejects the capture; it cannot overwrite the buffer or issue another
readback. Native forwarding continues on every diagnostic rejection. Once any
observer command is attempted, the buffer/source/list/allocator/device references
remain held for process lifetime. A returned transition is restored even if the
copy faults. Uncertain work is quarantined, not retried or prematurely released.

## Submission and save requirements

The readback has its own begin/end witnesses inside the native-copy journal
marker. Later TAA Dispatch must match the exact frame, packet, command pointer,
allocator and Reset generation. This check is performed at runtime before the
TAA marker and repeated independently by the reviewer. It does not assume that
the source-state filter alone establishes the correct command list.

The same selected TAA generation supplies Close, Execute, private Signal,
completed fence and later Reset witnesses. No additional queue submission or
Signal is introduced. Depth buffers may be mapped/saved only after completion,
recording retirement and hook drain qualify all three images. The reviewer
checks the native marker, observer order, restored states, footprint/padding,
unique allocations, exact filenames and SHA-256 hashes.

Version 1.9 explicitly scopes generation acceptance to selected frames. Extra
tail copies may remain unqualified only under the tested tail-prefix rules:
later frame and later marker, observed initial Reset, no Execute or later Reset,
at most one valid Close and no overlap. Selected-frame lifetimes remain strict.
The original v1.8 full-review rejection remains unchanged in its closed archive.

## Bounds and verification

Ten hooks, two-second window, 150 ms warmup, three selected consecutive frames,
64 native-copy requests/consumers, eight barrier calls / 32 barriers per copy.
Existing submission bounds remain 4096 events and 8192 submitted pointers.
Nine images maximum: three each of depth/color/motion, at most 16 MiB per
allocation and **144 MiB total**. **27 observer GPU commands**, three existing
private Signals, zero observer Execute calls, projection writes or SDK calls.

115 native checks pass with warnings as errors. Seven new owned WARP cases
cover exact depth pixels, repeated native copies, wrong copy-point state,
duplicate eligible copy, missing selection, missing retirement and map failure.
The depth fixture clears real D32 source textures to distinct known values,
copies them natively to the R32_FLOAT candidate, and verifies every saved pixel
and removed row padding. Pure state-contract tests reject incomplete/split/alias
and earlier-copy intervals. The Python suite passes 55 tests (nine jitter-only
skips), including frame/generation/order, allocation alias, restored-state,
artifact-integrity and budget damage. Historical evidence is checked during
preparation without modifying prior archives.

An accepted capture verifies depth readback and triplet association only.
Depth/color content alignment, complete writer coverage, typed input views,
SDK evaluation and sustained DLAA remain separate pending checks.

## Local procedure and rollback

Evidence: `local/temporal-depth-pixels-20260925/v1.9.0/`. Preparation verifies
the closed 196-file predecessor archive and preserves the exact MO2 slot DLL,
output-sidecar state and four configurations. Source/reviewer files and the
tested DLL are frozen before the prelaunch seal. Deployment receipts establish
the actual installation status; nothing is armed during preparation.

Use the existing bare-profile MO2 test entry, DLSS Off, TAA On, DX12 On and the
current resolution. Load the offline/private save and hold the camera still;
posing is optional. Reply from the phone when loaded. Arm only after matching
the ready log to the current process generation and sealed DLL/configuration.
After capture, exit normally with DLSS Off. With the game closed,
`deploy.ps1 -Action Restore` restores the exact previous DLL and sidecar state.
Original game assets and settings remain unchanged.
