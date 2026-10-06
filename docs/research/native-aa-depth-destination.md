# Depth destination identity correction, v1.21.5

Prepared 2026-09-27 after the [v1.21.4 live diagnostic](native-aa-depth-eligibility-result.md).
That run recorded 312 scene-depth rejections changing only four transient source
attachment fields; the destination texture/container/SRV identities stayed equal.
This change narrows the dispatch-time comparison to that destination identity.
It has not yet been tested in the game.

## Admission contract

`depth_destination_equal` requires a readable scene and exact equality of:

- Scene container.
- Full destination texture `Owner` (all thirteen members).
- SRV pointer, CPU descriptor and SRV readability.

It excludes only `attachment`, `attachment_owner`, `attachment_resource` and
`attachment_readable`. Those describe a mutable source attachment at the current
context, rather than the retained destination copy used by the resolve.
The original full `SceneDepth` equality remains unchanged at publication, and
it remains observed at dispatch for diagnostic comparison.

All subsequent gates remain unchanged: motion state, private depth copy's list,
generation, frame, availability and unused status, resource-pin agreement,
history/reset validity and output-copy compatibility. Earlier depth-copy
production still validates its frame/epoch/view/bank/async/container, packet,
source/destination identity and API copy states before creating the private
copy. This change does not invent or reuse a missing copy. Missing/stale leases
continue to suspend reconstruction and use unfiltered output.

The entire prior runtime source is reproducible by removing the new include,
trace instance/serialization and replacing one conditional with the v1.21.4
conditional. The scene reader, full producer equality, copy path, SDK, native
state supplement, queue ordering, ownership and hook-removal code are unchanged.
There are no new hooks, native memory writes, game calls or unconditional GPU
commands. Additional SDK work can occur only after the narrower identity check
and every remaining gate succeed.

The check executes inside the existing `ID3D12GraphicsCommandList::Dispatch`
hook (`WINAPI`, Windows x64 ABI), at hash-gated consumer return RVA `0x259CFCA`,
under the runtime mutex and existing callback/resource lifetime. The same depth
snapshot is used for old observation and new admission; no extra native read.
Diagnostic addresses are copied values, not later dereferenced authorities.
Pinned host version 0.0.0.0 SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
All existing code/host gates remain required; no new offsets were derived.

## Diagnostic agreement

The old `depth_eligibility` channel continues to report the whole-structure
comparison. Its scene `rejected` counter now means that the **old comparison**
would reject; it is not necessarily an actual unfiltered frame in v1.21.5.

New header/status policy `depth_destination=depth-destination-identity-v1`
reports checks, accepted, rejected and `attachment_only`, with first-eight and
latest samples of attachment-only admissions. Values and difference masks make
it possible to independently replay that only the four allowed fields changed.
An accepted destination sample does not itself prove a successful SDK Evaluate;
the later lease/history gates may still suspend that frame.

The tenth checkpoint channel, `depth_eligibility_verified`, now validates both
objects. Its returned `destination` object includes `destination_identity_verified`.
Full new-version review and the frozen capture script require the new object.
The reviewer checks:

- New checks equal old scene checks; new accepted equals old passes plus
  attachment-only admissions; old rejections equal new rejections plus those
  admissions. Actual dispatch reason 3 equals the **new** rejected count.
- Retained old-comparison samples independently constrain the accepted/rejected
  partition under the new rule, accounting for omitted bounded samples.
- Every new retained admission differs only in allowed attachment fields;
  overlapping old/new samples carry identical observations.
- Lease checks follow new accepted destinations, and unchanged reason-7/SDK
  coverage remains required. Both channels preserve immutable first receipts
  and monotonic counters.

Historical v1.21.4 logs remain reviewed under their original comparison policy.

## Verification and live plan

Release build passes /W4 /WX, with no new project warning. 42 native and 163
Python checks pass. The destination fixture covers 20 cases including each of
the 21 fields' comparison behavior, a texture change combined with an attachment
change, unreadable core depth, optional source-attachment read failure, every
lease predicate, and 1,000 bounded attachment admissions alongside 1,000 genuine
descriptor rejections. It also confirms producer full equality still detects
each field change. Python replays the nine retained v1.21.4 scene samples and
checks corrupt/missing/mismatched evidence and new-version policy enforcement.
These are offline contract tests, not proof of in-game recovery or visual quality.

Use the same bare MO2 forced-DLL entry, native AA/DLSS Off, DX12, Windowed
2560x1440. After save load, verify identity/startup and arm once. Inspect all ten
channels, attachment-only admissions, actual dispatch-reason-3 counts, lease
failures and SDK growth. Follow with one area change through a loading screen
and walking/panning. Observe recovery rather than assuming every admitted
destination reaches Evaluate. Announce intentional stop, verify drain, request
normal exit, then archive and keep the DLL installed.

Root `local/native-aa-depth-destination-20260927/v1.21.5/` preserves the v1.21.4
DLL and sidecar before installation. If explicitly requested with the game
closed, `deploy.ps1 -Action Restore` restores that exact prior slot. Do not
restore automatically. No saved configuration changes are required; the build
is not armed by deployment. Empty-copy lifecycle, whole-renderer AA exclusivity,
CMFM/UI, DLSS upscaling and frame generation are outside this change.
