// The game-function stubs (temporal_entry.asm) call native(): each hook's
// before/after around the forwarded original. And AL's D3D12 chains.
#include "runtime_state.hpp"
#include "../game_memory/layout.hpp"

extern "C" void mhw_binding_forward(void *, mhw::projection_binding::Frame *);
namespace mhw::continuous_dlaa::detail {
namespace {
bool observe_prepared_resolve(const NativeFrame &f) {
    if (!sr::owns_packet(f.call.gpr[3])) return false;
    std::uintptr_t backend{}, recording{}, command{};
    if (!reader.get(f.call.gpr[2] + layout::executor_device::backend, backend) || backend != cfg.backend ||
        !reader.get(f.call.gpr[1] + layout::executor::recording, recording) || !recording ||
        !reader.get(recording + layout::recording::command, command) || !command)
        return false;
    Lock lock;
    auto *list = reinterpret_cast<ID3D12GraphicsCommandList *>(command);
    const auto *entry = find(list);
    return entry && !entry->closed && sr::observe_resolve(f.call.gpr[3], list, entry->generation);
}
void native_seh(unsigned kind, NativeFrame *f) {
    auto timing = perf::hook_timing::begin(perf::hook_timing::native_frame);
    bool admitted = false, returned = false;
    // Forwarding saves native RETURN registers into f->call. R8 is volatile;
    // preserve the entry packet independently for after-call scope cleanup.
    const auto prepared_packet = f->call.gpr[3];
    const bool prepared = kind == 1 && observe_prepared_resolve(*f);
    ForwardedRequest forwarded{};
    if (kind == 0)
        admitted = taa_before(*f);
    else if (kind == 1)
        admitted = consumer_before(*f, false, &forwarded);
    else if (kind == 2)
        admitted = depth_before(*f);
    else if (kind == 3)
        admitted = consumer_before(*f, true);
    else
        admitted = projection_before(*f);
    __try {
        perf::hook_timing::forward_begin(timing);
        mhw_binding_forward(originals[kind], &f->call);
        perf::hook_timing::forward_end(timing);
        returned = true;
    } __finally {
        if (prepared) sr::finish_resolve(prepared_packet, returned);
        if (kind == 0)
            taa_after(admitted, returned);
        else if (kind == 1 || kind == 3) {
            finish_forwarded(forwarded, returned);
            consumer_after(admitted, returned);
        } else if (kind == 2)
            depth_after(admitted, returned);
        else
            projection_after(admitted, returned, *f);
        perf::hook_timing::end(timing);
    }
}
} // namespace
void native(unsigned kind, NativeFrame *f) {
    pending.add();
    __try {
        native_seh(kind, f);
    } __finally {
        pending.sub();
    }
}
namespace {
template <auto Detour, unsigned I> using Chained = astera::Gate<Detour, chain_live, originals, I>;
struct ChainedMethod {
    unsigned index;
    astera::Family family;
    unsigned slot;
    void *detour;
};
const ChainedMethod chained_methods[]{
    {5, astera::command_list, 10, reinterpret_cast<void *>(&Chained<&reset_hook, 5>::call)},
    {6, astera::command_list, 9, reinterpret_cast<void *>(&Chained<&close_hook, 6>::call)},
    {7, astera::queue, 10, reinterpret_cast<void *>(&Chained<&execute_hook, 7>::call)},
    {8, astera::command_list, 26, reinterpret_cast<void *>(&Chained<&barrier_hook, 8>::call)},
    {9, astera::command_list, 14, reinterpret_cast<void *>(&Chained<&dispatch_hook, 9>::call)},
    {10, astera::command_list, 17, reinterpret_cast<void *>(&Chained<&copy_hook, 10>::call)},
    {11, astera::command_list, 12, reinterpret_cast<void *>(&Chained<&draw_hook, 11>::call)},
    {12, astera::command_list, 13, reinterpret_cast<void *>(&Chained<&indexed_hook, 12>::call)},
};
static_assert(std::size(chained_methods) == hook_count - first_api_hook, "every D3D12 hook is chained");
} // namespace
} // namespace mhw::continuous_dlaa::detail
extern "C" void mhw_temporal_dispatch(mhw::temporal_bindings::NativeFrame *f) {
    mhw::continuous_dlaa::detail::native(0, f);
}
extern "C" void mhw_temporal_consume(mhw::temporal_bindings::NativeFrame *f) {
    mhw::continuous_dlaa::detail::native(1, f);
}
extern "C" void mhw_temporal_depth_produce(mhw::temporal_bindings::NativeFrame *f) {
    mhw::continuous_dlaa::detail::native(2, f);
}
extern "C" void mhw_temporal_depth_consume(mhw::temporal_bindings::NativeFrame *f) {
    mhw::continuous_dlaa::detail::native(3, f);
}
extern "C" void mhw_temporal_jitter(mhw::temporal_bindings::NativeFrame *f) {
    mhw::continuous_dlaa::detail::native(4, f);
}
namespace mhw::continuous_dlaa {
using namespace detail;
void astera_hooks(std::vector<astera::Hook> &out) {
    for (const auto &m : chained_methods)
        out.push_back({astera::method(m.family, m.slot), m.detour, &originals[m.index]});
}
} // namespace mhw::continuous_dlaa
