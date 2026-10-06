# Reflex On area-load and focus transitions accepted

2026-10-02, probe v0.4.2 SHA-256
`4f1e21729f7fc61798cb73f776288c4c09694ca4733eb3c0e56942a31bba4def`.
The only native probe change from the [accepted v0.4.1 baseline](reflex-on-accepted-result.md)
extends the capture ceiling from thirty seconds to ten minutes. Frame policy,
three native targets, presentation routing, Reflex sleep/markers and bounded
Off confirmation remain unchanged. The strict reviewer recognizes the new
600-second contract while retaining older version contracts.

PID 33240, creation 134354219360527733, frame owner 55504;
DX12, windowed 2560x1440, format 87, three buffers, adapter LUID 93911:0.
Native device 354403792, backend 349044864, factory 6810432, swap chain
359419920. Retail executable and full native function qualification hashes
are recorded in the [Reflex On contract](reflex-on.md).
Native AA/DLSS stayed Off, window size stayed fixed, and the user-set external
NVIDIA Low Latency Mode remained Off for this controlled diagnostic. The agent
changed no driver settings. This standalone probe did not enable upscaling.

## Accepted observations

After explicit arming in a loaded save, the driver confirmed On at the first
five-second checkpoint. Three saved log-prefix checkpoints establish the
reported test order; their byte counts and SHA-256 hashes match prefixes of
the final immutable log.

| Checkpoint, local time (UTC+03) | Completed frames | User report |
| --- | ---: | --- |
| Baseline, 16:41:08 | 10,331 | Save loaded |
| Area load, 16:41:53 | 19,566 | Area loaded; scene normal after walking/panning |
| Focus return, 16:42:39 | 29,860 | Five-second Alt-Tab away/return, ten-second wait; scene normal |

All checkpoints have zero frame faults, matched routed/native presentation
counts and an active recorder. The driver readback at these checkpoints is
the earlier running-On sample, not an additional per-checkpoint query. Final
drained On was confirmed separately before requesting Off.

The explicit stop completed **30,224 tokens, 30,224 sleeps and 181,344 markers**,
with zero faults, pending calls or incomplete final frame. Eight complete
six-stage samples pass SDK result/count/QPC/owner/epoch/bank checks. All sixty-
four complete ordered driver reports, IDs **30161..30224**, match the final
sixty-four integration tokens. Off was confirmed immediately after the SDK
Off request: one query, zero measured elapsed milliseconds, within the unchanged
one-second confirmation bound.

SDK shutdown and pin release passed. SDK logs show Reflex, PCL and common
shutdown, zero warnings/errors, and zero delayed resources. Full startup-to-
stop lifetime: 47,103 matched routed/native Presents, five queues, one swap
chain and four resizes. Seventy observer Presents were outside tracked frames
and emitted no synthetic markers. These lifetime resize counts include startup;
they do not establish resizing or swap-chain recreation coverage while On.

Independent bounded query/VM_READ checks while the game remained open verified
all seventeen original hook heads and five full native functions, native graphics
identities and native host swap-chain interface: 355 reads, 25,592 bytes, zero
writes or remote execution. Strict review and independent cleanup review passed.

The user reported normal rendering after stop and normal exit. All four game/
profile/probe configuration hashes stayed unchanged. The current v0.4.2 DLL
remains installed as requested; exact v0.4.1 DLL and both sidecar backups and
removal instructions are retained in the deployment receipt.

## Evidence and remaining scope

The 24-file `local/reflex-on-20261002/game-on-04-archive` and exact 324-file
`source-on-0.4.2` freeze are verified, alongside seven prior archives and six
prior source freezes. Earlier failed shutdown receipts remain rejected.
Archive manifest SHA-256:
`4af93580cb760dcc4b4e88c41a08ae980b9776ab9c8fd984d4acaf191f55a17c`.
Log SHA-256:
`c36d9d4d8fc9a1fe111349c828c84be77d8849eec41c8a6ac3943799a9e4abbd`.
SDK log SHA-256:
`2f7abf58b4a7bb738c344d23c1cb0a2642f659c6103a23e923cc5d098df28fa9`.
Source manifest SHA-256:
`626493356451221b5bf2ad156a66ea84f88d628b16641358ded0fae1ac4e2e6d`.

This accepts the tested area/loading and isolated focus transitions with Reflex
On. It verifies mode, frame sequencing and teardown; latency improvement was
not measured. Coexistence with the preserved DLAA/DLSS renderer remains the next
integration step before ordinary frame generation and later MFG. Live resizing/
device recreation, Boost, override-aware production teardown and production
settings remain outside this result. Presets/UI/performance cleanup stay deferred.
