#include "fake_astera.hpp"
#include "../third_party/al/al_plugin.h"
#include <MinHook.h>
#include <cstdlib>
#include <iostream>
#include <mutex>

namespace mhw::test::fake_astera {
namespace {
struct Entry {
    void *target{};
    al_hook_state_t state{AL_HOOK_FAILED};
};
std::mutex mutex;
std::vector<Entry> entries; // handle = index + 1
int plugin_token;
al_plugin_t *self() noexcept {
    return reinterpret_cast<al_plugin_t *>(&plugin_token);
}

// al.scanner syntax as AL's hooks use it: hex bytes, "??" for any byte.
bool matches(const char *pattern, const unsigned char *at) noexcept {
    const auto nibble = [](char c) {
        return c >= '0' && c <= '9'   ? c - '0'
               : c >= 'A' && c <= 'F' ? c - 'A' + 10
               : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                      : -1;
    };
    unsigned n{};
    for (const char *p = pattern; *p;) {
        if (*p == ' ') {
            ++p;
            continue;
        }
        if (p[0] == '?' && p[1] == '?') {
            p += 2;
            ++n;
            continue;
        }
        const int hi = nibble(p[0]), lo = hi < 0 ? -1 : nibble(p[1]);
        if (lo < 0 || at[n] != hi * 16 + lo) return false;
        p += 2;
        ++n;
    }
    return n >= 5;
}
std::uint32_t AL_CALL hook(al_plugin_t *plugin, const al_hook_t *h) {
    if (plugin != self() || !h || h->size < sizeof(al_hook_t) || !h->pattern || !h->address || !h->detour || !h->next)
        return 0;
    if (!matches(h->pattern, static_cast<const unsigned char *>(h->address) - h->offset)) return 0;
    std::lock_guard lock(mutex);
    for (const auto &e : entries)
        if (e.target == h->address && e.state != AL_HOOK_FAILED) return 0; // the same plugin twice
    if (MH_CreateHook(h->address, h->detour, h->next) != MH_OK) return 0;
    if (MH_EnableHook(h->address) != MH_OK) {
        MH_RemoveHook(h->address);
        *h->next = nullptr;
        return 0;
    }
    entries.push_back({h->address, AL_HOOK_ACTIVE});
    return static_cast<std::uint32_t>(entries.size());
}
al_result_t AL_CALL set_enabled(al_plugin_t *plugin, std::uint32_t handle, std::uint32_t enabled) {
    std::lock_guard lock(mutex);
    if (plugin != self() || !handle || handle > entries.size() || entries[handle - 1].state == AL_HOOK_FAILED)
        return AL_ERROR;
    auto &e = entries[handle - 1];
    const auto status = enabled ? MH_EnableHook(e.target) : MH_DisableHook(e.target);
    if (status != MH_OK && status != MH_ERROR_ENABLED && status != MH_ERROR_DISABLED) return AL_ERROR;
    e.state = enabled ? AL_HOOK_ACTIVE : AL_HOOK_DISABLED;
    return AL_OK;
}
al_hook_state_t AL_CALL state(al_plugin_t *plugin, std::uint32_t handle) {
    std::lock_guard lock(mutex);
    return plugin == self() && handle && handle <= entries.size() ? entries[handle - 1].state : AL_HOOK_FAILED;
}
const al_hooks_t table{sizeof(al_hooks_t), AL_HOOKS_VERSION, hook, set_enabled, state};
// AL marks the calling thread's DXGI/D3D12 calls internal; nothing to do here.
void begin_internal(void *) {}
void end_internal(void *) {}
} // namespace

void start() {
    const auto status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        std::cerr << "fake Astera Loader: MinHook\n";
        std::exit(1);
    }
    astera::activate(begin_internal, end_internal, self());
    astera::provide_game_hooks(&table, self());
}
bool install_methods(const std::vector<astera::Hook> &hooks, const Objects &objects) {
    for (const auto &h : hooks) {
        void *object{};
        switch (h.method >> 16) {
        case astera::factory: object = objects.factory; break;
        case astera::swapchain: object = objects.swapchain; break;
        case astera::device: object = objects.device; break;
        case astera::queue: object = objects.queue; break;
        case astera::command_list: object = objects.command_list; break;
        default: return false;
        }
        if (!object || !h.detour || !h.next) return false;
        void *target = (*static_cast<void ***>(object))[h.method & 0xffff];
        if (MH_CreateHook(target, h.detour, h.next) != MH_OK || MH_EnableHook(target) != MH_OK) return false;
    }
    return true;
}
unsigned active_game_hooks() {
    std::lock_guard lock(mutex);
    unsigned n{};
    for (const auto &e : entries) n += e.state == AL_HOOK_ACTIVE;
    return n;
}
} // namespace mhw::test::fake_astera
