#include "cmfm_hooks.hpp"
#include <atomic>

namespace mhw::cmfm {
namespace {
std::atomic<bool> running{};
void (*begin_call)(void *){};
void (*end_call)(void *){};
void *call_context{};
}
void activate(void (*begin)(void *),void (*end)(void *),void *context) noexcept {
    begin_call=begin;end_call=end;call_context=context;running.store(begin&&end,std::memory_order_release);
}
bool active() noexcept {return running.load(std::memory_order_acquire);}
void begin_internal() noexcept {if(active())begin_call(call_context);}
void end_internal() noexcept {if(active())end_call(call_context);}
}
