# First accepted DLSS Create capture

Date: 2026-09-07. **v0.6.2, PID 19800** completes the no-handle Create milestone.
The game reached the routed provider Create with a verified D3D12 command list,
the bridge captured all 11 planned scalar queries and deliberately returned
unsupported, and both provider shutdown paths completed. Evaluate remains
unobserved; modern rendering is unimplemented.

## Identity and evidence

The active DLL matches the staged Release v0.6.2 artifact: 331,264 bytes, SHA-256
`C2245D6D56A23DCE5AD3F449AE2370531D5C36AC9C530EE1AC87D57633CBFF48`.
The runtime verifies executable SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`
and all [pinned core/provider identities](modern-provider-bridge.md#pinned-chain),
RTX 5090, recognized `capture-create` mode and both complete routing transactions.
Both synthetic Init calls verify D3D12, application ID `0xB9D2F5C`, API `0x12`
and the complete validation mask `0x7F`. No binary or hook was changed for this run.

The complete trace is preserved as
`artifacts/logs/bridge-19800.run002.v0.6.2.log`, 60,179 bytes, SHA-256
`CF3425775758D0D96A6D50F00BB3A35B361E31CAC27BFA025D56942A3AFF6950`.
Structured evidence is in `local/sizing-20260907/runs/19800/accepted-run.json`;
the deployment manifest records this as the accepted Create run.

## Trigger and captured contract

The preceding run 20164 stayed within Options while DLSS was On and recorded
only sizing calls. In run 19800 the user enabled On, left Options to the title
screen and noticed a brief stutter. On returning to Options, DLSS was already
greyed out and Off. They noticed another brief stutter while leaving Options
to close the game. The saved settings afterward confirm DLSS Off, DX12 On,
FullScreen and 2560x1440. The trace does not measure the stutters or establish
their precise cause.

Ten sizing callbacks (sequences 58..67) completed with unchanged 2560x1440
dimensions, PerfQualityValue 0 and RTXValue 0, and verified output DLSSMode 1
and Sharpness 0. This accounts for 40 input getters, 40 completed writes and
40 verified readbacks. The separate 28 capability scalar queries also completed.

Sequence **68** records `NVSDK_NGX_D3D12_CreateFeature` for provider DLSS:

- Caller: pinned modern core `+0x67DE6`.
- Feature: 1; D3D12 command-list verification: true.
- Lifecycle: populated to create-captured.
- Decision: create-captured-fail-closed; capture committed: true.
- Result: `0xBAD00001`; token: `0x0`. No feature handle was issued.

| Key | Getter type | Present | Returned value | Getter result |
| --- | --- | --- | ---: | --- |
| Width | uint32 | yes | 2560 | `0x1` |
| Height | uint32 | yes | 1440 | `0x1` |
| OutWidth | uint32 | yes | 2560 | `0x1` |
| OutHeight | uint32 | yes | 1440 | `0x1` |
| ResourceWidth | uint32 | no | unavailable | `0xBAD00010` |
| ResourceHeight | uint32 | no | unavailable | `0xBAD00010` |
| CreationNodeMask | uint32 | yes | 1 | `0x1` |
| VisibilityNodeMask | uint32 | yes | 1 | `0x1` |
| PerfQualityValue | int32 | yes | 0 | `0x1` |
| RTXValue | int32 | yes | 0 | `0x1` |
| Hint.HDR | int32 | yes | 1 | `0x1` |

All 11 getter calls completed; the capture summary is complete with no fault
and stop reason none. The logged zero storage for absent ResourceWidth and
ResourceHeight is not evidence of zero-sized resources. Hint.HDR is a parameter
value, not validation of swap-chain format, HDR presentation or resource format.
This is a Create scalar contract only; no frame/resource descriptions are captured.

## Expected rejection after Create and cleanup

Sequences 69..71 are later sizing invocations with verified caller and accessor
bytes but admission false. The one-shot lifecycle is now create-captured, while
the sizing callback requires populated. Each returns unsupported without any
input getter, setter or output readback. This agrees with the disabled menu
state reported after applying On.

The v0.6.2 telemetry labels these skipped captures `fault-latched=true` with
`stop-reason=invalid-state` and publication decision `invalid-input`. This is
the local capture summary produced for rejected admission, not an SEH exception
or a failed host getter. Likewise, `boundary-clean=false` here means the success
path's explicit finish was not taken; it does not independently prove contention.
Final boundary-contention counts are zero. Version 0.7.0 distinguishes rejected
admission from a getter fault and explicitly finishes the callback boundary.
The old labels do not justify discarding this accepted Create capture.

Sequences 72 and 73 record successful DLSS and ISP Shutdown, from modern core
`+0x43815`. DLSS transitions create-captured to inert, and ISP populated to inert.
Both report no outstanding Init/token cleanup, no dropped bridge events, no
boundary contention and no lifecycle overflow. `trace.complete` reports
80,766 ms, quiesced true and complete two-provider startup coverage, with zero
dropped resolver events. The MHW process was closed when the log was inspected.

No Scratch, ISP Create, Evaluate or Release call was observed. No bridge handle
or rendering work was issued. The deliberate Create failure and subsequent
sizing rejection were followed by successful bridge cleanup in this run.

## Next milestone

No repetition of the Create test is required. The subsequent
[v0.7.0 review and diagnostic](one-evaluate-capture.md) corrects the dormant
handle check to account for the core's copied record and limits Evaluate to
one capture. Static ownership review is complete; runtime Evaluate and cleanup
acceptance remain pending. No rendering backend has been added.

Evaluate capture is needed to identify the actual color, depth, motion and
output inputs. Their formats, dimensions, states and frame alignment remain
later validation requirements before integrating a modern reconstruction runtime.
