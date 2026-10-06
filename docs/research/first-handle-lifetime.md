# First accepted copied-handle lifetime

Run **32832**, v0.7.0, on 2026-09-07 accepts the first successful diagnostic
Create, validation of the core-owned copied handle at Release, and Release
before both provider Shutdowns. **Evaluate was never called.** This passes the
handle-lifetime milestone but leaves the full
[one-Evaluate diagnostic](one-evaluate-capture.md) pending.

## Evidence identity

The user reported completing the requested short title-screen test. The exact
menu state on reopening and visual outcome were not specified. The runtime
record, rather than a presumed UI result, supports the conclusions below.

- Archived log: `artifacts/logs/bridge-32832.run001.v0.7.0.log`, 61,022 bytes.
- Log SHA-256: `72F876097D135EBDA39FC51EE3A5DC6F0B580F346FC7E2F3E4F3F374B0B0B6EE`.
- Active v0.7.0 DLL reverified: `14E343D0A023270EA786F5FF3D5584EC58632CBB88727BAD3B902F53C56CD90F`.
- Start record: recognized `capture-one-evaluate`; ISP effective mode remains
  `capture-create`. Both provider routes, synthetic Init and Populate completed.
- The log verifies game SHA-256
  `C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`,
  the pinned modern/legacy cores and cached providers, and an RTX 5090 adapter.
  Create verifies a real D3D12 command list. This bridge run does not establish
  resource or swap-chain formats.

The archived log was hash-checked against its source. Its local structured
assessment is `local/evaluate-20260907/runs/32832/accepted-run.json`; the bounded
assertion script is `local/evaluate-20260907/accept_handle_run_32832.py`.

## Observed lifetime

| Sequence | Boundary | Result and evidence |
| --- | --- | --- |
| 68 | DLSS Create, core `+0x67DE6` | Success `0x1`, feature 1, verified command list, token `0x000000010000B12D`, transition to evaluate-token-issued |
| 69, 70 | Sizing after Create | Both succeed; sizing remains available while the token is outstanding and before Evaluate |
| 71 | DLSS Release, core `+0x8AB7` | Success `0x1`, released state; all four copied-handle checks true |
| 72 | Sizing after Release | Admission rejected; zero queries, writes or readbacks; no getter fault; boundary clean |
| 73, 74 | DLSS and ISP Shutdown, core `+0x43815` | Both succeed, both inert, no outstanding Init/token cleanup |

Create's 11-query scalar contract matches run 19800: 2560x1440 input and output,
both node masks 1, quality 0, RTX 0, Hint.HDR 1. ResourceWidth/ResourceHeight are
absent with getter result `0xBAD00010`; the other nine fields are present with
result `0x1`. The complete snapshot has no fault.

The Release `bridge.handle` event confirms `read-attempted`, `read-completed`,
`core-create-record-verified` and `copied-handle-binding-matched` are all true.
This validates the bounded record layout, copied payload and creation-command-list
identity check on this live path. The handle was retained for **9,328 ms**
between Create and Release. Release's `token=0` event field means no new token
was issued by that transition; `argument0=0x10000B12D` records the validated
token being released.

There were 12 successful sizing calls, 48 input queries, 48 completed writes
and 48 verified readbacks, followed by the one expected post-Release rejection.
The 28 capability queries completed without faults. Both provider summaries
have zero Evaluate calls, zero suppressed Evaluate calls, zero Scratch calls,
zero dropped events, zero boundary contention and no lifecycle overflow.
No ISP Create occurred. DLSS reports copied-handle-bound true and
handle-read-faulted false.

The trace completed quiesced after **48,984 ms**, with complete two-provider
startup coverage and no dropped resolver events. MHW was closed at inspection;
no matching new MHW crash dump was found in the configured CrashDumps directory.
Settings remain DLSS Off, DX12 On, full screen 2560x1440, with the same settings
file hash as before the run. These facts support clean cleanup, not a claim
that DLSS rendered or that scene stability has been tested.

## Next rendering boundary

A new bounded export from the same hash-recorded captured host is stored in
`local/evaluate-20260907/host-evaluate-caller/`. Function `+0x259DAE0` dispatches
to the DLSS D3D12 parameter wrapper `+0x2383CF0` at call `+0x259DCD2` when the
work item has type 3 at offset +16. Type 4 dispatches separately to the ISP
wrapper. This inspected caller contains no input/output dimension comparison
around the DLSS call. It does not prove which upstream scene conditions enqueue
the type-3 item, nor rule out an upstream 1:1-size gate.

The subsequent [loaded-scene attempts](loaded-scene-evaluate-gate.md) with the
unchanged v0.7.0 artifact also recorded zero Evaluate calls. The user confirmed
that DLSS cannot be changed in game and instead enabled it from the main menu
before reloading a save. The former in-game-toggle suggestion is superseded;
the next investigation is the host's upstream rendering conditions.

MHW ignores Evaluate failure and this diagnostic does no GPU writes, so the
scene can appear incomplete if Evaluate is reached. No extended play is needed.
The existing MO2 process 19460 has the correct inherited mode, verified by this
run; no restart or DLL replacement is needed while it remains open. If it is
restarted normally, use the process-scoped launch command in the runtime guide.
