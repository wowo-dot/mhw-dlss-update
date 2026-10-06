# Main-menu activation failure, v1.25.12

PID44148/134352021939485649; startup passed, main-menu arm accepted. Collector
ended after 8.609 seconds with runtime fault29, before any DLSS evaluation.
The sole qualified resolve candidate hit single_native_output/bytecode gating:
one shader check, one rejection, and one unclaimed native dispatch. Current
telemetry combines shape, copy and hash mismatch; exact failing subcheck unknown.
SDK feature creation returned success; evaluation was never attempted.
One ordinary output plan built, three warmup draws; no reconstructed output.

Cleanup verified: no private GPU submissions, both jitter changes restored,
full resolution restored, all callbacks drained/hooks disabled, 38 full native
code spans and 17 hook heads match. The standard healthy-run reviewer rejects
the unhealthy terminal as expected. failure-review-44148.json records failure
and independent cleanup checks. User reported normal scene and normal exit.

A bounded post-stop read at the retained dispatch packet address found that
its frame arena had already been reused (no longer compute opcode6). No shader
was dumped or attributed from that stale pointer. The known accepted shader is
technique23086,15492bytes,SHA2d7b26c742c27db2c83546c6cf6a1dfda47d7cfd4336e69f8c2302de5802b0d1.
Cannot yet claim this menu uses a different shader versus binding/layout failure.

Wider playtest did not begin. Next: bounded first-rejection metadata and exact
shader capture at the failed invocation, written after callbacks stop; preserve
the existing gate. Same DLL left installed. Use fresh evidence before relaunch.

## v1.25.13 recorder prepared

Shader admission and fault29 behavior remain unchanged. A fixed first-rejection
record distinguishes shape mismatch, inability to read tested bytecode, and
bytecode mismatch. It retains frame/epoch, packet/generation, source owners,
technique, recipe and bindings. Exact tested bytes are retained when available;
otherwise it attempts a bounded copy of the shader at rejection time, marked
separately. No new hook, renderer policy or render-thread file I/O is added.
The record is smaller than280KB; shader size is bounded52..262144bytes.
After stopping, the worker uses CREATE_NEW for
inputs-PID-CREATION-rejected-taa.dxbc. Final JSON contains size/hash/save status.
Metadata does not grant rendering permission; a main-menu stop is still expected.

44native tests and51Python tests passed, including exact tested-byte retention,
immutable first rejection, bounded/failed reads and unchanged dispatch routing.
Synthetic v1.25.13 healthy-log parsing requires the new channel. No new compiler
warnings. Evidence local/main-menu-shader-20260930. Same MO2 entry/settings,
stay at MAIN MENU for arming. Diagnose the captured rejection before correction.
The wider playtest is pending; this build makes no visual compatibility claim.
Keep latest DLL installed per user. Manual undo with game closed:
local/main-menu-shader-20260930/deploy.ps1 -Action Restore. Prior DLL and sidecar
are preserved exactly; use fresh evidence before relaunching into a sealed root.

# Main-menu shader rejection captured, v1.25.13

PID47580/134352030501838281 stopped with fault29 before DLSS Evaluate.
Captured technique29842,15284bytes,SHA
e14459733cbbccc47fdaed77ce818062968cf96303f12e28bde7d86a50d6b47c.
The shape predicate rejected the technique/size; its normal tested-byte copy
and hash comparison short-circuited. The bounded rejection recorder copied
the exact invocation's shader before returning and saved it after stop.

All six SRVs and the single UAV match existing roles, identities, descriptors
and nonaliasing checks. Resource declarations and initial packed-motion/color
loads match gameplay shader23086. Main difference is native history filtering:
neighborhood min/max instead of variance clipping. Both write only u0.
The 1x1 named-depth placeholder also occurs in accepted gameplay; it is NOT
evidence of missing DLSS depth, which uses a separate scene-depth path.
Private comparison JSON, disassembly and diff are archived locally only.

SDK Create succeeded; no Evaluate, no private GPU submissions. Both jitter
changes restored, full scale restored, all hooks disabled; independent live
reads matched38fullcode spans and17hookheads. User corrected an accidental
answer: scene normal, game closed normally. Process absence and unchanged
settings/installed DLL verified. Healthy reviewer rejects this failed run.

Next add this exact variant with independent cached byte verification for
each known technique, retaining every resource/state/lifetime/output guard.
Menu evaluation and menu-to-gameplay transitions remain untested; broad
cutscene/monster testing has not started. Keep latest DLL installed.

## v1.25.14 exact menu variant admission prepared

Only the scaled runtime now accepts the captured technique29842/15284-byte
shader alongside gameplay23086/15492. Each requires its exact SHA on first
use, then compares all bytes against its own immutable verified cache.
Technique/size substitutions, changed bytecode, absent/aliased/wrong resource
owners and unexpected output bindings remain rejected. Unknown shaders still
record fault29. No depth/state/lifetime/geometry/history policy is relaxed.
Unscaled DLAA retains gameplay-only admission. No new hooks or GPU work.
New shader_admission telemetry counts each verified variant separately;
the v1.25.14 reviewer requires exact identities and accounting/monotonicity.

44native and51Python tests passed, plus11admission-corruption rejections
and synthetic two-variant reviewer coverage. /W4 /WX build clean after fixing
a bounded size conversion warning. The previous source/log/DLL archive remains
sealed. New root local/main-menu-variant-20260930, exact previous slot/sidecar
backup. Runtime menu admission, real depth/route qualification and visual
output remain UNTESTED. Next launch main menu for arm, then load save after
baseline succeeds, allowing broader testing only once that transition passes.
Native AA/DLSS Off; same windowed2560x1440,Performance1280x720. Keep latest DLL.
