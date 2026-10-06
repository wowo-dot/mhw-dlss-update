# Native state rejection capture — v1.21.1

Completed 2026-09-27. The diagnostic captured the remaining native forwards
without changing state admission or rendering behavior. User reported the
area change, walking and camera pan remained active and clean. The assistant
announced an intentional stop, verified drainage, and the user exited normally.
The current diagnostic DLL remains installed at the user's request; no restore
or configuration write was performed. The baseline backup remains verified.

## Observed rejection pattern

All four rejects were the raw-color/output state gate, with zero motion-state
rejects. All four bounded API traces and native state lists were complete;
the independent reviewer replayed every sample. No aliasing, split flags,
subresource transition, broken chain or wrong final-state rejection appeared.

| Sample | Native frame | Point in run | Color evidence in consumer | Output evidence in consumer |
| --- | ---: | --- | --- | --- |
| 1 | 18168 | First admitted requests after arm | No transition or matching native node | Native 0x400 -> 0x400, API UAV barrier |
| 2 | 18169 | Next frame | API 0x4 -> 0x40 | Native 0x400 -> 0x400, API UAV barrier |
| 3 | 23296 | After baseline, by area-load report | No transition or matching native node | Native 0x400 -> 0x400, API UAV barrier |
| 4 | 23297 | Next frame | API 0x4 -> 0x40 | Native 0x400 -> 0x400, API UAV barrier |

Motion had API 0x4 -> 0xc0 in every sample. Native words use the game's enum;
they are not the D3D12 flag values. The previously decoded converter maps native
0x400 to UAV and emits a UAV barrier when its before/after words are equal.
That branch is now matched to live output-resource evidence in all four samples.

The two observed pairs have the same barrier/node pattern. This is stronger
than the previous shared reason counter: `state_of` rejects because it requires
an explicit transition in this consumer scope, and these samples lack one.
The first frame of each pair also lacks local color-state evidence, so an
output-only exception would leave another missing proof.

This capture does **not** establish persistent GPU state independently. A UAV
barrier orders accesses but does not encode a before/after state. The native
node observations are bounded reads rather than an atomic snapshot of other
engine writers. Neither same-state words nor successful native rendering alone
justify weakening admission. `resources[].format` is UINT_MAX here because the
snapshot field is unset; it is not an independently measured texture format.
Existing resource pin/format validation is separate.

## Run and closure

- Process 2584, creation 134349919514657184; arm tick 986171, stop tick 1092796,
  duration 106.625 seconds.
- 7,825 successful DLAA replacements, 578 unfiltered copies, four native forwards.
- 498 suspensions and 498 resumptions; no failure latch, faults or collisions.
- All 8,325 SDK/depth recordings and 578 independent copy recordings submitted,
  completed, reset and released. Zero live recording references.
- All 8,905 jitter scopes restored; hooks disabled, pending callbacks zero.
- Baseline, area-load and final snapshots each passed all eight review channels.
  Complete log review accepted; user normal exit and process absence verified.
- Installed DLL, output sidecar, baseline backup and all four configuration
  hashes verified after exit. Native AA/DLSS Off, Windowed 2560x1440, DX12.

## Next implementation decision

Locate the game's persistent resource-state provenance and its lifetime/reset
rules, covering both the missing color entry and same-state output. Alternatively,
prove a wider barrier history with correct command-list generation, execution
order, aliasing and resource-recreation handling. Do not treat an unqualified UAV
barrier or a remembered pointer as sufficient state proof.

Keep the native preparation/bypass, qualified DLAA, unfiltered suspension and
recording-retirement paths intact. This build remains `exclusive_mode=false`;
producer publication/unmatched consumers and actual SDK failure behavior remain
separate coverage limits. No new live pixel comparison or exhaustive stability
claim comes from this short evidence run. CMFM/UI and resolution-changing DLSS
remain deferred until the agreed AA work is complete.

## Evidence

Root: `local/native-aa-state-20260927/v1.21.1/` (ignored/local).
Frozen 312 sources, 38 native and 119 Python checks; no frozen source changed.
Executable version/hash and hook contracts are recorded in
[diagnostic design](native-aa-state-trace.md).

- Installed DLL SHA: `bc33760a39b20cbee92b0e5ace26e1d72ed72979f307097c0bd4fc934579cfaa`.
- Prelaunch 368-file seal: `a367e2a1445688b9c05321f0fb7e17fbe002e06d21d98887a338ddf356a22595`.
- Final log: `363dd4ec644e5ba092ffcf2868879d3d59c930db3d578077a426b8c50251c590`.
- Final 389-file archive: `9c8df14ee1efbe7ab679f40dad4bac7a61594d6b334702239233c72c2b8512c0`.

Archive entries are immutable. `run-result.json` explicitly records
`dll_left_installed=true` and `restoration_performed=false`. Preserve the
installed slot and backups until the user asks to change them or authorizes
the next runtime build; do not run restoration as automatic cleanup.
