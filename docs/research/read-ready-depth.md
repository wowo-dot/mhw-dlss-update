# Depth preparation with the requested read bit already present

Date: 2026-09-22. Bridge stays **v0.7.9**; recorder **v0.6.1**.
All 25 local tests pass. **[Run 46868](three-frame-result.md) accepted three
consecutive corrections and the intentional fourth-fault stop**, with first-Evaluate
capture and closed/submitted depth recordings in all three CPU intervals.
GPU completion and contents remain unverified. The preparation below is historical.
This is a narrow follow-up to [the three-frame diagnostic](bounded-depth-frames.md).

## Run 7148: guard rejected before any correction

The user enabled DLSS once after ARMED, left Options and reported the crash.
Startup verified bridge v0.7.9, both providers and producer-709 channels.
Recorder v0.6.0 passed twelve live code gates and captured exactly one matching
first-chance null-backing fault at `0x1422927F2` on thread 1952.

Both snapshots completed: 105 work-item reads and 32 tracker reads, without
missing values, budget exhaustion or changed anchors. Renderer counter and
tracker/context epoch were **22062 (`0x562E`)**. The depth owner still selected
its primary backing, with a null secondary. The tracker differed from run 24944:

| Field | Observed value |
| --- | --- |
| Tracker, active and rechecked state | engine `0x4` |
| Active record's previous state | engine `0x800` |
| Tracker's previous state | engine `0xFF` |
| Requested state | engine `0x4` |
| Active operation / current operation | 2809 / 2831 |
| Same epoch, complete, stable, matching anchors | true |

The v0.6.0 three-frame policy admitted only the same-epoch `0x800` branch, so it
reported `tracker-branch`, applied **zero** context corrections and forwarded
the original fault. It detached cleanly after 324 debug events/two exceptions.
This accepts the guard and capture, not multi-frame correction behavior.

The stopped-process journal contains **219 completed calls / 545 barrier
descriptions**, with zero pending/failed slots and zero flags. All entry/return
orders 1..438 are present. The later retained snapshot is identical. There is
no Evaluate checkpoint, the mailbox is still idle, and no transition names the
observed native depth pointer. Thus the engine read bit does **not** establish
a D3D12 read declaration or completed GPU transition.

Evidence and independent checks are in ignored
`local/three-frame-depth-20260922/review-7148.json`, with the original JSONL,
crash log and exact saved-On settings. After confirmed game exit, only DLSS was
reset Off. Settings returned to SHA-256
`02BAB1311AD739420182D2FF5DB8A6ED9536235E80B51D2B736A6232633CB235`.

## Statically verified branch and recorder change

The same hash-recorded host helper compares the context and tracker epochs,
loads active state from `active+34`, then tests the requested mask. With both
state and request exactly `4`, its conditional jump at RVA `0x2292854` reaches
`0x22928C4`. That block writes the current operation to `active+8`, then jumps
to the register-restoring epilogue. It does not enter the record-allocation or
barrier-record branches. This is a prediction from captured code, not an
observed corrected execution in run 7148.

Recorder v0.6.1 adds exact live checks for that block and epilogue:

| RVA | Checked bytes |
| --- | --- |
| `0x22928C4` | `897708e9df010000` |
| `0x2292AAB` | `488b7c2420488b742418488b6c24104c8b7424285bc3` |

All **fourteen** code gates match the decrypted capture SHA-256
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
The disk executable remains version 0.0.0.0, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
All fourteen gates are required live before attachment and at every matching
fault. The fault/caller/register contract and resource lifetime assumptions are
unchanged from the prior diagnostic.

The explicit three-frame option now accepts exactly two same-epoch states:
`0x800` (previously observed record-allocation branch) or `0x4` (requested read
already present). Tracker state, active state and their rechecks must all
agree, and the compatible-read prediction must agree with the selected state.
Other masks, including `8`, `0xC`, `0xFF` and mixed `0x804`, remain rejected.
The first frame may use either branch; each later frame is checked afresh.

The three-admission limit, consecutive counters, stable twelve identities,
same preparation thread, 500 ms admission/commit window, complete capture
channels and two-second journal bound are unchanged. Output rotation remains
permitted only with a valid full work-item snapshot. RIP is unchanged; RDX is
the only corrected register. No new remote memory writes/calls or GPU commands
are introduced. The normal one-use option remains one-use.

## Verification and completed run procedure

Release `/W4 /WX` build and all 25 CTests pass. New policy cases cover both
allowed states, alternating states across three frames, fourth-attempt refusal,
disagreement between active/tracker/rechecks and rejection of all other tested
read/write masks. The owned debugger fixture supplies states `4/0x800/4` to the
policy while executing three real corrected loads, then forwards the fourth AV
to its application handler. It does not execute the proprietary helper itself.
The existing single-retry/rollback/capture tests still pass. Independent JSON
checks confirm stopped captures, immutable prefixes and one mailbox publication.

Only the external recorder changes. The installed bridge remains byte-identical
to v0.7.9, SHA-256
`FE31C1A27871D07AC33143E84269072BA1807C36300343CA3B82920FBA0AC237`.
Recorder v0.6.1 SHA-256 is
`5E7740F3B5ED850BC2D79DB262C1A44519BDA9B37D306B2D82E5EE9BC7308AB6`.
The previous v0.6.0 recorder and v0.7.8 bridge backup remain preserved. The new
recorder, source snapshot, gate review, fixture captures and preparation manifest
are archived in ignored `local/read-ready-depth-20260922/`.

The following procedure produced run 46868; do not repeat it unchanged.
Launch the existing **Monster Hunter: World - DLSS 1.2.110 test** MO2 entry
offline and stay at the main menu with DLSS Off. Verify fresh startup, then arm
archived recorder v0.6.1 with a new JSONL path, 180-second limit and
`--repair-depth-three-frames`. Only after ARMED, enable DLSS once, close the
popup and leave Options. Stay at the main menu; no save load or further toggle.
Reply by phone to avoid a display switch. Another crash is expected when a
guard or the correction budget ends the experiment.

First-Evaluate capture remains one-shot and intentionally NotImplemented;
later journal entries are CPU observations. Review each corrected frame and
the terminal fault, resource binding, list generations and actual declarations/
submissions. GPU completion, contents, temporal alignment and modern rendering
remain unverified. If stopping, do not attach the recorder; disable the bridge's
MO2 Force Load entry with the game closed. Bridge rollback is unchanged from
the prior note and requires no original game-file restoration.
