# The game's own DLSS 1, declined (v0.27.7, 2026-10-14)

Question: on cards where the game's own DLSS 1 still starts (possibly RTX 20
and 30, maybe 40), would this mod run? It would not: every check before
v0.27.7 required that the game have no NGX context. The user agreed to turn
the game's DLSS 1 off for good while the mod is installed ("nobody would need
dlss1 after our release"). An older-generation tester checks it before the
release.

Static reading of the clean database (`local/ida-clean-20261011/mhw.i64`,
image `a78ea69b…`). The game ships DLSS 1.1.13 (`nvngx_dlss.dll`) and
DLISP 1.2.17 (`nvngx_dlisp.dll`) (file versions only).

## How the game builds it (confirmed, static)

- `cD3DRender::initForD3D12` calls `0x23842a0(device)` on an NVIDIA adapter
  (vendor 0x10de) when +0x2da is set. +0x2da means Windows 10 1709 or newer
  (`0x225cbd0`). The call stores the result at backend +0x42700 and the
  "features ready" byte at +0x42718 (`0x2384400`).
  - D3D11 has its twin, `0x2384120` from `initForD3D11`. This mod is D3D12
    only, so it does not touch that one.
- `0x23842a0` does four things in order:
  1. `NVSDK_NGX_D3D12_Init` (the game's static NGX, `0x273f230`, app
     0x05f8a85f). This mod's NGX startup hooks this call and initializes its
     own NGX just before it (`ngx_startup`).
  2. `GetParameters`.
  3. Two availability checks, `SuperSampling.Available` and
     `ImageSignalProcessing.Available`. On a failure it stores the reason in
     `0x51c6c28` (1 init, 2 SuperSampling, 3 ImageSignalProcessing) and
     returns null, with NGX left initialized.
  4. Otherwise it allocates the 72-byte wrapper (`MtScalableAllocator`,
     constructor `0x2383760`):
     - +0: the NGX parameters;
     - +16: a critical section;
     - +56 and +64: the DLSS and DLISP features, null until the next buffer
       rebuild.
- On the 5090, the game's NGX init returns 1 (game-133 startup log
  `native_return`). One of the availability checks then fails. The user
  confirms the game's DLSS has always been unavailable on RTX 50.
- **With a context but the game's DLSS off** (ini `NVIDIA DLSS=Off`), the
  context is inert:
  - renderer +168 (mode) is 0;
  - `0x2383ef0` returns scale 1 and type 0;
  - the buffer rebuilds (`present12`, `0x23ac560`) create no features
    (`0x2384420` with mode 0), and the ready byte stays 0;
  - the TAA2 DLSS branch (`0x2392150`: +165, +180 bit 0, context, ready)
    never runs.
- **With it on:**
  - mode 2: the features are created at the next rebuild;
  - the render scale drops: +176/+512 (`0x229bed0`/`0x229a8c0`), and
    sRender +0x1f548 bits 0x2040 request the rebuild;
  - anti-aliasing becomes TAA and resolution scaling High
    (`game-aa-option.md`).
- **The context's own release** (`0x23837a0`) releases the features, deletes
  the critical section, frees the wrapper, and shuts NGX down.
- **The game's checks of the context:**
  - its Display page row for NVIDIA DLSS is available only with a context
    (`0xeeb090` case 3);
  - with ini DLSS on and no context, the boot sequence (`0x25f5d0`) saves
    `NVIDIA DLSS=Off` and shows message 556 once: "NVIDIA DLSS has been
    switched off. Please check your graphics card and graphics driver
    settings to use NVIDIA DLSS."

## What the mod does (v0.27.7)

- `ngx_startup` hooks `0x23842a0` in the same transaction as the game's NGX
  init: hooked before that init's hook is enabled, and unhooked with it.
- After the original returns a wrapper, the hook frees it as `0x23837a0`
  does, but without the NGX shutdown, since this mod's NGX shares the process:
  - it checks that the features at +56/+64 are null;
  - `DeleteCriticalSection(+16)`;
  - `MtScalableAllocator::genericFree` (`0x2175260`);

  then returns null.
- The game then holds the state its own refusals leave, which is the 5090's
  tested state: NGX initialized, parameters kept, no context.
- It runs always while the alpha is loaded, also with `dlss_mode = off`.
- **Gates:** `0x23842a0`, `0x2383760`, `0x23837a0`, `0x2175260`. If any
  differs, nothing is hooked, and the mod refuses beside a context as before
  (fail closed).
- **Log:**
  - startup log: `game_dlss` (`declining`, `code`), and `stopped.game_dlss`;
  - `dlss_summary.runtime.game_options.game_dlss`:
    - `context` and `refusal`: the game's own;
    - `declined`: 1 hooked and not called yet, 2 the game made none,
      3 declined, 4 kept (a wrapper with features: refused), 5 not hooked.
- **On the 5090:** the game makes none (2); nothing else changes.

## For the older-generation test before release

- The mod arms: `alpha_arm` ready.
- The startup log shows `game_dlss` 3, and the summary `declined` 3, with
  `context` false.
- In the game's Display settings, NVIDIA DLSS is unavailable.
- If that player had the game's DLSS on, the game shows message 556 once and
  saves it Off.
- Frame generation, the AA and resolution scaling rows (`game-aa-option.md`),
  and DLSS Off and back behave as on the 5090.
