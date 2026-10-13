# Depth of field before the motion filter, and HDR (2026-10-13)

Status at the end of the day's session. Run folders `game-103` to `game-105`
(private, `local/fg-game-20261006`).

## Depth of field in the hub and at the title

**Finding (game-103, game-101 title).** With the game's Depth of Field option
on, the hub and the title put `uNewDOFFilter` (vtable RVA `0x3526650`) at
**order 16**: after the game's TAA (order 2) and before the motion filter
(`uMotionBlurReconstructionFilter`, order 100).

| Order | Filter |
|---|---|
| 0 | uToneMapFilter |
| 2 | the game's TAA (uTemporalAAFilter2, `0x351bda8`) |
| 16 | uNewDOFFilter |
| 100 | the motion filter |
| 1024 | uGodRaysFilter (`0x3536c98`), title only |
| 0x7fffff1 | the endpoint |

- **In the field** (the training grounds) the depth of field comes after the
  motion filter, as a middle-chain link, where the route already admitted its
  six compute passes and composition.
- **The route took any filter between TAA and motion as a single-draw
  pre-motion screen filter** (the depth-colour slot). The depth of field's
  first compute dispatch hit the overlay command gate (plan rejection 17,
  branch 7 unsupported opcode, key 0x78000010), and every such frame fell
  back to the spatial path.
- **Frames lost:** 1,482 title frames in perf12 (game-101), and 2,202 and
  2,457 frames in the HDR test (game-103).
- **Why it never showed before:** the user's earlier runs had depth of field
  off. Depth of field is likely on for most players.

**v0.24.5:** the pre-motion slot takes the depth of field.
- It reads the six dispatches as in the chain (`dof_read`), then the
  composition draw: kind 3, PS `9f04832c…`, with its scene source (t2) the
  TAA output.
- Dependency discovery admits kind 3 there only when it closes that
  sequence, and requires its scene read to be the TAA copy (resource 1) that
  DLSS fills at full size.
- Qualification and recording are the existing six-pass path.

**game-104 (v0.24.5):**
- **Hub, after loading: fixed.** The depth of field qualified on 2,209 frames
  (six passes each, 0 failures, 0 rejections). DLSS ran 3,443 frames and
  frame generation tagged 3,442 with 0 errors.
- **Title screen: still refused** (898 frames, the same gate at a
  depth-of-field dispatch). The title's depth of field differs in some way:
  a seventh pass, an unreadable dispatch, or a second sequence.

**v0.24.6** (diagnostic only) records the reason in the gate's `seen` word:
0x100 * why + passes read.
- **why 1:** a dispatch after the composition;
- **why 2:** another sequence's key;
- **why 3:** a seventh dispatch;
- **why 4:** an unreadable dispatch.

**game-105 (v0.24.6) did not answer it.** The worker stopped with
`driver-On-not-enabled` before any frame: the mod armed after 3.1 s, before
the game's first frame, and the driver reported Reflex low latency off at the
first heartbeat. The user attributes it to a corrupted NVIDIA cache or driver
state, a known MHW issue, and will do a clean driver reinstall. See
`game-105/NOTES.md`.

**Next:**
1. After the driver reinstall, a title-only run on v0.24.6. Read
   `route.latest_rejection.command_gate[6]`.
2. Fix the title's variant, then confirm the hub depth of field visually:
   the blur's size and position.
3. Check whether arming before the game's first frame makes the
   `driver-On-not-enabled` check a race of its own. Should it wait for
   presented frames?

## HDR (game-103, v0.24.4)

The user turned on Windows HDR and the game's HDR mode (`ScreenMode=HDRMode`).
- **The swap chain:** the game creates it as B8G8R8A8 and resizes it to
  **R10G10B10A2 (format 24, HDR10)**, switching back and forth on mode
  changes. The mod followed every resize; there were no faults.
- **DLSS** ran whenever its plan qualified (628 frames). It works on the
  linear scene before tone mapping. Most frames fell back to the
  depth-of-field rejection above, not because of HDR.
- **Frame generation** armed and presented generated frames: 620 presents
  with a generated frame, 0 SDK errors.
- **The HUD-less image:** its CBSystem brightness capture never matched in HDR
  (626 fallbacks, 0 GPU matches). The game's HDR screen write is not the SDR
  one the HUD-less pass copies (brightness curve, sRGB encode, R8G8B8A8). So
  the HUD-less image was SDR-encoded for an HDR10 back buffer. **The user saw
  the HUD looking fine**, but the colour space is wrong in principle.
- **Next (later):** reverse the game's HDR screen write (PQ encode and its
  brightness parameters) and give the HUD-less pass an HDR path. Until then,
  consider failing closed: no HUD-less image while the back buffer is not
  8-bit SDR.

## Also open from the day

- Two single-frame rejections with a radial blur (order 4100) and a filter
  at order 26005 (perf12).
- The plan build's lock time (`route_queue`, 0.15-0.2 ms a frame), if a CPU
  with fewer cores shows it (performance.md, perf12).
