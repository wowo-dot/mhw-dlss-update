# Native Reflex boundary traces, 2026-10-02

The 15-second main-menu and loaded-gameplay traces passed. Both establish the
observed native frame order in their captured scenes; neither enables Reflex
or proves input-to-frame alignment. The signed Streamline owned-device fixture
remains a separate SDK test.

## Identity and result

Probe v0.1.0 SHA-256:
`7c36460b5c26a79832cbd5d37b1589d4ba53b12ddfd17addbc0860c903141d6c`.
Retail executable version 0.0.0.0, SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
PID 40084 / creation 134353920787710229. DX12, 2560x1440, three swap-chain
buffers, DXGI format 87, adapter LUID 93911:0. Native AA/DLSS stayed Off.

The log contains 50,537 complete entry/return call pairs. Engine update, bank
advance, publication, renderer maintenance, native Present and actual API
Present each appeared 3,596 times; ExecuteCommandLists appeared 28,961 times.
Every observed boundary ran on thread 50120. Zero exceptions, dropped records,
unstable stamps or pending recorded callbacks remained at stop.

Independent interval analysis verified 3,595 complete engine intervals:

1. Engine update enters with the preceding epoch/temporal/bank.
2. Bank advance increments epoch and temporal by one and toggles bank.
3. Command recording/publication uses that new stamp.
4. Five game-origin GPU submissions use the same stamp and finish before Present.
5. Native Present contains one actual Present call (sync 0, flags 512).
6. Two to four additional GPU submissions from non-game callers occur inside
   the API Present call.
7. API Present, native Present and finally engine update return in that order.

The first presentation precedes the first captured engine entry. The final
engine interval reaches publication/submission but its presentation occurs after
recording closes. Neither edge is included in the complete-interval proof.

## Consequences for implementation

Engine-update return cannot be SimulationEnd: it occurs after presentation.
ExecuteCommandLists alone cannot define the game rendering interval because
other components submit during Present. Neither observation is an SDK fault.

The decoded, hash-verified call chain is:
`1af2b60 -> 229a080 -> 22980b0 -> 23a7f50 -> 23aa290 -> 23a9ce0 -> Present`.
These are sequential calls within the engine routine; the arrows do not imply
that publication calls the handoff routine itself. `229a080` records rendering
commands. `22980b0` includes native waits/maintenance, backend submission and
presentation. The narrower `23a7f50` backend function returns before presentation.
Simulation/input semantic boundaries and pipeline overlap still need confirmation.

The second trace used the same observational DLL in a loaded save and confirmed
the frame-counter relationship and CPU/submission ordering under gameplay work.
Real Reflex integration still requires correctly placed markers, a sleep call
before consuming input, shared frame tokens, and Streamline presentation
maintenance on the game's actual device/swap chain. No latency gain is claimed.

## Cleanup and evidence

The worker stopped automatically after 15 seconds and restored the hooks.
External live reads matched five full native functions to their SHA-256 gates
and seven original 32-byte heads to the probe's retained pre-install heads.
The Present disk comparison differed, while the retained-original comparison
passed. Preserve both observations; do not misclassify the disk difference as
our detour remaining installed. The retained-array lookup inspected only bounded
metadata in the verified probe's writable section and emitted hashes, not a dump.

The user reported normal game exit. Keep the current diagnostic DLL installed.
The v1.25.44 DLSS baseline and exact pre-install sidecar are preserved separately.
No resolution, jitter, AA setting, vendor SDK state or GPU rendering was changed
by this observational run. One unsubmitted owned queue was used to discover the
concrete ExecuteCommandLists method.

Ignored evidence: `local/reflex-foundation-20261002/game-trace-01/`:
`review.json`, `interval-analysis.json`, `live-cleanup.json`,
`retained-head-cleanup.json`, process log, preparation/deployment and source
provenance. Log SHA-256:
`dd1d3a2d9f509d8861a9bf6cffbb2eb064db9f49f06174dbd5b7858585ef8e34`.
All code/disassembly/runtime binaries remain local and untracked.

## Loaded-gameplay trace

The unchanged v0.1.0 probe recorded PID 41268 / creation 134353931435000686.
DX12 identity stayed 2560x1440, format 87, three swap-chain buffers and adapter
LUID 93911:0. There were **40,384 complete call pairs**, **2,821 actual presents**
and **2,820 complete engine intervals**, all on thread 47856. No exceptions,
dropped records or unstable stamps were observed.

Every complete interval followed the main-menu ordering above, including five
game-origin submissions at the same five callers. Two to four non-game
submissions occurred inside API Present. The first captured presentation
preceded the first engine entry; all captured engine intervals were complete.
This corroborates the counter/bank relationship in gameplay without proving
the precise input or SimulationEnd boundary.

External read-only checks again verified all five full native functions and
seven retained original heads after stop. A bounded subsystem reader also
verified 58 engine fields and their vtables for offline input/update mapping;
its output explicitly leaves semantic roles unproven. The user exited normally.
All eight prelaunch files and 291 frozen observational source files retained
their recorded hashes. Graphics settings, profile settings and diagnostic mode
were unchanged. The post-run MO2 mod list added the enabled effect mod
`Xiemo_Greatsword_Wyvern_Ignition_Fatal_EFX`; its activation time relative to this
capture is unknown. That user change is preserved.

Evidence: `local/reflex-foundation-20261002/game-trace-02/`, including
`review.json`, `interval-analysis.json`, `retained-head-cleanup.json`,
`gameplay-subsystems.json`, `run-result.json` and `run-evidence-manifest.json`.
Log SHA-256:
`e7560f616da0225b25a0896003e49100882c62a1ad1b2c80afd55aaea48573b6`.
The installed DLL and its output sidecar are unchanged. This folder is not
sealed while the installed probe still selects it; a later deployment must use
a fresh output directory.

The new pure ordering policy accepts all 6,415 complete intervals from both
traces, rejects corrupted/truncated replay input, and ignores TEST presents.
The borrowed-SDK adapter passed a separate owned-device run: 288 completed GPU
frames/tokens/sleeps, 1,728 markers, 192 ordered driver reports, three modes and
exit code 0. `owned-test-03/` stores its log, review, executable and provenance;
`source-frame-session-20261002/` stores a separate source snapshot. The fixture
uses synthetic stamps and performs no game-process access. Its success does not
enable Reflex in the installed observational DLL or establish a latency gain.
