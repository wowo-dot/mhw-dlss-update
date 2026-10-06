# Radial RGB write-mask live result: dependency-binding failure

## Observations

v1.25.30, PID 44728, creation 134353033973054093. Game executable SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Installed DLL SHA-256
`1cf3542717ca5a0bb113246785d39a33b35f0be0663de146d145f82368caad81`.
Evidence root: `local/radial-rgb-write-mask-20261001`.

Main-menu checkpoint passed at 736 DLSS frames; save-load checkpoint passed
at 1,523. The user reported a clean loaded scene, then a shutdown or artifact
during the requested wingdrake trip. Terminal evidence confirms a full feature
shutdown, not a recovered suspension. Gallery was not tested.

The previous `effect native recipe` rejection is gone. The production route
recorded 11 scene-copy draws and 10 radial draws before failing with
`stream dependency bindings changed` (scaled-route fault 6, terminal runtime
fault 45). Total: 2,791 DLSS evaluations, 44 unfiltered resolves. This proves
the RGB-mask correction admitted real native recipes; it does not establish
that wingdrake travel is fixed.

The corrected collector detected the nested route fault while the top-level
fault was still zero, at 2026-10-01 04:48:40.721 UTC. It preserved the terminal
event automatically. The checkpoint helper correctly preferred the stopped
terminal row over the older latest-status file.

## Retained binding evidence

With the game open and hooks disabled, a bounded read-only reader recovered
the diagnostic DLL's three latest retained plans (frames 25446–25448), their
draw recipes, and dependency-binding snapshots. Existing build objects were
relinked into a separate local DLL to obtain a map. Complete `.text` equality,
section correspondence, loaded module identity, process identity, and repeated
stable reads of retained records were verified. Structure offsets came from
a helper compiled against the same source. No process writes, suspension,
remote code execution, or unrestricted memory dump was used.

`retained-recipes-confirmed-44728.json` contains the receipts: 135 reads,
1,487,968 bytes read, selected fields and shader hashes saved. The source
compilation and link artifacts are in `retained-reader`. The initial link
attempt used the repository rather than build working directory and failed;
the second attempt succeeded. No installed binary was replaced.

Retained radial recipes have the expected RGB-only mask 7; the copy retains
mask 15. The saved temporary, occlusion, and mask texture identities agree
across the three retained frames. Frame 25448's current shader binding object
table differs from its saved table when read after shutdown. Other current
tables were unavailable or no longer readable. These are **post-stop** values
from a still-running native engine: they do not identify which field differed
at the instant of the failed check.

The current comparison re-reads an entire `HazeBindings` value at draw time,
including constants, samplers, texture owners, views, and descriptor handles,
and compares it to the preparation snapshot. The failure text combines a
read/shape rejection with an equality rejection. The exact branch and field
are therefore still unknown. Next investigation should establish the lifetime
of that mutable binding table and compare the bindings actually consumed by
the native draw. Do not merely remove resource or descriptor validation based
on the post-stop differences.

## Cleanup

All 2,835 GPU signals completed. All 2,835 main recordings were released.
All 3,325 jitter changes were restored; full render scale returned. All hook
domains reported disabled and pending callbacks were zero. Independent live
reads confirmed 40 full native code spans and 18 hook heads were restored.

The output-route lifetime table reported 2,825 retained / 2,823 released /
2 still live at shutdown. Those two references remained pinned until normal
process exit; do not claim every output-route reference was released in-process.
The user confirmed the scene was normal and exited normally; process and
collector absence were checked. Keep v1.25.30 installed as requested.

This run's failure must remain recorded as a failed runtime test with passing
cleanup. Combat effects, Gallery, and hunter silhouette scaling remain open.
