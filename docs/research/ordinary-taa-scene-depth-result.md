# Separate scene-depth candidate — accepted run 54612

2026-09-25, recorder **v1.7.0**, PID 54612, creation 134348174799322152.
The [bounded ownership capture](../runtime/temporal-scene-depth.md) passed its
frozen reviewer and is closed. User confirmed normal exit; the previous DLL
was restored at 16:50:07 +03, the temporary output sidecar removed, and all four
configuration hashes are unchanged. No next test is installed or armed.

Host version `0.0.0.0`, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Recorder SHA-256
`9be0b84d584360b43783230bb4161be842df1bbdcfcfc37d4ca710d7e6c2673b`.
Log SHA-256
`44887afe343dd07e4d0bc37ec787828ac242cead8a96e255955da057470ac681`.
Private evidence: `local/temporal-scene-depth-20260925/v1.7.0/`.

## Confirmed observations

Frames **14157–14159** produced three consecutive color/motion pairs and a
separate depth-candidate observation for each matched TAA request and consumer.
All six files are 14,745,600 bytes, with verified source identity, state
restoration, submission, completion, retirement and file hashes. Color and
decoded motion values are finite throughout. A fixed-tone-map color preview
shows the loaded gameplay scene; it is not the game's final display output.
This run does not measure camera motion, color/depth alignment or jitter.

The selected-view lookup `view+628 -> container+A0` resolves the same candidate
across all three frames. Its owner, backing, native resource, canonical identity
and SRV wrapper/handle stay equal through the observed producer and native
compute checks.

| Property | Captured value |
| --- | --- |
| Container | 14302165840 |
| Texture owner | 14302168752 |
| Backing | 5366866320; secondary backing is null |
| Native resource / canonical identity | 5208636368 / 5208636320 |
| Resource description | 2560×1440 Texture2D, `R32_FLOAT` (41), `ALLOW_RENDER_TARGET` (1) |
| Mips / array slices / samples | 1 / 1 / 1; quality 0 |
| Layout / alignment | UNKNOWN / 65,536 bytes |
| Native SRV wrapper / CPU descriptor address | 14303758288 / 2160697632 |
| Earlier-copy current attachment path | `context+50` is null at this TAA request |
| Recording-local depth state | Unknown in all three matched compute consumers |

The candidate is distinct from the copied color/motion resources and from
ordinary TAA's named `tDepthMap`, which remains a 1×1 `R8G8B8A8_UNORM` fallback.
The selected compute consumer records one native barrier call containing five
barriers per sample; none establishes this candidate's state. That is missing
state history, not proof of COMMON or a known shader-read state.

## Interpretation and limits

The lookup exposes an `R32_FLOAT` render-target resource, whereas earlier depth
readbacks validated an `R32_TYPELESS` depth-stencil resource and its D32 view.
Consequently the strict `scene_depth_matches_prior_shape` result is false.
The metadata diagnostic still passes: it was designed to report the actual
candidate rather than silently substitute a guessed format or reject useful
ownership evidence.

Earlier [native depth-copy observations](depth-copy-result.md) saw full-size
`R32_FLOAT` render-target destinations of copies from the original depth buffer.
The new candidate is consistent with that class of resource. **Its identity as
one of those copy destinations, current contents, depth convention and frame
alignment remain hypotheses to test in this process path.** Cross-run pointer
values cannot establish identity.

The SRV address is not its typed descriptor. No descriptor bytes, depth pixels,
depth copy, projection write or vendor evaluation were captured/issued. The
reviewer keeps typed views, depth-content alignment, depth-copy readiness and
DLAA readiness false. Existing color jitter uncertainty is unaffected.

Offline follow-up found the selected-view constructor calling `269D8F0` before
publishing its `+628` container. That constructor allocates the `+A0` owner with
engine format 16 and flags `600000`, consistent with the captured engine fields.
Private instruction fragments are retained as leads; this is not a complete
write/submission map for the texture and no hook was added from those fragments.

## Capture closure and next decision

Capture, consumer and submission fault/pending counts are zero; all seven hooks
are disabled. All 24 producer pins, three consumer list pins and 242 lifecycle
references are released. The 1,016 submission events comprise 418 Reset, 409
Close, 186 Execute and three Dispatch records. Final observed completion occurs
204 ms after the capture start, with no journal events afterward. Issued copy
resources retain their existing process-lifetime policy; normal exit has ended
that lifetime.

The 167-file prelaunch snapshot and 120 frozen source files verify. Preparation
had passed 93 native tests, 43 Python tests (nine jitter-only skips), 49 code
span checks, and the closed predecessor's 200-file archive. Frozen runtime
review, finite pixel checks, preview, restoration and closure documents are
included in this run's final archive.

Next, correlate the selected `container+A0` owner with an actual native
depth-copy **destination**, its original depth source and the same frame's TAA
request. Reuse the already mapped copy producer/consumer and submission rules
to find an observed valid state interval for the third readback channel.
Do not copy this candidate at TAA dispatch using an assumed state. Once actual
depth pixels are paired and checked, assemble explicit typed input views for
the bounded DLAA integration.
