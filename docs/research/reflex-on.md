# Reflex On diagnostic, 2026-10-02

v0.4.2 preserves the [accepted Off frame sequence](reflex-live-frame-result.md)
and its hash-qualified native/presentation hooks. It starts with Reflex Off,
waits for explicit arm and requests `eLowLatency`. The accepted v0.4.1 baseline
captured thirty seconds; v0.4.2 extends only the capture ceiling to ten minutes
so one area/load transition and one isolated focus cycle can be checked before
an explicit stop. Frame policy, hook targets and SDK calls remain unchanged.
Frame limiter is zero, Boost and marker optimization are not requested. No
reconstruction, render-resolution, jitter or native AA setting changes.

Options change before native frame hook installation. The first five-second
checkpoint reads the driver's actual low-latency flag, serialized with the
frame callbacks. After the final complete frame, hooks/routes disable and
drain; another readback confirms On. Options explicitly return to Off and a
final readback must confirm Off before SDK shutdown/pin release. Off confirmation
waits at most one second/101 read-only queries and records the initial flag,
final flag, query count and elapsed time. Setup errors
also attempt Off after callback drain, including a partially failed On request.
Unverified restoration retains the SDK/device pins and fails review.

`slReflexGetState` exposes availability/reports, not an active-mode field. The
`driver_state.mode` record is explicitly marked as **requested**. Actual mode
comes from the public read-only `NvAPI_D3D_GetSleepStatus` query on the pinned
native device. A minimal MIT-licensed public ABI subset is kept in
`third_party/nvapi_sleep_status.hpp`, pinned to NVIDIA/nvapi commit
`70d337db9186e968eab622f7e786de7e437faf3d`. Query ID `aef96ca1`, V1 size 136,
version 65672, low-latency flag offset 4. It borrows the SDK-initialized driver
context and temporarily retains its already-loaded module; no driver loading,
NVAPI initialization/unload, driver option writes or extra runtime deployment.
Full public-header reference hashes are in the ignored provenance receipt.

References: [public status definition](https://github.com/NVIDIA/nvapi/blob/70d337db9186e968eab622f7e786de7e437faf3d/nvapi.h),
[query interface](https://github.com/NVIDIA/nvapi/blob/70d337db9186e968eab622f7e786de7e437faf3d/nvapi_interface.h),
[pinned Streamline mode mapping/state implementation](https://github.com/NVIDIA-RTX/Streamline/blob/e8aaa6eaac968711fb62473d4ae8256dde20919b/source/platforms/sl.chi/generic.cpp).

## Owned verification

The initial immediate-On readback assertion rejected before frame calls. A
second diagnostic completed 144 frames and confirmed On after drain; a third
recorded Off immediately, after the first sleep/Present, and at frame 8, then
On at frame 32 and after drain. Off restoration readback was immediate. This
is an observed reporting delay; its internal driver cause is not established.
The game probe therefore records the immediate requested-On readback without
requiring its flag yet, then requires On at five seconds and after drain.

The final owned On test passed 144 tokens/sleeps, 864 markers, Present/Present1,
TEST exclusion, LastError preservation, two resizes, swap-chain recreation,
restored native rendering, SDK shutdown and zero final device references. Off
and the default presentation regressions also passed. SDK logs have no
warnings/errors or delayed resource references. Three native ABI/policy/COM
tests and twenty-two strict-review tests passed, including failed mode readbacks,
wrong device/ABI identity, early/missing restoration, changed options and
driver/token alignment. The accepted 5,949-frame game Off trace still passes.

## Game test and removal

The [first real On run](reflex-on-first-result.md) confirmed 6,003 complete
sequences and actual driver On, but rejected immediate Off/shutdown confirmation.
It is a partial milestone, not a full cleanup pass. The [v0.4.1 follow-up](reflex-on-off-wait-result.md)
completed 6,449 frames without faults, but its 95 queries over one second
still reported effective On after accepted Off. SDK shutdown/pin release
remain unverified for that run. The user confirmed a per-game Ultra override
and manually changed it to Off. The [same-binary controlled repeat](reflex-on-accepted-result.md)
passed 6,298 complete frames, 37,788 markers, 64 aligned reports and zero faults;
effective Off was immediate, SDK shutdown/pin release passed and all native
code was independently verified restored. This is the first accepted real On
baseline; neither earlier rejection is accepted retroactively. It strongly
supports the override explanation. Requested application mode and effective
driver policy must be separated in future production teardown; the current
strict diagnostic intentionally still requires effective Off. No latency
benefit has been measured. The [v0.4.2 transition test](reflex-on-transition-result.md)
then passed one area/load change and an isolated focus cycle: 30,224 complete
frames, 181,344 markers, zero faults, matched final driver reports and verified
Off/SDK/native cleanup. Coexistence with the temporal renderer remains next.

For the controlled diagnostic, start normally through MO2, solo/offline, native AA
and native DLSS Off, fixed window size; load one save and leave it open. Arm
with `tools/reflex_frames.py arm` using the new process-specific log. Leave the
game open after the explicit stop (or ten-minute ceiling) for strict review and the
independent query/VM_READ check: seventeen original hook heads, five full
native functions and graphics identities. Readbacks must show baseline Off,
running/final On, restored Off; sixty-four ordered driver reports must match
the last sixty-four completed integration tokens. This is mode/sequence
verification; a latency improvement has not been measured.

Evidence and exact source freeze live under `local/reflex-on-20261002`.
The deployment receipt records the MO2 destination and rollback: with the
game closed, restore `previous-slot.dll.disabled` and
`output-sidecar.before.bin` to their recorded destinations. Runtime sidecar
and game/profile settings remain unchanged. Keep this diagnostic installed
between tests as requested; older archives/freezes are preserved.

## Live transition check

Keep the user-set per-game Low Latency Mode Off for this controlled comparison.
Arm in a loaded save, confirm running On at the first five-second heartbeat,
then change areas through one loading screen and walk/pan for ten seconds.
After a healthy checkpoint, keep the camera still, Alt-Tab away for five seconds,
return and wait ten seconds without travel or resizing. Record each checkpoint
before stopping. The ten-minute ceiling does not imply a ten-minute minimum.
This test changes no render resolution, AA, DLSS or driver profile settings.

The v0.4.2 reviewer requires matching 600-second ready/armed receipts while
retaining the five driver readbacks, bounded Off wait and full native cleanup
checks. Older versions still require their original 30-second receipts.
The actual [transition run passed](reflex-on-transition-result.md), with the
baseline, area-load and focus checkpoints preserved as verified log prefixes.
The earlier static baseline alone did not establish this transition coverage.
The duration change builds with `/W4 /WX` and zero new project warnings. All
24 strict-review tests and three native policy/ABI/COM tests pass. The accepted
5,949-frame Off and 6,298-frame On traces still pass; both earlier failed On
shutdowns remain rejected. The unchanged owned fixture binaries and their
saved successful evidence were hash-verified without repeating those runs.
