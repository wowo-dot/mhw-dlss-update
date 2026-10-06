# Exact work-item capture confirms the depth backing mismatch

Date: 2026-09-22. **Run 14388 confirms the actual failing payload**, resolving
the missing heap evidence in [run 44972](taa-activation-depth-crash.md).
An external Windows x64 C++20 recorder captured the first-chance exception
before normal game exception handling, then detached. The v0.7.3 bridge DLL
and all game code were unchanged. This accepts the diagnostic capture, not
Evaluate or rendering. The subsequent [v0.2.0 argument retry](depth-argument-retry.md)
reached the first Evaluate capture in run 12196; the contract below describes
the original capture-only run and v0.1.x behavior.

## Confirmed runtime result

The user started offline with TAA selected and DLSS Off, enabled DLSS once
at the main menu and left Options. The companion bridge log records successful
DLSS Create at tick 7029203, with a verified D3D12 command list and 11 completed
scalar queries. No Evaluate, Release or Shutdown was logged before the crash.

The external recorder verified the executable identity and nine code gates
before attachment and again at the fault. It observed:

- First-chance `C0000005`, thread 38532, reading `0x70` at host RVA `0x22927F2`.
- RDX=0, R8=4 and direct stack return host+`0x229E89E`.
- Descriptor opcode 23, subtype 3, pointing to the exact RBX payload.
- All **105 scalar reads completed**, no exhausted budget, and all recorded
  pointer rechecks agreed. The renderer frame was 23190 (`0x5A96`).
- The payload's depth owner was exactly the main-view container's **+160**
  object, distinct from **`mpRTDepth` at +144**.
- The depth owner had **kind 5, primary backing +96 present, secondary backing
  +240 null**. Its primary native-pointer and tracking fields were nonzero.
- `mpRTDepth` had kind 6, primary backing null, secondary backing present,
  with nonzero native-pointer and tracking fields.

The captured resource owners all identify as `nDraw::Texture` and report
2560x1440 dimensions. Their engine metadata is:

| Actual payload member | Kind | Engine format ID | Flags | Backing selected by host preparation |
| --- | --- | --- | --- | --- |
| Color | 5 | 34 | `0x200002` | +96, present |
| Motion | 0 | 5 | `0x2` | +96, present |
| Output | 0 | 34 | `0x600002` | +96, present |
| Depth | 5 | 16 | `0x600000` | **+240, null** |

The separate `mpRTDepth` is kind 6 / format 17 / flags `0x500000`. These are
engine format numbers, not identified DXGI formats. All GPU-format fields were
`0xFFFFFFFF`. Nonzero native pointers do not independently validate a COM
object, resource description, state, image content or frame alignment.

The immediate failure is now directly established: the diagnostic route
passes a kind-5 texture into a preparation path that unconditionally reads
its null secondary backing. The earlier allocator analysis explains that
kind 6 moves its backing from +96 to +240. Meanwhile, the downstream Evaluate
setup expects the depth owner's +96 backing. Swapping in `mpRTDepth` would
therefore leave that downstream access inconsistent. No field substitution,
host null bypass or rendering correction was applied.

## Recorder contract

The executable is `mhw_depth_exception_capture`. It accepts only the exact
`MonsterHunterWorld.exe` name and SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
Mapped headers must identify x64, PE timestamp `0x66D13CB6`, SizeOfImage
`0x55D3000`. No machine-local game path is compiled into it.

Nine short byte sequences cover the fault, subtype selector, depth preparation,
depth getter, texture metadata getter, backing move, main-view/frame access and
the `mpRTDepth` metadata offset. They were derived after verifying captured-host
SHA-256 `D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
Both admission and the matching exception require all nine live executable
image checks. This is an exact-build diagnostic, not a portable signature.

Only the matching AV reads thread context and payload. The fault/caller
conditions above must pass, and the descriptor must match opcode/subtype and
point to RBX before resource decoding. Unknown owner vtables are not decoded.
The wrapper and four owner fields, texture metadata, two backing fields and
their native/tracking fields are bounded scalar reads. Six payload DWORDs at
+40..60 are retained as uninterpreted bits. The main-view +144/+160 owners are
compared directly with the payload depth. No COM, NGX or other remote function
is invoked, and no target memory or thread context is changed.

The snapshot allows 192 scalar reads. The underlying reader allows 256 reads
and 4096 bytes per operation, including code checks; individual reads are at
most eight bytes for data and 64 for code. Arithmetic overflow, guard/no-access
pages, uncommitted memory and cross-region reads reject before ReadProcessMemory.
A failed scalar read latches off subsequent reads. JSON null means unreadable;
`"0x0"` means a readable zero. CPU threads pause for the debug event, but this
does not prove GPU completion or frame alignment.

## Exception handling and lifetime

The tool uses documented
[DebugActiveProcess](https://learn.microsoft.com/en-us/windows/win32/api/debugapi/nf-debugapi-debugactiveprocess)
events. Attachment temporarily suspends threads and generates one system
breakpoint; only that initial breakpoint is consumed before `armed` is emitted.
The tool installs no breakpoints, elevates no privileges and changes no
protection checks. Debugger attachment can affect timing and debugger-sensitive
behavior, which remains separate from acceptance of the exact target exception.

Immediately after attachment,
[DebugSetProcessKillOnExit(FALSE)](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-debugsetprocesskillonexit)
prevents debugger exit from killing the target. Failure to set it triggers
detachment. Every subsequent exception, including the target AV, retains
[DBG_EXCEPTION_NOT_HANDLED](https://learn.microsoft.com/en-us/windows/win32/api/debugapi/nf-debugapi-continuedebugevent)
so the game's handlers run normally. Other exceptions are counted without
recording their payloads or debug strings. The recorder does not prevent or
repair the crash.

It captures one matching fault and detaches after continuing that event. A
1..180-second timer, 50,000-event limit and cancellation handler also bound the
session. Waits are 100 ms. Scope guards continue a pending event and detach on
C++ errors. Output is a new local JSONL file (CREATE_NEW), flushed line by line;
existing files cannot be overwritten. No game/MO2 installation is required.

Run 14388 recorded 319 debug events, including two exceptions: the initial
attach breakpoint and the target AV. The log records successful detachment
while the process was still alive; the user then reported the expected crash.
The process was independently confirmed closed before settings recovery.

## Build, tests and version detail

Release builds with `/W4 /WX` and CFG. All **15 CTests pass**. The new self-test
checks the exact exception filter, rejection before owner reads, known/unknown
classes, unreadable/null distinction, guard-page preservation and complete
main-view comparison with unique JSON field names. Two owned child processes
test native context reads, capture during an event, unrelated exception
forwarding, application handler execution, capture detachment and timeout
detachment without killing the child. Fabricated fixtures never relax MHW
identity checks.

Run 14388 used **v0.1.0**, SHA-256
`15BF4C9C258CCC9875A2F974AAB9E67C1C666E4D1567A4670C1C6C17DDD2F3F4`.
Its JSON had duplicate `.stable` keys for the two container members: an owner
check and slot check shared each name. Both occurrences of both keys were
`0x1`; no conflicting value or resource field was lost. The original capture
is preserved. **v0.1.1** names these `.owner_stable` and `.slot_stable`, adds
exception tick telemetry and tests complete-snapshot key uniqueness. The
v0.1.1 binary SHA-256 is
`60AB82B1F88E207AF2143377DDD39DB748DC59B0BA5C3F37BB7C889052CC9F5B`.
Its local tests pass; it has not required another MHW run.

```powershell
cmake --build build --config Release --target mhw_depth_exception_capture
ctest --test-dir build -C Release --output-on-failure
```

## Recovery and next work

The exact post-crash graphics settings were preserved, then only saved DLSS
was reset from On to Off. TAA, DX12, full screen and 2560x1440 remain selected.
The active v0.7.3 bridge hash is unchanged. **No repeat crash is needed to prove
this mismatch.** Next work is a validated resource-preparation path before the
existing host crash, with resource formats, states and lifetime established
before any new provider or GPU work. Evaluate remains uncaptured.

Ignored evidence directory: `local/depth-exception-20260922/`, including the
original recorder binary/source snapshot, output, bridge log, recovery backup
and CTest logs. Raw pointers and any proprietary dump content remain local.

| Evidence | SHA-256 |
| --- | --- |
| `capture-14388.jsonl` | `65FF056F10D2C49C03DFD7EB387BA9150B8DD9365886D1D2842C05243A13369A` |
| `bridge-14388.crash.log` | `33BA5D30A8967473E554C0D00FF9BCE09ACB0526B37F45F5B0A8A54E73CF1608` |
