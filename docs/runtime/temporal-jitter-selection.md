# Larger native jitter selection: v1.6.1

Accepted and closed 2026-09-25: [run 44552](../research/ordinary-taa-strong-jitter-result.md).
The selector skipped one weak preview and captured three accepted pairs with
one restored stimulus. Scoped color direction and static-motion jitter exclusion
reproduce; precise color displacement remains method/region dependent. Archive
and exact rollback are complete. The procedure below describes the tested build.

Prepared 2026-09-25. The prior
[v1.6.0 run 35156](../research/ordinary-taa-jitter-result.md) supported color
jitter direction and static-motion jitter exclusion, but its ~0.080-pixel
displacement exposed estimator bias. This revision selects a stronger native
table entry while preserving the three-frame baseline/stimulus/recovery design.
It does not evaluate an SDK or change persistent rendering settings.

## Selection and unchanged mutation scope

All targets, Windows x64 calling conventions, source/copy ownership, executable
gates and restoration rules remain those of the
[v1.6.0 recorder](temporal-jitter-pairs.md). Tested executable version `0.0.0.0`,
SHA-256 `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Projection helper RVA `228F3B0`, 2,000 bytes, caller return RVA `228E834`;
code SHA-256 `df1bf81584401fe529bc5fbbc3e19010e8959c2d13745eca6f87897183148373`.
RCX is camera +0x50, RDX is renderer +0x710, AL is the result. Camera, native
table and constants are borrowed; only owned copy references are retained.
No new native offsets or targets are introduced.

After the existing 150 ms submission warmup, each otherwise eligible projection
call can inspect one additional eight-byte entry from the already validated
20-sample table, at `((frame % 20) + 1) % 20`. This predicts the next frame's
native sample. Both finite components must have absolute value **0.3125–0.5**,
corresponding to at least **0.15625 raster pixels on each axis** under the
independently derived `(jx/2, -jy/2)` relation.

A valid but weaker sample leaves that projection and its TAA producer untouched,
without admitting a record or making a copy. At most twenty eligible previews
are allowed. The twentieth can succeed; twenty weak previews close the test.
Unreadable/invalid table entries or frame/epoch overflow close it immediately.
The original two-second window and 4,096-event journal budget are unchanged.
Repeated eligible calls can exhaust the preview budget; no unbounded wait is
introduced, and no full table is dumped.

On a strong preview, the current frame becomes the zero-jitter baseline and
the selected next frame/candidate bits are retained. The stimulus must still
arrive on the immediately following acknowledged projection/TAA frame. Before
the single camera-field compare/exchange, its actual native-table bits must
equal the preview and still meet the strength bound. A changed entry, even
another strong one, rejects the run before injection. No native values are
scaled or manufactured. Recovery remains zero jitter on the next frame.

All original scope checks and exception-safe restoration remain active. There
are still eight hooks, one injection/restoration, three projection samples,
three color and three motion copies, eighteen private command-list operations,
three private Signals and no observer Execute. Each copy is bounded to 16 MiB,
all six to 96 MiB; constant-block reads remain bounded to 4 KiB per sample.
The extra table preview is separate from that constant-block bound. Issued copy
resources and trampolines retain their documented process lifetime.

Header fields `jitter_min_abs=0.3125` and `jitter_selection_limit=20` pin the new
policy. The projection report adds selection attempt/skip counts and selected
frame/candidate. Independent review requires attempts 1–20, skips = attempts−1,
the next-frame identity, exact candidate agreement and both strength bounds.
All v1.6.0 matrix/provider/frame/restoration checks still apply. Earlier capture
versions keep their original interpretation; broad pixel/DLAA flags remain false.
New fault codes are 12 (invalid preview), 13 (selection limit), and 14 (changed
or inadequate stimulus after baseline).

## Verification

Release `/W4 /WX` build passes **86 native and 41 Python tests**. Added native
coverage includes deferred selection, each axis just below threshold, the exact
threshold, unreadable/nonfinite lookahead, twenty weak attempts, success on the
twentieth attempt, and a changed strong candidate before injection. The delayed
paired WARP fixture forwards an unselected CPU producer, then verifies exactly
the three selected projection-associated color/motion copies. Its owned queue
reset models the next frame; it is not a game render or raster-jitter model.

Python tests reject damaged selection counts, identities, budgets and weak or
nonfinite candidates. The archived v1.6.0 game capture must still pass without
altering its frozen review. Existing v1.3/v1.4/v1.5 captures and 48 executable
code spans are checked during preparation. The closed v1.6.0 archive is verified
before preparing the fresh `local/temporal-jitter-20260925/v1.6.1/` directory.

## Runtime procedure and rollback

Use the existing bare-profile MO2 test entry, offline/private, with **DLSS Off,
TAA On, DX12, unchanged 2560×1440 resolution**. Load the save and keep the
character/camera still. Reply **Save loaded** from the phone. Arm only after
checking that process generation, deployed DLL hash and ready event. No camera
pan or DLSS toggle is requested. Keep the scene steady until capture completes.

The fresh deployment preserves the exact previous slot DLL and any output
sidecar, freezes the source and records configuration hashes. After the user
exits normally, use this directory's `deploy.ps1 -Action Restore`. Do not reuse
the closed v1.6.0 deployment or overwrite its evidence. Game executable, original
assets, profile and graphics settings remain unchanged.

Analysis will report the actual chosen matrix displacement, fit color on
baseline/recovery-selected pixels, and compare all three motion fields. Retain
the known-shift estimator controls and show sensitivity to smoothing/masks.
This run should tighten color scale; acceptance alone does not establish it.
Same-frame scene depth, typed views and bounded SDK output remain later DLAA
integration requirements.
