# Bounded live owned GPU inputs — v1.10.0

Accepted and closed 2026-09-25:
[run 51256](../research/live-owned-temporal-inputs-result.md).
All three live GPU sets and 77,414,400 converted float values verify.
Previous DLL restored, sidecar removed, configurations unchanged.
This extends the accepted v1.9.0
depth/color/motion capture with direct GPU copies into private textures and
private-list typed conversion. It does not replace TAA or call an SDK.

## Native recording contract

The same ten Windows x64 hooks and 51 executable code gates apply; see
[depth pixels](temporal-depth-pixels.md) and
[depth routes](temporal-depth-routes.md) for target functions and calling
conventions. Host version 0.0.0.0, SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded image SHA-256:
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.

The worker prepares three independent sets on the discovered game device:
color R11G11B10_FLOAT, depth R32_FLOAT, motion R32_UINT. Each input is a private
DEFAULT texture with no resource flags, one mip/slice/sample, initially
COPY_DEST. Each set has its own queue, allocator, list, fence, descriptor heap,
root signature, conversion PSO, outputs and readback buffers. Creation and
shader compilation finish before observation begins. Game debug settings stay
unchanged.

For each admitted native source, the existing readback interval now records:
source transition to COPY_SOURCE; original CPU-reference readback copy; direct
CopyTextureRegion into its private GPU texture; original-state restoration.
No allocation, descriptor binding or conversion occurs inside the hook.
The original depth interval is after returned native CopyResource at
image+259E100 (candidate COPY_DEST 0x400). Color and motion are just before
original TAA Dispatch at image+259CFCA (states 0x40 and 0xC0 respectively).
The native dispatch always forwards. Source state restoration uses the
existing SEH finally path.

The private copy shares the raw copy's exact Site interval. Its receipt
records destination identity, format, flags, subresource, state and returned
copy status. Both copies must return; otherwise the set is unusable. These are
two commands within one witnessed interval, not independent journal events.
Existing selected-frame, packet, list, allocator, Reset, Close, Execute,
private producer Signal, completion and recording-retirement checks still
apply. Source/device/canonical-identity checks remain in place. Raw and private
input references are retained until process exit after any attempted native
transition, including uncertain failures.

## Private conversion

After hooks are disabled and drained, all nine raw images must qualify and
save successfully. Only then does the worker submit three one-shot conversions.
Completed producer fences establish the dependency before private Execute;
the worker does not assume frame elapsed time implies GPU completion.

Private inputs transition COPY_DEST to NON_PIXEL_SHADER_RESOURCE on their
private lists. Explicit SRVs use default component mapping (5768), formats
26/41/42. The previously tested compute shader outputs float32 RGBA color
(alpha 1), unchanged R32 depth and RG32 motion:
`previous_pixel = current_pixel + (-half(high16)*width/2,
half(low16 & 0xfffe)*height/2)`. This is the measured history-offset convention,
not a claim that any vendor SDK accepts that scale or those output formats.

Each output transitions UAV to COPY_SOURCE and copies to a private readback.
The private queue signals its fence; the worker waits up to 15 seconds and
checks device removal, then resets the completed allocator/list before mapping.
An uncertain submitted failure retains the whole context until process exit.
These lists never use native render targets or native descriptor heaps.

Budgets: three selected frames, two-second observation, 150 ms warmup,
36 added native-list commands (18 transitions, nine raw copies, nine private
copies), three existing producer Signals. Separately, private conversion uses
three Executes, three Signals, three Dispatches, nine copies and 18 transitions.
There are 18 private descriptor writes and zero native descriptor writes.
Each owned set is capped at 300 MiB of image allocations, including GPU textures
and readback buffers; raw readbacks retain their separate 144 MiB cap. Converted
pixel files are capped at 336 MiB total. At 2560x1440 the nine converted files
total 309,657,600 bytes; nine original files add 132,710,400 bytes.

## Verification and use

Seven new native fixture cases cover success, multiple native depth copies,
wrong state, duplicate selected depth copy, missing selection, missing
retirement and raw readback failure. Positive fixtures run with the D3D12
debug layer and GPU validation. They check every converted float, forwarding,
restored source states and ownership. They exclude only the exact expected
three/nine clear-value performance warnings from the fixture's varying
ClearDepthStencilView calls; all other warnings/errors fail.
The production system-module gate is unchanged. A compile-time fixture-only
branch permits the System32 D3D12SDKLayers vtables used by those tests.

The independent Python reviewer checks all original v1.9.0 evidence, private
object identities, formats, recipe/budgets and every converted value using
NumPy. Depth must also match the source file byte for byte. Tests reject
unqualified state, private aliases, receipt corruption, budget changes and
incorrect pixels even after their artifact hashes have been recomputed.
Accepted fixture evidence does not claim game validation or DLAA readiness.

Use the existing barebonesdlss profile and injected test entry, DX12 On,
native DLSS Off, TAA On, fullscreen 2560x1440. Load one save and stay still.
No camera pan or option toggle is required. Respond from the phone when
loaded; the agent checks the exact process/DLL and arms the named event.
After the capture is reviewed, leave DLSS Off and exit normally.

Private preparation/deployment/rollback:
`local/temporal-owned-live-20260925/v1.10.0/`.
`deploy.ps1 -Action Restore` requires the game closed, verifies the installed
DLL/sidecar and backup, restores the exact previous destination DLL and removes
the temporary sidecar (or restores its prior contents). No original game asset,
executable, profile or graphics setting is changed.
