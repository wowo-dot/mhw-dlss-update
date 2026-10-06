# Startup-owned bounded live DLAA — v1.13.0

Completed: [accepted live run 47780](../research/first-live-dlaa-output.md).
All three private outputs pass; the game closed normally and the exact prior
DLL is restored. The following describes the frozen experiment.

This joins the [accepted startup initialization](../research/startup-sdk-capability-result.md)
to the existing [three-frame private evaluator](live-dlaa-sdk.md). Original
TAA remains the presented image; native DLSS remains Off. The experiment
creates one private DLAA feature and saves three reset-history outputs.

## Sequence and ownership

The existing worker reserves its nearby MinHook buffer before any SDK load,
then runs the startup transaction. Its same four hashes, five-minute readiness
bound, unused native slots, original call contract and later display query
remain required. A successful pool reservation explicitly admits MinHook's
already-initialized status; the standalone startup target still requires a
fresh MinHook instance. The early hook is disabled before capture can be armed.

The shared startup library exposes a validation callback only after exactly
one native call, successful native result, public initialization and both
capability checks, disabled hook and drained callbacks. The retained device
and exact pinned provider remain owned by that library until process exit.
The callback checks device identity, the requested provider path, live module
handle and pinned file hash. Failure cannot fall back to another initialization.

Only then is the ordinary input recorder marked ready. After the user loads a
save, the existing named event arms the unchanged ten native hooks and three
frame copies. All 51 native code gates, resource/producer checks, GPU copy
limits, conversion and fence retirement remain those of v1.10/v1.11.

After those hooks disable and drain, evaluation revalidates startup ownership
and all nine input textures and three producer fences. It queries capabilities
again and performs the same bounded private feature creation/evaluations.
SDK event schema 3 replaces the evaluator's Init call with a `sdk_reuse`
receipt recording the verified device/provider and zero initialization calls.
An already-loaded provider without this ownership proof is still rejected.
There is no borrowed-device Shutdown. Feature/parameter cleanup follows
completed private GPU work, as before.

## Verification

Release `/W4 /WX` builds pass. Fourteen current-build hardware fixtures cover
fresh initialization, retained startup evaluation with prior MinHook setup,
native-error ownership refusal, missing/rejected ownership, four invalid-input
cases and the five original startup admission/forwarding cases. All relevant
GPU fixtures report zero debug errors/warnings with GPU validation enabled.
The three startup-owned outputs exactly match the fresh-init control outputs.

Seven focused native tests cover readiness, SDK/input contracts, typed WARP
conversion and pool reservation/failure. Python checks pass: 61 capture tests
(nine historical jitter skips), 13 SDK tests for reuse, 13 for fresh init (two
reuse-only skips), six cross-run ownership-link tests, and 15 startup reviewer
tests. Full output review checks 33,177,600 RGB values and rejects a copied,
self-consistently rehashed NaN. Its outer process/display metadata is explicitly
synthetic; this is not a live-game result.

The full live reviewer requires both logs to agree on version, process
generation, DLL/host hashes, device, adapter, extent and swap-chain format.
The standalone startup reviewer must pass and SDK schema 3 must prove reuse.
Historical SDK schemas remain readable; they cannot satisfy the new handoff.

## Run and rollback

Evidence/deployment: `local/dlaa-startup-live-20260925/v1.13.0/`.
Target `mhw_temporal_dlss_probe`; existing MO2 test slot and exact previous-DLL
backup. No original game file, graphics option or profile is changed.

Launch the existing barebones test entry with DX12 On, DLSS Off and TAA On.
Load one save and remain still. Startup runs automatically; the input capture
waits up to 30 minutes for the agent's verified arm event. Report from the
phone. No toggle or camera movement is required. A brief capture pause is
possible; the visible image remains native TAA.

After both logs and outputs are saved, exit normally with DLSS still Off.
The local `deploy.ps1 -Action Restore` checks hashes, restores the exact prior
DLL and removes/restores its output sidecar. Frozen prelaunch evidence and
runtime results must be archived before preparing another version.
