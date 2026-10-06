# Typed jitter property writer observer v1.0.0

This standalone diagnostic observes the generic boolean setter only when its
descriptor names the exact TAA2 jitter byte. It does not substitute an NGX
provider, enable DLSS, change a property value or admit rendering work.
See the [static map and unresolved source question](../research/jitter-property-writer.md).

The first test is complete: [run 44144](../research/jitter-property-writer-result.md)
records two 1-to-0 writes and a clean stop. The game exited normally, the
previous DLL is restored, and this observer is no longer installed. The
following installation and sequence describe that completed experiment.
The subsequent [v1.1.0 track-context run](../research/jitter-track-writer-result.md)
also completed and was restored. Neither writer build is currently installed.

## Hook contract

Windows x64/C++20, pinned MinHook v1.3.4 with the existing BSD license.
Host SHA-256 is
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`,
version `0.0.0.0`, PE timestamp `0x66D13CB6`, image size `0x55D3000`.
Disk/mapped identity, three live code spans, the registration/update vtable
slots and exact property-name bytes must pass before installation.

The single hook is the complete leaf function at RVA `0x218F3F0` (106 bytes).
Windows x64 arguments used: RCX is the borrowed property descriptor, DL is
the original byte value. No stack arguments are used by this reviewed target.
The observer records no return-value meaning. Its MASM wrapper forwards the
original exactly once and preserves volatile integer/vector registers around
observation, outgoing flags and LastError. It adds timing overhead.

Admission reads the descriptor name, owner and owner vtable; unrelated
properties/types are counted or skipped. A selected descriptor must have
name RVA `0x3344478`, owner vtable RVA `0x351BDA8`, flags 3, index 0 and
storage exactly owner `+0x184`. Entry and exit snapshots copy those fields and
the byte. All reads are guarded scalars of at most eight bytes. Per admitted
entry there are at most ten scalar reads, and at most seven at exit. No
unbounded strings, stacks or process-memory dumps are captured.

A bad read or unsupported selected descriptor ends observation while still
forwarding the game call. Before/after snapshots are sequential, not atomic.
Thread/order markers expose overlapping recorded calls. A differing exit byte
is retained rather than interpreted as proof of the setter's sole effect.
Pointers are followed only during that invocation; logs retain addresses for
correlation, never ownership of game objects.

One explicitly armed window is capped at 64 selected calls, 180 seconds or
1,000,000 inspected setter admissions. The following admission closes a count
limit. A shared try-lock avoids blocking the calling thread; contention is
counted. Fixed record storage requires no hook-time allocation or file I/O.
Only completed pairs are published, after a maximum two-second drain.
Unfinished original calls leave explicit pending evidence and cannot be
accepted. Original game exceptions are not intercepted or swallowed.

After capture/stop/failure, only this hook is disabled. DLL/trampoline memory
remains resident until process exit for calls already in flight. The observer
does not uninitialize another component's MinHook. Idle pre-arm wait is capped
at 30 minutes. Re-arming the same process is unsupported.

## Output, control and interpretation

The sidecar `<loaded-DLL-path>.output-dir` supplies an existing absolute
repository evidence directory. No machine-local path is compiled into the DLL.
New logs are `writer-<pid>-<creation>.jsonl`; existing files are never replaced.
Headers identify the host, DLL and experiment. Adapter inventory/D3D12 module
presence are recorded, but active adapter, actual API and swap-chain format
are not qualified by this CPU observer. GPU qualification is always false.

Events `Local\MhwJitterWriter-v1-<pid>-<creation>-1` and `...-0` arm and stop.
Tools verify the process creation time and ready/active state before signaling.

```powershell
python tools/arm_jitter_writer.py --log <writer.jsonl> --action arm --output <request.json>
python tools/arm_jitter_writer.py --log <writer.jsonl> --action stop --output <stop-request.json>
python tools/review_jitter_writer.py <writer.jsonl> --output <review.json>
```

Run read-only preflight before arming. Require the actual `armed` record;
the request receipt is not an acknowledgment. Review needs `capture_complete`
and a clean `stopped` record with hooks disabled and no pending pairs. The
Python reviewer rejects malformed/partial records, identity/order mismatches,
wrong target fields and incomplete publication. Failed samples remain explicit.
An empty valid observation is distinct from an accepted writer capture and
excludes no other writer or earlier initialization path.

## Local installation and rollback

For run 44144 it was installed at the existing MO2 force-load slot
`D:\MO2\MHW\mods\dlssTEST\mhw_dlss_bridge_probe.dll`, SHA-256
`39118AACEE231D5368E403A1018F9960559AB08D0C1F819B470DAF610B762FF1`.
During that run this filename held the writer observer.
The prior exact file is preserved at
`mhw_dlss_bridge_probe.v0.15.0.before-property-writer.dll.disabled`, SHA-256
`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
Previous callback/v0.14 backups remain untouched.

Private deployment, verification, map, build/source copies and runtime receipts
belong under `local/jitter-property-writer-20260923/`. The local `deploy.ps1`
requires the game closed and known hashes for installation/restoration. Use
`-Action Restore` after the run: restore the preserved DLL, remove only the
new output sidecar (or restore a preexisting one), and retain backups/evidence.
The original `.mode` sidecar is unchanged and ignored by this diagnostic.
No game executable/assets or saved/profile settings were edited.

## User sequence

1. Keep `barebonesdlss`, mods and Stracker disabled. Prefer offline play.
2. Launch **Monster Hunter: World - DLSS 1.2.110 test** through MO2.
3. Keep DLSS Off and TAA selected; report arrival at the main menu by phone.
4. Operator verifies the new process/recorder and arms the writer. Stay at
   the menu until told whether a load is needed; menu assignments may already
   fill the capture or identify the route.
5. If requested while recording, load one save and report arrival. No camera
   movement or DLSS toggle is required. Operator stops/reviews the capture.
6. After evidence and hook disabling are confirmed, exit normally for archive
   and restoration. An empty capture is investigated before another launch.

## Verification

Release `/W4 /WX` build passes. Five native fixture cases exercise the actual
MinHook/MASM wrapper: exact forwarding and RAX/XMM0/LastError preservation,
name/type filtering, before/after values, concurrent capacity/publication,
deadline, invalid selected descriptors, failed reads and restoration. The
fixture removes its trampoline only after joining all callers.
Ten Python checks cover acceptance and rejection behavior, including partial
JSON, process-generation/re-arm errors, wrong target fields, pending pairs,
overlapping writes, nonmatching exit values and empty captures. The static map
verification also passes. Run 44144's accepted capture, archive replay and
restoration are documented in the linked result. No observer source changes
were made during analysis of that run.
