# Matched current-color and motion readback v1.5.0

Accepted and closed 2026-09-25: [run 43820](../research/ordinary-taa-paired-result.md)
saves all three pairs and supports motion sign, pixel scale and frame association
in the tested static scenery (~0.12 px median disagreement from independent
image matching). Previous DLL restored; settings unchanged. Jitter inclusion
and sustained DLAA remain unverified. The preparation contract follows.

The [rightward pan](../research/ordinary-taa-motion-pan-result.md) supports
camera-motion coverage and horizontal sign. This recorder adds current color
to the same three motion receipts so actual image displacement can be compared
with the shader-decoded vectors. It does not enable DLAA or certify alignment.

## Recording contract

The seven hooks, native Win64 ABIs, system-method discovery and 48 executable
code gates remain as documented for the [submission observer](temporal-submission-observer.md)
and [motion readback](temporal-motion-readback.md). No additional game hook or
offset is introduced. Tested executable version `0.0.0.0`, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.

The producer retains all seven named source identities as before; both the
motion identity and current-color identity reach the matched native consumer.
Three consecutive producer frame numbers are now required for paired mode.
Both source descriptions, distinct resource/canonical identities, and locally
established nonconflicting states are checked before the first private copy.

| Source | Format | Exact resource flags | Required/restored state |
| --- | --- | --- | --- |
| tGBuffer4 | R32_UINT (42) | ALLOW_RENDER_TARGET (1) | pixel + nonpixel shader read, 0xC0 |
| tBaseMap | R11G11B10_FLOAT (26) | ALLOW_RENDER_TARGET + ALLOW_UNORDERED_ACCESS (5) | nonpixel shader read, 0x40 |

Both are full-resolution 2D textures with one mip, array slice and sample,
sample quality zero and unknown texture layout. These exact color properties
and its native render-target-to-0x40 transition were observed in run 34276.
The second source is the current TAA input, not the output/history texture.

Before admission, six separate READBACK buffers are allocated. Each requires
an exact validated copy footprint and at most 16 MiB, including row padding;
the combined ceiling is **96 MiB**. At 2560×1440 the six buffers/files hold
88,473,600 bytes (84.375 MiB) altogether. No renderer resolution is changed.

At each matched Dispatch, using the same command list and Reset generation:

1. Transition motion 0xC0 → COPY_SOURCE, copy subresource zero, restore 0xC0.
2. Transition current color 0x40 → COPY_SOURCE, copy subresource zero, restore 0x40.
3. Forward the original Dispatch once with unchanged arguments.

Each copy is bracketed separately in the shared submission order. Both have
the same receipt/frame/packet/list/allocator/Reset/Dispatch marker. Motion
restoration must precede the color copy; color restoration must precede native
Dispatch. There are at most **18 private command-list operations** (12 barriers,
six copies), three private queue Signals and no observer ExecuteCommandLists.

Each pair shares the independently observed successful Close, unique actual
Execute, completed private fence and successful Reset retirement. All six
copies must qualify after hooks disable and callbacks drain before any Map.
Every issued allocation and its source/list/allocator/device references stay
retained until process exit. Unissued buffers release on failure. The existing
SEH restoration/quarantine behavior remains; uncertain operations never qualify.

Files are exclusively created with row padding removed, preserving original
little-endian 32-bit packed words:
`inputs-<pid>-<creation>.motion-{0,1,2}.u32` and
`inputs-<pid>-<creation>.color-{0,1,2}.u32`.
Partial files are retained on failed saving and aggregate acceptance is false.
Only the worker maps/hashes/writes after all completion and retirement checks.

## Independent review and decoding

Version 1.5 extends the existing reviewer with color format/state/source checks,
copy ordering, consecutive frames and matching pair identities. Six logged
readback buffer pointers must be nonzero, distinct and separate from sources.
Every footprint, raw byte count and SHA-256 is checked. Prior 1.3/1.4 captures
remain reviewable. `paired_color_motion_readback_verified` means paired copy
provenance, not visual correspondence; `color_motion_alignment_verified`, broad
`motion_contents_verified`, typed-view verification and DLAA readiness stay false.

`temporal_color.decode_color` follows the locally installed Windows SDK
10.0.26100.0 `DirectXPackedVector.h` XMFLOAT3PK layout and the corresponding
`XMLoadFloat3PK` implementation: unsigned R in low 11 bits, G in the next 11,
B in the upper 10, exponent bias 15, mantissa widths 6/6/5. It preserves HDR
values, subnormals and nonfinite values for analysis; no gamma/tone-map conversion
is performed by the recorder. The SDK headers are not copied into the repository.

Release `/W4 /WX` build and **58 native tests / 32 Python tests** pass.
Nine new GPU cases use two distinct real WARP textures, separate known patterns
and the actual lifecycle detours. They check exact contents, padding, state/format
rejection, source aliasing, skipped frames, missing retirement, failed private
Signal and failed color Map. All 663 RGB triples from SDK-packed and GPU-copied
fixture pixels decode to their known input values; 663 motion words remain exact.
Python tests also reject damaged color evidence, order, paired allocations,
frame adjacency, files and budgets. The existing pure contract cases remain.

## Run, analysis boundary and rollback

Fresh evidence/deployment/rollback directory:
`local/temporal-pairs-20260925/v1.5.0/`. Preserve all earlier closed archives.
The local `deploy.ps1` installs only while the game is closed, saves the exact
previous slot and output sidecar, and checks configuration identities. After
normal exit, `deploy.ps1 -Action Restore` restores that DLL and restores/removes
the temporary sidecar. No graphics, profile, mode or original game files change.

Use the existing bare-profile MO2 test entry with DLSS Off/TAA On, DX12,
2560×1440, offline/private. Load the same scene with textured static scenery,
keep the character still, then gently pan right. Arm only after the user confirms
panning; keep motion underway until capture completion is acknowledged. Reply
from the phone to preserve focus. Save/review all six files before requesting exit.

After acceptance, compare C[n−1] and C[n] against decoded M[n] using static,
textured regions. Evaluate sign, scale and alternate frame associations instead
of assuming the producer frame label proves pixel correspondence. Exclude image
borders/disocclusions and independently moving objects; account for brightness
changes when measuring image agreement. Three frames provide two adjacent image
pairs. If one scene lacks usable texture or the result is ambiguous, report the
limit rather than claiming scale/alignment. Jitter inclusion and sustained DLAA
input/output integration still require separate evidence.
