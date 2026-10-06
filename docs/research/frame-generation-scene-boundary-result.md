# FG final scene boundary: v0.6.2 runtime result

2026-10-03. The corrected recorder passed the exact final-draw/submission/
Present association checks in four requested samples. This is an accepted
**partial metadata map**, not verified HUD separation or game frame generation.
DLSS Performance v1.25.44 and Reflex On remained healthy; the user reported
clean main-menu, gameplay, menu-open and area-load scenes, then a normal exit.

## Confirmed observations

Each selected plan's final packet matched one returned, replaced, API/native
binding-qualified DrawInstanced call. The draw used three vertices, one instance,
the same native root/PSO/RTV, and a non-rotating view. Its exact command-list
generation was submitted before qualified Reflex PresentStart. There were no
capture overflows, duplicate submissions or premature recording resets.

| Sample | Plan frame/epoch | Recording and Present frame/epoch | Events | Unreadable | Outside executor scope |
|---|---:|---:|---:|---:|---:|
| Main menu | 11271 | 11272 | 4 | 153 | 24 |
| Loaded save | 13646 | 13647 | 2 | 55 | 0 |
| Menu open | 16446 | 16447 | 10 | 1281 | 435 |
| After area load | 18443 | 18444 | 2 | 73 | 0 |

The producer's counter precedes the recording/Present counter by one in every
sample. The recorder uses packet identity, returned draw, sealed choice and
submission receipts to close; it does not encode that observed offset as a
general engine rule. The qualified Present owner was thread 5400; the final
draw worker was thread 45828. All captured submissions used the same queue.
Different CPU callback times were ordered using submission batch/list position.

The final target interface label was stable across these four samples, with
engine extent 2560x1440. Its engine GPU-format field was UINT32_MAX; this does
not identify its DXGI format. Each sample recorded the target's transition
from RENDER_TARGET to PIXEL_SHADER_RESOURCE|COPY_SOURCE (4 to 0x880).
The target did not match any sampled GetBuffer return in this run. These are
historical samples, so absence from that set is not current-buffer/lifetime
proof. The following copy/composition remains to be mapped.

Only the qualified final draw passed the same-target direct-draw checks in
each sample. No post-final HUD draw was identified. The unreadable and unscoped
counts explicitly prevent a complete draw inventory or HUDless-color claim;
menu-open sample 3 is especially useful for investigating that missing scope.

## Renderer and cleanup

The joined review accepted 7,191 DLSS frames and 8,377 complete Reflex frames,
50,262 markers and 64 complete driver reports, with zero runtime faults or
request collisions. Four skipped resolves are retained in the evidence; this
is not a claim that every frame was reconstructed. All 7,195 GPU signals
completed, all 7,999 jitter changes were restored, full render resolution
returned, Reflex Off and SDK shutdown were confirmed, and resource pins released.

Independent post-stop VM_READ checks verified all 37 native hook heads and
46 full native functions restored (778 reads /75,163 bytes, zero writes).
An earlier supplemental live reader rejected the intentionally hooked code
at RVA 0x23931e0 before writing a preliminary review. Its rejection is preserved;
the reader's native-code gates were kept intact and the full post-stop check
passed. The successful preliminary metadata review used the qualified paired
input-log image base instead. Saved graphics bytes were unchanged and the
test DLL remains installed as requested.

No Streamline FG plugin, FG input tags, generated MHW frames, GPU copies or
new hooks were introduced by this diagnostic. The existing NGX runtime's
nvngx_dlssg mapping is documented separately and does not imply FG activation.

## Evidence and next work

Private run: `local/frame-generation-scene-20261003/game-02`, PID 18272,
creation 134354777527664172. Its 43-file archive manifest SHA-256:
`bfeb3e9a445f31b44d7297c6c4ddf0e6ec278e2cca7b7b49fd0bbf11c87c9226`.
Exact 686-file source freeze `source-0.6.2` manifest:
`24e2c1d42072eb4be90258a03aed03204c8e9efbea0d56dbaf46338edfbd1b47`.
Installed DLL:
`91fe440c4203a52e1c0cdc9840c4ef0df113f58c0dde6fea07b689e1549643ce`.
Retail executable:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.

Next: use the existing CopyTextureRegion hook to observe consumers of this
final target, and record bounded reasons for rejected/unscoped composition
draws. Establish the actual scene-to-presentation and UI path before choosing
a HUDless capture point. Pixel semantics, format verification, transparent UI,
matching camera/depth/motion and shared frame token, immutable input ownership
and producer fences remain pending. The separate FG chain retirement/recreation
contract must also be implemented before enabling FG in MHW. Accepted owned
FG/MFG fixture results and the previous renderer/ownership archives are unchanged.
