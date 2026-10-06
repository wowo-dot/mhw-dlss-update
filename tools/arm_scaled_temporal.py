"""Signal the versioned scaled temporal recorder for its exact process generation."""
import arm_temporal_inputs as signal
from review_scaled_temporal import parse

# Signaling/receipt implementation is shared; only the admitted experiment
# header changes. These captures deliberately use MhwTemporalInputs-v1.
signal.parse = parse
if __name__ == '__main__':
    signal.main()
