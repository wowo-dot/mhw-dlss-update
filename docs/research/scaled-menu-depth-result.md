# v1.25.3: menu opening exposes CPU-order-dependent depth capture

2026-09-29, process 2220 / creation 134351225242495351, installed DLL SHA-256
`97eb817d09c40807b63869f78a1b314999675b3ce382f66b1812f05c22749d5d`.
Executable/adapter/provider identities remain those in
`scaled-menu-depth-admission.md`. Startup review passed. The arm event was
35767062; explicit stop was 36467296. The timed collector completed normally
at 36469562 (702,500 ms since arm).

## Observations

- Quiet baseline: 1,579 successful DLSS frames, zero missing depth-copy leases,
  zero runtime faults. One initial stream-readiness suspension recovered.
  A later pre-menu observation exceeded 3,500 frames with the same counts.
- The user reported that the menu was open and DLSS was skipping. The saved
  menu checkpoint had 17,910 evaluations, 1,325 missing depth leases and one
  readiness rejection. The feature was waiting, without an SDK/runtime fault.
- Every retained missing-frame sample had three successful native depth-packet
  publications and a selected final depth sequence. No depth-consumer or draw
  event for that frame was yet present when the missing lease was sampled.
  This alone would not establish writer absence or GPU execution order.
- A bounded, read-only snapshot of the mod's own diagnostic ring showed the
  subsequent writer callbacks. Four final selected writers passed draw
  admission but failed temporal-request selection with mask **510/511**:
  all nine predicates except the request's `live` bit matched. This includes
  depth sequence, frame/epoch, view, bank, async mode, container and destination
  owner. Earlier writers correctly failed both live and selected-sequence
  predicates (mask 506).
- In that ring, final selected writers for frames 45840..45843 used native
  command list 358817392. Missing-lease samples identify temporal consumption
  on another list, 358823328. Multiple native recording lists were active.
  CPU callback order is not a proof of the order in which their GPU commands
  execute. A fix cannot assume that retaining `live` alone makes this legal.
- No validated-forwarding retirement occurred: started/retired were both zero.
  The normal temporal consumer copies a request to thread-local `consumer`,
  releases the registry slot's resources, and marks it non-live at entry.
  The post-draw depth-copy callback searches only live registry requests.
  This explains the observed failed selection when the temporal consumer is
  prepared first, even though the matching depth writer passes its checks.
- Closing the menu restored normal rendering according to both the user and
  counters. The recovery checkpoint had 19,701 evaluations and all 55
  suspension periods resumed.

The diagnostic ring read is in `read_depth_ring.py` / `menu-ring-2220.json`.
It targets only this build's mod-owned fixed ring, derived from the verified
RIP-relative LEA at DLL RVA `0x44771` (trace RVA `0x11899e0`). PE identity and
the exact instruction bytes were checked. Each of 63 sampled events was read
twice and matched; the ring advanced by 19, within the 64-slot overwrite
margin. This is an observational snapshot, not an atomic freeze. No process
memory was written. The derived failure-mask summary is saved separately.

## Result and closure

Final: 19,779 DLSS evaluations, expanded copies and completed routes; 59,337
native downstream effect draws; all 28,155 route publications built. There
were 8,376 unfiltered frames (8,375 missing depth leases, one readiness skip),
55 suspensions and 55 resumptions, zero faults. All 19,780 primary GPU signals
completed; all 19,779 route recordings were released; all 28,156 jitter
changes were restored. Full resolution returned and all hooks reported off.

The full versioned review passed diagnostic consistency, runtime ownership
and cleanup. Read-only checks separately verified all 35 native code gates
and 17 native hook entry points restored. COM hook removal is covered by
runtime receipts, not those native byte checks. These are not claims of
artifact-free pixels or completed area/focus coverage.

User confirmed a normal scene and normal game exit. The DLL/sidecar remain
installed as requested. Native AA/DLSS remain Off; graphics/profile/mode
hashes are unchanged. Evidence is in
`local/continuous-dlss-20260929/v1.25.3`.

## Correction to pursue

Replace the assumption that depth-writer CPU recording must finish while the
temporal request is still queued. Depth must be available to reconstruction
according to the native scheduled GPU order, regardless of callback arrival.
Prefer copying the correctly identified native depth in the reconstruction
recording after proving its current state and preceding scheduled writer.
The existing native planner/current-use work is a starting point for that
proof, not automatic authorization for a new resource.

Before injecting that copy, verify same-frame writer/consumer scheduling,
backing/resource identity, state at the exact consumer point, and retirement
on the recording that owns the new copy. Use the planner's current span,
never its final planned state. Preserve fail-closed admission when that proof
is unavailable. A bare `live` relaxation, stale-depth reuse, CPU waits for
other recording workers, or unproven cross-list transfer is not the fix.

This run identifies a concrete CPU-lifetime/order dependency and four matching
late writer rejections. It does not prove every one of the 8,375 missing-copy
frames has that exact cause, nor establish the GPU scheduling/state contract
needed for the correction. That work remains; no fix is deployed yet.
