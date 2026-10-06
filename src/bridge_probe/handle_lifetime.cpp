#include "handle_lifetime.hpp"

namespace mhw::bridge_probe {
bool HandleLifetime::issue(Mode mode, std::uint64_t token, std::uintptr_t command_list,
                           HandleWriter writer, void *context) noexcept {
  const std::lock_guard lock(mutex_);
  const bool fallback = reusable_fallback_mode(mode);
  const bool token_valid = fallback ? is_fallback_generation_token(token)
      : (mode == Mode::capture_one_evaluate || mode == Mode::capture_three_evaluates) &&
        token == diagnostic_token(ProviderId::dlss);
  if (state_.closed || state_.read_faulted || state_.token || !command_list || !writer || !token_valid ||
      (state_.last_token && (!fallback || mode_ != mode || token <= state_.last_token))) return false;
  mode_ = mode;
  token_storage_ = token;
  state_.last_token = token;
  if (!writer(context, &token_storage_)) { state_.closed = true; return false; }
  state_.token = token;
  state_.command_list = command_list;
  state_.copied_handle = 0;
  ++state_.creates;
  return true;
}

std::uint64_t HandleLifetime::read_locked(std::uintptr_t handle, HandleReader reader,
                                         void *context, HandleEvidence &evidence) noexcept {
  constexpr auto prefix = offsetof(CoreCreateRecord, token);
  if (state_.closed || state_.read_faulted || !state_.token || !reader || handle < prefix ||
      handle % alignof(std::uint64_t) != 0 ||
      !copied_handle_address_matches(handle, reinterpret_cast<std::uintptr_t>(&token_storage_), state_.copied_handle)) return 0;
  CoreCreateRecord record{};
  evidence.read_attempted = true;
  const auto result = reader(context, handle - prefix, record);
  if (result != HandleRecordRead::success) {
    if (result == HandleRecordRead::fault) state_.read_faulted = true;
    return 0;
  }
  evidence.read_completed = true;
  evidence.record_verified = core_create_record_matches(record, state_.token, state_.command_list, mode_);
  if (!evidence.record_verified) return 0;
  state_.copied_handle = handle;
  evidence.binding_matched = true;
  return state_.token;
}
std::uint64_t HandleLifetime::read(std::uintptr_t handle, HandleReader reader, void *context,
                                  HandleEvidence &evidence) noexcept {
  const std::lock_guard lock(mutex_);
  return read_locked(handle, reader, context, evidence);
}
void HandleLifetime::retire_locked() noexcept {
  state_.token = 0;
  state_.command_list = state_.copied_handle = 0;
}
std::uint64_t HandleLifetime::release(std::uintptr_t handle, HandleReader reader, void *context,
                                     HandleEvidence &evidence) noexcept {
  const std::lock_guard lock(mutex_);
  const auto token = read_locked(handle, reader, context, evidence);
  if (token) { retire_locked(); ++state_.releases; }
  return token;
}
void HandleLifetime::cancel(std::uint64_t token) noexcept {
  const std::lock_guard lock(mutex_);
  if (token && state_.token == token) { retire_locked(); state_.closed = true; }
}
void HandleLifetime::close() noexcept {
  const std::lock_guard lock(mutex_);
  retire_locked(); state_.closed = true;
}
HandleLifetimeStats HandleLifetime::stats() const noexcept {
  const std::lock_guard lock(mutex_);
  return state_;
}
} // namespace mhw::bridge_probe
