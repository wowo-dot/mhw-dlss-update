# Gallery native compute integration — live result

The previously affected Gallery cutscene ran with DLSS active and the complete
native blur sequence connected. The user reported a clean cutscene and gameplay,
then a clean area change through a loading screen. This validates the tested
Gallery path at 2560x1440 output / 1280x720 render; it is not a claim that every
cutscene or monster effect is covered.

## Run and evidence

Candidate v1.25.41, PID 50900, process creation `134353360807718159`.
DLL SHA-256:
`797d937e8203b1c2d5ab8ab0079559fed15cf02ef7c88a536316273f4c548dbc`.
The test was armed in the loaded save, rather than at the main menu. Startup,
seven module identities, four configuration files and 805 prelaunch artifacts
passed verification. Native AA and DLSS remained Off; the DX12 window remained
2560x1440. The recorded Stracker-enabled profile was unchanged.

The live sequence gate accepted 886 plans. Two frames warmed the private PSOs
(12 native warmup dispatches), followed by 884 complete six-pass sequences and
884 matching composition draws:

| Check | Result |
| --- | ---: |
| Completed native compute sequences | 884 |
| Dispatch receipts, each of six passes | 884 |
| Total admitted compute dispatches | 5,304 |
| Adapted pass 0 / pass 3 receipts | 1,768 |
| PSO/predication restoration receipts | 1,768 |
| Sequence, binding or composition rejections | 0 |
| Runtime / output-route faults | 0 |
| DLSS evaluations across the run | 2,892 |

The first cutscene checkpoint had 72 complete sequences. Later checkpoints
reached 884 while DLSS continued evaluating. After leaving the cutscene, the
sequence count stopped increasing while DLSS frames continued; the subsequent
area transition also returned to active reconstruction. The user reported no
rectangle, softness, flickering or broken blur in this test.

There were six skipped dispatch observations across the whole run: two at the
initial baseline, two more around Gallery entry, and two more by the area-change
checkpoint. These are not six measured displayed-frame drops. There was no
sustained Gallery suspension, no rejected output plan, and all four suspension
periods recovered. Visual confirmation supports this result; no new live pixel
readback or independent temporal-quality comparison was collected.

## Stop and preservation

The stop was announced and intentional. All 2,898 main GPU recordings and five
unfiltered recordings completed and released their recording references. The
output route released all 2,946 retained recordings; 12,084 jitter changes were
restored, full render scale returned, and every hook reported disabled. Read-only
live checks independently verified 40 complete native code spans and 18 hook
heads against the recorded identities.

The frozen full-log review and all 14 component reviews passed. The bounded
collector ended on the healthy native terminal event, with no observed fault;
one status-file read was retried. Its timeline is retained for chronological
review. The user confirmed a normal scene and normal exit. Exact closure,
configuration and archive checks are recorded in
`local/dof-native-compute-route-20261001/closed-result-50900.json` and
`archive-final.json`.

The current DLL stays installed as requested. Its output directory is now an
archive: select a fresh output directory before another launch. The next work
is the previously reported monster-effect interruptions, followed by hunter
silhouette scaling. Camp-entry behavior remains deferred. The original native
view contents are still not independently decoded; this implementation preserves
the actual native bindings instead of reconstructing them.
