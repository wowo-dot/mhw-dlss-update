# Auxiliary texture replacement at native draw entry

## Scope and evidence

v1.25.32 addresses the seven-field mask replacement captured by v1.25.31:
[dependency-binding-boundary-result.md](dependency-binding-boundary-result.md).
The tested executable SHA-256 remains
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
There are no new hook addresses or calling conventions. The existing native
draw-entry hook (RVA 0x259d670, RCX executor, RDX backend context, R8 packet)
captures the bindings before the original native binder runs. The existing
DrawInstanced boundary rechecks them before recording either private branch.

Only the captured radial recipe's auxiliary PS t1/t2 storage may refresh.
The owner object, type, ID, kind, flags and format metadata must remain exact.
The allowed differences are width/height, primary/secondary/effective backend,
resource, SRV view and CPU descriptor. Binding layout, constants, samplers,
t0 scene-derived input and outputs stay exact. Scene-copy admission is unchanged.
No area, monster, effect event, texture name or particular dimensions enable it.

## Admission and lifetime

At entry, read the complete known binding contract. Acquire COM references
to changed auxiliary resources, validate their device, texture description,
engine/GPU formats and sampling support, and reject aliases with scene inputs,
temporaries or outputs. Re-read the complete snapshot after acquisition.
Capacity exhaustion, unreadable data, unsupported formats, identity changes,
or an unstable second read reject without publishing a partial snapshot.

Append accepted resources to the bounded plan rather than overwriting old
references. Redirect only the changed auxiliary read indices. Import any
existing command-list state observations for new indices; subsequent native
barriers are observed normally. The native SRV and its barriers stay untouched.
Unknown local read state may inherit native state as before; contradictory
observed state still rejects. No guessed transitions are injected.

At DrawInstanced require full equality with the accepted entry snapshot. This
continues to reject changes during native binding, when the table might no
longer describe the GPU descriptor that was copied. Native descriptor heaps,
roots, PSO, geometry and every plan resource join the existing recording pins
before the first private draw. Old and replacement resources stay retained
through GPU completion AND command-list Reset, even if the plan is recycled.

The acquisition relies on the native draw owning its current input while the
entry callback executes, the same lifetime boundary used for other draw inputs.
Repeated CPU snapshot reads are not an engine lock or independent descriptor
readback. Changes after entry remain rejected; this does not claim arbitrary
concurrent texture mutation is supported. The fixed plan/recording capacity
remains fail-closed.

## Telemetry

The existing bounded comparison trace remains diagnostic. Its entry comparison
is against preparation; the API comparison is against the accepted entry
snapshot, or preparation if entry admission failed. An entry difference can
now coexist with a successful refresh and API equality. Renderer health and
`dependency_refresh` admission counters determine success, not the diagnostic
entry-rejection counter by itself.

`dependency_refresh` schema 1, policy `auxiliary-entry-retention-v1`, records
entries = unchanged + refreshed + rejected, API checks/rejections, and the
latest refresh's frame/stage/register/old-new dimensions. Rejection reasons:
1 unreadable/invalid contract; 2 immutable field changed; 3 texture validation;
4 scene/write alias; 5 capacity; 6 snapshot changed during acquisition. The
last rejection reason is sticky. No per-frame raw-memory or texture logging.
The v1.25.32 reviewer requires both traces and preserves failed-run rejection.

## Offline validation and runtime follow-up

Evidence root: `local/auxiliary-binding-refresh-20261001`.
The production fixture recreates all seven live differences for a 128x128 to
256x256 mask. It verifies acceptance and separate old/new pins, strict scene
provenance, nine unsafe cases, later descriptor mutation rejection and imported
contradictory state. It recycles the plan during real GPU work and verifies
81,920 exact old/new texture pixels; resource references remain until both
fence completion and Reset, then return to baseline.

The actual refreshed radial recording matches a standalone effect using the
current bindings on all 8,192 output pixels. This comparison tests integration;
shader-coordinate correctness has the existing independent dependency oracle.
The copy path still passes its expanded/native output and binding-restoration
checks. WARP and RTX 5090 runs pass with GPU validation and no warnings/errors.
The existing 96-frame / 288-effect continuous regression passes, alongside
59 native and 62 Python tests. Initial fixture builds caught shadowed names;
GPU validation caught a mismatched optimized clear value. Both fixture issues
were corrected, and the failed receipts are retained.

The targeted live test passed on 2026-10-01: the reproduced 128x128 to 256x256
replacement was admitted, with 347 copy/radial pairs and zero runtime faults.
See [the runtime result](auxiliary-binding-refresh-result.md) for cleanup,
the separately deferred camp-entry observation and a frozen diagnostic
reviewer schema limitation. The installed DLL is kept after normal exit.
Exact previous DLL and sidecar backups accompany the installation; original
game assets and settings are not changed.
