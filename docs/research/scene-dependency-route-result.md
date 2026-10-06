# Scene dependency route: native RGB mask rejection

## Observed run

v1.25.29, PID 48688 / creation 134353019717854239, executable SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Evidence: `local/scene-dependency-route-20261001`.

Main menu and save loading passed. Wingdrake travel reached dependency
discovery and then stopped the feature: scaled-route fault 6,
`effect native recipe`. This was a latched shutdown, not successful recovery.
The latest status file remained at a pre-shutdown snapshot with top-level
fault zero; the terminal JSONL and collector terminal status are authoritative.
The first user recovery report was superseded by their report of a full stop.
Gallery was not tested.

2,755 DLSS evaluations succeeded before the failure; 294 unfiltered resolves.
All 3,049 signals completed and all 3,049 main recordings were released.
All 3,571 jitter changes were restored, full render scale returned, and
40 complete native code spans plus 18 hook heads matched after shutdown.
The output-route lifetime table retained one recording reference awaiting its
second lifetime condition (Reset); do not report all output pins released in
process. Normal game exit was confirmed and process absence checked separately.

## Retained evidence and cause

A read-only reader recovered the diagnostic DLL's retained draw recipes after
hooks stopped. Existing objects were relinked to a separate local DLL solely
to generate a symbol map; section layout and complete machine-code equality
were checked against the installed build. A helper compiled against the same
source reported structure offsets. Loaded module identity and process creation
were checked; reads were bounded and repeated. No process writes, suspension,
remote execution, or game-memory dump was used.

The confirmed capture covers frames 37894–37896. Frame 37896 has the known copy
and radial shader pairs, matching geometry and input layouts. Copy uses mask
15 with blending disabled. Radial uses mask **7** (RGB), source-alpha /
inverse-source-alpha blending, and its existing RGB-only R11G11B10 output.
The constructor required mask 15 for both stages. Other dependent metadata
checks match the retained radial recipe. Neither private dependency draw ran.

`retained-recipes-confirmed-48688.json` is the corrected frame-index capture.
The initial reader mislabeled Snapshot's first pointers as frame/epoch and
selected all 32 plans; its bounded field observations remain preserved but its
frame labels must not be used. The corrected reader uses Snapshot byte 40 and
read 1,072,612 bytes, saving only selected recipe fields and shader hashes.

## Correction under verification

v1.25.30 permits RGB or RGBA writes for the qualified radial pass; the RGBA
temporary copy still requires all four channels. It preserves native blending
and write masks. Partial RGB masks, depth/stencil, logic operations, and other
existing disallowed state remain rejected. No new game hook, shader identity,
resource lifetime, or coordinate transformation is introduced.

The offline production fixture reproduces the old `effect native recipe`
failure with mask 7. The revised fixture compares native and adapted rendering
with RGB-only writes and tests partial-mask rejection. The collector also
checks component faults rather than only the top-level runtime fault. Runtime
checkpoints must prefer a terminal log over an older status snapshot.

Next live check: main menu, save loading, wingdrake travel, and—if still
healthy—the Gallery cutscene. This evidence does not establish all reported
combat effects or hunter-silhouette scaling as fixed.
