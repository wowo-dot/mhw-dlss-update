#pragma once
#include <bit>
#include <cmath>
#include <cstdint>

// The texture mip bias's pure rules (docs/research/texture-mip-bias.md;
// tests/texture_bias_tests.cpp). texture_bias.cpp applies them.
namespace mhw::continuous_dlaa::texture_bias {
// The game's sampler banks (built once, 0x23d6690): 15 of 63 states each;
// bank 0 has no bias, banks 1-7 -log2(c) and banks 8-14 +log2(c), for c = 2.0,
// 2.5 ... 5.0, added to each template's own bias up to its MaxLOD. A state has
// a native sampler for each anisotropy level (1, 2, 4, 8, 16).
inline constexpr unsigned bank_count = 15, states_per_bank = 63, levels = 5;
inline float bank_bias(unsigned bank) noexcept {
    if (bank == 0 || bank >= bank_count) return 0.F;
    const float c = 2.F + 0.5F * static_cast<float>((bank - 1) % 7);
    return bank <= 7 ? -std::log2(c) : std::log2(c);
}
// The bank materials take once the mod runs: a positive bank the shipped game
// never binds, with bank 0's descriptors copied over it, so it renders as bank
// 0 even without the hook (texture_bias.cpp).
inline constexpr unsigned material_bank = 14;

// NVIDIA's bias for a render scale (render width / output width; DLSS
// Programming Guide 3.5: log2(scale) - 1), as the nearest of the game's banks
// 0-7. The game's ladder is that formula at 1, 0.8, 0.67, 0.57, 0.5, 0.44 and
// 0.4: DLAA takes bank 1 (-1), Quality bank 3 (-1.58), Balanced bank 4 (-1.81
// for -1.79) and Performance bank 5 (-2). No scale (DLSS not drawing) is bank
// 0, the game's own.
inline unsigned bank_for(float scale) noexcept {
    if (!(scale > 0.F) || scale > 1.0001F) return 0;
    const float wanted = std::log2(scale) - 1.F;
    unsigned best = 0;
    for (unsigned b = 1; b <= 7; ++b)
        if (std::fabs(bank_bias(b) - wanted) < std::fabs(bank_bias(best) - wanted)) best = b;
    return best;
}

// The resident sampler table cache never evicts and never checks its bound
// (2048 slots); every new list of handles keeps its slots for good.
// - A bias bank's first binds make a set of material tables: 40 to 70 slots
//   (run 26960: five banks at two levels, 707 slots in all).
// - A new anisotropy level makes a set for every sampler with variants,
//   materials or not: about 225 slots (run 54460: four new levels, 433 to 1329).
// A bias bank may start a set only while the cache keeps room for it and for
// a new level's set at every level bank 0 hasn't been bound at yet (DLSS Off,
// a stop or a fault brings bank 0 back at whatever level the player picks).
struct Room {
    std::uint32_t slots{}, capacity{}; // the cache's next slot and its size
    std::uint32_t set_cost{};          // a bias bank's set: the largest measured, at least min_set_cost
    std::uint32_t level_cost{};        // a new level's set
    std::uint32_t margin{};
    std::uint32_t bank0_levels{}; // bit per level bank 0 has a set at
};
inline constexpr std::uint32_t min_set_cost = 64, level_cost = 256, room_margin = 128;
inline bool room_for_new_set(const Room &r) noexcept {
    const auto have = static_cast<unsigned>(std::popcount(r.bank0_levels & ((1U << levels) - 1U)));
    const std::uint64_t needed =
        std::uint64_t{r.slots} + r.set_cost + std::uint64_t{r.level_cost} * (levels - have) + r.margin;
    return needed <= r.capacity;
}
} // namespace mhw::continuous_dlaa::texture_bias
