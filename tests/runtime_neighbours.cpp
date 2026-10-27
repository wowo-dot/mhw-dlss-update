#include "runtime_neighbours.hpp"
#include "../src/scaled_temporal/stream_route.hpp"
#include "../src/scaled_temporal/resolve.hpp"
#include "../src/continuous_dlaa/game_options.hpp"
#include "../src/frame_generation/game_session.hpp"
#include "../src/sharpen/sharpen.hpp"
#include <string>

namespace nb = mhw::test::neighbours;

namespace mhw::render_resolution {
bool install(std::uintptr_t, float, std::string &reason, bool) {
    reason = "render resolution test double";
    return false;
}
bool ready(std::uintptr_t, float, std::string &reason) {
    reason = "render resolution test double";
    return false;
}
void observe_viewports(ID3D12GraphicsCommandList *, unsigned, const D3D12_VIEWPORT *, std::uintptr_t) noexcept {}
bool active_ready() noexcept {
    return nb::resolution_active;
}
bool retarget(float, std::string &reason) {
    reason = "render resolution test double";
    return false;
}
std::uint64_t applied_visits() noexcept {
    return 0;
}
// A rendering game: each look sees new visits.
std::uint64_t full_content_visits() noexcept {
    static std::uint64_t visits{};
    return visits += 2;
}
std::uint64_t restorations() noexcept {
    static std::uint64_t visits{};
    return ++visits;
}
bool scene_changed() noexcept {
    return false;
}
bool rebase(Extent, unsigned, float, std::string &reason) {
    reason = "render resolution test double";
    return false;
}
bool input_stamp(std::uintptr_t view, InputStamp &out) noexcept {
    if (!nb::stamp_valid || nb::stamp.view != view) return false;
    out = nb::stamp;
    return true;
}
void yield() noexcept {}
bool released() noexcept {
    return true;
}
bool full_scale(std::uintptr_t) noexcept {
    return false;
}
bool resume(float, std::string &reason) {
    reason = "render resolution test double";
    return false;
}
void stop() noexcept {}
bool healthy() noexcept {
    return true;
}
bool finish() {
    return true;
}
void write(std::ostream &) {}
} // namespace mhw::render_resolution

namespace mhw::scaled_temporal::stream_route {
namespace tb = temporal_bindings;
void observe_final_scene(FinalSceneObserver) noexcept {}
bool prepare(const tb::Config &, unsigned, unsigned, unsigned, unsigned, std::string &reason) {
    reason = "stream route test double";
    return false;
}
void begin() noexcept {}
bool copy_region(ID3D12GraphicsCommandList *, const D3D12_TEXTURE_COPY_LOCATION *, UINT, UINT, UINT,
                 const D3D12_TEXTURE_COPY_LOCATION *, const D3D12_BOX *) noexcept {
    return false;
}
void publish(const tb::Snapshot &, const tb::Packet &) noexcept {}
void post_queue_complete(std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t) noexcept {}
void full_post_enter(std::uintptr_t, bool) noexcept {}
void full_post_taa(std::uintptr_t) noexcept {}
void full_post_exit(std::uintptr_t) noexcept {}
void full_post_states(const tb::Snapshot &, unsigned, std::uintptr_t, unsigned, std::uintptr_t) noexcept {}
bool ready(ID3D12GraphicsCommandList *, std::uint64_t, const tb::Snapshot &, std::uintptr_t) noexcept {
    return false;
}
bool full_contract(const tb::Snapshot &) noexcept {
    return false;
}
bool owns_packet(std::uintptr_t) noexcept {
    return false;
}
bool observe_resolve(std::uintptr_t, ID3D12GraphicsCommandList *, std::uint64_t) noexcept {
    return false;
}
void finish_resolve(std::uintptr_t, bool) noexcept {}
void resolved(bool, bool) noexcept {}
bool submission_needed(std::span<const std::array<std::uint64_t, 2>>) noexcept {
    return false;
}
bool validate_submission(std::uint64_t, std::span<const std::array<std::uint64_t, 2>>, std::uint64_t) noexcept {
    return true;
}
void reject_submission(std::span<const std::array<std::uint64_t, 2>>) noexcept {}
bool submitted_batch(std::uint64_t, std::span<const std::array<std::uint64_t, 2>>, std::uint64_t) noexcept {
    return true;
}
void observe_submission(std::uint64_t, std::uint64_t, std::span<const std::array<std::uint64_t, 2>>) noexcept {}
void barriers(ID3D12GraphicsCommandList *, unsigned, const D3D12_RESOURCE_BARRIER *) noexcept {}
bool indexed_draw(ID3D12GraphicsCommandList *, unsigned, unsigned, unsigned, int, unsigned, std::uintptr_t) noexcept {
    return false;
}
bool reset(ID3D12GraphicsCommandList *, std::uint64_t) noexcept {
    return true;
}
void collect(std::uint64_t) noexcept {}
bool drained() noexcept {
    return true;
}
bool callbacks_idle() noexcept {
    return true;
}
bool abandon() noexcept {
    return true;
}
void stop() noexcept {}
bool idle() noexcept {
    return true;
}
bool resize(unsigned, unsigned, unsigned, unsigned, std::string &reason) noexcept {
    reason = "stream route test double";
    return false;
}
bool unhook() noexcept {
    return true;
}
void disabled(bool) noexcept {}
void chains_live(bool) noexcept {}
bool healthy() noexcept {
    return true;
}
void write(std::ostream &) {}
void write_summary(std::ostream &) {}
} // namespace mhw::scaled_temporal::stream_route

namespace mhw::scaled_temporal {
continuous_dlaa::ResolveResult record_resolve(dlss_sdk::InlineFrame &, temporal_present::Output &,
                                              ID3D12GraphicsCommandList *, ID3D12Resource *,
                                              const temporal_present::Bindings &, dlss_sdk::HistoryFrame, bool, float,
                                              bool) noexcept {
    return {};
}
} // namespace mhw::scaled_temporal

namespace mhw::continuous_dlaa::game_options {
bool install(std::uintptr_t, std::string &reason) {
    reason = "game options test double";
    return false;
}
bool cas_handled() noexcept {
    return false;
}
void take_over() noexcept {}
void give_back() noexcept {}
bool taken() noexcept {
    return false;
}
void write(std::ostream &) {}
} // namespace mhw::continuous_dlaa::game_options

namespace mhw::continuous_dlaa::texture_bias {
void observe(std::uintptr_t) noexcept {}
void write(std::ostream &) {}
} // namespace mhw::continuous_dlaa::texture_bias

namespace mhw::frame_generation::game {
void stage(ID3D12GraphicsCommandList *, std::uint64_t, std::uint64_t, ID3D12Resource *, ID3D12Resource *,
           const SlotCamera &) noexcept {}
void record(ID3D12GraphicsCommandList *, const temporal_present::Bindings &, ID3D12Resource *, std::uint64_t,
            std::uint64_t, D3D12_RESOURCE_STATES) noexcept {}
void closing(ID3D12GraphicsCommandList *, const temporal_present::Bindings &) noexcept {}
bool capture_wanted() noexcept {
    return false;
}
void submitted(std::uint64_t, std::uint64_t, std::uint64_t) noexcept {}
void reset(std::uint64_t, std::uint64_t) noexcept {}
} // namespace mhw::frame_generation::game

// NVSharpen stays off in the runtime's tests (tests/sharpen_pass_warp.cpp has it).
namespace mhw::sharpen {
void percent(unsigned) noexcept {}
unsigned percent() noexcept {
    return 0;
}
bool prepare(ID3D12Device *, std::string &) noexcept {
    return true;
}
void reset() noexcept {}
void record(ID3D12GraphicsCommandList *, const temporal_present::Bindings &, ID3D12Resource *, D3D12_RESOURCE_STATES,
            Hdr) noexcept {}
void write(std::ostream &) {}
} // namespace mhw::sharpen
