# Scene-depth ownership alongside TAA color/motion

Accepted and closed 2026-09-25: [run 54612](../research/ordinary-taa-scene-depth-result.md).
The separate candidate is stable R32_FLOAT, but its state and depth contents
remain unverified. Previous DLL restored; no test armed. The preparation and
procedure below describe this completed experiment.

Prepared 2026-09-25, standalone recorder **1.7.0**.
This is the next bounded input-assembly diagnostic after the
[larger jitter result](../research/ordinary-taa-strong-jitter-result.md).

## Question and scope

Does the selected primary view retain a real scene-depth resource while its
ordinary TAA color and packed motion are recorded, and does the selected native
consumer declare a usable state for that resource?

The reviewed optional-depth getter at RVA `255CE10` is a 15-byte Windows x64
leaf: RCX is the selected view, RAX returns the texture owner through
`view+628 -> container+A0`. Its SHA-256 is
`1aeeca798d67b2352fa2c1dcfd68361fd34aff602c35ba18877acf6aaf0bc809`.
The pinned TAA Execute call at `23926E6` passes this same selected view when its
optional-depth branch is enabled. The recorder performs bounded scalar reads;
it never invokes the getter or enables the native depth branch.

Tested host version `0.0.0.0`, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded-image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Both are checked during offline preparation; host identity and live code gates
are checked before installing hooks. This build adds the getter's full span
to the existing gates; these offsets apply only to this exact executable.

## Capture and ownership

The seven existing TAA bindings are unchanged. The separately resolved depth
candidate is an eighth observed resource, pinned on the verified D3D12 device.
Three consecutive requests retain frame/epoch/bank, selected view, packet,
native resource description and canonical COM identity. Each candidate adds one
producer reference, for 24 acquired/released references on an accepted run.

The candidate's owner/backing and optional SRV wrapper/CPU handle must remain
equal across the producer call and at native consumer entry, Dispatch and exit.
The producer also records whether `context+50 -> attachment+58` still names a
readable texture owner, the source path used by the earlier depth-copy recorder.
That attachment is supplemental evidence; it can be unavailable here and is
not required to remain current as the frontend starts other work.

SRV wrapper/handle addresses are not typed descriptor contents. This run does
not inspect opaque driver descriptor memory, modify descriptors or bind views.
Native typed views remain unverified. A later integration can use explicitly
owned typed views after validating the resource contract and access state.

The existing barrier observer also derives the candidate's recording-local
state from actual calls inside the matched native compute consumer. Missing
transitions remain unknown; split/subresource/alias conflicts remain invalid.
No earlier-list state, implicit promotion, queue order or scene-depth content
alignment is inferred from an owner pointer or a matching frame number.

## Bounds and validation

Seven existing hooks: TAA compute producer, native consumer, ResourceBarrier,
Dispatch, Reset, Close and ExecuteCommandLists. Their ABI and lifetime contracts
remain those in [consumer observation](temporal-consumer-observer.md),
[submission observation](temporal-submission-observer.md) and
[paired copies](temporal-paired-readback.md).

The projection hook is disabled for this experiment: zero jitter writes.
The two-second window, 150 ms warmup, three consecutive pairs, six readbacks,
18 observer GPU commands, three private signals and 96 MiB readback bound remain.
No depth copies or SDK calls are issued. Copy sources, barriers and lifetimes
retain the existing fail-closed checks. Issued copy references remain pinned
for process lifetime; metadata-only depth pins are released after hook drain.

93 native tests pass with warnings as errors. The seven added WARP cases cover
stable depth, known/unknown depth state, missing current attachment, unreadable
selected depth and replacement during producer, consumer entry and dispatch.
Pure bounded-read checks cover pointer overflow and optional metadata absence.
The versioned reviewer checks ownership, references, actual barrier replay and
scope flags. Its 43 Python tests pass (nine jitter-only cases skip for this
non-jitter fixture); historical jitter evidence is reviewed separately.

An accepted capture establishes request/consumer ownership association only.
The reviewer separately reports whether the resource matches the previously
validated `R32_TYPELESS`, full-resolution, single-mip/slice/sample depth shape.
Even a matching shape and known local state do not certify its content/frame
alignment. `scene_depth_copy_ready`, depth pixel/content flags, typed views and
`dlaa_ready` remain false.

## Local procedure and rollback

Evidence and frozen tools: `local/temporal-scene-depth-20260925/v1.7.0/`.
Preparation checks code spans, test evidence, the closed predecessor archive
and exact DLL/configuration identities. `deploy.ps1 -Action Install` requires
the game closed and preserves the exact destination DLL and any output sidecar.
`seal_prelaunch.py` verifies the installed bytes, frozen sources and settings.

Use the existing MO2 bare profile and `Monster Hunter: World - DLSS 1.2.110 test`
entry. Keep DLSS Off, TAA On, DX12 and the current resolution. Load a save in the
offline/private test scene. Hold the camera still; a stationary pose is helpful
but is not an acceptance invariant. Reply from the phone when loaded. Arm only
after verifying that process generation's ready log and sealed deployment.

After the capture is saved, leave DLSS Off and exit normally. With the game
closed, `deploy.ps1 -Action Restore` restores the exact previous DLL and the
prior output sidecar (or removes the temporary sidecar if none existed).
The game executable/assets, graphics settings, mode sidecar and profile are
not changed by this experiment.
