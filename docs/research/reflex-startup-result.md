# First Reflex startup/device result, 2026-10-02

The v0.1.3 one-shot startup test passed in the actual DX12 game process. It
initialized the pinned Streamline 2.12.0 Reflex/PCL runtime, attached the game's
main native device, confirmed Reflex availability in Off and shut down before
returning to game initialization. Both temporary API hooks were restored.
This establishes startup and device compatibility. In-game sleep, frame
markers, presentation proxies and latency reduction remain unimplemented.

## Runtime evidence

Process PID 716 / creation `134354004477108115`; owner thread 13596. Executable
version `0.0.0.0`; retail SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Probe SHA-256:
`80d0b986326b5c5c4535645598120c0d42fed3f94d34effa1a5c5a0cc3bf443d`.
Qualified device caller RVA `0x23a4e2e`; feature level `0xc000` (12.0);
adapter LUID 93911:0. The untouched native swap chain was 2560x1440, BGRA format
87, three buffers and windowed. Native AA/DLSS were Off.

The accepted order was: load the two system API modules, install the public
hooks, create the native main device, initialize the SDK in manual mode,
attach/check Reflex Off, shut down, restore hooks, observe the native swap
chain. Initialization after native device creation is explicitly permitted by
the pinned manual-hooking guide. It occurs before returning to this verified
initializer's queue/swap-chain creation; it does not claim the earlier timing
recommended by the standard Reflex guide.

The strict reviewer accepted the complete lifecycle: one qualified device,
zero pending callbacks at stop, zero sleep calls, zero frame markers and no
game proxy installation. No qualified factory call was observed or required.
The SDK log contained no warning/error records; Reflex, PCL and common each
shut down, with zero delayed resources remaining.

Independent query/VM_READ checks compared both 32-byte API heads against their
retained originals in the verified probe module. Both matched. The complete
native constructor and DX12 initializer also matched their expected hashes
(4,158 and 11,101 bytes). The current native device matched the device checked
by the SDK; native factory storage matched the public factory export. The
external check performed 171 bounded reads, 18,227 bytes, no writes or remote
execution. The user exited normally; graphics/profile/mode/mod-list snapshots
retained their hashes. The installed DLL and its sidecars remain in place.

Runtime log SHA-256:
`a5cdb1a5f94ecd23d51d1ebc014440e68fe6470d48f754106f1f3158cd834d81`.
The 301-file frozen-source manifest SHA-256 is
`7e0d39fe3374e1879af0061f407fefc088f98d5beb0b50c32489df60480b770e`.
Evidence is preserved separately under
`local/reflex-startup-20261002/game-startup-03-archive/`. The still-selected
`game-startup-03/` output directory is not sealed against future launches.
SDK binaries, captured native assembly and runtime evidence remain ignored.

## Remaining integration

Persistent SDK operation needs correctly owned presentation interfaces and
common presentation maintenance on every actual frame, including recreation
and resize. The tested frame-token adapter then needs verified input/simulation
boundaries, render-submit markers and actual-present markers. Earlier native
trace order alone does not prove SimulationEnd: engine return follows Present.
Ordinary FG/MFG follow that complete contract.

The owned startup fixture and older ownership/resize fixture already pass; they
do not substitute for this persistent in-game lifecycle. See
[startup contract](reflex-startup-device.md), [foundation](reflex-foundation.md)
and [native frame map](reflex-frame-boundary-result.md).
