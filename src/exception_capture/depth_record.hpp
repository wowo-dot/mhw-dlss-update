#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace mhw::exception_capture {

using Address = std::uint64_t;
using Read = bool (*)(void *, Address, std::span<std::byte>);
struct Field {
  std::string name;
  std::optional<std::uint64_t> value;
};
struct Snapshot {
  std::vector<Field> fields;
  unsigned reads{};
  bool descriptor_verified{};
  bool read_failed{};
  bool budget_exhausted{};
};

// All offsets refer to the exact host identity validated by the executable.
// The reader owns no game object and performs no remote calls or writes.
Snapshot capture_record(Read read, void *context, Address base, Address payload,
                        Address descriptor, bool include_main_view);

} // namespace mhw::exception_capture
