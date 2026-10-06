# MHW frame-generation handoff: presentation ownership probe

## Scope and status

Probe v0.6.0 adds a bounded CPU ownership map to the accepted DLSS Performance
v1.25.44 / Reflex On renderer. It does **not** load the FG plugin, tag FG inputs,
change swap-chain flags or generate game frames. The game ownership map was
accepted on 2026-10-03; see
[frame-generation-game-ownership-result.md](frame-generation-game-ownership-result.md).
The owned FG/MFG foundation remains separately accepted; see
[frame-generation-foundation.md](frame-generation-foundation.md).

The owned SDK-paced renderer admitted buffer/index/presentation calls on its
single chain-creation thread. MHW's API owners must be observed before extending
that contract. The SDK worker can use the same physical chain: routing its calls
to logical SDK buffers previously caused device removal in our owned control.
Likewise, the FG chain uses the SDK's pacing queue, so the existing Reflex-only
stop/shutdown path cannot simply be reused with the FG plugin loaded.

## Recorder contract

All hooks are existing x64 `STDMETHODCALLTYPE` public COM hooks. Each target is
qualified as an executable image method, its original 32 bytes retained, and
restoration checked after stop/drain. Native return values/interfaces and
LastError preservation retain the existing presentation contract. Native game
creation and frame functions keep their full-code SHA-256 gates. The tested
retail executable is version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
No new native offsets or signature discovery are introduced.

Method IDs are the existing hook indices:

| ID | Method | Recorded arguments |
|---|---|---|
| 0 | Device CreateCommandQueue | type, priority bits, flags, node mask |
| 1–3 | Factory CreateSwapChain / ForHwnd / ForCoreWindow | native queue, width, height, format, buffer count, flags, swap effect, windowed |
| 4, 9 | Present / Present1 | interval, flags, matching Reflex observer ticket, synchronous native-call count |
| 5 | GetBuffer | index, requested IID is ID3D12Resource; returned interface identity |
| 6 | SetFullscreenState | fullscreen, output identity |
| 7 | GetDesc | width, height, format, buffer count, flags, windowed |
| 8, 11 | ResizeBuffers / ResizeBuffers1 | count, width, height, format, flags |
| 10 | GetCurrentBackBufferIndex | index in the result field |

Unused argument slots are zero. Queue creation is recorded after a successful
routed return. Swap creation arguments are copied before the SDK call, then
associated with its HRESULT/native returned identity. Other methods are sampled
only for the qualified host chain. SDK recursion and forwarding bypasses are
excluded. This is not an inventory of every process-wide DXGI call.

The recorder holds no COM references and dereferences no retained pointer
identities. It introduces no GPU commands or per-frame disk writes. The existing
renderer retains its own resources and diagnostics. Calls update fixed arrays
under the existing presentation mutex. There are at most 64 buckets and 512
samples, keyed by phase, method, thread and caller. Each bucket stores its exact
count; samples retain its first four calls and power-of-two checkpoints through
65,536. Saturation is explicit and rejects a complete-map acceptance. Unsampled
intervals do not prove buffer identities or lifetimes stayed unchanged.

Phase 0 covers startup, phase 1 starts at the arm request. QPC observations,
native pointers, caller addresses/RVAs and results are emitted after presentation
stop/drain. Caller RVA zero identifies calls outside the qualified main image;
those remain in the map but are excluded from the game-owner inference. The
existing Reflex stage samples link the game presentation thread to frame tokens.
No new token is created by this diagnostic.

## Verification before game deployment

The /W4 /WX build passed. Six native contract tests passed, including sample and
bucket saturation. Fifty-eight Python review tests passed, including rejection
of false counts, incomplete sampling, wrong caller classification, unordered
observations and an undrained recorder.

The exact ownership fixture completed 144 Reflex On frames, 864 markers, two
resizes and one chain recreation. It mapped 305 calls into 17 buckets / 55
samples with no drops. All twelve hook heads restored; the driver confirmed Off,
SDK shutdown and pin release completed, native rendering continued afterward,
and final device references were zero. It is a Reflex-only fixture, without a
GPU validation layer or generated presentations. Evidence is under the ignored
`local/frame-generation-game-20261002/owned-ownership-01` folder.

The new game reviewer first validates the v0.6.0 scope and bounded trace, then
reuses the unchanged v0.5.1 combined renderer checks through an explicit identity
normalization. It also requires that the observed game Present owner agrees
with the Reflex frame owner. Buffer, camera, depth/motion, final post-processed
HUDless color, UI alpha and FG consumption lifetimes remain separate work.

## Runtime procedure and removal

Deploy through the existing MO2 diagnostic slot with the game closed. Preserve
the exact DLL and output sidecar before replacing either. Keep its existing
Streamline/NGX runtime configuration, profile and graphics settings. Runtime
evidence, source freeze, digests and removal instructions belong under the new
repository-local run folder. Do not restore a previous diagnostic automatically;
the user requested keeping the current DLL installed between tests.

Launch into the main menu with native AA/DLSS Off and the same window size,
arm once, load a save and keep the game open. After observing one area-load or
focus transition, stop intentionally; review both renderer channels and the
ownership map, then independently read back native hook heads/functions while
the game remains open. Ask for normal exit after verification. For removal with
the game closed, restore the preserved slot DLL and exact output-sidecar bytes
specified in that run's deployment receipt. Original game files are untouched.
