# Modern provider bridge: first offline test

This procedure is for the first `capture-create` run only. The bridge changes
NGX lifecycle results in that explicit mode, so it is more invasive than the
v0.4.1 observer. It does not load modern DLSS, evaluate an upscaler, or produce
a valid feature handle.

## Build and disabled staging

Build all policy/snapshot tests and the bridge probe before putting it in MO2:

```powershell
cmake -S . -B build -A x64
cmake --build build --config RelWithDebInfo --target ALL_BUILD --clean-first
ctest --test-dir build -C RelWithDebInfo --output-on-failure
```

The CMake component `bridge-probe` installs
`mhw_dlss_bridge_probe.dll` at the MO2 mod root, not under
`nativePC/plugins`. It must be available before the first NGX export lookup;
Stracker's plugin scan is too late.

Stage the first build with a disabled suffix while MHW is closed:

```powershell
$bridgeSource = 'D:\mh world modding\dlssupdate\build\bin\RelWithDebInfo\mhw_dlss_bridge_probe.dll'
$bridgeDisabled = 'D:\MO2\MHW\mods\dlssTEST\mhw_dlss_bridge_probe.dll.disabled'
if (Test-Path -LiteralPath $bridgeDisabled) {
    throw "Preserve the existing staged bridge before continuing: $bridgeDisabled"
}
Copy-Item -LiteralPath $bridgeSource -Destination $bridgeDisabled
Get-FileHash -Algorithm SHA256 -LiteralPath $bridgeDisabled
```

Stop if the destination already exists; preserve and identify it instead of
overwriting it. Staging is complete while `dlssTEST` and every Force Load rule
for the bridge remain disabled. Do not copy the DLL into the physical game
directory or over `nvngx.dll`, `_nvngx.dll`, `nvngx_dlss.dll`, a loader, or the
game executable.

For the controlled run, preserve any existing active bridge file, rename this
one to `mhw_dlss_bridge_probe.dll`, and configure that exact root DLL as the
only enabled MO2 Force Load library for the dedicated MHW executable entry.
`mhw_dlss_unlock.dll` may remain archived in the mod, but its Force Load rule
must be disabled. Do not load the old observer and bridge together.
The bridge rechecks for a mapped `mhw_dlss_unlock.dll` when the provider
transaction begins, but this is a final rejection gate rather than permission
to configure both DLLs.

## Preconditions

- Use offline Steam and the title/options flow or a private solo save. Never
  take this experiment into public multiplayer.
- Confirm MHW is fully stopped before changing the forced-library rule.
- Require the exact executable, cores, cache path, hashes, PE metadata, and
  versions listed in
  [`../research/modern-provider-bridge.md`](../research/modern-provider-bridge.md).
- Use DirectX 12, fullscreen, and FidelityFX CAS + Upscaling off. These retain
  the known menu-eligibility state; they are not proof that DLSS is available.
- Disable every unrelated forced library, including stale LuaEngine entries.
  Record any unavoidable loader or installation confounder before the run.
- Keep `mhw_dlss_probe` under `nativePC/plugins` disabled for this test.

Close MO2 completely, then start it from a PowerShell process that supplies the
one exact opt-in value. This keeps the variable scoped to that process tree:

```powershell
$env:MHW_DLSS_BRIDGE_MODE = 'capture-create'
& '<path-to-ModOrganizer.exe>'
```

Do not use `setx`, and do not begin with `capture-one-evaluate`. If MO2 was
already running, it will not inherit the new process environment.

## Test sequence

1. Launch MHW through the dedicated MO2 executable entry.
2. Stay at the title/options flow and open Display settings. Record whether a
   DLSS row appears; its absence is a valid diagnostic result.
3. If a DLSS choice appears, select it only once if needed to provoke Create.
   Do not repeatedly toggle settings or continue into multiplayer.
4. Allow the bridge to capture Create and complete its exact Shutdown cleanup,
   or let the five-minute bounded window finish, then exit MHW normally. The
   log writer does not close while synthetic Init cleanup remains outstanding.
5. Inspect `%LOCALAPPDATA%\MhwDlssBridgeProbe\bridge-<pid>.log` before another
   run.

The run is acceptable for analysis only when the log proves all of the
following:

- mode was recognized exactly as `capture-create`;
- host, system resolver, current core, cached feature, and the four exact host
  getter RVAs all passed preflight; the pinned legacy core identity should also
  appear in the completed runtime telemetry when that observer slot resolves;
- the exact Init-first 14-lookup provider transaction completed in the pinned
  order, with `lookup-seen-mask=0x3FFF`, `route-mask=0x3F`,
  `guard-seen-mask=0x2FC0`, `callback-seen-mask=0x1000`,
  `route-table-complete=true`, and
  `startup-coverage=complete-init-first-14-lookup-transaction`;
- the six base lifecycle exports resolved to their pinned cached RVAs and were
  routed only to their matching wrappers; the seven guard-only ABI variants
  all resolved null, so a clean first transaction reports `guard-lookups=7`,
  `guard-rejects=0`, and `guard-suppressions=0`;
- the exact generic `NVSDK_NGX_PopulateParameters` target was cached
  feature `+0x1DFA0`, its resolver caller was `_nvngx.dll+0x43351`, it routed
  to `cache_populate_wrapper`, and `callback-routes=1`; a clean first
  transaction reports `routes=7` and `route-rejects=0` (later verified
  duplicate lookups may increase successful lookup counters but must not
  change the masks or introduce a rejection);
- Init came from `_nvngx.dll+0x6769A`; wrapped Populate invocation came only
  from `+0x67ACE` or `+0x67BC2`; and Create, if reached, came from `+0x67DE6`;
- the one Create snapshot either completed within its fixed query budget or
  stopped with an explicit getter-fault reason;
- Create returned without a feature handle and no Evaluate capture occurred;
- there were no dropped bridge events, recursion conflicts, unquiesced calls,
  boundary contentions, lifecycle-ledger overflow, access violations, hangs,
  or crashes.

No Create call means the run is timing/coverage evidence only, not a contract
capture. `startup-coverage=inconclusive-init-lookup-missed` specifically means
the async worker lost the first lookup and refused to join the 14-member
transaction halfway; the provider was left untouched and the result may be
retried. Any identity mismatch, altered prologue, wrong caller, partial or
reordered transaction, non-null guard, guard suppression/rejection,
generic-Populate binding failure, route rejection, lifecycle-order rejection,
callback fault, or dropped event is otherwise failed/inconclusive. Preserve
the log and do not advance to `capture-one-evaluate` until the Create result
has been reviewed.

## Rollback

1. Exit MHW completely.
2. Disable the bridge's MO2 Force Load rule. An absolute Force Load entry can
   remain effective even when the source mod is unchecked.
3. Disable `dlssTEST` and rename the active bridge back to
   `mhw_dlss_bridge_probe.dll.disabled`, or remove only that staged bridge file
   after preserving its hash and log.
4. In the PowerShell process that launched MO2, clear the temporary value with
   `Remove-Item Env:MHW_DLSS_BRIDGE_MODE -ErrorAction SilentlyContinue`, or
   simply close that process tree.

Rollback takes effect on the next launch. The test must leave
`MonsterHunterWorld.exe`, the physical game directory, the NVIDIA NGX cache,
and every signed NVIDIA binary unchanged.
