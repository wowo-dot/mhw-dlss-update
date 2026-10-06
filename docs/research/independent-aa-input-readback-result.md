# Independent-AA native input readbacks verified

2026-09-26, v0.5.0, PID 46856 / creation 134349214275665878. The independent
AA-Off path captured three color/depth/packed-motion triplets at a qualified
native transition boundary. The frozen reviewer accepted all nine images,
their resource states, restoration and GPU completion/retirement chains.

The user closed normally. Exact previous diagnostic DLL restored at
21:39:22.264686 +03, test sidecar removed, all four configurations unchanged.
No next build installed or armed. Keep native AA and DLSS Off, Windowed
2560x1440, DX12 On and the bare offline profile.

## Runtime evidence

Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
DLL SHA-256 `41275553793713c42b2443ca6bcb3324d47586501ad02dda634c8ff9c43d12ea`.
D3D12 device 354368608, adapter LUID 98931:0, swap-chain format 87,
2560x1440. [Runtime entry/ownership contract](../runtime/independent-aa-input-readback.md).

Eight source frames 9469-9476 produced eight distinct packets and exactly one
consumption each on thread 55680. The renderer counter was source +1 at every
consumption. Immutable source identity was retained. Native parameter bindings,
owner identities and original post-queue restoration passed; native AA remained
Off. Each callback added one empty packet; each native phase appended 32 total.

All eight requests reached the first relevant native barrier after their marker
on the same Reset generation. The paired color/motion transitions 4 -> 128
provided explicit states after native forwarding; depth was already known 128.
No state was borrowed from another command list or previous frame. The earlier
marker snapshots correctly still report motion unknown.

| Input | Resource | Format | Flags | State at read point |
| --- | --- | --- | --- | --- |
| Color | 5234444272 | R11G11B10_FLOAT (26) | RT + UAV (5) | PIXEL_SHADER_RESOURCE (128) |
| Depth | 5234334112 | R32_FLOAT (41) | RT (1) | PIXEL_SHADER_RESOURCE (128) |
| Packed motion | 5234361040 | R32_UINT (42) | RT (1) | PIXEL_SHADER_RESOURCE (128) |

All are distinct, full-size, single-mip/slice/sample images. Receipts 0, 3 and 7
(source frames 9469, 9472, 9476) each captured the three inputs. Each source was
transitioned 128 -> COPY_SOURCE, copied, then restored to 128 before returning
to native rendering. Nine images of 14,745,600 bytes each were saved: 132,710,400
bytes total, matching their format, extent, footprint and SHA-256 declarations.

For the first sample, Reset 3555 precedes marker 3599/3600, the read-point
barrier is 3601/3602, Close starts at 3603, Execute is 3609/3610, Signal is
3611/3612, and retirement Reset is 3649/3650. The same chain is independently
verified for all eight. All fences reached value 1 at tick 32050750.

The 500 ms journal contains 3546 API events, 759 selected barrier calls and 966
native transition records on one observed queue. Faults/pending callbacks zero,
all six hooks disabled and eight descriptors detached. All 548 journal pins,
watch/request pins and private readback references were released after the
required completion, retirement and save steps. The shared journal's legacy
copy fields are unused; these new copies are scoped inside the linked native
barrier event and reported in `input_readbacks`.

## Pixel inspection

The saved color images show the loaded scene without the HUD. Depth visually
matches the character, companion, floor and background geometry. Motion is
concentrated around animated objects; most of the still background has zero
decoded motion. No obvious empty, corrupt or unrelated image was found in the
three triplets. This is visual evidence, not an exact temporal alignment proof.

All decoded color, depth and motion values are finite. Depth ranges from zero
to approximately 0.06881 in each image. About 4.66-4.69% of motion pixels have
decoded magnitude greater than 0.01 pixels; the 99th-percentile magnitudes are
approximately 1.00, 0.81 and 1.11 pixels. Color, depth and motion all change
between captures, consistent with an animated scene and not identical cached
buffers. Such changes do not establish the exact frame of every producer.

`inspect_pixels.py` cross-checks 303 color words and 303 motion words against
the existing scalar decoders and preserves numeric statistics. Its contact
sheet uses Reinhard/gamma color, logarithmic depth and HSV motion direction/
magnitude for inspection; these transforms are not native presentation output.
Preview and raw captures remain ignored local game-data artifacts.

## Meaning and next integration

This establishes usable input capture and scoped native-state restoration with
native AA Off. It resolves the v0.4.0 missing local motion state by moving the
actual read to the subsequent native declaration. It does not claim that the
earlier marker acquired a known motion state.

Next, carry the immutable request/projection association into this entry and
connect scoped camera jitter and the existing input conversion/DLAA record.
Any compute work added inside the native barrier callback must also preserve
the native GPU bindings; plain copies in this version do not modify them.
Verify the jitter used to render each request, motion conversion and history
reset before treating the result as independent temporal reconstruction.
Output handoff, exclusive suppression of other native AA choices and AA-Off
failure latching remain integration work. There is no need to repeat unchanged
input readbacks as a separate milestone.

No SDK, jitter, DLAA evaluation or output replacement ran in this test.
The frozen reviewer explicitly leaves pixel freshness, cross-queue input
writes and live independent DLAA unverified. Historical continuous DLAA success
does not change those limits for this new path.

## Archive and rollback

Evidence: `local/independent-aa-inputs-20260926/v0.5.0/`, with 347 frozen sources
and 389 sealed prelaunch files. Prelaunch SHA-256
`a1a4e76424910c1a62cdcdbbeab33e4d92d133e079ce6bfc3eab89d73f090980`.
Saved log/capture: 891,486 bytes, SHA-256
`37afc3c65dffe2349ab6f2dd03d2ed9f3598dc578d81bebaa934fd2ac58b5332`.
`review-46856.json`, `save-46856.json`, `pixel-inspection-46856.json`, raw image
hashes, preview, arm/prearm receipts and `restored.json` are retained.

No native source/test/tool changed during this capture/closure. Previous slot
restored to `689bef7cf00ff71e4518f452f0467edf6c3d877f2a6dd090553388d8b5f288cc`.
No original executable or game asset was modified. Ten native checks and all
37 Python checks passed before deployment; the actual run now adds independently
verified live readback evidence.
