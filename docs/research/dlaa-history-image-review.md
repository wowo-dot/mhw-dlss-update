# Retained-history image review — 2026-09-25

Input: the accepted [v1.14 live capture](first-live-dlaa-history.md), PID 49200,
frames 23398–23400. Its sealed archive remains unchanged. Analysis is under
`local/dlaa-history-analysis-20260925/v1/`.

The full scene remains coherent in all three private DLAA outputs. Fixed-scale
previews and nearest-neighbor enlarged rail/character crops show smoother edges
and some texture softening. No obvious broad corruption was visible in these
crops. This is a limited visual observation, not a temporal-quality pass.
All previews use the same nonnegative Reinhard mapping followed by sRGB;
there is no per-image normalization. A few high input peaks are substantially
reduced by reconstruction; the review does not establish highlight fidelity.

A real isolated-device reset replay used the exact three captured input sets
with the frozen v1.14 fixture. GPU validation reported no errors or warnings.
Frame zero was byte-identical to its live counterpart. History outputs differ
from reset replay on later frames (RGB mean absolute differences approximately
0.0016781 and 0.0011931). The middle-frame comparison includes both retained
history and SDK jitter differences; it does not isolate the effect of either.

The view and unjittered view-projection matrices were bit-identical across the
three captures. A strict image-alignment mask required exact baseline/recovery
color agreement, zero motion in all three frames and stable neighborhoods.
Only 1,579 pixels survived across the chosen static regions before the gradient
filter. Each attempted fit had fewer than the required 1,000 usable pixels, so
all 24 fits were rejected. No subpixel compensation estimate is claimed and the
threshold was not weakened.

The next bounded diagnostic extends accumulation to nine adjacent frames,
including seven distinct native jitter samples. It preserves independent
projection/input association, scoped restoration and private offscreen output.
Longer accumulation and eventual controlled presentation remain necessary to
judge stability, sharpness, shimmer and ghosting in actual gameplay.
