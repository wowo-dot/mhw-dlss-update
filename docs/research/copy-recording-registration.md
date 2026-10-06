# Copy-first recording registration, v1.25.22

The v1.25.21 main-menu run completed 2,647 DLSS frames, then stopped during save
loading with runtime fault 45 / output-route fault 5. The terminal log and
collector agree this was an actual fault, not an assistant-requested stop.
No overlay draws had been admitted. The cutscene rectangle remains untested.

All 2,752 private GPU signals completed and private recording references were
released; all 2,754 jitter changes were restored. Full resolution returned,
and read-only live checks verified 38 native function spans and 18 hook heads.
One downstream recording remained retained until normal process exit. The user
confirmed a normal scene and normal exit. Evidence is sealed under
`local/output-recording-20260930`, frozen source `source-1.25.21`.

## Confirmed implementation gap

The new state journal observes transitions without creating a plan-local
recording. A draw imports those observations when first encountered, but the
copy path requested an existing recording only. A copy recorded on a different
command list from the temporal resolve therefore rejected valid, observed
COPY_SOURCE/COPY_DEST states merely because it was first to use that list.

A new GPU fixture separates resolve, copy/motion, grade and final output onto
four command lists. Before the correction it rejected the first copy. The saved
receipt proves the plan-local entry was absent while the exact generation's
journal knew source state 2048 and destination state 1024. After importing that
entry, the fixture completes 96 frames over all 24 CPU recording orders and
single/batched GPU submissions, with identical pixels and clean retirement.

This reproduces a real defect in the installed implementation and the same
fault class seen in-game. The runtime fault-5 log did not identify its individual
failed predicate, so the exact in-game cause still requires the next run.

## Correction and limits

`copy_override` now creates/imports a plan recording for the currently observed
command list and Reset generation, exactly as the draw path does. It still
requires both resource states, exact packet and copy fields, expected source
and destination, caller, dimensions and subresources. No binding-based state
inference or cross-list state inheritance is added. Packet submission order,
fences and reference ownership remain unchanged. No new hook is introduced;
the native copy wrapper/API calling conventions are unchanged.

Host executable SHA-256 remains
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
This correction does not change the v1.25.21 overlay qualification or geometry.
Existing invalid-state, duplicate, caller, copy-box and packet guards remain
covered by the GPU fixture. Build and verification records are in
`local/copy-recording-20260930`.

Next runtime check: start at the main menu, load the save, then test the reported
cutscene rectangle and quest abandonment before continuing the broader session.
Native AA/DLSS remain Off. Keep the diagnostic DLL installed after testing.
