# Public SDK before native NGX initialization — v1.12.1

Prepared 2026-09-25 to test the initialization-order hypothesis from
[live v1.11.0](../research/live-sdk-capability-result.md). This diagnostic
creates no feature, evaluates no image, adds no jitter and submits no project
GPU commands. The game's original SDK call runs with its unchanged arguments
and return value, including on rejected diagnostic inputs or failed SDK Init.

## Mapped boundary

Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Mapping evidence remains local at `local/ngx-startup-20260925/v1/`.

The D3D12 renderer initialization calls `+0x23842A0` at `+0x23A72E8`.
The wrapper then calls the executable's exported `NVSDK_NGX_D3D12_Init`
at `+0x273F230`, returning to `+0x23842DC`. Arguments use Windows x64 ABI:
RCX = original application identifier, RDX = original application-data path,
R8 = D3D12 device, R9D = API version 18. The mapped immediate identifier is
`0x05F8A85F`; it is only an admission check, never used for the mod's SDK Init.
The mod uses its own public project GUID.

The client resolves its driver-core function table, initially seven zero
pointers at `+0x51D8808`, before calling the resolved core Init. Its normal
signed-module loader remains unchanged. The renderer wrapper immediately
queries DLSS and ISP availability afterward. This boundary precedes the
feature-provider exports observed in the older bridge work; their Init ABI
and later translated identifier must not be substituted here.

Startup verifies the executable export RVA, host hash/mapped PE identity and
all three contiguous unwind spans of the 500-byte SDK entry, plus the full
344-byte wrapper. Version 1.12.1 polls these same four gates every 10 ms for
at most five minutes, before the sole MinHook installation. All four must
match and all seven native dispatch slots must be readable and zero. Any
populated slot rejects a missed startup immediately; a deadline also rejects.
The readiness receipt records the initial/final gate masks, poll count, ticks,
slot admission and absence of installed hooks. Installation and the callback
still independently require unused slots. The fixture uses a separate
assembly stub, not game instructions.

## First startup result and timing correction

Version 1.12.0, PID 46468, failed its immediate `startup-live-code` gate about
84 ms after process creation. Its two-event log shows no installation or
public SDK initialization. A later bounded read-only inspection at the main
menu found all four regions matching exactly (844 bytes) and all seven native
dispatch pointers populated. This supports early code-readiness timing; the
initial failed region and bytes were not recorded, so its exact cause remains
unproven. No offsets or expected hashes have been relaxed for the retry.

The user exited normally. The exact previous DLL was restored and all four
configuration hashes were unchanged. Immutable evidence:
`local/ngx-startup-20260925/v1.12.0/`, 87 files, archive SHA-256
`aa5a7b700264068d17ed00b3bd8c241bf6287b7adafbbc0b8d34a915979bc119`.

## Experiment and ownership

The early-loaded worker installs one hook before the initial native call.
Installation rejects a populated native dispatch table or already-loaded
copy of the exact modern provider. The first callback additionally checks
the exact caller, identifier, API 18, nonnull device/path and all seven zero
slots. A missed startup or rejected admission leaves the original call intact.
Subsequent calls only forward and cannot repeat public initialization.

The callback uses public `Init_with_ProjectID` with API 21, the existing
project GUID, CUSTOM engine `0.1.0` and the pinned signed 310.7.0.0 provider
directory. It logs actual availability/update/init values before and after
forwarding the native call. Each independently obtained capability parameter
object is destroyed through the public API. The mod makes no success
substitutions and does not bypass support or module validation.

The DLL and trampoline remain pinned until exit. The one SDK initialization
and COM device reference are retained; **no Shutdown is called on the borrowed
game device**. No feature, image resources or command queues are created by
the production diagnostic. The SDK may perform its own internal initialization.

The worker disables the hook once the transaction returns, drains active
callbacks and keeps the trampoline allocated for any suspended entry. It
waits at most ten minutes for the initial call. After disabling, it tries for
up to two minutes to record the swap-chain description using the established
backend slot and offsets (`+0x51C7050`, device `+0x18`, swap chain `+0x1470`).
COM method addresses must belong to executable pages of loaded DXGI, QI must
retain the same base-interface identity, and the backend pointer must remain
stable. Missing display state is logged as unobserved and cannot pass the full
review; it does not delay the early SDK initialization.

## Verification and scope

Release builds use `/W4 /WX`. The actual hook and public initialization pass a
hardware fixture. Its assembly stub deliberately returns `0xBAD00002`; that
result and all four arguments survive unchanged. Second calls only forward;
calls after disabling bypass the hook. Four negative fixtures reject a wrong
caller, API, null device or populated dispatch slot before any public Init.

A headless composition swap chain tests the later display query: the missing
swap chain is rejected, then the created 64x64 RGBA8 surface records format 28.
Fifteen Python reviewer tests reject damaged ordering, admission, capabilities,
native results, closure, display identity and readiness receipts. A C++
readiness test covers every missing code gate, delayed readiness, unreadable
or populated native slots and deadline rejection. Historical v1.12.0 logs
remain readable; v1.12.1 requires its readiness receipt before installation.
The success fixture's synthetic
native error is explicitly replaced only inside the reviewer unit-test input;
it is not presented as a successful real native SDK call.

These tests do not establish coexistence with MHW's older core/client. The
live result must show public initialization before the real native call,
real native Init success, successful capabilities before/after, exact loaded
provider, and clean hook closure. Even a fully accepted result establishes
capability initialization only; live DLAA evaluation remains a later test.

## Run and rollback

Evidence/deployment: `local/ngx-startup-20260925/v1.12.1/`.
Build target `mhw_ngx_startup_probe`; MO2 destination remains the existing
`dlssTEST/mhw_dlss_bridge_probe.dll` slot. Its exact previous bytes are backed
up before installation. No game files, MO2 profile, mode or graphics options
are changed. Provider directory and writable SDK data stay in the repository.

Launch the existing barebones test entry with DX12 On, DLSS Off and TAA On.
Reach the main menu and reply from the phone. Do not enable DLSS or load a save.
Recording starts automatically during startup; there is no arm event. A normal
scene does not itself prove the initialization result. Review
`startup-<pid>-<creation>.jsonl` with `tools/review_ngx_startup.py`.

After review, close normally and run the prepared `deploy.ps1 -Action Restore`.
It checks the destination/backup hashes and sidecar before restoring the exact
previous slot and removing or restoring its output-directory sidecar.
