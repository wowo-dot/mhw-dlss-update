# Driver 610.88 and Windows 26100.9278 revalidation

Date: 2026-09-22. Bridge **v0.7.3** refreshes the exact module identities and
the two Windows resolver export RVAs after local driver and Windows updates.
The v0.7.2 binary would reject this environment before enabling its hooks.
This is a compatibility update to the existing diagnostic. Subsequent runs
verified the updated identities; [TAA activation then exposed a pre-Evaluate
depth-resource crash](taa-activation-depth-crash.md). Modern DLSS rendering
and live Evaluate acceptance remain pending.

## Recorded inputs

All inputs were hashed before deriving offsets. The MHW executable remains
SHA-256 `C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`,
PE timestamp `0x66D13CB6`, SizeOfImage `0x55D3000`, file version `0.0.0.0`.
The cached DLSS 1.2.110 and ISP 1.2.103 hashes remain unchanged.

| Module | File version | PE timestamp | SizeOfImage | SHA-256 |
| --- | --- | --- | --- | --- |
| `_nvngx.dll` | 32.0.16.1088 | `0x6A6013FD` | `0x173000` | `FA30CD56A48F83378FA25AE181B771D63FA6856D0D5A4D65A9D38C9C45432A39` |
| `nvngx.dll` | 30.0.14.9516 | `0x60E65AC5` | `0x78000` | `54F22163631076EFC3CA289FFB2D46B1E7AD143A85836BB43DB6734912551661` |
| `KernelBase.dll` | 10.0.26100.9278 | `0x28606B21` | `0x402000` | `BECAD014FB8EFA8CB5E314931CCA92778AD42C649B12A6909632CACD68AF4F40` |
| `kernel32.dll` | 10.0.26100.9278 | `0x3ECCCF12` | `0xCB000` | `8F8629741558B5DF71DEF4E1DD8EF9645D4F806B949762C47757C50FEA1A9E6D` |

The exact modern-core hash occurs in one immediate DriverStore directory.
Discovery still scans bounded directories and rejects ambiguous matches;
the machine's current `nvmd.inf_...` directory is not compiled into the DLL.

## Confirmed binary comparison

Copies of the previous IDA databases supplied the old bytes. Their recorded
input MD5 values were checked and both had zero patched bytes. File-backed
bytes were compared at the same preferred image base. Undefined IDA tails
and BSS were excluded, and IDA's synthetic IAT contents were compared by
import module, symbol/ordinal and slot RVA instead of raw bytes.

- Legacy core: identical `.text`, remaining `.rdata`, initialized `.data`,
  `.pdata` and `_RDATA`; all 97 imports and previous export RVAs match.
  This supports retaining its existing resolver/parameter ABI. It does not
  establish that the whole-file hash difference is exclusively signing data.
- Modern core: all 176 imports and previous export RVAs match. Initialized
  `.data`, `.pdata` and `_RDATA` match. Six `.text` bytes differ in five
  instructions outside the functions used by the probe. Those differences
  change references into read-only data; other read-only data also changed.
  The entire new module is therefore not described as identical.
- Ten complete functions, including all their IDA chunks, match byte for
  byte: status recorder `0x264E0`, capability `0x669F0`, parameters `0x66C50`,
  provider resolution `0x43050`, Init `0x672D0`, population `0x677A0`,
  Create/handle copy `0x67C60`, Evaluate `0x66F90`, Release `0x8A30` and
  Shutdown `0x43550`. All seven existing modern-core prologue/handle/call
  byte guards match independently in the new file.

Consequently the 14-lookup transaction, lifecycle return addresses, status
recorder's x64 ABI, and copied-handle ownership/lifetime contract are retained.
Their evidence is the complete unchanged functions, not a version-number
assumption. See [one-Evaluate capture](one-evaluate-capture.md) and
[provider bridge](modern-provider-bridge.md) for those contracts. The game
and cached providers are unchanged, so their existing contracts are retained.

## Windows resolver boundary

`KernelBase!GetProcAddressForCaller` moved from RVA `0xB30B0` to `0xB49E0`.
`kernel32!GetProcAddress` moved from `0x32060` to `0x32130`. The existing
21-byte resolver prologue still matches. The latter remains the 11-byte
stub that places its return address in R8 and jumps through an import slot.

A separate Python process resolved its already loaded system DLLs and
verified that this slot points to the new KernelBase resolver. It read only
the named exports' bounded bytes and the single destination pointer. It did
not install hooks or load NGX. The resolver retains its native x64
`FARPROC(HMODULE, LPCSTR, void*)` ABI and process-lifetime hook ownership.
Both file hashes, mapped PE identities, export RVAs, executable ownership,
the prologue and the live stub destination remain mandatory runtime checks.

## Build and staging

Release builds with `/W4 /WX` pass; all 14 CTest tests pass. The bridge DLL is
374,272 bytes, SHA-256
`1F2309021E4DF15D3645C0B7C4A264F0B40D02394438DE1B3F6C65EEE1D29552`.
All 15 distinct exported wrappers/callbacks remain in its 104-target CFG
table. Shared observer source now reports **v0.4.2** because its identity
contract also changed; that standalone DLL was built but not deployed.

Local evidence, scripts, input metadata, code comparisons, CTest log, binary
audit and staging manifest are under ignored `local/driver-61088-20260922/`.
No proprietary binary or IDA database belongs in version control.

The exact active v0.7.2 DLL was preserved before replacing the separate MO2
mod DLL. MO2 PID 7112's `MHW_DLSS_BRIDGE_MODE=capture-one-evaluate` was verified
with bounded read-only access; no other environment values were logged.
The current `dlss` profile enables this DLL for the test entry. The graphics
configuration was backed up and only `ScreenMode=Windowed` changed to
`ScreenMode=FullScreen`. DLSS stays Off, DX12 On, resolution 2560x1440.

Game executable, assets and system/vendor DLLs were not modified. No new
hooks, resource reads, state transitions or GPU work were added. This static
and local verification did not establish rendering validity. Runs 20564 and
44972 subsequently verified startup, and run 44972 crashed in host resource
preparation before Evaluate. Saved DLSS was reset Off with TAA retained after
preserving the post-crash settings. Use the
[current diagnostic status](../runtime/modern-bridge-probe-test.md); the earlier
enable-and-load procedure is on hold pending depth-resource validation.
