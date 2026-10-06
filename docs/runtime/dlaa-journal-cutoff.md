# Close journal admission before hook removal — v1.15.3

The [v1.15.2 live run](../research/dlaa-journal-capacity-result.md) preserved all
depth markers but filled the fixed 4,096-event journal during cleanup. An
offline reproduction injects 8,192 original D3D Close/Reset callbacks at the
first hook-removal boundary. With the previous ordering, the journal grows
from 46 to 4,096 events and faults. The source, executable and before/after
traces are preserved under
`local/dlaa-sequence-live-20260926/v1.15.3/checks/before-fix/`.

## Shutdown contract

1. Stop new TAA producer/consumer, jitter and depth callback admission.
2. Drain admitted dependent callbacks with all required interception and
   submission logging still available, for at most two seconds. Timeout
   latches producer fault 17 and permanently rejects this capture.
3. Close submission admission before the first hook removal. New D3D calls
   still forward normally, without extending the journal. A D3D callback
   admitted before cutoff can still finish its recorded return; submission
   cleanup separately drains these callbacks and retains references on failure.
4. Remove hooks and release eligible references. Child observers do not repeat
   the already-bounded dependent wait. Missing-marker, timeout, native-exception,
   GPU-completion and ownership failures continue to prevent conversion/SDK work.

The header requires `pre_unhook_cutoff:true`. A shutdown receipt records the
dependent drain result, journal cutoff and first-removal ordering, with tick
timestamps. The submission journal records its closed admission and cutoff
tick. The reviewer requires these witnesses, their ordering and the two-second
drain bound, as well as the existing marker and selected-frame checks.
Admitted D3D return events may complete after admission closes; this is distinct
from admitting new callbacks and is covered by a synchronized fixture.

The journal remains limited to 4,096 events. No signatures, hook targets,
calling conventions, native arguments, input formats, SDK configuration or
feature ownership change. The host remains v0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
The [packet lifetime](dlaa-packet-generations.md) and
[missing-marker rejection](dlaa-journal-drain.md) contracts still apply.

## Verification and runtime test

Release /W4 /WX builds and all 148 native temporal tests pass. The corrected
stress fixture forwards 8,192 additional D3D calls without changing the journal
at all. A second case combines this traffic with an admitted depth callback
paused across cancellation. A third pauses a D3D Close return until after the
cutoff, verifying that its result drains correctly and a later Reset forwards.
Positive WARP fixtures retain exact typed color/depth/motion conversion and
clean GPU validation, with fixture clear-value performance warnings counted.

Current and historical reviewers exercise required cutoff/drain evidence,
missing markers and all prior ownership/jitter conditions. Accepted v1.14
and rejected v1.15.1/v1.15.2 live captures retain their classifications. SDK
validation reuses the sealed unchanged evaluator fixtures, with hashes checked;
this is not a new hardware SDK run or a live v1.15.3 success claim.

Use the same barebones MO2 entry with DX12 On, DLSS Off and TAA On. Load a save,
remain still (a stationary pose helps), then reply from the phone. The agent
verifies startup/build/process before arming. The diagnostic still requests
nine private DLAA outputs, seven scoped jitter samples, retained history and
fixed diagnostic timing. It does not replace the displayed native TAA image.

Evidence/deployment: `local/dlaa-sequence-live-20260926/v1.15.3/`.
The versioned `deploy.ps1 -Action Restore` restores the preserved exact prior
slot DLL/sidecar after normal exit and verifies four configuration hashes.
No original game assets are modified. Live verification is pending.
