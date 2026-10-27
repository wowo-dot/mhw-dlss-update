#include "frame_session.hpp"
#include <array>
#include <atomic>

namespace mhw::reflex {
namespace {
std::atomic<bool> latency_ping{};
} // namespace
void request_latency_ping() noexcept {
    latency_ping.store(true, std::memory_order_relaxed);
}
sl::Result FrameSession::fail(sl::Result result) noexcept {
    policy_.abort();
    token_ = nullptr;
    failure_ = result;
    return result;
}
sl::Result FrameSession::apply(FrameStep step) {
    if (failure_ != sl::Result::eOk) return failure_;
    if (step.fault != FrameFault::none) return fail(sl::Result::eErrorInvalidIntegration);
    if (!step.accepted || !step.actions) return sl::Result::eOk;
    if (step.actions & bits(FrameAction::new_token)) {
        token_ = nullptr;
        const auto result = sdk_.frame(token_, &step.id);
        if (result != sl::Result::eOk) return fail(result);
        if (!token_ || static_cast<std::uint32_t>(*token_) != step.id) return fail(sl::Result::eErrorInvalidState);
        ++tokens_;
    }
    if (!token_ || static_cast<std::uint32_t>(*token_) != step.id) return fail(sl::Result::eErrorInvalidState);
    if (step.actions & bits(FrameAction::sleep)) {
        const auto result = sdk_.sleep(*token_);
        if (result != sl::Result::eOk) return fail(result);
        ++sleeps_;
    }
    constexpr std::array markers{std::pair{FrameAction::simulation_start, sl::PCLMarker::eSimulationStart},
                                 std::pair{FrameAction::simulation_end, sl::PCLMarker::eSimulationEnd},
                                 std::pair{FrameAction::render_submit_start, sl::PCLMarker::eRenderSubmitStart},
                                 std::pair{FrameAction::render_submit_end, sl::PCLMarker::eRenderSubmitEnd},
                                 std::pair{FrameAction::present_start, sl::PCLMarker::ePresentStart},
                                 std::pair{FrameAction::present_end, sl::PCLMarker::ePresentEnd}};
    for (const auto &[action, marker] : markers) {
        if (!(step.actions & bits(action))) continue;
        const auto result = sdk_.marker(marker, *token_);
        if (result != sl::Result::eOk) return fail(result);
        ++markers_;
    }
    // A ping asked for since the last simulation start: this frame picks up
    // its input. Optional: a refusal does not stop the frame's markers.
    if ((step.actions & bits(FrameAction::simulation_start)) && latency_ping.exchange(false, std::memory_order_relaxed))
        ++(sdk_.marker(sl::PCLMarker::ePCLatencyPing, *token_) == sl::Result::eOk ? pings_ : ping_failures_);
    return sl::Result::eOk;
}
sl::Result FrameSession::begin(unsigned thread, FrameStamp stamp) {
    return apply(policy_.begin(thread, stamp));
}
sl::Result FrameSession::render_begin(unsigned thread, FrameStamp stamp) {
    return apply(policy_.render_begin(thread, stamp));
}
sl::Result FrameSession::render_end(unsigned thread, FrameStamp stamp) {
    return apply(policy_.render_end(thread, stamp));
}
sl::Result FrameSession::present_begin(unsigned thread, FrameStamp stamp, bool test) {
    return apply(policy_.present_begin(thread, stamp, test));
}
sl::Result FrameSession::present_end(unsigned thread, FrameStamp stamp, bool test) {
    return apply(policy_.present_end(thread, stamp, test));
}
sl::Result FrameSession::end(unsigned thread) {
    const auto result = apply(policy_.end(thread));
    if (result == sl::Result::eOk) token_ = nullptr;
    return result;
}
} // namespace mhw::reflex
