# v1.23.3: first complete half-scale temporal input capture

Accepted and archived 2026-09-27. PID 46960, creation 134350141126140646,
host SHA `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`,
DLL SHA `b27776f260b750f952432d656832e55ee07be36f513b956e5266a2fbb3651ae2`.
User confirmed normal restored scene and normal exit. Exact process exit,
unchanged graphics/profile/mode, and preserved installed DLL are verified.

## Confirmed result

Frames 15673–15675 each used a stable 1280x720 active rectangle within 2560x1440
allocations, with native dispatch groups 80x45x1. All nine raw color/depth/motion
images and nine converted images were saved and hash verified. Three native
submission signals and three private conversion signals completed; recording
retirement passed. All 19,353,600 compared active-region float values matched
their source conversion exactly, maximum absolute error zero.

Depth observed 12 native draws. Selected final writers were receipts 2, 5, 8,
using source state 0x80 and DrawInstanced arguments [3,1,3,0]. Each selected
depth copy shared TAA's command/allocator/reset generation, and restored native
RENDER_TARGET state. Color and motion restored 0x40 and 0xc0 respectively.

Color had an explicit local transition in all three captured callbacks. The
new supplementary native-current-use proof was therefore **not attempted**.
The successful capture does not establish that the preceding run's missing
local transition would have passed this proof. That branch has offline GPU and
metadata rejection coverage; its live execution remains unobserved. No need
to repeat this successful capture solely to force that branch before analysing
the captured scaled inputs.

All recorder/activation/scale faults and pending callbacks are zero. Observer
pins released: producer 24/24, native consumer 3/3, depth 36/36, submission
262/262. Submission retention is false. Successfully mapped readbacks and owned
resources follow the existing process-lifetime retention policy; do not call
them released at hook shutdown. Verified normal process exit ends that ownership.
All hooks disabled, activation patch removed, full scale restored after 442
overrides. No saved settings writes, injected jitter or NGX evaluation.

## Image inspection and limits

The active color regions contain the complete scene. Depth silhouettes visibly
correspond to the character/weapon, floor and background structures. Motion is
mostly static in the background and concentrated around animated character/
weapon regions. These qualitative observations are saved alongside numeric
statistics and a three-frame contact sheet. Preview contrast/tone mapping is
for inspection only and does not modify source artifacts.

The motion conversion is still verified allocation-size arithmetic, not a
verified scaled DLSS coordinate convention. No injected jitter was tested here.
The native TAA shader was captured (15,492 bytes, SHA
`2d7b26c742c27db2c83546c6cf6a1dfda47d7cfd4336e69f8c2302de5802b0d1`).
Next analyse that shader and the existing projection/motion findings to separate
active render dimensions from allocated/output dimensions. Establish scaled
motion and jitter units, then adapt the existing SDK creation/evaluation and
handoff for smaller inputs and full-size output. This is implementation work
toward DLSS; the parked DLAA beta backlog remains parked.

## Evidence

Root `local/scaled-temporal-20260927/v1.23.3/`: frozen final JSONL, successful
frozen-tool review, 18 images, shader, image statistics/preview/inspection,
capture-analysis.json, run-result.json, finalizer and 478-file archive.
Final log SHA `bccc863dbb4cf641f6e23ac487c3c969d7603b688bb8c0cb1f52b2dfe45042af`;
archive SHA `af733ab76b8ec26501f06ce9ee869737012211c76a606e20de7c6ae9e4cfa3f7`.
All 447 prelaunch files, 259 frozen/workspace sources and deployment identities
were reverified. Keep v1.23.3 installed. No active capture or next build prepared.
