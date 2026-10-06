# Independent AA-Off DLAA evaluation, v0.7.0

**Accepted and closed 2026-09-26:** PID 56588 completed all eight evaluations
and verified all 24 images. The exact previous diagnostic DLL is restored;
this one-shot capture is not armed. See the [result](../research/independent-aa-sdk-result.md)
for evidence and the remaining native-output integration. The launch procedure
below records this completed experiment's contract.

This connects the existing inline DLAA implementation to the verified AA-Off
input read point. It evaluates eight consecutive source frames with one DLAA
feature, resets history only on the first, and saves private results for receipts
0, 3 and 7. It does not replace displayed color. Native AA and DLSS stay Off;
no TAA/FXAA option writes or native fallback are introduced.

The existing 25 capture hooks, code gates, Windows x64 ABIs, projection scope,
owned queue insertion and completion-before-unhook procedure are unchanged from
[v0.6.1](independent-aa-projection-closure.md). The verified host remains version
0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
decoded analysis SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.

## Startup and ownership

The existing [NGX startup transaction](ngx-startup-capabilities.md) runs before
the native initialization call, with its separately documented hook and code
gates. It is disabled before the capture becomes ready. Startup reservation is
shared within this DLL. Arming requires a successful, reviewed startup receipt
for the exact process generation and deployed DLL. Modern initialization remains
owned by that startup transaction; this test never calls Shutdown on the game's
device. The modern runtime path comes from the session's `sdk-runtime-dir.txt`.
No machine-local path is compiled into the binary.

The worker creates one `SdkPass` before installing the active capture hooks. Its
three private input textures have the native formats 26/41/42 and COPY_DEST
state. The existing `InlineFrame` owns conversion textures, preset-K full-size
DLAA (create flags 75), a private RGBA32F output and packing pass. Its destination
is also private, in UAV state. Consequently the old inline handoff contract can
be reused without applying its UAV assumption to native AA-Off color.

## Recording

The native read point still requires all three tracked inputs in PS-SRV state
128, the exact immutable source projection, and a valid binding snapshot on the
matching Reset generation. The existing input readbacks run first for receipts
0/3/7. The SDK adapter then records, under the same exclusive journal lease:

1. Native input 128 -> COPY_SOURCE, copy to private raw texture, restore 128.
2. Private conversion and DLAA Evaluate, using the matching frame and epoch.
3. Private packing/output copy, with no native output destination.
4. Restore compute/graphics root signatures and arguments, pipeline and heaps.
5. Copy converted color/depth/motion, RGBA32F DLAA and packed DLAA to diagnostic
   readbacks at receipts 0/3/7, preserving the private texture states.

The SDK receives actual raster jitter `(projection_x / 2, -projection_y / 2)`,
matching the previously verified projection convention. Both raw bits and the
SDK values are logged per receipt. Motion conversion is unchanged: packed half
motion becomes pixel offsets `(-dx * width/2, dy * height/2)`, not jittered MVs.
This diagnostic uses fixed 16.666666ms, not a measured production frame time.

Before reusing the feature and private buffers, receipts 1-7 require the previous
selected submission's observed GPU completion AND successful Reset retirement.
The diagnostic polls for at most 250ms while native APIs continue forwarding.
It does not infer ordering from CPU recording or presume all queues are the same.
The existing overall two-second capture deadline and 4,096-event budget remain.
This synchronization is diagnostic overhead, not the intended streaming design.

A thread-local scope bypasses recursive barrier observation only for SDK commands
on this same command list. Native copy transitions use the original trampoline;
the SDK only receives private images. Binding setters remain observed, and exact
post-record mirror equality is required. The underlying native barrier lease
is retained until all injected recording finishes; other threads/lists retain
normal observation.

The first recording error stops the experiment. Missing prior completion cannot
reuse history. Partially referenced objects are retained until process exit.
There is no retry or silent native AA activation. After all eight completions
and retirements, observation closes before hooks are removed. A held fence from
receipt 7 authorizes SDK release/parameter destruction. Saving requires this
proof; successful saving/logging releases the adapter's private resources.

## Verification and acceptance

The actual NVIDIA SDK fixture runs outside the game with GPU-based validation.
At 256x144 and 2560x1440 it performs eight evaluations with retained history and
nonzero jitter, checks all native input pixels remain unchanged, checks binding
mirror equality, and dispatches a downstream shader whose exact output proves
the restored compute bindings work. Five failure cases cover missing state,
incomplete prior work, failure after conversion, failed Evaluate result and
missing retirement proof. All seven final fixtures have zero D3D12 errors and
warnings. Independent Python inspection verifies converted inputs and packed
DLAA output pixel-for-pixel for both successful fixtures (15 images each).

48 relevant native checks, 46 AA Python checks and 15 existing startup review
checks passed. The native completion tests also cover borrowing a completed
fence and rejecting incomplete or already released receipts. The SDK reviewer
rejects altered frame identities, jitter mapping, early history reuse, missing
restoration, wrong completion fence, output aliases and damaged images.

An initial hardware fixture was rejected solely for its test buffer's deprecated
initial-state warning; the fixture now uses COMMON. An initial startup-test
invocation omitted its required fixture argument; the documented invocation
passed all 15. Both initial outputs are preserved outside the final test package.

Runtime acceptance requires the frozen `review_aa_sdk.py`: the earlier CPU/input/
projection/binding/closure checks, exact startup identity/device, eight successful
SDK receipts and serialized history, 15 verified private images, exact input
conversion and exact packing from RGBA32F DLAA. SDK alpha is not used as a validity
signal; RGB must be finite and contain none of the clear sentinel. A successful
private-output capture is not proof of native presentation or temporal quality.

## Launch and rollback

Session: `local/independent-aa-sdk-20260926/v0.7.0/`.
Use the usual bare offline MO2 profile and **Monster Hunter: World - DLSS 1.2.110
test** entry. Keep AA and DLSS Off, Windowed 2560x1440, DX12 On. Load one save and
keep camera/character still before arming. A brief freeze during the eight-frame
diagnostic is possible. No persistent visual change is expected from private
output evaluation.

Arm with the sealed session's helper only after both startup and ready logs pass.
Events are `Local\\MhwAaSdk-v1-PID-CREATION-1` (arm) and suffix `-0` (stop).
The header uses `private_gpu_commands=-1` because SDK command count is opaque;
the observable limit is eight evaluations and `sdk_output=private-only`.

The exact existing MO2 slot DLL is backed up as `previous.dll`. After normal
game exit, use `restore.py` to restore it and remove only the matching output
sidecar. Graphics/profile/mode hashes must remain unchanged. Never overwrite the
game executable, original assets or archived test captures. Capture closure is
required before preparing a subsequent native-output handoff build.
