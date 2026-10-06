# Persistent Reflex Off presentation result, 2026-10-02

Probe v0.2.1 passed its main-menu, loaded-save and windowed focus checks. The
pinned Streamline 2.12.0 Reflex/PCL SDK remained initialized throughout the test
and received presentation maintenance through private owned proxies. The game
retained its native device, factory, queues and swap-chain interfaces. This
establishes persistent SDK compatibility for this run; it does not enable
Reflex sleep/markers or demonstrate latency reduction.

## Identity and observed result

Retail executable version 0.0.0.0, SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Probe SHA-256:
`743ef0eb0b20a4dc3d8c2d951584865e67e00620e59f2e8ab4a809f5d2c2631f`.
PID 51188, creation 134354056681038800. DX12, adapter LUID 93911:0, feature
level 12.0, windowed 2560x1440, format 87, three swap-chain buffers.

The SDK checked Reflex availability and attached to the main native device in
Off before the game created its queues or swap chain. The adapter's public
GetParent returned the factory of the renderer still being constructed. The
qualified device-output owner, adapter, device and factory matched. The global
renderer was not yet published at that boundary, corroborating the v0.2.0
startup-order diagnosis. Independent reads later matched that local owner to
the published renderer and confirmed the host swap chain remained native.

The main-menu checkpoint had 11,988 matched routed/native presentations. After
loading a save, the checkpoint had 27,809 matched presentations; the user
reported a normal scene. A five-second Alt-Tab away/back cycle followed by ten
seconds in the scene also looked normal. All recorded heartbeats had zero
faults, 12 installed COM hooks and matching presentation counts.

The recorder stopped intentionally after **49,453 routed presentations and
49,453 actual native Present calls**. Five queues, one swap chain and four resize
calls were observed. The resize calls are runtime observations; the user was
asked to keep the window size unchanged, so they are not a manual resolution
change test. No SDK warnings/errors were logged. The SDK log records about
229 seconds from initialization to shutdown. No reconstruction, frame tokens,
sleep calls or frame markers were issued. Device replacement/rebinding remains
outside this diagnostic's supported contract.

## Cleanup and evidence

The strict reviewer accepted the lifecycle, Off flags, native identities,
presentation counts, callback drain, SDK shutdown, pin release, two public API
head restorations and 12 COM head restorations. External read-only checks matched
all 14 retained original 32-byte heads and both full native creation functions
to their expected hashes while the same game process remained open. The checker
used 292 bounded reads totaling 19,135 bytes, with no process-memory writes or
remote execution. The SDK shut down Reflex/PCL/common and reported zero delayed
resources. Trampoline/module storage stays resident for stale native forwarding
entries, as documented in the bridge contract.

Ignored evidence is under
`local/reflex-presentation-20261002/game-presentation-02/`: versioned process
log, stage checkpoints, stop sentinel, strict review, live restoration,
deployment/configuration backups and prelaunch provenance. Log SHA-256:
`de2b051bb6b6b496c6ab4aa1b836853059038a49635acc664c442eb884e40d6f`.
The 309-file source freeze is `source-presentation-0.2.1/`; the owned GPU fixture
is preserved separately in `owned-03/`. The user exited normally with a normal
scene. Post-exit hashes confirmed the DLL, both sidecars, graphics, mode, profile
and mod list unchanged. The 20-file stable copy is
`game-presentation-02-archive/`, with its own verified archive manifest. The
current DLL stays installed; the active output directory is not sealed against
another launch, so use the stable copy for durable evidence.

Next: verify the consuming-input and SimulationEnd boundaries, then connect the
tested frame-token/marker adapter. Engine-update return occurs after Present
and cannot be SimulationEnd. Reflex On/Boost tests follow correct marker
placement; ordinary FG/MFG follow the completed Reflex frame contract. See
[presentation contract](reflex-persistent-presentation.md) and
[native frame traces](reflex-frame-boundary-result.md).
