# One-use depth preparation argument retry

Date: 2026-09-22. The external recorder **v0.2.0** adds the explicit
`--repair-depth-once` experiment. **Run 12196 accepts the one-use correction
and the first complete Evaluate parameter capture.** The next renderer frame
hit the uncorrected fault and crashed as expected. This establishes progress
through the original preparation helper, not a permanent renderer fix or
modern reconstruction.

The unchanged one-use recorder subsequently enabled
[run 14392's resource-description capture](evaluate-resource-descriptions.md)
with bridge v0.7.4: all four resources shared the command list's device and
the selected depth backing described an R32_FLOAT texture. Actual GPU states
and depth/motion conventions remain unvalidated.

## Accepted run 12196

The user stayed at the main menu with TAA selected, enabled DLSS once after
ARMED, left Options and reported a crash. The unchanged v0.7.3 bridge verified
the host, NVIDIA RTX 5090, system/NGX identities and two-provider startup.
Create succeeded at tick 8369250 with the copied diagnostic token and a
verified D3D12 command list; all 11 creation queries completed.

The first depth fault occurred at tick **8369328**, renderer frame **32186**.
All ten code gates and all 105 scalar reads passed, with unique JSON fields and
stable pointer rechecks. The tool substituted that depth owner's primary
backing in RDX, verified the other registers and RIP unchanged, and continued
the original instruction exactly once.

The bridge then observed Evaluate on thread 44792 at tick 8369328, through
the exact core caller `+0x6704A`. Its copied creation record and bound handle
verified, and command-list admission passed. **All 17 getters completed with
no fault**, selecting `evaluate-captured-fail-closed`. The four non-null
resource pointers exactly match the first captured work item's primary
backings' native-pointer fields:

| Key | Observed value |
| --- | --- |
| Color, MotionVectors, Depth, Output | Present, non-null; each matches its work-item native pointer |
| PreviousOutput | Present, null |
| Sharpness | 0 |
| MV.Scale.X / Y | 1 / 1 |
| MV.Offset.X / Y | 0 / 0 |
| Reset | 1 |
| HDRClamp, DepthInverted, DLAAMode | Absent; getter result `BAD00010` |
| Hint.RunPrePass, Hint.RunPass, Hint.RunPostPass | Absent; getter result `BAD00010` |

Absent keys are not interpreted as zero. The provider returned its intentional
`BAD00012` not-implemented result. A later sizing callback was rejected with
zero getters because first Evaluate had consumed that admission window; this
was not a foreign getter fault.

At tick **8369359**, frame **32187**, the same depth preparation fault recurred
with a new work-item payload. Its 105 reads also completed. The tool refused a
second correction, continued normal exception handling and detached. The log
records 317 debug events, three exceptions including attach, one context
attempt and one instruction retry. The user then confirmed the crash.

This accepts the **CPU preparation path and parameter capture only**. Actual
COM resource descriptions, DXGI and swap-chain formats, depth interpretation,
resource states, motion/jitter conventions and frame alignment are unverified.
No Release or Shutdown was observed, so full lifecycle and stability acceptance
remain outstanding. No visual reconstruction success is claimed.

Both logs and exact post-crash settings were archived. With the game confirmed
closed, only saved DLSS On was reset to Off; the settings SHA returned to
`02BAB1311AD739420182D2FF5DB8A6ED9536235E80B51D2B736A6232633CB235`.
TAA, DX12 and fullscreen 2560x1440 remain selected. The executable and deployed
bridge hashes are unchanged.

| Local evidence | SHA-256 |
| --- | --- |
| `retry-12196.jsonl` | `35E5A8126CF51796DA99193D272D48ACEAD69869008248E27C4545CFFE394148` |
| `bridge-12196.crash.log` | `72FEC934664371411C464919254A0B9F6B40E90B7408F61AA1E65F94A312C3F6` |

`review_capture.py` verifies schema uniqueness, both frame records, one-use
continuation, exact pointer correspondence, query completeness, absent keys,
identity preservation and settings recovery. Its report is `review-12196.json`
in the ignored evidence directory. **Do not repeat this run.** Next work is a
bounded resource-description/state investigation using this now-confirmed
Evaluate boundary before a lasting preparation fix or vendor integration.

## Evidence and target contract

[Run 14388](depth-exception-capture.md) captured the actual opcode-23/subtype-3
payload. Its depth owner is the main-view container's +160 kind-5 texture:
primary backing +96 exists, secondary +240 is null. Preparation at host
`+0x229E892` nevertheless loads the secondary into RDX. The helper
`+0x22927F0` then faults at `+0x22927F2`, `mov r10,[rdx+0x70]`.

The downstream parameter producer `+0x2383CF0` obtains the same payload depth
owner's **primary +96**, then its native pointer +32. It passes that value as
Depth to NGX. Using primary +96 for the preparation argument is therefore a
specific hypothesis supported by both the recorded work item and the consumer.
It preserves the original state/tracker helper and its later barrier handling.
It does not establish correct depth content, formats, states or frame alignment.

The Windows x64 helper receives its scratch context in RCX, backing in RDX and
state selector in R8. At the fault, its two-byte `push rbx` has executed; the
direct caller return is at RSP+8. Admission requires RDX=0, R8=4, exact RIP and
caller `host+0x229E89E`. RBX identifies the payload and R15 its descriptor.
All objects remain host-owned, with no refcount changes or retained pointers.
The debug event pauses CPU threads; this does not imply GPU completion.

Host name, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`,
timestamp `66D13CB6`, x64 and SizeOfImage `55D3000` remain mandatory.
Ten live byte gates now include the downstream +96/+32 loads at `+0x2383DA4`.
All ten were compared with the captured executable after verifying SHA-256
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
Checks run before attach and again at the fault. These exact-build RVAs and
byte gates are diagnostic contracts, not distributable signatures.

## Admission and continuation

Capture-only remains the default. With the explicit flag, only the first
matching **first-chance** fault is eligible. The pure policy rejects missing,
unreadable or duplicate fields, incomplete captures, unexpected descriptor or
class, changed pointer rechecks, and any difference from the captured resource
metadata. All four payload resources must have their expected kind, engine
format, flags and 2560x1440 packed word; their primary/native/tracker fields
must exist and secondary fields must be zero. Depth must equal container +160
and differ from the kind-6 raw depth at +144. Both container slots, texture
owners, backings, main view and descriptor must agree with their rechecks.

The raw depth packed word is **`0x85A00A00`**, whereas the payload resources
use `0x05A00A00`. The policy retains this exact extra bit rather than treating
it as height or generalizing its meaning. The raw depth must have its original
secondary backing and null primary. It is never substituted into the payload.

One admitted attempt consumes the session budget before mutation. At the
stopped event, [SetThreadContext](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadcontext)
sets INTEGER context copied from the original, with **only RDX changed** to
the captured depth primary backing. A fresh context read checks every integer
register, RIP, RSP, EFlags and control segments. No CONTROL context is set.
A tentative change is restored if validation or logging throws before it is
accepted; restoration failure is reported and the exception remains unhandled.

Only the accepted retry receives `DBG_CONTINUE`, resuming the **same load**.
RIP is not advanced, no helper/barrier is skipped, and no instruction, owner
field, process memory, asset or executable file is patched. No remote function
is called by this tool. The host naturally continues its own renderer work.
The following matching fault is captured, passed to the game's normal handlers
and detached. Other exceptions retain NOT_HANDLED semantics. The timer,
event/read limits, cancellation and kill-on-debugger-exit protection remain.
The one-use limit is per tool session; do not reattach to spend another retry
in the same game process.

## Validation

Release `/W4 /WX` build and all **15 CTests pass**. The extended self-test:

- Captures fabricated metadata through the normal reader, then checks every
  required field's absence/unreadability, inconsistent values, duplicate keys,
  second-chance rejection, invalid context admission and exhausted budget.
- Attaches only to owned test children for capture and timeout detachment.
- Uses an image-backed assembly load `[rdx+0x70]` with null RDX. It tests a
  tentative correction and rollback, then one verified correction. The child
  must return the backing's marker from that unchanged instruction. Its next
  identical AV and an unrelated exception must reach its own handlers.

An additional ignored offline C++ replay ran the same pure policy against all
105 reads from the hash-verified 14388 record. The old duplicate schema was
checked for agreement and normalized offline; live admission rejects duplicate
fields. Replay selects that record's primary backing exactly once. It exposed
the raw depth's extra packed bit, now represented explicitly in the fixture.

v0.2.0 executable SHA-256:
`22BFEAC717950A4EEFD877538C0E04B012D64119A60ED34310DC6AD57D068F60`.
Evidence and versioned source/binaries remain ignored under
`local/depth-retry-20260922/`. The active v0.7.3 bridge remains unchanged at
SHA-256 `1F2309021E4DF15D3645C0B7C4A264F0B40D02394438DE1B3F6C65EEE1D29552`.

## Procedure used for the accepted run

1. Launch the existing offline MO2 test entry with DLSS Off, TAA, DX12,
   fullscreen 2560x1440. Stay at the main menu.
2. Inspect the fresh bridge log for v0.7.3, exact identities, recognized
   `capture-one-evaluate` mode and complete two-provider startup. Attach the
   external tool and require its ARMED event before enabling DLSS:

   ```powershell
   .\build\bin\Release\mhw_depth_exception_capture.exe <pid> <new-repository-jsonl> 180 --repair-depth-once
   ```

3. Enable DLSS once and leave Options. Avoid Alt-Tab; phone replies keep display
   changes out of this attempt. A second depth fault can still crash: only one
   argument correction is allowed. Do not repeat toggles or load a save merely
   to extend the attempt.
4. Compare the retry's context verification and completed continuation with
   the bridge's first Evaluate admission, getter/resource records, budgets and
   errors. No Evaluate is also a useful result. A callback attempt alone does
   not accept resource capture or rendering. The bridge intentionally returns
   failure and the game ignores it, so an incomplete frame remains possible.
5. Preserve both logs. If the game exits with saved DLSS On, back up the exact
   settings before resetting only DLSS Off while the game is closed.

No new DLL deployment or restoration is required for this external diagnostic.
Omitting the flag restores capture-only behavior. Ending the debugger session
removes its attachment; ending the game discards the one corrected context and
all resulting host runtime state. Existing bridge removal instructions remain
in the [runtime guide](../runtime/modern-bridge-probe-test.md).
