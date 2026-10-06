#pragma once
#include "evaluate_gate.hpp"
#include <mutex>

namespace mhw::bridge_probe {
enum class HandleRecordRead { success, rejected, fault };
using HandleReader = HandleRecordRead (*)(void *, std::uintptr_t, CoreCreateRecord &) noexcept;
using HandleWriter = bool (*)(void *, const std::uint64_t *) noexcept;
struct HandleEvidence {
  bool read_attempted{}, read_completed{}, record_verified{}, binding_matched{};
};
struct HandleLifetimeStats {
  std::uint64_t token{}, last_token{}, creates{}, releases{};
  std::uintptr_t command_list{}, copied_handle{};
  bool closed{}, read_faulted{};
};

// The caller must verify its ABI/caller/host before entering. One lock covers
// publication, bounded record reads, retirement and Shutdown. Readers cannot
// outlive a successful release/close. No resource or foreign pointer is owned.
class HandleLifetime final {
public:
  bool issue(Mode mode, std::uint64_t token, std::uintptr_t command_list,
             HandleWriter writer, void *context) noexcept;
  std::uint64_t read(std::uintptr_t handle, HandleReader reader, void *context,
                     HandleEvidence &evidence) noexcept;
  std::uint64_t release(std::uintptr_t handle, HandleReader reader, void *context,
                        HandleEvidence &evidence) noexcept;
  void cancel(std::uint64_t token) noexcept;
  void close() noexcept;
  HandleLifetimeStats stats() const noexcept;
private:
  std::uint64_t read_locked(std::uintptr_t handle, HandleReader reader, void *context,
                            HandleEvidence &evidence) noexcept;
  void retire_locked() noexcept;
  mutable std::mutex mutex_;
  HandleLifetimeStats state_{};
  Mode mode_{Mode::disabled};
  std::uint64_t token_storage_{};
};
} // namespace mhw::bridge_probe
