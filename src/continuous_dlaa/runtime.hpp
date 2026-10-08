#pragma once
#include "../temporal_bindings/inputs.hpp"
#include <filesystem>
#include <iosfwd>
namespace mhw::continuous_dlaa {
bool install(std::uintptr_t,const std::filesystem::path &,std::string &);
// install()'s qualification of the game (renderer discovery, code gates and,
// for the scaled build, the native sizing admission) without side effects.
bool ready(std::uintptr_t,std::string &);
void stop() noexcept;
bool healthy() noexcept;
// Scene and window sizes from discovery (equal unless the aspect-ratio option
// letterboxes or pillarboxes the scene). False before install.
bool scene_extent(unsigned &scene_width,unsigned &scene_height,unsigned &window_width,unsigned &window_height) noexcept;
// Worker-only cleanup receipt. Does not release NGX's process-lived ownership.
bool quiescent() noexcept;
void finish() noexcept;
void save_rejected_shader(const std::filesystem::path &) noexcept;
void save_compute_depth(const std::filesystem::path &prefix) noexcept;
void write(std::ostream &);
}
