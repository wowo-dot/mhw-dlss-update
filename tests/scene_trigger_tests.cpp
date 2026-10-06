#include "scene_trigger.hpp"
#include <windows.h>
#include <iostream>
#include <stdexcept>
namespace b = mhw::bridge_probe;
void check(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
int main() {
  try {
    FILETIME c{}, x{}, k{}, u{};
    check(GetProcessTimes(GetCurrentProcess(), &c, &x, &k, &u) != 0, "process generation");
    const auto generation = (static_cast<std::uint64_t>(c.dwHighDateTime) << 32) | c.dwLowDateTime;
    const auto name = b::scene_trigger_name(GetCurrentProcessId(), generation);
    b::SceneTrigger owner, collision;
    check(!owner.consume() && owner.create() && !owner.create(), "single owned event");
    check(!collision.create(), "pre-existing event rejected");
    check(!owner.consume(), "no initial signal");
    check(OpenEventW(EVENT_MODIFY_STATE, FALSE, b::scene_trigger_name(GetCurrentProcessId(), generation+1).c_str()) == nullptr,
          "wrong generation has no event");
    auto writer = OpenEventW(EVENT_MODIFY_STATE, FALSE, name.c_str());
    check(writer && SetEvent(writer), "external scalar-capture request");
    check(owner.consume() && !owner.consume(), "auto-reset consumes once");
    CloseHandle(writer); owner.close(); owner.close();
    check(!owner.consume() && OpenEventW(EVENT_MODIFY_STATE, FALSE, name.c_str()) == nullptr, "close removes event");
    {
      b::SceneTrigger scoped; check(scoped.create(), "scope recreation");
    }
    check(OpenEventW(EVENT_MODIFY_STATE, FALSE, name.c_str()) == nullptr, "RAII releases handle");
    std::cout << "Scene trigger identity, collision, one-shot and lifetime checks passed\n";
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
