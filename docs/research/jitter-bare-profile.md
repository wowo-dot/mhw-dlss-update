# Jitter observation with an empty MO2 profile

Updated 2026-09-23: run 2632 observes the same disabled TAA jitter byte and
zero stored camera jitter in menu and loaded gameplay with no enabled MO2
mods and no detected project diagnostic or mod-loader DLL. The earlier active
bridge, passive bridge and enabled mod list are not necessary for these
sampled values. The native assignment and callback execution remain unresolved.

The user exited normally; process absence and unchanged saved Off/TAA settings
and profile files are verified. No code, deployment, game setting or profile
was changed by the assistant. No new launch is needed for this result.

## Controls compared

| Run | Enabled MO2 mods | Bridge behavior | Menu jitter byte / stored jitter | Gameplay jitter byte / stored jitter |
| --- | --- | --- | --- | --- |
| [5836](jitter-pre-enable-observation.md) | Original mod list | Active startup, no Create | 0 / zero | 0 / zero |
| [8164](jitter-startup-control.md) | Original mod list | Synthetic behavior disabled; observers retained | 0 / zero | 0 / zero |
| 34948 | None | Inherited Force Load still loads active bridge | 0 / zero | Not sampled |
| 2632 | None | No bridge DLL detected | 0 / zero | 0 / zero |

These are asynchronous CPU observations. They neither establish that jitter
is zero throughout every frame nor show that native TAA is broken. A layer
found in the registry is not automatically the active callback argument.
The user later clarified that they did not deliberately move the camera during
the tests. This adds stationary-input context, not a motion-vector check or
proof that every camera matrix stayed constant.
Original assets were not exhaustively verified, and normal MO2 virtualization,
Steam and graphics-driver runtime modules remain. Describe the actual control
conditions rather than claiming an independently verified pristine installation.

## The inherited Force Load rule

The user's `barebonesdlss` profile has zero `+` mod entries. Its initial launch,
PID 34948, still loaded `mhw_dlss_bridge_probe.dll`: the cloned profile retained
the separate forced-library rule for the DLSS test executable entry. Disabling
the mod list alone does not disable that rule.

The auxiliary menu sample was preserved rather than mislabeled as a no-bridge
baseline. Its 16 broad and 16 focused attempts all have the jitter byte 0 and
stored jitter zero; 10 focused attempts satisfy combined consistency. Its
trace records no Create/Evaluate, two successful Shutdowns and cleared bridge
cleanup. `auxiliary-review-34948.json` retains those distinctions.

The user then launched the ordinary **Monster Hunter: World** entry with the
same empty profile. Its forced-library group is disabled. Both run-2632
module snapshots contain no `mhw_dlss_*` DLL, no module mapped from the MO2
mods directory, and no unexpected game-directory module. In that directory
the mapped names are MonsterHunterWorld.exe, steam_api64.dll and amd_ags_x64.dll.
Stracker's loader was not detected. The test entry's inherited rule remains
in the profile; the assistant did not edit it. Continue to verify the actual
module inventory rather than trusting the enabled mod list alone.

## Run 2632 evidence

PID 2632, process creation `134346459423636911`, uses the pinned host version
`0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
The decoded code-comparison image is
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
Both preflights and every reader invocation verify the six previously reviewed
live code spans. The broad/focused readers are byte-identical copies of those
used in the earlier controls, with the same read budgets and failure behavior.

Settings remain DLSS Off, TAA, DX12 On, FullScreen, 2560x1440. D3D12 runtime
modules are present; system adapter inventory records RTX 5090 / driver
32.0.16.1088 and the virtual display driver. This external CPU observation
does not instrument a device, swap-chain format or GPU execution.

| Observation | Attempts | Source frames | List nodes | Combined focused consistency |
| --- | ---: | --- | ---: | ---: |
| Menu Off, broad | 16 | 8817–9187 | 27 | Not computed |
| Menu Off, focused | 16 | 9386–9754 | 27 | 11 |
| Loaded save Off, broad | 16 | 24253–24626 | 238 | Not computed |
| Loaded save Off, focused | 16 | 24831–25205 | 238 | 11 |

All 64 attempts retain jitter-enable 0 and stored jitter bits `[0,0]`. Each
traversal finds one TAA candidate with vtable `0x351BDA8`, type 8, flags `0x13`
and blend bits `0x3F733333`; menu and gameplay have different layer addresses.
Camera type is `0x33F82A8`. The backend NGX/context pointer is absent in all
broad attempts while renderer `+0xA5` stays 1. All 32 broad attempts record
the game foreground; focused passes do not record foreground separately.

Broad menu reads retain stable links in 16, anchors in 15 and unchanged
renderer counters in 13. Broad gameplay retains stable links in 16, anchors
in 2 and unchanged counters in 1. All broad layer-field/jitter rechecks pass.
Each focused phase retains stable links, anchors, types and scalar rechecks
in all 16, but only 11 also retain matching counters/camera epoch. Only those
11 per phase receive combined consistency. Do not treat the broad gameplay
pass as a coherent frame observation or upgrade any sample to GPU evidence.

## Exit, archive and next decision

No bridge trace is expected for run 2632 because its DLL is absent. The external
JSON files are the observation channel and have no five-minute deadline.
Shutdown was not instrumented. The user reported a normal exit, process absence
was checked and copies of graphics/profile/mod-list files match their preflight
hashes. The saved settings retain SHA-256
`02BAB1311AD739420182D2FF5DB8A6ED9536235E80B51D2B736A6232633CB235`.

Private evidence resides in `local/jitter-bare-20260923/`: separate inventories
and receipts for 34948 and 2632, four final sample sets for 2632, profile/settings
archives, adapter inventory and the unchanged readers. `accept_2632.py` replays
identities, module exclusions, counter consistency and exit receipts into
`acceptance-2632.json`, including artifact hashes. No memory writes, remote calls,
debugger attachment or new hook were used. Nothing requires restoration.

Next, prioritize a type-bound observation of the native jitter assignment and
callback/reset order. The [startup map](jitter-startup-control.md) separates
general TAA enable from `mJitterEnable`; the latter defaults to 1 statically
but is observed as 0 after startup. Identify the actual writer or the active
callback argument before attributing this to a defect or forcing the byte.
Repeatedly removing the same mods or rerunning the same DLSS-enable sequence
will not answer that remaining question. Modern SDK integration and temporal
input/raster qualification remain inactive or unverified.

The subsequent [native callback map](native-jitter-callback-map.md) identifies
the dispatch gates, frame-indexed selector and stale-epoch clearing path.
Runtime ordering and the enable-byte writer remain open; no recorder was
deployed by that offline investigation.
