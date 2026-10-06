# Dependency binding boundary result: auxiliary mask replacement

## Run and diagnosis

v1.25.31, PID 4236, creation 134353055156351855. Executable SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
DLL SHA-256 `bb0d06f3e942975cf0b19206da92a1b42a90acd7806dc3c20495e4fffa278f45`.
Evidence root: `local/dependency-binding-check-20261001`.
The diagnostic contract is documented in
[dependency-binding-boundaries.md](dependency-binding-boundaries.md).

Main menu passed at 606 DLSS frames, save loading at 2,039. The user reported
a clean loaded scene, then a shutdown/artifact during wingdrake travel.
Terminal evidence confirms full shutdown: 3,289 DLSS evaluations, 285
unfiltered resolves, route fault 6 and runtime fault 45, with failure
`stream dependency bindings changed`. Nine scene-copy and eight radial draws
executed. Gallery was not tested. This diagnostic run did not fix travel.

## Confirmed fault-time evidence

The first rejection is radial stage 5, frame/epoch 16266, tick 34376281.
Both native-entry and DrawInstanced checks were readable and satisfied the
binding contract, but differed from preparation. The two receipts are
identical: seven differences, field-category mask 896, read phase/index 0/0.
The diagnostic independently passed review while renderer health failed.

All differences belong to the mask bound at PS t2:

| Field | Prepared | At native entry and DrawInstanced |
| --- | --- | --- |
| Owner width/height | 128 x 128 | 256 x 256 |
| Primary and effective backend | Previous backend | Replacement backend |
| GPU resource | Previous resource | Replacement resource |
| SRV view and CPU descriptor | Previous view/descriptor | Replacement view/descriptor |

The same owner object remained, as did IDs, modes, binding words, constants,
shader identities and other captured owner fields. Private receipts retain
the exact pointer changes. Owner indices 29/30/35/37/38 identify t2 width,
height, primary backend, effective backend and GPU resource; view/descriptor
indices are both 2. There was no further observed change between draw entry
and the API check. This proves replacement between preparation and entry,
not a mutation caused by the native binder during this draw. These remain CPU
table observations, not independent GPU descriptor-content readback.

Texture streaming or resolution promotion is a plausible cause of the
replacement, not established by this capture. The proven integration defect
is that the prepared snapshot and pinned resource describe an earlier mask,
while the native draw uses its replacement. Removing equality checks alone
would not ensure that the actually used resource remains alive.

## Cleanup and next step

All 3,574 GPU signals completed and all 3,574 main recordings were released.
All 4,052 jitter changes were restored; full render scale returned. Hook
domains reported disabled and pending callbacks were zero. Read-only live
verification confirmed all 40 native code spans and 18 hook heads restored.
The route retained 3,323 recordings and released 3,321; two references stayed
pinned until normal process exit. Do not claim all route references were
released in-process. The user confirmed a normal scene and normal exit.
The closure review verifies process/collector absence, installation and
configuration hashes, and seals the evidence. Keep v1.25.31 installed.

Next implementation should validate and retain replacement auxiliary inputs
at the actual draw boundary, with a consistent snapshot and correct resource
state/descriptor correspondence. Keep scene/temporary dependency provenance
strict. Retain every resource used by recorded GPU work through fence
completion and command-list Reset, including the earlier resource where an
earlier recording still uses it. Reproduce a 128-to-256 replacement between
preparation and draw in the production fixture before another runtime test.
No replacement-admission implementation or new candidate is prepared yet.
