# Request-lifetime diagnostic: successful comparison, no reproduction

Run 10652 (creation 134349011993956692), 2026-09-26, used v1.18.3 in Windowed
2560x1440 DX12, native DLSS Off/TAA On. The user reported normal images after two
separately cued area-loading transitions and a subsequent stationary focus cycle.
Every checkpoint showed DLAA still active with fault 0. Final telemetry review
passed; the earlier v1.18.2 fault 6 did not recur. No retirement policy was fixed.

| Checkpoint | DLAA evaluations | Published TAA | Matched TAA | Resources |
| --- | ---: | ---: | ---: | ---: |
| Loaded baseline | 2,526 | 2,526 | 2,526 | 6 |
| First area return | 6,172 | 6,230 | 6,230 | 9 |
| Second area return | 9,344 | 9,511 | 9,511 | 12 |
| Focus return | 13,050 | 13,220 | 13,219 | 12 |
| Clean stop | 14,245 | 14,415 | 14,414 | 12 |

Final closure: 14,277 private signals/depth copies completed, 15,074 jitter
injections restored, zero pending callbacks/private recording, all hooks disabled.
There were 444 history resets, 170 skips and 21 tracked lists. Retained-history
evaluations totaled 13,801, with a final/longest streak of 3,985. Reset reasons
overlap; sparse checkpoints do not attribute each reset or skip to a user action.

Eleven TAA and twelve depth consumer calls met the publishing guard. The saved
first/latest samples matched no live record; they do not describe every
intermediate blocked call. No collision, changed packet, multiple matching
generation or reentrancy was recorded. All published TAA requests had been
consumed at both area checkpoints. The final unmatched tracking entry does not
represent pending private GPU work and does not by itself reproduce the fault.

## Interpretation and next investigation

Area loading and focus changes can succeed in this instrumented build. This is
not evidence that the prior fault was caused by either action alone, or that it
is repaired. Added mutex acquisition/scanning on the publication-blocked branch
can change overlap timing. The user chose desktop replies, so uninterrupted
focus around area loading is not established. The explicit focus cycle was
cued only after both area checkpoints and did not include another requested load.

Review native publication/consumption and cancellation lifetimes offline before
selecting a retirement rule. In particular, a consumer can encounter an in-flight
producer before that producer publishes its record; a snapshot of live records
alone cannot identify that incoming record. Consider bounded event-time logging
that avoids adding a blocking lock to this branch. The existing same-frame and
wraparound fixtures show possible mechanisms, not the actual previous cause.
Do not enlarge the ring or silently overwrite a live entry as a correctness fix.

Native TAA still executes. The agreed next feature is its qualified-frame bypass
with native fallback, after resolving request lifetime. No objective live image,
performance, resize or full device/swap-chain recreation claim follows from this run.

## Evidence and restoration

Evidence root: `local/dlaa-request-lifetime-20260926/v1.18.3/` (ignored).
262-file archive SHA-256
`f6bb29c4260f77e14c2ba88fa742e418bcb5d8857e50cd2e227f9d9c93b56850`.
Prelaunch: 240 files, 211 frozen sources; SHA-256
`d9660a3cac4704decac2179b9423c7cb6aa7617646ec4174ee5273fb28cab83f`.
Test DLL `470b48c2ef625489cee7aac7bbf58938cbe706fa3c7d4946b9114d8509cb4958`.
Final log `9e52b33d7d00ad05a08ef4330332c57d33a5a569bf77af45402f4036dc413b74`.
Startup log `e0bb2ed9241e4954f213f7adc101431f9a2fce7ad7fee6107191a93aec35880a`.
Adapter [0,98931], swap-chain format 87. Arm-to-stop requests span 175.094 seconds.

The user confirmed normal exit. The process was gone before restoration, which
completed at 16:05:18 +03. Exact previous DLL `689bef7c...` restored, sidecar
removed, all four configuration hashes unchanged, including Windowed settings.
The assistant made no game asset or graphics-setting writes. The finalizer
rechecked the prelaunch archive and independently replayed the full review and
all four checkpoint request/history reviews before sealing the archive.
