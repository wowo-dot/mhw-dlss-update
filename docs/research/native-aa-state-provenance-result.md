# Native state provenance capture, v1.21.2

2026-09-27, PID 19116, creation FILETIME 134349940773007435. Game closed
normally; process absence checked. The diagnostic remains installed at the
user's request. No settings, rendering admission or game assets were changed.

## Result

The capture locates the native planned state for the four previously unexplained
local-state rejections. It does not yet change their admission or prove complete
AA exclusivity. The final log passes the frozen reviewers, with one separately
saved live checkpoint failure retained and explained below.

| Checkpoint | DLAA evaluations | State rejections | Review |
| --- | ---: | ---: | --- |
| Baseline | 1,072 | 2 | All eight channels pass |
| Area load | 3,566 | 4 | Seven pass; live copy GPU coverage fails |
| Return to scene | 4,973 | 4 | All eight pass; suspension recovered |
| Intentional stop | 6,101 | 4 | All eight pass; full log passes |

User reported a clean scene after the area load and after returning to the
scene for another walk/pan. Runtime fault, SDK failure and request collisions
were zero. All 6,463 SDK/depth and 5,107 unfiltered recordings were submitted,
completed, reset and released. All 11,622 jitter scopes were restored; pending
callbacks and live recording pins were zero, all hooks disabled. The test was
intentionally stopped after 144.812 seconds between arm/stop requests.

## Confirmed captured state

Each retained snapshot has a unique consumer in the schedule, a complete
bounded predecessor window, and readable/complete trackers whose headers remain
unchanged on reread. Renderer/tracker epoch equals the consumer frame epoch +1
in all four samples. This is an observation, not a universal epoch rule.

| Sample | Frame | Consumer index / schedule size | Color source | Output source |
| --- | ---: | --- | --- | --- |
| 1 | 57598 | 5843 / 5987 | Earlier compute packet 5841 | Initialization at 5841, same-state use at 5843 |
| 2 | 57599 | 5841 / 5985 | Current consumer | Initial planned state already UAV |
| 3 | 61297 | 368 / 594 | Earlier compute packet 366 | Initialization at 366, same-state use at 368 |
| 4 | 61298 | 1373 / 1595 | Current consumer | Initial planned state already UAV |

Native state words below are not D3D12 flags:

- Samples 1 and 3: a packet two schedule entries earlier contains color
  `0x100 -> 4` and output `0 -> 0x400` state nodes. The color span identifies
  that earlier packet, and its last-use ordinal extends to the TAA consumer.
  The consumer itself contains output `0x400 -> 0x400` and an API UAV barrier,
  but no local color transition. This explains why the local-only proof fails.
- Samples 2 and 4: color transitions locally. The output tracker's initial
  state is `0x400`, and its first use span is the current consumer's
  `0x400 -> 0x400`. No output transition appears in the retained earlier window.
- In every sample, output's final planned state is `0x20` and color's final
  state is `8`, despite their current uses requiring `0x400` and `4`. Therefore
  reading the tracker's final state directly would be wrong for this pass.
- Motion has the expected local API transition in every rejected sample.

The earlier color transition and a shared read span do not establish that the
planner's barrier-hoisting optimization caused these samples. Compatible read
reuse also explains this arrangement. Captured native scheduling intent is not
an independent observation of GPU execution ordering/state. Native tracker and
span pointers remain transient; resource COM ownership does not pin them.

## Two additional observations

After the area report, evaluations stopped advancing at 3,566 while unfiltered
copies continued. Dispatch reason 7 means the current depth lease did not match
the list/generation/frame requirements (or was absent/already used). No SDK
failure was latched. Returning to the scene and walking/panning produced 1,407
additional DLAA evaluations at the next checkpoint; all suspensions recovered.
Final reason-7 count was 4,748. Exact focus/menu correlation and the individual
lease predicate responsible are not captured; do not label this a proven
focus behavior or an unrecoverable feature failure.

The area checkpoint's `unfiltered GPU coverage` failure is preserved. It shows
1,610 created, 1,609 submitted/reset, 1,608 released, two live, one awaiting
submission/reset and zero awaiting GPU, with completed fence 1,609. One reset
recording had completed by serialization but had not yet been collected.

Source inspection confirms that runtime `write` calls unfiltered `collect`
before writing other diagnostics; `UnfilteredLifetime::write` then reads the
fence again for the completed field and again for `RecordingPins::write`.
GPU progress between those reads can produce exactly this snapshot. The
reviewer's lower bound assumes collection and reporting share a completion
sample. This is a telemetry consistency defect, not evidence of early release.
The later scene/final snapshots drain fully. All continuous rows in the final
JSONL pass the unchanged full reviewer; that does not erase the separately
captured checkpoint failure.

## Next implementation work

1. Use a single completion observation for collection and serialization of
   unfiltered lifetime telemetry, preserving fault-time pin retention. Add a
   deterministic regression case for completion advancing between observations.
2. Design a bounded current-use state resolver. Keep the existing API proof;
   any additional native proof must bind resource/backing identity, planning
   epoch, packet use, metadata lifetime and command execution ordering. Do not
   substitute the final tracker state or treat a UAV barrier as a transition.
3. Retain the recovered depth-lease suspension as an unresolved observation;
   separate lease predicates if further diagnosis is required.

CMFM/UI, resolution-changing DLSS and frame generation remain deferred.

## Preserved artifacts

Root: `local/native-aa-state-provenance-20260927/v1.21.2/`.
`run-result.json` includes replay-checked sample associations and the checkpoint
caveat. `finalize_run.py` verifies the frozen sources, all prelaunch files,
installed DLL/sidecar, previous-slot backup and four unchanged configurations.
It does not restore or uninstall the diagnostic.

- Installed DLL SHA-256:
  `c256f8c970f2d39b42f4af7ebebd0e1e99f48447831ea5ec039562036e89e881`
- 453-file final archive SHA-256:
  `5fbc9ba3be94585d364464d66a32ae09b03091387f08edf1e4f6004e42f76a96`
- Final log SHA-256:
  `0e7b81182e336f126e05f9bd06fb8718ff3fe8677225a81f940b6801eb8e3024`

No source changes to the tested binary were made during this run. The 39 native
and 128 Python preparation checks remain its build validation; live verification
adds the captured evidence and caveats above.
