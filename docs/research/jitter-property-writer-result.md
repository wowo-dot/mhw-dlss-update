# Native jitter property writes: run 44144

Updated 2026-09-23. The [typed writer observer](../runtime/jitter-property-writer.md)
recorded two complete calls changing two distinct TAA2 objects' `mJitterEnable`
bytes from 1 to 0. Both calls returned to RVA `0x221D5ED`. This matches the
decoded track-value branch in the [property writer map](jitter-property-writer.md).
It identifies a native assignment route for the disabled flag; it does not
establish why that track requests zero or qualify native raster jitter.

## Identity and conditions

Host version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
PID 44144, creation FILETIME `134346513247162325`.
Observer v1.0.0 SHA-256
`39118AACEE231D5368E403A1018F9960559AB08D0C1F819B470DAF610B762FF1`.

Read-only preflight verified the process generation, pinned host/recorder,
live code gates and 135 loaded modules. MO2 used `barebonesdlss`, zero enabled
mods and its test entry's force-loaded writer observer. Saved settings were
DLSS Off, TAA, DX12, fullscreen 2560x1440. This observer performs no provider
substitution and does not change setter inputs.

The recorder was explicitly armed at the menu. The user then loaded a save
and reported reaching gameplay; the operator requested stop. Scene boundaries
and camera/input state were not instrumented. Do not assign either recorded
owner to the menu or loaded scene solely from this sequence.

## Accepted CPU evidence

| Observation | Result |
| --- | --- |
| Selected complete setter calls | 2 |
| Input, before, after | Both 0, 1, 0 |
| Caller return RVA | Both `0x221D5ED` |
| Time after actual arm | 31,703 ms and 40,907 ms |
| Descriptor/owner identity | Two distinct owners; each stable across its call |
| Selected destination | Exact TAA2 owner `+0x184`, flags 3, index 0 |
| Thread/order | Same thread; nonoverlapping pairs |
| Generic setter admissions inspected | 332,894 |
| Dropped admissions | 10, before property classification |
| Invalid snapshots / read failures / pending pairs | 0 / 0 / 0 |
| Stop | Explicit request; hooks disabled, no fatal event |

The stop request was issued 76,172 ms after arm; it is not an exact timestamp
for hook removal. Capture publication and clean stop are separately recorded.
DLL and trampoline memory remained pinned until normal process exit.

The two writes are positive evidence. The ten unclassified drops prevent an
exhaustive assignment count. Sequential before/after snapshots and the absence
of overlap among selected calls do not exclude other unobserved memory writes.
Neither owner is proven to drive the active raster output by this recorder.

## Source route mapped after capture

The captured return address matches a call at `0x221D5E8` to the verified
generic setter `0x218F3F0`. In the pinned decoded image, its enclosing routine
starts at `0x221C9E0`; chained unwind entries connect the later code fragment
to that entry. Kind 11 in the routine's dispatch table selects the boolean
branch at `0x221D5D9`.

The routine receives a track record in RDX. Its descriptor is embedded at
record `+0x10`, source header at `+0x00`, kind at `+0x68`, and selected key
index at `+0x6C`. The source header supplies a 16-bit key count at `+0x02`,
key-word array at `+0x20`, and value array at `+0x28`. The selector uses the
key words' low 24 bits and stores the chosen index before dispatch. For the
observed branch it passes `values[index] & 1` to the property setter.

Thus an input of zero means a clear low bit; the raw source byte need not be
zero. This run did not copy the source record, raw byte, key index or asset
identity. It also did not live-hash these additional caller spans: this source
explanation is a pinned offline mapping of the recorded return address.

A static parent route in `0x221F210` indexes `0x78`-byte track records, obtains
the target's property list through vtable `+0x18`, finds a descriptor by type
and name (with a name-only fallback), and copies it into record `+0x10`.
Its call at `0x221F516` supplies that record to `0x221C9E0`. This parent route
is mapped, not observed as a live call chain in this run.

The offline follow-up checks two exact spans, 22 instructions, both unwind
chains and the boolean dispatch entry against decoded SHA-256
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
It leaves disassembly and binaries private.

## Archive and next step

Evidence is under `local/jitter-property-writer-20260923/`:
`writer-44144-134346513247162325.jsonl`, preflight, arm/stop requests,
`review-44144.json`, `acceptance-44144.json`, `track-route-map.json`, the
exact observer build and 11 source copies. Final capture SHA-256:
`765B6D9E7D1688A24D5E05EF2DDF1A47DF520590E444539E33A830FDC94EFB93`.
`accept_44144.py` and `verify_track_route.py` both pass.

The user reported normal exit. Restoration independently required the game
absent and restored the previous v0.15 bridge at the MO2 slot, SHA-256
`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
The new output sidecar was removed. Four saved/profile/mode configurations
remain byte-identical. Backups and evidence are retained. No native source
change or new build was needed to analyze this run; the five native and ten
Python fixture checks remain applicable to the retained observer.

Next is a bounded source-context extension at the existing setter boundary:
first live-verify the identified caller route, then validate the track record,
count/index and source pointers during the same selected invocation. Capture
the chosen key word and raw byte alongside the existing before/after pair.
Do not dereference retained game pointers after the call, assume an asset
identity, force the flag or repeat this unchanged capture. The extension is
completed in [v1.1.0 run 42104](jitter-track-writer-result.md), which observes
the shared one-key zero-value source. GPU/raster and motion-vector qualification
remain open before modern DLAA/DLSS integration.
