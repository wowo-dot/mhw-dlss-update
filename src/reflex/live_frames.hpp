#pragma once
#include "presentation.hpp"
#include "driver_status.hpp"
#include <cstdint>
#include <iosfwd>
#include <string>

namespace mhw::reflex {
struct LiveFrameStats {
    std::uint64_t completed{}, tokens{}, sleeps{}, markers{}, faults{}, present_tests{}, untracked_presents{};
    unsigned owner{}, pending_calls{}, frame_id{};
    bool running{}, frame_active{}, restored{};
    const char *fault_reason{};  // the first fault's reason (a string literal), or null
    // The first fault's context. call: 0 Main:Update, 2 Render:Publish,
    // 11 Render:Submit (+100 on return), 20 Present, 21 Present return.
    // links: the identity checks that failed (1 engine, 2 renderer, 4 backend
    // globals, 8 device, 16 engine->renderer, 32 renderer->backend). code: the
    // Streamline result (+0x100 when the frame policy is disabled), or Present's
    // HRESULT.
    struct Fault { unsigned call{}, thread{}, owner{}, links{}, code{}, stage{}; std::uint64_t frame{}; bool in_frame{}; } fault;
};
// Process-lived storage; one instance. Worker installs/restores. FrameSession
// is serialized to the qualified native owner thread. Original calls always
// forward, including after feature rejection. SDK shutdown follows drain.
class LiveFrames final {
public:
    struct State;
    explicit LiveFrames(Streamline& sdk);
    static bool reserve(std::uintptr_t image, std::string& reason);
    void install(std::uintptr_t image, std::uintptr_t sdk_device, std::ostream& log);
    PresentationObserver observer() noexcept;
    LiveFrameStats snapshot() const;
    DriverSleepStatus driver_status(IUnknown* native_device) const;
    void request_stop() noexcept;
    bool stop() noexcept;
    void describe(std::ostream& log, unsigned requested_mode = 0) const;
    void write(std::ostream& log) const;
private:
    State* state_;
};
}
