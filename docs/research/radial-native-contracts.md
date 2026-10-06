# Native radial-mask and shared-key contracts — v1.25.42

Prepared 2026-10-02. This candidate addresses two admission failures recorded in
the [combat family validation](postprocess-family-validation-result.md). Live
Safi/Alatreon compatibility is pending; offline success does not establish it.

## Evidence and change

The native radial helper selects a default texture when its custom-mask pointer
is null. The paired Safi snapshot, helper disassembly and manager read identify
the same 1x1 engine-format-7 RGBA8 texture at both radial t1 and t2. The old t2
rule admitted only engine format 23 and rejected this valid default. The change
admits format 7 only with actual RGBA8 UNORM/sRGB storage. Device identity,
dimensions, sample/mip limits, shader sampling support, owner rereads, shader
recipes and native binding checks remain required. No mask or sampler is
substituted; its dimensions, SRV and coordinates remain native. Aliased t1/t2
resources are retained once.

The same run recorded two source-free descriptors with key 0x78001004 and the
same output owner. A sort key is not a unique effect identity. Equivalent
candidates now share command selection only when their complete output-owner
snapshots match. Every actual draw is retained and independently qualified, in
native stable queue order. Conflicting outputs, source-producing ambiguity,
duplicate packets, capacity overflow and invalid dependencies still reject.
The scene/copy/radial graph uses the latest preceding scene writer for each
pair. There are no monster names, area IDs or attack triggers in admission.

Dependency rejection receipts now carry the failing draw's key and packet.
The detailed single-descriptor recorder still does not support ambiguous
multi-descriptor captures; ordinary command receipts retain that evidence.

Within one prepared frame, changing the mask **owner** remains forbidden.
The existing same-owner storage/view replacement path remains intact, with old
and new resources retained until both fence completion and command-list Reset.
Switching between default and custom owners is supported through fresh plans.

## Verification

The production graph fixture passes on WARP and the local NVIDIA adapter. New
cases cover the shared default mask, custom/default fresh plans, forbidden
mid-plan owner replacement, integer/unknown formats, two repeated copy/radial
pairs, descriptor-order independence, conflicting output owners, duplicate
packets and exact rejection key/packet reporting. These are synthetic native
structures backed by real GPU resources and captured shader bytecode.

Existing GPU checks also pass on both adapters: expanded/native copy footprints,
8,192 radial pixels matching the current-binding reference, 81,920 exact
resource-retention pixels, API mutation rejection, state import and fence plus
Reset retirement. The 96-frame continuous-route regression matches 786,432
pixels across 288 effects with GPU validation. These pixel checks exercise the
existing custom-mask path; default-mask and repeated-pair additions exercise
production graph admission, not a captured complete combat-frame replay.

Eleven focused CTests and 78 Python tests pass. Historical v1.25.41 and synthetic
v1.25.42 log review, including required-channel rejection, are checked separately.
Final builds use /W4 /WX and have no warnings. Earlier test-harness compilation,
observation-reset and missing-fixture invocation failures are retained locally
with their corrected passes.

## Hook and deployment scope

No new hook target, calling convention, native patch span or ownership contract.
The existing queue-preparation callback builds a frame-local plan under the
route lock; resource pins and native draw-entry/API checks retain their existing
lifetimes. Retail executable SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded research executable SHA-256:
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Native helper provenance and captures are in the preceding sealed local run.

Candidate evidence: `local/radial-native-contracts-20261002`. Its preparation,
source snapshot, deployment and prelaunch receipts identify exact binaries and
backups. Native AA/DLSS remain Off; DX12 windowed 2560x1440 and the recorded
Stracker-enabled solo/offline profile remain the test baseline. Keep the new DLL
installed after the run. Rollback is documented beside the deployment script;
restoring a sealed output-directory sidecar requires a fresh folder before any
subsequent launch. No original executable or game asset is modified.

Next: main-menu/save baseline, Safi nova, Alatreon frost/nova, recovery and cleanup.
Compare new default-mask and repeated-pair admission against the previous two
rejection signatures. Hunter silhouette scaling and camp entry remain separate
open work; this is not a claim that all post-processing effects are handled.
