#include "../src/exception_capture/depth_tracker.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
namespace e = mhw::exception_capture;
void check(bool v, const char *s) { if (!v) throw std::runtime_error(s); }
template <typename T, std::size_t N> void put(std::array<std::byte, N> &b, std::size_t offset, T v) {
  std::memcpy(b.data() + offset, &v, sizeof(v));
}
struct Fixture {
  std::array<std::byte, 104> owner{};
  std::array<std::byte, 120> backing{};
  std::array<std::byte, 48> tracker{};
  std::array<std::byte, 40> active{};
  std::array<std::byte, 24> context{};
  std::array<e::Address, 3> slots{0x1234, 7, 100};
  e::Snapshot work;
  unsigned reads{}, state_reads{};
  bool fail_tracker{}, mutate_state{}, active_read{};
  Fixture() {
    put(owner, 96, address(backing)); put(backing, 32, e::Address{0x12345678});
    put(backing, 112, address(tracker)); put(tracker, 8, address(active));
    put(tracker, 16, slots[2]); put(tracker, 26, std::uint16_t{4});
    put(active, 34, std::uint16_t{4});
    for (unsigned i = 0; i < 3; ++i) put(context, i * 8, reinterpret_cast<e::Address>(&slots[i]));
    work.descriptor_verified = true;
    work.fields = {{"payload.depth", address(owner)}, {"depth.primary", address(backing)},
                   {"depth.primary.tracker", address(tracker)}, {"depth.primary.native", 0x12345678}};
  }
  template <std::size_t N> static e::Address address(const std::array<std::byte, N> &v) {
    return reinterpret_cast<e::Address>(v.data());
  }
  static bool read(void *ctx, e::Address a, std::span<std::byte> out) {
    auto &f = *static_cast<Fixture *>(ctx); ++f.reads;
    if (a == address(f.tracker) + 26 && ++f.state_reads == 2 && f.mutate_state) put(f.tracker, 26, std::uint16_t{8});
    if (f.fail_tracker && a >= address(f.tracker) && a < address(f.tracker) + f.tracker.size()) return false;
    const std::array<std::span<const std::byte>, 6> memory{f.owner, f.backing, f.tracker, f.active, f.context,
        std::as_bytes(std::span(f.slots))};
    for (const auto bytes : memory) {
      const auto lo = reinterpret_cast<e::Address>(bytes.data());
      if (a >= lo && a - lo <= bytes.size() && out.size() <= bytes.size() - (a - lo)) {
        if (lo == address(f.active)) f.active_read = true;
        std::memcpy(out.data(), bytes.data() + (a - lo), out.size()); return true;
      }
    }
    return false;
  }
  e::TrackerSnapshot capture() { return e::capture_depth_tracker(read, this, work, address(context)); }
};
}
int main() {
  try {
    Fixture same;
    auto s = same.capture();
    check(s.complete && s.same_epoch && s.compatible_read && same.active_read, "active record read state predicts no new transition");
    check(s.data.reads <= 64 && s.data.reads == same.reads, "bounded scalar reads counted");
    Fixture other;
    put(other.tracker, 16, e::Address{99}); put(other.tracker, 8, e::Address{1});
    s = other.capture();
    check(s.complete && !s.same_epoch && s.compatible_read && !other.active_read,
          "older epoch uses tracked state without dereferencing stale record");
    Fixture wrong_active;
    put(wrong_active.active, 34, std::uint16_t{0x100});
    s = wrong_active.capture();
    check(s.complete && !s.compatible_read, "active write state does not inherit compatible tracker state");
    Fixture null_active;
    put(null_active.tracker, 8, e::Address{0});
    check(!null_active.capture().complete, "same-epoch null record rejects retry precondition");
    Fixture guard;
    guard.fail_tracker = true;
    s = guard.capture();
    check(!s.complete && s.data.read_failed, "unreadable tracker is not a zero state");
    Fixture stale;
    stale.mutate_state = true;
    check(!stale.capture().complete, "changed tracker rejected by scalar recheck");
    Fixture replaced;
    put(replaced.backing, 112, e::Address{1});
    check(!replaced.capture().complete && replaced.reads == 3, "mismatched backing anchor stops before traversal");
    Fixture duplicate;
    duplicate.work.fields.push_back(duplicate.work.fields[0]);
    check(!duplicate.capture().complete && duplicate.reads == 0, "duplicate work-item key rejected without reads");
    Fixture invalid;
    invalid.work.read_failed = true;
    check(!invalid.capture().complete && invalid.reads == 0, "failed work-item capture cannot feed tracker traversal");
    std::cout << "Depth tracker branches, anchor checks, bounded reads and unreadable/stale data checks passed.\n";
    return 0;
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
