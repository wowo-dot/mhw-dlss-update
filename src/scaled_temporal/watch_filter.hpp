#pragma once
#include <array>
#include <atomic>
#include <bit>
#include <cstdint>

namespace mhw::scaled_temporal {
// v0.24.3: a bit filter of watched addresses (engine packets or D3D12
// resources), two bits an address. may_contain() never misses an added
// address, so a hook can reject an address that no plan watches with two
// loads instead of scanning every watched plan: the stream route keeps up to
// 32 plans watched with about 30-40 packets each, and every engine draw asked
// (game-100: 0.5-0.8 us a draw). A hit still goes to the exact list.
//
// Writers (add, fill, clear, assign) run under the route lock; readers on any
// thread. Bits are only added while a plan is watched; a union is rebuilt
// word by word without the unwatched plan, so a reader always sees at least
// every watched plan's bits.
template <unsigned Words>
class WatchFilter {
public:
    static constexpr unsigned words = Words;
    static constexpr unsigned shift = static_cast<unsigned>(std::countr_zero(Words * 64U));
    static_assert(std::has_single_bit(Words) && 2 * shift <= 64);
    static constexpr std::array<unsigned, 2> bits(std::uintptr_t address) noexcept {
        // Engine objects and resources are 16-byte aligned: drop those bits,
        // then take two fields of a multiplicative hash's high bits.
        const auto h = (static_cast<std::uint64_t>(address) >> 4) * 0x9E3779B97F4A7C15ULL;
        return {static_cast<unsigned>(h >> (64 - shift)), static_cast<unsigned>((h >> (64 - 2 * shift)) & (Words * 64U - 1))};
    }
    void add(std::uintptr_t address) noexcept {
        for (const auto b : bits(address)) words_[b >> 6].fetch_or(1ULL << (b & 63), std::memory_order_relaxed);
    }
    // Every address may be watched (a plan whose list overflowed).
    void fill() noexcept {
        for (auto &w : words_) w.store(~0ULL, std::memory_order_relaxed);
    }
    void clear() noexcept {
        for (auto &w : words_) w.store(0, std::memory_order_relaxed);
    }
    bool may_contain(std::uintptr_t address) const noexcept {
        for (const auto b : bits(address))
            if (!(words_[b >> 6].load(std::memory_order_acquire) & (1ULL << (b & 63)))) return false;
        return true;
    }
    // A rebuilt union, one word at a time: a reader sees the old or the new
    // word, and both hold every still-watched address.
    void assign(const std::array<std::uint64_t, Words> &value) noexcept {
        for (unsigned i = 0; i < Words; ++i) words_[i].store(value[i], std::memory_order_release);
    }
    // For building a union locally.
    static void set(std::array<std::uint64_t, Words> &value, std::uintptr_t address) noexcept {
        for (const auto b : bits(address)) value[b >> 6] |= 1ULL << (b & 63);
    }

private:
    std::array<std::atomic<std::uint64_t>, Words> words_{};
};
// One plan's list (about 40 addresses: under 1% of others pass).
using PlanFilter = WatchFilter<16>;
// Every watched plan (up to about 1,300 addresses: about 2% of others pass).
using UnionFilter = WatchFilter<256>;
}  // namespace mhw::scaled_temporal
