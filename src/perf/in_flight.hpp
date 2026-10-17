#pragma once
#include <windows.h>
#include <array>
#include <atomic>

namespace mhw::perf {
// Callbacks in flight, counted without an atomic read-modify-write on the
// callback's path (v0.23.10). The binding observer and the continuous hooks
// count every call they admit, about 25,000 a frame on several recording
// threads; a locked instruction there waits for the store buffer the driver
// has just filled, about 20-50 ns a call in game (game-94).
//
// Each thread has its own slot on its own cache line and changes it with
// plain stores (one writer). A callback adds, then reads its admission flag;
// the compiler may not reorder the two, but the processor may (store, then
// load). The drainer covers that: after clearing the flag it calls zero(),
// which runs FlushProcessWriteBuffers (an interrupt on every processor, so
// each one's earlier stores become visible and its later loads see the
// cleared flag) before reading the slots. A callback that saw the flag set is
// then visible in its slot until it subtracts; one that runs after the flush
// sees the flag cleared. Poll with idle() after one zero(). Threads beyond the
// slots share one counter with locked instructions.
class InFlightCount {
public:
    void add() noexcept {
        if (auto *s = slot()) {
            s->store(s->load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
            std::atomic_signal_fence(std::memory_order_seq_cst);
        } else {
            shared_.fetch_add(1, std::memory_order_seq_cst);
        }
    }
    void sub() noexcept {
        if (auto *s = slot())
            s->store(s->load(std::memory_order_relaxed) - 1, std::memory_order_release);
        else
            shared_.fetch_sub(1, std::memory_order_release);
    }
    // Drainer, after clearing the admission flag: every slot at zero.
    bool zero() const noexcept {
        FlushProcessWriteBuffers();
        return idle();
    }
    // The slots without the flush: for polling after one zero().
    bool idle() const noexcept {
        if (shared_.load(std::memory_order_acquire)) return false;
        for (const auto &s : slots_)
            if (s.value.load(std::memory_order_acquire)) return false;
        return true;
    }
    // A sum for status output; not a consistent snapshot.
    unsigned load() const noexcept {
        unsigned n = shared_.load(std::memory_order_acquire);
        for (const auto &s : slots_) n += s.value.load(std::memory_order_acquire);
        return n;
    }

private:
    static constexpr unsigned slot_count = 256;
#pragma warning(push)
#pragma warning(disable : 4324) // padded to its own cache line on purpose
    struct alignas(64) Slot {
        std::atomic<unsigned> value{};
    };
#pragma warning(pop)
    std::array<Slot, slot_count> slots_{};
    std::atomic<unsigned> shared_{};
    std::atomic<unsigned> *slot() noexcept {
        const auto i = thread_index();
        return i < slot_count ? &slots_[i].value : nullptr;
    }
    // One index per thread for every count.
    static unsigned thread_index() noexcept {
        static std::atomic<unsigned> next{};
        thread_local const unsigned index = next.fetch_add(1, std::memory_order_relaxed);
        return index;
    }
};
} // namespace mhw::perf
