# Prepared draw dependency capture

2026-10-01, v1.25.28. Diagnostic addition after the v1.25.27 wingdrake
attachment rejection; see `multi-overlay-draws-result.md`. Rendering admission,
SDK calls, resource barriers, native draw selection and hook targets are
unchanged. This build is not a compatibility fix.

`OutputDependencies` observes a rejected overlay at the existing completed
post-filter queue callback in `stream_route::build_plan`. Geometry/input
rejections select a descriptor key structurally; there is no effect-name,
monster or wingdrake whitelist. The current executable identities remain:

- Host: `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- Local decrypted PE: `d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.

No new hook or calling convention is introduced. Existing x64 callback and
queue-completion contracts from `multi-overlay-draws.md` and
`prepared-command-rejections.md` apply. The route mutex serializes observation
before recording workers. The recorder keeps bounded CPU copies, no COM
references or pointers to dereference later. It neither calls the GPU nor
changes process memory outside its own diagnostic storage.

For every selected draw, it records its actual attachment, resource and
format, rectangle, vertex buffer data/layout, VS/PS bytes, packed shader
binding words, referenced owner identities and texture resources. Two complete
CPU snapshots must match and the frame stamp must remain current. This is
CPU preparation evidence, not GPU resource-description/state verification.

Bounds: two accepted samples, four draws per sample, sixteen attempts,
at least sixty frames between attempts, sixty-four owners, 128 binding words
per shader stage, 16 KiB per shader, thirty-two input elements and 1,020
vertex bytes. Failed/oversized captures retain an error count and never
relax a rendering guard. The recorder uses fixed global storage rather than
large stack allocations. Shader bytes stay in private local logs; they are
not committed or distributed.

`continuous_dependencies.py` checks bounds, immutable sample history, owner
and binding coverage, queue order, vertex extents, DXBC structure/stage and
explicit CPU-only claims. v1.25.28 full-run review requires this telemetry.
`extract_output_dependencies.py STATUS --output DIRECTORY` writes validated
shader files and identifies prior-writer/sample-resource matches within each
sample. Such matches are candidate dependency edges, not permission to scale
or omit a draw.

Offline verification: warning-free Release build, 57 focused native CTest
cases, 54 Python tests, historical v1.25.27 and synthetic v1.25.28 schema
checks, and 96 WARP fixture frames exercising the existing multi-draw route.
Native fixtures cover the temporary-target dependency, changing texture or
frame, queue/draw/owner/shader bounds, unreadable ranges and exhausted budgets.
These results do not establish live capture success.

Next runtime: same profile/settings, verified main-menu arming, clean save
baseline, one wingdrake trip, and recovery. Inspect accepted dependency
samples before requesting another reproduction. Expected interruption can
remain. Keep the installed DLL after normal exit and archive the evidence.
