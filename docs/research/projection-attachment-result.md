# Projection draw and depth attachment: accepted run 11804

2026-09-24. The read-only v0.3.1 capture links the known projection-buffer
binding and recorded indexed draw to a validated 2560x1440 depth attachment.
Canonical COM identity, owning device and resource description remain stable
through three checks. This completes the resource-description portion of the
[attachment experiment](../runtime/projection-attachment-plan.md).

## Version and evidence

- PID `11804`, creation `134346734018361364`.
- Host version `0.0.0.0`, SHA-256
  `C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
- Tested DLL SHA-256
  `B42BC8BF5F82600A3A80E9120CE28A5B5786BCF859AC9A939CAA7E30C8F69D1B`.
- Log SHA-256
  `35cb673b003b19c7d7436eff3de436fe0196647eb7b1ab7a4eb83e129d9e9317`.
- Local archive: `local/projection-binding-20260923/v0.3.1/`.
- Verdict: `accepted-readonly-draw-attachment-resource-description`.

The run used the bare MO2 profile with DLSS Off/TAA On and a user-confirmed
loaded save. All 39 live code spans passed and all 24 targets installed. The
recorder completed with zero faults, drops or pending calls; hooks disabled
and drained and retained resource references were released. It neither
injected jitter nor added GPU commands.

## Correlated source, binding and draw

Frame/epoch `15908` supplies 1,072 bytes of projection constants with native
jitter `[0, 0]`. Matrix checks pass. The source belongs to arena 2 while the
allocation index at consumption is 0; correlation follows the actual source
and GPU address rather than assuming the current allocation is its owner.

The selected native draw is kind 7, RVA `259D930`. On the same command list,
eleven returned API events include the output bind, viewport/scissor, pipeline
and root-signature binds, root arguments and the actual indexed draw:

| Observation | Recorded value |
| --- | --- |
| Color targets / depth targets | 0 / 1 |
| Viewport | `[0, 0, 2560, 1440, 0, 1]` |
| Scissor | `[0, 0, 2560, 1440]` |
| Projection root-CBV | Parameter 9, GPU address `12381849856` |
| Native compact root mapping | VS b0, layout index 1 |
| Indexed draw | `DrawIndexedInstanced(2520, 1, 2586, 0, 0)` |
| Local recorded barriers | 0 |

The original native draw and actual draw API returned; source bytes and native
root/shader metadata stayed stable. Both captured shader containers match
[run 46324](projection-jitter-binding-result.md). The known VS contract uses
`fViewProj` at b0; the captured PS has no color output. The selected PSO came
from the packet cache. Its bind was observed, but its creation and executing
bytecode identity were not established.

## Validated resource and native depth metadata

The attachment-owner/backend-view chain correlates with the freshly bound
depth handle. The queried `ID3D12Resource::GetDesc` returns:

| Field | Value |
| --- | --- |
| Dimension | `TEXTURE2D` |
| Format | `R32_TYPELESS` (39) |
| Width / height | 2560 / 1440 |
| Array size / mip levels | 1 / 1 |
| Sample count / quality | 1 / 0 |
| Layout | `UNKNOWN` (0) |
| Flags | `ALLOW_DEPTH_STENCIL` (2) |
| Alignment | 65536 |

After attachment setup, immediately before the draw API and at native draw
return, the full resource/device/identity/description check succeeds. Each
phase has six `S_OK` results, checkpoint 15, valid description and released
temporary interfaces. The resource interface is `359053552`; canonical
`IUnknown` is `359053504`. Repeated canonical queries agree and the resource
roundtrip returns the exact original interface. The observed -48 difference
is evidence for this run, not a hard-coded adjustment or a layout contract.
All three phases return the same expected device (`356914720`) and description.

This validates the [v0.3.1 identity correction](../runtime/projection-attachment-identity.md)
against the game. The original [v0.3.0 run 5208](projection-attachment-first-run.md)
remains rejected; its missing query checkpoints are not reconstructed.

Native pipeline metadata has `DepthEnable=1`, `DepthWriteMask=1`,
`DepthFunc=7` (`GREATER_EQUAL`) and stencil disabled. Native format conversion
maps 39 to `D32_FLOAT` (40). Together these qualify a depth-write candidate.
They do not independently prove the actual DSV creation descriptor, the cached
PSO's creation descriptor or completed GPU depth writes.

## Remaining qualification

The incoming resource state is unknown. No `ResourceBarrier` occurred within
this selected draw's scope; transitions may precede it. Absence of a local
barrier does not establish `COMMON`, `DEPTH_WRITE` or any other state.
Likewise, the single mip and array slice constrain the resource but do not
directly record the view's subresource selection or read-only flags.

The next offline work is to map actual depth-view creation/descriptor
provenance and establish bounded earlier barrier history tied to command-list
generation. Any later GPU readback must additionally establish the relevant
submission and completion before CPU inspection. Cached PSO creation,
GPU depth contents and controlled raster displacement remain separate gates.
No motion/history alignment or working DLAA/DLSS is claimed by this capture.
An unchanged rerun is unnecessary.

## Verification, archive and rollback

Preparation passed Release `/W4 /WX`, 76 native tests and 117 Python tests
(34 binding, 25 stimulus, 58 attachment). A real owned D3D12 texture also
passed the production validator before deployment. Existing accepted reviews
replayed unchanged; rejected historical runs remained rejected.

`accept_11804.py` independently checks the capture, archived-source reviewer
replay, shader/source hashes and rollback. `acceptance-11804.json` retains its
accepted verdict; `archive-11804.json` seals the evidence and verifies all 42
archived source files. Later documentation changes do not alter those sources.

The user exited normally. Restoration completed at
`2026-09-24T00:47:23.2162615+03:00`, returning the exact previous v0.15 DLL with
SHA-256 `689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
The added output sidecar was removed. Graphics settings, bare-profile modlist,
profile settings and diagnostic mode all retain their original hashes.
`restored.json` records the destination and checks. No new runtime deployment
or launch is pending.
