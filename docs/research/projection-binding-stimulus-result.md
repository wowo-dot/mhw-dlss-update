# Run 42212: jittered view-projection reaches backend consumption

On 2026-09-23, v0.2.0 captured a valid CPU baseline and one scoped nonzero
jitter sample. The sample changed `fViewProj` as predicted, the camera field
was restored, and the exact source reached publication/backend consumption.
The full experiment is rejected: recovery admission collided with the
consumer's capture lock before a draw was recorded. No recovery, shader or
draw/API record exists in this trace. The game remained responsive and the
user exited normally; this was a recorder failure, not a reported game crash.

## Identity and evidence

- PID 42212, creation `134346659906644422`.
- Executable version `0.0.0.0`, SHA-256
  `C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
- Recorder SHA-256
  `7162D34161A6150917F46FE7F8C93329BA98F11D068B8469675C505685D853E9`.
- JSONL SHA-256
  `620be25bb31b815f6a4923140c51b9cd232177bd663dfc48bd0e4213f3342ecd`.
- Startup reservation and all 36 live code gates passed; twenty targets
  installed. Eight callbacks admitted, one contention drop, zero pending calls.

Private evidence lives in `local/projection-binding-20260923/v0.2.0/`.
`reject_42212.py` uses the archived reviewer to preserve full rejection and
independently checks the two CPU samples into `rejection-42212.json`.
`archive_42212.py` verifies all 31 archived source files, restoration and
evidence hashes. The old trace and source archive are not rewritten by the fix.

## Confirmed partial result

Frame/epoch 15329 is the zero-jitter baseline; 15330 is the stimulus. Both
have the same camera/type, view matrix, unjittered matrices and viewport
2560x1440. The native sample is `[-0.03125, -0.37654319405555725]`. Exactly one
injection and one successful restoration are recorded; the restored field is
zero. Before/after provider snapshots match: raw switch 1, sizing flags zero,
same backend/device and no NGX wrapper.

Independent review accepts the view/projection products and homogeneous jitter
relations for each retained CPU sample, without loosening tolerances. Maximum
projection residual for the stimulus is about `1.23e-11`; maximum VP product
residual is about `4.45e-4`. VP x/y coefficients measurably differ from their
unjittered values while the baseline is identical. This closes the CPU question
of whether the scoped stimulus changes the matrix used by the previously
captured candidate vertex shader. It does not establish shader execution or
pixel displacement in this run.

The stimulus's 1,072-byte source at CPU address `1226115328` is published in
packet `3236890176` and consumed unchanged by the backend. It belongs to
arena 0, offset 264448, GPU address `12348295424`, while the allocator has
advanced to arena 1. Restoration order 6 precedes publication order 8 and
consumption order 9. The chain stops before any draw evidence.

The terminal diagnostic identifies contention site 2 (producer entry), thread
33756, against owner site 6 (consumer entry), thread 45580. Those threads match
the selected CPU producer and consumer. Two CPU records were already complete,
so the colliding producer admission would select recovery, not another stimulus.
The exact recovery frame was not recorded and is not inferred. Stop 5/fault 7
marks the capture invalid. Its `fatal` flag causes the reviewer's generic
`unclean shutdown` rejection even though hooks were disabled and drained.

After normal exit the previous v0.15 DLL was restored, SHA-256
`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
The temporary output sidecar was removed and all four Off/TAA/profile/mode
configuration hashes remained unchanged.

## Narrow correction in v0.2.1

Before recovery has reserved a CPU source, it has no mutation or packet to
preserve. If its entry cannot obtain the bookkeeping lock, forward the original
unchanged and select a later eligible recovery call. Log a separate deferral
count; permit at most 64 such deferrals within the same two-second window.
The next attempted deferral stops the experiment as capacity-exhausted.

Only producer entry after the completed stimulus/restoration may defer, and
not while another selected producer is active. Stimulus admission, all admitted
CPU exits, and publication/consumption/draw/API capture remain strict. No extra
jitter write or retry is introduced. Full acceptance still requires three
valid CPU observations, the observed draw/binding chain, and zero capture faults
or critical drops; a skipped recovery admission alone is never a recovery result.

Owned MinHook tests reproduce lock contention at this recovery entry, verify
successful later recovery with a frame gap, check the deferral cap, and ensure
a consumer collision remains fatal. The independent reviewer checks the new
counter/version and retains v0.2.0 support. The subsequent [v0.2.1 run 46324](projection-jitter-binding-result.md)
now accepts the full CPU/stimulus/binding/recovery experiment. It does not add
missing draw or recovery evidence to this rejected run.
