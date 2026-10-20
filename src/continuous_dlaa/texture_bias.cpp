#include "texture_bias.hpp"
#include "../game_memory/layout.hpp"
#include "../astera/astera_hooks.hpp"
#include "../projection_binding/memory.hpp"
#include <windows.h>
#include <d3d12.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <ostream>
#include <string>

namespace mhw::continuous_dlaa::texture_bias {
namespace {
namespace pb = projection_binding;
// 0x263daf0: a sampler state's native sampler at the game's anisotropy level.
// Its callers (the table builders 0x259f070, 0x259f330, 0x259f610 and
// SetShaders' 0x259a490) read only the native's CPU handle (+0x10).
using Pick = std::uintptr_t (*)(std::uintptr_t);
constexpr const char *pick_pattern = "40 53 48 83 EC 20 48 8B D9 48 8B 0D 80 69 B8 02";
static_assert(astera::valid_pattern<16>(pick_pattern), "0x263daf0's first 16 bytes");
constexpr std::uintptr_t native_handle = 0x10; // a native sampler's CPU descriptor handle (0x24e6d40)

std::atomic<std::uintptr_t> image{};
// The live bias, set before the hook goes in and then only read.
bool live{};
std::string refusal_text;
unsigned attempts{};
Pick pick_original{};
astera::OwnedHook hook{};
std::array<const std::uintptr_t *, bank_count> banks{}; // sShader's arrays, alive while the game runs
const std::uintptr_t *materials{};                      // banks[material_bank]
// The bank material picks take, and per bank the anisotropy levels it has a
// table set at (bit per level).
std::atomic<unsigned> active{};
std::array<std::atomic<std::uint32_t>, bank_count> used{};
std::atomic<bool> room{};
std::atomic<std::uint32_t> material_level{};
std::atomic<std::uint64_t> fallbacks{};
// The worker's (follow()): the bank asked for, the table set cost measured
// over the 2 s after new sets appear, and the cache.
unsigned wanted{}, changes{};
float wanted_scale{};
std::array<std::uint32_t, bank_count> seen{};
std::uint32_t measured_cost{}, window_slots{}, window_sets{}, previous_slots{}, slots_now{}, capacity_now{};
std::uint64_t window_end{};

std::uintptr_t pick(std::uintptr_t state) noexcept {
    const auto id = *reinterpret_cast<const std::uint32_t *>(state + layout::sampler_state::id);
    if (id >= states_per_bank || materials[id] != state) return pick_original(state);
    const unsigned bank = active.load(std::memory_order_relaxed);
    const std::uintptr_t chosen = banks[bank][id];
    std::uintptr_t native = pick_original(chosen);
    const auto *variants = reinterpret_cast<const std::uintptr_t *>(chosen + layout::sampler_state::native);
    unsigned level = 0;
    for (unsigned v = 0; v < levels; ++v)
        if (variants[v] == native) {
            level = v;
            break;
        }
    // The player's anisotropy level, as the samplers with variants show it
    // (one without any stays at level 0 whatever the option).
    if (level && material_level.load(std::memory_order_relaxed) != level)
        material_level.store(level, std::memory_order_relaxed);
    const auto bit = 1U << level;
    unsigned set_bank = bank;
    if (!(used[bank].load(std::memory_order_relaxed) & bit)) {
        if (bank && !room.load(std::memory_order_relaxed)) {
            // No room for this bank's set here: the game's own (its set is in
            // the cache's reserve).
            fallbacks.fetch_add(1, std::memory_order_relaxed);
            native = pick_original(banks[0][id]);
            set_bank = 0;
        }
        if (!(used[set_bank].load(std::memory_order_relaxed) & bit))
            used[set_bank].fetch_or(bit, std::memory_order_relaxed);
    }
    return native;
}

struct Gate {
    unsigned rva, bytes;
    const char *sha;
};
bool gates_pass(std::uintptr_t host) noexcept {
    // The bank builder, the state and its native sampler, the material
    // resolve (bank[sScene +0x1a8]), the generic bank setter (every caller
    // passes bank 0), the four callers of the pick, and the table cache.
    constexpr Gate gates[]{{0x23d6690, 3945, "4fcaad2721504f3d3ec57b1e6dcfc38e638da4bbcfd9233445e719939889c9f6"},
                           {0x263d890, 316, "ba29c1f8acfe13f35281f6c9df923dfdd0109a858306081c5311e3ea57bc7096"},
                           {0x24e6d40, 265, "b9509f83d5377de3ea72bcb7314da0e50b1d7c0f2588c7076d51f41ba0fc6cc6"},
                           {0x2383400, 854, "c72c62201206bbc558669a138feea004ea56a46ae2d8dcc43e3a178429ffc45b"},
                           {0x23cfec0, 106, "54b1c5f3783efa69e4f0a4320a2fa46e4ff48458c67b9128b1f4eb3a6aecb1f2"},
                           {0x259f070, 702, "02ecc05d095347092ea8c350814f9365199632946c976f7b633d50f5d59a11ad"},
                           {0x259f330, 736, "bdb718afb5e58ba071b856c4de0c5683e927e62c6a88f57ff5ab67ac41bb23bc"},
                           {0x259f610, 924, "936ac61d6575029672cd657bc8956e63f00d8ac50df2ede414d8c93dcf886bb5"},
                           {0x259a490, 1978, "55fb86429f59cc6e07f18ec214c3a4e1c9a8f218ddb7d14dd3a2d64b7655bc7e"},
                           {0x23a9200, 335, "14b396e91a0589003dd42149e088a9ba736793ef0bd174c5e9b74b8e9ba440e1"},
                           {0x2624870, 312, "9f96e702858b0630aae7560b039f8b27f4ad8ec6b0575772e527cb59201aa19a"},
                           {0x23a2750, 145, "8a1676fe4edd6d8d6380f83cbba6d77b30945fbec0de76f4654139e5b457bf49"}};
    for (const auto &g : gates)
        if (!pb::verify_code(host, g.rva, g.bytes, g.sha)) {
            refusal_text = "texture bias code gate " + std::to_string(g.rva);
            return false;
        }
    // The hooked pick: its original head with the live rest (a shared chain).
    std::array<std::uint8_t, 16> head{};
    if (!astera::pattern_bytes<16>(pick_pattern, head) ||
        !pb::verify_code_with_head(host, 0x263daf0, 103,
                                   "04691a793a505d1ea1e8b98e1308514b7278209a31dfe5692247437d439c0852", head.data(),
                                   16)) {
        refusal_text = "texture bias code gate " + std::to_string(0x263daf0);
        return false;
    }
    return true;
}

template <class T> bool write_value(std::uintptr_t at, T value) noexcept {
    __try {
        *reinterpret_cast<volatile T *>(at) = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// The banks as the game built them: every state where it belongs, the same
// template as bank 0's but for the bank's bias, and the same anisotropy
// variants. false: not built yet (retry) or not as tested (`bad`).
bool banks_as_built(std::uintptr_t shader, bool &bad) noexcept {
    bad = false;
    std::array<std::uintptr_t, bank_count> arrays{};
    for (unsigned b = 0; b < bank_count; ++b)
        if (!pb::read(shader + layout::shader::sampler_banks + 8 * b, arrays[b]) || !arrays[b]) return false;
    for (unsigned i = 0; i < states_per_bank; ++i) {
        std::uintptr_t base{};
        std::uint32_t packed0{};
        float bias0{}, max0{};
        std::array<std::uintptr_t, levels> natives0{};
        if (!pb::read(arrays[0] + 8 * i, base) || !base ||
            !pb::read(base + layout::sampler_state::sampler_template, packed0) ||
            !pb::read(base + layout::sampler_state::mip_bias, bias0) ||
            !pb::read(base + layout::sampler_state::max_lod, max0))
            return false;
        for (unsigned v = 0; v < levels; ++v)
            if (!pb::read(base + layout::sampler_state::native + 8 * v, natives0[v])) return false;
        if (!natives0[0]) return false;
        for (unsigned b = 0; b < bank_count; ++b) {
            std::uintptr_t state{};
            std::uint32_t id{}, packed{};
            float bias{};
            if (!pb::read(arrays[b] + 8 * i, state) || !state || !pb::read(state + layout::sampler_state::id, id) ||
                !pb::read(state + layout::sampler_state::sampler_template, packed) ||
                !pb::read(state + layout::sampler_state::mip_bias, bias))
                return false;
            const float expected = b ? std::min(bias0 + bank_bias(b), max0) : bias0;
            if (id != i || packed != packed0 || !(std::fabs(bias - expected) <= 1e-3F)) {
                bad = true;
                return false;
            }
            for (unsigned v = 0; v < levels; ++v) {
                std::uintptr_t native{}, handle{};
                if (!pb::read(state + layout::sampler_state::native + 8 * v, native)) return false;
                if ((native != 0) != (natives0[v] != 0) ||
                    (native && (!pb::read(native + native_handle, handle) || !handle))) {
                    bad = true;
                    return false;
                }
            }
        }
    }
    for (unsigned b = 0; b < bank_count; ++b) banks[b] = reinterpret_cast<const std::uintptr_t *>(arrays[b]);
    return true;
}

// The material bank renders as bank 0: each native's descriptor copied from
// bank 0's (CPU-only heaps; nothing has bound this bank yet).
bool copy_bank_zero(ID3D12Device *device) noexcept {
    __try {
        for (unsigned i = 0; i < states_per_bank; ++i) {
            const auto *from = reinterpret_cast<const std::uintptr_t *>(banks[0][i] + layout::sampler_state::native);
            const auto *to =
                reinterpret_cast<const std::uintptr_t *>(banks[material_bank][i] + layout::sampler_state::native);
            for (unsigned v = 0; v < levels; ++v) {
                if (!from[v]) continue;
                const D3D12_CPU_DESCRIPTOR_HANDLE source{*reinterpret_cast<const SIZE_T *>(from[v] + native_handle)};
                const D3D12_CPU_DESCRIPTOR_HANDLE target{*reinterpret_cast<const SIZE_T *>(to[v] + native_handle)};
                device->CopyDescriptorsSimple(1, target, source, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
            }
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool read_cache(std::uintptr_t base, std::uint32_t &tables, std::uint32_t &slots, std::uint32_t &capacity) noexcept {
    std::uintptr_t backend{}, cache{};
    return base && pb::read(base + layout::image::d3d_render, backend) && backend &&
           pb::read(backend + layout::d3d_render::sampler_tables, cache) && cache &&
           pb::read(cache + layout::sampler_table_cache::count, tables) &&
           pb::read(cache + layout::sampler_table_cache::next, slots) &&
           pb::read(cache + layout::sampler_table_cache::capacity, capacity);
}
} // namespace

void observe(const std::uintptr_t base) noexcept {
    image.store(base);
}

Install install(const std::uintptr_t host) noexcept {
    if (live) return Install::installed;
    if (!refusal_text.empty() && refusal_text != "banks not built yet") return Install::refused;
    ++attempts;
    image.store(host);
    try {
        if (attempts == 1 && !gates_pass(host)) return Install::refused;
        std::uintptr_t shader{}, scene{}, backend{}, device{};
        std::uint32_t bank{};
        if (!pb::read(host + layout::image::shader, shader) || !shader ||
            !pb::read(host + layout::image::scene, scene) || !scene ||
            !pb::read(host + layout::image::d3d_render, backend) || !backend ||
            !pb::read(backend + layout::d3d_render::device, device) || !device) {
            refusal_text = "banks not built yet";
            return Install::retry;
        }
        bool bad{};
        if (!banks_as_built(shader, bad)) {
            refusal_text = bad ? "sampler banks not as tested" : "banks not built yet";
            return bad ? Install::refused : Install::retry;
        }
        // As shipped materials take bank 0; anything else is another mod's choice.
        if (!pb::read(scene + layout::scene::sampler_lod_bias, bank) || bank) {
            refusal_text = "the materials' sampler bank is " + std::to_string(bank);
            return Install::refused;
        }
        materials = banks[material_bank];
        if (!copy_bank_zero(reinterpret_cast<ID3D12Device *>(device))) {
            refusal_text = "material bank copy";
            return Install::refused;
        }
        hook.name = "mhw_dlss Textures:SamplerPick";
        hook.pattern = pick_pattern;
        hook.target = reinterpret_cast<void *>(host + 0x263daf0);
        if (!astera::hook_owned(hook, reinterpret_cast<void *>(pick), reinterpret_cast<void **>(&pick_original))) {
            refusal_text = "texture bias hook";
            return Install::refused;
        }
        // Materials loaded from now on take the material bank, which the hook
        // follows (bank 0, the game's own, until DLSS draws).
        if (!write_value(scene + layout::scene::sampler_lod_bias, std::uint32_t{material_bank})) {
            astera::unhook_owned(hook);
            refusal_text = "material bank select";
            return Install::refused;
        }
        refusal_text.clear();
        live = true;
        return Install::installed;
    } catch (...) {
        refusal_text = "texture bias install";
        return Install::refused;
    }
}

bool installed() noexcept {
    return live;
}
const char *refusal() noexcept {
    return refusal_text.c_str();
}

void follow(const float scale, std::ostream &log) noexcept {
    if (!live) return;
    const auto now = GetTickCount64();
    std::uint32_t tables{};
    if (!read_cache(image.load(), tables, slots_now, capacity_now)) return;
    capacity_now = std::min<std::uint32_t>(capacity_now, 2048U);
    // New table sets since the last loop: their cost is the cache's growth
    // over the 2 s after the last of them appeared.
    unsigned fresh = 0;
    for (unsigned b = 0; b < bank_count; ++b) {
        const auto u = used[b].load(std::memory_order_relaxed);
        fresh += static_cast<unsigned>(std::popcount(u & ~seen[b]));
        seen[b] = u;
    }
    if (fresh) {
        if (!window_end) window_slots = previous_slots;
        window_sets += fresh;
        window_end = now + 2000;
    } else if (window_end && now >= window_end) {
        if (window_sets && slots_now > window_slots)
            measured_cost = std::max(measured_cost, (slots_now - window_slots) / window_sets);
        window_end = window_sets = 0;
    }
    previous_slots = slots_now;
    const Room r{slots_now,  capacity_now, std::max(measured_cost, min_set_cost),
                 level_cost, room_margin,  used[0].load(std::memory_order_relaxed)};
    const bool fits = room_for_new_set(r);
    room.store(fits, std::memory_order_relaxed);
    // The mode's bank; without room for its set at this level, the nearest
    // bank that has one there (bank 0's is in the reserve).
    wanted = bank_for(scale);
    wanted_scale = scale;
    const auto bit = 1U << material_level.load(std::memory_order_relaxed);
    unsigned bank = wanted;
    if (bank && !(used[bank].load(std::memory_order_relaxed) & bit) && !fits) {
        bank = 0;
        for (unsigned b = 1; b <= 7; ++b)
            if ((used[b].load(std::memory_order_relaxed) & bit) &&
                std::fabs(bank_bias(b) - bank_bias(wanted)) < std::fabs(bank_bias(bank) - bank_bias(wanted)))
                bank = b;
    }
    if (bank == active.load(std::memory_order_relaxed)) return;
    active.store(bank, std::memory_order_relaxed);
    ++changes;
    log << "{\"event\":\"texture_bias\",\"bank\":" << bank << ",\"bias\":" << bank_bias(bank)
        << ",\"wanted\":" << wanted << ",\"scale\":" << scale << ",\"level\":" << material_level.load()
        << ",\"slots\":" << slots_now << ",\"capacity\":" << capacity_now << ",\"room\":" << fits
        << ",\"set_cost\":" << r.set_cost << "}\n"
        << std::flush;
}

void write(std::ostream &out) {
    const auto base = image.load();
    std::uint32_t tables{}, slots{}, capacity{};
    const bool cache_read = read_cache(base, tables, slots, capacity);
    std::uintptr_t scene{};
    std::uint32_t bank{};
    float default_bias{};
    const bool scene_read = base && pb::read(base + layout::image::scene, scene) && scene &&
                            pb::read(scene + layout::scene::sampler_lod_bias, bank) &&
                            pb::read(scene + layout::scene::default_bias, default_bias);
    out << "{\"live\":" << live << ",\"refusal\":\"" << refusal_text << "\",\"attempts\":" << attempts
        << ",\"active\":" << active.load() << ",\"wanted\":" << wanted << ",\"scale\":" << wanted_scale
        << ",\"level\":" << material_level.load() << ",\"room\":" << room.load() << ",\"set_cost\":" << measured_cost
        << ",\"fallbacks\":" << fallbacks.load() << ",\"changes\":" << changes << ",\"used\":[";
    for (unsigned b = 0; b < bank_count; ++b) out << (b ? "," : "") << used[b].load();
    out << "],\"cache\":";
    if (cache_read)
        out << "{\"tables\":" << tables << ",\"slots\":" << slots << ",\"capacity\":" << capacity << '}';
    else
        out << "null";
    out << ",\"material_bank\":";
    if (scene_read)
        out << bank << ",\"default_bias\":" << default_bias;
    else
        out << "null";
    // Each bank's first template bias and MaxLOD, null for one that doesn't read.
    out << ",\"banks\":[";
    std::uintptr_t shader{};
    const bool shader_read = base && pb::read(base + layout::image::shader, shader) && shader;
    for (unsigned i = 0; i < bank_count; ++i) {
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
