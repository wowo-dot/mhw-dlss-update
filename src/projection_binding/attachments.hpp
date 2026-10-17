#pragma once
#include <array>
#include <cstdint>
namespace mhw::projection_binding {
struct ViewBinding {
    std::uintptr_t owner{}, backend{}, resource{}, handle{};
    unsigned format{}, rotating{}, rotation_index{};
    bool operator==(const ViewBinding &) const = default;
};
struct NativePipeline {
    std::uintptr_t render{}, graphics{}, shader_handle{}, input_layout{}, input_elements{};
    std::uintptr_t depth_owner{}, depth_state{}, blend_owner{}, blend_state{}, raster_owner{}, raster_state{};
    unsigned input_count{}, graphics_flags{}, group{}, raster_variant{}, dsv_format{};
    std::array<unsigned, 13> depth{};
    std::array<unsigned, 82> blend{};
    std::array<unsigned, 11> raster{};
    bool operator==(const NativePipeline &) const = default;
};
struct AttachmentSnapshot {
    bool valid{};
    std::uintptr_t state{}, command{}, attachment{}, active_attachment{}, incoming_pso{};
    unsigned color_count{}, attachment_flags{}, scissor_enabled{};
    std::array<ViewBinding, 9> views{}; // eight color slots, then depth
    std::array<std::uintptr_t, 9> cached{};
    std::uintptr_t cached_depth_resource{};
    std::array<int, 4> viewport{}, scissor{};
    NativePipeline pipeline{};
    bool operator==(const AttachmentSnapshot &) const = default;
};
// A native draw executor call: its arguments (executor, context, packet) and
// the command list its recording state holds.
struct DrawSite {
    std::uintptr_t executor{}, context{}, packet{}, command{};
};
// Immutable CPU-side draw recipe, without consulting a recording's caches.
bool snapshot_native_pipeline(std::uintptr_t image, std::uintptr_t packet, NativePipeline &,
                              unsigned depth_format = 0) noexcept;
bool snapshot_attachments(std::uintptr_t, const DrawSite &, AttachmentSnapshot &) noexcept;
bool attachment_inputs_equal(const AttachmentSnapshot &, const AttachmentSnapshot &) noexcept;
bool attachment_cache_matches(const AttachmentSnapshot &) noexcept;
bool system_method(std::uintptr_t, unsigned, std::uintptr_t &) noexcept;
} // namespace mhw::projection_binding
