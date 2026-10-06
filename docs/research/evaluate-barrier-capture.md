# First-Evaluate barrier recording capture

Date: 2026-09-22. Bridge **v0.7.5** captured actual barrier calls in **run 30272**.
The observer and declared color/motion/output transitions are accepted;
depth's declared state and GPU state/execution remain unvalidated.
The preceding [run 14392](evaluate-resource-descriptions.md)
accepted the four texture descriptions and canonical device identity; it did
not establish resource states. This diagnostic records the actual legacy D3D12
barrier API calls between admitted DLSS Create and the first Evaluate.

The first v0.7.5 launch, PID 46236, verified the executable/system/NGX identities
and complete startup for both providers. Its five-minute worker window finished
before the main-menu readiness message was inspected. No Create or Evaluate
occurred, and no external recorder was attached. The completed log and its
timeout review are preserved locally. The subsequent launch produced the
accepted observations below.

## Accepted run 30272

The user enabled DLSS once at the main menu after ARMED, left Options and
reported a crash. Create sequence 68 armed all four observer hooks. The external
v0.2.0 recorder corrected the same null-secondary depth argument once on frame
**17234**, with all ten code gates and 105 scalar reads passing. Evaluate
sequence 69 then completed all 17 getters and resource inspection.

The observer captured **232 completed API calls and 705 barrier descriptions**
within its bounds, without faults, contention, overflow, expiration or unfinished
callbacks. The Evaluate command list matched the hooked methods. It had two
observed successful Resets; the selected recording began after call 165 and
contained three barrier calls. All six retained resource matches were on the
Evaluate thread, tick 17391093. No Close, failed Reset or opaque bundle appeared
in the selected interval.

| Input | Recorded sequence in selected command list | Accepted scope |
| --- | --- | --- |
| Color | RENDER_TARGET (`0x4`) to NON_PIXEL_SHADER_RESOURCE (`0x40`) | Latest declared state matches expected input state |
| MotionVectors | COMMON (`0`) to UNORDERED_ACCESS (`0x8`), then NON_PIXEL_SHADER_RESOURCE (`0x40`) | Consecutive declared states agree; final state matches input expectation |
| Depth | No matching transition, UAV or aliasing record | **Unknown**, not COMMON and not proof of a missing earlier transition |
| Output | COMMON (`0`) to UNORDERED_ACCESS (`0x8`), then two UAV barriers | Latest declared state matches expected output state; UAV barriers do not change it |
| PreviousOutput | Null | No resource state inferred |

Every matched transition covered ALL_SUBRESOURCES with no split flags. There
were no relevant aliasing records or continuity mismatches. Calls 183 and 192
returned to host `+0x259CF9E`. The final preparation flush, call 225, returned to
**host `+0x259DB35`** and contained exactly motion's `0x8` to `0x40` transition
and an output UAV barrier. This connects the statically traced preparation path
to an observed API call immediately before Evaluate. It does not establish GPU
submission, completion, initial-state correctness or cross-list ordering.

All four resource descriptions again matched the corrected work item's native
pointers: 2560x1440, same canonical device, color/output R11G11B10_FLOAT,
motion R32G32_FLOAT and depth R32_FLOAT. All ten temporary references were
released without fault. Presentation creation again reported a unique matching
B8G8R8A8_UNORM, 2560x1440, three-buffer chain; current presentation is unobserved.

Frame **17235** hit the same uncorrected fault **32 ms** later. The one-use
recorder rejected a second retry and detached, then the game exited after the
reported crash. It observed three debug exceptions (attach breakpoint plus the
two matching faults), one context correction and zero remote-memory writes.
No Release/Shutdown was captured. Evaluate's intentional `BAD00012` result and
the absence of modern reconstruction are unchanged.

The existing static export of host `+0x22927F0` shows that a compatible tracked
read state can return without adding a new transition record. The known primary
backing's +112 tracker pointer was captured, but its state word was not. An
earlier compatible depth state is therefore a **hypothesis**, not an accepted
result. This report excludes other command lists and earlier recordings; it
cannot say whether depth transitioned there. Next work should connect the depth
tracker and earlier depth transitions to their recording/submission order.
Do not repeat this unchanged capture to fill that gap.

`review_resources.py` and `review_barriers.py` validate the logs and write
`review-30272.json` under `local/barrier-capture-20260922/`.

| Evidence | SHA-256 |
| --- | --- |
| `retry-30272.jsonl` | `CBF217667D1969AE841A9B313CF1AFCCF79A6AB2FA2E00CB4B75F0A92F8CA980` |
| `bridge-30272.crash.log` | `B564437A09C4DCACD03CC7542EDDEB1C5706DCB0802F8108C41613FA368ACA98` |

The exact saved-On settings were backed up after verifying the game had closed.
Only saved DLSS was reset to Off; the user's current **FullScreen**, TAA, DX12,
2560x1440 settings were retained. This differs from the Borderless mode recorded
at staging. No game executable or asset was modified.

## Observation contract

The existing exact host, system resolver, NGX caller, provider lifecycle and
Create parameter checks remain required. Only a verified DLSS Create requesting
the one-Evaluate token attempts installation. If installation fails, no token
is published and the existing fail-closed path is used. ISP is unchanged.

The observer obtains four method targets from the verified Create command
list's vtable. Each target must be executable committed MEM_IMAGE memory in
the system-directory `D3D12Core.dll` or `d3d12.dll`; targets must be distinct.
No game instruction is hooked or modified. This is structural method discovery,
not a fixed-address signature. The methods use Windows COM `STDMETHODCALLTYPE`
with the Windows x64 ABI:

| Method | Vtable slot | Tested D3D12Core RVA | Observation |
| --- | --- | --- | --- |
| ResourceBarrier | 26 | `0x23B280` | Count and bounded copies of barrier descriptions |
| Reset | 10 | `0x239FD0` | HRESULT; successful reset starts a new recording |
| Close | 9 | `0x235DC0` | HRESULT; a later Close prevents declaration acceptance |
| ExecuteBundle | 27 | `0x238A80` | Marks the recording opaque to this observer |

Tested original executable: version **0.0.0.0**, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
Tested system D3D12Core: version **10.0.26100.9278**, SHA-256
`70710DE807F188A85A3F5AD3A729A41BFA7A92B7621CEEAF38059F5640399267`.
Both WARP and the local hardware adapter exposed the method RVAs above.
They are evidence, not hard-coded hook targets.

The host owns each command list and barrier array throughout its API call.
The observer copies only the documented 32-byte barrier structures and scalar
call metadata before forwarding the original, unchanged arguments. It marks
each observation complete only after the original returns. It preserves
HRESULTs and LastError. No COM reference is acquired or retained, no deferred
resource dereference occurs, and no GPU command, barrier or submission is added.
Copied resource/list pointer values are correlation labels, not lifetime proof.

Hooks and any enabled trampolines remain alive with the process-pinned probe.
Never-enabled partial installations can be removed; once any hook was enabled,
a failed installation disables observation and preserves its trampolines for
potential in-flight calls. The standalone test removes hooks only while quiescent.

## Bounds, ordering and uncertainty

The one-use window begins at installation during Create and ends at the first
DLSS Evaluate entry, before any Evaluate getters. It is additionally limited to
**two seconds, 2,048 API calls, 16,384 descriptions total, and 1,024 descriptions
per call**. Concurrent callbacks reserve disjoint fixed-storage slots. An atomic
admission gate tracks callbacks that have entered but not returned; freezing
the gate prevents later admissions. There is no rendering-thread allocation,
lock or wait. Admission retries are bounded to 32 attempts.

Faults while copying descriptors, exhausted bounds, admission contention,
expired time or unfinished callbacks invalidate coverage. Access violations
and in-page errors in observation are latched; original driver calls and other
exceptions retain their normal handling. Failed or incomplete observation does
not alter the game's API call.

At Evaluate, the command list must expose the same four method targets. The
replay then considers only that command-list address, after its latest observed
successful Reset. If no Reset was observed, the interval begins at arming; it
does not claim a complete recording from command-list creation. Failed Reset,
Close or ExecuteBundle in the selected interval prevents declaration acceptance.
Calls on other lists cannot supply a target's declared state.

Each NGX resource pointer is matched against transition, UAV and aliasing
records. Whole-resource transitions with no split flags establish the latest
**declared** after-state. Consecutive transition before/after mismatches,
split/per-subresource transitions or relevant aliasing mark the result uncertain.
A UAV barrier never creates a state value. Null PreviousOutput remains absent.
The report retains the latest four matching events per role plus full counts.

`bridge.barrier-summary` reports coverage and bounds; `bridge.barrier-resource`
reports per-role declarations; `bridge.barrier-match` includes call sequence,
batch index, original caller, callback thread, tick, type, flags, subresource,
before and after states. CPU admission order is not cross-queue execution order.
The resource-description log continues to say `state=unobserved`, because
GetDesc itself does not expose state.

Expected declarations from the [static conversion](evaluate-resource-descriptions.md#state-conversion-traced-statically)
are `0x40` NON_PIXEL_SHADER_RESOURCE for color/motion/depth and `0x8`
UNORDERED_ACCESS for output. The relevant host preparation flush returns are
`+0x259DB35` and `+0x259DCB5`; neither is used as a hook target or assumed to
have executed. A live matching event must establish that correspondence.

This captures legacy ResourceBarrier calls through the observed implementation.
It does **not** prove initial states, pointer lifetime continuity, implicit
promotion/decay, enhanced barriers, queue submission/synchronization, successful
Close after Evaluate, GPU completion or texture contents. Missing transitions
remain unknown. It cannot by itself authorize modern reconstruction or a
persistent depth correction. See Microsoft's
[ResourceBarrier contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-resourcebarrier),
[Reset contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-reset)
and [resource-state guidance](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12).

## Verification and attended test

Release builds with `/W4 /WX`. All **21 CTests pass**. Replay tests cover role
matching, other-list rejection, Reset, failed Reset, Close, opaque bundles,
missing/contradictory inputs, split and subresource barriers, continuity errors,
global UAV/aliasing, malformed ranges and bounded history. Real WARP tests
exercise all four hooks, original calls, LastError, quiescent removal and actual
call/batch/description/time limits. The hardware-adapter fixture also passes.
No fixture submits a command list.

Binary audit: 506,880 bytes, 110 CFG targets; all 15 provider wrappers/callbacks
remain distinct and in the CFG table. v0.7.5 SHA-256:
`045465F3C84E6637DB283B08550218170366C7643E046E4B90680E7B32933035`.
The external recorder remains v0.2.0 with one permitted depth argument retry.
Modern reconstruction remains unimplemented and Evaluate still returns the
intentional not-implemented result.

Use the existing offline MO2 **Monster Hunter: World - DLSS 1.2.110 test** entry.
Start with saved DLSS Off and TAA selected. At the main menu, wait for the
external recorder's ARMED event before enabling DLSS once and leaving Options.
The existing worker also has a separate **five-minute window from startup**;
if that has already completed, a fresh launch is required before arming.
Stay at the main menu, without toggling again or loading a save. The next
uncorrected depth fault can still crash. Preserve both logs, check the new
barrier records against the work item and Evaluate pointers, and review gaps
before deciding whether another run is needed.

Artifacts are ignored under `local/barrier-capture-20260922/`, including the
test logs, binary audit, source/binary archives and `staging-075.json`. After the
user closed the game, deployment preserved the exact active v0.7.4 DLL as
`mhw_dlss_bridge_probe.v0.7.4.dll.disabled` at the same MO2 mod destination.
Saved settings were backed up without modification: DLSS Off, TAA, DX12,
Borderless 2560x1440, as saved by the user's latest normal exit. The existing
MO2 process can remain open. The external recorder sources still match the
v0.2.0 archive exactly. The CMake regeneration rebuilt that executable; its
rebuilt copy is preserved separately, and the original hash-verified v0.2.0
binary was restored for the attended run and passed its self-test again.
For removal, close MHW and disable the bridge Force Load entry. For rollback,
preserve v0.7.5 under a disabled name, verify the v0.7.4 backup against
`FA743BE800A67E646FB4145C23EB8E90D53169D86394D5C13F514F45E54006C0`,
and restore it to the active DLL name. No original executable or asset changes.
