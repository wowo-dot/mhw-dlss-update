#pragma once
#include "projection_snapshot.hpp"
#include <cstdint>
#include <string>

namespace mhw::jitter_activation {
inline constexpr unsigned capacity=3, window_ms=1500, visit_limit=256;
inline constexpr char host_sha[]="c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea";
enum class Stop : unsigned { idle, recording, complete, requested, deadline, invalid, visits };
struct ProviderState {
  std::uintptr_t backend{}, wrapper{};
  unsigned use_dlss{}, sizing_flags{};
  bool valid{};
  bool operator==(const ProviderState &) const = default;
};
struct Call {
  unsigned stage{}, thread{}, rejection{};
  unsigned view_flags{}, projection_active{}, display_index{}, camera_stamp{}, admission_checkpoint{};
  std::uint64_t tick{}, exit_tick{}, original_jitter{}, candidate{}, restored_jitter{};
  std::uintptr_t camera{}, camera_type{}, projection_object{}, caller{};
  bridge_probe::ProjectionAnchor anchor{};
  bridge_probe::ProjectionSnapshot projection{};
  ProviderState provider_before{}, provider_after{};
  bool injected{}, restored{}, returned{}, result{}, complete{};
};
struct State {
  Stop stop{};
  unsigned reserved{}, pending{}, visits{}, dropped{}, injections{}, restorations{};
  std::uint64_t started{};
};
struct InstallState {
  unsigned stage{}; // 0 not attempted, 1 initialize, 2 create, 3 enable
  int status{};
  unsigned win32_error{}; // Observed LastError, not a MinHook error contract.
  bool hook_created{}, hook_enabled{};
};
InstallState install_state() noexcept;
const char *install_status_name() noexcept;
bool host_identity(std::uintptr_t base,std::string &reason);
std::string file_sha256(const std::wstring &path);
bool install_verified(std::uintptr_t base,std::string &reason);
bool arm(std::uint64_t tick=0) noexcept;
void stop() noexcept;
void poll(std::uint64_t tick=0) noexcept;
State state() noexcept;
bool settled() noexcept;
bool copy_call(unsigned index,Call &out) noexcept;
bool disable() noexcept;
#ifdef MHW_ACTIVATION_OWNED_FIXTURE
bool install_owned(std::uintptr_t target,std::uintptr_t base) noexcept;
bool remove_owned() noexcept;
#endif
}
