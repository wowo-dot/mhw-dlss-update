# Attachment identity correction — v0.3.1

2026-09-24. Corrects the recorder assumption reproduced after
[partial run 5208](../research/projection-attachment-first-run.md). This is the
same read-only attachment experiment as [v0.3.0](projection-attachment-plan.md),
with canonical COM identity checks and bounded query diagnostics.
[Game run 11804](../research/projection-attachment-result.md) now validates this
version's three resource-query phases and complete draw/attachment capture.
That runtime cycle is complete: the previous DLL is restored and all four
configuration hashes are unchanged. The preparation protocol below is retained
for reproducibility; no repeat launch is requested.

## Identity and lifetime contract

The recorded view candidate must still query to the exact expected
`ID3D12Resource` address and return the expected owning `ID3D12Device`.
Its canonical `IUnknown` may have a different address. Querying that canonical
interface for itself must return the same canonical identity; querying it for
`ID3D12Resource` must return the original resource interface. Querying the
roundtrip resource for `IUnknown` must return the same canonical identity.
No pointer adjustment or driver-specific layout is used.

The implementation uses scoped owners for all temporary interfaces. It guards
method targets and calls, records HRESULTs, and releases every temporary on
success or failure. It transfers one validated resource reference to the
existing capture pin array. Pin release occurs after admitted hooks drain.
This preserves native objects through the checks without altering rendering.

The complete identity/device/description validation runs at three points:
after attachment setup, immediately before the actual draw API, and at native
draw return. Rechecks must equal the original resource description and
canonical identity. There are at most nine resources and 27 query records;
failed attempts are retained even when no description was accepted.
All COM calls remain outside the recorder's bookkeeping lock.

## Diagnostic record

`attachments.resource_checks` contains phases 0, 1 and 2, each with a bounded
`queries` array. A query retains candidate, resource interface, owning device,
canonical identity, repeated canonical identity, roundtrip resource and
roundtrip canonical identity as separate pointer values. `results` contains
six unsigned HRESULTs in this order: resource QI, GetDevice, canonical QI,
canonical self-QI, resource roundtrip QI, roundtrip canonical QI. Unattempted
operations are `E_PENDING` (`0x8000000A`). The last attempted check is:

| Checkpoint | Operation |
| ---: | --- |
| 1 | Resource QueryInterface result |
| 2 | Exact resource-interface address |
| 3 | Resource method-target validation |
| 4 | GetDevice result |
| 5 | Owning-device identity |
| 6 | Canonical IUnknown query |
| 7 | Canonical self-query |
| 8 | Canonical self identity equality |
| 9 | Resource-interface roundtrip query |
| 10 | Roundtrip resource address |
| 11 | Roundtrip canonical query |
| 12 | Roundtrip canonical equality |
| 13 | GetDesc and texture-description validation |
| 14 | Temporary-release or pin-capacity failure |
| 15 | Complete successful query |

`valid` and `temporaries_released` must both be true for an accepted query.
A later phase can complete its local query yet disagree with the original
identity or description; that still rejects the capture. The independent
reviewer requires all three phases and compares their identities against the
accepted description. Legacy v0.3.0 retains its original interpretation and
cannot be upgraded by changing how an old failed record is reviewed.

## Verification and runtime procedure

Release `/W4 /WX`: 76 native tests and 117 Python review/control tests pass.
The Python total is 34 binding, 25 stimulus and 58 attachment checks.
The SDK run on the installed adapter accepted the texture with different
resource/canonical addresses and passed both rechecks; it rejected the buffer
at checkpoint 13. This is a real API validation of the corrected recorder,
not a claim about the game's attachment. Historical accepted game reviews and
all seven v0.3.0 owned-fixture reviews must remain identical during preparation;
failed game runs 42212 and 5208 must remain rejected.

The owned native fixture now exercises a separate canonical interface,
wrong canonical identity, wrong resource roundtrip, failed/null/exceptional
queries, identity changes at draw/return, and cleanup after a later resource
fails. Each rejection checks its actual checkpoint and balanced references.

An additional SDK test runs the **production validator** on a real locally
created D3D12 depth texture and upload buffer. The texture must pass all three
query phases; the buffer must fail texture eligibility at checkpoint 13.
It opens no game process, installs no hooks and submits no commands. This
supplements the synthetic fixtures that missed the original COM assumption.

The native ABI, ten native/fourteen API hooks, 39 complete code gates, two-second
capture, zero-jitter source, and all shader/resource bounds are unchanged.
GPU contents, view-subresource provenance, actual GPU state and raster jitter
remain unverified. Hash/host requirements remain the pinned MHW version
`0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.

Local archive: `local/projection-binding-20260923/v0.3.1/`. Installation must
preserve the exact previous slot and sidecar and all four configuration hashes.
Use the same MO2 test entry and bare profile, DLSS Off/TAA On, load one save,
then arm only after its new ready record and the loaded-scene report.
After capture, exit normally and run `deploy.ps1 -Action Restore` from this
version's archive. Deployment/preflight/restoration receipts identify the
actual state; an earlier archive is never overwritten.
