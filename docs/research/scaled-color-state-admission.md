# v1.23.3: reuse native current-use evidence for scaled color capture

Prepared 2026-09-27 after [v1.23.2](scaled-color-state-result.md). The live run
proved the corrected depth draw path, but could not qualify color: its transition
was absent from the current TAA callback. Output had a UAV barrier and motion
had its expected 0xc0 transition. This revision reuses the unchanged
`continuous_dlaa::NativeStateAdmission` implementation already used by DLAA.

## Runtime contract

The same Windows x64 TAA consumer/Dispatch hooks are retained; no hook target,
ABI, native shader or render-scale policy changes. The 21-hook, three-frame
capture budget remains. There is no NGX initialization/evaluation or injected
jitter. Existing depth source 0x80/0xc0 and destination RENDER_TARGET checks,
exact depth/TAA recording association, submission/fence/retirement checks and
state restoration remain mandatory.

Only the scaled build supplements a missing color transition. An observed
local 0x40 transition follows the existing path. A conflicting, split, aliased
or wrong local state cannot be overridden. The supplementary proof requires
the native planner's exact current packet and epoch, bounded complete earlier
schedule entries, linked resource-use spans, the earlier color initialization,
current output UAV use, matching API barriers and a successful metadata reread.
The tracker’s final planned state is never a current-state authority. Failure
stops this capture and forwards the original dispatch.

Each selected callback owns one proof slot, attempted at most once (three total).
Large evidence carriers have static storage; all metadata reads/rereads happen
inside the owning callback and are not reused for a later frame/recording.
Following admission, the active packet, packet contents, shader recipe, depth
owner and deadline are checked again before input copies. Existing COM pins and
GPU ownership rules remain unchanged; failed/unproven issued copies retain their
process-lifetime quarantine rather than being freed while potentially in flight.

Raw local barrier evidence remains unchanged. A separate `color_admission`
receipt records attempted/accepted/revalidated/reset and the complete versioned
native proof. Offline review replays the existing decision and binds it to the
CPU frame/epoch/packet, publication, command/reset, renderer, owner/backing/
resource identities, engine extents/formats and exact pre-dispatch API barriers.
This is native contract telemetry, not an independent GPU state query. Completed
GPU readbacks still require the separate submission proof.

Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
captured executable SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Five existing DLAA planner code gates are added to this recorder's install
checks: RVAs 0x228f3b0 (2000 bytes), 0x2292610 (304), 0x22927f0 (736),
0x229cee0 (7756), 0x24e7a10 (2620). All 32 recorder code gates are rehashed
against the pinned capture before packaging. No new native addresses were derived.

## Verification and next live run

Build /W4 /WX passes. Twenty-eight native checks pass, including real WARP
single/multiple depth-writer captures whose color transitions occur before the
observed consumer and whose output receives only a UAV barrier. The three proof
receipts accept, real GPU copies/conversions match expected pixels, and D3D12 GPU
validation is clean. Stale trackers and aliased native backing reject with fault
16, forward the original dispatch and issue no motion/color copies. Original
local-transition and full-copy routes still pass.

Ninety-four Python checks pass; thirteen inapplicable cases skip (nine historical
jitter tests and four proof-specific tests on local-transition fixtures). These
include existing native proof replay tests and adversarial cross-binding checks
for frames, packets, generations, owners, dimensions, barriers and reread receipts.
A packaging runner initially reused a check-log filename and refused to overwrite
it; remaining checks ran under distinct names. This was not a capture failure.

Package: `local/scaled-temporal-20260927/v1.23.3/`. Exact prior DLL and output
sidecar are preserved; `deploy.ps1 -Action Restore` provides explicit rollback
with the game closed. Keep the new DLL installed after testing, as requested.
Saved settings and game assets are not modified.

Launch the same bare MO2 entry, keep AA/DLSS Off and windowed 2560x1440, load a
save and remain still. Verify the new PID/creation/header before arming with
the frozen `arm_scaled_temporal.py`. Half scale applies briefly, three input
sets are attempted, full scale restores automatically, and hooks are removed.
Then require the complete GPU/image/cleanup review. Live acceptance is pending;
scaled motion convention and DLSS evaluation remain later steps.
