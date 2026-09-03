# MHW DLSS Update

Open-source research into modern temporal anti-aliasing and upscaling for the
Windows PC version of Monster Hunter: World.

The repository is currently in Phase 0: observing and minimally restoring the
game's existing NGX/DLSS 1 integration before changing rendering behavior.
`mhw_dlss_probe` is a
bounded diagnostic DLL that observes the game's signed `nvngx_dlss.dll`
create/evaluate/release boundary and the system DXGI swap-chain creation
boundary. It verifies that NGX receives a real D3D12 command list and correlates
its adapter with the queue used to create the swap chain. It does not patch the
protected game executable.

The default smoke-test mode logs only lifecycle calls and the verified API.
Parameter/resource queries are a separate opt-in stage via
`MHW_DLSS_PROBE_QUERY_PARAMETERS=1`; the probe permanently stops querying after
the first protected getter or resource-description exception.

The probe is locked to the recorded executable and feature-DLL SHA-256 values
and to the feature's API, snippet, application IDs, and untouched in-memory hook
prologues. DXGI targets must be executable image addresses owned by the system
`dxgi.dll`. Captures are written by a background thread after the original call
returns. The default limit is 16 evaluations.

The probe has a CMake install target for staging as a distinct, disabled MO2
mod named `dlssTEST`. It must not be copied into the physical game directory.
Staging, controlled offline-run gates, and removal are documented in
[`docs/runtime/mo2-strackers-loader.md`](docs/runtime/mo2-strackers-loader.md).

The current input contract, confirmed MHW evidence, and unresolved runtime
questions are tracked in
[`docs/research/phase-0-requirements-gap.md`](docs/research/phase-0-requirements-gap.md).

`mhw_dlss_unlock` is currently an exact-build, observation-only tracer for the
legacy NGX startup boundary. It is loaded before game code through MO2 Force
Load and wraps only six verified `GetProcAddressForCaller` results spanning the
game, NVIDIA's legacy/modern driver cores, and the two installed DLSS feature
images. Every wrapper calls the original once and returns its result unchanged.
The preceding v0.3.3 cache-classifier bridge was retired after a clean runtime
test recorded zero calls at both patched decision points. See
[`docs/research/legacy-rtx50-classifier.md`](docs/research/legacy-rtx50-classifier.md)
and the isolated
[`docs/runtime/rtx50-unlock-test.md`](docs/runtime/rtx50-unlock-test.md)
acceptance procedure.

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
  hash-gated observational NGX/DXGI probe with runtime identity correlation,
  exact-build NGX resolver/export tracer, and isolated MO2 packaging
- Not implemented: modern DLSS evaluation or frame generation
- Game files are never committed or patched
