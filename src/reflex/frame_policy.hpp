#pragma once
#include <cstdint>
#include <limits>

namespace mhw::reflex {
// Pure single-thread frame contract. The integration must first qualify the
// engine/renderer objects and bind these events to verified native boundaries.
// An action is a request, not evidence that an SDK call or GPU frame completed.
enum class FrameAction : unsigned {
    none = 0,
    new_token = 1,
    sleep = 2,
    simulation_start = 4,
    simulation_end = 8,
    render_submit_start = 16,
    render_submit_end = 32,
    present_start = 64,
    present_end = 128
};
constexpr unsigned bits(FrameAction action) noexcept {
    return static_cast<unsigned>(action);
}
enum class FrameFault { none, order, thread, stamp, counter, exhausted, sdk };
struct FrameStamp {
    std::uint64_t epoch{}, temporal{};
    unsigned bank{};
    bool valid{};
    friend constexpr bool operator==(const FrameStamp &, const FrameStamp &) = default;
};
struct FrameStep {
    unsigned actions{};
    std::uint32_t id{};
    FrameFault fault{};
    bool accepted{};
};
class FramePolicy final {
public:
    // Explicit monotonically increasing IDs belong to this integration, not
    // to the renderer's temporal counter (which can remain still when paused).
    explicit constexpr FramePolicy(std::uint32_t first_id = 1) noexcept : next_(first_id) {}
    constexpr FrameStep begin(unsigned thread, FrameStamp stamp) noexcept {
        if (disabled_) return result(0, false);
        if (stage_ != Stage::idle) return fail(FrameFault::order);
        if (!thread || (owner_ && thread != owner_)) return fail(FrameFault::thread);
        if (!good(stamp)) return fail(FrameFault::stamp);
        if (next_ == (std::numeric_limits<std::uint32_t>::max)()) return fail(FrameFault::exhausted);
        owner_ = thread;
        id_ = next_++;
        initial_ = stamp;
        stage_ = Stage::simulation;
        return result(bits(FrameAction::new_token) | bits(FrameAction::sleep) | bits(FrameAction::simulation_start));
    }
    constexpr FrameStep render_begin(unsigned thread, FrameStamp stamp) noexcept {
        if (disabled_) return result(0, false);
        if (thread != owner_) return fail(FrameFault::thread);
        if (stage_ != Stage::simulation) return fail(FrameFault::order);
        if (!good(stamp)) return fail(FrameFault::stamp);
        if (initial_.epoch == (std::numeric_limits<std::uint64_t>::max)() || stamp.epoch != initial_.epoch + 1 ||
            stamp.bank != (initial_.bank ^ 1U) || stamp.temporal < initial_.temporal ||
            stamp.temporal - initial_.temporal > 1)
            return fail(FrameFault::counter);
        rendering_ = stamp;
        stage_ = Stage::render;
        return result(bits(FrameAction::simulation_end) | bits(FrameAction::render_submit_start));
    }
    constexpr FrameStep render_end(unsigned thread, FrameStamp stamp) noexcept {
        return advance(thread, stamp, Stage::render, Stage::await_present, FrameAction::render_submit_end);
    }
    constexpr FrameStep present_begin(unsigned thread, FrameStamp stamp, bool test) noexcept {
        // DXGI_PRESENT_TEST does not submit a presented frame, regardless of
        // its thread or timing. Leave the ongoing frame untouched.
        if (test) return result(0, false);
        return advance(thread, stamp, Stage::await_present, Stage::present, FrameAction::present_start);
    }
    constexpr FrameStep present_end(unsigned thread, FrameStamp stamp, bool test) noexcept {
        if (test) return result(0, false);
        return advance(thread, stamp, Stage::present, Stage::complete, FrameAction::present_end);
    }
    constexpr FrameStep end(unsigned thread) noexcept {
        if (disabled_) return result(0, false);
        if (thread != owner_) return fail(FrameFault::thread);
        if (stage_ != Stage::complete) return fail(FrameFault::order);
        stage_ = Stage::idle;
        ++completed_;
        return result();
    }
    // No fabricated closing markers on SDK failure or intentional shutdown.
    // Native originals must still run; the caller owns hook/resource cleanup.
    constexpr void abort(FrameFault fault = FrameFault::sdk) noexcept {
        disabled_ = true;
        fault_ = fault;
    }
    constexpr bool disabled() const noexcept { return disabled_; }
    // The current frame reached the end of its Present.
    constexpr bool presented() const noexcept { return !disabled_ && stage_ == Stage::complete; }
    constexpr std::uint64_t completed() const noexcept { return completed_; }
    constexpr std::uint32_t id() const noexcept { return id_; }
    constexpr bool input_token_available(unsigned thread) const noexcept {
        return !disabled_ && owner_ == thread && thread &&
               (stage_ == Stage::simulation || stage_ == Stage::render || stage_ == Stage::await_present);
    }

private:
    enum class Stage { idle, simulation, render, await_present, present, complete };
    constexpr static bool good(FrameStamp stamp) noexcept { return stamp.valid && stamp.bank < 2; }
    constexpr FrameStep result(unsigned actions = 0, bool accepted = true) const noexcept {
        return {actions, id_, fault_, accepted};
    }
    constexpr FrameStep fail(FrameFault fault) noexcept {
        abort(fault);
        return result(0, false);
    }
    constexpr FrameStep advance(unsigned thread, FrameStamp stamp, Stage expected, Stage next,
                                FrameAction action) noexcept {
        if (disabled_) return result(0, false);
        if (thread != owner_) return fail(FrameFault::thread);
        if (stage_ != expected) return fail(FrameFault::order);
        if (!good(stamp)) return fail(FrameFault::stamp);
        if (stamp != rendering_) return fail(FrameFault::counter);
        stage_ = next;
        return result(bits(action));
    }
    Stage stage_{Stage::idle};
    FrameStamp initial_{}, rendering_{};
    unsigned owner_{};
    std::uint32_t next_{}, id_{};
    std::uint64_t completed_{};
    FrameFault fault_{FrameFault::none};
    bool disabled_{};
};
} // namespace mhw::reflex
