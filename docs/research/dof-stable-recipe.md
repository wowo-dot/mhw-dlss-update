# Gallery stable-recipe recorder correction — v1.25.39

The [v1.25.38 capture](dof-entry-capture-result.md) localized eleven rejected
dispatch observations to the eight-byte state-list head at packet `+0x8`.
The native scheduler writes that field after our prepared-queue capture. It is
not a stable part of the compute recipe across preparation and execution.

## Comparison boundary

`OutputDependencies::compute_recording_difference` compares every field covered
by the existing complete comparison, except prefix words 2/3. The raw comparison
and prefix remain recorded independently. No other prefix word is masked,
including the opcode, mode-adjacent bytes, shader-state pointer and dispatch
dimensions. Shader bytes/identity, constants, owners, textures, views and binding
recipes retain their existing checks.

Only the diagnostic native-entry/API-time observations use this comparison.
The prepared queue's double snapshot and resource-description stability checks
still require exact equality, including the state-list field. No native memory
is changed. No state is inferred from this pointer, and it is not dereferenced
by the correction. GPU-state observations remain scoped to actual command
lists and Reset generations. Missing transitions remain unknown.

The hook, Windows x64 calling convention, ownership/lifetime and executable
hash gates from [the resource recorder](dof-resource-capture.md) are unchanged.
The existing native planner/converter analysis is linked from the v1.25.38
result. There are no additional hooks, GPU commands or resource references.
Gallery rendering admission is unchanged and DLSS may still suspend there.

## Versioned evidence

`compute_resources` now uses schema 3, policy `native-compute-stable-recipe-v3`.
Every pass retains the schema-2 fields and adds `stable_difference`, the
difference mask after excluding only the scheduling pointer. `recipe` still
contains complete-reread success and the **raw** eight-group difference mask.
`observed_prefix` is the raw prefix from the most recent completed diagnostic
reread at entry or Dispatch. An unreadable reread has zero comparison masks.

Thus raw mask 2 with stable mask 0 can be a successful observation when only
words 2/3 differ. Entry reason 11 and API error 3 require an actual stable
recipe mismatch (or, for API error 3, a failed reread). The Python reviewer
independently recomputes the stable prefix mask and preserves every other raw
group bit. Historical schemas 1/2 retain their original interpretation;
v1.25.39 full-run review requires schema 3.

## Verification and runtime plan

Native fault-injection tests change each prefix word, both pointer halves,
and fields in every recipe group. A pointer change cannot conceal a simultaneous
stable-field change. Changing the pointer during the same-stage double
snapshot still prevents capture. Python tests cover raw/stable-mask agreement,
unknown state preservation, rejection reasons, schema requirements and
completed-evidence immutability. New-version full-review exercises are explicitly
synthetic and do not establish twelve successful live dispatch observations.

The candidate's `validation.json` records completed builds and tests. The
native-format shader replay from v1.25.38 is unchanged; its previous synthetic
pixel results are not repeated or presented as new live proof.

Next: the same solo/offline profile, native AA/DLSS Off, DX12 Windowed
2560x1440. Main menu -> save -> affected Gallery cutscene -> recovery ->
announced stop and independent cleanup. Inspect all twelve native observations,
their actual resource states, raw/stable differences and barrier coverage.
An unknown or rejected observation must remain explicit.

Evidence root: `local/dof-stable-recipe-20261001`. Preserve the installed v38
DLL and sidecar before deploying; keep the new DLL installed after normal exit.
With the game closed, explicit rollback is `deploy.ps1 -Action Restore`.
That restores the previous sidecar, which points at a sealed archive; select a
fresh output destination before relaunching. Camp-entry skips remain deferred.
