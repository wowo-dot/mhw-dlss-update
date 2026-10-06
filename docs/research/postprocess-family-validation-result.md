# Combat postprocessing: shared radial contract gaps

On 2026-10-02, the unchanged v1.25.41 build reproduced combat-effect
interruptions. Reconstruction recovered without a runtime fault, but this is
not a compatibility pass. The useful result is a concrete shared failure:
the radial mask validator excludes the native default texture. A second
rejection concerns overlapping radial descriptors with the same command key.
No renderer change or replacement DLL was made in this run.

## Default radial mask

The Safi repeat produced a paired, bounded external CPU snapshot at native
frame 67341. Both shaders match the previously qualified scene-copy/radial
pair. The resource connections remain `scene -> temporary -> scene`.
Radial PS t1 and t2 use exactly the same native owner, resource, SRV wrapper
and CPU descriptor. That owner has native format 7 and extent 1 x 1.

An independent paired live read identifies it as the engine default at the
manager reached through image RVA 0x51c4368, slot 0x288. The pinned native
helper at RVA 0x24819d0 confirms this is intentional: when the custom mask
pointer at parameter offset 0x70 is null, it binds this default to
`tFilterTempMap1` (0x24838c8..0x248390f). The same default is used for
`tOcclusionMap` in the earlier branch. These are observations of native
binding selection, not a claim about the default texture's texel values.

The in-process diagnostic during the reported nova recorded dependency
rejection 3 with `[role=3, engine=7, DXGI=28, dimension=3, width=1, height=1]`.
This is a Texture2D RGBA8 UNORM resource. `dependency_texture()` currently
requires engine format 23 for role 3, even though it already permits RGBA8
as an actual GPU format. Role 2 accepts this same native format-7 texture.
The role-3 wrapper-format gate therefore rejects a supported shader's valid
native input before reconstruction can proceed.

The other accepted external snapshot, frame 65196, contains the same
qualified copy/radial shader pair with a custom 256 x 256, native format-23
mask. The failure does not require a new shader or a monster-specific path.
Actual GPU-format evidence comes from the in-process reader; the external
snapshots do not decode GPU descriptors or prove resource state atomically.

## Overlapping native descriptors

The second retained rejection has command key 0x78001004, with two distinct
`nEffect::uEffectRadialBlurFilter` descriptors, both phase 15/order 4100,
pointing at the same scene output. Its bounded native queue sample contains
four draws and four state packets. The current planner rejects a key with
more than one descriptor before qualifying those draws.

Multiple draws for one descriptor already work. The missing case is multiple
descriptors sharing the key. A correction should group equivalent
source/output contracts, retain every distinct draw in native stable queue
order, and still reject conflicting contracts or duplicate packets. Do not
assign each draw to a descriptor solely from the key. Resource dependencies,
shader identities, states and lifetime checks remain required.

## Alatreon and evidence limits

Frost attacks and a separately reported nova were observed. Their checkpoints
and timeline show further interruptions with the same two retained rejection
shapes and the same default-mask description; no third rejection shape or
runtime fault appeared. The user confirmed DLSS active and clean afterward.
The external reader did not obtain a complete paired Alatreon draw snapshot.
Consequently, the exact binding is directly established for the Safi repeat,
with Alatreon corroborated by the in-process telemetry, not an independent
Alatreon shader capture. User report times bound the attack intervals; they
are not a frame-accurate video alignment.

The hunter silhouette was not captured in this run and remains separate.
Camp entry remains deferred. This run does not claim all combat effects,
ratios, resizing, temporal quality or final displayed pixels are verified.

## Run and cleanup

- Process: PID 1464, creation 134353662552278232; armed at the main menu.
- DX12, RTX 5090, LUID [0,93911], windowed 2560 x 1440; render 1280 x 720.
- Native AA/DLSS/MotionBlur Off; the recorded Stracker-enabled profile retained.
- 40,626 DLSS evaluations; 6,321 skipped attempts; zero runtime faults.
- 6,255 output-plan rejections: 5,165 dependency-discovery rejections and
  1,090 ambiguous-key rejections. These counters are not displayed-frame counts.
- 1,793 completed scene-copy/radial pairs; five accepted auxiliary refreshes.
- All 59 suspension periods resumed before the intentional stop.
- All 46,947 main and 6,259 unfiltered GPU recordings completed and released;
  all 46,487 output recordings released; 48,007 jitter changes restored.
- Full render scale restored; frozen aggregate and 14 component reviews pass.
  Those checks validate telemetry and cleanup, not uninterrupted reconstruction.
- Independent live reads verified 40 complete native code spans and 18 hook
  heads restored. The user confirmed a normal scene and normal exit.
- Game and collector absent; installed DLL and four configuration hashes
  unchanged. Keep the current DLL installed and select a fresh capture folder
  before the next launch.

Private evidence: `local/postprocess-family-validation-20261002-r2`, including
the checkpoint/family/timeline records, `safi-binding-review-1464.json`, both
paired Safi snapshots, `native-default-radial-mask-1464.json`, pinned offline
helper disassembly, failed external-capture receipts, final review, live
restoration receipt and closed archive manifest. Proprietary bytes stay local.

Retail executable version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded analysis image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Installed DLL SHA-256
`797d937e8203b1c2d5ab8ab0079559fed15cf02ef7c88a536316273f4c548dbc`.

Next: implement and test the two shared contract corrections, including
default/custom masks, repeated radial pairs, conflicting-key rejection and
native command ordering. Improve the dependency diagnostic's failing key
and stage: it currently reports key zero, preventing the detailed in-process
draw recorder from triggering. Then validate the corrected combat path live.

Related: [native family audit](postprocess-family-audit.md),
[auxiliary refresh result](auxiliary-binding-refresh-result.md),
[Gallery result](dof-native-compute-route-result.md).
