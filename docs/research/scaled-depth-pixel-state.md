# v1.23.2: pixel-only native depth draw state

Prepared 2026-09-27 after the [v1.23.1 live rejection](scaled-depth-draw-result.md).
This corrects the recorder's source-state requirement; it does not change the
game's depth draw, shader bindings or scale policy. No NGX evaluation occurs.
The continuous DLAA implementation is unchanged.

## Contract

The existing scoped DrawInstanced observer and its Windows x64 ABI remain as
documented in [v1.23.1](scaled-depth-draw-capture.md). There are still 21 hooks,
three selected frames and the same bounded capture/drain windows. No hook or
native address has been added. Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
captured executable SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
All 27 existing native code spans are rechecked during packaging.

The source must now be exactly PIXEL_SHADER_RESOURCE (0x80), or the combined
pixel/non-pixel shader read state (0xc0). The destination must still be exactly
RENDER_TARGET (4). This follows the observed final writer and Microsoft's
[resource-state definitions](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_states).
Other masks, missing states, aliasing, split/partial transitions and conflicting
chains still reject the capture. The full-allocation CopyResource route and its
COPY_DEST restoration remain unchanged.

Readback still follows the last matching published depth producer before TAA,
restores destination state 4, and requires exact TAA command-list, allocator/
reset, submission, fence and retirement evidence. Nothing infers GPU completion
from CPU publication. Source/destination descriptions, frame/view stamps,
draw arguments, packet and cached-pipeline stability checks remain intact.

Both offline depth-route and depth-pixel reviews use the same state predicate.
Only v1.23.2 admits the additional 0x80 state; historical v1.23.1 evidence retains
its original 0xc0 requirement. All scaled versions retain active-region versus
allocation-size distinctions. Motion conversion remains allocation-size
diagnostic arithmetic, not a verified scaled DLSS motion convention.

## Verification and runtime procedure

Build with /W4 /WX. Twenty-four native cases cover the corrected source states,
the existing full-size copy route, caller/argument/packet/pipeline rejection,
missing retirement and private-signal failure. Pure native logic rejects
non-pixel-only, write, extra-bit, unknown, aliased, split, partial and conflicting
states. No GPU commands are issued for these invalid-state pure cases.

Four retained WARP draw cases cover single/multiple combined-state writers,
single pixel-only writer, and two combined-state writers followed by a pixel-only
writer. Pixel-only cases use firstVertex=3 in both the native packet and actual
API call. A fixture vertex shader repeats its triangle for vertices 3-5; this
is synthetic validation, not a claim about the game's vertex shader. Pixel
sampling writes expected per-frame depth into an 8x6 region of a 17x13 target,
with a distinct value outside. GPU validation and exact image/conversion checks
pass. Seventy-four Python cases pass, with nine inapplicable historical jitter
tests skipped. Reviews reject corrupted active pixels even when rehashed and
reject invalid states; historical 0xc0-only review remains strict.

The first offline pixel review exposed a second historical source==0xc0 check
in the pixel reviewer. It was corrected using the shared predicate and all four
draw review suites rerun. The earlier failure log is preserved in the package.

Evidence root: `local/scaled-temporal-20260927/v1.23.2/`. Freeze sources/tools,
builds, fixture captures, checks and exact previous slot DLL/output sidecar.
Install only with the game closed. `deploy.ps1 -Action Restore` restores the
backed-up v1.23.1 files after verifying destination identities; rollback is
explicit, not automatic after a test. Saved settings are never written.

Use the same bare MO2 launch entry, native AA and DLSS Off, DX12 and fixed
windowed 2560x1440. Load a save and stay still. Agent verifies the new process
generation and arms using frozen `arm_scaled_temporal.py`. Capture restores
full scale automatically, then closes all hook groups and saves evidence.
Require all cleanup, GPU and image checks before accepting the three frames.
Keep the installed DLL after the run. This revision still requires live testing.
