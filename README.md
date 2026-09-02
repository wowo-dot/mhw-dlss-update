# MHW DLSS Update

Open-source research into modern temporal anti-aliasing and upscaling for the
Windows PC version of Monster Hunter: World.

The repository is currently in Phase 0: observing the game's existing NGX/DLSS
1 integration without changing rendering behavior. `mhw_dlss_probe` is a
bounded diagnostic DLL that waits for the game's signed `nvngx_dlss.dll`, hooks
only that module's create/evaluate/release boundary, and verifies that the
command object really is D3D12. It does not patch the protected game executable.

The default smoke-test mode logs only lifecycle calls and the verified API.
Parameter/resource queries are a separate opt-in stage via
`MHW_DLSS_PROBE_QUERY_PARAMETERS=1`; the probe permanently stops querying after
the first protected getter or resource-description exception.

The probe is locked to the recorded executable and feature-DLL SHA-256 values
and to the feature's API, snippet, application IDs, and untouched in-memory hook
prologues. Captures are written by a background thread after the original
NVIDIA call returns. The default limit is 16 evaluations.

The probe is not yet packaged for installation and should not be copied into the
game directory. Runtime loading and removal will be documented only after a
Steam-verified baseline and the staged offline smoke-test plan are ready.

The current input contract, confirmed MHW evidence, and unresolved runtime
questions are tracked in
[`docs/research/phase-0-requirements-gap.md`](docs/research/phase-0-requirements-gap.md).

## Build

Configure a 64-bit MSVC build:

```powershell
cmake -S . -B build -A x64
cmake --build build --config RelWithDebInfo
ctest --test-dir build -C RelWithDebInfo --output-on-failure
```

MinHook 1.3.4 is fetched at configure time from its pinned upstream commit; see
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

## Repository status

- Supported research target: DirectX 12
- Implemented: dependency-free binary string scanner, requirements/gap report,
  and a hash-gated observational NGX feature-boundary probe
- Not implemented: loader, deployment, modern DLSS evaluation, or frame generation
- Game files are never committed or patched
