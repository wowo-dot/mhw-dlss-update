# Large resolutions (v0.24.0)

Roadmap step 4 (the long session) took over the 4K check from the
after-release list on 2026-10-13. The user asked why a size limit exists at
all; this note records the answer and the change that followed.

## 4K in game (game-96, v0.23.10)

The user set 3840x2160 in the game's menu, without DSR. The window was taller
than the 5120x1440 desktop, so the screen did not show it correctly. The logs
show that the game rendered at 4K:

- **Swap chain:** 3840x2160, windowed.
- **DLSS Quality:** rendered at 2560x1440 (exactly 2/3) for 3,319 frames, with
  3 skips (one loading moment) and fault 0.
- **Frame generation 2x:** tagged 3,318 of 3,320 frames, and 3,300 presents
  carried a generated frame.
- **HUD-less:** 3,318 frames, no failures.
- **Exceptions and SDK errors:** 0.

The 1440p launch before it also ran frame generation 2x on v0.23.10 (983 of
983 frames tagged), a first frame generation re-check after the observer
changes.

## Why the old limit existed, and why it was the wrong one

Sizes come from the game's memory. A bound stops a bad read from becoming a
huge allocation, and the mod's images grow with the screen. The bounds were
fixed numbers:

- **The pixel ceiling:** 8192 a side and 16,777,216 pixels (v0.11.0; before it
  4,194,304, sized by diagnostic readbacks).
- **The DLSS image budget:** a fixed 400 MiB in `dlss_sdk::InlineFrame`. This
  was the real limit: 4K DLAA needed 398 MB of it, and 5K failed at every preset
  but Performance.
- **A research readback in the alpha:** two scene-sized R32 readbacks of the
  native depth (`compute_depth_pixels`, 64 MiB cap, about 66 MB at 4K),
  allocated at install although the alpha never reads them.

None of these is a GPU or DLSS requirement. D3D12 allows 16384 a side.

## v0.24.0

- **Sides:** up to D3D12's 16384 (`D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION`)
  everywhere on the playing path: the DLSS geometry, the output and spatial
  images, the unfiltered copy, the render-scale plan and the filter geometry.
  No pixel ceiling. The game packs texture heights in 14 bits
  (`temporal_bindings/inputs.cpp`), so its scenes are at most 16383 tall.
- **Video memory** (`src/gpu_memory/budget.hpp`):
  - The game's adapter is pinned once. Its swap chain's factory enumerates it
    by the device's LUID, so no second factory is created.
  - Before allocating, the runtime reads the process's local budget
    (`IDXGIAdapter3::QueryVideoMemoryInfo`) and checks its images against the
    room left (budget minus usage). It fails closed when they don't fit or the
    budget can't be read.
  - Allocations past the budget would still succeed. Windows would then page
    the game's own resources to system memory, so the mod refuses instead.
- **Where the memory check runs:**
  - **Install:** the plan is computed first, then the check, then the
    allocations. A refusal leaves DLSS off for the session.
  - **Live preset change:** checked before anything changes, while the old
    images still exist. A refusal keeps the current preset running, with no
    retry (`ModeChange::memory`, logged reason "video memory: …").
  - **Live aspect change:** a refusal stops DLSS (fault 48), since the game's
    new scene needs the new images.
  - **Frame generation:** its ring, keyed staging copies and HUD-less images
    are measured exactly (`GetResourceAllocationInfo`) before creation. A
    refusal leaves frame generation off and DLSS running.
- **Exact second check:** `InlineFrame`'s byte limit is now the caller's. The
  alpha passes the room left in the budget, measured exactly as the DLSS images
  are created. Fixtures keep the old 400 MiB default.
- **Logs:**
  - `dlss_summary.runtime.video_memory`: checked, read, passed, budget_mb,
    usage_mb, needed_mb and images_mb (the exact DLSS images).
  - `fg_status.session.video_memory`: the same fields, without images_mb.
- **The research readback** is built only without `MHW_ALPHA`.

### The game truncates in float (IDA, clean image)

The new test sizes exposed a boundary. At 5120x2880 Quality the plan gives
3408x1916, where exact arithmetic gives 1917 rows. The plan is right, and
IDA shows why:

- The scale setter `0x229bd70` quantizes in float.
- The view rectangles are scaled with `cvtdq2ps`, `mulss` and `cvttss2si`, for
  example at `0x26352b0` and around `0x23c38a8`.
- 2880 × 0.6656249762 (the float nearest 0.665625) is 1916.99993, which
  truncates to 1916.

`dlss_sdk::single_scale` required the strict interval, so it refused this
render size. The intervals' upper ends are now closed, and a full side still
requires the other side to be full. DSR 4x of 5120x1440 (10240x2880) at
Quality, which renders 6816x1916, hits the same boundary.

## The mod's memory by size

From `gpu_memory::dlss_bytes` and the frame generation sizes (64 KiB
rounding). The render size follows the game's own plan. Each cell is the DLSS
images plus the frame generation images. Streamline's and NGX's own
allocations are not included.

| Size | DLAA | Quality | Performance |
|---|---|---|---|
| 2560x1440 | 225 + 295 MB | 1696x954: 169 + 153 MB | 151 + 105 MB |
| 3840x2160 (4K) | 506 + 665 MB | 2560x1440: 383 + 348 MB | 340 + 237 MB |
| 5120x1440 | 450 + 590 MB | 3408x958: 340 + 309 MB | 302 + 211 MB |
| 5120x2880 (5K) | 900 + 1181 MB | 3408x1916: 680 + 617 MB | 604 + 421 MB |
| 7680x2160 (DLDSR 2.25x of 5120x1440) | 1012 + 1329 MB | 5120x1440: 766 + 696 MB | 680 + 475 MB |
| 10240x2880 (DSR 4x of 5120x1440) | 1800 + 2362 MB | 6816x1916: 1361 + 1234 MB | 1209 + 843 MB |
| 7680x4320 (8K) | 2025 + 2657 MB | 5120x2880: 1532 + 1392 MB | 1360 + 949 MB |

**Follow-up, not done:** the DLSS colour input and output are RGBA32F
(16 bytes a pixel). The game's colour is R11G11B10, which RGBA16F holds
exactly. The colour input and output could be FP16, saving 8 bytes per render
pixel and per output pixel, plus bandwidth. Motion stays RG32F: FP16 resolves
only 0.5 px at 1000 px. Measure before changing.

## Tests

- **Contract tests:** sides up to 16384 (4K, 32:9, 5K, 8K, 10240x2880,
  16384x16384) pass and 16385 is refused (`dlss_sdk_contract`).
- **The float boundary:** 3408x1916 of 5120x2880 is accepted; 5120x2879 and
  5119x2880 are refused.
- **Plans:** the presets at 5K, 8K and 10240x2880 are one scale, and 8K and
  16384x16384 plans are valid (`render_resolution_contract`).
- **The memory check:** budget arithmetic, image sizes without overflow at
  16384 a side, refusal without a budget, and a pinned adapter's budget read
  (`gpu_memory_tests`).
- **Full suite:** 673 tests pass.

## In game (game-97, v0.24.0, 2026-10-13)

The user enabled DSR 2.25x and 4x (legacy, smoothness 0) in the NVIDIA app.
Every launch ran Quality with frame generation 2x and Reflex Boost.

| Launch | Window | Scene | Render | DLSS frames | FG tagged | Fault |
|---|---|---|---|---|---|---|
| 50480 | 3840x2160 windowed | full | 2560x1440 | 1,014 | 1,013 | 0 |
| 64152 | 10240x2880 fullscreen | 5120x2880 (16:9) | 3408x1916 | 2,265 | 2,264 | 0 |
| 34800 | 10240x2880 fullscreen | 5120x2880 (16:9) | 3408x1916 | 892 | 892 | 0 |
| 46276 | 7680x2160 fullscreen | 3840x2160 (16:9) | 2560x1440 to DLAA and every preset | 6,219 | 6,215 | 0 |

Every launch had zero exceptions, zero SDK errors and no HUD-less failures.

- **The video memory budget** was 31,419 MB (RTX 5090).
  - At 4K Quality, DLSS needed 383 MB by the estimate and its images measured
    264 MB exactly. Frame generation needed 354 MB.
  - At 10240x2880 the numbers were 680, 462 and 622 MB, with the game using
    7.5 to 11 GB.
- **Live presets at 7680x2160:** Quality to DLAA (3840x2160), then to Quality,
  Balanced (2224x1251), Performance (1920x1080) and DLAA again. Each change
  took 187-203 ms and passed the memory check before anything changed (DLAA
  411 MB, measured 382). Frame generation came back every time.
- **3408x1916 is the float boundary.** v0.23.10 would have refused DLSS at it.

**Vanilla MHW breaks at 10240x2880.** The game restarted itself without
Astera Loader after the resolution change. Its 5120x2880 scene lost its
lighting in the lower part: dark and smeared, the HUD intact (the user's
screenshot). With the mod at Quality the user saw it "flawless". The game
then renders 3408x1916, smaller than 4K, and DLSS reconstructs the 5K scene.
So the game's own limit depends on the size it renders itself. Hypothesis
(not reversed): a fixed-size buffer in its lighting. DLAA at that size
would render 5120x2880 natively and should show the vanilla breakage; not
tested.

v0.24.0 tagged.
