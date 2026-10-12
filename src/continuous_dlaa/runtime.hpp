#pragma once
#include "../astera/astera_hooks.hpp"
#include <vector>
#include "../temporal_bindings/inputs.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>
#include "../dlss_sdk/geometry.hpp"
namespace mhw::continuous_dlaa {
// Before ready()/install() (alpha: dlss_mode in mhw_dlss.ini): the DLSS preset,
// which also sets the render scale asked of the game. Performance by default.
void reconstruction_mode(dlss_sdk::ReconstructionMode) noexcept;
dlss_sdk::ReconstructionMode reconstruction_mode() noexcept;
// Any thread (the alpha menu): ask for a live preset change; the worker takes
// it with requested_mode() and runs change_mode(). The scaled build only.
void request_mode(dlss_sdk::ReconstructionMode) noexcept;
bool requested_mode(dlss_sdk::ReconstructionMode &) noexcept;  // takes the request
// Whether change_mode() can run now: installed, healthy, scaled build.
bool live_modes() noexcept;
// Worker, live preset change (docs/research/live-presets.md). Frame
// generation must be off with its inputs settled first (FG copies are
// covered by the queue barrier, not by this runtime's own fence).
//  1 pause: no new frames are admitted; the game's own TAA renders them;
//  2 drain: no callback inside our work, no route plan prepared, every
//    injected submission complete;
//  3 queue barrier: our fence, signalled on the game's queue after all it
//    was given, completes; every earlier submission is done on the GPU;
//  4 retarget the render scale; wait until the game renders at it;
//  5 rebuild: retire the DLSS feature and the spatial output, create them at
//    the new extent, resize the stream route, forget published requests and
//    the DLSS history (the next frame resets);
//  6 resume.
// A failure before 4 resumes as before (resumed); from 4 on the runtime
// stops (fail closed: the game's own rendering at its own scale returns).
struct ModeChange {
    bool ok{}, resumed{}, unchanged{};
    unsigned phase{};  // the last phase reached (above)
    std::string reason;
    std::array<std::uint64_t, 7> us{};  // microseconds from the start at the end of each phase
    std::uint64_t barrier{};  // the queue barrier's fence value
    dlss_sdk::ReconstructionMode mode{};  // the mode DLSS runs afterwards
    unsigned render_width{}, render_height{};
};
ModeChange change_mode(dlss_sdk::ReconstructionMode) noexcept;
bool install(std::uintptr_t,const std::filesystem::path &,std::string &);
// install()'s qualification of the game (renderer discovery, code gates and,
// for the scaled build, the native sizing admission) without side effects.
bool ready(std::uintptr_t,std::string &);
void stop() noexcept;
bool healthy() noexcept;
// Scene and window sizes from discovery (equal unless the aspect-ratio option
// letterboxes or pillarboxes the scene). False before install.
bool scene_extent(unsigned &scene_width,unsigned &scene_height,unsigned &window_width,unsigned &window_height) noexcept;
// DLSS's render size (half the scene at Performance in the scaled build). False before install.
bool render_extent(unsigned &width,unsigned &height) noexcept;
// Worker-only cleanup receipt. Does not release NGX's process-lived ownership.
bool quiescent() noexcept;
void finish() noexcept;
// AL: the runtime's D3D12 detours with the storage AL fills (hooks 5..).
void astera_hooks(std::vector<astera::Hook>&);
void save_rejected_shader(const std::filesystem::path &) noexcept;
void save_compute_depth(const std::filesystem::path &prefix) noexcept;
void save_inserted_effect(const std::filesystem::path &prefix) noexcept;
void write(std::ostream &);
// Counters only (frames, DLSS history resets by reason) for the alpha's minute log.
void write_summary(std::ostream &);
}
