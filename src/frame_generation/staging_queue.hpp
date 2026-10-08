#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace mhw::frame_generation {
// Frames whose DLSS inputs were copied and whose final draw has not been
// recorded yet (perf::keyed_staging, v0.12.5). Frame f uses staging copy
// f % N. In heavy scenes MHW records frame f's DLSS before frame f-1's final
// draw, so two frames can be staged at once; each final draw takes its own.
template <unsigned N, class Payload>
class StagingQueue final {
public:
    struct Entry {
        bool valid{};
        std::uint64_t frame{}, epoch{};
        Payload payload{};
    };
    struct Taken {
        Entry entry;
        unsigned copy{};
    };
    std::uint64_t dropped{};

    // Before copying frame's inputs into copy frame % N: an entry holding that
    // copy, or a frame not older than this one, can no longer be used.
    unsigned push(std::uint64_t frame, std::uint64_t epoch, const Payload &payload) noexcept {
        const auto copy = static_cast<unsigned>(frame % N);
        for (auto &e : entries_)
            if (e.valid && (e.frame % N == copy || e.frame >= frame)) {
                e = {};
                ++dropped;
            }
        for (auto &e : entries_)
            if (!e.valid) {
                e = {true, frame, epoch, payload};
                break;
            }
        return copy;
    }
    // At frame's final draw: its entry, if staged. Entries up to this frame are
    // released either way (an older frame's final draw can no longer come).
    std::optional<Taken> take(std::uint64_t frame, std::uint64_t epoch) noexcept {
        std::optional<Taken> found;
        for (auto &e : entries_) {
            if (!e.valid) continue;
            if (e.frame == frame && e.epoch == epoch) found = Taken{e, static_cast<unsigned>(e.frame % N)};
            if (e.frame <= frame) {
                if (!(found && found->entry.frame == e.frame && e.epoch == epoch)) ++dropped;
                e = {};
            }
        }
        return found;
    }
    // Newest staged frame (for mismatch diagnostics), if any.
    std::optional<std::uint64_t> newest() const noexcept {
        std::optional<std::uint64_t> n;
        for (const auto &e : entries_)
            if (e.valid && (!n || e.frame > *n)) n = e.frame;
        return n;
    }
    void clear() noexcept { entries_ = {}; }

private:
    std::array<Entry, N> entries_{};
};
}  // namespace mhw::frame_generation
