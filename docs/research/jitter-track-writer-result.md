# Shared zero-value jitter track: run 42104

Updated 2026-09-23. The [v1.1.0 observer](../runtime/jitter-track-writer.md)
captured two TAA2 jitter-enable writes with valid source context. Two distinct
destination objects and track records used the same source header and arrays.
Each selected the only key, index 0, key word 0, raw value 0; the native setter
then changed that object's jitter-enable byte from 1 to 0.

This establishes a shared in-memory zero-value source for these calls. It
does not identify the file that supplied the data, author intent, the active
raster owner, or correct jitter/motion-vector behavior for modern DLAA/DLSS.

## Run identity and sequence

Host version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
PID 42104, creation FILETIME `134346528923998289`.
Observer v1.1.0 SHA-256
`2BE1934B249E5D67D61F8EED83E68C32629667F9341789B341D07C4F462B5D69`.

MO2 used `barebonesdlss`, zero enabled mods, and the test entry's force-loaded
observer. Saved settings were DLSS Off/TAA, DX12, fullscreen 2560x1440. Read-only
preflight verified the process, 135 modules and nine code spans, including the
generic setter and newly mapped caller/binding spans. The observer independently
verified five code spans before installing its single hook. Unlike run 44144,
the additional source-route bytes were verified live in this run.

The operator armed at the menu, required the actual schema-2 acknowledgment,
and requested one save load. The user reported reaching the save and the
operator stopped recording. No scene-boundary or input instrumentation was
added; neither write is assigned to a particular scene by its timestamp alone.

## Accepted source and destination evidence

| Observation | Result |
| --- | --- |
| Completed selected calls / source pairs | 2 / 2 |
| Destination before / requested / after | Both 1 / 0 / 0 |
| Return RVA | Both `0x221D5ED` |
| Destination owners / embedded track records | 2 distinct / 2 distinct |
| Source header, key array and value array | Same pointers for both records |
| Source kind / key count / selected index | Both 11 / 1 / 0 |
| Selected 32-bit key word / raw source byte | Both 0 / 0 |
| Source changes across each call | 0 |
| Source low-bit/input mismatches | 0 |
| Invalid destination / source snapshots | 0 / 0 |
| Recorded same-target overlap | 0; both calls on one thread |
| Calls after arm | 13,640 ms and 26,390 ms |
| Generic admissions inspected / dropped | 115,282 / 1 |
| Stop | Requested; no fatal event, pending pair or enabled hook |

The stop request was issued 37,828 ms after arm; actual hook disabling is
established by the separate stopped record. The source data were unchanged
within each pair, and the copied source fields also agree across calls 12,750
ms apart. That does not prove immutability between samples. The one admission
drop occurred before property classification, preventing an exhaustive count.
Sequential snapshots do not prove atomicity. Key word zero is scheduler data,
not a measured graphics frame number.

The existing [track map](jitter-property-writer-result.md) and verified caller
code explain the relationship: kind 11 passes `values[selected_index] & 1`
to the typed scalar setter. Here the raw source itself is zero. The observer
changed neither the incoming value nor the destination.

## Static scheduler identity follow-up

An offline follow-up links this track machinery to `uScheduler` and
`uSchedulerEdit`. Their type-name initialization, type getters and vtable
slots agree: registration is `0x221D850`, update is `0x221EB50`, and the
update calls the mapped track dispatcher `0x221F210`.

Registration exposes resource property `mpScheduler`, getter `0x221E380`,
and setter `0x221EF20`. The getter returns controller `+0x138`. The setter
replaces that resource pointer and resets scheduling fields `+0x178` to
float 0 and `+0x17C` to float -1 before its follow-up initialization. The
update compares those positions before deciding whether to apply track values.

This identifies a native scheduler/resource mechanism, not the exact live
controller type or resource filename. Derived controllers can share the same
track routine; this recorder did not capture the controller pointer or its
parent call chain. `verify_scheduler_map.py` checks eight bounded spans, seven
instructions, both type-name/getter/vtable relationships and resource-property
references against the pinned decoded SHA-256
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.

## Metadata correction, archive and restoration

The read-only preflight had a report-field bug: its added code-hash loop reused
the variable holding expected settings after the settings checks had passed.
The JSON settings field therefore contains the last code hash. The original
report and exact used preflight script are retained unchanged. The corrected
script uses a distinct variable, and `preflight-settings-correction-42104.json`
records settings recovered from the deployment's archived graphics file,
whose hash still matches both its deployment receipt and post-exit file.
The capture, live code comparisons and native source snapshots are unaffected.
Acceptance explicitly validates this correction; it does not silently replace
or treat the malformed field as original settings evidence.

Private evidence: `local/jitter-track-writer-20260923/`, including
`writer-42104-134346528923998289.jsonl`, preflight/correction, arm/stop requests,
`review-42104.json`, `acceptance-42104.json`, `scheduler-binding-map.json`,
the tested DLL and 11 exact source copies. Capture SHA-256:
`99277C4580904938A5146374E6119F9E02DAD3B3705ADB7A2B4CCD33EE8BF039`.
Archive replay and scheduler-map checks pass. The unchanged native build's
12 native and 19 Python fixture checks remain applicable; no new build was
needed for this analysis.

The user confirmed normal exit. Restoration independently required process
absence, restored the previous v0.15 bridge with SHA-256
`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`,
and removed the new output sidecar. Four saved/profile/mode files are unchanged.
All backups and evidence remain. No game executable or asset was edited.

## Next decision

The assignment and selected source are now observed. No unchanged writer
capture is needed. Next, map a reversible activation point in the typed native
TAA2 path and its downstream camera/projection consumers, then prepare a bounded
jitter-behavior experiment with explicit restoration and input/output logging.
No such override is implemented or deployed. Do not infer working native TAA,
raster jitter, motion-vector alignment or modern DLAA from this source result.
