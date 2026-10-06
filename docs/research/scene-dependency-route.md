# Scene-dependent post-filter route — v1.25.29

## Scope and evidence

Connects the offline [dependency/coordinate work](scene-color-dependencies.md)
to the continuous DX12 renderer. This is a runtime candidate, not an in-game
success claim. The v1.25.28 wingdrake capture established a scene copy followed
by a radial draw; Gallery and other reported effects need live confirmation.

Executable SHA-256 remains
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
No new hook address, calling convention, native code mutation, SDK option or
game-setting change is introduced. Existing stream entry/return hooks, native
code gates and DirectX command-list observer retain their previous contracts.
All native shaders are supplied privately at runtime; none are embedded.

## Admission and recording

`dependency_route.inl` qualifies each actual draw in completed native queue order.
The descriptor's advertised scene output is insufficient: the copy's actual
RTV is matched to the consumer's retained temporary owner. The dependency graph
must prove scene → temporary → scene before the frame can be prepared.
Unknown inputs, reordered reads, self-aliases, auxiliary writes and a missing
return to scene reject the entire plan. Ordinary auxiliary-only overlays keep
their existing extent path and cannot read a newly expanded temporary.

Only the captured copy VS/PS pair and radial VS/PS pair are admitted, by complete
SHA-256. The former uses packed 16-byte vertices; the latter uses the observed
28-byte position/primary-UV/auxiliary-UV/color layout. Both must match full native
quad coverage and reduced primary UVs. Their original packet offset and first
vertex remain qualified. Independent auxiliary UVs remain unchanged.

Native binding executor `0x259f330` remains code-gated. Captured binding words,
owner slots, zero modes and unique PS register coverage must agree. The copy
has t0; radial has t0/t1/t2. Every texture is checked against its actual device
and GPU description. Scene is full-allocation R11G11B10_FLOAT, temporary is
full-allocation R10G10B10A2_UNORM. Auxiliary textures retain native extents and
must be shader-readable normalized formats, never integer/depth resources.
These are native-table checks, not independent GPU descriptor-heap readback.
Tables/owners/views are rechecked at DrawInstanced.

Effect now supports private copy/radial PSOs and immutable vertex buffers.
Copy samples the expanded scene; radial uses the already validated coordinate
adapter. Native blend state is retained; depth, stencil, logic operations and
multisample coverage are excluded. Only PSO, vertex buffer, viewport and scissor
change, and all four are restored. RTVs, descriptor contents, root arguments,
constants, barriers and native shader bytes stay native.

Both new draws participate in the existing immutable frame predicate and
FrameHandoff execution-order proof. Native warmup runs until all required PSOs
exist. Sixteen immutable dependency variants are bounded and never evicted
while recordings may reference them. Cache exhaustion admits no new recipe.
Every private draw needs an observed local RT state; native sampled read states
may be inherited, but an observed contradiction rejects the draw.

Plans retain at most 32 resources (including base/haze resources). All temporary
and auxiliary resources join every participating recording's ownership set.
Fence completion alone cannot release them: successful command-list Reset is
also required. Existing fallback, submission, stop and jitter/scale restoration
mechanisms are reused.

Telemetry adds `dependency_route`, overlay policy
`native-extent-and-dependencies-v2`, and readiness schema 4 with twelve possible
draw identities. Qualification reasons 12/13 distinguish resource/binding
discovery from dependency-order rejection. Historic logs remain reviewable.

## Offline verification

Evidence: `local/scene-dependency-route-20261001`.

- Build `/W4 /WX`: no project compiler warning or error in final build.
- 58 native tests passed.
- Actual production Effect draws: 192/192 cases on WARP, all exact; 192/192 on
  RTX 5090, 148 exact. Others satisfy the previously documented bounded packed
  filtering comparison (maximum decoded error 0.015625, two packed steps).
  Every temporary copy is exact; four deliberately incorrect coordinate
  controls fail as expected. No GPU validation warning/error.
- Actual completed-queue discovery on WARP and hardware: copied target identity,
  native offsets/layouts and owner stability; seven malformed cases rejected.
- Actual `record_effect` copy branch on WARP and hardware: expanded output and
  unchanged native-quarter output selected correctly; bindings and predication
  restored; extra references retained through completion and released on Reset.
- Existing continuous runtime fixture: 96 reconstructed and 96 native-fallback
  frames, changed vertex allocations and reordered command-list recording.
- Python diagnostics, overlays, scaled routing, historical logs and synthetic
  v1.25.29 logs checked, including missing telemetry and extended pipeline-mask
  rejection. Synthetic logs are not runtime evidence.

Initial fixture failures (wrong historical shader prefix, a base-only root
signature used for radial PSO creation, and optimized-clear mismatch warnings)
are preserved in the local work directory; final checks correct the fixtures
without weakening shader identities or GPU validation.

## Next live test

Use the current MO2 profile, DX12 windowed 2560×1440, native AA/DLSS/MotionBlur
Off. Start at main menu, arm after startup checks, load one save, take a wingdrake
trip, then replay the previously affected Gallery cutscene. Capture copy/radial
counts, frame-wide selection, resource qualification and recovery. Do not claim
all combat effects or the hunter silhouette fixed from this candidate.
Keep the installed DLL after normal shutdown, as requested. Deployment and
exact restore instructions live beside the candidate and its preserved backup.
