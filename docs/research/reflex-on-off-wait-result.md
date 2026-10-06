# Second real Reflex On: Off wait exhausted; effective-mode interpretation under review

2026-10-02, probe v0.4.1 SHA-256
`e81cb2e121d77da401a6560fdb2aabe5b1b3a649edc48bd6b902045ed9c06cb1`.
Retail executable/hash-qualified functions and SDK identities remain those in
[the On contract](reflex-on.md). PID 33764, creation 134354152835423494,
owner 47960; DX12, windowed 2560x1440, format 87, three buffers, adapter LUID
93911:0. The user was asked to keep the game focused during the baseline.

The thirty-second capture completed **6,449 tokens, 6,449 sleeps and 38,694
markers**, zero faults and no pending/incomplete final frame. Eight complete
six-stage samples passed SDK return/count/QPC/owner/epoch/bank checks. The
sixty-four ordered complete driver reports, IDs 6386..6449, exactly match the
final sixty-four tokens. Driver status was Off at early SDK startup and On
after the request, at the five-second checkpoint and after frame drain. A
latency improvement was not measured.

The longer presentation lifetime had 62,329 matched routed/native Presents,
five queues, one swap chain, four resizes and zero faults. Independent read-only
inspection verified all seventeen native hook heads, five full functions,
native graphics identities and the native host swap-chain interface: 355
bounded reads, 25,592 bytes, zero writes or remote execution.

**The current shutdown contract was rejected.** SDK Off returned success,
but the driver still reported On after 95 status queries over 1,000 ms.
The initial and final flags were both On. The probe retained SDK/device pins
and did not call SDK shutdown; the final active flag correctly remains
conservative. Strict review rejected `SDK shutdown`. SDK logs have zero
warnings/errors, with no shutdown or resource-release milestone. Merely
waiting one second did not satisfy this contract.

The user reported a normal scene and normal exit. Four game/profile/probe
configuration files remain unchanged. The current diagnostic DLL stays
installed. The 17-file `game-on-02-archive` and exact 322-file
`source-on-0.4.1` freeze are verified, together with all prior evidence.
Archive manifest SHA-256:
`5f0f34d3864d01db9d90dba3bf6cd522439fc38ee3207142678f7682860161c9`.
Log SHA-256:
`acda9fe19cb50ce07d4dbf7764dbbaaa597dc5792baf9f86fd0fcf3b148638d5`.
SDK log SHA-256:
`16ab07b033eb541947807e351c8fb96d61549d923ef85ed35b5110b3fb3a303d`.

## Effective driver mode and possible override

NVIDIA documents `bLowLatencyMode` as the effective driver mode, which need
not equal the previously requested application mode, including because of a
Control Panel override. The SDK's public pinned source maps options to
`NvAPI_D3D_SetSleepMode`, while its state API returns availability/reports,
not the requested-mode setting. Thus a successful application Off request and
a still-On effective status can coexist. This does not establish the cause
of this game run by itself.

A bounded, read-only public DRS inspection in a separate Python process found
the `Monster Hunter World` profile's user-set `Maximum pre-rendered frames`
(ID `007ba09e`) is 1. The corresponding global profile did not enumerate that
setting. Public name lookups for `Low Latency Mode` and `Ultra Low Latency`
returned setting-not-found, so no exact UI mode is inferred from those lookups.
The initial receipt preceded the user's Control Panel check. No profile values
were changed by the inspection; no game process was opened. The receipt is
`local/reflex-on-20261002/driver-profile02.json`.

The user then supplied a screenshot confirming the game's **Low Latency Mode
was Ultra**, with global Off shown in the selector, and manually changed it
to Off. A repeat read-only DRS query observed the per-game queue value change
from 1 to 0; the selected global values and driver-library hash were unchanged.
This supports an override explanation, but does not yet demonstrate that
the application's Off/shutdown path succeeds with the override removed.
Both profile receipts are preserved; no agent driver-profile writes occurred.

A controlled repeat was prepared in `game-on-03`: the exact same v0.4.1 binary,
SDK, source freeze, game settings and hook/review protocol. Only the user's
driver-profile choice differs; the output sidecar selects a fresh evidence
folder. The current DLL was not rewritten. The [repeat passed](reflex-on-accepted-result.md):
6,298 complete frames, effective Off immediately, verified SDK shutdown and
independent native-code restoration. This strongly supports the override
explanation while preserving the rejected status of this earlier receipt.

The early startup Off sample predates ordinary native presentations. There
is **no effective-mode sample immediately before activation** after those
presentations; it is unknown whether the mode was already On before our On
request. Driver On was observed during integration, but attribution solely
to our request needs that missing baseline if a profile override is active.
Preserve the earlier rejected receipts rather than retroactively accepting
their unverified cleanup.

The controlled repeat is accepted separately. A pre-activation effective
baseline remains useful for later override-aware validation, but was
deliberately not added in that single-variable comparison. Do not force a user's driver profile Off,
substitute a longer blind wait, or call the existing shutdown accepted.

References: [public NVAPI status, mode and DRS definitions](https://github.com/NVIDIA/nvapi/blob/70d337db9186e968eab622f7e786de7e437faf3d/nvapi.h),
[public query IDs](https://github.com/NVIDIA/nvapi/blob/70d337db9186e968eab622f7e786de7e437faf3d/nvapi_interface.h),
[Streamline options mapping](https://github.com/NVIDIA-RTX/Streamline/blob/e8aaa6eaac968711fb62473d4ae8256dde20919b/source/platforms/sl.chi/generic.cpp),
[Reflex plugin option path](https://github.com/NVIDIA-RTX/Streamline/blob/e8aaa6eaac968711fb62473d4ae8256dde20919b/source/plugins/sl.reflex/reflexEntry.cpp).
