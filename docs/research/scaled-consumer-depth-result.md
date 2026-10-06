# v1.25.5: depth failures absent; brief menu-opening skips remain

2026-09-29, process 16700 / creation 134351268006472560, DLL SHA-256
`48eebe8790b985131f5c5bd5156338c23ae16834675eb4aae4f2bc708c73f93b`.
See `scaled-consumer-depth-copy.md` for the implementation and its limits.

Startup, installed/source identities and unchanged settings passed. The quiet
baseline had 1,301 DLSS frames, no faults or missing depth copies, and one initial
output-readiness rejection. After the user opened the previously reproducing
menu, the checkpoint had 2,439 frames and no additional skips; the user initially
reported no visible skipping.

The user repeated menu close/open three times and panned afterward. The next
checkpoint had 3,753 DLSS frames, still zero missing depth copies, but four new
output-readiness rejections. The user subsequently clarified that intermittent
skipping remained during the brief menu-opening transition, possibly at a
different camera angle. **This run is not an artifact-free menu result.** An
initial clean report does not override the later user observation or skip counts.

Final totals after an announced intentional stop:

- 4,176 successful DLSS replacements and expanded output copies; 12,528 native
  downstream effect draws; all 4,176 output recordings completed and released.
- 4,181 consumer-depth attempts, accepted copies, signals, completed fences and
  released recordings; zero rejected depth proofs and zero missing depth leases.
  All admitted depth states were RT (4); the hoisted PS branch was not exercised
  by this live run.
- Five total skips, all dispatch reason 7 and all `scaled_depth_capture` output
  readiness rejections: one startup warmup plus four later frames. No SDK/runtime
  faults. Three suspension periods, including startup, all resumed.
- All 4,182 jitter changes restored; full render scale returned; no live private
  resource recordings remained. The full versioned reviewer passed.
- Read-only live memory checks verified all 36 native code gates and 17 hook
  heads restored. COM removal is covered by runtime receipts, not those native
  byte checks. The user confirmed normal exit; the latest DLL stays installed.

Armed at tick 40033453; intentional stop requested at 40163390. The collector
ended normally at 40165625, 132,172 ms after arming, with 26 snapshots/events and
no failed reads. Its timeline SHA-256 is
`fb1b70c2c610d4aed8bb403723677623f32f862f1e32b8a382c93ad705d62c0d`.

## Remaining issue and next diagnostic

`readiness-analysis-16700.json` identifies two sampled intervals, each containing
two new readiness rejections: 06:43:19.229-06:43:24.322 and
06:43:24.322-06:43:29.456 local time. Five-second snapshots bound these observations;
they do not establish an exact menu input or camera position for each frame.
The user reports menu opening, possibly at another angle. A precise correlation
to every visible flash has not been independently measured.

All route plans were built; none was counted unqualified, and no pipeline recipe
mismatch fault occurred. Warmup/native fallback draws grew from 3 to 15. That
counter also advances when an existing pipeline handles an unclaimed plan, so it
does **not** establish that twelve draws rebuilt pipelines.

The remaining rejection is inside `stream_route::ready`. Its current boolean
result combines several checks: occupied prior route, plan built/claimed/executed
state, plan identity, pipeline readiness/signature, temporal packet identity and
resource retention. These logs do not distinguish the exact rejecting predicate.
An already recorded downstream draw is one hypothesis, not a confirmed cause.

Next add bounded reason receipts at this readiness decision, including the active
route and current plan identities/executed mask. Reproduce short menu openings
with a camera-angle change. Preserve consumer-depth checks and route ownership;
do not solve this by removing readiness checks or calling the visible issue fixed.

Evidence: `local/continuous-dlss-20260929/v1.25.5`. Graphics/profile/mode hashes
remain unchanged. Keep native AA/DLSS Off and the window size unchanged.
