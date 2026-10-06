# First real Reflex On: activation confirmed, shutdown rejected

2026-10-02, probe v0.4.0 SHA-256
`5bc33c340799fd172bac78185951675bd9ab369511794963e4e87bbc9aecf701`.
Retail executable and qualified hook/SDK identities remain those in
[the On contract](reflex-on.md). PID 6608, creation 134354144771592127,
owner thread 54880; DX12, windowed 2560x1440, format 87, three buffers,
adapter LUID 93911:0. User was asked to keep the game focused for this baseline.

The thirty-second capture completed **6,003 tokens, 6,003 sleeps and 36,018
markers**, zero frame faults, no pending/incomplete final frame. Eight complete
six-stage samples passed SDK return/count/QPC/owner/epoch/bank checks. Driver
readbacks reported baseline Off, then On immediately after the request, at the
five-second checkpoint, and after frame/route drain. The sixty-four complete
ordered reports had IDs 5940..6003, exactly the final sixty-four owned tokens.
This confirms real On mode and sequencing, not a measured latency benefit.

Longer startup-to-stop presentation lifetime had 30,918 matched routed/native
Presents, five queues, one swap chain, four resizes, zero faults. Sixty-four
observer Presents had no tracked frame and generated no synthetic markers.
Independent query/VM_READ inspection verified all seventeen native hook heads,
five full functions, native graphics identities and native host swap-chain
interface: 355 bounded reads, 25,592 bytes, zero writes/remote execution.

**Full acceptance failed at shutdown.** The SDK accepted the explicit Off
request, but its immediate readback still reported On. The probe intentionally
retained SDK/device/interface pins and did not run SDK shutdown. No later
in-process driver readback was obtained; it is not established whether the
mode flag would have become Off after waiting. The final log's
`game_reflex_active:false` field is therefore not verified and must not be used
as evidence of Off restoration. SDK logs had no warnings/errors, but contain
no shutdown/resource-release milestone. Strict review rejected `SDK shutdown`.

User reported a normal scene and exited normally. Separate partial-review and
rejected-hook-restoration receipts preserve the distinction between working
On sequencing, restored native code, and unconfirmed vendor teardown. Evidence
is under `local/reflex-on-20261002/game-on-01`; the exact 321-file v0.4.0 source
freeze remains unchanged.

The next correction waits up to one second/101 read-only queries for Off after
callback drain, recording initial/final flag, query count and elapsed time.
SDK teardown still requires confirmed Off. A bounded reporting delay is a
plausible explanation given the earlier owned activation observations; the
game-specific cause of this failed immediate Off sample is not established.

The [v0.4.1 follow-up](reflex-on-off-wait-result.md) also failed confirmation
after a full one-second wait. A read-only driver-profile inspection found a
per-game one-frame queue setting; NVIDIA documents that the status flag is
effective driver mode and may be overridden. Exact profile mode and the
missing pre-activation effective baseline are under investigation. The
earlier shutdown rejection remains preserved.
