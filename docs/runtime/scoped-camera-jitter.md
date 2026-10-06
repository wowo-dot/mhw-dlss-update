# Scoped camera-jitter experiment v1.0.2

Prepared after [track run 42104](../research/jitter-track-writer-result.md).
Run 25448 with v1.0.0 stopped at hook installation, recording zero visits and
zero injections. The hook was reported disabled, the game exited normally and
the previous DLL was restored. Its exact MinHook stage/status was not logged;
the cause is unresolved. Run 5152 with v1.0.1 installed successfully, but its
first baseline call was rejected before injection. It exposed an incorrect
raw `mUseDLSS == 0` eligibility rule: that renderer switch can remain true with
the menu setting Off and no wrapper. Version 1.0.2 corrects the rule, records
provider snapshots and admission checkpoints. It now has accepted CPU evidence
from [run 17072](../research/scoped-camera-jitter.md#run-17072-cpu-stimulus-and-recovery-accepted):
baseline, one nonzero sample and recovery all pass. The game exited normally,
the previous DLL is restored and the output sidecar removed. This document
describes the completed installation and protocol. The standalone probe tests one nonzero native
sample at the projection consumer, followed by a zero-jitter recovery call.
It does not change TAA2's enable flag or substitute a modern provider.

## Target, identity and lifetime

Host version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
Decoded reference SHA-256
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
No address is accepted without exact host, mapped PE and live-code checks.

The only detour is the complete 2000-byte function at RVA `0x228F3B0`.
Windows x64 ABI: RCX is a borrowed embedded projection object, RDX is the
recording context, and AL returns a bool. Both arguments and the result are
forwarded exactly once; incoming/outgoing LastError are preserved. Undefined
volatile register results and flags are not an additional return contract.

The camera manager is a separate object from RCX. Its two accepted vtables,
`0x350D188` and `0x33F82A8`, have slot `+0x48 -> 0x228E770`. That caller
iterates eight `0x1A0`-byte entries starting at camera `+0x50`, calling the
helper at `0x228E82F` and testing AL at return address `0x228E834`.
The experiment accepts only that return address, entry zero, active byte
`entry+0x20 != 0`, display index `entry+0x25 == 0`, and context equal to
renderer `+0x710`. It does not treat the embedded entry as a vtable-bearing
camera or claim this slot is proven to drive the rasterized image.

Eight complete functions and one provider-selection block must match before installation:

| RVA | Bytes | Role |
| --- | ---: | --- |
| `0x228F3B0` | 2000 | Projection helper; sole hook |
| `0x23D0E70` | 5321 | Constant publication |
| `0x23CCC50` | 203 | Constant allocation |
| `0x28E280` | 1030 | Matrix multiplication |
| `0x1FAD4C0` | 211 | Matrix copy |
| `0x227FF00` | 1116 | Native sample-table construction |
| `0x227DB60` | 52 | Native sample selection |
| `0x228E770` | 528 | Embedded-entry caller |
| `0x2392A18` | 62 | Native DLSS provider-selection block |

Digests are pinned in `src/jitter_activation/recorder.cpp` and the ignored
`activation-map.json`. Both virtual slots are checked live. The 12 relocated
prefix bytes contain only pushes and LEA, with no relocated call. Original
body calls therefore retain the native function's unwind metadata. The fixture
deliberately keeps its body call outside the relocated prefix too; it does not
claim generic exception support for calls inside arbitrary MinHook trampolines.

Pointers are borrowed only during the original call; no COM retention or later
pointer traversal is performed by the logging worker. This assumes the game's
camera and context remain alive during their own projection call. Guarded
reads and compare/exchange detect some changes, but do not prove ownership
against every possible uninstrumented concurrent writer or allocation reuse.

## Three-call protocol

One arm allows at most 1500 ms, 256 visits and three selected calls. Repeated
calls in the same frame/epoch are skipped. Later stages require strictly
increasing frame and epoch, unchanged camera/type, renderer, context, view,
table and calling thread. Each admission requires zero stored jitter, a current
camera epoch, clear special-view flags, provider exclusion below, valid bank/async state,
and a finite, nonzero native 20-entry sample with each component within ±0.5.

Provider exclusion requires a nonnull renderer backend at `+0x78`, clear bit 0
of renderer sizing flags `+0xB4`, and a null wrapper at backend `+0x42700`.
The raw renderer switch at `+0xA5` must be a valid boolean and is recorded; its
being true is allowed. The pinned native selector requires that switch, sizing
bit 0, backend readiness and a wrapper before taking its DLSS path. This probe
requires both sizing and wrapper exclusion, with identical before/after
provider snapshots. These sampled conditions do not establish atomic ownership
against an uninstrumented concurrent writer. Menu Off/TAA is still required.

1. **Baseline:** call the original without mutation; require valid CPU matrices
   and the independently mapped zero-jitter relation.
2. **Stimulus:** compare/exchange exactly eight aligned bytes at camera `+0x38`
   from zero to the current `table+0xE6A0+8*(frame%20)` sample. Call the original
   once, capture its CPU projection constants, then restore the eight bytes
   immediately using compare/exchange against the injected value.
3. **Recovery:** on a later frame/epoch, call the original with zero stored
   jitter and capture the resulting zero-jitter matrix relation.

Only committed PAGE_READWRITE storage is eligible for mutation. No protection
changes are used for camera data. The `__finally` handler attempts restoration
on normal return and SEH unwind; exceptions propagate. An unexpected competing
value is preserved and makes the experiment invalid. Hard process termination
is not an unwind guarantee. There is no repeated forcing or retry.

The stimulus's generated constants intentionally remain jittered after the
camera field is restored. Restoration of the field does **not** undo a rendered
frame or temporal history. The next unmodified projection call produces its
usual constants. A transient image/history disturbance or crash remains possible.

An exclusive try-lock spans the selected original call. Contention ends the
experiment and counts a drop, while competing native calls still run normally.
Stop/drain synchronizes with admission, including its pre-publication window.
There is a two-second drain bound; pending or invalid results cannot pass
review. Only this hook is disabled. DLL and trampoline memory remain pinned
until process exit so callers already in flight cannot return into freed code.

## Evidence and limits

Schema 1 / version 1.0.2 logs `scoped-camera-jitter`, three sample records,
input/source identities, restoration results, projection/unjittered matrices,
viewport, layout, and terminal bounds. No raw memory dumps are taken.
Control names are `Local\MhwJitterActivation-v1-<pid>-<creation>-<0-or-1>`;
both the header and live process generation must agree.

Version 1.0.1 records installation details on both `armed` and `failed`: stage
1 initialize, 2 create, 3 enable; MinHook status and name; observed Win32
LastError; and whether the hook was created/enabled. LastError is supplemental
evidence, not a documented MinHook error contract. These are the installation
results before cleanup, while `stopped.hooks_disabled` describes cleanup.
The reader retains support for v1.0.0 and requires successful installation
details for acceptance of v1.0.1. No mutation budget or retry is added.

Version 1.0.2 additionally logs `provider_before` / `provider_after` with
validity, backend, wrapper, raw switch and sizing flags. Rejection 9 means an
invalid or changed exit provider snapshot. `admission_checkpoint` identifies
the last attempted check: 1 global objects, 2 typed camera/context, 3 embedded
entry, 4 jitter/stamp, 5 view, 6 provider exclusion, 7 bank/async, 8 native sample,
100 complete admission. Failed reads or invalid values retain that checkpoint
and whatever earlier values were read; partially read groups are not valid
snapshots. The reviewer requires checkpoint 100 and equal excluded provider
snapshots to accept v1.0.2. It still reads both older versions.

The offline reviewer independently checks `Pj-Pn`: columns 0/1 equal the
unjittered row's homogeneous coefficient times jitter divided by viewport
width/height, and columns 2/3 have no change. It rejects a stimulus with no
measured matrix difference, wrong scale, changed anchors, incomplete stages,
failed restoration, contention, or an unclean stop. CPU acceptance does not
qualify GPU upload, raster displacement, motion-vector sign/scale, temporal
history, native TAA activation, or working DLAA/DLSS. Adapter inventory and
D3D12 module presence are logged; active API/adapter and swap-chain format
remain unqualified by this standalone CPU probe.

## Installation and operator sequence

During run 17072, MO2 slot `D:\MO2\MHW\mods\dlssTEST\mhw_dlss_bridge_probe.dll` held
SHA-256 `63737E4102453B7ED7569CCD32CE7CDB29ABC54B06669BAE448E6CF205DD0B5B`.
The exact previous DLL was preserved as
`mhw_dlss_bridge_probe.v0.15.0.before-jitter-activation.dll.disabled`, SHA-256
`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
That previous DLL is now restored. The new `.dll.output-dir` sidecar pointed to repository
`local/jitter-activation-20260923/provider-gate-1.0.2/` and is now removed. Existing backups are retained. Saved
Off/TAA, profile and mode files are unchanged; no game executable or asset was
modified. Source/binary copies, static map, logs and restore helper are retained
in that ignored evidence directory. The following sequence is historical;
no unchanged rerun is needed or follow-up probe installed.

Use profile **barebonesdlss**, with no enabled mods or Stracker, and executable
**Monster Hunter: World - DLSS 1.2.110 test**. Prefer offline play. Keep DLSS Off
and TAA selected; report the main menu from the phone. The probe starts idle
without hooks and permits 30 minutes before arming. No MO2 restart is needed.

The operator runs `local/jitter-activation-20260923/provider-gate-1.0.2/preflight.py --log <path>`
before installation of any hook. It checks live host/dependency code, virtual
slots, loaded modules, profile/settings and the exact deployed binary. After
preflight, ask for a loaded save and an idle scene. Only then use
`tools/arm_jitter_activation.py --log <path> --action arm --output <receipt>`.
No toggle or pan is needed. The three calls should complete automatically;
require actual `armed`, `capture_complete` and clean `stopped` receipts, then
run `tools/review_jitter_activation.py`. Retain failed/empty captures too.

After capture and normal game exit, run
`local/jitter-activation-20260923/provider-gate-1.0.2/deploy.ps1 -Action Restore`. It requires
process absence and unchanged destination/backup/sidecar hashes, restores the
previous DLL, and removes only the newly introduced output sidecar. Archive
the capture and verify saved settings before another experiment.

Release `/W4 /WX` build passes without project warnings. Sixteen native cases
exercise the real MinHook path: baseline/stimulus/recovery, baseline mismatch,
exception unwind, restoration conflict, deadline, inactive input, read-only
camera memory, contention, changed camera, repeated frame, pending stop and
provider-wrapper/sizing/backend rejection, a provider change during the
stimulus, and a deliberately non-executable hook target whose
failure must retain the exact MinHook stage/status. The owned fixture now uses
the mapped native stack-prologue shape; normal forwarding and exception
restoration pass. This does not reproduce the live installation failure.
Twenty Python cases check independent projection
math, restoration, bounds, malformed/incomplete evidence and process controls.
All 36 pass. Normal native fixtures now use the observed raw-switch-true,
sizing-disabled, wrapper-absent state. New cases verify provider snapshots,
partial admission and restoration after a provider change.
These preparation checks apply to the unchanged binary. Run 17072 additionally
passes CPU experiment acceptance; GPU/raster and motion qualification remain open.
