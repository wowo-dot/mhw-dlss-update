# Attachment recorder v0.3.0: partial run 5208

2026-09-24. PID 5208, creation `134346711591100544`, tested DLL SHA-256
`B08EB9238DC392008B8F05B16D00E2D354E3A373DCDBA42EC38BFFA6A7F1BD47`.
Pinned host version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
Local evidence: `local/projection-binding-20260923/v0.3.0/`.

Follow-up: the correction is validated in [v0.3.1 run 11804](projection-attachment-result.md).
This earlier run retains its rejected verdict and sealed evidence.

## Result

The complete experiment is **rejected**. All 39 live code gates and 24 hook
targets passed. A zero-jitter source at frame/epoch 16801 reached publication,
consumption and the selected native indexed-draw function (`259D930`).
Independent matrix checks pass. The two shader containers match run 46324.

Before state setup, the recorder observed three returned calls on the selected
command list: `OMSetRenderTargets`, `RSSetViewports`, `RSSetScissorRects`.
The actual output bind had **zero color targets and one depth handle**.
Entry/post-helper attachment anchors, view links and native pipeline inputs
agree; the cached DSV equals the freshly bound handle. Both viewport and
scissor cover 2560x1440. Native depth state has `DepthEnable=1`,
`DepthWriteMask=1`, `DepthFunc=7` (`GREATER_EQUAL`). Native depth format 39
maps to DSV format 40. These are native recipe observations, not proof of the
actual cached PSO's creation descriptor or GPU depth writes.

Resource qualification failed with first fault `identity` before any resource
description was accepted. Consequently there is no recorded draw API, fresh
root-CBV bind, barrier capture or PSO creation for this run. The original native
draw returned and the source remained unchanged; later cleanup marked the
attachment record unstable because qualification was incomplete. Hooks
disabled and drained with zero pending calls and zero drops. The recorder's
`fatal:true` means rejected experiment, not a process crash; the process
reported responsive afterward. No jitter writes or added GPU commands.

The log SHA-256 is
`42b26f355341c762f995adfd97d86bed205ba86ccfe9f25024063acb4dc6259a`.
`review_partial_5208.py` validates these partial observations, verifies both
shader files and all 40 archived source hashes, and confirms that the original
full reviewer rejects the run. Do not relax that verdict retrospectively.

## Live inspection and owned SDK reproduction

A bounded asynchronous read after hook shutdown found the same view-owner,
backend-view, resource candidate and descriptor handle. The candidate and the
already validated arena resource use the same `D3D12Core.dll` vtable. Relevant
methods are executable System32 image addresses and remained stable across
the reads. This narrows the failure but does not replace COM validation at
the hook boundary. The inspection performs no remote calls or writes.

A separate `/W4 /WX` SDK program creates its own upload buffer and typeless
depth texture on the same adapter LUID. Both retain the expected
`ID3D12Resource` interface, owning device and query roundtrip. Their canonical
`IUnknown` pointers differ from their resource-interface addresses by -48
bytes while remaining stable across queries. The program opens no game
handle and creates no command list or submission.

This reproduces a concrete recorder bug: `resource()` requires the canonical
`IUnknown` pointer to equal the `ID3D12Resource` pointer. COM requires stable
canonical `IUnknown` identity across interfaces; different interfaces need
not have identical addresses. See Microsoft's
[QueryInterface identity rules](https://learn.microsoft.com/en-us/windows/win32/com/rules-for-implementing-queryinterface).
The owned mock returned the same address for both interfaces, so its 63 native
and 102 Python tests did not expose that assumption. The SDK reproduction
does. The v0.3.0 log lacks individual query checkpoints, so the exact failing
in-game subcheck remains unrecorded; the reproduced bug is the likely cause.

## Next correction

Use canonical `IUnknown` equality between queried interfaces and verify the
roundtrip to the required resource interface. Never hard-code the observed
-48 adjustment. Preserve exact resource-interface/device checks, bounded
descriptions, pinned lifetimes, and rechecks at draw/return. Add owned fixtures
with adjusted canonical identity, mismatched identity and wrong roundtrip,
plus per-step query results so future failures identify their precise stage.
Extend the independent reviewer under a new version; retain the original
v0.3.0 reviewer and source archive unchanged.

The user exited normally. `restored.json` and `archive-5208.json` verify the
exact previous v0.15 DLL, removal of the newly added output sidecar, all four
unchanged configuration hashes and all 40 archived source files. No new DLL
has been prepared by this run. The actual depth
resource description, view subresource, command submission and raster jitter
remain unverified.
