# Binding recorder allocation failure and startup reservation

Run 23832 on 2026-09-23 used v0.1.1 against executable SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`,
file version `0.0.0.0`, timestamp `66D13CB6`, image size `55D3000`.
The process creation identity was `134346611154358660`.

## Confirmed failure

Arming after the save loaded failed at the first native hook (producer RVA
`228F3B0`): install stage 2, MinHook status 9 (`MH_ERROR_MEMORY_ALLOC`),
Win32 error 0. No hook was installed, no callback visited the recorder, and
no source, publication, consumption, draw, or shader was captured. The v0.1.1
contention correction therefore remains untested in the game. The strict
review rejects this run. The reached install stage implies preceding discovery
returned successfully, but no `armed` record persisted its graphics details.

A read-only check of 32 bytes in our own pinned DLL found MinHook's private
heap initialized, its block list null, and hook array pointer/capacity/count
all zero. A separate audit link of the same objects produced a symbol map;
identical `.text` and `.data` section bytes and layouts were verified before
using its offsets against the deployed DLL. In the pinned MinHook source,
`AddHookEntry` sets capacity before attempting its allocation. These values
localize this failure to `AllocateBuffer`, before hook bookkeeping allocation.
No game memory was read by that check, and no process memory was written.

A later `VirtualQueryEx` survey found free candidate intervals near the native
code. This is not a snapshot at failure time and does not prove any candidate
would accept `VirtualAlloc`. Address-space exhaustion or timing is a hypothesis;
the precise allocation failure cause remains unknown. The process's dynamic
code policy query returned flags zero. No policy or protection was changed.

The user exited normally. The exact previous v0.15 DLL was restored, the test
output sidecar removed, and all four graphics/profile/mode hashes matched.
The ignored archive `local/projection-binding-20260923/v0.1.1/` retains the
log, diagnostics, replay script `reject_23832.py`, rejection receipt, exact DLL,
sources and restoration receipt.

## v0.1.2 mitigation

The worker verifies host identity, then asks the recorder's private MinHook
instance for one unused trampoline slot during startup. This pins a 4 KiB
block with 62 slots left for later hooks. Reservation reads no target code,
changes no target protection, and installs no hook. The block must be committed
private executable memory within MinHook's actual 1 GiB search radius of all
ten native targets. It is retained until process exit along with the DLL.
API targets in system modules still use MinHook's normal allocation path.

This uses the unmodified private buffer API of pinned MinHook v1.3.4, commit
`c3fcafdc10146beb5919319d0683e44e3c30d537`. The 4 KiB/64-byte geometry and
search range are coupled to that version. Calls are worker-only before any
hook creation. A failed policy query, prohibited dynamic code, failed allocation,
or invalid range stops the recorder without arming. Policy is never changed.
The reservation and readiness are logged and checked by the arm helper.

Explicit loaded-scene arming still performs all 29 live code gates and normal
resource/ownership checks before installing the same twenty hooks. The capture
is still bounded to two seconds and one native draw, with no injected jitter,
provider integration, or added GPU commands.

## Owned-process verification and remaining test

Three new native cases verify unchanged target bytes/unhooked forwarding after
reservation, a negative control where filled nearby address space makes fresh
MinHook allocation fail, and successful full-chain capture with a reserved
pool under equivalent pressure. The pressure fixture reserves only free
regions in its own test process and releases only its own reservations.
All 18 native tests and 23 Python review/control tests pass in Release `/W4 /WX`.

These tests establish the mitigation's behavior, not the game's original
allocation cause or a successful game capture. The next run must report a
valid startup reservation, pass fresh arm gates, then correlate the CPU source
through backend consumption and recorded draw binding. GPU execution, shader
use of projection, raster jitter, and working DLAA/DLSS remain unproven.
