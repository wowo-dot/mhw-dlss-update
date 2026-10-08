#pragma once
#include <array>
#include <cstdint>

// Bounded ownership ring for FG input copies. The caller serializes every call.
// A slot acquired unconfirmed (the final draw was recorded before its frame's
// DLSS inputs were staged, v0.12.5) can be used only after confirm().
//
// recording -> recorded (on the game command list) -> ready (list generation
// submitted) -> tagged (owner thread, before PresentStart) -> consuming (SDK
// consumption fence value) -> free (fence completed). Copies are never
// overwritten while the SDK may still read them; a frame without a free slot
// simply gets no FG inputs.
namespace mhw::frame_generation {

template <unsigned N>
class InputRing final {
public:
    static constexpr unsigned none = UINT32_MAX, capacity = N;
    enum class State : unsigned { free, recording, recorded, ready, tagged, consuming };
    struct Slot {
        State state{State::free};
        std::uint64_t frame{}, epoch{}, list{}, generation{}, batch{}, token{}, fence{};
        bool confirmed{true};
    };
    struct Counters {
        std::uint64_t acquired{}, starved{}, duplicates{}, abandoned{}, submitted{}, discarded{}, taken{},
            stale{}, no_ready{}, consumed{}, reclaimed{}, unconfirmed{};
    };

private:
    std::array<Slot, N> slots_{};
    Counters counters_{};

public:
    // Render side, before recording copies for a producer frame.
    unsigned acquire(std::uint64_t frame, std::uint64_t epoch, std::uint64_t completed_fence, bool confirmed = true) noexcept {
        reclaim(completed_fence);
        for (const auto &s : slots_)
            if (s.state != State::free && s.state != State::consuming && s.frame == frame && s.epoch == epoch) {
                ++counters_.duplicates;
                return none;
            }
        for (unsigned i = 0; i < N; ++i)
            if (slots_[i].state == State::free) {
                slots_[i] = {State::recording, frame, epoch};
                slots_[i].confirmed = confirmed;
                ++counters_.acquired;
                return i;
            }
        ++counters_.starved;
        return none;
    }
    bool recorded(unsigned slot, std::uint64_t list, std::uint64_t generation) noexcept {
        if (slot >= N || slots_[slot].state != State::recording || !list || !generation) return false;
        auto &s = slots_[slot];
        s.state = State::recorded;
        s.list = list;
        s.generation = generation;
        return true;
    }
    void abandon(unsigned slot) noexcept {
        if (slot < N && (slots_[slot].state == State::recording || slots_[slot].state == State::recorded)) {
            slots_[slot] = {};
            ++counters_.abandoned;
        }
    }
    // The frame's inputs were staged after its slot was recorded: the slot may
    // now be presented. Returns the slot, or none.
    unsigned confirm(std::uint64_t frame, std::uint64_t epoch) noexcept {
        for (unsigned i = 0; i < N; ++i) {
            auto &s = slots_[i];
            if (!s.confirmed && s.frame == frame && s.epoch == epoch &&
                (s.state == State::recording || s.state == State::recorded || s.state == State::ready)) {
                s.confirmed = true;
                return i;
            }
        }
        return none;
    }
    // Successful native ExecuteCommandLists receipt for one list generation.
    void submitted(std::uint64_t list, std::uint64_t generation, std::uint64_t batch) noexcept {
        for (auto &s : slots_)
            if (s.state == State::recorded && s.list == list && s.generation == generation) {
                s.state = State::ready;
                s.batch = batch;
                ++counters_.submitted;
            }
    }
    // A Reset of a list generation that never reached the queue discards it.
    void reset(std::uint64_t list, std::uint64_t generation) noexcept {
        for (auto &s : slots_)
            if (s.state == State::recorded && s.list == list && s.generation == generation) {
                s = {};
                ++counters_.discarded;
            }
    }
    // Owner thread, before PresentStart. The presented image belongs to the
    // newest submitted producer frame older than the presenting native frame;
    // older ready slots missed their Present and are released.
    unsigned take(std::uint64_t present_frame, std::uint64_t token) noexcept {
        unsigned chosen = none;
        for (unsigned i = 0; i < N; ++i) {
            const auto &s = slots_[i];
            if (s.state == State::ready && s.confirmed && s.frame < present_frame &&
                (chosen == none || s.frame > slots_[chosen].frame))
                chosen = i;
        }
        for (unsigned i = 0; i < N; ++i)
            if (i != chosen && slots_[i].state == State::ready && slots_[i].frame < present_frame) {
                ++(slots_[i].confirmed ? counters_.stale : counters_.unconfirmed);
                slots_[i] = {};
            }
        if (chosen == none) {
            ++counters_.no_ready;
            return none;
        }
        slots_[chosen].state = State::tagged;
        slots_[chosen].token = token;
        ++counters_.taken;
        return chosen;
    }
    // After Present: the SDK's input-consumption fence value for this frame.
    // Zero means already complete.
    void presented(unsigned slot, std::uint64_t fence_value) noexcept {
        if (slot >= N || slots_[slot].state != State::tagged) return;
        if (!fence_value) {
            slots_[slot] = {};
            ++counters_.consumed;
            return;
        }
        slots_[slot].state = State::consuming;
        slots_[slot].fence = fence_value;
    }
    // Tagging failed or FG was not used: the SDK never saw the slot.
    void untag(unsigned slot) noexcept {
        if (slot < N && slots_[slot].state == State::tagged) slots_[slot] = {};
    }
    void reclaim(std::uint64_t completed_fence) noexcept {
        for (auto &s : slots_)
            if (s.state == State::consuming && s.fence <= completed_fence) {
                s = {};
                ++counters_.reclaimed;
            }
    }
    const Slot &slot(unsigned i) const noexcept { return slots_[i]; }
    const Counters &counters() const noexcept { return counters_; }
};

}  // namespace mhw::frame_generation
