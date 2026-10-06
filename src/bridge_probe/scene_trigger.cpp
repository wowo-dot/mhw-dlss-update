#include "scene_trigger.hpp"
#include <windows.h>

namespace mhw::bridge_probe {
std::wstring scene_trigger_name(unsigned pid, std::uint64_t creation) {
  return L"Local\\MhwDlssJitterScene-v1-" + std::to_wstring(pid) + L"-" + std::to_wstring(creation);
}
bool SceneTrigger::create() noexcept {
  if (event_) return false;
  try {
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) return false;
    const auto generation = (static_cast<std::uint64_t>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
    const auto name = scene_trigger_name(GetCurrentProcessId(), generation);
    const auto candidate = CreateEventW(nullptr, FALSE, FALSE, name.c_str());
    const auto error = GetLastError();
    if (!candidate) return false;
    if (error == ERROR_ALREADY_EXISTS) { CloseHandle(candidate); return false; }
    event_ = candidate;
    return true;
  } catch (...) { return false; }
}
bool SceneTrigger::consume() noexcept {
  return event_ && WaitForSingleObject(event_, 0) == WAIT_OBJECT_0;
}
void SceneTrigger::close() noexcept {
  if (event_) { CloseHandle(event_); event_ = nullptr; }
}
} // namespace mhw::bridge_probe
