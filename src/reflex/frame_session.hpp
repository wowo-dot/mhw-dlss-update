#pragma once
#include "frame_policy.hpp"
#include "streamline.hpp"

namespace mhw::reflex {
// Single-owner-thread adapter. Hook callers must qualify their engine/renderer
// object before entering it and serialize lifecycle changes with that thread.
// The SDK owns FrameToken storage. This class borrows the initialized SDK and
// does not own the device, swap chain, hook lifetime or SDK shutdown.
class FrameSession final {
public:
    explicit FrameSession(Streamline& sdk) noexcept : sdk_(sdk) {}
    sl::Result begin(unsigned thread, FrameStamp stamp);
    sl::Result render_begin(unsigned thread, FrameStamp stamp);
    sl::Result render_end(unsigned thread, FrameStamp stamp);
    sl::Result present_begin(unsigned thread, FrameStamp stamp, bool test = false);
    sl::Result present_end(unsigned thread, FrameStamp stamp, bool test = false);
    sl::Result end(unsigned thread);
    void stop() noexcept { policy_.abort(FrameFault::none); token_ = nullptr; }
    bool disabled() const noexcept { return policy_.disabled(); }
    bool presented() const noexcept { return policy_.presented(); }
    std::uint32_t id() const noexcept { return policy_.id(); }
    std::uint64_t completed() const noexcept { return policy_.completed(); }
    std::uint64_t tokens() const noexcept { return tokens_; }
    std::uint64_t sleeps() const noexcept { return sleeps_; }
    std::uint64_t markers() const noexcept { return markers_; }
    // Borrow the existing Reflex token on its owner thread before Present.
    // Never cache it across end/stop or request another SDK token for FG.
    const sl::FrameToken* input_token(unsigned thread) const noexcept {
        return policy_.input_token_available(thread) ? token_ : nullptr;
    }
private:
    sl::Result apply(FrameStep step);
    sl::Result fail(sl::Result result) noexcept;
    Streamline& sdk_;
    FramePolicy policy_;
    sl::FrameToken* token_{};
    sl::Result failure_{sl::Result::eOk};
    std::uint64_t tokens_{}, sleeps_{}, markers_{};
};
}
