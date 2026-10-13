// v0.24.0: the video memory check that replaced the fixed size ceilings.
#include "../src/gpu_memory/budget.hpp"
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <cstdint>
#include <cstdio>

namespace {
namespace gm = mhw::gpu_memory;
int failures{};
void check(bool ok, const char *what) {
    if (!ok) { std::printf("FAIL: %s\n", what); ++failures; }
}

void pure_tests() {
    check(gm::room({1000, 400}) == 600 && gm::room({1000, 1000}) == 0 && gm::room({1000, 1200}) == 0, "room left in the budget");
    check(gm::fits({1000, 400}, 600) && !gm::fits({1000, 400}, 601) && gm::fits({1000, 1200}, 0) && !gm::fits({1000, 1200}, 1),
          "fits only inside the budget, also when the game is over it");
    check(gm::texture_bytes(1, 1, 4) == 65536 && gm::texture_bytes(128, 128, 4) == 65536 && gm::texture_bytes(129, 128, 4) == 131072,
          "textures round up to 64 KiB");
    // 4K DLAA: copies 12, converted 28, output 16, handoff 4, spatial 4 bytes a pixel.
    const std::uint64_t k4 = 3840ULL * 2160;
    const auto dlaa = gm::dlss_bytes(3840, 2160, 3840, 2160);
    check(dlaa >= 64 * k4 && dlaa < 64 * k4 + 9 * 65536, "4K DLAA images");
    check(gm::dlss_bytes(3840, 2160, 3840, 2160, false) == dlaa - 3 * gm::texture_bytes(3840, 2160, 4), "a preset change keeps the copies");
    // Quality at 4K renders 2560x1440: the converted inputs shrink.
    const auto inputs = [](unsigned w, unsigned h) {
        return gm::texture_bytes(w, h, 16) + gm::texture_bytes(w, h, 4) + gm::texture_bytes(w, h, 8);
    };
    check(gm::dlss_bytes(3840, 2160, 2560, 1440) == dlaa - inputs(3840, 2160) + inputs(2560, 1440), "render-size inputs");
    // The largest side D3D12 allows does not overflow (about 17 GiB at DLAA).
    const auto largest = gm::dlss_bytes(16384, 16384, 16384, 16384);
    check(largest >= 64ULL * 16384 * 16384 && largest < 65ULL * 16384 * 16384, "16384x16384 without overflow");
}

// The game's adapter: pinned once; the budget is read through it.
void adapter_tests() {
    gm::Local m;
    check(!gm::local(m), "no budget before an adapter is pinned");
    const auto unread = gm::check(1);
    check(unread.done && !unread.read && !unread.passed, "fails closed without a budget");
    check(!gm::pin(nullptr), "a null adapter is not pinned");
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter3;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))) || FAILED(factory->EnumAdapters1(0, &adapter)) ||
        FAILED(adapter.As(&adapter3))) {
        check(false, "an adapter for the budget test");
        return;
    }
    check(gm::pin(adapter3.Get()) && gm::pinned.load() == adapter3.Get(), "pinned");
    Microsoft::WRL::ComPtr<IDXGIAdapter1> second;
    Microsoft::WRL::ComPtr<IDXGIAdapter3> second3;
    if (SUCCEEDED(factory->EnumAdapters1(0, &second)) && SUCCEEDED(second.As(&second3)))
        check(gm::pin(second3.Get()) && gm::pinned.load() == adapter3.Get(), "a later pin keeps the first adapter");
    check(gm::local(m), "the budget is read");
    const auto none = gm::check(0), all = gm::check(UINT64_MAX);
    check(none.read && none.passed && none.needed == 0, "nothing needed fits");
    check(all.read && !all.passed && all.needed == UINT64_MAX, "more than any budget is refused");
}
}  // namespace

int main() {
    pure_tests();
    adapter_tests();
    if (failures) return 1;
    std::printf("gpu_memory_tests: ok\n");
    return 0;
}
