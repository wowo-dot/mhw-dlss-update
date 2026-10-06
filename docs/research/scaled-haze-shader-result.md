# Haze shader extracted; current-frame recipe still unverified

On 2026-09-30, the existing v1.25.7 DLL ran in process 46396 (creation FILETIME
134351923361060777). Local evidence is `local/scaled-haze-20260930`. The user
entered the sauna, stayed with menus closed, then left and confirmed DLSS
recovered. The scene was normal after intentional stop and normal exit.
No rendering correction or new native hook was installed during this run.

## Runtime result

The complete versioned capture passed review: 2,271 DLSS evaluations, zero
runtime faults, and 29,769 skipped evaluations. Those skips comprise one
pipeline warmup and 29,768 unbuilt output plans. No consumer-depth interval or
CPU callback-order rejection occurred in this run. Retained rejected graphs
again identify stage 15/order 20000, uHazeFilter type RVA 0x35372b0, connecting
motion output to a different grade input. Both suspension periods recovered.
This verifies the observed failure classification, not a sauna fix.

All 32,040 reconstruction/depth GPU recordings and 29,769 unfiltered recordings
completed and released; all 32,041 jitter changes were restored. Full render
scale returned. The cleanup log and independent live-memory comparison passed
36 native code spans and 17 hook heads. The game then exited normally.

## Evidence limits

The strict external reader did **not** accept a full recipe. The primary-view
and queue window was short: repeated asynchronous observations found no active
primary view, a partially built queue, or no matching haze draw. The strict
reader's same-frame/double-snapshot checks were not weakened.

A separate bounded query/VM_READ helper examined retained queue-tail candidates
and recovered a pixel shader whose reflection contains CBHazeFilter, together
with its vertex shader. Each blob was read twice with identical bytes; packet
and shader-set links were also rechecked. This used 52 logical reads / 39,064
bytes, no process writes and no thread suspension. Three invalid candidates
were rejected. The candidate lay outside the active queue count: it establishes
saved shader content, **not** current-frame membership, bound GPU descriptors,
resource states, or native geometry. Do not admit a live route from it.

Host SHA-256 is
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
decoded comparison SHA-256 is
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
The read-only preparation records the native function hash gates. Captured
DXBC and disassembly remain local and untracked; they are not distributable
project sources.

## Shader findings

| Stage | Bytes | SHA-256 |
| --- | ---: | --- |
| PS | 14,596 | 4f890ffa89ea59a218db7e191d8aab48e9d5fe4be200c50a606b22594bb57f69 |
| VS | 712 | c7c73b6ac661514c34c53681e217dc3fe7014e21a99a41adb0fcb7befc1f5de6 |

The PS reflection names color `tBaseMap` at t0, depth `tDepthMap` at t1,
3D noise `tVolumeMap` at t2, and `tFilterTempMap1` at t3. This names the shader's
declared inputs; it does not prove which resources were bound at capture time.
The samplers are s0 for base/depth and s1 for the filter inputs.

The 2,604-word ps_5_0 program switches on CBHazeFilter's technique field with
four explicit cases and a default. All five branches end with an identical
t0 sample instruction. Their SHEX DWORD offsets are 487, 942, 1585, 2135 and
2580. Each consumes r1.xy and writes r0.xyz. Four t1 depth samples appear in
the two depth-aware branches, alongside five t3 and forty t2 samples across
the program. Temporary declaration offset 46 currently requests six temps.
The reproducible local analysis verifies these against the full PS hash.

Native clamps use CBScreen's fScreenInverseSize (byte 24) and fViewSize (byte
56). Other calculations use inverse view/projection and camera position, plus
haze distance, height, noise and intensity parameters. The VS passes texture
coordinates through and separately carries clip-space XY for reconstruction.
This differs from the already supported motion/grade vertex shader.

## Implementation direction, not yet verified

The captured program permits a narrow candidate adaptation: preserve native
depth/noise/reconstruction calculations, and use a new temporary to expand
only the final t0 color sample coordinate in each branch. For a full-size
color target, the candidate multiplier is output extent / render extent.
The existing motion shader adapter already follows this separation of color
sampling from auxiliary inputs. Changing all UVs together would also alter
depth lookups and haze behavior. Clamps, edge behavior, implicit sampling
derivatives and actual native vertices still need validation before this
becomes a supported effect.

Next prepare that hash-gated adapter offline with identity/2x GPU comparisons,
and capture the missing recipe from the existing fullscreen callback at native
RVA 0x2297e20, narrowed to haze caller 0x24c7ff9. Capture during queue building,
then validate its actual draw bindings and lifetime before scaled admission.
Reuse the installed hook rather than relying on another asynchronous queue
sampling window. Extending the graph means representing an optional haze
stage between motion and grading and preserving its native inputs/output;
merely accepting the changed owner link remains incorrect.

The earlier panning-history hypothesis and early CPU-recording issue remain
separate work. This run did not isolate panning, and it supplies no controlled
comparison for changing the history policy. No performance improvement is
claimed. The current DLL stays installed by user request.
