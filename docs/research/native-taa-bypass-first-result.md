# First native TAA bypass run: working replacements, request-lifetime fault

2026-09-27, v1.19.0, PID 53628 / creation 134349367051776333. The user reported a
clean image while moving and changing areas, followed by DLAA stopping on an area
change. They clarified that they had already visited the sauna and it was mostly
fine, but wanted more testing. The sauna was not separately checkpointed, and an
exact loading-screen/focus sequence is not established. This run is **not an
accepted transition test**.

## Confirmed observations

The first 1,793-frame checkpoint and later 5,316-frame movement/area checkpoint
were active, fault-free and passed SDK, history, request, retention and resolve
reviews. They reported seven and 31 unclaimed native candidate dispatches,
respectively. Every claimed resolve at those checkpoints was replaced by DLAA.

Final telemetry records **5,703 claimed resolves, 5,703 successful reconstructions
and 5,703 SDK evaluations**, with no rejected shader, failed claimed resolve,
unfiltered recovery or SDK failure. There were 94 unclaimed native candidates
among 5,797 observed candidates. These counters cover the qualified dispatch
boundary, not every possible AA pass in the renderer. No objective pixel readback
or performance measurement was made during this run.

The diagnostic stopped with **fault 6: request-slot collision**:

- Slot 9 still held frame/epoch 26473 when frame/epoch 26505 tried to publish:
  exactly 32 frames later, matching the ring size.
- Only one request remained live. The old request had one recorded consumer
  bypass while one publisher was active; the callback packet pointer matched it.
- The existing publishing guard forwarded that callback without entering the
  replacement path or clearing the request's live flag.
- At collision, the latest evaluated lease belonged to frame 26504. There were
  no changed-packet, multiple-match or reentrant-consumer reports.

This reproduces the earlier request-tracking failure class from the native-TAA
route, now with old/new request and bypass metadata. It differs from the
independent-AA sauna depth-provenance fault 19. The evidence supports a stale
tracking entry after a publication-blocked native callback. That branch records
pointer equality without checking the full packet generation, so it does not yet
prove that retiring the record solely from this pointer match is safe.

## Closure and rollback

All 5,736 private GPU signals completed, all 6,279 jitter injections were restored,
pending callbacks/private recording were zero, and all hooks disabled. The SDK
reported an empty failure string and successful output/binding restoration.
The ordinary success reviewer correctly rejects the run (`clean stop`).

One four-resource recording remained pinned although its Reset and GPU proofs
were complete. `runtime::write` deliberately freezes remaining pins after any
fault, so the healthy retention reviewer rejects these final counts. This is
conservative retention through process exit, not evidence of unfinished GPU work
or a reason to weaken the healthy-run checks. 5,735 of 5,736 recordings had already
released; peak live count was two.

The user confirmed normal exit. Process absence was checked before restoring the
exact previous diagnostic DLL, removing the matching output sidecar, and restoring
the original graphics file. All four configuration hashes match their backups:
AA Off, native DLSS Off, Windowed 2560x1440, DX12, bare profile. No test is installed
or armed; the MO2 slot contains the previous baseline diagnostic.

## Next work

Repair request publication/consumption bookkeeping around the global publishing
guard. Validate packet identity and ownership when a callback forwards native work;
retire only requests whose consumption is established, including producer/consumer
overlap. Exercise those interleavings and the observed 32-frame reuse in owned CPU
fixtures before the next live run. Enlarging the ring or blindly overwriting live
entries would hide the problem. Native preparation and resolve bypass remain the
chosen integration route; CMFM/UI work stays deferred.

## Evidence

Ignored root: `local/native-taa-bypass-20260927/v1.19.0/`.
All 307 prelaunch files and 287 source identities reverified before archival;
327-file final archive, `accepted=false`, fault 6.

- DLL: `aab26314bf1b099cc5dcf48aee23e14a85c1c594cafec2cd4bb3ce4b1ccf0c13`.
- Final archive: `03c3527994b37f26ce3043af8430551585241bc551dd4d6680eb771422c550fc`.
- Input log: `837c032afa317f5ec9ad26928f43a7074e788b9fcf6aa113fbc69e8ed3136555`.
- Startup log: `15d7d2cf37105f65bd164d7e0ccaa6524da696c619a56f018968ff76d609c5fb`.
- Prelaunch: `62abd037c7b0c6aeb5fa0873828f26ce857be321099efacbea370bbf73c8ad91`.

Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
DX12, adapter [0,98931], swap-chain format 87. See `failure-review-53628.json`,
the three checkpoints, separate user reports and `restored.json` in that archive.
