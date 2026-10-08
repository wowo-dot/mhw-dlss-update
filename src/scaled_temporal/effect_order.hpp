#pragma once
namespace mhw::scaled_temporal {
// Stage of a plan's ordinal-th effect before the plan is ordered (a built plan
// always is; only the readiness trace reads an unbuilt one): TAA output, haze
// (3), grade, final, then overlays from stage 4. A build that fails after it
// counted haze and overlays leaves exactly that state.
constexpr unsigned unordered_effect_stage(bool haze,unsigned ordinal){
    constexpr unsigned haze_order[]{0,3,1,2};
    if(haze)return ordinal<4?haze_order[ordinal]:ordinal;
    return ordinal<3?ordinal:ordinal+1;  // no stage 3 without haze
}
}
