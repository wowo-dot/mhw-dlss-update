#pragma once
#include <array>
#include <atomic>

namespace mhw::perf {
// A count of callbacks in flight that many threads change at a high rate
// (v0.23.8). Each thread has its own shard on its own cache line, so the
// read-modify-writes of busy threads no longer contend for one line (the
// binding observer's single counter took two contended atomic adds on each of
// about 22,000 calls a frame, game-92). A thread always uses the same shard.
//
// The drain contract is the single counter's: a callback adds before it reads
// its admission flag and subtracts when done; after clearing the flag, a
// drainer that observes every shard at zero once (zero() or settled_within)
// knows that no callback that saw the flag set is still running, since such a
// callback's add precedes the flag's clearing and only its own subtract can
// bring its shard back to zero.
class ShardedCount {
public:
    void add() noexcept { slot().fetch_add(1, std::memory_order_acq_rel); }
    void sub() noexcept { slot().fetch_sub(1, std::memory_order_release); }
    bool zero() const noexcept {
        for (const auto &s : shards_)
            if (s.value.load(std::memory_order_acquire)) return false;
        return true;
    }
    // A sum for status output; not a consistent snapshot.
    unsigned load() const noexcept {
        unsigned n{};
        for (const auto &s : shards_) n += s.value.load(std::memory_order_acquire);
        return n;
    }

private:
    static constexpr unsigned shard_count = 64;
#pragma warning(push)
#pragma warning(disable : 4324)  // padded to its own cache line on purpose
    struct alignas(64) Shard {
        std::atomic<unsigned> value{};
    };
#pragma warning(pop)
    std::array<Shard, shard_count> shards_{};
    std::atomic<unsigned> &slot() noexcept {
        static std::atomic<unsigned> next{};
        thread_local const unsigned index = next.fetch_add(1, std::memory_order_relaxed) % shard_count;
        return shards_[index].value;
    }
};
}  // namespace mhw::perf
