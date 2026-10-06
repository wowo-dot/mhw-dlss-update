# Gallery DOF compute bundle capture (v1.25.35)

## Confirmed gap

The closed v1.25.34 run rejected the first compute command under DOF key
`0x78006595`, before dependency discovery or the DOF composition adapter ran.
Its queue contains `[11,6,6,6,6,6,6,0]`: one state command, six direct compute
dispatches, and a composition draw. Dispatch groups are `(8,1440,1)`,
`(128,5,1)`, `(16,9,1)`, and three `(80,45,1)`. This shape was already present
in the earlier prepared-command evidence. The v1.25.34 GPU fixture covered
composition but omitted those producers; passing it did not establish a
complete Gallery route. See [the live result](dof-output-route-result.md).

The shaders and bindings of those six dispatches have not yet been captured.
Their role, coordinate conventions, and dependence on reconstructed scene
color remain unproven. Simply admitting opcode 6 would not establish correct
sampling, ordering, or resource states.

## Capture contract

v1.25.35 adds a read-only `compute_dependencies` channel at the existing
completed post-filter preparation callback. It installs no new hooks and
does not relax the rendering admission rule. The selector requires reason
17, branch 7, opcode 6, and exactly one matching intermediate descriptor of
the observed DOF type (image-relative `0x3526650`). This offset is a diagnostic
selector for the hash-gated executable, not a distributable signature.

Tested host SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
The shared packet decoder matches the earlier compute producer/recipe mapping
in `temporal_bindings/inputs.cpp` and `shader.cpp`. The packet is 48 bytes;
dispatch mode is only the byte at +0x24, and opcode is the low five bits.
Shader-set +0x44 is its compute binding range and +0x78 its compute shader.

Bounds are two complete samples, sixteen attempts, at least sixty frames
between attempts, sixteen commands per selected key, eight compute dispatches,
four draws, sixty-four owners per recipe, 128 binding words, 64 KiB per compute
shader and 16 KiB per graphics shader. Two full snapshots must compare equal,
and renderer frame/epoch/view/bank/context must remain stable. Reads are bounded
and guarded. Samples live in static recorder storage, not render-thread stack
temporaries. Exhaustion stops further capture reads.

Each sample preserves queue order, direct dispatch sizes, shader bytes,
technique, owner IDs/modes/pointers, SRV/UAV binding words and descriptor handles,
texture owner/backing/resource identities and declared dimensions/formats.
It includes the existing composition-draw recipe and geometry in the same
snapshot. Constant-buffer owners are recorded as metadata; their bytes are
not guessed or dumped. These are CPU observations, not GPU descriptor/state,
content, or execution verification. No COM references are retained or invoked
by this capture. Native command-arena pointers must not be reused after the
sample's frame ends.

`extract_compute_dependencies.py` validates the channel, extracts private DXBC
by hash under `local/`, and reports ordered UAV/RTV-to-SRV identity edges plus
external reads. Repeated resources are associated with the latest earlier
writer, never a later write. Shader reflection/disassembly and actual resource
state/coordinate validation must precede any compute-route admission change.

## Verification and next run

The CPU fixture covers the full eight-command sequence and synthetic producer
edges, double-read disagreement, indirect dispatch refusal, owner/range/shader/
queue bounds, stale descriptors, unrelated selectors and capacity/throttling.
The native plan fixture now explicitly verifies that the complete observed
sequence is rejected at its first compute. The separate composition-only GPU
fixture remains evidence only for that adapter.

Next run: normal solo/offline profile, DX12 windowed 2560x1440, native AA/DLSS
Off, unchanged window size. Arm at the main menu, confirm loaded-scene health,
play the affected Gallery cutscene, capture the two bundles, confirm recovery,
then intentionally stop and independently verify cleanup. Gallery DLSS may
still suspend; this build obtains the missing contracts. Keep the current DLL
installed after normal exit, as requested. Exact prior DLL/sidecar backups and
explicit restoration instructions belong to the new local evidence directory.
