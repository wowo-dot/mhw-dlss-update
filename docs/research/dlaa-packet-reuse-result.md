# Nine-frame recorder stopped on packet reuse — 2026-09-25

The v1.15.0 run, PID 32996, passed startup and exact process/DLL verification.
The nine-frame recorder then rejected its fifth TAA publication. This was a
recorder identity restriction, before any SDK evaluation or owned conversion.

Confirmed from the frozen trace:

- Frames 36346–36349 completed native TAA consumption. Frame 36350 was rejected.
- Frames 36348 and 36350 used packet address 3298974448. The earlier consumer
  had returned at tick 35860703, before the new producer entered at 35860734.
- `gpu::publish` rejected all repeated addresses across the entire capture,
  reporting consumer fault 12. The next projection lacked the rejected TAA
  acknowledgement and reported secondary projection fault 3.
- Four scoped jitter injections all restored to zero. Every recorder hook
  disabled and pending counts reached zero. No DLAA evaluation started.
- The user reported camera drift before arming. The observed rejection is an
  address-equality check and is not evidence of a camera-motion failure.

The user confirmed normal exit. The exact previous DLL was restored at
23:03:40 +03; graphics, profile, modlist and mode hashes are unchanged.
Evidence: `local/dlaa-sequence-live-20260925/v1.15.0/`.
The 343-file closed archive SHA-256 is
`1d95ef4090e78a9175c41de490f24d6d5e1a5f038632fe299a81b470987161d1`.
The capture itself is rejected; its partial receipts do not establish a
successful nine-frame DLAA result.

The [v1.15.1 correction](../runtime/dlaa-packet-generations.md) distinguishes
completed CPU packet uses by publication and frame/epoch, while preserving
duplicate, overlap and independent GPU completion checks.
