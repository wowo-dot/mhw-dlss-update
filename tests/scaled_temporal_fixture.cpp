// Real WARP capture/submission/conversion fixture with a deterministic scale
// stamp. Actual engine-shaped scale qualification is in render_resolution_tests.
#include "../src/render_resolution/runtime.hpp"
#include "temporal_probe_tests.cpp"
namespace mhw::continuous_dlaa::activation {
// The separate activation contract exercises the real native-shaped frame
// scope. This GPU fixture supplies its deterministic admission signal.
bool owns_projection(std::uintptr_t context,std::uintptr_t view) noexcept {return projection_admission(context,view);}
}
namespace mhw::render_resolution {
bool input_stamp(std::uintptr_t view,InputStamp &s) noexcept {
    if(view!=get<std::uintptr_t>(cfg.renderer+0x1f450))return false;
    s={get<std::uint64_t>(cfg.renderer+0x1f210),view,17,13,8,6,.5F,1.F};return true;
}
}
