#include "reflex/frame_stamp.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using namespace mhw::reflex;
constexpr std::uintptr_t test_renderer = 0x1000;
void check(bool value, const char *why) {
    if (!value) throw std::runtime_error(why);
}
struct Reader {
    unsigned calls{}, missing{};
    std::uint64_t epoch{100}, temporal{99}, after{100};
    unsigned bank{}, bank_after{};
    template <class T> bool operator()(std::uintptr_t at, T &value) noexcept {
        ++calls;
        if (calls == missing) return false;
        if (at == test_renderer + 0x1f208)
            value = static_cast<T>(calls == 1 ? epoch : after);
        else if (at == test_renderer + 0x1f210)
            value = static_cast<T>(temporal);
        else if (at == test_renderer + 0x1f200)
            value = static_cast<T>(calls == 3 ? bank : bank_after);
        else
            return false;
        return true;
    }
};
void tests() {
    Reader stable;
    auto s = read_frame_stamp(stable, test_renderer);
    check(s.status == StampReadStatus::stable && s.value == FrameStamp{100, 99, 0, true} && stable.calls == 5,
          "qualified unchanged counters");
    for (unsigned missing = 1; missing <= 5; ++missing) {
        Reader reader;
        reader.missing = missing;
        s = read_frame_stamp(reader, test_renderer);
        check(s.status == StampReadStatus::unreadable && !s.value.valid && !retry_anchor_stamp(s.status, 1, 0),
              "unreadable observation never retries");
    }
    for (unsigned bad = 0; bad < 2; ++bad) {
        Reader reader;
        if (bad)
            reader.bank_after = 2;
        else
            reader.bank = 2;
        s = read_frame_stamp(reader, test_renderer);
        check(s.status == StampReadStatus::invalid_bank && !s.value.valid && !retry_anchor_stamp(s.status, 1, 0),
              "invalid bank remains fatal");
    }
    for (unsigned moved = 0; moved < 2; ++moved) {
        Reader reader;
        if (moved)
            reader.bank_after = 1;
        else
            reader.after = 101;
        s = read_frame_stamp(reader, test_renderer);
        check(s.status == StampReadStatus::changing && !s.value.valid && retry_anchor_stamp(s.status, 1, 0),
              "moving publication is eligible for setup retry only");
        Reader next;
        s = read_frame_stamp(next, test_renderer);
        check(s.status == StampReadStatus::stable && s.value.valid, "next coherent observation qualifies");
    }
    check(!retry_anchor_stamp(StampReadStatus::stable, 1, 0) && !retry_anchor_stamp(StampReadStatus::changing, 0, 0) &&
              retry_anchor_stamp(StampReadStatus::changing, 63, 99) &&
              !retry_anchor_stamp(StampReadStatus::changing, 64, 0) &&
              !retry_anchor_stamp(StampReadStatus::changing, 1, 100),
          "both retry budgets are bounded");
    Reader overflow;
    s = read_frame_stamp(overflow, (std::numeric_limits<std::uintptr_t>::max)());
    check(s.status == StampReadStatus::unreadable && !overflow.calls, "overflow never dereferences");
    s = read_frame_stamp(overflow, 0);
    check(s.status == StampReadStatus::unreadable && !overflow.calls, "null never dereferences");
}
} // namespace
int main() {
    try {
        tests();
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
