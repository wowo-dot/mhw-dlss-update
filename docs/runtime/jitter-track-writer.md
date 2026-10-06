# Selected track context observer v1.1.0

Prepared 2026-09-23 after [writer run 44144](../research/jitter-property-writer-result.md).
That run captured two 1-to-0 writes returning to `0x221D5ED`. This extension
observes the selected key and raw value at the same generic setter boundary.
The first capture is complete: [run 42104](../research/jitter-track-writer-result.md)
records two matching source pairs with shared one-key, zero-value data. The
game exited normally, the previous DLL is restored and the new output sidecar
removed. This document describes that completed installation and protocol.

## Hook and source contract

The single target remains RVA `0x218F3F0`, Windows x64: RCX is a borrowed
property descriptor, DL is the requested byte, and no stack arguments are
used. The MASM wrapper is unchanged: original called once, volatile register
and outgoing flag/LastError preservation, no input forcing. Timing overhead
remains. The [v1.0.0 contract](jitter-property-writer.md) describes the existing
typed destination checks, fixed storage, limits and hook lifetime.

Host version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
Decoded reference SHA-256
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
The three existing live code gates are joined by `0x221C9E0..0x221D824`
and `0x221F39D..0x221F827`, including the selector, boolean branch, dispatch
table and descriptor-binding code. All five must match before hook installation.
The offline map verified their hashes, unwind relationships and 22 instructions.

Source reads apply only to selected TAA2 jitter descriptors whose call returns
to `base + 0x221D5ED`. Other callers still receive destination observations
but no inferred track traversal. For the selected route:

| Field | Validation / copied evidence |
| --- | --- |
| Record | Descriptor minus `0x10`, arithmetic checked |
| Record `+0x08` owner | Must equal the typed destination owner |
| Record `+0x68` kind | Must equal 11 |
| Record `+0x6C` selected index | Must be below nonzero 16-bit key count |
| Record `+0x00` source header | Nonnull, checked address arithmetic |
| Header `+0x02`, `+0x20`, `+0x28` | Key count, key-word pointer, value pointer |
| Selected key / value | One 32-bit word and one raw byte; no array scan |

Before and after snapshots retain these fields during the original call.
They add at most nine guarded scalar reads on each side; selected entry/exit
totals are at most 19/16 reads, each at most eight bytes. No pointers are
followed later by the logging worker, and no strings, asset contents or stacks
are captured. Source failure 1 means an unreadable scalar; failure 2 means
an invalid destination/layout/owner/bound. Either ends observation while the
original setter still receives its original input exactly once.

The raw source byte is not assumed boolean: native code passes its low bit.
A source/input mismatch or changed context is retained without claiming a
stable source relation. The reviewer distinguishes accepted destination calls
from `accepted_source_context`, which additionally requires all selected track
pairs to be valid, unchanged, low-bit matching and free of recorded same-target
overlap. Sequential snapshots do not prove atomicity or exclude other writers.
This does not identify a source asset or qualify active raster/GPU ownership.

## Control and limits

The JSONL header uses schema 2 / version 1.1.0 and `armed.live_spans` is 5.
Each write adds `track_before` and `track_after`. Control event names use
`Local\MhwJitterWriter-v2-<pid>-<creation>-<0-or-1>`. The existing arm/review
tools support both versions; v1.0.0 evidence and its exact source copies remain
in the previous archive. Do not manually signal old v1 events for this build.

One explicit arm admits at most 64 selected calls, 180 seconds or 1,000,000
generic setter visits. Try-lock drops are counted. No rearm; bounded drain;
only this hook disabled on termination; DLL/trampoline memory retained until
exit. Provider substitution is false. Adapter inventory and D3D12 presence are
logged, but active API/adapter and swap-chain format remain unqualified.

## Completed deployment and run protocol

During the run, MO2 slot `D:\MO2\MHW\mods\dlssTEST\mhw_dlss_bridge_probe.dll` held SHA-256
`2BE1934B249E5D67D61F8EED83E68C32629667F9341789B341D07C4F462B5D69`.
The exact previous v0.15 bridge is preserved at
`mhw_dlss_bridge_probe.v0.15.0.before-track-writer.dll.disabled`, SHA-256
`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
Existing backups remain untouched. The new output sidecar pointed to repository
`local/jitter-track-writer-20260923/`; mode and saved/profile files are unchanged.
No game executable or asset was edited.

Use `barebonesdlss`, no mods or Stracker, and **Monster Hunter: World - DLSS
1.2.110 test**. Prefer offline play. Leave DLSS Off/TAA and report arrival at
the menu by phone. The operator runs the new directory's read-only
`preflight.py --log <path>` before using `tools/arm_jitter_writer.py`.
Preflight now also checks the generic setter and both added caller spans.
Require the actual `armed` receipt, then request one save load if still active.
No DLSS toggle or camera panning is needed. Stop after the loaded-save report,
review the evidence, require clean hook disabling, then request normal exit.

After exit, `local/jitter-track-writer-20260923/deploy.ps1 -Action Restore`
requires game absence and expected destination/backup/sidecar hashes, restores
the previous DLL and removes only the new output sidecar. Keep the archive.

Release `/W4 /WX` build passes with no new project warnings. Twelve native
cases exercise the real hook, including raw value 2, selected indices, source
changes, foreign caller filtering, out-of-range index, guard-page reads, owner
mismatch and address overflow, plus the five previous cases. Nineteen Python
cases cover both schema versions, control names, source bounds, failure
retention, mismatch/change reporting and overlapping calls. All 31 pass.
The result also documents a corrected preflight report-field bug; the original
receipt and used helper are preserved alongside an explicit settings correction.
