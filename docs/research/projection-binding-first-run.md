# Projection binding run 33032: source captured, correlation rejected

On 2026-09-23, standalone v0.1.0 ran in the user-reported loaded save with
DLSS Off/TAA and the bare MO2 profile. Process creation was
`134346603239471641`; executable hash was
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
DLL SHA-256 was
`F09D96514333A33F6D8B365F4236B02D60FBA0F101CAF8477B582E9E6D577F21`.

## Confirmed observations

All 29 exact-code spans and the runtime D3D12 discovery gates passed; all
20 native/API targets were installed. The device LUID was `(0, 99257)`.
The swap chain reported format 87 (`B8G8R8A8_UNORM`), 2560x1440, three buffers.

Frame/epoch 16523 on thread 2352 produced the 268-word/1,072-byte projection
buffer at CPU address `1259669760`. The two observed projection matrices were
bit-identical with zero camera jitter. Its tagged pointer had no low-bit tags.
The copied buffer SHA-256 was
`39088ca30a0ea49c4e98417b569542101696f7b54ad640a32797c646961c4b7c`.

The backend reported arena mode 2 and the same CPU/GPU resource pointer,
`358257248`. Resource descriptions reported a 50,331,648-byte buffer: three
16,777,216-byte arenas. The resource's GPU base was `12348030976`; source
arena 2, offset 264448 mapped to GPU address `12381849856`, with a rounded
CBV extent of 1280 bytes inside its used range. This supports the mapped-arena
route in the static analysis. No GPU memory readback or draw consumption was
observed.

The exact buffer ID `3107172363` and CPU pointer appeared in entry zero of
publication packet `3291416128`, whose observed count was one. Source and
publication records passed their native checks.

The recorder stopped at phase 2 (published), stop 5 (invalid), fault 7
(contention), with one drop and 14 admitted visits. No backend consumption,
draw or shader was captured. In-flight count drained to zero and all hooks
were disabled. `fatal: true` describes the rejected recorder capture; the
user subsequently exited the game normally. The exact prior v0.15 DLL was
restored, the new sidecar removed, and all four configuration hashes matched
their pre-run snapshots.

The full correlation is rejected. The independent reviewer reports
`unclean shutdown` because it rejects the fatal recorder flag, even though
hook cleanup and normal game exit were separately confirmed. Source and
publication are partial observations only.

## Diagnosis and correction

The v0.1.0 log does not identify which hook held or contended for the lock.
Code inspection found that packet/context filtering happened after entering
the shared capture lock. Unrelated traffic could therefore terminate the
capture. The arena rollover guard also used that lock despite requiring only
bounded lifetime checks. These are demonstrated collision paths, not a claim
that this particular run identified the exact colliding pair.

v0.1.1 filters immutable published keys before lock admission and tracks arena
overlap/reuse with atomics. The protection against reuse remains fail-closed;
the patch does not wait for a capture lock on a render thread. Relevant
collisions still invalidate the capture and now retain contender and sampled
owner sites/threads. Deterministic two-thread fixtures hold the bookkeeping
lock while exercising unrelated producer, publication, consumer and draw
traffic; a separate fixture overlaps a non-reusing arena rollover. Both must
complete without drops. A matching-packet collision must still reject and
identify the diagnostic sites. Existing source-change, reuse, exception,
shutdown and cached-binding tests remain required.

Evidence and immutable v0.1.0 sources/binary:
`local/projection-binding-20260923/v0.1.0/`.
Log SHA-256:
`c46312121dc9b65f5fdd3d25ddbeeb05ebfc56bffbd06b157e1cd228a269f89b`.
`reject_33032.py` independently replays the bounded observations, rejected
verdict and rollback receipt. v0.1.1 preparation is in the adjacent versioned
directory; do not rerun or overwrite the old deployment receipt.
