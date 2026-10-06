# Gallery full-depth input result — v1.25.40

The 2026-10-01 test saved both complete native R32_FLOAT depth inputs immediately
before the Gallery horizontal reduction. All twelve compute observations and
both GPU readbacks passed verification, with no capture error or runtime fault.
The active 1280x720 region changed throughout between the two samples; the other
three quarters of the 2560x1440 allocation were bit-identical. This closes the
missing full-allocation pixel capture, not the Gallery rendering integration.

## Identity and capture

Process 43576, creation 134353312651227350, loaded v1.25.40 DLL SHA-256
`a9449d789f2e67f812e299547fbb479e709b5dfb97edb119e85e119fb8075e42`.
Host SHA-256 remained
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Prearm checks verified 690 prelaunch files, seven module identities and four
configurations. Settings remained DX12, windowed 2560x1440, native AA/DLSS Off.
Main-menu and loaded-save checkpoints passed with one startup skip.

Prepared bundles at renderer frames 14139 and 14199 retain all six compute
commands and the composition draw. Their native executions were observed at
frames 14140 and 14200. Each bundle used one command list, with Reset generations
90243 and 91143 respectively. All twelve dispatch observations had zero entry
errors and zero stable recipe differences. All 36 captured texture-use states
match [v1.25.39](dof-stable-recipe-result.md), as do nine resource descriptions per
sample and all eight shader files. The CPU dependency graph retains 22 edges
and eight external reads. Forty-eight barriers were recorded, with none lost.

Both full-subresource copies read the same native depth resource, in state
`0xC0`, before its first reduction. Each saved 14,745,600 bytes: 2560x1440 R32F,
without row padding. Readbacks completed under existing queue signals 1488 and
1548. Successful native Reset and fence completion were verified before saving;
all readback-owned references were subsequently released. Source state was
restored around each copy. The [capture design](dof-depth-pixels.md) documents
the admission checks and ownership.

| Sample | Native frame | Complete file SHA-256 |
| --- | --- | --- |
| 0 | 14140 | `0c38e102612deed6c70d2cd10858599a0ebcd2463d536b1e58904c57b10b994b` |
| 1 | 14200 | `0c535360cf2414d3008b290dcbddba382dd212275cc6a752e590372dff1b8b18` |

## Pixel evidence and its limits

Direct comparison of the saved 32-bit representations gives:

| Region | Pixels | Changed between samples | Sample 0 range | Sample 1 range |
| --- | ---: | ---: | --- | --- |
| Active view, x=0..1279, y=0..719 | 921,600 | 921,600 | 0.001377966–0.050364759 | 0–0.201867104 |
| Right, x=1280..2559, y=0..719 | 921,600 | 0 | 0–0.006408627 | same |
| Bottom, x=0..2559, y=720..1439 | 1,843,200 | 0 | 0.002807567–0.018246945 | same |

Every value is finite and within [0,1]. The right region has 44,124 zero values
in each sample; the bottom has none. These are varied depth values, not a
uniformly cleared outer region. The samples are separated by 60 renderer frames.

Retained data outside the active view is a supported hypothesis. Identical
pixels do not by themselves prove the last writer, their age, or that they were
not rewritten with the same values. The copies capture subresource-zero storage;
they do not read opaque native SRV descriptors or prove the sampling view. Do
not substitute zeros, crop to the active view, or declare the entire allocation
to be fresh current-frame depth. The earlier offline replay demonstrated that
the outer depth region can influence reductions, blur and the final image.

The next offline replay can now use these complete real depth inputs while
preserving native values. Descriptor/view validation and the live six-pass
pipeline connection remain outstanding. This run did not change Gallery
rendering admission, and DLSS still suspended there as expected.

## Recovery and cleanup

The first visual recovery reply was followed by a checkpoint still showing
1,486 DLSS frames and `reconstruction.waiting=true`. It is retained as
`checkpoint-recovery-43576.json`; it is not counted as verified recovery.
After the explicit return to gameplay, `checkpoint-recovery-gameplay-43576.json`
shows 3,008 DLSS frames, no new skips, `waiting=false`, and both suspensions
matched by resumptions. The user also confirmed active, clean gameplay.

The announced intentional stop completed with:

- 3,595 DLSS frames, zero runtime faults and 2,235 skipped dispatches: one
  startup skip and 2,234 Gallery rejections. These are not displayed-frame counts.
- All 5,830 main and 2,235 unfiltered GPU recordings completed and released;
  all 3,621 output recordings and 18 resource-observer references released.
- Both pixel copies saved and released; all 44,159 jitter changes restored;
  full render scale restored and all hooks disabled, with no pending work.
- Frozen full review and thirteen component reviews passed. Independent live
  reads verified all 40 full native spans and 18 hook heads restored.
- Collector ended on the healthy terminal record: 86 events/snapshots,
  23,293,329 bytes, no observed fault and zero read retries.

The user confirmed a normal scene and normal exit. Closure verifies game and
collector absence, installed and backup DLL identities, all four unchanged
configurations and all 690 prelaunch files before sealing the evidence. Keep
v1.25.40 installed as requested. A fresh output destination is required before
another launch; no next candidate is installed by this closure.

Private evidence: `local/dof-depth-pixels-20261001`, especially
`gallery-depth-review-43576.json`, `gallery-input-analysis-43576.json`, the two
`.r32f` files, `final-review-43576.json`, `closed-result-43576.json` and
`archive-final.json`. Gallery remains ahead of monster effects and hunter
silhouette. Camp-entry work remains deferred.
