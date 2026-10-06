#pragma once

#include "bridge_policy.hpp"
#include "contract_snapshot.hpp"

namespace mhw::bridge_probe {

// The pinned core copies this packed value into its own Create record and
// dispatches by the high DWORD. Only DLSS may issue it in this milestone.
constexpr std::uint64_t diagnostic_token(ProviderId provider) noexcept {
  return provider == ProviderId::count
             ? 0U
             : (static_cast<std::uint64_t>(feature_id(provider)) << 32U) |
                   0xB12DU;
}
constexpr Mode provider_mode(Mode requested, ProviderId provider) noexcept {
  if (provider == ProviderId::count)
    return Mode::disabled;
  return evaluate_limit(requested) != 0 && provider == ProviderId::isp
             ? Mode::capture_create
             : requested;
}

// Keep sizing available while the configured diagnostic token can be used.
// Three-Evaluate mode also admits queries between bounded Evaluate captures.
bool sizing_lifecycle_ready(Mode mode, LifecycleState state,
                            bool token_outstanding) noexcept;

struct CoreCreateRecord {
  std::uint32_t operation{};
  std::uint32_t reserved{};
  std::uint64_t parameter_clone{};
  std::uint64_t creation_command_list{};
  std::uint64_t token{};
};
static_assert(sizeof(CoreCreateRecord) == 32U);
static_assert(offsetof(CoreCreateRecord, token) == 24U);

// Read only the one bounded record immediately preceding the core's handle.
// Runtime callers must first verify the exact core caller and outstanding
// token.
bool core_create_record_matches(
    const CoreCreateRecord &record, std::uint64_t expected_token,
    std::uint64_t expected_creation_command_list,
    Mode mode = Mode::capture_one_evaluate) noexcept;
constexpr bool copied_handle_address_matches(std::uintptr_t incoming,
                                             std::uintptr_t original,
                                             std::uintptr_t bound) noexcept {
  return incoming != 0U && incoming != original &&
         (bound == 0U || incoming == bound);
}

// Require the nine keys seen in accepted run 19800. ResourceWidth/Height were
// absent there and are not prerequisites. This does not validate resources.
bool create_contract_allows_evaluate(
    const CreateContractSnapshot &snapshot) noexcept;

} // namespace mhw::bridge_probe
