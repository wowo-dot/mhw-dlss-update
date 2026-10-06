#include "temporal_snapshot.hpp"
#include <cstring>
#include <iostream>
#include <limits>

namespace b = mhw::bridge_probe;
namespace {
int fail(const char *text) {
  std::cerr << text << '\n';
  return 1;
}
struct Fixture {
  static constexpr std::uintptr_t host = 0x140000000U, manager = 0x100000U;
  std::array<std::uint32_t, 4> ids{b::kTemporalSentinel, b::kTemporalSentinel,
                                   b::kTemporalSentinel, b::kTemporalSentinel};
  std::uintptr_t root{manager};
  std::uint8_t mode56{}, mode57{};
  std::uint32_t queued{5}, calls{}, fail_on{};
  bool changed{};
  static b::ScalarReadResult read(void *context, std::uintptr_t address,
                                  std::span<std::byte> output) noexcept {
    auto &f = *static_cast<Fixture *>(context);
    ++f.calls;
    if (f.calls == f.fail_on)
      return b::ScalarReadResult::fault;
    const auto copy = [&output]<typename T>(const T &value) {
      if (output.size() != sizeof(T))
        return b::ScalarReadResult::unreadable;
      std::memcpy(output.data(), &value, sizeof(T));
      return b::ScalarReadResult::success;
    };
    for (std::size_t i = 0; i < f.ids.size(); ++i)
      if (address == host + b::kTemporalCacheRvas[i])
        return copy(f.ids[i]);
    if (address == host + 0x51CC238U)
      return copy(f.changed && f.calls > 5 ? f.root + 8 : f.root);
    if (address == manager + 56)
      return copy(f.mode56);
    if (address == manager + 57)
      return copy(f.mode57);
    if (address == manager + 60)
      return copy(f.queued);
    return b::ScalarReadResult::unreadable;
  }
  b::TemporalSnapshot capture() {
    return b::capture_temporal_snapshot(host, &read, this);
  }
};
} // namespace
int main() {
  Fixture initial;
  const auto before = initial.capture();
  if (before.status != b::TemporalStatus::complete || before.reads != 9 ||
      !before.manager_present || !before.pointer_stable ||
      b::temporal_non_sentinel_mask(before) != 0)
    return fail("sentinel baseline failed");
  for (std::uint32_t step = 1; step <= 9; ++step) {
    Fixture f;
    f.fail_on = step;
    const auto s = f.capture();
    if (s.status != b::TemporalStatus::read_failed || s.reads != step ||
        f.calls != step || b::temporal_non_sentinel_mask(s) != 0 ||
        b::temporal_initialization_changes(before, s) != 0)
      return fail("failed read was followed or partial IDs were interpreted");
  }
  Fixture missing;
  missing.root = 0;
  const auto no_manager = missing.capture();
  if (no_manager.status != b::TemporalStatus::complete ||
      no_manager.manager_present || no_manager.reads != 6)
    return fail("absent manager was dereferenced");
  Fixture changed;
  changed.changed = true;
  if (changed.capture().status != b::TemporalStatus::pointers_changed)
    return fail("replaced manager was accepted");
  for (int which = 0; which < 2; ++which) {
    Fixture f;
    (which ? f.mode57 : f.mode56) = 2;
    if (f.capture().status != b::TemporalStatus::invalid_values)
      return fail("invalid mode byte accepted");
  }
  Fixture initialized;
  initialized.ids = {0U, 0x10000U, 0xFFFFFFFFU, 0xFFFFU};
  auto after = initialized.capture();
  if (b::temporal_non_sentinel_mask(after) != 0xBU ||
      b::temporal_initialization_changes(before, after) != 0xBU ||
      b::temporal_initialization_changes(after, before) != 0U)
    return fail("cache identifiers were confused with the full-width sentinel");
  auto volatile_queue = after;
  ++volatile_queue.queue_count;
  if (!b::same_temporal_values(after, volatile_queue))
    return fail("queue fluctuation should not flood logs");
  volatile_queue.mode_56 = 1;
  if (b::same_temporal_values(after, volatile_queue))
    return fail("mode change suppressed");
  Fixture overflow;
  const auto bad = b::capture_temporal_snapshot(
      std::numeric_limits<std::uintptr_t>::max() - 16, &Fixture::read,
      &overflow);
  if (bad.status != b::TemporalStatus::read_failed || overflow.calls != 0)
    return fail("wrapped address read");
  std::cout << "Temporal cache and context snapshots passed\n";
}
