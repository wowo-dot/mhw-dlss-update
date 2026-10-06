# Color-grading capture result — 2026-09-28

v1.23.10 captured frames 25218–25220 at active 1280x720 in 2560x1440
allocations. All three input triplets passed the corrected external reviewer;
19,353,600 converted float values matched the raw-input reference exactly.
All recorded GPU work completed and references were released, the single
jitter write was restored, full render scale was restored, and all hooks were
disabled. The user reported a normal scene and normal exit. The latest DLL
remains installed as requested; no subsequent capture is prepared.

## Confirmed observations

The captured native CPU command chain is:

`TAA final copy -> motion filter -> color grading -> final-screen draw`.

The motion-filter output owner/resource equals the color-grading input; the
color-grading output equals the final-screen source in all three records. The
deferred endpoint is the unique AutoExposure unit+0x238 at stage15/order
0x7fffff1. The input/output engine allocations are 2560x1440, engine format34.
These are construction/ownership observations, not evidence of full-output GPU
consumption. [Observer contract and hook lifetime](scaled-colorgrade-observer.md).

All three color-grading shaders are identical: ps_5_0, 5,824 bytes, SHA-256
`174a09e5b23c7e01fb6c47ce86b61250d695113109abb4ffae7219117c31a8cb`.
The shader reads scene color once through interpolated TexCoord0.xy. It does
not consume SV_POSITION, depth, or motion. Its other texture is a 3D color LUT
addressed from sampled RGB, with CBRenderFrame and CBToneMapping controlling
color grading, tone mapping and HDR conversion. Alpha comes from scene color.
Live constant-buffer contents were not captured, so reflection defaults do
not establish which branches executed.

The four captured vertices cover clip space [-1,1] with UVs [0,0.5] on both
axes. The final-screen triangle still addresses the half-size active region
and includes the native half-texel correction. Its destination viewport is
already 2560x1440. It must not keep sampling that half-size region after the
upstream output becomes full-size.

Visual inspection of the diagnostic previews shows complete gathering-hub
color images, corresponding depth silhouettes, and motion concentrated around
the animated character/weapon. Motion preview ranges vary between frames;
these previews do not establish motion units/sign or independently measure
raster jitter.

## Integration consequence and remaining qualification

For this shader, a full-size color-grading target/viewport and UVs [0,1]
appear sufficient; no pixel-shader bytecode change is indicated. Preserve the
native LUT, tone-mapping constants and earlier exposure draw. This is an
inference from the captured program, not a live full-output test.

Next qualify and implement the native GPU state/binding and resource-lifetime
handling for the motion-filter draw, color-grading draw and final-screen
triangle. Connect the [verified motion-filter adapter](scaled-filter-adapter.md)
and [full-output SDK transaction](scaled-output-recovery.md) through that route.
The motion filter retains its low-resolution depth/motion coordinates while
reading reconstructed full-size color. Color grading should retain its shader
and effects. The final-screen draw should sample the full image without
reapplying the old low-resolution stretch. No live SDK evaluation, shader
replacement, or full-output mutation occurred in this capture.

## Reviewer correction and reproducibility

The frozen prelaunch reviewer rejected this run with `resource references`:
seven shared modules omitted version1.23.10 from existing scaled-capture
branches. The first check consequently expected 21 resource pins rather than
the correct 24. Runtime counts were balanced at 24 acquired/24 released.

The correction only adds 1.23.10 to those established branches. It changes no
runtime code, captured evidence, or numerical acceptance tolerances. The
original failure and prelaunch source tree remain untouched. Separately frozen
corrected tools accept this run and the previous v1.23.9 run. Eighteen reviewer
tests pass, including current-version shared input review, incorrect pin-count
rejection and scaled geometry/state checks. Prelaunch native/GPU validation
was 87 passing tests; no native rebuild was needed for this Python-only fix.

Evidence directory (ignored): `local/scaled-temporal-20260928/v1.23.10/`.
Process PID48440, creation134350890950996078. Final log SHA-256
`9ca7602aace6f68cc93c9a09381bb7a58f0e85a5e6c0f35c2f2afb15389d7d8c`.
Installed DLL SHA-256
`326daf080aad3768d77d9c6407f36a5f5ae07745b396150c4460a128e479b905`.
Host EXE version0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decrypted analysis image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Shaders and captured private data remain local and untracked.
