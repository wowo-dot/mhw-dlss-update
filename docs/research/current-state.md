# Current state

Updated 2026-10-06. The v0.6.5 scene diagnostic is built, source-frozen and
installed; runtime validation is pending. The game was closed for installation.
Keep the diagnostic DLL installed between tests. No Streamline FG plugin or MHW
generated frames have been enabled. Priority remains ordinary FG, then MFG;
performance cleanup and the configuration-based alpha follow. Work continues
from the Codex handoff in `claude-handoff-20261006.md`.

Latest accepted runtime result: v0.6.4 (`frame-generation-scene-raster-result.md`).
Its eight bounded reason-9 samples had complete raster fields and a readable
depth attachment, which the preserved one-RTV/no-depth observer excludes. All
four captures linked the exact returned final draw to qualified Reflex
PresentStart; gameplay showed one returned full-window copy from the final
target to an intermediate texture. 9,534 DLSS Performance / 11,009 Reflex On
frames, zero runtime faults, all GPU work and jitter drained, and 37 hook heads
/ 46 native functions restored. Archive manifest
`09640cee104e23c10684d96edf3d663b010f76169b40162253b38f4b216bb604`.

v0.6.5 (`frame-generation-scene-depth-presentation.md`) asks two questions.
First, do one-RTV draws with a depth attachment return and get submitted before
the qualified Present, and where are they relative to the final draw? They get
a separate diagnostic qualification with native color/depth/root/PSO agreement;
`output_known` and reconstruction admission are unchanged. Second, which
swap-chain buffer does that Present show, and does an ordered copy or draw reach
it? A scene-only presentation library records routed GetBuffer labels per
chain generation, the host index query, and one index query at Present entry.
Diagnostic events never gate capture closure. No new hooks, GPU work, resource
references, pixel reads or FG tags were added.

Local run: `local/frame-generation-scene-20261006/game-05`.
Installed DLL SHA-256: `5be04bc3cfce30628a7f4dcd2a397488ca9a338ab6b0fafdc6674dded2eec793`.
Exact 692-file `source-0.6.5` manifest SHA-256:
`7059f9a78560bfbea8159960bbfe7e58c6cde0aaef5e128be96eda5d55171393`.
Scene and default DLL builds are /W4 /WX clean; 10 native and 82 Python checks
passed. The default DLSS/Reflex target links only the default observer and
presentation libraries. Prior archives and freezes were reverified unchanged;
exact previous-slot DLL/sidecar/settings backups and removal steps are saved.

Next runtime: same MO2 profile, native AA/DLSS Off, fixed window. Arm the
600-second recorder at the main menu; capture main menu, gameplay/HUD, open
menu and gameplay after an area load. Stop intentionally, run the strict v0.6.5
review and `check_live05.py` before exit, then exit normally.

Then classify post-final composition candidates with their pipeline facts and
connect them to the presented buffer. FG pixel/format semantics, matching frame
tokens, immutable input ownership, producer fences and FG-chain retirement/
recreation remain implementation work before game FG activation.

Older dated entries: `history/current-state-log-to-20261005.md`.
