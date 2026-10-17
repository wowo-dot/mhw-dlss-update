#pragma once
#include <wrl/client.h>
#include <array>
#include <cstdint>
#include <span>

namespace mhw::scaled_temporal {
// Caller serializes access. Slots are recording generations, never frame IDs or
// resource-pointer identities. Reset may precede GPU completion or vice versa.
// Exhaustion rejects admission before recording; it never evicts a live use.
class StreamLifetime {
    using Pin = Microsoft::WRL::ComPtr<IUnknown>;
    struct Entry {
        IUnknown *list{};
        std::uint64_t generation{}, signal{};
        bool reset{};
        std::array<Pin, 96> pins{};
        unsigned count{};
    };
    std::array<Entry, 128> entries_{};
    std::uint64_t retained_{}, released_{};
    unsigned peak_{};

public:
    bool retain(IUnknown *list, std::uint64_t generation, std::span<IUnknown *const> pins) noexcept {
        if (!list || !generation || pins.empty()) return false;
        Entry *entry{}, *empty{};
        for (auto &e : entries_) {
            if (e.list == list && e.generation == generation) entry = &e;
            if (!e.list && !empty) empty = &e;
        }
        if (entry && (entry->signal || entry->reset)) return false;
        auto *e = entry ? entry : empty;
        if (!e) return false;
        std::array<IUnknown *, 96> extra{};
        unsigned count{};
        const auto add = [&](IUnknown *p) {
            if (!p) return false;
            for (unsigned i = 0; i < e->count; ++i)
                if (e->pins[i].Get() == p) return true;
            for (unsigned i = 0; i < count; ++i)
                if (extra[i] == p) return true;
            if (e->count + count == extra.size()) return false;
            extra[count++] = p;
            return true;
        };
        if (!add(list)) return false;
        for (auto *p : pins)
            if (!add(p)) return false;
        if (!entry) {
            e->list = list;
            e->generation = generation;
            ++retained_;
        }
        for (unsigned i = 0; i < count; ++i) e->pins[e->count++] = extra[i];
        if (live() > peak_) peak_ = live();
        return true;
    }
    bool submitted(IUnknown *list, std::uint64_t generation, std::uint64_t signal) noexcept {
        if (!signal || signal == UINT64_MAX) return false;
        for (auto &e : entries_)
            if (e.list == list && e.generation == generation) {
                if (e.signal || e.reset) return false;
                e.signal = signal;
                return true;
            }
        return false;
    }
    bool reset(IUnknown *list, std::uint64_t generation) noexcept {
        for (auto &e : entries_)
            if (e.list == list && e.generation == generation) {
                if (!e.signal || e.reset) return false;
                e.reset = true;
                return true;
            }
        return true;
    }
    void collect(std::uint64_t completed) noexcept {
        if (completed == UINT64_MAX) return;
        for (auto &e : entries_)
            if (e.list && e.reset && e.signal && e.signal <= completed) {
                e = {};
                ++released_;
            }
    }
    bool tracked(IUnknown *list, std::uint64_t generation) const noexcept {
        for (const auto &e : entries_)
            if (e.list == list && e.generation == generation) return true;
        return false;
    }
    unsigned live() const noexcept {
        unsigned n{};
        for (const auto &e : entries_)
            if (e.list) ++n;
        return n;
    }
    unsigned peak() const noexcept { return peak_; }
    std::uint64_t retained() const noexcept { return retained_; }
    std::uint64_t released() const noexcept { return released_; }
};
} // namespace mhw::scaled_temporal
