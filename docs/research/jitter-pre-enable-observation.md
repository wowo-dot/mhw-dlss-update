# Native jitter before the first DLSS enable

Updated 2026-09-23 after [run 27200](jitter-scene-probe.md#run-27200-menu-and-loaded-scene-cpu-samples-accepted).
Run 5836 observes the existing TAA path with DLSS left Off throughout menu and
loaded gameplay. The sampled TAA jitter-enable flag is already 0 and stored
camera jitter is zero before the first Create/Evaluate. The first Create and
its native depth/fallback hooks are therefore not necessary for this state.
The existing bridge's startup callbacks and other enabled mods remain possible
influences; this is not a vanilla baseline or proof of the exact writer.

## Offline findings and limits

The pinned TAA constructor writes `0x01010001` at layer `+0x184`, so its
jitter-enable byte defaults to 1. The separate `TemporalAA` scene parameter
also initializes its `+0x19` value to 1, with `+0x18` presence initially 0.
The scene-property metadata helper writes presence, not the layer's value.
These defaults do not identify the assignment responsible for the observed 0.

An apparent pair of clear/restore stores at `0x1AB83BD`/`0x1AB83D9` belongs
to a `MotionBlurReconstructionFilter`, established by the named lookup that
populates its owner pointer. They are not evidence of a TAA store. Other
examined leaf candidates initialize unrelated repeated fields. A wider-copy
scan retains 627 candidate unwind ranges from 6831 ranges examined; it does
not establish an exhaustive set of writers or a relevant assignment.

Stop broadening displacement searches without a type or execution link.
Use the live baseline to distinguish a pre-existing disabled state from a
change caused during this diagnostic's first enable. If a writer observation
is later needed, first identify the live object and observation window.

## External read contract

Private tool: `local/jitter-assignment-20260923/read_jitter_baseline.py`.
It opens only `PROCESS_QUERY_INFORMATION | PROCESS_VM_READ`. It adds no
debugger, hook, remote call, process write, GPU command or rendering admission.
The deployed v0.15 DLL and sidecar remain unchanged.

The executable is version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`;
the decoded image used for code comparison is
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
The reader verifies disk/mapped PE identities, process generation and six
bounded live code spans governing the previously reviewed list traversal,
TAA field layout and camera-jitter callbacks. Code reads are at most 128 bytes;
data reads are named scalars of at most eight bytes with region checks.

Each sample traverses `*[host+0x51238C8]+5584`, following `+48` next links,
with a 512-node/cycle bound. Only the verified TAA vtable `0x351BDA8` receives
field interpretation, with at most eight matches. The reader records type,
flags, blend and four adjacent boolean fields, with repeated field checks.
It also reads the verified camera's stored jitter and epoch, renderer
counters, main-view flags and the backend NGX/context pointer. It rechecks list links and
root/type pointers. No unrelated node contents are logged.

The default is 16 attempts, 100 ms apart; the CLI caps attempts at 32 and
scalar reads at 4096 per attempt. A failed read stops the run and preserves
completed samples. Existing output is never overwritten. Pointer/value
rechecks and unchanged counters remain asynchronous observations; they do
not prove ownership, execution of the earlier callback, frame coherence or
GPU/raster jitter. The layer-list candidate need not be the actual active
producer argument. Multiple matches must remain explicit.

Start the same offline MO2 entry with saved DLSS Off/TAA and stay at the
main menu. Capture the baseline before requesting any toggle. If useful,
repeat the bounded read in a loaded save with DLSS still Off. Keep scene and
setting labels tied to the user's confirmation, not to a guessed internal
renderer byte. Archive the reported normal exit and verify settings afterward;
claim shutdown/cleanup only when covered by the trace.

## Run 5836: disabled jitter observed before first Create

The user confirmed the main menu, then a loaded save, while leaving DLSS Off.
PID 5836, process creation `134346289060627051`, used the unchanged v0.15.0
DLL in `native-depth-probe` mode. Standard preflight verified 25 spans / 16950
bytes and both idle producer-1500 channels. Each external reader additionally
verified the six code spans above and the same process generation.

| Observation | Attempts | Source frames | List nodes | Enable 0 / stored jitter zero |
| --- | ---: | --- | ---: | ---: |
| Menu Off, broad | 16 | 16775–17141 | 25 | 16 / 16 |
| Loaded save Off, broad | 16 | 34779–35182 | 338 | 16 / 16 |
| Loaded save Off, focused | 16 | 58283–58701 | 338 | 16 / 16 |

Every traversal found one TAA candidate with verified vtable `0x351BDA8`,
type 8, flags `0x13` and blend bits `0x3F733333`. Menu and gameplay used
different layer addresses. Broad reads also retained reset 0, velocity-base 1
and sharpen 0. The camera type was `0x33F82A8`; the game was foreground for
all 32 broad attempts. The focused reader did not record foreground status.

These are asynchronous observations with differing consistency limits:

- Menu: layer fields and jitter repeated unchanged in all 16 attempts; list
  links were stable in 16, anchors in 14 and frame/epoch unchanged in 15.
- Broad gameplay: layer fields and jitter repeated unchanged in all 16, but
  the larger traversal crossed frame/epoch advances in every attempt. List
  links were stable in 13 and broad anchors in 0. This pass is not accepted
  as a same-counter observation.
- Focused gameplay: `read_jitter_focus.py` independently discovers and rechecks
  the typed list before each short scalar window. It then repeats 11 named
  scalars, brackets them with renderer counters and rechecks root pointers.
  All 16 retained stable links, anchors, types and scalars; 11 also retained
  matching counters and camera epoch. Only those 11 satisfy its combined
  `observation_consistent` flag. No earlier sample's layer pointer is reused
  for discovery. This still proves neither ownership nor frame/GPU coherence.

The broad tool's `wrapper_present=true` field means a nonnull backend
NGX/context pointer at `backend+0x42700`; it does not establish a live feature
handle. Likewise `use_dlss=1` is not interpreted as the user's DLSS setting.
Both values were present before any recorded Create.

## Trace coverage and exit

All three sample windows lie inside the trace: ticks 9031359–9033000,
9112312–9114125 and 9216890–9218750. The trace starts at 8945906 and closes
at 9246140 after its configured 300000 ms active window. It records no
CreateFeature, EvaluateFeature, ReleaseFeature or Shutdown calls, no native
depth/fallback installation, and zero created/released handles at the final
checkpoint. Both provider summaries have zero dropped lifecycle/bridge events.

Startup was not inert: DLSS and ISP Init/Populate completed, and the final
checkpoints retain two successful optimal-settings callbacks. Therefore
this comparison excludes first-Create behavior as a prerequisite, but does
not exclude the bridge's earlier capability/sizing behavior, serialized game
properties, or other mods.

The user reported a normal exit and the process was confirmed absent. The
trace had already expired, so Shutdown and final cleanup are **unobserved**.
Both summaries still report initialization cleanup outstanding at that cutoff;
this does not establish a leak or failure during the later exit. Do not reuse
run 27200's successful shutdown result as evidence for this run.

The final trace exactly matches its source file. Saved settings exactly match
the prior SHA-256 `02BAB1311AD739420182D2FF5DB8A6ED9536235E80B51D2B736A6232633CB235`:
DLSS Off, TAA, DX12 On, FullScreen, 2560x1440. Active DLL, sidecar and previous
DLL backup identities are unchanged. No native code, deployment, game setting
or MO2 configuration changed during this investigation.

Private evidence is in `local/jitter-assignment-20260923/`: the three sample
JSON files, both reader scripts, final trace, saved settings and exit receipt.
`accept_5836.py` replays the archived identities, counter checks, trace coverage
and limits into `acceptance-5836.json`, with SHA-256 hashes of the evidence.
The unchanged native build retains its prior 48 native / 78 Python test result;
this run adds no new native implementation to validate.

## Next discriminating observation

The following control is now completed in [run 8164](jitter-startup-control.md#run-8164-menu-and-gameplay-remain-zero-in-the-control):
menu and gameplay remain zero with synthetic startup substitution disabled.
Its evidence and restored configuration are recorded there.

First prepare an otherwise identical Off/TAA comparison with bridge startup
substitution disabled, preserving the current MO2 configuration and rollback.
Keep the other mods and save fixed initially so the bridge is the changed
variable. Verify the actual loaded modules/routing before interpreting it.
If the flag stays zero, investigate scene/property assignment or isolate other
mods separately. If it changes, narrow the bridge's startup callbacks.

An earlier bounded observation of the assignment remains an alternative once
the live object and window are identified. This run itself did not implement
either control. Do not force the flag, repeat the unchanged enable/save experiment,
or claim raster jitter/modern SDK readiness from these external CPU samples.
