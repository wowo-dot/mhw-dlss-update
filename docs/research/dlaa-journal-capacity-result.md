# Depth markers fixed; cleanup exhausted the bounded journal

Run 54036 on 2026-09-25 used v1.15.2. All nine selected native frames
35062–35070 completed, all seven jitter injections were restored, and both
dependent observers drained before hook removal. All 42 recorded depth
consumers have valid markers and marker failure zero. Independent frozen
consumer, jitter, scene-depth and depth-route checks pass.

The run is nevertheless rejected. The submission journal reached exactly
4,096 events and set fault 2; its other bounded collections were below their
limits (21 lists, 45 allocators, one queue, 1,543 submitted-list entries).
All selected completion/retirement witnesses were already present. The last
selected completion was tick 39159328; journaling continued until tick
39159671, 343 ms later. All hooks ultimately disabled and all callbacks
settled; journal/consumer/depth reference counts balanced.

No private conversion or SDK evaluation ran. Raw pixel copies were aborted
without mapping/saving, so this run produced no new DLAA output images. The
frozen full reviewer rejects it as `stopped`; the successful component checks
remain separate and do not promote full acceptance.

## Diagnosis and next correction

The earlier stop-before-marker race is resolved in this capture. v1.15.2
keeps the journal open through multiple sequential hook-disable operations.
Each removal may suspend/resume the game's threads; the game continues
generating unselected Reset/Close/Execute events between removals. The trace
confirms the late budget exhaustion. It does not timestamp each hook removal,
so exact per-removal costs are not established.

Separate the short admission/drain phase from hook removal: stop new producer
and dependent-consumer admission, allow admitted callbacks to finish while
their required hooks/journal remain available, then stop journal admission
before the first slow unhook. Keep original forwarding, bounded drain timeout,
retained references on incomplete cleanup, and rejection of missing markers.
Exercise concurrent callback drain and continued unrelated D3D traffic across
the removal phase offline. Do not merely increase the journal budget.

Evidence: `local/dlaa-sequence-live-20260925/v1.15.2/`, creation time
134348430457181577. Host v0.0.0.0 SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
DLL SHA-256
`8901ba0f5412f97748511090b04ec29dcc0ee754b1d9217c63895381edd8d14c`;
capture SHA-256
`cff8cdc06303a82b1f379ed38b3a53d4327f037c4794e194f6b253d2922adfcd`.
The prelaunch manifest contains 521 files, SHA-256
`d37c918b6a564ae00a6061347a6ebb0b0ccdb012a942bf30d5d11aefbe0dd523`.

The user confirmed normal exit. The exact previous diagnostic DLL was restored
at 23:58:35 +03, the output sidecar was removed, and all four configuration
hashes remained unchanged. No next diagnostic is installed. The final archive
contains 537 files, SHA-256
`fbf852c6d710965faf5119151bd03b93737f0c3fc36a1bf1854b7b596aa89824`.
