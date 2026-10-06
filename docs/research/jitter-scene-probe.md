# v0.15.0: explicitly triggered gameplay jitter samples

Updated 2026-09-23. Run 27200 accepts both CPU sample phases; checkpoint
qualification and GPU/raster correctness remain false. The game is closed.
This follows [run 30924](jitter-sequence-probe.md#run-30924-both-cpu-phases-accepted):
32 valid main-menu observations had zero stored jitter and `mJitterEnable=0`.
The previous automatic sequence filled before a loaded scene could be observed.

## Run 27200: menu and loaded-scene CPU samples accepted

Preflight verified the deployed DLL and host identities below, all 25 code
spans (16950 bytes), producer 1500 on both idle channels and the ready scene
event. The user enabled once, reported a brief freeze, then loaded a save.
The request at tick 7739343 received a worker acknowledgement at 7739359;
16 subsequent phase-1 samples completed before the shared deadline. The event
does not detect gameplay: scene attribution includes the user's confirmation.

| Observation | Initial menu | Triggered loaded scene |
| --- | --- | --- |
| Valid samples | 16/16 | 16/16 |
| Source frames | 15902-16215 | 29379-29738 |
| Sample span | 1641 ms | 1640 ms |
| TAA layer address, process-local | 4996347200 | 6261663696 |
| Stored camera jitter | Zero in all samples | Zero in all samples |
| `mJitterEnable` | 0 in all samples | 0 in all samples |
| Wrapper present | 16/16 | 16/16 |

The layer changes to another instance of the verified TAA class
(`vtable RVA 0x351BDA8`); the first projection matrices also differ between
phases. Within every sample, the jittered and unjittered matrices are
bit-identical. The derived camera type remains `0x33F82A8`. The sampler
therefore follows current producer arguments through the scene change rather
than rereading a retained menu-layer address.

In all 32 observations, the enable byte is the only false captured native
jitter-predicate clause: type 8, layer flags `0x13`, view flags `0x2D00D1FD`
(mask `0x840000` clear), blend bits `0x3F733333` and zero reset vector.
Renderer `use_dlss` remains 1 and the wrapper remains present during both
bursts. Nonzero table candidates are not stored camera jitter. This rules out
the narrow explanation that this disabled state was confined to the sampled
menu instance. It does not identify who cleared the flag, establish the
pre-enable state, or prove the earlier jitter callback's execution/gates.

The original one-item diagnostic retains one producer, four preparations,
one depth correction and one completed consumer/Evaluate with zero guard
rejections. All 17 parameter queries and resource inspection complete.
The publication-bound source frame is 15902 and Evaluate frame 15903.
Presentation is DX12, 2560x1440, format 87, three buffers, adapter LUID
`0:99257`; color/motion/depth/output formats are 26/16/41/26. No gameplay
Evaluate, GPU readback or modern SDK call is added by the second CPU burst.

The Evaluate checkpoint remains **unqualified**: `in_flight=true`,
`coverage_complete=false`, and receipt `qualified=false`. Barrier call 295
overlaps marker 592 (entry 589, return 595) on another command list. Its
23 retained transitions reference none of the captured Evaluate resources.
That list is submitted at index 0, ahead of the Evaluate list at index 2.
This classifies the overlap without retroactively qualifying the snapshot.

The independent final journal drains with 517 records, flags 0 and no pending
or failed slots. Submitted-order review observes the expected declarations
(color/motion `0x40`, depth `0xC0`, output `0x8`) and binds Reset 215, Close
323, submission 342, Signal 344 and completion 517, value 15927, 15 ms later.
Completion terminates admission as designed. Initial states, all resource
lifetimes/cross-queue uses and temporal contents remain unverified;
`gpu_safe=false` and raster-jitter qualification stay false.

### Return to menu and normal exit

After both bursts completed, Release 76 succeeded at tick 7785171. A new
Create 77 at 7785500 returned `0xBAD00001` and abort 78 made the provider
inert. The intermediate policy event says `evaluate-token-issued`, but no
second handle was published: the boundary result was failure.

This diagnostic's `arm_barrier_observer` rejects every second attempt through
`attempted.exchange(true)`. `native-depth-probe` requires successful arming
before publishing a handle, so recreation cannot succeed after this run's
first capture. The trace's unarmed second Create and the user's greyed-out
Off setting are consistent with that enforced bound. This is a limitation
of the capture mode, not acceptance of repeated feature creation.

The user left the unavailable setting alone and exited normally. Both
Shutdown calls succeeded, cleanup cleared and the trace quiesced after
204062 ms. Saved settings match the original Off/TAA hash below; no settings
edit was needed or performed. DLL, mode, backup and all 145 staged source/
archive pairs were verified unchanged at exit, before these note edits.
The existing 48 native and 78 Python tests and binary/hardware checks still
apply; this acceptance work changes no native code or deployed binary.

Evidence is private under `local/jitter-scene-20260923/`: final trace,
`channels-27200.json`, request/acknowledgement, both independent reviews,
exit identity and `acceptance-27200.json`. `accept_27200.py` replays the
reviews, checks payload hashes, both phases and lifecycle/budget invariants,
and preserves the false checkpoint qualification. Capture SHA-256:
`E5B06478911D850753736B17DC20C53869E9C8616976C79D673C4A624161BBC2`;
journal SHA-256:
`2235476F214DCF418B306B8C9A4B5256283E97CE8F93DB92B5DBAF0C2A3CB896`.

Next: trace the assignment from scene properties into the live jitter-enable
byte and design a bounded pre-enable or earlier-callback observation if the
offline path remains ambiguous. Do not request an unchanged menu/save rerun
or force the byte On on the strength of producer-time samples alone.

## Offline scene/property trace

The exact host remains version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
Decoded-image SHA-256 is
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.

At `0x1AB5DE1`, the scene-parameter path calls `0x16EA1E0` with the group
name `uSdlTemporalAA`. The latter passes `mJitterEnable` to `0x16E8EC0`.
Inspection across the split unwind ranges clarifies that this helper writes
the parameter's **presence/selection byte** (`+0x18`), based on matching
property/group metadata. It does not directly write the TAA layer's enabled
value. Registration `0x16E41D0` names the separate `+0x19` value
`mJitterEnable .mValue`; stream reader `0x16E7990` normalizes an input byte
and stores it there. This identifies a scene-controlled parameter path, but
does not yet establish the assignment that cleared the observed live layer.

The exact TAA layer constructor, reflection registration and native jitter
callback remain confirmed as described in v0.14.0. Broad displacement and
leaf-instruction candidate scans do not prove an exhaustive set of writers;
unrelated object fields and instructions decoded from interior bytes remain
candidates, not identified TAA stores. No game process or asset was modified
for this offline work. Private artifacts are in `local/jitter-scene-20260923/`.

## Capture contract

The existing Windows x64 producer hook at `0x23913F0`, signature
`uintptr_t(uintptr_t args)`, still accepts only return address `0x2392F8E`.
All 25 existing exact-build code gates, source/camera/layer guards, scalar read
limits and pointer-lifetime rules are unchanged. No new host hook is added.
The single producer/four preparations/one consumer/Evaluate limit is unchanged.

Sequence schema **2**, producer **1500**, has two fixed phases:

- Phase 0: 16 initial samples, starting only after a nonnull wrapper is seen.
  Once started, it records actual wrapper presence; disappearance does not
  start phase 1 or change the budget.
- Phase 1: 16 samples after one explicit scene request. The caller must wait
  until phase 0 is full. This phase records either wrapper state and does not
  imply that DLSS is On or Off.

Both phases have 100 ms minimum spacing and share the original 120-second
deadline from the first sample. The 32-sample capacity never resets. Invalid
observations consume slots; duplicate/regressing source frames are rejected
within each phase. Only one scene request can be accepted. Premature, repeated,
late or post-close requests cannot replenish samples or admit rendering work.
The scene label describes the intended capture after the user's confirmation;
the event itself does not detect gameplay or prove camera alignment.

The worker owns an auto-reset event named
`Local\MhwDlssJitterScene-v1-<pid>-<process-creation>`. A pre-existing event
causes creation to fail closed. The worker polls without blocking, then arms
the scalar sampler under its existing mutex. No borrowed address is retained
for later dereferencing. Trace closure closes the event and permanently stops
sampling. RAII also releases the event. A missing event prevents scene capture;
it does not change the original diagnostic admission policy.

`tools/arm_jitter_scene.py` requires the saved v0.15 preflight, checks the
process creation time and active trace, requires a completed initial burst
with no prior scene arm, and reserves at least five seconds before either
deadline. It opens only the exact event with `EVENT_MODIFY_STATE` and signals
it once. The process handle is query-only; no process memory is written. Its
receipt reports a **request**, not an acknowledgement. Acceptance requires
the worker's `bridge.jitter-scene-trigger accepted=true` row and subsequent
phase-1 samples. Data still appears in the normal trace log.

Mailbox schema 1, journal schema 2 and frame-packet schema 2 are unchanged;
their producer becomes 1500. The offline reviewer accepts old v0.14 schema-1
sequences separately from v0.15 schema-2 sequences. GPU/raster qualification
stays false; modern SDK integration remains inactive.

## Verification and deployment

All **48 native tests** and **78 Python tests** pass. The owned hook fixture
waits after 16 initial samples, accepts one explicit scene trigger, captures
eight present-wrapper and eight absent-wrapper scene samples, and still
executes exactly one producer/four preparations/one consumer. Independent
JSON review accepts all 32 CPU observations and their matrix relationships.
Event tests cover process generation, collision rejection, auto-reset behavior,
explicit closure and RAII. Reviewer/request tests reject premature sampling,
stale process generations, expired windows, duplicate triggers, wrong versions
and traces closed before scene capture.

The saved run 30924 still replays with submission 406, Signal 408, completion
615 and flags 0; its legacy 32-sample sequence remains accepted. Hardware
D3D12 completion, CFG (128 targets, 18 distinct wrappers), helper assembly and
unwind checks pass. The accepted build log has no compiler warnings/errors.

The new DLL is 789504 bytes, SHA-256
`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
Staging preserves v0.14.0 as `mhw_dlss_bridge_probe.v0.14.0.dll.disabled`, hash
`8BDEE78C69B4A20E74AB4F229932517877D25F34C727CCB15F22A4CFF1E2888D`,
and the unchanged sidecar as `mhw_dlss_bridge_probe.mode.before-0150.disabled`
inside the MO2 `dlssTEST` mod. Previous backups remain intact. Mode stays
`native-depth-probe`; inherited environment stays `capture-one-evaluate`.
Source/build/evidence identities are recorded in `staging-0150.json`.

Saved Off/TAA/DX12/FullScreen/2560x1440 settings hash remains
`02BAB1311AD739420182D2FF5DB8A6ED9536235E80B51D2B736A6232633CB235`.
Mode-file hash remains
`E94AC2A93344FD2069AA07C24FC58A005F59F0FCB68882C07B9C0CFD562154A6`.
Only the dedicated MO2 DLL is replaced. The original game executable and
assets remain untouched. With the game closed, rollback copies the exact
v0.14.0 backup over the active DLL and verifies its hash. Full removal disables
the MO2 Force Load rule and the `dlssTEST` mod; retain evidence and backups.

## Completed controlled-run procedure

1. Launch the same MO2 test entry offline. Leave DLSS Off at the main menu
   and wait for the v0.15 preflight, including `jitter-scene-event ready=true`.
2. Enable once, close the popup and leave Options. Remain at the menu while
   both Evaluate channels and the 16 initial samples are saved.
3. When instructed, load one save and remain in the scene. Reply from the
   phone. Do not return to Options or toggle again. The operator sends the
   event request only after this confirmation, within the existing window.
4. Save the scene trace after 16 additional samples (or retain a rejected/
   incomplete result explicitly). Compare camera/layer identities, gates,
   actual wrapper state, jitter and matrices across the two phases.
5. After evidence is saved, return to the main menu, turn DLSS Off if
   selectable, apply and exit normally. Archive final logs/settings and verify
   cleanup and source/deployment identities. No special three-second Off wait
   is required in this version; that is no longer the second phase.

```powershell
python local/jitter-scene-20260923/preflight_0150.py --pid <pid>
python tools/save_capture.py --pid <pid> --output-directory local/jitter-scene-20260923
python tools/review_submission.py --receipt local/jitter-scene-20260923/channels-<pid>.json --output local/jitter-scene-20260923/review-<pid>.json
python tools/arm_jitter_scene.py --preflight local/jitter-scene-20260923/preflight-<pid>.json --output local/jitter-scene-20260923/scene-request-<pid>.json
python tools/review_jitter_sequence.py --log local/jitter-scene-20260923/bridge-<pid>.scene.log --output local/jitter-scene-20260923/sequence-review-<pid>.json
```

MO2 may remain open. This version requires no separate debugger/recorder and
no intentional terminal crash. If the existing producer is absent in gameplay,
the second phase can remain incomplete; do not infer gameplay values from the
menu or claim that signalling the event captured anything.
