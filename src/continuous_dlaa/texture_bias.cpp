#include "texture_bias.hpp"
#include "../game_memory/layout.hpp"
#include "../projection_binding/memory.hpp"
#include <atomic>
#include <ostream>

namespace mhw::continuous_dlaa::texture_bias {
namespace pb = projection_binding;
namespace {
std::atomic<std::uintptr_t> image{};
}

void install(const std::uintptr_t base) noexcept {
    image.store(base);
}

void write(std::ostream &out) {
    const auto base = image.load();
    // The resident sampler table cache (cD3DRender +0x1370): tables made and
    // descriptor slots used, of its capacity.
    std::uintptr_t backend{}, cache{};
    std::uint32_t tables{}, slots{}, capacity{};
    const bool cache_read = base && pb::read(base + layout::image::d3d_render, backend) && backend &&
                            pb::read(backend + layout::d3d_render::sampler_tables, cache) && cache &&
                            pb::read(cache + layout::sampler_table_cache::count, tables) &&
                            pb::read(cache + layout::sampler_table_cache::next, slots) &&
                            pb::read(cache + layout::sampler_table_cache::capacity, capacity);
    // The bank materials bake at load (0 as shipped) and the neighbouring
    // default bias.
    std::uintptr_t scene{};
    std::uint32_t bank{};
    float default_bias{};
    const bool scene_read = base && pb::read(base + layout::image::scene, scene) && scene &&
                            pb::read(scene + layout::scene::sampler_lod_bias, bank) &&
                            pb::read(scene + layout::scene::default_bias, default_bias);
    out << "{\"cache\":";
    if (cache_read)
        out << "{\"tables\":" << tables << ",\"slots\":" << slots << ",\"capacity\":" << capacity << '}';
    else
        out << "null";
    out << ",\"material_bank\":";
    if (scene_read)
        out << bank << ",\"default_bias\":" << default_bias;
    else
        out << "null";
    // Each bank's first template bias and MaxLOD (bank 0 none, 1-7 below,
    // 8-14 above), null for a bank that doesn't read.
    out << ",\"banks\":[";
    std::uintptr_t shader{};
    const bool shader_read = base && pb::read(base + layout::image::shader, shader) && shader;
    for (unsigned i = 0; i < layout::shader::sampler_bank_count; ++i) {
        std::uintptr_t states{}, state{};
        float bias{}, max_lod{};
        if (i) out << ',';
        if (shader_read && pb::read(shader + layout::shader::sampler_banks + 8 * i, states) && states &&
            pb::read(states, state) && state && pb::read(state + layout::sampler_state::mip_bias, bias) &&
            pb::read(state + layout::sampler_state::max_lod, max_lod))
            out << '[' << bias << ',' << max_lod << ']';
        else
            out << "null";
    }
    out << "]}";
}
} // namespace mhw::continuous_dlaa::texture_bias
