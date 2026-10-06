# Native callbacks with DLSS Off: run 37452

On 2026-09-23, the standalone [callback recorder](../runtime/jitter-callback-recorder.md)
captured the native TAA2 update/reset callbacks and camera-jitter setter in the
main menu, a loaded scene with still input requested, and the same scene during
a user-reported camera pan. All three bounded CPU windows pass review. The
callbacks execute, but jitter-enable is already zero at every sampled update
entry; deliberate panning did not change that sampled state.

## Identity and conditions

Host version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
PID 37452, process creation FILETIME `134346491504211112`.
Recorder v1.0.0 SHA-256
`8646227E8F9D58E3E34C8DF3F930B589144D6B0B5D0F88BE9ED843DD8C102AD4`.

Read-only preflight verified the process generation, mapped/disk identities,
reviewed live spans, loaded recorder, empty `barebonesdlss` profile and saved
DLSS Off / TAA / DX12 / fullscreen / 2560x1440 settings. The ordinary bridge
was replaced at its preserved MO2 force-load slot by this independent recorder;
there was no provider substitution, Create/Evaluate or depth correction.
The recorder verified seven spans and the vtable slots before hook installation.
Adapter inventory and D3D12 module presence are recorded, but active device,
swap-chain format and actual graphics API remain unqualified by this observer.

The preceding PID 3332 launch ended at the user's request before arming.
Its log contains only start/ready with hooks uninstalled. The arming tool
rejected the exited process without signaling its event. It supplies no
callback evidence and is kept separately.

## Observed calls and order

| Window | Paired calls | TAA2 update | TAA2 reset | Camera setter | Recorded duration |
| --- | ---: | ---: | ---: | ---: | ---: |
| Menu | 256 | 86 | 85 | 85 | 375 ms |
| Loaded scene, still requested | 256 | 86 | 85 | 85 | 375 ms |
| Loaded scene, reported pan | 256 | 86 | 85 | 85 | 375 ms |

Each window reached its 256-call capacity before the 1.5-second deadline and
reported one dropped admission. All 768 pairs are complete and type/read valid;
their before/after snapshots pass the recorder's counter-stability checks.
This accepts the recorded calls, not continuous or lossless coverage.

Each window has 85 same-frame groups ordered update, reset, setter, followed
by one update at the capacity boundary. Entry/exit markers do not overlap
within those groups, and frame/epoch counters match across each paired call.
The frame ranges are 13489–13574, 26977–27062 and 40924–41009 respectively.
This establishes the order of these recorded calls; it is not a complete
render-graph or GPU execution timeline.

Every sampled TAA2 update has `mJitterEnable` (`+0x184`) equal to zero. The
other captured predicate clauses pass: view bits `0x40000`/`0x800000` clear,
nonzero blend, state 8 and flags containing `0x12`. Packed bytes `+0x184..187`
are `0x00010000`. Update and reset before/after scalar snapshots are identical;
renderer reset mode is zero. This does not exclude unobserved intermediate
writes, but neither endpoint reveals a flag change during these callbacks.

The update return address is RVA `0x2228FDA`, immediately after the reviewed
unit dispatcher call. Reset return addresses include `0x221B5D5`,
`0x225F3ED` and `0x225F6CE`; their distribution varies by window. A tail-call
return address alone is not treated as proof of the complete dispatch route.
The menu and gameplay TAA2 objects differ; still and pan use the same object.
All camera types match the admitted derived vtable RVA `0x33F82A8`.

All 255 setters are outside recorded TAA2 callback intervals, with zero input
and zero stored camera jitter afterward. Their return address is RVA
`0x229A339` in renderer camera preparation. At setter entry the camera epoch
is stale; at exit it matches the renderer epoch. The observed view bit
`0x800000` is clear. These facts support the reviewed stale-epoch clearing
path in the [static map](native-jitter-callback-map.md). The call site is shared
by other branches, and no instruction trace was collected, so attribution
uses the combined caller, view and epoch evidence rather than the caller alone.

## Interpretation and next decision

The prior asynchronous samples are now supplemented by actual callback
entry/exit evidence. In these windows the TAA2 callback is dispatched and
already has a disabled jitter byte. The renderer later supplies zero jitter
and advances the camera epoch. That clearing explains the stored zero under
the observed conditions; it does not explain who first disabled the byte.

The user reported panning before phase 3 was armed and was asked to continue
until capture completion. Input and camera motion matrices were not recorded.
This adds a reported movement condition but does not qualify motion vectors,
raster jitter or temporal image alignment. It also does not prove native TAA
is broken or justify forcing jitter on.

Next: follow the typed `mJitterEnable` property assignment before callback
entry, including descriptor-driven writes and object lifecycle. The constructor
default and reflection binding are already mapped; literal-offset searches
alone can miss indirect property writes. Review that path offline before
choosing a bounded writer observation. No identical isolation or pan rerun
is needed. Modern DLAA/DLSS integration remains inactive.

## Stop, restoration and evidence

After phase 3 the recorder reports `fatal=false`, zero pending pairs and
`hooks_disabled=true`. Trampolines/DLL remain resident until process exit by
design. The user reports normal exit; the restoration script independently
required the game process to be absent.

The previous v0.15 bridge was restored and verified at its exact destination,
SHA-256 `689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
Only the new `.dll.output-dir` sidecar was removed. Backups and evidence remain.
Saved graphics, profile settings, mod list and the original mode sidecar are
byte-identical to their pre-deployment copies. All 11 recorded source identities
and the recorder build hash remain unchanged. No game asset was modified.

Private evidence is under `local/jitter-callbacks-20260923/`: generation-bound
preflight and three request/review receipts, menu/still snapshots, the final
JSONL, deployment/restoration receipts and `accept_37452.py` with its retained
`acceptance-37452.json`. Final capture SHA-256:
`47C41ADFB467A9EE06AE1A1493FBD5419C41A82F4D126AA24B409AF8989EE55E`.
Offline acceptance replay verifies the capture, observations, source/config
identities and restoration. Prior native fixture and eight Python checks apply
to the unchanged implementation; this pass changes research notes only.
