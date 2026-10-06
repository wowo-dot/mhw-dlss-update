# FG scene observer: v0.6.4 runtime result

2026-10-05. Four requested CPU metadata captures passed their joined review:
main menu, loaded gameplay, open gameplay menu, and gameplay after an explicitly
confirmed area change through a loading screen. The user reported clean active
DLSS in gameplay and after the area change, then a normal scene and normal exit.
The diagnostic DLL remains installed. MHW frame generation remains disabled.

## Confirmed observer exclusion

All eight bounded reason-9 entry samples (two per capture) have complete vertex,
viewport, scissor, topology and index observations; matched root/raster recording
generations; and a successfully forwarded OMSetRenderTargets observation with
one readable RTV descriptor and one readable depth descriptor. The observer's
preserved output-known condition requires **one RTV and no depth descriptor
pointer**, so these samples fail its output condition. Missing raster fields do
not explain those eight samples.

The raw contiguous-descriptor BOOL is zero in every sampled case. Inspection of
the existing forwarding wrapper established that its `c` argument is the depth
descriptor pointer, not that BOOL. This run confirms the depth-pointer exclusion
in the samples; it does not establish a contiguous-range restriction.

| Requested sample | Plan counter | Final draw / Present counter | Qualified events | Reason-9 entries | Other unreadable entries | Outside executor scope |
|---|---:|---:|---:|---:|---:|---:|
| Main menu | 29423 | 29424 | 1 | 23 | 2 | 0 |
| Loaded save | 33056 | 33057 | 3 | 53 | 1 | 0 |
| Menu open | 37905 | 37906 | 1 | 59 | 0 | 0 |
| After area load | 39514 | 39515 | 1 | 107 | 17 | 47 |

There are 242 reason-9 entries in total, but only eight argument snapshots were
retained. The confirmed explanation applies to those eight; this is not proof
that every rejected draw has the same cause. Both indexed and nonindexed native
executor call sites appear in the samples. Unsupported native attachment modes,
resource-label read failures and calls outside the two qualified executor scopes
remain separate exclusions.

The raster-known mask is 31 (the five raster fields known, output unknown).
The native-cache read mask is 255: state, list, packet, attachment, active
attachment, flags, view and backend were read. Later native RTV/root/PSO fields
remain unread after this rejection. Their serialized zeros do not prove zero
game bindings. The sampled RTV handle is `1854486720`; the sampled depth handle
is `365537512`. These are process-local labels, not portable identities.

The diagnostic samples were taken at executor entry. They do **not** establish
successful draw return, GPU completion, HUD/UI semantics, descriptor-resource
ownership, or a safe rendering admission rule. No renderer gate was relaxed.

## Qualified scene and copy scope

Every capture contains the exact returned, replaced final DrawInstanced call,
linked by recording generation and same-queue submission to qualified Reflex
PresentStart. Its target label is `2509987008`, engine extent 2560x1440 and engine
format UINT32_MAX. The underlying DXGI format is not established by this field.

Only the loaded-gameplay sample contains an outgoing native CopyTextureRegion:
the final target transitions from state 4 to 2176, followed by an unreplaced,
returned copy to label `2492296416`. Both locations name subresource zero;
destination XYZ is zero and the source box is `[0,0,0,2560,1440,1]`. This is a
verified submitted call description, not verified resource extents or pixel
coverage. No copy was captured in the other three samples. Those absences do
not prove that no later copy occurs in the renderer.

Neither label matches the run's historical sampled GetBuffer interfaces. The
current indexed backbuffer, complete post-final draw inventory, HUDless color,
transparent UI, immutable FG input lifetime and producer fences remain unproven.
The v0.6.3 copy observations remain separately accepted and archived; they are
not substituted for missing v0.6.4 events.

## Verification and cleanup

The run completed 9,534 DLSS Performance frames and 11,009 complete Reflex On
frames, 66,054 markers and 64 complete driver reports, with zero runtime faults
or request collisions. Three skipped resolves remain in the trace; this does
not claim every frame was reconstructed. All 9,537 GPU signals completed and
all 9,536 submitted recordings retired/released, leaving zero live recordings
or retained resource references. All 10,551 jitter changes and full render scale
were restored. Reflex Off, SDK shutdown and pin release were confirmed.

Independent checks while the game remained open verified 37 native hook heads
and 46 full native functions restored (778 bounded VM_READ operations, 75,163
bytes, zero process memory writes). Routed/native presentations matched at
40,478. The user then confirmed normal exit. Graphics bytes and runtime/output
sidecars remain unchanged; the installed v0.6.4 DLL is preserved.

Before deployment, both the scene diagnostic and default combined probe built
with /W4 /WX. Ten native and 76 Python checks passed. The additional observer
metadata is compiled into the separate `mhw_fg_temporal_state` library linked
only by the scene diagnostic; the default probe keeps its existing observer.
The tested freeze and all prior archived evidence were rehashed unchanged after
the run. No new hooks, COM resource methods/references, pixel readbacks, GPU
commands, FG tags or Streamline FG plugin were added by this diagnostic change.
The existing NGX `nvngx_dlssg` mapping is not MHW FG activation. This run does not
measure performance or latency improvement.

## Evidence and next step

Private run: `local/frame-generation-scene-20261003/game-04`, PID 37012,
creation 134357035808482668. The 51-file `game-04-archive` manifest SHA-256 is
`09640cee104e23c10684d96edf3d663b010f76169b40162253b38f4b216bb604`.
The exact 688-file `source-0.6.4` manifest is
`a5943305109f13509741f700f4e2f8a2d3dddc5cc43c13a032cfb0028cc5025d`.
Installed DLL:
`d173d34338b5e7aaddf451eefac4a721f14a4a4f10ee49ad6815dd5727bf2fee`.
Retail executable version 0.0.0.0:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded private audit executable:
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.

Next, observe successful return and submission of depth-bearing single-RTV
draws through a separate diagnostic path. Preserve the existing reconstruction
admission checks. Capture native output/root/PSO agreement and ordered resource
edges before classifying any work as UI. Follow the copied texture through the
native presentation attachment to the current buffer. This supplies the missing
evidence for scene/UI separation; it does not yet authorize FG input tagging.
