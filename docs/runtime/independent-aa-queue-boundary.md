# Independent AA: post-processing queue boundary

2026-09-26, observer v0.1.0. Built for a new callback-level AA-Off control after
the external resource observation. The AA controller remains **unconnected to
the renderer**. This DLL forwards the native phase and records eight pairs;
it neither enqueues a new pass nor changes AA flags, jitter, resources, GPU work
or SDK state. The queue contract/admission code is a foundation for the owned
pass, not an implemented exclusive-AA renderer.

## Why this boundary

The camera render loop at RVA 0x228e770 performs projection, resets a post-effect
manager, traverses native layers, then calls the manager phase at 0x24f1fc0,
return address 0x228e92d. The manager sorts descriptor pointers, assigns each
descriptor's input/output owners, then invokes its unit's vtable+0x60. TAA's
descriptor is embedded at layer+0x140; its input/output fields are +0x150/+0x158.
Those fields are therefore filled by the queue in the deferred path rather than
solely by the initial TAA Execute call.

The [eight-frame live control](../research/independent-aa-queue-boundary-result.md)
passed with TAA/FXAA Off: the queue executed with four descriptors and no known
native AA unit, then disabled/drained cleanly. GPU input validity and an owned
pass remain unverified.

This is a candidate route for an owned pass independent of the user's TAA choice:
register the pass at the appropriate phase/order, let the engine assign its
current color endpoints, and supply DLAA/unfiltered output. It could avoid forcing
native layer flags or changing the TAA option table. Queue availability and
ordering are observed in that loaded scene; owned-callback conditions and
GPU owner lifetime remain unestablished. No
claim that merely registering a node gives valid temporal inputs follows.

The preparation helper qualifies room under a conservative 16-descriptor budget,
collecting phase, no duplicate owned descriptor and no known native AA units.
It does not mutate the queue or use TAA/FXAA option bytes as an eligibility test.
The 16-node bound matches the observed layout between +0x40 and +0xc0; it remains
a diagnostic bound, not proof of the native enqueue routine's capacity checks.

## Hook contract

Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded comparison SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Both were rehashed before static discovery. The live host identity and both
complete code spans must match before installing the hook:

| Function | RVA | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| Post-effect phase | 0x24f1fc0 | 835 | `1ec618caf8d1d455cc12fc12c57889b73e1073074faf04e82364fba10d01e190` |
| Camera render loop | 0x228e770 | 528 | `33ae7df95058ac2e1ce8d920ff89c62d664068a975931bb10afb95ea88f00b97` |

Windows x64 ABI: RCX=post manager, RDX=render context, R8=view. The wrapper
preserves volatile integer/vector arguments and return registers via the existing
unwind-described forwarding helper. Only the known caller and primary view/context
are admitted. EFLAGS are volatile in this ABI. Original exceptions propagate;
`__finally` completes capture accounting without calling the original twice.

All engine object addresses are borrowed only while this synchronous native phase
executes. Bounded scalar records are copied into DLL-owned storage. No engine
object is pinned or mutated. Two equal snapshots are required on each side;
frame/epoch/view/context/rectangle must also match across the original call.
Sorting may reorder descriptor entries, so review compares sets and each
descriptor's identity separately. Pointer observations are not GPU lifetime proof.

The DLL reserves a MinHook block at startup, waits up to 30 minutes for its
process-generation-specific arm event, and then performs existing D3D12 discovery
to log adapter LUID, swap-chain format and dimensions. It installs one native
hook. It stops after eight pairs, a read/reentrancy/identity failure, explicit stop,
or a five-second capture deadline. It disables the hook and drains callbacks;
DLL/trampoline memory remains pinned. No render-thread file writes.

## Verification and use

Release `/W4 /WX` build and targeted CTest checks pass: existing AA selection,
queue bounds/missing bytes/duplicate and native-AA rejection/frame matching,
and 1,000 wrapper forwards plus native exception unwinding. Python arm tests
reject stale process generations, wrong DLL/scope, reused arms and finished runs.
The live control passed CPU queue review; it does not qualify GPU inputs or DLAA.

Evidence root: ignored `local/independent-aa-scheduler-20260926/`. The authoritative
complete static spans are in `boundary-gates.json`; earlier exploration maps
contain partial unwind ranges and are not hook gates. Deployment/frozen sources,
build/test logs and later capture receipts are recorded in the version directory.

Use the usual bare offline MO2 test profile, Windowed 2560x1440, DX12 On,
**native DLSS Off and AA Off**. Load a save and leave the game open; no panning,
toggle, phone reply or menu transition is needed. After checking startup identity,
signal `tools/arm_aa_boundary.py` with the exact log, DLL hash and fresh receipt.
Review all pairs and final disabled/drained state, then request normal exit.

The exact installation destination is the existing local MO2 test slot documented
in `deployment.json`. Preserve its previous DLL and `.dll.output-dir` state.
After game exit, `restore.py` in the version directory verifies deployed identity,
restores that exact prior DLL, and removes only the sidecar created for this test.
Do not touch the game executable or original assets. Current graphics/profile/
mode files stay unchanged throughout preparation and rollback.
