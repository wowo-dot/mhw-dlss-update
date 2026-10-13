#include "astera_hooks.hpp"
#include "../../third_party/al/al_plugin.h"
#include <atomic>

namespace mhw::astera {
namespace {
std::atomic<bool> running{};
void (*begin_call)(void *){};
void (*end_call)(void *){};
void *call_context{};
std::atomic<const al_hooks_t *> game_table{};
al_plugin_t *game_self{};
}
void activate(void (*begin)(void *),void (*end)(void *),void *context) noexcept {
    begin_call=begin;end_call=end;call_context=context;running.store(begin&&end,std::memory_order_release);
}
bool active() noexcept {return running.load(std::memory_order_acquire);}
thread_local unsigned internal_depth{};
void begin_internal() noexcept {++internal_depth;if(active())begin_call(call_context);}
void end_internal() noexcept {if(active())end_call(call_context);--internal_depth;}
bool internal_call() noexcept {return internal_depth!=0;}

void provide_game_hooks(const void *table,void *self) noexcept {
    const auto *hooks=static_cast<const al_hooks_t *>(table);
    if(!hooks||!self||hooks->size<sizeof(al_hooks_t))return;
    game_self=static_cast<al_plugin_t *>(self);game_table.store(hooks,std::memory_order_release);
}
bool game_hooks_available() noexcept {return game_table.load(std::memory_order_acquire)!=nullptr;}
std::uint32_t game_hook(const GameHook &hook) noexcept {
    const auto *hooks=game_table.load(std::memory_order_acquire);
    if(!hooks)return 0;
    const al_hook_t request{sizeof(al_hook_t),hook.order,hook.name,hook.pattern,0,0,hook.address,hook.detour,hook.next};
    return hooks->hook(game_self,&request);
}
bool set_game_hook_enabled(std::uint32_t handle,bool enabled) noexcept {
    const auto *hooks=game_table.load(std::memory_order_acquire);
    return hooks&&handle&&hooks->set_enabled(game_self,handle,enabled?1u:0u)==AL_OK;
}
GameHookState game_hook_state(std::uint32_t handle) noexcept {
    const auto *hooks=game_table.load(std::memory_order_acquire);
    if(!hooks||!handle)return GameHookState::failed;
    const auto state=hooks->state(game_self,handle);
    return state<=AL_HOOK_FAILED?static_cast<GameHookState>(state):GameHookState::failed;
}
}
