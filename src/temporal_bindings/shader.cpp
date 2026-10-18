#include "shader.hpp"
#include "../game_memory/layout.hpp"
#include <cstring>
namespace mhw::temporal_bindings {
namespace {
bool pointer(std::uintptr_t p, std::uintptr_t n) {
    return p && p <= UINTPTR_MAX - n;
}
} // namespace
bool shader_recipe(const Reader &r, const Snapshot &s, ShaderRecipe &o) {
    o = {};
    o.state = s.state;
    std::uint16_t count{}, start{};
    if (!pointer(o.state, 0x30 + 8 * owner_limit) || !r.get(o.state + layout::binding_state::shader, o.handle) ||
        !pointer(o.handle, 8) || !r.get(o.handle, o.set) || !pointer(o.set, 0x8c) ||
        !r.get(o.set + layout::shader_set::compute_stage, o.entry) || !pointer(o.entry, 0x18) ||
        !r.get(o.entry + layout::shader_stage::code, o.descriptor) || !pointer(o.descriptor, 0x10) ||
        !r.get(o.descriptor, o.size) || o.size < 52 || o.size > shader_limit ||
        !r.get(o.descriptor + layout::shader_code::bytecode, o.bytecode) || !pointer(o.bytecode, o.size) ||
        !r.get(o.state + layout::binding_state::technique, o.technique) || o.technique != s.technique ||
        !r.get(o.set + layout::shader_set::owner_count, o.owner_count) || !o.owner_count ||
        o.owner_count > owner_limit || !r.get(o.set + layout::shader_set::ids, o.ids) ||
        !pointer(o.ids, 4 * o.owner_count) || !r.get(o.state + layout::binding_state::modes, o.modes) ||
        !pointer(o.modes, o.owner_count) || !r.get(o.set + layout::shader_set::binding_count, count) || !count ||
        count > binding_limit || !r.get(o.set + layout::shader_set::binding_start, start) || !r.get(o.set, o.words) ||
        !pointer(o.words, 4ULL * (static_cast<unsigned>(start) + count)))
        return false;
    o.binding_count = count;
    o.binding_start = start;
    for (unsigned i = 0; i < o.owner_count; ++i) {
        auto &v = o.owners[i];
        unsigned char mode{};
        if (!r.get(o.ids + 4ULL * i, v.id) || v.id == UINT_MAX || !r.get(o.modes + i, mode) ||
            !r.get(o.state + layout::binding_state::objects + 8ULL * i, v.object) || !v.object)
            return false;
        v.mode = mode;
        // Names come from the independently observed IDs, never register guesses.
        for (const auto &named : s.inputs)
            if ((named.id & 0xfff) == (v.id & 0xfff) && named.object != v.object) return false;
    }
    for (unsigned i = 0; i < count; ++i) {
        auto &v = o.bindings[i];
        if (!r.get(o.words + 4ULL * (start + i), v.word)) return false;
        const auto type = v.word & 15, index = (v.word >> 4) & 255;
        if (type > 3 || (type && index >= o.owner_count)) return false;
        if (type == 2 || type == 3) {
            const auto object = o.owners[index].object;
            if (!pointer(object, 0x88) || !r.get(object + (type == 2 ? 0x78 : 0x80), v.view) ||
                !pointer(v.view, 0x18) || !r.get(v.view + layout::descriptor_view::cpu_handle, v.cpu_descriptor) ||
                !v.cpu_descriptor)
                return false;
        }
    }
    return true;
}
} // namespace mhw::temporal_bindings
