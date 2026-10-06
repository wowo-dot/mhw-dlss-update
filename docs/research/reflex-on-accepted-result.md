# First accepted real Reflex On baseline

2026-10-02, probe v0.4.1 SHA-256
`e81cb2e121d77da401a6560fdb2aabe5b1b3a649edc48bd6b902045ed9c06cb1`.
This is the exact DLL used in the [rejected one-second Off wait](reflex-on-off-wait-result.md).
The user confirmed the game's NVIDIA Low Latency Mode was Ultra and manually
disabled it. A read-only public DRS repeat observed the game's queue setting
change from 1 to 0; selected global values and the driver-library hash stayed
unchanged. The agent wrote no driver settings. SDK binaries, game/profile
settings and implementation were unchanged; the output sidecar selected a new
capture folder.

PID 53424, creation 134354205993001122, frame owner 26988;
DX12, windowed 2560x1440, format 87, three buffers, adapter LUID 93911:0.
Native device 354985024, backend 348717184, factory 7005312, swap chain
359745776. Retail executable/qualified native code hashes remain those in
[the On contract](reflex-on.md). The user was asked to keep the game focused.

## Accepted runtime observations

The thirty-second capture completed **6,298 complete tokens, 6,298 sleeps and
37,788 markers**, with zero faults and no pending or incomplete final frame.
Eight complete six-stage samples passed SDK result/count/QPC/owner/epoch/bank
checks. Sixty-four complete ordered driver reports, IDs 6235..6298, exactly
match the final sixty-four integration tokens. The driver reported baseline
Off, On during the run and after frame drain, then Off immediately after the
SDK Off request: one query, zero measured elapsed milliseconds.

SDK shutdown succeeded and device/interface pins were released. The SDK log
records shutdown of Reflex, PCL and common, zero warnings/errors and a delayed
resource list count of zero. The probe's final active flag is false. This is
the first accepted real-game On -> Off -> SDK shutdown baseline.

Longer startup-to-stop presentation lifetime: 17,168 matched routed/native
Presents, five queues, one swap chain, four resizes and zero faults. Fifty-three
observer Presents had no tracked frame and produced no synthetic frame markers.
The lifetime counts include startup; they are not proof of live On-mode
resize/recreation coverage.

Independent bounded query/VM_READ inspection while the game remained open
verified all seventeen native hook heads, five full functions, native graphics
identities and native host swap-chain interface: 355 reads, 25,592 bytes,
zero writes or remote execution. Strict sequence review and the independent
cleanup review both passed.

The user reported a normal scene and normal exit. All four game/profile/probe
configuration hashes remained unchanged; the diagnostic DLL stays installed.
The 20-file `game-on-03-archive` and exact 322-file `source-on-0.4.1` freeze
are verified, alongside six previous archives and five previous source freezes.
The source/DLL were not rebuilt or rewritten for this comparison.

Archive manifest SHA-256:
`5706dc46cbd65ae699f481aa24d9b41d79453cfecc8e9a5b4334486e7d5aa4b1`.
Log SHA-256:
`ff7576b54f850c29c6b596f1d70227da08da184e5f34417ee0fdda6638d5cc0b`.
SDK log SHA-256:
`b8f9fa1cab7120f6ef981847b5c20b4f718ad7c3eaf373d8aae0d4ee1c2636e4`.
Evidence/source manifest references are under `local/reflex-on-20261002`.

## Interpretation and remaining scope

The same-binary comparison strongly supports the user's Ultra override as
the reason the earlier effective driver flag remained On after accepted SDK
Off requests. NVIDIA documents that this flag represents effective driver
mode, which can differ from the application's request under Control Panel
overrides. The previous failed-contract receipts remain rejected and preserved;
this successful repeat does not retroactively demonstrate their SDK teardown.

Production mode ownership must distinguish application-requested Reflex
options from external effective driver policy. Requiring an effective Off
flag irrespective of a user's external policy is unsuitable as a general
shutdown rule. The controlled Off-profile baseline accepted here does not
yet implement an override-aware shutdown policy. A readback immediately before
activation would also separate the post-presentation effective baseline from
the early startup sample when an override is active.

This test verifies activation, real frame sequencing and clean teardown;
**a latency improvement was not measured**. The subsequent [v0.4.2 run](reflex-on-transition-result.md)
accepted live On through one area/loading and isolated focus transition.
Coexistence with the preserved temporal renderer remains next before ordinary
frame generation and later MFG.
Boost, production settings/UI, presets and performance cleanup remain deferred.

Reference: [NVIDIA effective-mode status semantics](https://github.com/NVIDIA/nvapi/blob/70d337db9186e968eab622f7e786de7e437faf3d/nvapi.h).
