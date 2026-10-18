// Observer per-call cost (v0.23.8 work; not a ctest). WARP command lists,
// native and with the binding observer's hooks installed (MinHook, as the
// research build; the plugin chains through Astera Loader instead).
// - one thread: a million SetGraphicsRootConstantBufferView calls;
// - N threads, each recording its own list (the observer's entries are
//   adjacent), a draw-like mix: topology, viewport, root CBV, table-less
//   constants. Contention or false sharing between the threads shows as a
//   per-call cost that grows with N.
#include "../src/temporal_present/observer.hpp"
#include "../src/perf/hook_timing.hpp"
#include "../src/perf/toggles.hpp"
#include "fake_astera.hpp"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>
namespace obs = mhw::temporal_present::observer;
using Microsoft::WRL::ComPtr;
static void require(bool v, const char *s) {
    if (!v) {
        std::fprintf(stderr, "%s\n", s);
        std::exit(1);
    }
}
struct Recorder {
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
};
static ComPtr<ID3D12Device> device;
static ComPtr<ID3D12RootSignature> root;
static void restart(Recorder &r) {
    require(SUCCEEDED(r.list->Close()) && SUCCEEDED(r.allocator->Reset()) &&
                SUCCEEDED(r.list->Reset(r.allocator.Get(), nullptr)),
            "reset");
    r.list->SetGraphicsRootSignature(root.Get());
}
// A draw-like group of four state calls; returns ns per call.
static double record(Recorder &r, unsigned groups) {
    const D3D12_VIEWPORT viewport{0, 0, 64, 64, 0, 1};
    const auto start = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < groups; ++i) {
        r.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        r.list->RSSetViewports(1, &viewport);
        r.list->SetGraphicsRootConstantBufferView(0, 0x10000ULL + 256ULL * (i & 255U));
        r.list->SetGraphicsRootConstantBufferView(0, 0x20000ULL + 256ULL * (i & 255U));
    }
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(end - start).count() / (4.0 * groups);
}
static double threaded(std::vector<Recorder> &recorders, unsigned threads, unsigned groups) {
    for (unsigned t = 0; t < threads; ++t) restart(recorders[t]);
    std::atomic<unsigned> ready{};
    std::vector<double> results(threads);
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < threads; ++t)
        pool.emplace_back([&, t] {
            ++ready;
            while (ready.load() < threads) {}
            results[t] = record(recorders[t], groups);
        });
    for (auto &p : pool) p.join();
    double sum{};
    for (const auto r : results) sum += r;
    return sum / threads;
}
int main(int argc, char **argv) {
    const unsigned max_threads = argc > 1 ? static_cast<unsigned>(std::atoi(argv[1])) : 8U;
    ComPtr<IDXGIFactory4> factory;
    ComPtr<IDXGIAdapter> adapter;
    require(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) &&
                SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))) &&
                SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))),
            "WARP device");
    D3D12_ROOT_PARAMETER parameter{};
    parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC desc{1, &parameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ComPtr<ID3DBlob> blob, errors;
    require(SUCCEEDED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors)) &&
                SUCCEEDED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                                      IID_PPV_ARGS(&root))),
            "root signature");
    std::vector<Recorder> recorders(max_threads);
    for (auto &r : recorders)
        require(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&r.allocator))) &&
                    SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, r.allocator.Get(), nullptr,
                                                        IID_PPV_ARGS(&r.list))),
                "list");
    constexpr unsigned groups = 100000;
    std::vector<double> native(max_threads + 1);
    for (unsigned n = 1; n <= max_threads; n *= 2) {
        native[n] = 1e9;
        for (unsigned k = 0; k < 3; ++k) native[n] = std::min(native[n], threaded(recorders, n, groups));
    }
    // The loader installs the observer's registered detours; the observer
    // checks them, with every hook the DLL installs.
    mhw::test::fake_astera::start();
    std::vector<mhw::astera::Hook> hooks;
    obs::astera_hooks(hooks);
    require(mhw::test::fake_astera::install_methods(hooks, {.command_list = recorders[0].list.Get()}),
            "loader installs the observer's detours");
    std::string reason;
    const auto resolver = [](std::uintptr_t p, unsigned slot, std::uintptr_t &target) noexcept {
        target = (*reinterpret_cast<std::uintptr_t **>(p))[slot];
        return target != 0;
    };
    mhw::perf::toggles.store(mhw::perf::all_toggles);
    require(obs::install(device.Get(), resolver, reason), reason.c_str());
    obs::begin();
    // Adjacent observer entries, one per list, from their Reset generations.
    for (unsigned t = 0; t < max_threads; ++t) {
        restart(recorders[t]);
        obs::reset(recorders[t].list.Get(), t + 1, nullptr);
    }
    for (unsigned n = 1; n <= max_threads; n *= 2) {
        double hooked = 1e9;
        for (unsigned k = 0; k < 3; ++k) hooked = std::min(hooked, threaded(recorders, n, groups));
        std::printf("%u thread(s): native %6.1f ns a call, observer %6.1f, overhead %6.1f\n", n, native[n], hooked,
                    hooked - native[n]);
    }
    // The in-game estimate on the same calls: hook timing's own time per call
    // against the overhead measured from outside.
    namespace ht = mhw::perf::hook_timing;
    ht::enable();
    const auto sum = [&](auto field) {
        std::uint64_t n{};
        for (unsigned c = ht::observer; c <= ht::observer_predicate; ++c) n += (ht::totals[c].*field).load();
        return n;
    };
    const auto timed_before = sum(&ht::Totals::timed), own_before = sum(&ht::Totals::own);
    const auto t0 = std::chrono::steady_clock::now();
    const unsigned rdtsc_probe_calls = 1000000;
    for (unsigned i = 0; i < rdtsc_probe_calls; ++i) static_cast<void>(__rdtsc());
    const double rdtsc_ns =
        std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / rdtsc_probe_calls;
    LARGE_INTEGER q0{}, q1{}, f{};
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&q0);
    const auto c0 = __rdtsc();
    const double timed_overhead = threaded(recorders, 1, groups);
    QueryPerformanceCounter(&q1);
    const auto c1 = __rdtsc();
    const double ticks_per_ns = double(c1 - c0) / (double(q1.QuadPart - q0.QuadPart) * 1e9 / double(f.QuadPart));
    const auto timed = sum(&ht::Totals::timed) - timed_before, own = sum(&ht::Totals::own) - own_before;
    std::printf("hook timing: %llu timed calls, own %.1f ns a call (rdtsc %.1f ns; with timing on: %.1f ns a call)\n",
                static_cast<unsigned long long>(timed), timed ? double(own) / ticks_per_ns / double(timed) : 0.0,
                rdtsc_ns, timed_overhead);
    mhw::temporal_present::Bindings state;
    require(obs::inspect(recorders[0].list.Get(), state) && state.known, "observer state");
    obs::stop();
    return 0;
}
