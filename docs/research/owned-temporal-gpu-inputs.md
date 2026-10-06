# Owned GPU input views and conversion

2026-09-25. **Standalone D3D12 replay passes on the captured hardware adapter**
for all three full-resolution frames from [pan run 31044](ordinary-taa-depth-pan-result.md).
Explicit shader views read color/depth correctly and convert packed motion to
the validated two-component history-pixel offset. Every output value agrees
with both C++ and independent NumPy references. No game process was opened,
hook changed, test DLL installed or vendor SDK called in this step.

## Input ownership decision

Use private owned input textures with explicitly created views for the first
bounded evaluation. Captured native CPU descriptor handles remain opaque;
their internal contents are not established by the earlier ownership trace.
The new path does not depend on reading those internals. The recorder's existing
format/state/frame checks identify the copy sources, and owned resources carry
the pixels across the evaluation boundary.

The standalone implementation is `src/temporal_gpu_inputs/`, with
`mhw_temporal_input_replay` and `tools/replay_temporal_capture.py`. It is not
linked into the game probe. The wrapper verifies the entire sealed capture
archive and reproduces its frozen review before any replay. It selects the
same adapter by the recorded LUID, never silently substituting another GPU.

| Plane | Explicit input SRV | GPU operation | Comparison output UAV |
| --- | --- | --- | --- |
| Color | R11G11B10_FLOAT, Texture2D, mip 0, one mip | Integer-coordinate Load, no tone map | R32G32B32A32_FLOAT, alpha 1 |
| Depth | R32_FLOAT, Texture2D, mip 0, one mip | Integer-coordinate Load, no linearization | R32_FLOAT |
| Motion | R32_UINT, Texture2D, mip 0, one mip | Unpack two halves, mask low flag bit, scale/sign | R32G32_FLOAT |

All views use plane slice 0 and default component mapping. Input textures have
one slice/sample/mip. Texture2D and shader-load support are queried; output
formats additionally require typed UAV-store support. Comparison output
formats are diagnostic choices, not a claim that all are final SDK formats.

The compute conversion is:

```text
dx = f16tof32(packed >> 16)
dy = f16tof32(packed & 0xfffe)
history_pixel_delta = (-dx * width / 2, +dy * height / 2)
previous_coordinate = current_coordinate + history_pixel_delta
```

This preserves the convention established by the game's captured shader and
independent image matching. It does not convert to a particular SDK's API
scale or settle exposure/HDR options, jitter configuration or projection flags.

## GPU execution and lifetime

The replay owns its device, DIRECT queue/list/allocator, root signature, PSO,
descriptor heap, upload/readback buffers and all textures. Inputs are created
in COPY_DEST, uploaded, then explicitly transitioned to NON_PIXEL_SHADER_RESOURCE.
Outputs start in UNORDERED_ACCESS and transition to COPY_SOURCE before readback.
There are six texture copies, six transitions and one compute dispatch per frame.
The 16x16 shader bounds-checks excess threads.

A private fence signals after submission. Mapping and comparison happen only
after completion and a successful allocator/list reset retires the recording.
All descriptors, resources and uploads outlive GPU use. An uncertain submission,
Signal failure or wait timeout retains the complete context until the standalone
process exits. Pre-submission failures release unissued resources normally.
The input limit is 16 MiB per packed plane; the bounded image allocation account
is capped at 512 MiB. That account uses texture allocation information and
buffer footprints; it is not a measurement of total driver/process VRAM.

Microsoft defines SRV format/dimension/component mapping explicitly through
[CreateShaderResourceView](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createshaderresourceview).
The explicit transitions follow its [D3D12 resource-state rules](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12).
NVIDIA's [DLSS integration guide](https://raw.githubusercontent.com/NVIDIA-RTX/Streamline/main/docs/ProgrammingGuideDLSS.md)
identifies required color/depth/motion/output resources and warns that evaluation
can require restoring command-list state. A separate owned command list is the
chosen design for the first bounded offscreen evaluation; this is an integration
decision, not evidence that the in-game handoff is already implemented.

## Verification result

The new C++20 targets build in Release with `/W4 /WX`. Two new CTest entries
pass: pure extent/plane-size/finite/depth-range/packed-value controls, and WARP
typed-view replay. WARP cases use 17x19 and 64x16 images, exercising row padding,
excess dispatch threads, HDR values, signed/zero/subnormal half motion and the
masked low bit. D3D12 debug and GPU-based validation are enabled and clean.

The unchanged native recorder suite was not rerun: the new library/executable
are separate targets and no hook source or recorder DLL changed.

The hardware replay uses LUID **0:97816**, matching the sealed capture. Frames
**22592, 22593, 22594** each contain 3,686,400 pixels. Across the three frames,
**77,414,400 output float values** match with zero numerical difference:
four color components, one depth component and two motion components per pixel.
Depth is also byte-identical to the source files. D3D12 debug/GPU validation
report zero errors and warnings in every replay. Completion and allocator
retirement verify for all frames. The raw outputs total 309,657,600 bytes.

Private evidence: `local/temporal-gpu-inputs-20260925/v1/`, including native test
details, preserved replay executable, all GPU outputs and `hardware/verification.json`.
The wrapper verified all 188 files in the closed source archive, hash
`d5706b395a95d6d3080ed76017b1bbb4dbe8deb13959fb75dd851834178846e3`.
Its source log hash remains
`c203c45b8f956f0e3c81251bbaf4bf57b4462b4db9af784703e2b65c70592457`.

## Live handoff contract

The source code and accepted traces establish two different copy intervals:

| Native source | Verified interval | Required original-state restoration |
| --- | --- | --- |
| Matched real depth candidate | After CopyResource returns at image+259E100, selected TAA command-list generation | COPY_DEST (0x400) |
| Current color | Just before original TAA Dispatch, caller image+259CFCA | NON_PIXEL_SHADER_RESOURCE (0x40) |
| Packed motion | Same TAA Dispatch callback | PIXEL_SHADER_RESOURCE + NON_PIXEL_SHADER_RESOURCE (0xC0) |

The live implementation should prepare bounded owned GPU textures on the
worker, snapshot depth at its verified native-copy interval, and snapshot color
and motion at the verified TAA interval. Require the existing exact frame,
packet, list, allocator and Reset witnesses to join them. Restore each native
source state before forwarding. Conversion/evaluation may consume the owned
set only after an explicit producer-completion dependency; it must not assume
the game retains the original contents or that depth remains readable at TAA.

Keep the first conversion/evaluation on a private command list and private
output, retaining all objects until its own completion fence. This avoids
needing to reconstruct native pipeline state for the first bounded test. It
does not address presentation or replacing TAA yet. Complete original-writer
coverage is still unproven; private copies protect their contents after capture,
not against an unobserved competing writer during the native copy interval.

**Update 2026-09-25:** bounded live owned-copy handoff is implemented and
[verified in game, run 51256](live-owned-temporal-inputs-result.md).
All 77,414,400 converted float values match independent references, with
qualified producer completion and private conversion completion/retirement.
See [runtime contract](../runtime/temporal-owned-inputs.md).

**Next:** feed the validated private set into the first bounded SDK evaluation.
Engine descriptor internals
are not a prerequisite for this chosen owned-input path. SDK output, continuous
jitter/history, exposure choice and game output integration remain unverified.
