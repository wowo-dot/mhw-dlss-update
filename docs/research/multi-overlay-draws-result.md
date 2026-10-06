# Multiple overlay draws: first live result

2026-10-01, v1.25.27. The controlled wingdrake trip still suspended DLSS;
the user confirmed recovery after a repeated trip. The previous one-draw
limit is no longer the rejection, but no shared-key multi-draw plan reached
successful preparation. This is a failed compatibility test with clean
recovery and cleanup, not a visual fix.

## Confirmed observations

The completed command queue contains two draws under key `0x78001004`
(stage 15, order 4100, type RVA `0x3544910`). Its sequence remains
`11,0,11,0`, with draw count/first values `4/12` and `4/0`. Both draws are
enumerated before qualification. The planner rejects the first at geometry
stage 4, phase 2: qualification `262146`, command branch 9. This phase
checks the attachment's flags, depth absence, target resource, format and
rotation; it precedes vertex decoding.

The in-process double capture at frame/epoch 18482 records:

| Property | Descriptor output | First draw | Second draw |
|---|---|---|---|
| Target relationship | Scene color | Different resource | Matches scene color |
| DXGI attachment format | Scene attachment is 26 | 24 | 26 |
| Recorded rectangle | Render extent | 1280 x 720 | 1280 x 720 |
| Pixel shader byte count | Not a draw | 1080 | 6444 |

The first draw cannot satisfy the current requirement that every draw under
the descriptor target the same R11G11B10 scene resource. Supporting multiple
draws was necessary, but that alone does not describe this effect's internal
targets. A descriptor's destination describes its final scene output, not
necessarily every draw it enqueues.

The first retained rejection is frame 17047; the separate prepared-queue
capture above is frame 18482 with the same two-draw structure. They are not
claimed to be one atomic combined observation. Private pointer values and
the exact capture remain in the local evidence directory.

## Evidence limits and next contract

The different first target is confirmed. That the second draw samples this
temporary target is a dependency hypothesis until its actual resource
bindings are captured. No GPU resource description, sampled-resource edge,
or required UV scaling for this auxiliary target was proved in this run.
External read-only attempts did not yield an accepted stable dependency
snapshot. They must not be treated as shader or resource admission proof.

Next, capture per-draw attachments and sampled inputs at the existing
completed-queue observation point. Classify scene writes, auxiliary writes,
and producer/consumer edges from resource identity and ordering. Preserve
native auxiliary work only after verifying it is independent of the expanded
scene, or propagate extent/UV handling through its proven dependencies.
Do not simply skip the first draw, remove the attachment guard, force its
format to match scene color, or add a wingdrake/monster whitelist.

The Gallery intermediate-color connection and hunter silhouette coordinate
issue remain separate unresolved observations. This run does not fix them.

## Run and shutdown

- Version 1.25.27; PID 43144, creation 134352926375121966.
- Installed DLL SHA-256:
  `3b0a49d099455ee70c677fc98b0242bb16a5dc4f54e33bff257f824a396e715a`.
- Host SHA-256:
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- DX12, RTX 5090, 2560 x 1440 output, 1280 x 720 render, saved native AA/DLSS
  Off, same Stracker-enabled profile.
- 18,056 DLSS evaluations; 3,695 unfiltered attempts; zero runtime faults.
- 3,687 overlay rejections; shared-key prepared/completed both zero.
- 105 suspension periods and 105 resumptions; not waiting at stop.
- All 21,751 GPU recordings completed and released; all 22,350 injected
  jitter changes restored. Full render scale restored.
- Live read-only verification found all 40 full native function spans and
  18 hook heads restored before exit.
- User confirmed a normal scene and normal game exit. Current DLL retained.

Private evidence: `local/multi-overlay-draws-20261001`, including the frozen
source, original checkpoints, `wingdrake-attachment-evidence.json`, full
review, external-reader attempts, collector timeline, closure receipt and
final archive manifest. Frozen prelaunch files remain unchanged.
