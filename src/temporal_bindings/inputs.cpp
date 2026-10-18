#include "inputs.hpp"
#include "../game_memory/layout.hpp"
#include <limits>
namespace mhw::temporal_bindings {
namespace {
bool pointer(std::uintptr_t p, std::uintptr_t size) {
    return p && p <= UINTPTR_MAX - size;
}
bool owner(const Reader &r, std::uintptr_t at, Owner &o) {
    o.object = at;
    if (!pointer(at, 0xf8) || !r.get(at, o.type) || !o.type || !r.get(at + layout::texture_owner::kind, o.kind) ||
        !r.get(at + layout::texture_owner::backing, o.primary) ||
        !r.get(at + layout::texture_owner::backing_override, o.secondary) ||
        !r.get(at + layout::texture_owner::flags, o.flags) || !r.get(at + layout::texture_owner::format, o.format) ||
        !r.get(at + layout::texture_owner::gpu_format, o.gpu_format))
        return false;
    unsigned dimensions{};
    if (!r.get(at + layout::texture_owner::dimensions, dimensions)) return false;
    // Owner+0x88 packs a 16-bit width, 14-bit height and two flag bits.
    // These are engine metadata; actual GPU descriptions still require GetDesc.
    o.width = dimensions & 0xffff;
    o.height = (dimensions >> 16) & 0x3fff;
    o.backing = o.secondary ? o.secondary : o.primary;
    return o.width && o.height && pointer(o.backing, 0x28) &&
           r.get(o.backing + layout::backing::resource, o.resource) && o.resource;
}
} // namespace
bool read_owner(const Reader &r, std::uintptr_t at, Owner &o) {
    o = {};
    return owner(r, at, o);
}
bool scene_depth(const Reader &r, const Snapshot &s, SceneDepth &d) {
    d = {};
    std::uintptr_t object{};
    if (!pointer(s.view, 0x630) || !pointer(s.context, 0x58) || !r.get(s.view + layout::view::depth, d.container) ||
        !pointer(d.container, 0xa8) || !r.get(d.container + layout::depth_container::texture, object) ||
        !owner(r, object, d.texture))
        return false;
    // These addresses describe native ownership only, not the opaque descriptor.
    d.srv_readable =
        r.get(object + layout::texture_owner::srv, d.srv) &&
        (!d.srv || (pointer(d.srv, 0x18) && r.get(d.srv + layout::descriptor_view::cpu_handle, d.cpu_descriptor)));
    // At the earlier depth-copy producer this is the source attachment. Record
    // whether it is still available here; do not infer its lifetime or GPU state.
    Owner attachment_texture;
    d.attachment_readable = r.get(s.context + layout::context::attachment, d.attachment) &&
                            pointer(d.attachment, 0x60) &&
                            r.get(d.attachment + layout::attachment::owner, d.attachment_owner) &&
                            owner(r, d.attachment_owner, attachment_texture);
    if (d.attachment_readable) d.attachment_resource = attachment_texture.resource;
    return true;
}
bool snapshot(const Reader &r, const Config &c, std::uintptr_t layer, std::uintptr_t context, Snapshot &s) {
    s = {};
    s.layer = layer;
    s.context = context;
    std::uintptr_t renderer{}, backend{}, device{}, wrapper{}, type{}, primary_view{}, base_color{}, history{},
        output{}, g1{}, g4{}, prev{};
    unsigned char api{}, async{};
    unsigned sizing{}, context_flags{};
    std::uint16_t technique{};
    if (!pointer(c.image, 0x55d3000) || !pointer(c.renderer, 0x1f570) || !pointer(c.backend, 0x42708) ||
        !pointer(layer, 0x1b0) || context != c.renderer + layout::renderer::context ||
        !r.get(c.image + layout::image::graphics_api, api) || !api ||
        !r.get(c.image + layout::image::renderer, renderer) || renderer != c.renderer ||
        !r.get(renderer + layout::renderer::backend, backend) || backend != c.backend ||
        !r.get(backend + layout::d3d_render::device, device) || device != c.device || !device ||
        !r.get(backend + layout::d3d_render::game_dlss, wrapper) || wrapper ||
        !r.get(renderer + layout::renderer::sizing_flags, sizing) || (sizing & 1) || !r.get(layer, type) ||
        type != c.image + layout::image::taa2_vtable || !r.get(context + layout::context::view, s.view) ||
        !pointer(s.view, 0x628) || !r.get(renderer + layout::renderer::primary_view, primary_view) ||
        s.view != primary_view || !r.get(renderer + layout::renderer::frame, s.frame) || !s.frame ||
        !r.get(renderer + layout::renderer::epoch, s.epoch) || !r.get(renderer + layout::renderer::bank, s.bank) ||
        s.bank > 1 || !r.get(renderer + layout::renderer::parallel_rendering, async) || async > 1 ||
        !r.get(context + layout::context::technique, technique) || !r.get(context + layout::context::state, s.state) ||
        !s.state || !r.get(context + layout::context::flags, context_flags) || (context_flags & 0x54) ||
        !r.get(layer + layout::taa_filter::taa_bytes, s.taa_bytes) ||
        !r.get(s.view + layout::view::gbuffers, s.gbuffer) || !pointer(s.gbuffer, 0xc8))
        return false;
    s.async = async;
    s.technique = technique;
    for (unsigned i = 0; i < 4; ++i)
        if (!r.get(context + layout::context::viewport + 4 * i, s.rectangle[i])) return false;
    if (s.rectangle[0] >= s.rectangle[2] || s.rectangle[1] >= s.rectangle[3] || s.rectangle[2] > 16384 ||
        s.rectangle[3] > 16384)
        return false;
    for (unsigned i = 0; i < roles; ++i) {
        auto &o = s.inputs[i];
        std::uintptr_t object{};
        if (!r.get(c.image + id_rvas[i], o.id) || o.id == UINT_MAX ||
            !r.get(context + layout::context::bindings + 8ULL * (o.id & 0xfff), object) || !owner(r, object, o))
            return false;
        for (unsigned j = 0; j < i; ++j)
            if ((o.id & 0xfff) == (s.inputs[j].id & 0xfff)) return false;
    }
    // Validate the sources actually used at the reviewed TAA call site. Depth
    // may select a fallback; record that binding without inventing its identity.
    if (!r.get(layer + layout::taa_filter::base_color, base_color) ||
        !r.get(layer + layout::taa_filter::history + 8 * ((s.frame - 1) % 3), history) ||
        !r.get(layer + layout::taa_filter::history + 8 * (s.frame % 3), output) ||
        !r.get(s.gbuffer + layout::gbuffers::gbuffer1, g1) || !r.get(s.gbuffer + layout::gbuffers::gbuffer4, g4) ||
        !r.get(s.gbuffer + layout::gbuffers::previous_ids, prev))
        return false;
    return base_color == s.inputs[0].object && history == s.inputs[1].object && output == s.inputs[4].object &&
           g1 == s.inputs[3].object && g4 == s.inputs[5].object && prev == s.inputs[6].object && output != base_color &&
           output != history;
}
bool packet(const Reader &r, std::uintptr_t at, Packet &p) {
    p = {};
    p.object = at;
    unsigned raw{};
    unsigned char mode{};
    if (!pointer(at, 0x30) || !r.get(at, raw) || !r.get(at + layout::dispatch_packet::state, p.state) ||
        !r.get(at + layout::dispatch_packet::groups_x, p.x) || !r.get(at + layout::dispatch_packet::groups_y, p.y) ||
        !r.get(at + layout::dispatch_packet::groups_z, p.z) || !r.get(at + layout::dispatch_packet::mode, mode) ||
        !r.get(at + layout::dispatch_packet::indirect, p.indirect))
        return false;
    p.opcode = raw & 31;
    p.mode = mode;
    return p.opcode == 6 && p.state;
}
bool packet_matches(const Snapshot &s, const Packet &p, unsigned x, unsigned y, unsigned z) {
    return p.opcode == 6 && p.object && p.state == s.state && !p.indirect && !p.mode && p.x == x && p.y == y &&
           p.z == z && z == 1 && x == (s.rectangle[2] - s.rectangle[0] + 15) / 16 &&
           y == (s.rectangle[3] - s.rectangle[1] + 15) / 16;
}
} // namespace mhw::temporal_bindings
