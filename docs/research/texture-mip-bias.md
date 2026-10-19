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

## Measured in game

- **Run 54460 (0.31.0-dev.1, read only; DLAA, the user's anisotropy at its highest).**
  - Every bank's bias read exactly as above, with MaxLOD 65536. `mSamplerLodBias` was 0.
  - The cache after the hub and a hunt: 70 tables, 433 slots.
  - Stepping Anisotropic Filtering through every level: 174 tables, 1329 slots. A new level's set costs about 225 slots, for every sampler with variants.
  - Four live DLSS mode changes: flat.
- **The ladder is NVIDIA's formula.** `-log2(c)` for c = 2.0 … 5.0 is `log2(scale) - 1` at scales 1, 0.8, 0.67, 0.57, 0.5, 0.44 and 0.4. The game's own banks already hold:
  - DLAA: bank 1 (−1);
  - Quality: bank 3 (−1.585);
  - Balanced: bank 4 (−1.807 for −1.786);
  - Performance: bank 5 (−2).

  No descriptors of the mod's own are needed.
- **Run 26960 (0.31.0-dev.2, `texture_bias_ab = on`).**
  - Installed on the 7th try, while the game was still booting (cache at 9 slots), so nearly every material takes the bias.
  - 6 live mode changes followed by the bank; 53 bank switches; fault 0, a clean exit.
  - Five banks at levels 0 and 4: 707 slots. A bias bank's material set is 40 to 70 slots.
  - The user's screenshots (Hoarfrost Reach, Performance): crisper ice and cliffs, and bare twigs thin instead of clumped (alpha-tested foliage at a finer mip). The user saw **no shimmer**.

## What the mod does (0.31.0-dev.3, `continuous_dlaa/texture_bias.*`)

1. **Install**, from the worker's frames start, tried until the banks exist:
   - 13 code gates (the hooked pick head-aware);
   - every state of every bank checked against bank 0's template (id, packed word, bias relation, variants, handles);
   - `mSamplerLodBias` still 0 (else another mod chose a bank: refused);
   - bank 0's descriptors copied over bank 14 (`CopyDescriptorsSimple`, CPU-only heaps; bank 14 is never bound as shipped);
   - `0x263daf0` hooked through al.hooks;
   - then `mSamplerLodBias = 14`.

   Bank 14 renders as bank 0 even without the hook.
2. **The pick.** A bank-14 state (`materials[id] == state`) gets the original pick of the same id in the active bank. Every other state passes through.
3. **Following DLSS.** Each worker loop (100 ms):
   - the render scale (render ÷ scene width) gives `bank_for(scale)`;
   - DLSS Off, a stop or a fault (scale 0) gives bank 0;
   - the end of the session gives bank 0.
4. **The guard** (`room_for_new_set`).
   - A bank's first binds at a level (a new set) are allowed while the cache keeps room for that set (measured, at least 64) plus 256 slots for each level bank 0 hasn't been bound at.
   - Without room, a pick falls back to bank 0, and the worker picks the nearest bank that already has a set.
   - The level is the one samplers with variants show; one without variants stays at 0.
5. **Settings.**
   - `texture_bias = off` changes nothing in the game.
   - `texture_bias_ab = on` alternates every 10 s with the game's own (diagnostic).
   - `dlss_summary.texture_bias` and the `texture_bias` events log the bank, level, cache and guard.

Not covered:
- materials that resolved before the install keep bank 0;
- the game's temporal-upsample remap (view flag `0x800000`) is not used on PC.

## Compared: mhworld-dlss 0.3 (GPL-3.0, c286e9d3, read only)

Read for comparison at the user's request; nothing is used from it. Its notes
agree on the banks, `mSamplerLodBias` at `sScene+0x1a8` and the material
resolve. Its fix:
- before the banks are built, it repoints the `movss` that loads bank 1's
  constant (2.0 at `0x23d73ee`) to another float already in the image;
- it then selects bank 1.

One bias, fixed for the session and for materials loaded after the switch, by
a code patch. This mod instead:
- follows live mode changes with the game's existing banks;
- leaves the game's code bytes alone, apart from the al.hooks hook;
- stays bounded in the cache.
