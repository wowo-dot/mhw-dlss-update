# Native output-draw execution qualification, v1.23.12

Live qualification passed and the run is closed and archived. See the
[accepted result and shader findings](scaled-draw-execution-result.md). The
prelaunch contract and its limits are retained below.

The full-size output integration reuses the observed native chain: motion
filter, color grading, then final-screen draw. v1.23.10 established its CPU
construction and resource ownership. This step captures those three selected
draws at execution, together, for three frames. It does not yet replace their
output or evaluate DLSS.

## Implementation

The existing postfilter callback publishes the three exact packet identities
after their construction, including immutable attachment/IA/graphics/render
pointers, argument pair, output resource and source-frame identity. The final
destination is the texture owner already verified by the downstream recorder:
view+0x460 points to an attachment, whose +0x58 points to that texture owner.
The postfilter stores this owner snapshot and revalidates it before publishing;
it does not reinterpret the attachment as a texture. Nine fixed slots have an atomic
empty/publishing/selectable/executing/finished lifecycle. Multiple selectable
matches, reentry and changed packet inputs reject the observation.

One added hook targets RVA0x259d670, the native non-indexed draw executor.
Windows x64 ABI: RCX executor, RDX backend context, R8 packet. The complete
206-byte span is gated by SHA-256
`d6b0b80a37a8fadba69d215cfcebd960c74f8d3554e9375b03ac7785d4f6ee39`.
The pinned wrapper sets state+0x158 to its packet, prepares IA, attachments,
PSO/root/descriptors and barriers, then calls DrawInstanced exactly once with
the packet's count/first vertex, one instance and first instance zero.
No indexed, indirect or bundle path is admitted.

The callback forwards the original wrapper unchanged, including return state
and native exceptions. On normal return it checks attachment-cache agreement,
the recorded packet, PSO and graphics root, shader and render-state stability,
viewport/scissor caches, and unchanged input identity. No modern frame identity
is inferred from a renderer global that may already have advanced; source-frame
association is the published packet/ownership association. This is bounded
execution evidence, not a general packet-reuse authorization for a continuous
replacement implementation.

The existing attachment decoder supplies native blend, depth and rasterizer
metadata. The recorder also copies up to32 input elements, each with a bounded
64-byte semantic name, and VS/PS bytecode up to262,144 bytes each. Other shader
stages are rejected. Shader descriptors and bytes are rechecked on return.
The output resource is queried for ID3D12Resource and its owning device; it
must be the published resource, a full-allocation single-mip/slice/sample 2D
render target. Query references are released in the callback. No GPU work,
private descriptors, pipelines, vertices or resource contents are changed.

Reused attachment helper RVA0x259f9b0 is gated over1531bytes with hash
`7abd32763fb614e868e425a607d67d717205988f34a520fed0884fd51cb720c8`.
Pipeline binding helper RVA0x259e670 is gated over2554bytes with hash
`088e67c77b7e96af64c83c8297f6fc9f6d355dbc170530dda125b41f4cb53727`.
Tested host version0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decrypted analysis image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.

Admission stops before callback drain and hook removal. Completion now waits
for all nine output draws as well as the existing temporal-input capture.
There are26 hooks total, three input frames and one restored jitter sample.
The two-second input window and full-resolution restoration remain unchanged.
The DLL stays pinned; successful readback lifetime follows the existing recorder.

## Verification and limits

Eighteen native callback cases cover success, exceptions, stop while admitted,
reentry, ambiguous/duplicate publication, changed packet/attachment/shader/layout,
bad cache, wrong output/device, invalid arguments, shader/layout bounds and an
unexpected additional shader stage. Fixtures use real WARP resources and
compiled authored shaders with a synthetic native wrapper. They do not simulate
the game's GPU draw. All 101 affected native/GPU regression tests pass;
23 Python reviewer tests and the previous v1.23.10 capture pass. The combined
retained WARP input-capture fixture reports clean GPU validation. Project code
builds with /W4 /WX.

The integrated `publish_draws` regression constructs distinct attachment and
texture-owner objects, passes the verified destination through postfilter, and
requires three publications for each frame. It also verifies that publication
alone cannot certify execution completion. This catches the v1.23.11 pointer
handoff defect without another game run.

The external reviewer joins each execution record to the matching constructed
packet/output and checks the pre-integration half-size effect viewports and
full-size final-screen viewport. It verifies shader artifacts and construction/
execution PS equality for the two known effects. Cached pipeline metadata is
not claimed to be an API-captured PSO creation descriptor. No GPU submission,
completion, descriptor-table contents or full-output handoff is certified for
these added draw observations.

The v1.23.11 live attempt (PID18584, creation134350911012735423) stopped with
postfilter fault4 before any draw execution was observed. Its added publication
path treated the attachment as the texture owner. The existing downstream
record correctly distinguished those pointers. All references were released,
full scale restored, and hooks disabled; no jitter injection or SDK evaluation
occurred. The user confirmed a normal scene and normal exit. The reviewer
rejection and partial evidence remain preserved in the sealed
`local/scaled-temporal-20260928/v1.23.11/` archive, SHA-256
`4fce596c4982deb24ac274c339cdac6322f912bc427987118385235de75387c1`.

v1.23.12 corrects only the destination handoff and version/reviewer support;
the execution capture contract remains unchanged. The live run subsequently
passed; its result is linked above. Test configuration:
native AA/DLSS Off, MotionBlur Off, DX12 and windowed2560x1440. The usual MO2
test entry and bare profile remain unchanged. Preparation preserves the exact
previous DLL and output sidecar under ignored
`local/scaled-temporal-20260928/v1.23.12/` evidence. Manual
removal, only with the game closed: run that evidence directory's deploy.ps1
with Restore. It verifies destination/backup hashes first. Do not restore
automatically; the user wants the latest DLL kept installed.

After qualification, use these native recipes to construct the private
full-size pipelines and apply output viewport/UV changes without altering
shared exposure work. The previously verified scaled SDK resolve and motion
filter adapter remain the components to connect.
