#include "presentation.hpp"
#include "../perf/frame_stats.hpp"
#include "owned_interface.hpp"
#include "resize_contract.hpp"
#include "presentation_route.hpp"
#include "presentation_ownership.hpp"
#include "identity_chain.hpp"
#include "../astera/astera_hooks.hpp"
#include <MinHook.h>
#include <wrl/client.h>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <cstring>
#include <intrin.h>
#include <ostream>
#include <vector>

namespace mhw::reflex {
namespace {
using Microsoft::WRL::ComPtr;
thread_local unsigned bypass_depth{};
thread_local bool routed_call{};  // Set when route() sent the call through the SDK proxy.
// `internal` also marks the thread's DXGI/D3D12 calls internal for AL: the
// SDK's own creation calls then reach the original methods once, past every
// plugin. Present routing keeps plain bypass: the SDK's native Present inside
// the proxy's must still reach present_hook, which counts it (Capture).
struct Bypass {
    bool internal{};
    explicit Bypass(bool marks_internal = false) : internal(marks_internal) { ++bypass_depth; if (internal) astera::begin_internal(); }
    ~Bypass() { if (internal) astera::end_internal(); --bypass_depth; }
};
struct NativePresent {
    IUnknown* swap{}; unsigned calls{}; HRESULT result{E_FAIL}; NativePresent* previous{};
};
thread_local NativePresent* current_present{};
struct Capture {
    NativePresent value;
    explicit Capture(IUnknown* swap) { value.swap = swap; value.previous = current_present; current_present = &value; }
    ~Capture() { current_present = value.previous; }
};
HRESULT record_present(IUnknown* swap, HRESULT result) {
    if (current_present && current_present->swap == swap) { ++current_present->calls; current_present->result = result; }
    return result;
}
// The game's latency waitable slot (fg-live-toggle.md): reads it, or replaces
// `expected` with `value` atomically. False if unreadable or not `expected`.
bool slot_handle(std::uintptr_t slot, HANDLE& out) noexcept {
    if (!slot || slot % alignof(HANDLE)) return false;
    __try { out = *reinterpret_cast<HANDLE volatile*>(slot); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool replace_slot_handle(std::uintptr_t slot, HANDLE expected, HANDLE value) noexcept {
    if (!slot || slot % alignof(HANDLE)) return false;
    __try { return InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(slot), value, expected) == expected; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
std::atomic<Presentation::State*> dispatch{};
// AL: where AL stores each detour's continuation (see astera_hooks.hpp).
std::array<void*, Presentation::hook_capacity> chain_next{};
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void api(sl::Result value, const char* why) { require(value == sl::Result::eOk, why); }
template<class T> ComPtr<T> native(Streamline& sdk, IUnknown* proxy) {
    void* value{}; api(sdk.native(proxy, &value), "presentation-native-interface");
    require(value != nullptr, "presentation-native-null");
    ComPtr<IUnknown> owned; owned.Attach(static_cast<IUnknown*>(value));
    ComPtr<T> typed; require(SUCCEEDED(owned.As(&typed)), "presentation-native-type");
    return typed;
}
bool image_function(void* address) {
    MEMORY_BASIC_INFORMATION info{};
    if (!address || VirtualQuery(address, &info, sizeof(info)) != sizeof(info)) return false;
    const auto protection = info.Protect & 0xffU;
    return info.Type == MEM_IMAGE && info.State == MEM_COMMIT &&
        !(info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
        (protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY);
}
using QueueFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_COMMAND_QUEUE_DESC*, REFIID, void**);
using SwapFn = HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
using HwndFn = HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
using CoreFn = HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*, IUnknown*, IUnknown*, const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**);
using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using BufferFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, REFIID, void**);
using FullFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, BOOL, IDXGIOutput*);
using DescFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, DXGI_SWAP_CHAIN_DESC*);
using ResizeFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using Present1Fn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
using IndexFn = UINT(STDMETHODCALLTYPE*)(IDXGISwapChain3*);
using Resize1Fn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT*, IUnknown* const*);
using TargetFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, const DXGI_MODE_DESC*);
// v0.27.4: the identity chain's other routed methods (identity_chain.hpp).
using GetFullFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, BOOL*, IDXGIOutput**);
using OutputFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, IDXGIOutput**);
using StatsFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, DXGI_FRAME_STATISTICS*);
using CountFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT*);
using Desc1Fn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*, DXGI_SWAP_CHAIN_DESC1*);
using FullDescFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*, DXGI_SWAP_CHAIN_FULLSCREEN_DESC*);
using HwndGetFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*, HWND*);
using SetLatencyFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain2*, UINT);
using GetLatencyFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain2*, UINT*);
using WaitableFn = HANDLE(STDMETHODCALLTYPE*)(IDXGISwapChain2*);
using ColorCheckFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*, DXGI_COLOR_SPACE_TYPE, UINT*);
using ColorSetFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*, DXGI_COLOR_SPACE_TYPE);
using HdrFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain4*, DXGI_HDR_METADATA_TYPE, UINT, void*);
HRESULT STDMETHODCALLTYPE queue_hook(ID3D12Device*, const D3D12_COMMAND_QUEUE_DESC*, REFIID, void**);
HRESULT STDMETHODCALLTYPE swap_hook(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
HRESULT STDMETHODCALLTYPE hwnd_hook(IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
HRESULT STDMETHODCALLTYPE core_hook(IDXGIFactory2*, IUnknown*, IUnknown*, const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*, IDXGISwapChain1**);
HRESULT STDMETHODCALLTYPE present_hook(IDXGISwapChain*, UINT, UINT);
HRESULT STDMETHODCALLTYPE buffer_hook(IDXGISwapChain*, UINT, REFIID, void**);
HRESULT STDMETHODCALLTYPE full_hook(IDXGISwapChain*, BOOL, IDXGIOutput*);
HRESULT STDMETHODCALLTYPE desc_hook(IDXGISwapChain*, DXGI_SWAP_CHAIN_DESC*);
HRESULT STDMETHODCALLTYPE resize_hook(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
HRESULT STDMETHODCALLTYPE present1_hook(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
UINT STDMETHODCALLTYPE index_hook(IDXGISwapChain3*);
HRESULT STDMETHODCALLTYPE resize1_hook(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT*, IUnknown* const*);
HRESULT STDMETHODCALLTYPE target_hook(IDXGISwapChain*, const DXGI_MODE_DESC*);
HRESULT STDMETHODCALLTYPE get_full_hook(IDXGISwapChain*, BOOL*, IDXGIOutput**);
HRESULT STDMETHODCALLTYPE output_hook(IDXGISwapChain*, IDXGIOutput**);
HRESULT STDMETHODCALLTYPE stats_hook(IDXGISwapChain*, DXGI_FRAME_STATISTICS*);
HRESULT STDMETHODCALLTYPE count_hook(IDXGISwapChain*, UINT*);
HRESULT STDMETHODCALLTYPE desc1_hook(IDXGISwapChain1*, DXGI_SWAP_CHAIN_DESC1*);
HRESULT STDMETHODCALLTYPE full_desc_hook(IDXGISwapChain1*, DXGI_SWAP_CHAIN_FULLSCREEN_DESC*);
HRESULT STDMETHODCALLTYPE hwnd_get_hook(IDXGISwapChain1*, HWND*);
HRESULT STDMETHODCALLTYPE set_latency_hook(IDXGISwapChain2*, UINT);
HRESULT STDMETHODCALLTYPE get_latency_hook(IDXGISwapChain2*, UINT*);
HANDLE STDMETHODCALLTYPE waitable_hook(IDXGISwapChain2*);
HRESULT STDMETHODCALLTYPE color_check_hook(IDXGISwapChain3*, DXGI_COLOR_SPACE_TYPE, UINT*);
HRESULT STDMETHODCALLTYPE color_set_hook(IDXGISwapChain3*, DXGI_COLOR_SPACE_TYPE);
HRESULT STDMETHODCALLTYPE hdr_hook(IDXGISwapChain4*, DXGI_HDR_METADATA_TYPE, UINT, void*);
}
struct Presentation::State {
    explicit State(Streamline& value) : sdk(value) {}
    Streamline& sdk;
    mutable std::mutex mutex;
    std::atomic<unsigned> active{};
    std::atomic<bool> accepting{};
    PresentationStats stats;
    PresentationObserver observer;
    PresentationOwnership ownership;
    std::uintptr_t diagnostic_image{};
    unsigned diagnostic_image_bytes{};
    PresentationContract contract{PresentationContract::synchronous_native};
    std::atomic<std::uintptr_t> paced_host_swap{};
    std::atomic<unsigned> paced_host_thread{};
    std::uintptr_t creation_queue{};
    unsigned device_nodes{};
    std::array<PresentationHook, hook_capacity> heads{};
    std::array<void*, hook_capacity> original{};
    LifecycleListener listener;
    std::atomic<bool> listener_ready{};  // Published after listener is complete.
    // Lifecycle ring: its own lock, never held across a driver or SDK call.
    SRWLOCK lifecycle_lock = SRWLOCK_INIT;
    std::array<LifecycleEvent, lifecycle_capacity> lifecycle_ring{};
    std::uint64_t lifecycle_written{}, lifecycle_read{}, lifecycle_dropped{};
    void lifecycle_record(LifecycleEvent e) noexcept {
        AcquireSRWLockExclusive(&lifecycle_lock);
        if (lifecycle_written - lifecycle_read == lifecycle_capacity) { ++lifecycle_dropped; ++lifecycle_read; }
        e.sequence = ++lifecycle_written;
        lifecycle_ring[(e.sequence - 1) % lifecycle_capacity] = e;
        ReleaseSRWLockExclusive(&lifecycle_lock);
    }
    ComPtr<ID3D12Device> device, proxy_device;
    ComPtr<IDXGIFactory2> factory, proxy_factory;
    struct Queue { ComPtr<ID3D12CommandQueue> proxy; ID3D12CommandQueue* native{}; };
    std::array<Queue, 16> queues{};
    // v0.28.6 (queue_admission): set once the game's chain is published; the
    // queues made after it (counted, first callers kept) take no route mutex.
    std::atomic<bool> chain_made{};
    std::atomic<std::uint64_t> late_queues{};
    std::array<std::atomic<std::uintptr_t>, 4> late_callers{};
    void late_queue(std::uintptr_t caller) noexcept {
        if (const auto n = late_queues.fetch_add(1); n < late_callers.size()) late_callers[static_cast<std::size_t>(n)].store(caller);
    }
    ComPtr<IDXGISwapChain3> swap, proxy_swap;
    bool stopped{}, released{};
    // v0.27.4 (fg-live-toggle.md): the identity chain the game holds (a
    // composition chain, never presented; identity_key publishes it to the
    // hooks) while `swap`/`proxy_swap` is the real chain on its window, which
    // switch_chain() releases and makes again with or without DLSS-G.
    ComPtr<IDXGISwapChain3> identity;
    std::atomic<std::uintptr_t> identity_key{};
    struct Creation {
        HWND hwnd{};
        DXGI_SWAP_CHAIN_DESC1 desc{};
        bool has_full{};
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC full{};
        ComPtr<IDXGIOutput> restrict_to;
        IUnknown* queue{};  // the game's queue (native)
    } creation;
    // What the game set through the identity chain, set again on a new real chain.
    struct Applied {
        bool latency{}, color{}, hdr{};
        UINT latency_value{};
        DXGI_COLOR_SPACE_TYPE color_value{};
        DXGI_HDR_METADATA_TYPE hdr_type{};
        std::vector<std::byte> hdr_data;
    } applied;
    // The latency waitable handed to the game, and where the game keeps it
    // (waitable_slot(); 0 unknown).
    HANDLE game_waitable{};
    std::uintptr_t waitable_slot{};
    // switch_chain() requests: 0 none, 1 without DLSS-G, 2 with (outcomes in stats).
    std::atomic<unsigned> switch_request{};
    bool is_identity(IUnknown* value) const noexcept { return identity && value == identity.Get(); }
    // Where a native call for `self` goes: the real chain for the identity chain.
    // (T: IDXGISwapChain3 or one of its bases.)
    template<class T> T* native_of(T* self) const noexcept {
        return is_identity(self) && swap ? static_cast<T*>(swap.Get()) : self;
    }
    // Scene diagnostic only: CPU labels, no retained buffers. Writers already
    // hold the route mutex; readers take only this lock.
    SRWLOCK scene_lock = SRWLOCK_INIT;
    mhw::frame_generation::PresentationBufferMap scene_buffers;
    template<class Fn> void scene(Fn&& fn) noexcept {
        AcquireSRWLockExclusive(&scene_lock); fn(scene_buffers); ReleaseSRWLockExclusive(&scene_lock);
    }
    // One index query on the routed host chain before the SDK/native Present.
    // Synchronous Reflex-only contract; the SDK-paced FG contract is excluded.
    void scene_present_entry(IUnknown* self) noexcept {
        if (contract != PresentationContract::synchronous_native || !proxy_swap) return;
        const auto index = proxy_swap->GetCurrentBackBufferIndex();
        scene([&](auto& map) { map.present(reinterpret_cast<std::uintptr_t>(self), index); });
    }

    void record(OwnershipCall call) noexcept {
        if(!diagnostic_image)return;
        const auto error=GetLastError(); LARGE_INTEGER qpc{};
        QueryPerformanceCounter(&qpc);
        call.qpc=static_cast<std::uint64_t>(qpc.QuadPart);
        call.thread=GetCurrentThreadId();ownership.record(call);
        SetLastError(error);
    }

    // A fault: counted with where, its HRESULT, why and the thread, and
    // routing ends only where the game can render into the chain it holds
    // (fault_keeps_routing). v0.28.6, tifa-6: a queue-table fault used to end
    // it with the identity chain too, and the game's next Present reached the
    // identity chain itself (never presented): DXGI_ERROR_INVALID_CALL, abort.
    void fault(FaultSite site, HRESULT hr, const char* why) noexcept {
        ++stats.faults; stats.fault_site = static_cast<std::uint32_t>(site); stats.fault_hr = static_cast<std::uint32_t>(hr);
        stats.fault_thread = GetCurrentThreadId(); stats.fault_why.fill('\0');
        if (why) strncpy_s(stats.fault_why.data(), stats.fault_why.size(), why, _TRUNCATE);
        if (!fault_keeps_routing(contract, identity.Get() != nullptr)) accepting.store(false);
    }
    // Consecutive failed paced Presents (paced_present_result).
    unsigned paced_failures{};
    HRESULT paced(HRESULT decided, HRESULT sdk_result) noexcept {
        paced_failures = FAILED(sdk_result) ? paced_failures + 1 : 0;
        return paced_present_result(decided, sdk_result, paced_failures);
    }
    // In a catch block: the fault with the exception's what().
    void fault_exception(FaultSite site) noexcept {
        try { throw; }
        catch (const std::exception& e) { fault(site, E_FAIL, e.what()); }
        catch (...) { fault(site, E_UNEXPECTED, "non-standard exception"); }
    }
    void add(unsigned index, IUnknown* object, unsigned slot, void* detour) {
        auto** table = *reinterpret_cast<void***>(object);
        auto* target = table[slot]; require(image_function(target), "presentation-native-code");
        auto& head = heads[index];
        if (head.created) { require(head.target == reinterpret_cast<std::uintptr_t>(target), "presentation-vtable-changed"); return; }
        for (const auto& prior : heads) require(prior.target != reinterpret_cast<std::uintptr_t>(target), "presentation-aliased-method");
        head.target = reinterpret_cast<std::uintptr_t>(target);
        // Under AL the method is patched by AL, whose chain already holds
        // this detour; nothing is patched or compared here.
        if (!astera::active()) {
            std::memcpy(head.original.data(), target, sizeof(head.original));
            require(MH_CreateHook(target, detour, &original[index]) == MH_OK, "presentation-create-hook");
        }
        head.created = true;
    }
    void enable() {
        for (auto& head : heads) if (head.created && !head.enabled) {
            require(astera::active() || MH_QueueEnableHook(reinterpret_cast<void*>(head.target)) == MH_OK, "presentation-queue-hook");
            head.enabled = true; // also cover partial application on failure
        }
        require(astera::active() || MH_ApplyQueued() == MH_OK, "presentation-enable-hooks");
    }
    ID3D12CommandQueue* proxy_queue(IUnknown* value) const noexcept {
        for (const auto& q : queues) if (q.native && value == q.native) return q.proxy.Get();
        return nullptr;
    }
    bool is_swap(IUnknown* value) const noexcept { return swap && value == swap.Get(); }
    void retire_swap() noexcept {
        paced_host_swap.store(0); paced_host_thread.store(0); creation_queue = 0; swap.Reset(); proxy_swap.Reset();
        identity_key.store(0); identity.Reset(); stats.identity = false;
        scene([](auto& map) { map.chain(0); });
    }
    void describe() {
        DXGI_SWAP_CHAIN_DESC desc{};
        require(swap && SUCCEEDED(swap->GetDesc(&desc)), "presentation-swap-description");
        stats.width = desc.BufferDesc.Width; stats.height = desc.BufferDesc.Height;
        stats.format = static_cast<unsigned>(desc.BufferDesc.Format); stats.buffers = desc.BufferCount;
    }
    template<class T> HRESULT publish(ComPtr<T>& created, T** result, HRESULT hr, IUnknown* queue) {
        if (FAILED(hr) || !created) return hr;
        auto host = native<T>(sdk, created.Get());
        ComPtr<IDXGISwapChain3> next_native, next_proxy;
        require(SUCCEEDED(host.As(&next_native)) && SUCCEEDED(created.As(&next_proxy)), "presentation-swap3");
        require(next_native.Get() != next_proxy.Get(), "presentation-proxy-required");
        // Retire the previous chain while SL is live; its destruction callback
        // runs through the proxy. No backbuffer is retained by this bridge.
        retire_swap();
        swap = next_native; proxy_swap = next_proxy;
        scene([&](auto& map) { map.chain(reinterpret_cast<std::uintptr_t>(swap.Get())); });
        creation_queue = reinterpret_cast<std::uintptr_t>(queue);
        paced_host_swap.store(reinterpret_cast<std::uintptr_t>(swap.Get()));
        paced_host_thread.store(GetCurrentThreadId());
        add(4, swap.Get(), 8, reinterpret_cast<void*>(&present_hook));
        add(5, swap.Get(), 9, reinterpret_cast<void*>(&buffer_hook));
        add(6, swap.Get(), 10, reinterpret_cast<void*>(&full_hook));
        add(7, swap.Get(), 12, reinterpret_cast<void*>(&desc_hook));
        add(8, swap.Get(), 13, reinterpret_cast<void*>(&resize_hook));
        add(9, swap.Get(), 22, reinterpret_cast<void*>(&present1_hook));
        add(10, swap.Get(), 36, reinterpret_cast<void*>(&index_hook));
        add(11, swap.Get(), 39, reinterpret_cast<void*>(&resize1_hook));
        add(12, swap.Get(), 14, reinterpret_cast<void*>(&target_hook));
        add(13, swap.Get(), 11, reinterpret_cast<void*>(&get_full_hook));
        add(14, swap.Get(), 15, reinterpret_cast<void*>(&output_hook));
        add(15, swap.Get(), 16, reinterpret_cast<void*>(&stats_hook));
        add(16, swap.Get(), 17, reinterpret_cast<void*>(&count_hook));
        add(17, swap.Get(), 18, reinterpret_cast<void*>(&desc1_hook));
        add(18, swap.Get(), 19, reinterpret_cast<void*>(&full_desc_hook));
        add(19, swap.Get(), 20, reinterpret_cast<void*>(&hwnd_get_hook));
        add(20, swap.Get(), 31, reinterpret_cast<void*>(&set_latency_hook));
        add(21, swap.Get(), 32, reinterpret_cast<void*>(&get_latency_hook));
        add(22, swap.Get(), 33, reinterpret_cast<void*>(&waitable_hook));
        add(23, swap.Get(), 37, reinterpret_cast<void*>(&color_check_hook));
        add(24, swap.Get(), 38, reinterpret_cast<void*>(&color_set_hook));
        add(25, swap.Get(), 40, reinterpret_cast<void*>(&hdr_hook));
        enable(); describe(); ++stats.swapchains;
        stats.native_swapchain = reinterpret_cast<std::uintptr_t>(swap.Get());
        stats.proxy_swapchain = reinterpret_cast<std::uintptr_t>(proxy_swap.Get());
        chain_made.store(true, std::memory_order_release);
        *result = host.Detach(); // one new owned native reference for the host
        return hr;
    }
    // v0.27.4: after a resize the identity chain follows the real chain's size
    // and format (Astera Loader describes it natively before its hooks route).
    void follow_identity() noexcept {
        DXGI_SWAP_CHAIN_DESC1 d{};
        if (!identity || !proxy_swap || FAILED(proxy_swap->GetDesc1(&d))) return;
        Bypass internal(true);
        identity->ResizeBuffers(0, d.Width, d.Height, d.Format, 0);
    }
    // v0.27.4, the game's window chain (CreateSwapChainForHwnd) after publish():
    // the game receives a composition chain on its queue with its size and
    // format instead (identity_chain.hpp), if it is the real chain's class.
    // Otherwise the game keeps the real chain (no live switch). Bypass(true) held.
    void adopt_identity(IDXGISwapChain1** result, HWND hwnd, const DXGI_SWAP_CHAIN_DESC1& desc,
                        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* full, IDXGIOutput* restrict_to, IUnknown* queue) {
        ComPtr<IDXGISwapChain1> made;
        for (const auto format : {desc.Format, DXGI_FORMAT_B8G8R8A8_UNORM}) {
            const auto d = identity_description(desc, format);
            if (SUCCEEDED(factory->CreateSwapChainForComposition(queue, &d, nullptr, &made)) && made) break;
            made.Reset();
        }
        ComPtr<IDXGISwapChain3> chain;
        if (!made || FAILED(made.As(&chain)) || !identity_same_class(swap.Get(), chain.Get())) { ++stats.identity_refused; return; }
        creation.hwnd = hwnd; creation.desc = desc; creation.has_full = full != nullptr;
        if (full) creation.full = *full;
        creation.restrict_to = restrict_to; creation.queue = queue;
        identity = chain;
        identity_key.store(reinterpret_cast<std::uintptr_t>(identity.Get()), std::memory_order_release);
        paced_host_swap.store(reinterpret_cast<std::uintptr_t>(identity.Get()));
        stats.identity = true;
        // The game gets the identity chain (one reference; `identity` keeps
        // another); publish() had handed it the real one.
        (*result)->Release();
        *result = chain.Detach();
    }
    // v0.27.4: the switch (fg-live-toggle.md), in the game's ResizeBuffers on
    // the identity chain with the route mutex held, so the game holds no back
    // buffer. Refused (false, nothing changed) without an identity chain, in
    // exclusive fullscreen, or with the game's latency waitable not where
    // waitable_slot() says. Otherwise the real chain and Streamline's proxy are
    // released, DLSS-G loaded or unloaded, the chain made again at the new size
    // with the game's parameters and state (made as before if that fails), the
    // game's waitable replaced and the identity chain resized to match; `out`
    // is the ResizeBuffers result.
    // The real chain's description for the game's resize of the identity
    // chain (0 and UNKNOWN keep `current`'s), with the game's flags.
    DXGI_SWAP_CHAIN_DESC1 resized(const DXGI_SWAP_CHAIN_DESC1& current, UINT count, UINT width, UINT height, DXGI_FORMAT format, UINT flags) const noexcept {
        auto desc = creation.desc;
        desc.Width = width ? width : current.Width;
        desc.Height = height ? height : current.Height;
        desc.Format = format != DXGI_FORMAT_UNKNOWN ? format : current.Format;
        if (count) desc.BufferCount = count;
        desc.Flags = flags;
        return desc;
    }
    // A new real chain in swap/proxy_swap (a switch or the last resort): what
    // the game set before, the waitable it waits on, the identity chain at its
    // size. Bypass(true) held.
    void real_chain_made(const DXGI_SWAP_CHAIN_DESC1& desc) noexcept {
        if (applied.latency) proxy_swap->SetMaximumFrameLatency(applied.latency_value);
        if (applied.color) proxy_swap->SetColorSpace1(applied.color_value);
        if (ComPtr<IDXGISwapChain4> p4; applied.hdr && SUCCEEDED(proxy_swap.As(&p4)))
            p4->SetHDRMetaData(applied.hdr_type, static_cast<UINT>(applied.hdr_data.size()), applied.hdr_data.data());
        if (game_waitable) {
            const auto fresh = proxy_swap->GetFrameLatencyWaitableObject();
            if (fresh && replace_slot_handle(waitable_slot, game_waitable, fresh)) { CloseHandle(game_waitable); game_waitable = fresh; }
            else { if (fresh) CloseHandle(fresh); ++stats.waitable_failures; }
        }
        identity->ResizeBuffers(0, desc.Width, desc.Height, desc.Format, 0);
        stats.native_swapchain = reinterpret_cast<std::uintptr_t>(swap.Get());
        stats.proxy_swapchain = reinterpret_cast<std::uintptr_t>(proxy_swap.Get());
        try { describe(); } catch (...) {}
    }
    // v0.28.6: no real chain through Streamline (a switch's attempts failed).
    // It is made natively on the game's window with the game's queue, past
    // Streamline and every plugin, with DLSS-G unloaded; `proxy_swap` is that
    // chain, so routed calls reach its own methods (synchronous contract).
    // The game presents again, without frame generation; switches are refused.
    bool last_resort(const DXGI_SWAP_CHAIN_DESC1& desc) noexcept {
        if (!identity || !factory || !creation.hwnd || !creation.queue) return false;
        Bypass internal(true);
        swap.Reset(); proxy_swap.Reset();
        try { sdk.frame_generation_load(false); } catch (...) {}
        ComPtr<IDXGISwapChain1> made;
        HRESULT hr{};
        for (unsigned attempt = 0; attempt < 25; ++attempt) {
            hr = factory->CreateSwapChainForHwnd(creation.queue, creation.hwnd, &desc,
                creation.has_full ? &creation.full : nullptr, creation.restrict_to.Get(), &made);
            if (hr != E_ACCESSDENIED) break;
            ++stats.switch_waits;
            Sleep(20);
        }
        ComPtr<IDXGISwapChain3> chain;
        if (FAILED(hr) || !made || FAILED(made.As(&chain))) {
            stats.switch_final_hr = static_cast<std::uint32_t>(FAILED(hr) ? hr : E_NOINTERFACE);
            return false;
        }
        swap = chain; proxy_swap = chain;
        contract = PresentationContract::synchronous_native; stats.last_resort = true;
        real_chain_made(desc);
        return true;
    }
    // No real chain: the game's latency waitable was the released chain's and
    // would never be signalled again (a wait of up to a second each frame);
    // a set event takes its place until a real chain's (real_chain_made).
    void release_waitable() noexcept {
        if (!game_waitable) return;
        const auto set = CreateEventW(nullptr, TRUE, TRUE, nullptr);
        if (set && replace_slot_handle(waitable_slot, game_waitable, set)) { CloseHandle(game_waitable); game_waitable = set; }
        else { if (set) CloseHandle(set); ++stats.waitable_failures; }
    }
    // The game's resize of the identity chain with no real chain: the last
    // resort first (if `retry`), else the identity chain itself without the
    // game's flags (made with none; tearing or the waitable would be refused).
    HRESULT unroutable_resize(bool retry, UINT count, UINT width, UINT height, DXGI_FORMAT format, UINT flags) noexcept {
        if (!identity) return S_OK;  // retired meanwhile; route() re-checks first
        DXGI_SWAP_CHAIN_DESC1 current{};
        Bypass internal(true);
        if (FAILED(identity->GetDesc1(&current))) current = creation.desc;
        // Counted as a switch outcome: a worker's pending request ends here.
        if (retry && last_resort(resized(current, count, width, height, format, flags))) {
            stats.switch_reason = 8; ++stats.switches; ++stats.resizes;
            return S_OK;
        }
        if (retry) ++stats.switch_failures;
        return identity->ResizeBuffers(count, width, height, format, 0);
    }
    bool switch_chain(bool frame_generation, UINT count, UINT width, UINT height, DXGI_FORMAT format, UINT flags, HRESULT& out) {
        const auto refuse = [&](unsigned reason) { ++stats.switch_refusals; stats.switch_reason = reason; return false; };
        if (!identity || !swap || !proxy_swap || stats.last_resort || !sdk.frame_generation_requested()) return refuse(3);
        Bypass internal(true);
        BOOL fullscreen{};
        if (FAILED(swap->GetFullscreenState(&fullscreen, nullptr)) || fullscreen) return refuse(4);
        HANDLE held{};
        if (game_waitable && (!slot_handle(waitable_slot, held) || held != game_waitable)) return refuse(5);
        DXGI_SWAP_CHAIN_DESC1 current{};
        if (FAILED(proxy_swap->GetDesc1(&current))) return refuse(3);
        const auto desc = resized(current, count, width, height, format, flags);
        auto* queue = proxy_queue(creation.queue);
        if (!queue) return refuse(3);
        const bool before = sdk.frame_generation();
        // Our native reference first: Streamline's proxy then holds the last one
        // and takes the physical chain with it.
        swap.Reset(); proxy_swap.Reset();
        const auto make = [&](bool loaded) -> HRESULT {
            if (sdk.frame_generation_load(loaded) != sl::Result::eOk) return E_FAIL;
            ComPtr<IDXGISwapChain1> made;
            // The window takes one flip chain: the old one may go a moment
            // after its last reference (E_ACCESSDENIED meanwhile).
            HRESULT hr{};
            for (unsigned attempt = 0; attempt < 25; ++attempt) {
                hr = proxy_factory->CreateSwapChainForHwnd(queue, creation.hwnd, &desc,
                    creation.has_full ? &creation.full : nullptr, creation.restrict_to.Get(), &made);
                if (hr != E_ACCESSDENIED) break;
                ++stats.switch_waits;
                Sleep(20);
            }
            if (FAILED(hr) || !made) return FAILED(hr) ? hr : E_FAIL;
            try {
                auto host = native<IDXGISwapChain1>(sdk, made.Get());
                ComPtr<IDXGISwapChain3> n, p;
                if (FAILED(host.As(&n)) || FAILED(made.As(&p)) || n.Get() == p.Get() || !identity_same_class(n.Get(), identity.Get()))
                    return E_FAIL;
                swap = n; proxy_swap = p;
            } catch (...) { return E_FAIL; }
            return S_OK;
        };
        auto hr = make(frame_generation);
        if (FAILED(hr)) {
            ++stats.switch_failures; stats.switch_last_hr = static_cast<std::uint32_t>(hr);
            hr = make(before);
            if (FAILED(hr)) {
                // v0.28.6: the game's ResizeBuffers result went to its
                // checkResult (abort) and every later call to the identity
                // chain. Now the last resort, else the identity chain resized
                // and its calls answered (unroutable) until a later resize.
                stats.switch_final_hr = static_cast<std::uint32_t>(hr);
                if (last_resort(desc)) {
                    stats.switch_reason = 8; fault(FaultSite::switch_chain, hr, "switch failed: last resort");
                } else {
                    stats.switch_reason = 7; fault(FaultSite::switch_chain, hr, "switch failed: no real chain");
                    contract = sdk.frame_generation() ? PresentationContract::sdk_paced : PresentationContract::synchronous_native;
                    release_waitable();
                    out = unroutable_resize(false, count, width, height, format, flags);
                    return true;
                }
                out = S_OK;
                return true;
            }
            stats.switch_reason = 6;
        } else stats.switch_reason = frame_generation ? 1U : 2U;
        contract = sdk.frame_generation() ? PresentationContract::sdk_paced : PresentationContract::synchronous_native;
        real_chain_made(desc);
        ++stats.switches;
        out = S_OK;
        return true;
    }
};
namespace {
// active starts before accepting is read. Stop disables and drains callbacks
// before touching SDK state. TLS forwarding does not acquire the route mutex.
struct Call {
    Presentation::State& s;
    explicit Call(Presentation::State& value) : s(value) { s.active.fetch_add(1); }
    ~Call() { s.active.fetch_sub(1); }
};
template<class Fn> Fn original(Presentation::State& s, unsigned index) {
    return reinterpret_cast<Fn>(astera::active() ? chain_next[index] : s.original[index]);
}
// Host lifecycle call: the listener (outside the route mutex), timing, result
// and the chain description afterwards. The SDK's own recursion passes through.
template<class Forward, class Chain>
HRESULT lifecycle_call(unsigned method, IUnknown* self, std::array<std::uint64_t, 6> args, Forward&& forward, Chain&& chain) {
    auto* s = dispatch.load();
    if (bypass_depth || !s) return forward();
    Call active(*s);
    LifecycleEvent e{};
    e.method = method; e.thread = GetCurrentThreadId(); e.args = args;
    const auto host = s->paced_host_swap.load();
    e.current_chain = host && reinterpret_cast<std::uintptr_t>(self) == host;
    e.host_thread = e.thread == s->paced_host_thread.load();
    if (s->accepting.load() && s->listener_ready.load(std::memory_order_acquire))
        e.guard_off = s->listener.before(s->listener.context, method);
    LARGE_INTEGER t{}; QueryPerformanceCounter(&t); e.began_qpc = static_cast<std::uint64_t>(t.QuadPart);
    routed_call = false;
    const HRESULT hr = forward();
    e.routed = routed_call; routed_call = false;
    QueryPerformanceCounter(&t); e.ended_qpc = static_cast<std::uint64_t>(t.QuadPart);
    e.result = static_cast<std::uint32_t>(hr);
    const auto error = GetLastError();
    if (IUnknown* described = SUCCEEDED(hr) ? chain() : nullptr) {
        Bypass bypass; ComPtr<IDXGISwapChain> sc; DXGI_SWAP_CHAIN_DESC d{};
        if (SUCCEEDED(described->QueryInterface(IID_PPV_ARGS(&sc))) && SUCCEEDED(sc->GetDesc(&d))) {
            e.described = true; e.windowed = d.Windowed != FALSE;
            e.width = d.BufferDesc.Width; e.height = d.BufferDesc.Height; e.format = static_cast<std::uint32_t>(d.BufferDesc.Format);
            e.buffers = d.BufferCount; e.flags = d.Flags;
        }
    }
    if (e.described && (e.current_chain || method <= 3) && s->accepting.load() &&
        s->listener_ready.load(std::memory_order_acquire) && s->listener.after)
        s->listener.after(s->listener.context, e.format);
    SetLastError(error);
    s->lifecycle_record(e);
    return hr;
}
HRESULT STDMETHODCALLTYPE queue_hook(ID3D12Device* self, const D3D12_COMMAND_QUEUE_DESC* desc, REFIID iid, void** out) {
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    auto& s = *dispatch.load(); Call call(s);
    // The game's queues only (queue_admission): the mod's own private queues
    // (astera::Internal) are not proxied (game-125: the DLSS context's queue,
    // one per live rebuild, filled the 16 slots), nor anything made once the
    // game's chain exists (v0.28.6, tifa-6: ten more, from another thread
    // than the game's, were proxied and kept; the 17th faulted routing off
    // and the game aborted). A full table or a refused proxy is native.
    // Late ones are counted without the route mutex: other threads make them,
    // possibly while a switch holds it across Streamline's DLSS-G load.
    if (!bypass_depth && !astera::internal_call() && s.accepting.load() && self == s.device.Get() && desc && out &&
        iid == __uuidof(ID3D12CommandQueue)) {
        if (queue_admission(true, s.chain_made.load(std::memory_order_acquire), true) == QueueAdmission::late) {
            s.late_queue(caller);
            return original<QueueFn>(s, 0)(self, desc, iid, out);
        }
        std::lock_guard lock(s.mutex);
        auto slot = s.queues.begin(); while (slot != s.queues.end() && slot->proxy) ++slot;
        switch (queue_admission(s.accepting.load(), s.chain_made.load(), slot != s.queues.end())) {
        case QueueAdmission::native: break;
        case QueueAdmission::late: s.late_queue(caller); break;
        case QueueAdmission::overflow: ++s.stats.queue_overflows; break;
        case QueueAdmission::proxy: {
            Bypass bypass(true);
            try {
                ComPtr<ID3D12CommandQueue> proxy;
                const auto hr = s.proxy_device->CreateCommandQueue(desc, iid, reinterpret_cast<void**>(proxy.GetAddressOf()));
                if (FAILED(hr) || !proxy) {
                    ++s.stats.queue_failures; s.stats.queue_failure_hr = static_cast<std::uint32_t>(FAILED(hr) ? hr : E_POINTER);
                    break;
                }
                auto host = native<ID3D12CommandQueue>(s.sdk, proxy.Get());
                require(host.Get() != proxy.Get(), "presentation-queue-proxy");
                slot->native = host.Get(); slot->proxy = proxy;
                *out = host.Detach(); ++s.stats.queues;
                s.record({0,0,0,static_cast<std::uint32_t>(hr),0,0,caller,reinterpret_cast<std::uintptr_t>(self),
                    reinterpret_cast<std::uintptr_t>(*out),{static_cast<unsigned>(desc->Type),static_cast<unsigned>(desc->Priority),
                    static_cast<unsigned>(desc->Flags),desc->NodeMask}});
                return hr;
            } catch (...) { s.fault_exception(FaultSite::queue); }
            break;
        }
        }
    }
    return original<QueueFn>(s, 0)(self, desc, iid, out);
}
// A routed creation that throws leaves no routed chain: retired, the fault
// logged, routing off, and the game's creation native.
HRESULT swap_routed(IDXGIFactory* self, IUnknown* queue, DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** out, std::uintptr_t caller) {
    auto& s = *dispatch.load(); Call call(s);
    if (!bypass_depth && s.accepting.load() && self == s.factory.Get() && desc && out) {
        std::lock_guard lock(s.mutex);
        if (auto* proxy = s.accepting.load() ? s.proxy_queue(queue) : nullptr) {
            Bypass bypass(true);
            try {
                OwnershipCall v{1,0,0,0,0,0,caller,reinterpret_cast<std::uintptr_t>(self),0,
                    {reinterpret_cast<std::uintptr_t>(queue),desc->BufferDesc.Width,desc->BufferDesc.Height,
                    static_cast<unsigned>(desc->BufferDesc.Format),desc->BufferCount,desc->Flags,
                    static_cast<unsigned>(desc->SwapEffect),static_cast<unsigned>(desc->Windowed)}};
                s.retire_swap(); ComPtr<IDXGISwapChain> made;
                const auto hr=s.proxy_factory->CreateSwapChain(proxy,desc,&made);
                const auto result=s.publish(made,out,hr,queue);
                v.result=static_cast<std::uint32_t>(result);v.returned=SUCCEEDED(result)?reinterpret_cast<std::uintptr_t>(*out):0;s.record(v);
                return result;
            }
            catch (...) { s.retire_swap(); s.fault_exception(FaultSite::create); s.accepting.store(false); }
        }
    }
    return original<SwapFn>(s, 1)(self, queue, desc, out);
}
HRESULT hwnd_routed(IDXGIFactory2* self, IUnknown* queue, HWND hwnd, const DXGI_SWAP_CHAIN_DESC1* desc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* full, IDXGIOutput* restrict, IDXGISwapChain1** out, std::uintptr_t caller) {
    auto& s = *dispatch.load(); Call call(s);
    if (!bypass_depth && s.accepting.load() && self == s.factory.Get() && desc && out) {
        std::lock_guard lock(s.mutex);
        if (auto* proxy = s.accepting.load() ? s.proxy_queue(queue) : nullptr) {
            Bypass bypass(true);
            try {
                OwnershipCall v{2,0,0,0,0,0,caller,reinterpret_cast<std::uintptr_t>(self),0,
                    {reinterpret_cast<std::uintptr_t>(queue),desc->Width,desc->Height,static_cast<unsigned>(desc->Format),
                    desc->BufferCount,desc->Flags,static_cast<unsigned>(desc->SwapEffect),full?static_cast<unsigned>(full->Windowed):1}};
                s.retire_swap(); ComPtr<IDXGISwapChain1> made;
                const auto hr=s.proxy_factory->CreateSwapChainForHwnd(proxy,hwnd,desc,full,restrict,&made);
                const auto result=s.publish(made,out,hr,queue);
                if(SUCCEEDED(result)&&*out)s.adopt_identity(out,hwnd,*desc,full,restrict,queue);
                v.result=static_cast<std::uint32_t>(result);v.returned=SUCCEEDED(result)?reinterpret_cast<std::uintptr_t>(*out):0;s.record(v);
                return result;
            }
            catch (...) { s.retire_swap(); s.fault_exception(FaultSite::create); s.accepting.store(false); }
        }
    }
    return original<HwndFn>(s, 2)(self, queue, hwnd, desc, full, restrict, out);
}
HRESULT core_routed(IDXGIFactory2* self, IUnknown* queue, IUnknown* window, const DXGI_SWAP_CHAIN_DESC1* desc,
    IDXGIOutput* restrict, IDXGISwapChain1** out, std::uintptr_t caller) {
    auto& s = *dispatch.load(); Call call(s);
    if (!bypass_depth && s.accepting.load() && self == s.factory.Get() && desc && out) {
        std::lock_guard lock(s.mutex);
        if (auto* proxy = s.accepting.load() ? s.proxy_queue(queue) : nullptr) {
            Bypass bypass(true);
            try {
                OwnershipCall v{3,0,0,0,0,0,caller,reinterpret_cast<std::uintptr_t>(self),0,
                    {reinterpret_cast<std::uintptr_t>(queue),desc->Width,desc->Height,static_cast<unsigned>(desc->Format),
                    desc->BufferCount,desc->Flags,static_cast<unsigned>(desc->SwapEffect),1}};
                s.retire_swap(); ComPtr<IDXGISwapChain1> made;
                const auto hr=s.proxy_factory->CreateSwapChainForCoreWindow(proxy,window,desc,restrict,&made);
                const auto result=s.publish(made,out,hr,queue);
                v.result=static_cast<std::uint32_t>(result);v.returned=SUCCEEDED(result)?reinterpret_cast<std::uintptr_t>(*out):0;s.record(v);
                return result;
            }
            catch (...) { s.retire_swap(); s.fault_exception(FaultSite::create); s.accepting.store(false); }
        }
    }
    return original<CoreFn>(s, 3)(self, queue, window, desc, restrict, out);
}
using Args = std::array<std::uint64_t, 6>;
Args creation_args(const DXGI_SWAP_CHAIN_DESC1* d, unsigned windowed) {
    return d ? Args{d->Width, d->Height, static_cast<unsigned>(d->Format), d->BufferCount, d->Flags, windowed} : Args{};
}
template<class T> IUnknown* created_chain(T** out) { return out && *out ? static_cast<IUnknown*>(*out) : nullptr; }
HRESULT STDMETHODCALLTYPE swap_hook(IDXGIFactory* self, IUnknown* queue, DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** out) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const auto args = desc ? Args{desc->BufferDesc.Width, desc->BufferDesc.Height, static_cast<unsigned>(desc->BufferDesc.Format),
                                  desc->BufferCount, desc->Flags, static_cast<unsigned>(desc->Windowed)} : Args{};
    return lifecycle_call(1, self, args, [&] { return swap_routed(self, queue, desc, out, caller); }, [&] { return created_chain(out); });
}
HRESULT STDMETHODCALLTYPE hwnd_hook(IDXGIFactory2* self, IUnknown* queue, HWND hwnd, const DXGI_SWAP_CHAIN_DESC1* desc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* full, IDXGIOutput* restrict, IDXGISwapChain1** out) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    return lifecycle_call(2, self, creation_args(desc, full ? static_cast<unsigned>(full->Windowed) : 1),
        [&] { return hwnd_routed(self, queue, hwnd, desc, full, restrict, out, caller); }, [&] { return created_chain(out); });
}
HRESULT STDMETHODCALLTYPE core_hook(IDXGIFactory2* self, IUnknown* queue, IUnknown* window, const DXGI_SWAP_CHAIN_DESC1* desc,
    IDXGIOutput* restrict, IDXGISwapChain1** out) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    return lifecycle_call(3, self, creation_args(desc, 1),
        [&] { return core_routed(self, queue, window, desc, restrict, out, caller); }, [&] { return created_chain(out); });
}
// A game Present on the identity chain with no real chain (identity_route):
// the frame is dropped and S_OK returned; the frame observer still sees it.
HRESULT dropped_present(Presentation::State& s, IUnknown* self, UINT flags) noexcept {
    const auto key = reinterpret_cast<std::uintptr_t>(self);
    const bool ticket = s.observer.begin && s.observer.begin(s.observer.context, key, flags);
    if (ticket && s.observer.end) s.observer.end(s.observer.context, key, flags, S_OK);
    return S_OK;
}
// Host frame interval for the overhead measurements (one QPC read per Present).
void host_frame() noexcept {
    LARGE_INTEGER now{}, frequency{};
    QueryPerformanceCounter(&now); QueryPerformanceFrequency(&frequency);
    mhw::perf::host_frames.present(now.QuadPart, frequency.QuadPart);
    mhw::perf::present_thread.store(GetCurrentThreadId(), std::memory_order_relaxed);
}
template<class Routed, class Forward, class Unroutable>
auto route(IUnknown* self, OwnershipCall diagnostic, Routed&& routed, Forward&& forward, Unroutable&& unroutable) {
    auto& s = *dispatch.load(); Call call(s);
    // v0.27.4: with an identity chain every call on it reaches the real chain
    // and any other chain's (Streamline's own on the real chain) stays native.
    // v0.28.6 (tifa-6, identity_route): never the identity chain's own method,
    // whatever faulted; without a real chain the call is answered instead.
    if (const auto identity = s.identity_key.load(std::memory_order_acquire)) {
        const auto where = [&](bool real) { return identity_route(bypass_depth != 0, reinterpret_cast<std::uintptr_t>(self) == identity, real); };
        if (where(true) == IdentityRoute::forward) return forward(s);
        std::lock_guard lock(s.mutex);
        // Retired meanwhile (another creation): no longer the game's chain.
        if (s.identity_key.load() != identity) return forward(s);
        // Bypass for both: a plugin behind the original may call back here.
        Bypass bypass;
        if (where(s.proxy_swap != nullptr) == IdentityRoute::unroutable) { ++s.stats.unroutable_calls; return unroutable(s); }
        routed_call=true;const auto value=routed(s,diagnostic);
        diagnostic.object=reinterpret_cast<std::uintptr_t>(self);
        diagnostic.result=static_cast<std::uint32_t>(value);s.record(diagnostic);
        return value;
    }
    // The SDK worker can use the very same physical chain as the host. Its
    // native indices/buffers must never be replaced with logical SDK ones.
    // The paced foundation is explicitly single-host-thread; a game adapter
    // must establish its API owners before extending this contract.
    if (s.contract == PresentationContract::sdk_paced &&
        !paced_host_call(reinterpret_cast<std::uintptr_t>(self), s.paced_host_swap.load(),
            GetCurrentThreadId(), s.paced_host_thread.load())) return forward(s);
    if (!bypass_depth && s.accepting.load()) {
        std::lock_guard lock(s.mutex);
        if (s.accepting.load() && s.is_swap(self)) {
            Bypass bypass;routed_call=true;const auto value=routed(s,diagnostic);
            diagnostic.object=reinterpret_cast<std::uintptr_t>(self);
            diagnostic.result=static_cast<std::uint32_t>(value);s.record(diagnostic);
            return value;
        }
    }
    return forward(s);
}
HRESULT STDMETHODCALLTYPE present_hook(IDXGISwapChain* self, UINT interval, UINT flags) {
    return route(self, {4,0,0,0,0,0,reinterpret_cast<std::uintptr_t>(_ReturnAddress()),0,0,{interval,flags}}, [&](auto& s,auto& v) {
        const auto before = GetLastError();
        if (!(flags & DXGI_PRESENT_TEST)) host_frame();
        if (!(flags & DXGI_PRESENT_TEST)) s.scene_present_entry(self);
        const bool ticket = s.observer.begin && s.observer.begin(s.observer.context, reinterpret_cast<std::uintptr_t>(self), flags);
        SetLastError(before);
        auto* target = s.native_of(self);
        Capture capture(target);
        const auto hr = s.proxy_swap->Present(interval, flags);
        if (flags & DXGI_PRESENT_TEST) ++s.stats.test_presents; else ++s.stats.presents;
        const auto decision = presentation_result(s.contract, capture.value.calls, hr, capture.value.result);
        if (decision.fault) s.fault(FaultSite::present, hr, "present");
        if (decision.forward_native) record_present(target, original<PresentFn>(s, 4)(target, interval, flags));
        if (!(flags & DXGI_PRESENT_TEST)) s.stats.native_presents += capture.value.calls;
        if (!(flags & DXGI_PRESENT_TEST) && s.contract == PresentationContract::sdk_paced) ++s.stats.sdk_paced_presents;
        const auto result = s.contract == PresentationContract::sdk_paced ? s.paced(decision.result, hr) : capture.value.result;
        if (!(flags & DXGI_PRESENT_TEST)) s.scene([&](auto& map) { map.present_result(reinterpret_cast<std::uintptr_t>(self), static_cast<std::uint32_t>(result)); });
        v.args[2]=ticket;v.args[3]=capture.value.calls;
        const auto error = GetLastError();
        if (ticket && s.observer.end) s.observer.end(s.observer.context, reinterpret_cast<std::uintptr_t>(self), flags, result);
        SetLastError(error);
        // SL's after-hook may overwrite an occlusion/status HRESULT. Preserve
        // the actual native result and never present twice after an SDK error.
        return result;
    }, [&](auto& s) { return record_present(self, original<PresentFn>(s, 4)(self, interval, flags)); },
    [&](auto& s) { return dropped_present(s, self, flags); });  // no real chain
}
HRESULT STDMETHODCALLTYPE buffer_hook(IDXGISwapChain* self, UINT index, REFIID iid, void** out) {
    return route(self,{5,0,0,0,0,0,reinterpret_cast<std::uintptr_t>(_ReturnAddress()),0,0,{index,iid==__uuidof(ID3D12Resource)}}, [&](auto& s,auto& v) {
        const auto hr=s.proxy_swap->GetBuffer(index,iid,out);v.returned=SUCCEEDED(hr)&&out?reinterpret_cast<std::uintptr_t>(*out):0;
        if(v.returned&&iid==__uuidof(ID3D12Resource))s.scene([&](auto& map){map.buffer(reinterpret_cast<std::uintptr_t>(self),index,v.returned);});
        return hr;},
        [&](auto& s) { return original<BufferFn>(s, 5)(self, index, iid, out); },
        [&](auto& s) { return original<BufferFn>(s, 5)(self, index, iid, out); });  // the identity chain's own buffers
}
HRESULT STDMETHODCALLTYPE full_hook(IDXGISwapChain* self, BOOL full, IDXGIOutput* target) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    return lifecycle_call(6, self, Args{static_cast<unsigned>(full), reinterpret_cast<std::uintptr_t>(target)}, [&] {
        return route(self,{6,0,0,0,0,0,caller,0,0,{static_cast<unsigned>(full),reinterpret_cast<std::uintptr_t>(target)}}, [&](auto& s,auto&) { return s.proxy_swap->SetFullscreenState(full, target); },
            [&](auto& s) { return original<FullFn>(s, 6)(self, full, target); }, [&](auto&) { return S_OK; });
    }, [&] { return static_cast<IUnknown*>(self); });
}
HRESULT STDMETHODCALLTYPE desc_hook(IDXGISwapChain* self, DXGI_SWAP_CHAIN_DESC* desc) {
    return route(self,{7,0,0,0,0,0,reinterpret_cast<std::uintptr_t>(_ReturnAddress())}, [&](auto& s,auto& v) {
        const auto hr=s.proxy_swap->GetDesc(desc);
        if(SUCCEEDED(hr)&&desc)v.args={desc->BufferDesc.Width,desc->BufferDesc.Height,static_cast<unsigned>(desc->BufferDesc.Format),
            desc->BufferCount,desc->Flags,static_cast<unsigned>(desc->Windowed)};
        return hr;},
        [&](auto& s) { return original<DescFn>(s, 7)(self, desc); }, [&](auto& s) { return original<DescFn>(s, 7)(self, desc); });
}
HRESULT STDMETHODCALLTYPE resize_hook(IDXGISwapChain* self, UINT count, UINT w, UINT h, DXGI_FORMAT format, UINT flags) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    return lifecycle_call(8, self, Args{count, w, h, static_cast<unsigned>(format), flags}, [&] {
    return route(self,{8,0,0,0,0,0,caller,0,0,{count,w,h,static_cast<unsigned>(format),flags}}, [&](auto& s,auto&) {
        // A switch asked for (request_switch) happens in this resize.
        if (const auto want = s.is_identity(self) ? s.switch_request.exchange(0) : 0U) {
            HRESULT switched{};
            if (s.switch_chain(want == 2, count, w, h, format, flags, switched)) { if (SUCCEEDED(switched)) ++s.stats.resizes; return switched; }
        }
        const auto hr = s.proxy_swap->ResizeBuffers(count, w, h, format, flags);
        if (SUCCEEDED(hr)) { ++s.stats.resizes; try { s.describe(); } catch (...) { s.fault(FaultSite::resize, S_OK, "describe"); } }
        if (SUCCEEDED(hr) && s.is_identity(self)) s.follow_identity();
        if (SUCCEEDED(hr)) s.scene([&](auto& map) { map.resized(reinterpret_cast<std::uintptr_t>(self)); });
        return hr;
    }, [&](auto& s) { return original<ResizeFn>(s, 8)(self, count, w, h, format, flags); },
    [&](auto& s) { s.switch_request.store(0); return s.unroutable_resize(true, count, w, h, format, flags); });
    }, [&] { return static_cast<IUnknown*>(self); });
}
HRESULT STDMETHODCALLTYPE present1_hook(IDXGISwapChain1* self, UINT interval, UINT flags, const DXGI_PRESENT_PARAMETERS* params) {
    return route(self,{9,0,0,0,0,0,reinterpret_cast<std::uintptr_t>(_ReturnAddress()),0,0,{interval,flags}}, [&](auto& s,auto& v) {
        const auto before = GetLastError();
        if (!(flags & DXGI_PRESENT_TEST)) host_frame();
        if (!(flags & DXGI_PRESENT_TEST)) s.scene_present_entry(self);
        const bool ticket = s.observer.begin && s.observer.begin(s.observer.context, reinterpret_cast<std::uintptr_t>(self), flags);
        SetLastError(before);
        auto* target = s.native_of(self);
        Capture capture(target);
        const auto hr = s.proxy_swap->Present1(interval, flags, params);
        if (flags & DXGI_PRESENT_TEST) ++s.stats.test_presents; else ++s.stats.presents;
        const auto decision = presentation_result(s.contract, capture.value.calls, hr, capture.value.result);
        if (decision.fault) s.fault(FaultSite::present1, hr, "present1");
        if (decision.forward_native) record_present(target, original<Present1Fn>(s, 9)(target, interval, flags, params));
        if (!(flags & DXGI_PRESENT_TEST)) s.stats.native_presents += capture.value.calls;
        if (!(flags & DXGI_PRESENT_TEST) && s.contract == PresentationContract::sdk_paced) ++s.stats.sdk_paced_presents;
        const auto result = s.contract == PresentationContract::sdk_paced ? s.paced(decision.result, hr) : capture.value.result;
        if (!(flags & DXGI_PRESENT_TEST)) s.scene([&](auto& map) { map.present_result(reinterpret_cast<std::uintptr_t>(self), static_cast<std::uint32_t>(result)); });
        v.args[2]=ticket;v.args[3]=capture.value.calls;
        const auto error = GetLastError();
        if (ticket && s.observer.end) s.observer.end(s.observer.context, reinterpret_cast<std::uintptr_t>(self), flags, result);
        SetLastError(error);
        return result;
    }, [&](auto& s) { return record_present(self, original<Present1Fn>(s, 9)(self, interval, flags, params)); },
    [&](auto& s) { return dropped_present(s, self, flags); });
}
UINT STDMETHODCALLTYPE index_hook(IDXGISwapChain3* self) {
    return route(self,{10,0,0,0,0,0,reinterpret_cast<std::uintptr_t>(_ReturnAddress())}, [&](auto& s,auto&) {
        const auto index = s.proxy_swap->GetCurrentBackBufferIndex();
        s.scene([&](auto& map) { map.game_index(reinterpret_cast<std::uintptr_t>(self), index); });
        return index; },
        [&](auto& s) { return original<IndexFn>(s, 10)(self); }, [&](auto& s) { return original<IndexFn>(s, 10)(self); });
}
HRESULT STDMETHODCALLTYPE resize1_hook(IDXGISwapChain3* self, UINT count, UINT w, UINT h, DXGI_FORMAT format, UINT flags,
    const UINT* masks, IUnknown* const* queues) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    return lifecycle_call(11, self, Args{count, w, h, static_cast<unsigned>(format), flags}, [&] {
    return route(self,{11,0,0,0,0,0,caller,0,0,{count,w,h,static_cast<unsigned>(format),flags}}, [&](auto& s,auto&) {
        HRESULT hr{};
        if (s.contract == PresentationContract::sdk_paced) {
            // SDK 2.12 FG ResizeBuffers1 raised E_ABORT in the owned fixture.
            // Use the equivalent SDK ResizeBuffers route only when queue/node
            // assignment cannot change. General queue reassignment stays
            // unsupported in this FG foundation; never silently drop it.
            if (!count || count > 16 || !masks || !queues) return DXGI_ERROR_INVALID_CALL;
            std::array<std::uintptr_t, 16> labels{};
            for (unsigned i = 0; i < count; ++i) labels[i] = reinterpret_cast<std::uintptr_t>(queues[i]);
            if (!simple_resize_equivalent(count, s.device_nodes, s.creation_queue,
                {masks, count}, {labels.data(), count})) return DXGI_ERROR_INVALID_CALL;
            hr = s.proxy_swap->ResizeBuffers(count, w, h, format, flags);
        } else {
            // Accepted Reflex-only path preserves the native queue arguments.
            hr = s.proxy_swap->ResizeBuffers1(count, w, h, format, flags, masks, queues);
        }
        if (SUCCEEDED(hr)) { ++s.stats.resizes; try { s.describe(); } catch (...) { s.fault(FaultSite::resize1, S_OK, "describe"); } }
        if (SUCCEEDED(hr) && s.is_identity(self)) s.follow_identity();
        if (SUCCEEDED(hr)) s.scene([&](auto& map) { map.resized(reinterpret_cast<std::uintptr_t>(self)); });
        return hr;
    }, [&](auto& s) { return original<Resize1Fn>(s, 11)(self, count, w, h, format, flags, masks, queues); },
    [&](auto& s) { s.switch_request.store(0); return s.unroutable_resize(true, count, w, h, format, flags); });
    }, [&] { return static_cast<IUnknown*>(self); });
}
// v0.27.4: the identity chain's other methods (identity_chain.hpp). On it they
// reach the real chain through Streamline's proxy (what the game set is kept
// for switch_chain()); on any other chain they stay native. Without a real
// chain (v0.28.6, identity_route) `unroutable` answers, by default the
// identity chain's own method; where a composition chain refuses (window,
// fullscreen, latency) the answer is the game's own state instead.
template<class R, class Routed, class Native, class Unroutable>
R identity_call(IUnknown* self, Routed&& routed, Native&& native, Unroutable&& unroutable) {
    auto* sp = dispatch.load(std::memory_order_acquire);
    if (!sp) return native();
    auto& s = *sp; Call call(s);
    const auto key = s.identity_key.load(std::memory_order_acquire);
    if (!key) return native();
    const auto where = [&](bool real) { return identity_route(bypass_depth != 0, reinterpret_cast<std::uintptr_t>(self) == key, real); };
    if (where(true) == IdentityRoute::forward) return native();
    std::lock_guard lock(s.mutex);
    if (s.identity_key.load() != key) return native();
    Bypass bypass;
    if (where(s.proxy_swap != nullptr) == IdentityRoute::unroutable) { ++s.stats.unroutable_calls; return unroutable(s); }
    routed_call = true;
    return routed(s);
}
template<class R, class Routed, class Native>
R identity_call(IUnknown* self, Routed&& routed, Native&& native) {
    return identity_call<R>(self, routed, native, [&](auto&) -> R { return native(); });
}
HRESULT STDMETHODCALLTYPE target_hook(IDXGISwapChain* self, const DXGI_MODE_DESC* mode) {
    const auto args = mode ? Args{mode->Width, mode->Height, static_cast<unsigned>(mode->Format), mode->RefreshRate.Numerator,
                                  mode->RefreshRate.Denominator, static_cast<unsigned>(mode->Scaling)} : Args{};
    return lifecycle_call(12, self, args, [&] {
        return identity_call<HRESULT>(self, [&](auto& r) { return r.proxy_swap->ResizeTarget(mode); },
            [&] { return original<TargetFn>(*dispatch.load(), 12)(self, mode); }, [&](auto&) { return S_OK; }); },
        [&] { return static_cast<IUnknown*>(self); });
}
HRESULT STDMETHODCALLTYPE get_full_hook(IDXGISwapChain* self, BOOL* full, IDXGIOutput** target) {
    return identity_call<HRESULT>(self, [&](auto& s) { return s.proxy_swap->GetFullscreenState(full, target); },
        [&] { return original<GetFullFn>(*dispatch.load(), 13)(self, full, target); },
        [&](auto&) { if (full) *full = FALSE; if (target) *target = nullptr; return S_OK; });
}
HRESULT STDMETHODCALLTYPE output_hook(IDXGISwapChain* self, IDXGIOutput** output) {
    return identity_call<HRESULT>(self, [&](auto& s) { return s.proxy_swap->GetContainingOutput(output); },
        [&] { return original<OutputFn>(*dispatch.load(), 14)(self, output); });
}
HRESULT STDMETHODCALLTYPE stats_hook(IDXGISwapChain* self, DXGI_FRAME_STATISTICS* stats) {
    return identity_call<HRESULT>(self, [&](auto& s) { return s.proxy_swap->GetFrameStatistics(stats); },
        [&] { return original<StatsFn>(*dispatch.load(), 15)(self, stats); });
}
HRESULT STDMETHODCALLTYPE count_hook(IDXGISwapChain* self, UINT* count) {
    return identity_call<HRESULT>(self, [&](auto& s) { return s.proxy_swap->GetLastPresentCount(count); },
        [&] { return original<CountFn>(*dispatch.load(), 16)(self, count); });
}
HRESULT STDMETHODCALLTYPE desc1_hook(IDXGISwapChain1* self, DXGI_SWAP_CHAIN_DESC1* desc) {
    return identity_call<HRESULT>(self, [&](auto& s) { return s.proxy_swap->GetDesc1(desc); },
        [&] { return original<Desc1Fn>(*dispatch.load(), 17)(self, desc); });
}
HRESULT STDMETHODCALLTYPE full_desc_hook(IDXGISwapChain1* self, DXGI_SWAP_CHAIN_FULLSCREEN_DESC* desc) {
    return identity_call<HRESULT>(self, [&](auto& s) { return s.proxy_swap->GetFullscreenDesc(desc); },
        [&] { return original<FullDescFn>(*dispatch.load(), 18)(self, desc); },
        [&](auto& s) {
            if (!desc) return DXGI_ERROR_INVALID_CALL;
            if (s.creation.has_full) *desc = s.creation.full; else { *desc = {}; desc->Windowed = TRUE; }
            return S_OK; });
}
HRESULT STDMETHODCALLTYPE hwnd_get_hook(IDXGISwapChain1* self, HWND* hwnd) {
    return identity_call<HRESULT>(self, [&](auto& s) { return s.proxy_swap->GetHwnd(hwnd); },
        [&] { return original<HwndGetFn>(*dispatch.load(), 19)(self, hwnd); },
        [&](auto& s) { if (!hwnd) return DXGI_ERROR_INVALID_CALL; *hwnd = s.creation.hwnd; return S_OK; });
}
HRESULT STDMETHODCALLTYPE set_latency_hook(IDXGISwapChain2* self, UINT latency) {
    return identity_call<HRESULT>(self, [&](auto& s) {
        const auto hr = s.proxy_swap->SetMaximumFrameLatency(latency);
        if (SUCCEEDED(hr)) { s.applied.latency = true; s.applied.latency_value = latency; }
        return hr; },
        [&] { return original<SetLatencyFn>(*dispatch.load(), 20)(self, latency); },
        [&](auto& s) { s.applied.latency = true; s.applied.latency_value = latency; return S_OK; });
}
HRESULT STDMETHODCALLTYPE get_latency_hook(IDXGISwapChain2* self, UINT* latency) {
    return identity_call<HRESULT>(self, [&](auto& s) { return s.proxy_swap->GetMaximumFrameLatency(latency); },
        [&] { return original<GetLatencyFn>(*dispatch.load(), 21)(self, latency); },
        [&](auto& s) { if (!latency) return DXGI_ERROR_INVALID_CALL; *latency = s.applied.latency ? s.applied.latency_value : 1; return S_OK; });
}
HANDLE STDMETHODCALLTYPE waitable_hook(IDXGISwapChain2* self) {
    return identity_call<HANDLE>(self, [&](auto& s) {
        // The game asks once, at start, and keeps that one; a later caller's
        // handle is its own (a switch would otherwise be refused, reason 5).
        const auto handle = s.proxy_swap->GetFrameLatencyWaitableObject();
        if (handle && !s.game_waitable) s.game_waitable = handle;
        return handle; },
        [&] { return original<WaitableFn>(*dispatch.load(), 22)(self); });
}
HRESULT STDMETHODCALLTYPE color_check_hook(IDXGISwapChain3* self, DXGI_COLOR_SPACE_TYPE space, UINT* support) {
    return identity_call<HRESULT>(self, [&](auto& s) { return s.proxy_swap->CheckColorSpaceSupport(space, support); },
        [&] { return original<ColorCheckFn>(*dispatch.load(), 23)(self, space, support); });
}
HRESULT STDMETHODCALLTYPE color_set_hook(IDXGISwapChain3* self, DXGI_COLOR_SPACE_TYPE space) {
    return identity_call<HRESULT>(self, [&](auto& s) {
        const auto hr = s.proxy_swap->SetColorSpace1(space);
        if (SUCCEEDED(hr)) { s.applied.color = true; s.applied.color_value = space; }
        return hr; },
        [&] { return original<ColorSetFn>(*dispatch.load(), 24)(self, space); },
        [&](auto& s) { s.applied.color = true; s.applied.color_value = space; return S_OK; });  // for a later real chain
}
HRESULT STDMETHODCALLTYPE hdr_hook(IDXGISwapChain4* self, DXGI_HDR_METADATA_TYPE type, UINT size, void* data) {
    return identity_call<HRESULT>(self, [&](auto& s) {
        ComPtr<IDXGISwapChain4> p4;
        if (FAILED(s.proxy_swap.As(&p4))) return E_NOINTERFACE;
        const auto hr = p4->SetHDRMetaData(type, size, data);
        if (SUCCEEDED(hr) && size <= 4096 && (data || !size)) {
            s.applied.hdr = true; s.applied.hdr_type = type;
            const auto* bytes = static_cast<const std::byte*>(data);
            s.applied.hdr_data.assign(bytes, bytes + size);
        }
        return hr; },
        [&] { return original<HdrFn>(*dispatch.load(), 25)(self, type, size, data); },
        [&](auto& s) {
            if (size <= 4096 && (data || !size)) {
                s.applied.hdr = true; s.applied.hdr_type = type;
                const auto* bytes = static_cast<const std::byte*>(data);
                s.applied.hdr_data.assign(bytes, bytes + size);
            }
            return S_OK; });
}
}
// AL: each detour behind a gate that continues with AL's stored next
// method until this presentation exists (DEVICE_CREATED). Detours then follow
// their own accepting/route rules, as with MinHook.
namespace {
template<auto Detour, unsigned Index> struct Chained;
template<class R, class... A, R(STDMETHODCALLTYPE* Detour)(A...), unsigned Index> struct Chained<Detour, Index> {
    static R STDMETHODCALLTYPE call(A... a) {
        if (!dispatch.load(std::memory_order_acquire))
            return reinterpret_cast<R(STDMETHODCALLTYPE*)(A...)>(chain_next[Index])(a...);
        return Detour(a...);
    }
};
}
void presentation_astera_hooks(std::vector<astera::Hook>& out) {
    using astera::method;
    const auto add = [&](std::uint32_t m, void* detour, unsigned index) { out.push_back({m, detour, &chain_next[index]}); };
    add(method(astera::device, 8), reinterpret_cast<void*>(&Chained<&queue_hook, 0>::call), 0);
    add(method(astera::factory, 10), reinterpret_cast<void*>(&Chained<&swap_hook, 1>::call), 1);
    add(method(astera::factory, 15), reinterpret_cast<void*>(&Chained<&hwnd_hook, 2>::call), 2);
    add(method(astera::factory, 16), reinterpret_cast<void*>(&Chained<&core_hook, 3>::call), 3);
    add(method(astera::swapchain, 8), reinterpret_cast<void*>(&Chained<&present_hook, 4>::call), 4);
    add(method(astera::swapchain, 9), reinterpret_cast<void*>(&Chained<&buffer_hook, 5>::call), 5);
    add(method(astera::swapchain, 10), reinterpret_cast<void*>(&Chained<&full_hook, 6>::call), 6);
    add(method(astera::swapchain, 12), reinterpret_cast<void*>(&Chained<&desc_hook, 7>::call), 7);
    add(method(astera::swapchain, 13), reinterpret_cast<void*>(&Chained<&resize_hook, 8>::call), 8);
    add(method(astera::swapchain, 22), reinterpret_cast<void*>(&Chained<&present1_hook, 9>::call), 9);
    add(method(astera::swapchain, 36), reinterpret_cast<void*>(&Chained<&index_hook, 10>::call), 10);
    add(method(astera::swapchain, 39), reinterpret_cast<void*>(&Chained<&resize1_hook, 11>::call), 11);
    // v0.27.4: the identity chain's routed methods (identity_chain.hpp).
    add(method(astera::swapchain, 14), reinterpret_cast<void*>(&Chained<&target_hook, 12>::call), 12);
    add(method(astera::swapchain, 11), reinterpret_cast<void*>(&Chained<&get_full_hook, 13>::call), 13);
    add(method(astera::swapchain, 15), reinterpret_cast<void*>(&Chained<&output_hook, 14>::call), 14);
    add(method(astera::swapchain, 16), reinterpret_cast<void*>(&Chained<&stats_hook, 15>::call), 15);
    add(method(astera::swapchain, 17), reinterpret_cast<void*>(&Chained<&count_hook, 16>::call), 16);
    add(method(astera::swapchain, 18), reinterpret_cast<void*>(&Chained<&desc1_hook, 17>::call), 17);
    add(method(astera::swapchain, 19), reinterpret_cast<void*>(&Chained<&full_desc_hook, 18>::call), 18);
    add(method(astera::swapchain, 20), reinterpret_cast<void*>(&Chained<&hwnd_get_hook, 19>::call), 19);
    add(method(astera::swapchain, 31), reinterpret_cast<void*>(&Chained<&set_latency_hook, 20>::call), 20);
    add(method(astera::swapchain, 32), reinterpret_cast<void*>(&Chained<&get_latency_hook, 21>::call), 21);
    add(method(astera::swapchain, 33), reinterpret_cast<void*>(&Chained<&waitable_hook, 22>::call), 22);
    add(method(astera::swapchain, 37), reinterpret_cast<void*>(&Chained<&color_check_hook, 23>::call), 23);
    add(method(astera::swapchain, 38), reinterpret_cast<void*>(&Chained<&color_set_hook, 24>::call), 24);
    add(method(astera::swapchain, 40), reinterpret_cast<void*>(&Chained<&hdr_hook, 25>::call), 25);
}
bool presentation_astera_swapchain_chains() noexcept {
    constexpr unsigned last = Presentation::hook_capacity - 1;
    for (unsigned i = 4; i <= last; ++i) if (!chain_next[i]) return false;
    return true;
}
Presentation::Presentation(Streamline& sdk) : state_(new State(sdk)) {
    State* expected{}; require(dispatch.compare_exchange_strong(expected, state_), "one-presentation-dispatch-per-process");
}
void Presentation::install(ID3D12Device* device, IDXGIFactory2* factory, PresentationContract contract) {
    auto& s = *state_; std::lock_guard lock(s.mutex); Bypass bypass(true);
    require(device && factory && !s.device && !s.stopped &&
        (contract == PresentationContract::synchronous_native || contract == PresentationContract::sdk_paced),
        "presentation-install-arguments");
    s.contract = contract;
    s.device_nodes = device->GetNodeCount();
    s.device = device; s.factory = factory;
    s.proxy_device = s.device; s.proxy_factory = s.factory;
    api(upgrade_owned(s.proxy_device, [&](void** out) { return s.sdk.upgrade(out); }), "presentation-upgrade-device");
    api(upgrade_owned(s.proxy_factory, [&](void** out) { return s.sdk.upgrade(out); }), "presentation-upgrade-factory");
    require(native<ID3D12Device>(s.sdk, s.proxy_device.Get()).Get() == device, "presentation-device-identity");
    require(native<IDXGIFactory2>(s.sdk, s.proxy_factory.Get()).Get() == factory, "presentation-factory-identity");
    if (astera::active()) {
        // AL installs the device and factory chains before DEVICE_CREATED.
        for (unsigned i = 0; i < 4; ++i) require(chain_next[i] != nullptr, "presentation-al-device-factory-chains");
    } else {
        const auto initialized = MH_Initialize();
        require(initialized == MH_OK || initialized == MH_ERROR_ALREADY_INITIALIZED, "presentation-hook-initialize");
    }
    s.add(0, device, 8, reinterpret_cast<void*>(&queue_hook));
    s.add(1, factory, 10, reinterpret_cast<void*>(&swap_hook));
    s.add(2, factory, 15, reinterpret_cast<void*>(&hwnd_hook));
    s.add(3, factory, 16, reinterpret_cast<void*>(&core_hook));
    s.stats.native_device = reinterpret_cast<std::uintptr_t>(device);
    s.accepting.store(true); s.enable();
}
PresentationStats Presentation::snapshot() const {
    auto& s = *state_; std::lock_guard lock(s.mutex);
    auto stats = s.stats; stats.routing = s.accepting.load(); stats.restored = s.stopped;
    stats.real_chain = s.proxy_swap != nullptr;
    stats.late_queues = s.late_queues.load();
    for (std::size_t i = 0; i < s.late_callers.size(); ++i) stats.late_queue_callers[i] = s.late_callers[i].load();
    stats.pending_calls = s.active.load();
    stats.frame_generation_loaded = s.contract == PresentationContract::sdk_paced;
    stats.switch_pending = s.switch_request.load();
    for (const auto& head : s.heads) if (head.enabled) ++stats.installed_hooks;
    return stats;
}
bool Presentation::request_switch(bool frame_generation) noexcept {
    auto& s = *state_;
    if (!s.identity_key.load() || !s.sdk.frame_generation_requested()) return false;
    s.switch_request.store(frame_generation ? 2U : 1U);
    return true;
}
void Presentation::waitable_slot(std::uintptr_t address) noexcept {
    auto& s = *state_; std::lock_guard lock(s.mutex);
    s.waitable_slot = address;
    // The handle the game keeps there, whoever asked first (v0.28.6).
    if (HANDLE held{}; slot_handle(address, held) && held) s.game_waitable = held;
}
void Presentation::observe(PresentationObserver observer) {
    auto& s = *state_; std::lock_guard lock(s.mutex);
    require(s.accepting.load() && !s.stopped && observer.context && observer.begin && observer.end,
        "presentation-observer-arguments");
    require(!s.observer.context, "presentation-observer-already-set");
    s.observer = observer;
}
void Presentation::ownership(std::uintptr_t image,unsigned bytes) {
    auto& s=*state_;std::lock_guard lock(s.mutex);
    require(image && bytes && image<=UINTPTR_MAX-bytes && !s.device && !s.stopped && !s.diagnostic_image,"ownership-before-install");
    s.diagnostic_image=image;s.diagnostic_image_bytes=bytes;
}
void Presentation::ownership_running() {
    auto& s=*state_;std::lock_guard lock(s.mutex);
    require(s.diagnostic_image && s.accepting.load() && !s.stopped && !s.ownership.phase,"ownership-running-once");
    s.ownership.phase=1;
}
void Presentation::write_ownership(std::ostream& out) const {
    auto& s=*state_;std::lock_guard lock(s.mutex);if(!s.diagnostic_image)return;
    const auto& t=s.ownership;LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
    const auto rva=[&](std::uint64_t at){return at>=s.diagnostic_image && at-s.diagnostic_image<s.diagnostic_image_bytes?at-s.diagnostic_image:0;};
    out<<"{\"event\":\"ownership_summary\",\"schema\":1,\"image\":"<<s.diagnostic_image<<",\"image_bytes\":"<<s.diagnostic_image_bytes
       <<",\"qpc_frequency\":"<<frequency.QuadPart<<",\"calls\":"<<t.calls<<",\"buckets\":"<<t.bucket_count<<",\"samples\":"<<t.sample_count
       <<",\"bucket_limit\":"<<t.bucket_limit<<",\"sample_limit\":"<<t.sample_limit<<",\"dropped_buckets\":"<<t.dropped_buckets
       <<",\"dropped_samples\":"<<t.dropped_samples<<",\"phase\":"<<t.phase<<",\"drained\":"<<(s.stopped && !s.active.load())
       <<",\"pointers_owned\":false,\"known_bucket_counts_exact\":true,\"sampling\":\"first-four-then-powers-of-two-to-65536\"}\n";
    for(unsigned i=0;i<t.bucket_count;++i){const auto& b=t.buckets[i];
        out<<"{\"event\":\"ownership_bucket\",\"method\":"<<b.method<<",\"phase\":"<<b.phase<<",\"thread\":"<<b.thread
           <<",\"caller\":"<<b.caller<<",\"caller_rva\":"<<rva(b.caller)<<",\"count\":"<<b.count<<"}\n";}
    for(unsigned i=0;i<t.sample_count;++i){const auto& v=t.samples[i];
        out<<"{\"event\":\"ownership_sample\",\"method\":"<<v.method<<",\"phase\":"<<v.phase<<",\"thread\":"<<v.thread
           <<",\"caller\":"<<v.caller<<",\"caller_rva\":"<<rva(v.caller)<<",\"sequence\":"<<v.sequence<<",\"qpc\":"<<v.qpc
           <<",\"object\":"<<v.object<<",\"returned\":"<<v.returned<<",\"result\":"<<v.result<<",\"args\":[";
        for(unsigned n=0;n<v.args.size();++n){if(n)out<<',';out<<v.args[n];}out<<"]}\n";}
    out<<std::flush;
}
bool Presentation::stop() noexcept {
    auto& s = *state_; bool ok = true;
    // First synchronize with dynamic hook installation in a creation callback.
    // Under AL the chains stay and identity calls keep going to Streamline's
    // proxy, which the caller would then shut down under them: refused while
    // the identity chain exists (v0.28.6; the caller keeps SDK and pins).
    { std::lock_guard lock(s.mutex);
      if (astera::active() && s.identity_key.load()) return false;
      s.accepting.store(false); }
    // No route mutex while MinHook suspends threads or callbacks drain.
    // Under AL the chains stay: with accepting off every detour forwards.
    for (auto& head : s.heads) if (head.enabled) {
        const auto status = astera::active() ? MH_OK : MH_DisableHook(reinterpret_cast<void*>(head.target));
        if (status == MH_OK || status == MH_ERROR_DISABLED) head.enabled = false; else ok = false;
    }
    const auto began = GetTickCount64();
    while (s.active.load() && GetTickCount64() - began < 30000) Sleep(1);
    if (s.active.load()) return false;
    std::lock_guard lock(s.mutex);
    if (astera::active()) { s.stopped = ok; return ok; }
    for (auto& head : s.heads) if (head.created) {
        head.restored = std::memcmp(reinterpret_cast<void*>(head.target), head.original.data(), sizeof(head.original)) == 0;
        ok = head.restored && ok;
    }
    s.stopped = ok; return ok;
}
bool Presentation::release_after_shutdown() noexcept {
    auto& s = *state_; std::lock_guard lock(s.mutex); Bypass bypass(true);
    if (!s.stopped || s.active.load() || s.sdk.initialized()) return false;
    s.swap.Reset(); s.proxy_swap.Reset();
    // The identity chain holds the creation queue (and with it the device).
    s.identity_key.store(0); s.identity.Reset(); s.creation.restrict_to.Reset();
    for (auto& q : s.queues) { q.proxy.Reset(); q.native = nullptr; }
    s.proxy_factory.Reset(); s.factory.Reset(); s.proxy_device.Reset(); s.device.Reset();
    s.released = true; return true;
}
const std::array<PresentationHook, Presentation::hook_capacity>& Presentation::hooks() const noexcept { return state_->heads; }
void Presentation::lifecycle(LifecycleListener listener) {
    auto& s = *state_; std::lock_guard lock(s.mutex);
    require(s.accepting.load() && !s.stopped && listener.context && listener.before && !s.listener.before,
        "presentation-lifecycle-listener");
    s.listener = listener;
    s.listener_ready.store(true, std::memory_order_release);
}
unsigned Presentation::drain_lifecycle(std::array<LifecycleEvent, lifecycle_capacity>& out, std::uint64_t& dropped) noexcept {
    auto& s = *state_; AcquireSRWLockExclusive(&s.lifecycle_lock);
    const auto count = static_cast<unsigned>(s.lifecycle_written - s.lifecycle_read);
    for (unsigned i = 0; i < count; ++i) out[i] = s.lifecycle_ring[(s.lifecycle_read + i) % lifecycle_capacity];
    s.lifecycle_read = s.lifecycle_written; dropped = s.lifecycle_dropped;
    ReleaseSRWLockExclusive(&s.lifecycle_lock);
    return count;
}
bool scene_presentation_buffers(frame_generation::PresentationBuffers& out) noexcept {
    auto* s = dispatch.load(); if (!s) return false;
    AcquireSRWLockShared(&s->scene_lock); out = s->scene_buffers.snapshot(); ReleaseSRWLockShared(&s->scene_lock);
    return true;
}
}
