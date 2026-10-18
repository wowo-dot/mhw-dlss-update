// al.hooks before PRE_MAIN ends: a hook registered then is pending until it
// ends, and settled_game_hook_state waits for it (the NGX startup hooks, which
// the worker registers while other plugins' PRE_MAIN work still runs).
#include "../src/astera/astera_hooks.hpp"
#include "../third_party/al/al_plugin.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>

namespace {
void check(bool ok, const char *what) {
    if (!ok) {
        std::fprintf(stderr, "failed: %s\n", what);
        std::exit(1);
    }
}
int plugin_token;
std::atomic_uint polls{}, pending_polls{};
std::uint32_t AL_CALL hook(al_plugin_t *, const al_hook_t *) {
    return 0;
}
al_result_t AL_CALL set_enabled(al_plugin_t *, std::uint32_t, std::uint32_t) {
    return AL_ERROR;
}
// Handle 1 is pending for its first pending_polls polls, then active (PRE_MAIN
// ended); handle 2 stays pending; any other is failed, as AL answers.
al_hook_state_t AL_CALL state(al_plugin_t *, std::uint32_t handle) {
    if (handle == 1) return ++polls > pending_polls ? AL_HOOK_ACTIVE : AL_HOOK_PENDING;
    return handle == 2 ? AL_HOOK_PENDING : AL_HOOK_FAILED;
}
const al_hooks_t table{sizeof(al_hooks_t), AL_HOOKS_VERSION, hook, set_enabled, state};
} // namespace

int main() {
    namespace astera = mhw::astera;
    using State = astera::GameHookState;
    astera::provide_game_hooks(&table, &plugin_token);

    pending_polls = 3;
    check(astera::settled_game_hook_state(1, 60000) == State::active && polls == 4,
          "a pending hook is waited for until AL makes it active");
    polls = 0;
    pending_polls = 0;
    check(astera::settled_game_hook_state(1, 0) == State::active && polls == 1, "an active hook returns at once");
    const auto began = GetTickCount64();
    check(astera::settled_game_hook_state(2, 50) == State::pending && GetTickCount64() - began >= 50,
          "still pending after the limit: pending");
    check(astera::settled_game_hook_state(3, 60000) == State::failed, "a failed hook returns at once");
    check(astera::settled_game_hook_state(0, 60000) == State::failed, "no handle: failed");
    return 0;
}
