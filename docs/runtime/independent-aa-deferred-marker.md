# Independent-AA deferred marker v0.3.0

Purpose: connect the proven owned post callback to native DX12 command recording
without selecting or running native TAA. Eight unique, process-lifetime requests
at phase 15/order 2 carry immutable source frame/epoch/bank and color/depth/motion
identities. This test publishes an empty native parameter packet; it does not
evaluate DLAA, add private GPU commands, write jitter or change AA options.

## Native contract

Tested executable version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded analysis image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
The original game executable/assets are never patched. RVAs below are supported
only for that exact build, with runtime code hashes checked before installation.

Two MinHook targets use the same tested Windows x64 forwarding assembly:

| Target | Arguments | Scope |
| --- | --- | --- |
| `24f1fc0` | RCX manager, RDX frontend context, R8 view | Existing owned descriptor insertion and exact removal; caller must be `228e92d` |
| `259e1e0` | RCX executor, RDX backend context, R8 packet | Match only our eight exact packet addresses and immutable generations; always forward original |

No game virtual tables are modified. The mod unit has only its `+60` CPU callback
slot populated. Descriptor/unit/packet storage is unique per sample and retained
until process exit. Retiring the post descriptor does not retire its packet.

The packet header is opcode 11, preparation link at +8, count at +10 equal to zero.
Private cookie/generation fields occupy the otherwise unused tail. Native
preparation `229cee0` zeroes +8 and its opcode-11 case at `229e921` skips the
parameter loop when count is zero. It retains this entry in the prepared list.
The DX12 dispatcher calls executor vtable +78, which maps to `259e1e0`.
That consumer assigns recording+158, processes its preparation link, flushes
pending native barriers, then skips all parameter writes for the empty packet.
Our marker is never placed into frontend+1a8, where native publication could
coalesce more parameters into it. Admission requires that cache already be null.

The generic enqueue leaf `23cabf0(context, packet)` returns a queue-entry address,
not the packet. Its sort key is `(context[154]<<27) |
(((context[154]>>5)+(context[15c]>>5)) & 07ffffff)`.
Our callback scopes +154 to `(2<<5)|15` and clears only the order portion of +15c;
both original fields are restored in `__finally` and checked after publication.
It adds exactly one entry. Front/back boundary +2b84 and capacity +2b88 are read,
with at least 128 spare entries required before insertion. Only writable,
bounded queue/context storage is touched. No entry address is used as a deferred
identity: native sort/finalization can move entries, while the packet remains fixed.

Additional full byte gates (lowercase SHA-256):

| RVA / bytes | SHA-256 |
| --- | --- |
| `23cabf0` / 99 | `5c63fd6c587c250e0fb09f02c2cf776fb44ed90944e90d1b98c80a700036d433` |
| `229cee0` / 7756 | `11f7cc4d48598967f439659c87142839c4ad3fb900cfafb7fab3631655691af3` |
| `23d2370` / 1047 | `0cf82d2b8b6259fbb5cad0c7468a780bb7026d9a20f720e9bbd8b5cd501fffc3` |
| `23c3480` / 265 | `e1dfbdd44786bac3a9b62e900b3fb4a844dcd4c839425a3753899087e44e0245` |
| dispatcher body/table `22986b3` / 1125 | `80408f28b51d0fc50cfc0bab29eb961cdec4bf20da559b675fd12c7169029765` |

Existing discovery also gates the complete `259ba50`, `259e1e0` and related
native methods; owned-pass gates cover scheduler, caller and descriptor sorter.
The dispatcher interval is explicitly a body/table span, not a whole function.

## Evidence and lifetime limits

The publisher captures color from the owned in-place descriptor, scene depth
from view+628/container+A0, and motion candidate from view+620/container+78.
Owner fields are read twice and actual D3D12 resources are pinned and described.
Each must be a distinct full-size 2560x1440 single-mip/slice/sample Texture2D
on the discovered device. Color must be R11G11B10_FLOAT; depth/motion formats
are recorded for qualification, not assumed correct from their names.

At consumption, executor+8 identifies recording and recording+20 identifies the
real command list. QI/GetDevice/GetType must confirm the same device and DIRECT
type. Owner identities must still match the producer before and after forwarding.
The native parameter table's bounded 0x2400 bytes are compared across the call;
raw table contents are not logged. The current packet and drained barrier vector
are checked after the call. The global renderer counter is logged separately;
it cannot relabel the immutable source frame if it has advanced.

The request state claims one consumption atomically, rejects duplicates and never
reuses a slot. Resource pins last through diagnostic CPU consumption and hook
drain. **This is not GPU lifetime validation**: no private GPU command references
these resources, so pins may be released after CPU callbacks drain. A future
GPU pass must add command-list generation, submission completion and retirement
before reusing/releasing its resources. GPU states, pixel freshness, projection
jitter and temporal alignment are not certified by this diagnostic.

On rejection, stop publishing; leave queued empty packets valid, preserve all
native forwarding, remove post descriptors, disable hooks and drain callbacks.
Do not remove the queued packet or rewrite foreign queue mutations. DLL and
trampolines remain pinned even after the log closes.

## Offline checks

Release build uses /W4 /WX. Six native tests cover policy, descriptor graph and
scope restoration, ABI forwarding, marker identity, sort placement and capacity
boundaries. Twenty-seven Python checks include strict log/reviewer rejection of
partial captures, duplicate generations, binding mutations and stale identities.

Ignored verification under `local/independent-aa-deferred-20260926/` includes
96 executions of the hash-pinned enqueue leaf in private memory and eight
executions of the real consumer plus preparation helper in private allocations.
Four empty-packet cases preserve all parameters; four one-parameter positive
controls make exactly one expected write. They record no GPU calls and open no
game process. Static checks verify the preparation case and executor vtable map.

## Runtime procedure and exact rollback

Keep bare offline MO2 profile, DX12 On, Windowed 2560x1440, native AA Off and DLSS
Off. Launch the existing test entry and load a save. The DLL reserves startup
hook storage and waits up to 30 minutes for a process-generation-specific arm
event. Nothing is hooked before arming. Arming captures eight owned callbacks
within five seconds, then allows up to two seconds for deferred consumption.
No camera movement or settings toggle is needed.

Use `tools/arm_aa_deferred.py` with the startup log, exact deployed DLL hash and
new receipt path. The event is `Local\\MhwAaDeferred-v1-PID-CREATION-1` (stop=0).
Review with `tools/review_aa_deferred.py`. Require eight unique complete
requests, original queue restoration, unchanged native parameter bindings,
released diagnostic pins, no fault and both hooks disabled. Save the capture
before asking the user to close normally.

Versioned deployment evidence: `local/independent-aa-deferred-20260926/v0.3.0/`.
It freezes sources, tests, verification, and the exact previous DLL. The existing
mode/configuration files are preserved. The generated output-dir sidecar is the
only new installation file. After verifying game absence, run that directory's
`restore.py`: it checks current test/backup/sidecar hashes, restores the previous
DLL byte-for-byte and removes only the matching test sidecar. Configuration
hashes are reported; settings are never silently overwritten.
