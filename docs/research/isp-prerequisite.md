# MHW's ISP prerequisite and separate feature lifecycle

Date: 2026-09-07. Static mapping and accepted v0.5.3 scalar evidence. The
[v0.6.0 diagnostic lifecycle replacement](two-provider-create-capture.md) is
tested: ISP now publishes success, exposing the
[optimal-settings callback crash](optimal-settings-callback-crash.md) before
Create. Accepted v0.6.1 run 34496 verified callback containment, its four input
scalars and both bridge shutdown paths. Later v0.6.2
[run 19800](first-create-capture.md) reached DLSS Create with 1:1 sizing, captured
its scalar contract and completed both shutdown paths after deliberate failure.
ISP Create remains unobserved. Modern DLSS rendering is not implemented.

## Runtime conclusion

The [accepted v0.5.3 run](capability-publication.md#accepted-v053-isp-evidence)
returns DLSS availability `1`, ISP availability `0`, and ISP Init status
`0xBAD00002` (`FAIL_PlatformError`) through both core layers to MHW. All 28
attempted scalar queries complete. These are present values, not missing-key
defaults. That v0.5.3 bridge substitutes only the DLSS provider lifecycle;
ISP remains with its original provider.

The captured game's D3D12 setup requires both availability values. ISP failure
therefore explains a concrete unsatisfied setup prerequisite. It is not proof
of the complete menu-visibility path or of the exact instruction taken live.

## Identities and local evidence

- Current game: Steam build 15539686, version resource `0.0.0.0`, SHA-256
  `C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
- Captured game used for IDA: SHA-256
  `D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`,
  input MD5 `A137D3636F22A730F71522C193B0472E`. The return to MHW at RVA
  `0x23842FD` matches the runtime trace. Other captured instruction bytes have
  not been revalidated live and are not new hook signatures.
- Cached ISP: version 1.2.103.0, SHA-256
  `3C47CD2504517120CF48BC943BCCF158190C540C646BAA3F95E547AD9B557EAD`,
  input MD5 `FD47F379C5A28E34006A4B8B5BECCF65`, PE timestamp `0x601450EC`,
  image size `0x2A9000`. The hash was checked again before static analysis.
  Its observed location is
  `%ProgramData%/NVIDIA/NGX/models/dlisp/versions/66154/files/160_B9D2F5C.bin`.
- Current core: version 32.0.16.1047 and SHA-256
  `B3C82638C60F29AEFF68052FFE7147246A73C80CBEA5883B35A9B0D1ED57DDE7`.

IDA used a repository working copy of the existing captured-game database
and a new database for the unchanged cached ISP input. Bounded outputs and
configurations are under `local/capability-trace-20260907/`, in `host-isp-use`,
`host-isp-create` and `isp-provider`. Proprietary inputs/databases and exported
assembly remain untracked; these notes describe observations only.

## Captured game ownership and use

The setup function at RVA `0x23842A0` returns null unless both signed
availability reads succeed and are nonzero. Only then does it construct a
72-byte wrapper at `+0x2383760`. The wrapper holds the parameter object at
offset `0`, a critical section at `+0x10`, and separate feature handles at
`+0x38` and `+0x40`. The constructor clears the handles.

The larger renderer setup at `+0x23A48E0` calls this constructor path at
`+0x23A72E8` and stores the returned pointer. A null result prevents the
subsequent wrapper-ready query at `+0x2384400`. That query checks whether
either retained feature handle exists; wrapper construction alone is not
evidence of a usable feature.

The captured creation routine at `+0x2384420`:

1. Computes scale and two feature-selection flags through `+0x2383EF0`.
2. Writes dimensions and `ImageSignalProcessing.ScaleFactor`.
3. Conditionally requests DLSS feature index `1` (D3D12 call at `+0x238460D`).
   A failure result or null handle exits this path before later creation.
4. Separately, conditionally requests ISP feature index `9` (D3D12 call at
   `+0x23847B0`). It checks both result and handle.
5. Retains the resulting feature handles independently in the wrapper.

The two flags are separate bits in the scale helper's returned metadata; their
full selection logic and reachable combinations have not been mapped. The
branch structure permits ISP creation when the DLSS branch is skipped. It is
not safe to assume failing the existing DLSS Create wrapper will intercept
every possible ISP request.

The captured cleanup routine at `+0x23837A0` releases each nonnull handle
through the selected graphics API, deletes the critical section, frees the
wrapper, then calls the matching NGX Shutdown. This static path does not prove
it ran in the completed tests.

## Cached ISP provider surface

| Export | ISP RVA |
| --- | ---: |
| `NVSDK_NGX_D3D12_Init` | `0x83B0` |
| `NVSDK_NGX_D3D12_Shutdown` | `0x84F0` |
| `NVSDK_NGX_D3D12_GetScratchBufferSize` | `0x8550` |
| `NVSDK_NGX_D3D12_CreateFeature` | `0x8580` |
| `NVSDK_NGX_D3D12_ReleaseFeature` | `0x85A0` |
| `NVSDK_NGX_D3D12_EvaluateFeature` | `0x85C0` |
| `NVSDK_NGX_PopulateParameters` | `0x8B40` |

These are static export observations. The v0.6.0 work has now checked the
optional exports, lookup ordering, signatures and cleanup ABI, as recorded in
the linked contract. The accepted v0.5.3 log still does not contain a complete
second provider transaction; v0.6.0 must prove it at runtime before Init succeeds.

ISP Init checks the module containing its immediate return address and
requires the calling module path to contain `nvngx.dll`, then constructs
provider state and calls internal initialization at `+0x9210`. This identifies
another reason an original Init cannot be transparently observed by an ordinary
wrapper in the probe module. No such ISP Init wrapper is installed by v0.5.3;
its `0xBAD00002` must not automatically be attributed to that check. Internal
initialization can also fail, and the exact failing operation is unobserved.

Create enters provider implementation through `+0x93F0` when its state exists.
The advertised availability flag is therefore not a substitute for implementing
or safely containing that stateful feature lifecycle.

## Next implementation boundary

Keep v0.5.3 as the known diagnostic baseline. The next capture milestone must
cover both DLSS and ISP with distinct provider identities, routing transactions,
state, call budgets and ownership. It must reject partial provider replacement
and fail Create without a feature handle or GPU work for either feature.
Outstanding cleanup must remain serviceable for each admitted provider.

An ISP availability setter alone would expose a feature still backed by the
failed original provider, including possible ISP-only creation paths. Do not
use it as the next experiment. A new provider route requires its own tested
contract; no game-executable or signed-provider patch is proposed here.
