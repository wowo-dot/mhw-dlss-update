# Reflex with the accepted temporal renderer

Combined probe v0.5.1 preserves DLSS Performance v1.25.44 and DLAA v1.21.5.
The 2026-10-02 DLSS + Reflex game test passed; see [the live result](reflex-temporal-result.md).
Combined DLAA game acceptance remains pending. Owned tests do not establish
game transition stability, visual quality, performance or latency gain.

The first v0.5.0 game attempt initialized both SDKs on the same native device,
but failed `live-frame-anchors` before installing the three Reflex frame hooks.
It completed zero Reflex frames and zero DLSS evaluations. Temporal cleanup,
full render scale, effective Reflex Off and Streamline shutdown passed. Bounded
independent live reads verified 34 created/temporal hook heads and 46 full native
functions restored. The user reported a normal scene and exited normally.
This attempt is rejected, not evidence of combined live rendering.

The old compound anchor check did not identify its failed subcondition. A later
64-sample read found stable engine/renderer/backend/device links and valid banks;
that cannot prove the earlier failure was a counter race. v0.5.1 records separate
global, device, ownership-link and stamp failures. Setup alone may resample a
counter publication observed changing during its guarded reads, bounded by
64 attempts and 100 ms. Unreadable memory, invalid banks, device or link changes
reject immediately. Every retry revalidates the ownership links. Hook-thread
stamp failures still stop the feature immediately; SDK calls are never retried.
The final coherent stamp is logged before any Reflex frame hook is created.
The joined reviewer requires this receipt for v0.5.1 and checks its bounds,
identities, counter coherence and ordering. Pure stamp negatives and joined
receipt negatives pass. The accepted game run qualified on its first read;
it did not exercise retry recovery or identify the earlier failed subcondition.

The presentation worker owns startup, one process-specific arm/stop control and
the ten-minute ceiling. It reserves one shared MinHook game pool, admits early
native D3D12/DXGI creation, and runs the existing NGX initializer before its native
client call. Streamline attaches the native device before queue/swap-chain creation.
NGX's borrowed initialization must validate that exact same device before arming.
There is no second temporal worker and no duplicated queue-submission hook:
the temporal renderer owns submission bookkeeping; Reflex owns queue creation,
native-interface presentation routing and the three accepted engine frame gates.

The target function hashes, Windows x64 signatures, resource interpretation,
jitter/history, native resolve bypass, scaled output/effect handling and SDK
parameters are preserved. The new wrapper does not introduce frame generation,
new quality presets or menu configuration. Both game targets remain qualified
against retail SHA256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
See `reflex-on.md`, `reflex-on-transition-result.md` and the temporal research
contracts for individual hook calling conventions and lifetimes.

Shutdown stops temporal admission and restores render scale while its native
callback remains installed. Temporal callbacks, native hooks, observer hooks,
private recordings and their GPU signals must be quiescent before stopping
Reflex frames/presentation and releasing Streamline ownership. Failed temporal
cleanup retains SDK/device pins; it cannot produce an accepted SDK shutdown.
NGX initialization remains process-lived and does not receive a new Shutdown1.
Independent native-code checks are still required after a game run.

`tools/reflex_temporal.py` validates the new schema-3 combined identity and joins
the unchanged temporal and Reflex reviewers. Both channels must have the same
PID, creation time, DLL hash and native device. Temporal preparation/arming/drain
must occur in the documented order; both channels must complete actual work.
Simulation tokens and reconstruction counts need not equal each other during
loading/non-rendered stages. The older failed Reflex shutdowns remain rejected.

Owned hardware tests use cropped archived inputs (512x256); DLSS reconstructs
256x128 to 512x256, DLAA remains 512x256. They verify actual reconstruction,
converted inputs, downstream pixel/binding checks, Reflex Sleep/six markers,
native routed Present/Present1, 64 ordered driver reports and On->Off shutdown.
One actual unmarked drain Present, matching continuing native rendering after
frame admission stops, publishes the final driver report without a new token.
The final native Present after SDK shutdown is GPU-fenced before swap-chain
destruction. Debug validation includes that destruction. NGX's intentional
process-lived native device pin is distinguished from GPU work left in flight.

Earlier owned attempts are retained under `local/reflex-temporal-20261002`.
The first shutdown experiment exposed an owned-fixture resource-release error
(DX12 message 921 / raised exception 0x87d); the final post-shutdown Present had
not been fenced. It did not establish a game-renderer failure. The corrected
fixture verifies completion of that actual Present. Earlier partial receipts
and rejected driver-report attempts are not accepted merely because some SDK
operations succeeded.

For the first live test, use the existing MO2 profile in solo/offline mode,
native AA/DLSS Off, user-set external Low Latency Mode Off and fixed window size.
Arm at the main menu. Verify both running channels before loading a save;
then verify walking/panning, an area-load transition and a separate focus cycle.
Stop explicitly once evidence is collected; ten minutes is a ceiling. Leave
the game open for strict joined review and read-only native cleanup checks,
then exit normally. Preserve the installed DLL between tests as requested.

Deployment/backups, runtime hashes, source freeze, build/review/owned receipts
and removal instructions are in the new local run folder. Original executable,
assets, AA settings, profile, modlist and driver settings are not changed.
