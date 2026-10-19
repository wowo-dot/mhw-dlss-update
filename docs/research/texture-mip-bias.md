# Texture mip bias (research, 2026-10-19)

The gap first noted on 2026-10-10 (`phase-0-requirements-gap.md`: "Correct mip
bias ... Unresolved"): while DLSS renders below the output size, the game
picks texture mips for the render size, so materials are softer than at
native. The mod has no mip bias handling yet.

Static reading of the clean image (`a78ea69b…1d80`, clean IDA database).
**[C]** confirmed in the code, **[I]** inferred. RVAs are image-relative.

## What NVIDIA asks for [C]

DLSS Programming Guide (31 March 2026), section 3.5:

    DlssMipLevelBias = NativeBias + log2(RenderX / DisplayX) - 1.0 + epsilon

| Mode | Scale | Recommended | Without the extra -1 |
|---|---|---|---|
| DLAA | 1.0 | -1.0 | 0 |
| Quality | 0.667 | -1.58 | -0.58 |
| Balanced | 0.58 | -1.79 | -0.79 |
| Performance | 0.5 | -2.0 | -1.0 |

Section 3.5.1: where the bias causes flicker or moiré, back off, but not above
`log2(RenderX / DisplayX)`. The bias should follow render resolution changes;
"a limited set of mipmap biases" is acceptable. The release checklist lists
"Mip-map bias set when DLSS is enabled".

## How the game builds and binds samplers

- **Templates and banks [C].**
  - sShader (`image+0x51c73d0`) builds its sampler banks once, at shader package load (`0x23d6690`, called from `0x23d3c90`).
  - There are 15 banks of 63 sampler states each, at sShader `+0x2488` (15 pointers to arrays of 63).
  - Bank bias:
    - bank 0: none;
    - banks 1-7: `-log2(c)` for c = 2.0, 2.5 … 5.0;
    - banks 8-14: `+log2(c)`.
  - For a bias b ≠ 0, each template's bias becomes `min(template bias + b, MaxLOD)` (NVIDIA's "NativeBias + …").
  - 6 templates carry a bias computed at build time; its inputs were not traced.
- **A template (36 bytes) [C]**, from the D3D12 conversion at `0x24e6d40`:
  - `u32` packed: filter (byte 0), address U/V/W (bits 8-10/11-13/14-16), max anisotropy (bits 17-21), an anisotropy level offset (bits 22-25), comparison (bits 26-29);
  - then `f32` mip bias, `f32` MinLOD, `f32` MaxLOD, `f32` border[4] and `u32` id.
- **nDraw::SamplerState [C]** (ctor `0x263d890`):
  - `+0x58` the template; `+0x78` the id;
  - `+0x80` the native sampler at anisotropy 1, and `+0x88..+0xa0` the variants at 2/4/8/16 (aniso filters only, up to the template's max).
- **Native sampler [C]** (24 bytes, from `0x2623540`): `+0x10` is a CPU handle in cD3DRender's CPU-only sampler heap (`mDescHeap[1]`: 4096 descriptors, flags none), written with `CreateSampler`.
- **Material resolve [C]** (`0x2383400`, read at `0x2383566`):
  - a material's sampler parameter (index < 12) is baked at load to `bank[sScene+0x1a8][index]`;
  - `sScene+0x1a8` is `mSamplerLodBias` (registered in the sScene ctor `0x22646f0`): an enum, "Default", "-log2(2.0)" … "-log2(5.0)"…;
  - the ctor zeroes it (`0x226559e`), and no shipping code path writes it [I: no singleton-based writer found].
  - Neighbours from the same registration: `+0x1a4` mDefaultBias (float, −5..5), `+0x19c` mTextureMipLimit, `+0x1a0` mTextureMaxWidth, `+0x190` mModelDetail, `+0x260` mTextureTargetMode.
- **Banks 1-14 are materials only [C].**
  - All 241 calls of the generic bank setter `0x23cfec0` pass bank 0, and `0x235ef10` uses bank 0.
  - So in the shipping game banks 1-14 are never bound.
- **Per draw [C].**
  - The table builders `0x259f070`, `0x259f330`, `0x259f610` and the SetShaders path `0x259a490` call `0x263daf0` for every bound sampler state.
  - `0x263daf0` picks the anisotropy variant: the level is the template's offset plus the game's anisotropic filtering option, read from sMhRender by `0x2298ee0`.
  - All four callers read only `+0x10`, the CPU handle, of the returned native sampler.
- **The sampler table cache [C].**
  - `0x23a9200` takes a CRC of the list of CPU handles and looks it up in the cache at cD3DRender `+0x1370` (`0x2624870`).
  - Cache layout: `{entries, count +8, next +0xc, capacity +0x10 = 2048, SRW lock +0x18}`.
  - On a miss, `slot = next; next += n`, and the handles are copied with `CopyDescriptorsSimple` (`0x23a2750`) into `mResidentDescHeapSampler` (cD3DRender `+0x13c8`, 2048 descriptors, shader visible, CPU base `+0x13d0`, GPU base `+0x13d8`).
  - **No eviction and no bound check.** Every distinct handle list keeps its slots for the whole process, and the game relies on staying under 2048.
- **The game's own runtime remap [C, partly traced].** In material bind (`0x235f450`), when the view has flag `0x800000` (a temporal-upsample mode, which also turns TAA off), a sampler is replaced by id with special bank-0 entries. It is not used on PC as shipped [I].

## What follows

- Changing `mSamplerLodBias` affects only materials loaded afterwards, and is fixed for those materials.
- Rewriting a descriptor in place does not reach tables already cached in the resident heap. Rewriting the resident copies is unsafe: that heap is shader-visible and in flight.
- New handle combinations cost resident slots for good, so the number of bias sets must be small and bounded.

## Proposed design (not implemented; for the user to confirm)

1. **One bank per mode, built once.**
   - At install, take unused banks:
     - one for materials (bias 0, the same as bank 0);
     - one each for DLAA, Quality, Balanced and Performance.
   - Rewrite their CPU-only descriptors from the bank-0 templates with `min(native + b, MaxLOD)`, keeping every anisotropy variant.
   - Nothing has bound these banks yet, so no copies of them exist. The rewrite needs the game's conversion tables (filter, address, comparison) as data gates.
2. **Materials into their bank.** Set `mSamplerLodBias` to the materials' bank before the first stage loads.
   - Because that bank is rewritten to bias 0 first, it renders exactly like bank 0 even if the hook below is ever missing.
3. **Live selection.**
   - Hook `0x263daf0`. For a state of the materials' bank (id `+0x78` < 63 and `bank[id]` equal to it), return the original pick for the same id in the current mode's bank; all other states pass through.
   - Cost: two loads and a compare per bound sampler.
4. **Modes.**
   - DLSS Off, a fault or a stop selects the bias-0 bank (vanilla).
   - A mode change switches banks at the next bind. Biases are per mode from its nominal scale, not per frame (NVIDIA's "limited set").
5. **The cache guard.**
   - Log the cache's `count` and `next` in `dlss_summary`.
   - Never select a bank not yet used once `next` passes a margin under 2048; keep the current bank instead (softer, never an overflow).
6. **Fail closed.** If any gate fails, nothing is rewritten and `mSamplerLodBias` stays 0.

Open before implementing:
- **How full is the cache in vanilla** (hub, a hunt, a quest load), and with the mode banks? This needs a read-only measurement in game.
- **Which materials resolve before the switch** (title screen, boot-time persistent resources) and so stay unbiased. Our install point decides it.
- **The bias formula:** NVIDIA's full recommendation, or without the extra −1, and whether DLAA gets −1. It needs an in-game look for shimmer and moiré.

## Compared: mhworld-dlss 0.3 (GPL-3.0, c286e9d3, read only)

Read for comparison at the user's request; nothing is used from it. Its notes
agree on the banks, `mSamplerLodBias` at `sScene+0x1a8` and the material
resolve. Its fix:
- before the banks are built, it repoints the `movss` that loads bank 1's
  constant (2.0 at `0x23d73ee`) to another float already in the image;
- it then selects bank 1.

One bias, fixed for the session and for materials loaded after the switch, by
a code patch. The design above instead follows live mode changes, leaves the
game's code bytes alone, and stays bounded in the cache.
