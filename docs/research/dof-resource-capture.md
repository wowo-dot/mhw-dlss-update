# Gallery compute resource observation (v1.25.37)

The [offline coordinate replay](dof-coordinate-replay-result.md) passes both
captured parameter sets on NVIDIA and WARP. Before integrating that adapter,
this diagnostic establishes actual resource descriptions and observations of
resource states at the native six-pass compute chain. Rendering admission is
unchanged: Gallery can still suspend DLSS and recover afterward.

## Scope and ownership

Keep the two paired CPU bundles from v1.25.36. Immediately after each complete
capture, inspect up to sixteen distinct texture resources referenced by its
six compute passes. Validate the native owner chain, system-method provenance,
QueryInterface identity and device identity; acquire a COM reference before
GetDesc and recheck the original owner and complete compute recipe. Record
dimension, alignment, width, height, array/depth size, mip count, DXGI format,
sample count/quality, layout, resource flags and format plane count. Never
interpret the engine's `UINT_MAX` GPU-format field as a real DXGI format.

The prepared native queue owns its bindings during this synchronous inspection.
Our extra references remain until callbacks have drained and hooks are disabled.
There are at most thirty-two retained resources. This capture introduces no
GPU resource, barrier, copy, dispatch or submission. It does not read pixels or
independently recover SRV/UAV descriptor contents. Resource format alone does
not prove the view format or valid image footprint.

## Recording observations

Reuse the existing code-gated native compute executor hook at RVA `0x259cc20`
(1283 bytes, SHA-256
`6921c7cbfc7b277f4b076a68de8165bd54b3f6571ca574fdb16d5c35cbe154da`).
The Windows x64 entry supplies executor in RCX, backend context in RDX and
native packet in R8. Its Dispatch API call returns to RVA `0x259cfca`.
The tested executable remains SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
No new hook target or patch is installed.

For each captured packet, observe only its first matching execution within
two renderer-frame increments of preparation. Match packet fields, shader
bytes, typed constants, owners and CPU descriptor identities again at native
entry and at Dispatch. Record the command list, successful Reset generation,
native recording context, actual dispatch dimensions and API-observed compute
root/PSO. The native recording's current packet must still match. Capture
whether the native executor returns and require one Dispatch for a complete
observation. A mismatch stops that diagnostic observation without changing
the renderer's decision or forwarding behavior.

The existing barrier observer records at most 256 events for the retained
textures while captured passes remain pending. Knowledge belongs to one
command-list Reset generation. Complete transitions can establish state;
split transitions, aliasing and contradictions make it unknown. A subresource-0
transition counts as whole-resource evidence only for a single-mip,
single-array-slice, single-plane texture; ALL_SUBRESOURCES is recorded directly.
No cross-list state inheritance or implicit state inference is introduced.
Capacity loss stops state knowledge instead of fabricating a value.

API-time state snapshots are indexed by each captured binding word and resource
identity. They are CPU observations of native recording, not proof of GPU
completion. Missing state is a useful result and remains explicitly unknown.
Between captures and after their passes finish, this observer does no per-pass
inspection. The independent reconstruction and cleanup checks remain required.

## Evidence and test

The additive `scaled_route.compute_resources` channel has schema 1 and policy
`native-compute-resource-observation-v1`. The paired CPU bundle remains schema
3. The new reviewer checks associations, bounds, immutable descriptions/events,
completed-pass immutability and release of diagnostic references at stop.
Partial diagnostic samples may be retained; they do not constitute success
for live integration. `pixel_footprint_verified` and
`descriptor_contents_verified` stay false.

Use the same solo/offline profile, native AA/DLSS Off, windowed 2560x1440,
no resize: main-menu baseline -> loaded save -> affected Gallery cutscene ->
recovery. Require two complete samples with resource descriptions, all twelve
native dispatch observations, no diagnostic errors/lost events, and inspect
known versus unknown states per binding. After intentional stop, verify native
code restoration, GPU completion and all diagnostic references released.

Evidence/candidate root: `local/dof-resource-capture-20261001`. This is a fresh
root; the previous `local/dof-pipeline-20261001` archive remains sealed. The
exact previous DLL and sidecar are backed up on deployment. Keep the new DLL
installed after normal exit. Explicit rollback, only with the game closed,
is `deploy.ps1 -Action Restore`; its restored sidecar points to a sealed root
and must be replaced with a fresh output path before another launch.

Next use the actual formats/states to determine the necessary pixel readback
and live recording contract. In particular, the offline experiment proved
that depth outside the active half-sized view can influence native reductions;
this metadata-only capture does not settle those pixel contents. Camp remains
deferred; monster effects and hunter silhouette follow Gallery.
