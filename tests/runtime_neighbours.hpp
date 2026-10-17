#pragma once
// Test doubles for the modules the DLSS runtime (continuous_dlaa/runtime.cpp)
// calls: render resolution, the stream route, the game's AA options, the FG
// session and the scaled resolve. Each behaves as its module before install
// (nothing ready, healthy, idle) unless a fixture sets the controls below.
#include "../src/render_resolution/runtime.hpp"
#include <cstdint>

namespace mhw::test::neighbours {
// render_resolution::active_ready(): the override applied for 32 visits.
inline bool resolution_active{};
// render_resolution::input_stamp(): the stamp returned for `view`, if set.
inline bool stamp_valid{};
inline render_resolution::InputStamp stamp{};
}
