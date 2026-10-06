# Independent AA-Off continuous DLAA: admission diagnostic v0.8.1

This version investigates the [v0.8.0 stop](../research/independent-aa-continuous-result.md)
after 4,196 successful native handoffs during ordinary walking and panning.
It preserves the same continuous DLAA rendering, exact four-node admission
contract, history policy and failure latch. It does not claim to fix fault 8.

## Added evidence

The scheduler records one bounded `queue_rejection` when descriptor recycle or
insertion fails. The snapshot contains the last admitted queue, rejected queue,
source frame/epoch, tick, selected projection slot and whether recycle passed.
Owned-pass diagnostics distinguish lifecycle refusal, plan mismatch, first or
second owner read, owner change/extent, queue reads/revalidation, and individual
attachment checks. Owner and queue fields contain only the bounded structures
already used by admission. Partial reads are interpreted with the failure step.

The scheduler copies this evidence before publishing fault 8. The worker formats
only the saved copy under the existing runtime lock, without rereading native
memory or accessing a mutable owned descriptor. Later failures cannot replace
the first snapshot. There is no per-frame file output or image readback.

The `plan` step keeps the complete rejected queue so all exact contract checks
can be replayed offline, including node count, order, phase, flags, vtables and
color owners. The diagnostic does not bypass those checks or automatically
resume after failure. Native TAA remains unused. The 153 camera-distance resets
from the previous run remain a separate quality issue; the history policy is
unchanged to isolate this failure.

## Contract and verification

Hook targets, Windows x64 calling conventions, host hash, resource ownership and
GPU retirement rules are unchanged from [v0.8.0](independent-aa-continuous.md).
Only the diagnostic worker version, admission reporting and review allowlists
change. The optional reporting argument leaves existing owned-pass callers
using the same admission/removal rules.

The /W4 /WX build passed with no project compiler warnings. Eleven native
checks passed, including 1,000 alternating WARP queue submissions, 10,000
lifetime cycles, queue corruption refusal, immutable first-failure capture and
diagnostic serialization. Seven stream review and 16 startup review tests
passed. The JSON failure snapshot also passed a parse/field check.
The unchanged SDK/rendering pass retains its v0.8.0 hardware fixture evidence;
those hardware fixtures were not rerun for this diagnostic-only revision.

## Run and removal

Session: `local/independent-aa-admission-20260926/v0.8.1/`.
Use the existing bare offline MO2 profile and **Monster Hunter: World - DLSS
1.2.110 test** entry. Leave native AA and DLSS Off, Windowed 2560x1440, DX12 On.
Load the save, then arm through the sealed helper after startup passes. Repeat
ordinary walking and panning; no area transition is needed to investigate the
reported stop. This remains continuous with no fixed frame cutoff.

If DLAA stops, the worker should preserve the first refusal, finish submitted
GPU work, restore scoped jitter and disable hooks. Review the final durable log
before exit; the periodic status file can be older than final hook removal.
Exit normally before running `restore.py`. The exact prior MO2 DLL is backed up
as `previous.dll`; restoration removes only the matching output sidecar. Keep
source/tests/tools frozen through capture and closure.
