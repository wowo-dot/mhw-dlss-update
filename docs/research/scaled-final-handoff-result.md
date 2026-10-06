# Native final-copy route accepted

2026-09-28, v1.23.7, PID16580 / creation134350241839399603. The three-frame
capture and frozen reviewer passed. User confirmed the scene returned to normal
and exited normally. Process exit, unchanged graphics/profile/modlist/mode,
installed DLL, backup, frozen sources and prelaunch manifest were verified.
The current DLL remains installed as requested.

## Confirmed live result

Frames14906–14908 captured active1280x720 inside2560x1440 allocations. For each
frame the native final helper at RVA0x23917a0 was called from0x23931b3 on the
same render thread as the qualified TAA dispatch. Frame, epoch, bank, view,
context and ring-output owner matched. Three distinct ring-output resources
copied into one stable destination resource; both sides were2560x1440,
DXGI_FORMAT_R11G11B10_FLOAT(26), single-sample, one mip and one array slice.

Each helper call appended exactly one opcode13 packet. It copied the rectangle
`[0,0,1280,720]` to destination offset`[0,0]`, with zero mode and subresources
and null optional payload. Source/destination owners, rectangle, packet and
queue were stable across the call and bounded rereads. The resource COM pins
were all released: six acquired/six released. No observer mutations occurred.

**The game only queues a half-size-region copy at this point, despite both
allocations being full-size.** Merely filling the ring texture with a2560x1440
DLSS result would not make this packet copy the entire result. A full-output
handoff and validation of subsequent scaling are necessary.

## Input and shutdown checks

All nine raw and nine converted input images were saved. GPU submission,
completion and recording retirement passed.19,353,600 active float comparisons
had maximum conversion error0. The native-current-use color proof was used on
the first frame; later frames used explicit local transitions.

The projection viewport stayed2560x1440. The middle native jitter
`[0.34375,-0.48765432834625244]` corresponds to render-pixel SDK jitter
`[0.0859375,0.12191358208656311]` by the verified matrix/viewport mapping. There
was one write and one restoration. Eleven unowned projection calls and33
pre-baseline depth requests were skipped. Preview inspection shows the full
scene, qualitatively aligned depth silhouettes and motion concentrated around
the animated character/weapon; it does not establish motion units/sign.

Full scale was restored after436 overrides. All final hook states were
disabled; fault/pending counters were zero. Pins released: input24, native
consumer3, depth36, submission journal305, final-copy6. All jitter changes were
restored. Successful diagnostic readbacks remained allocated until verified
process exit.

## Limits and next step

This proves **native CPU copy construction and resource association**. The
observer did not track this final packet into its GPU copy, replace output, or
run live NGX. Downstream spatial upscaling remains to be accounted for when
presenting reconstructed full-resolution output. Scaled motion convention,
independent raster jitter fitting and continuous temporal behavior remain
integration checks; parked DLAA beta polish is unchanged.

Proceed to the full-size reconstructed-output handoff, including the final
packet extent and downstream scaling, using these verified identities. Avoid
expanding the native copy over uninitialized texture regions without first
providing full-size output.

Evidence root: `local/scaled-temporal-20260928/v1.23.7/`.
Installed DLL SHA-256:
`e0174e7e258daba2d9ddf4c67ddb6379f654dba56a72bf17be6fd49b5cab7c41`.
Final log SHA-256:
`85d7c21c04266495f558d5dcd56736941ddfc1bb47e1136bdfe09665711f2ab4`.
Verified555-file final archive SHA-256:
`d29d692a0397a3104789d2f95f572758284f3485b8db2e5c01fd9e5e0c7a7970`.
The archive contains the original/frozen log, frozen reviewer, retained input
images, reviews, preview, analysis, deployment receipts and verified exit.
