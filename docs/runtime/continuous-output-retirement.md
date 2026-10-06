# Continuous DLAA recording retirement — v1.18.4

The recorded v1.18.3 soak stopped at `presentation destination capacity` after
45,964 evaluations: 16 native output textures were retained for process lifetime.
This revision changes native resource ownership, not the reconstruction shaders,
SDK settings, native TAA forwarding, jitter, history decision or request routing.

## Ownership and proof

The native TAA producer validates and acquires four COM references (color, motion,
output and sampled depth) while their engine owners are valid. The published
request owns them until consumption. The consumer owns them through its callback,
including early rejection, and the producer releases its temporary references
after publication. Consumed slots contain only diagnostic metadata.

Before the first injected depth copy, the runtime copies those references into
a fixed 128-entry recording table keyed by DIRECT command-list identity and Reset
generation. The TAA insertion requires its exact four references to match that
recording. Submission stores the private fence value only after native Execute
and Signal return successfully. Cross-queue waits retain the existing ordering.

A successful native Reset retires that generation. It can occur before its GPU
work finishes; the old entry stays independently owned. Collection requires BOTH
that Reset and a completed fence covering its submission. Completion without
Reset retains references because a command list could be replayed. Reset without
completion also retains them. Device removal, missing submission, capacity
exhaustion and invalid receipts never cause eviction. Any runtime fault freezes
further collection, since a forwarded unexpected submission may invalidate old
fence coverage. Remaining entries are retained until process exit on stop.

Continuous Output/InlineFrame use an explicit caller-retained mode: runtime owns
the native resources and lists, so these helpers keep only borrowed destinations
and numeric diagnostic identities. Their legacy bounded ownership mode remains
the default for older one-shot/streaming callers. Private GPU resources, SDK state
and the 128-list registry remain process-lifetime retained in the runtime.
One additional COM reference preserves the previous successful depth identity
for the history comparison, preventing pointer reuse from hiding a depth change.

The mutex serializes request-to-recording ownership and receipts. No new hooks,
native signatures, per-frame allocations, GPU readbacks or game-thread waits
are added. CPU resource references live in bounded request/recording storage.
This is not a general hot-unload or device-recreation implementation.

## Telemetry and verification

`recordings` schema 1 / `reset-and-fence-v1` reports created, submitted, retired,
released, live, peak and the reasons references remain pending. `resource_count`
now counts four references per live GPU recording, not unique historical textures;
it excludes CPU requests and the single depth-history reference. The independent
reviewer checks ownership, counter partitions, bounded capacity, monotonic totals
and completed GPU work on a clean stop. Unreset recordings may remain pinned.

Offline verification:

- 23 native tests: actual runtime queue/reset callbacks, duplicate forwarding,
  a GPU gate held across Reset, 1,000 two-queue submissions, 2,000 ownership
  transitions past both former capacity limits, history and output packing.
- 90 Python checks for lifetime review, prior full-log review, request/history
  telemetry, startup and the external collector. An initial startup-test command
  supplied a log instead of its fixture directory; the corrected 15-test suite
  passed and both command outputs are preserved.
- Real NVIDIA SDK replay at 2560x1440: 600 evaluations, 60 newly allocated native
  destinations, three in-flight list generations, two ordered queues. All 600
  recordings released, peak three live, zero GPU validation warnings/errors.
  Final packed output and downstream compute/graphics pixels match independently.
- Four one-frame SDK regression cases: success, missing bindings, wrong output
  and invalid jitter. Rejected inputs preserve the native result.

These are offline results. In-game duration and transition stability are pending.
The earlier request-slot collision remains unresolved; native TAA bypass is still
a later step, followed by DLSS and FG/MFG.

## Repeat session

Evidence root: `local/dlaa-output-retirement-20260926/v1.18.4/` (ignored).
`deploy.ps1 -Action Restore` restores the exact DLL and sidecar with the game
closed. Preserve Windowed 2560x1440, native DLSS Off/TAA On, DX12 On and the bare
offline MO2 profile. Launch the usual test entry and load a save before arming.

Use the existing 30–60 minute recording procedure and external collector. Save
a healthy baseline with retirement already progressing before ordinary play.
Include natural quest/area transitions; keep the whole Steam recording. When
finished, stop the diagnostic, verify closure, exit normally, restore the previous
DLL and archive. A successful long run improves coverage; it is not a universal
lifecycle, performance or image-quality proof.
