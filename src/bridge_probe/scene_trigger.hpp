#pragma once
#include <cstdint>
#include <string>

namespace mhw::bridge_probe {
std::wstring scene_trigger_name(unsigned pid, std::uint64_t creation);
// Worker-owned, process-generation-scoped event. It requests scalar observation
// only; it never grants another rendering admission or resets a sample budget.
class SceneTrigger {
public:
  SceneTrigger() = default;
  ~SceneTrigger() { close(); }
  SceneTrigger(const SceneTrigger &) = delete;
  SceneTrigger &operator=(const SceneTrigger &) = delete;
  bool create() noexcept;
  bool consume() noexcept;
  void close() noexcept;
private:
  void *event_{};
};
}
