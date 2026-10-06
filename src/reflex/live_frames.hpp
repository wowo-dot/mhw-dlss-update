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
