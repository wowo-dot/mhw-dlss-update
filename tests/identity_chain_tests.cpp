// The identity chain (docs/research/fg-live-toggle.md): the game holds a
// composition swap chain while the real chain on its window can be made
// again. Astera Loader hooks the swap-chain methods of the object the game
// receives and refuses (al.d3d12 unavailable for every plugin) when two hooked
// slots resolve to one function. This checks, on this Windows, that a
// composition chain and a window chain share every routed method and that the
// routed slots are distinct functions. WARP device, a hidden window; no game.
#include "../src/reflex/identity_chain.hpp"
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdio>
#include <set>

using Microsoft::WRL::ComPtr;
namespace {
int failures = 0;
void check(bool ok, const char *what) {
    if (!ok) {
        ++failures;
        std::printf("FAIL %s\n", what);
    }
}
void *slot(IUnknown *object, unsigned index) { return (*reinterpret_cast<void ***>(object))[index]; }
}  // namespace

int main() {
    ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) return std::printf("no DXGI factory\n"), 1;
    ComPtr<IDXGIAdapter> warp;
    ComPtr<ID3D12Device> device;
    if (FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))) ||
        FAILED(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
        return std::printf("no WARP device\n"), 1;
    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    if (FAILED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue)))) return std::printf("no queue\n"), 1;

    WNDCLASSW cls{};
    cls.lpfnWndProc = DefWindowProcW;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"MhwIdentityChainTest";
    RegisterClassW(&cls);
    const HWND hwnd = CreateWindowExW(0, cls.lpszClassName, L"identity chain test", WS_OVERLAPPEDWINDOW, 0, 0, 640, 360,
                                      nullptr, nullptr, cls.hInstance, nullptr);
    if (!hwnd) return std::printf("no window\n"), 1;

    // The game's own description (cD3DRender::initForD3D12): three buffers,
    // flip discard, waitable and tearing flags.
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = 640;
    desc.Height = 360;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 3;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    ComPtr<IDXGIFactory2> f2;
    factory.As(&f2);
    ComPtr<IDXGISwapChain1> window_chain, identity;
    const auto made = f2->CreateSwapChainForHwnd(queue.Get(), hwnd, &desc, nullptr, nullptr, &window_chain);
    check(SUCCEEDED(made), "window chain");
    for (const auto format : {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM}) {
        auto d = mhw::reflex::identity_description(desc, format);
        identity.Reset();
        const auto hr = f2->CreateSwapChainForComposition(queue.Get(), &d, nullptr, &identity);
        std::printf("composition chain format %u: 0x%08lx\n", static_cast<unsigned>(format), static_cast<unsigned long>(hr));
        check(SUCCEEDED(hr) || format == DXGI_FORMAT_R10G10B10A2_UNORM, "identity chain in the game's SDR formats");
    }
    if (!window_chain || !identity) return std::printf("chains missing\n"), 1;

    // The same function behind every routed slot, and distinct functions.
    std::set<void *> targets;
    for (const auto index : mhw::reflex::identity_routed_slots) {
        const bool same = slot(window_chain.Get(), index) == slot(identity.Get(), index);
        if (!same) std::printf("slot %u differs between window and composition chains\n", index);
        check(same, "the identity chain shares the window chain's routed methods");
        check(targets.insert(slot(window_chain.Get(), index)).second, "each routed slot is its own function");
    }
    check(mhw::reflex::identity_same_class(window_chain.Get(), identity.Get()), "identity_same_class agrees");

    // The identity chain answers its own size until it is routed; resizing it
    // to follow the real chain works with no buffer referenced.
    auto d = mhw::reflex::identity_description(desc, desc.Format);
    d.Width = 800;
    d.Height = 450;
    check(SUCCEEDED(identity->ResizeBuffers(d.BufferCount, d.Width, d.Height, d.Format, d.Flags)), "identity chain resizes");
    DXGI_SWAP_CHAIN_DESC1 now{};
    check(SUCCEEDED(identity->GetDesc1(&now)) && now.Width == 800 && now.Height == 450, "identity chain size follows");

    // tifa-6 (v0.28.6): the identity chain's own Present with the game's
    // tearing flag fails (DXGI_ERROR_INVALID_CALL in the field; the game then
    // aborts), so a game call never reaches it (identity_route). Without a
    // real chain its resize goes to it without the game's flags.
    const auto own_present = identity->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    std::printf("identity chain Present(0, ALLOW_TEARING): 0x%08lx\n", static_cast<unsigned long>(own_present));
    check(FAILED(own_present), "the identity chain's own Present with the game's flags fails");
    const auto game_flags = identity->ResizeBuffers(3, 640, 360, desc.Format, desc.Flags);
    std::printf("identity chain ResizeBuffers with the game's flags: 0x%08lx\n", static_cast<unsigned long>(game_flags));
    check(FAILED(game_flags), "the identity chain refuses the game's resize flags");
    check(SUCCEEDED(identity->ResizeBuffers(3, 640, 360, desc.Format, 0)), "unroutable resize: the game's count and size, no flags");
    // The last resort: a window chain made again on the same window once the
    // previous one is released, with the game's description.
    window_chain.Reset();
    HRESULT again = E_ACCESSDENIED;
    for (unsigned attempt = 0; attempt < 25 && again == E_ACCESSDENIED; ++attempt) {
        if (attempt) Sleep(20);
        again = f2->CreateSwapChainForHwnd(queue.Get(), hwnd, &desc, nullptr, nullptr, &window_chain);
    }
    check(SUCCEEDED(again), "window chain made again");

    identity.Reset();
    window_chain.Reset();
    DestroyWindow(hwnd);
    UnregisterClassW(cls.lpszClassName, cls.hInstance);
    if (failures) return 1;
    std::printf("identity chain tests passed (%zu routed slots)\n", targets.size());
    return 0;
}
