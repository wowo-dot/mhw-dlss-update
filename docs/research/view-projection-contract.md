# View-projection matrices and native root layouts

Accepted 2026-09-23 in run 31912 after [run 6444](projection-binding-result.md).
Recorder v0.1.3 is read-only, with the same twenty hook targets and two-second,
one-draw limit. It closes the live zero-jitter matrix and native root-selection
gaps from that accepted run; it adds no jitter, provider calls or GPU commands.

## Pinned static evidence

Executable version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`;
decoded reference SHA-256
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
Private evidence lives in `local/projection-binding-20260923/view-contract/`.
`verify_view_contract.py` checks both identities, 37 selected instructions,
seven additional code spans, and both D3D12 serializer delay-import names.
`verified-v2.json` supersedes the first exploratory report, whose converter
entry covered only its first unwind fragment. The complete converter extent
is `2624A60..2624D3E`, including its final serializer tail jump.

The additional live gates cover root builders `2625100` (normal), `2626030`
(compact), `2626E40` (VS-only), creator `2627200`, that complete converter,
and the two serializer import stubs. Together with the previous 29 gates,
v0.1.3 checks 36 spans before arming. These are code-validation gates, not
additional hooks. The existing x64 forwarding, exception, ownership and
trampoline-retention contract remains in the runtime protocol.

`replay_root_builders.py` interprets the builders' bounded assignments,
branches and stack clearing into private Python memory; it executes no native
game code and opens no game process. Twelve combinations of three builders,
buffer mode and descriptor mode pass. Native compilation checks the Windows
SDK x64 root-parameter sizes and field offsets used by this interpretation.

## Matrix relationship

With row-vector notation, original projection P, view V, jitter translation J,
and the mapped depth conversion D, the producer builds:

```text
fProj              = (P * J) * D
fNoJitterProj      = P * D
fViewProj          = (V * (P * J)) * D
fNoJitterViewProj  = (V * P) * D
J[12] = stored_jitter_x / viewport_width
J[13] = stored_jitter_y / viewport_height
```

`23D0E70` computes the view/projection products before the depth conversion,
then copies them using registry IDs. `fViewProj`, `fView`, and
`fNoJitterViewProj` IDs are globals `4BE75F8`, `4BE75FC`, `4BE7640`.
They are decoded and range-checked at runtime; observed offsets are not blindly
substituted. All five matrix ranges, including the two existing projection
matrices, must fit the same owned constant-buffer copy without overlap.

The new snapshot retains those three matrices from the already captured
bounded source bytes, verifies finite elements, and checks IDs again. The IDs
and all source bytes remain stable through publication, consumption and draw.
Independent review checks VP against V multiplied by the retained projection,
allowing float32 reassociation/depth-conversion roundoff scaled to dot-product
magnitudes. It also checks the expected homogeneous-coordinate jitter relation.
Zero jitter requires numerical equality with the unjittered VP. Pure review
tests exercise 128 asymmetric projections with varied camera rotations,
translations and nonzero jitter; transposed, nonfinite, overlapping or changed
inputs reject. This does not substitute for a live nonzero-jitter capture.

## Root selection and register mapping

State setup selects one of five backend root-signature pointers:

| Condition, in priority order | Backend offset | Variant | VS root CBV for b0 |
|---|---|---|---|
| Hull stage present | `298` | Tessellation | Not qualified |
| Geometry stage present | `290` | Geometry | Not qualified |
| Shader-set flags `+10` and `+1C` both contain `10000` | `280` | Compact | 9 |
| Otherwise, shader-set word `+14` is nonzero | `278` | Normal | 18 |
| Otherwise | `288` | VS-only | 2 |

For the mapped three variants, VS CBVs use space zero and increase the root
parameter by the shader register. Descriptor-table mode is retained as partial
evidence and is not qualified by this mapping. The recipe object at backend
`+270` retains the device and flags used by the builders. The capture checks
that device against the engine device and retains the five signature pointers,
selection fields, and recipe flags. It rechecks them after native state setup
and after the draw; the selected pointer must match the native root cache and
the command's binding mode. Any changed observation invalidates this capture.

Independent review tracks actual root-signature calls and invalidation through
the draw. It never promotes the cached pointer alone to an observed API call.
For the exact VS hash from run 6444, whose local disassembly established b0 and
`fViewProj` at bytes 0–63, a fresh matching signature/binding sequence can qualify
`native_metadata_vs_b0_binding_correlated`. Other shaders remain captured for
offline inspection and do not inherit that shader-specific claim. Geometry,
tessellation, descriptor routes, or missing fresh root-signature calls also
remain partial rather than acquiring an unsupported register mapping.

The serialized signature and executing PSO's bytecode are not independently
captured. This result correlates verified native construction/selection with
recorded API arguments and native shader metadata; it does not claim independent
PSO-bytecode identity, GPU constant contents, submission, shader execution,
raster displacement, or working temporal reconstruction.

## Verification

Release `/W4 /WX` builds cleanly. All 24 native and 34 Python tests pass,
including the 128 numerical cases above; the separate twelve-case root-builder
replay also passes. The updated reviewer reproduces run 6444's old verdict
without changing its scope. Archive `local/projection-binding-20260923/v0.1.3/`
holds exact sources, DLL, preparation, deployment and rollback evidence.

## Run 31912: zero-jitter contract accepted

Process creation identity `134346641599084941`; recorder SHA-256
`FE128312B414BDAAFADFC6EA0E86C150648C3668CCAD102FC509E300C5AB8C34`.
JSONL SHA-256
`ea8a010dabc2ae3ff8d3fae7c408f8be1a7dea755b989ab7f455a3d56c2cfd57`.
The user reported a loaded save with DLSS left Off. Startup reservation
succeeded, all 36 live code spans passed, and twenty targets installed.
The source-to-draw capture completed with 16 admitted visits, nine API events,
zero faults/drops/pending calls, and clean hook disabling/drain.

Frame/epoch 15057 supplies 1,072 bytes in registry buffer slot 11. Decoded word
offsets are VP 0, view 16, unjittered VP 216, projection 32 and unjittered
projection 200. Both projection products pass the existing float32-scaled
review bound without changing its tolerance; maximum absolute residual is
`0.00021416821982711554`. Stored jitter is `[0, 0]`, and VP is bit-identical
to unjittered VP. This confirms the baseline product relationship; it does not
exercise the nonzero-jitter response in the game.

The constants at CPU address `1226180864` retain identical bytes through
publication, consumption and draw. They belong to arena 0 at offset 264448,
GPU address `12348295424`; allocation has advanced to arena 1 at consumption.
The resource is shared between CPU/GPU roles in mode 2, with three 16 MiB
arenas. This again demonstrates why the current allocation index cannot stand
in for the consumed arena.

Stable selection metadata reports compact variant 1, VS flags `100FF`, PS
flags `1FFFF`, recipe flags `[1, 1, 0]`, and matching engine/recipe device.
The selected signature pointer `357075776` matches both the native cache and
an actual `SetGraphicsRootSignature` call. The source GPU address is then bound
at root parameter 9 before `DrawIndexedInstanced(2520, 1, 2586, 0, 0)`.
The pinned construction route maps this parameter to VS b0 in space zero.

Both retained shader containers are byte-identical to run 6444, so its existing
private disassembly applies: the candidate VS reads `fViewProj` at CB b0 bytes
0–63. Independent review accepts `native_metadata_vs_b0_binding_correlated`,
along with the matrix and root-selection checks. This is stronger than inferring
b0 from the number 9 alone, but the executing PSO's bytecode and serialized root
blob remain independently unobserved. GPU contents, submission, shader execution
and raster jitter remain unqualified.

The user exited normally. The exact previous v0.15 slot DLL was restored
(`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`),
the temporary output sidecar was removed, and all four graphics/profile/mode
files retain their pre-run hashes. `accept_31912.py` replays the archived
reviewer, shader identity, 27 source hashes and rollback checks into
`acceptance-31912.json`; `archive-31912.json` retains evidence hashes.

Next, combine the validated scoped nonzero camera stimulus with this matched
binding capture. Require baseline/stimulus/recovery evidence, immediate camera
field restoration, the expected homogeneous-coordinate change in `fViewProj`,
and the unchanged publication/binding identity through the selected draw.
Use each frame's own view/unjittered matrices so camera movement cannot imitate
the stimulus. A successful result would establish nonzero CPU-to-command-recording
propagation; actual raster displacement still needs a separate GPU observation.
No unchanged zero-jitter rerun is needed.
