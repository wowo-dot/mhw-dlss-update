#include "memory.hpp"
#include "../game_memory/layout.hpp"
#include <windows.h>
#include <d3d12.h>
namespace mhw::projection_binding {
namespace {
bool address(std::uintptr_t p, unsigned n) noexcept {
    return p && p <= UINTPTR_MAX - n;
}
template <class T> bool field(std::uintptr_t p, unsigned off, T &v) noexcept {
    return address(p, off + sizeof(T)) && copy_memory(p + off, &v, sizeof(T));
}
bool view(std::uintptr_t p, bool depth, ViewBinding &v) noexcept {
    v.owner = p;
    if (!p) return true;
    if (!field(p, layout::texture_owner::backing, v.backend) || !v.backend ||
        !field(v.backend, layout::target_backing::handle, v.handle) || !v.handle ||
        !field(v.backend, layout::target_backing::resource, v.resource) || !v.resource)
        return false;
    if (depth) return field(p, layout::texture_owner::depth_format, v.format);
    unsigned char rotation{};
    if (!field(v.backend, layout::target_backing::format, v.format) ||
        !field(v.backend, layout::target_backing::rotation, rotation))
        return false;
    v.rotating = rotation;
    // No length for the alternative handle table is established. Preserve the
    // discriminator and fail closed instead of guessing a rotation bound.
    if (rotation) {
        field(v.backend, layout::target_backing::rotation_index, v.rotation_index);
        return false;
    }
    return true;
}
bool native_pipeline(std::uintptr_t image, std::uintptr_t packet, NativePipeline &p, unsigned depth_format) noexcept {
    if (!field(packet, layout::draw_packet::graphics, p.graphics) ||
        !field(packet, layout::draw_packet::render_state, p.render) ||
        !field(p.graphics, layout::binding_state::flags, p.graphics_flags) ||
        !field(p.graphics, layout::binding_state::shader, p.shader_handle) ||
        !field(p.graphics, layout::binding_state::technique, p.group) ||
        !field(p.graphics, layout::binding_state::input_layout, p.input_layout))
        return false;
    if (p.input_layout && (!field(p.input_layout, layout::input_layout::elements, p.input_elements) ||
                           !field(p.input_layout, layout::input_layout::count, p.input_count) || p.input_count > 32 ||
                           (p.input_count && !p.input_elements)))
        return false;
    unsigned char variant{};
    if (!field(p.render, layout::render_state::variant, variant) ||
        !field(p.render, layout::render_state::raster_owner, p.raster_owner) ||
        !field(p.render, layout::render_state::depth_owner, p.depth_owner) ||
        !field(p.render, layout::render_state::blend_owner, p.blend_owner))
        return false;
    p.raster_variant = variant & 1;
    p.dsv_format = depth_format == 53 ? 55 : depth_format == 44 ? 45 : depth_format == 39 ? 40 : depth_format;
    if (!depth_format) {
        std::uintptr_t shader{}, owner{};
        if (!field(image, layout::image::shader, shader) || !field(shader, layout::shader::depth_owner, owner) ||
            !field(owner, 0, p.depth_owner))
            return false;
    }
    if (!field(p.depth_owner, layout::depth_owner::state, p.depth_state) ||
        !field(p.blend_owner, layout::blend_owner::state, p.blend_state) ||
        !field(p.raster_owner, layout::raster_owner::states + 8 * p.raster_variant, p.raster_state))
        return false;
    return field(p.depth_state, layout::pipeline_state::desc, p.depth) &&
           field(p.blend_state, layout::pipeline_state::desc, p.blend) &&
           field(p.raster_state, layout::pipeline_state::desc, p.raster);
}
} // namespace
bool snapshot_native_pipeline(std::uintptr_t image, std::uintptr_t packet, NativePipeline &p,
                              unsigned depth_format) noexcept {
    p = {};
    return native_pipeline(image, packet, p, depth_format);
}
bool snapshot_attachments(std::uintptr_t image, const DrawSite &d, AttachmentSnapshot &s) noexcept {
    s = {};
    if (!field(d.executor, layout::executor::recording, s.state) ||
        !field(s.state, layout::recording::command, s.command) || s.command != d.command ||
        !field(d.packet, layout::draw_packet::bindings, s.attachment) ||
        !field(d.packet, layout::draw_packet::pipeline, s.incoming_pso) ||
        !field(d.context, layout::context::active_targets, s.active_attachment) ||
        !field(s.attachment, layout::target_set::flags, s.attachment_flags))
        return false;
    s.color_count = s.attachment_flags & 7;
    for (unsigned i = 0; i < s.color_count; ++i) {
        std::uintptr_t p{};
        if (!field(s.attachment, layout::target_set::colors + 8 * i, p) || !view(p, false, s.views[i])) return false;
    }
    std::uintptr_t p{};
    if (!field(s.attachment, layout::target_set::depth, p) || !view(p, true, s.views[8]) ||
        !field(s.state, layout::recording::cached_pipeline, s.cached) ||
        !field(s.state, layout::recording::cached_depth_resource, s.cached_depth_resource) ||
        !field(s.state, layout::recording::viewport, s.viewport) ||
        !field(s.state, layout::recording::scissor, s.scissor))
        return false;
    unsigned char enabled{};
    if (!field(s.state, layout::recording::scissor_enabled, enabled) || enabled > 1) return false;
    s.scissor_enabled = enabled;
    if (!native_pipeline(image, d.packet, s.pipeline, s.views[8].format)) return false;
    s.valid = true;
    return true;
}
bool attachment_inputs_equal(const AttachmentSnapshot &a, const AttachmentSnapshot &b) noexcept {
    return a.valid && b.valid && a.state == b.state && a.command == b.command && a.attachment == b.attachment &&
           a.incoming_pso == b.incoming_pso && a.attachment_flags == b.attachment_flags &&
           a.color_count == b.color_count && a.views == b.views && a.pipeline == b.pipeline;
}
bool attachment_cache_matches(const AttachmentSnapshot &s) noexcept {
    if (!s.valid || s.active_attachment != s.attachment) return false;
    for (unsigned i = 0; i < 9; ++i)
        if (s.views[i].handle != s.cached[i]) return false;
    return !s.views[8].owner || s.cached_depth_resource == s.views[8].resource;
}
} // namespace mhw::projection_binding
