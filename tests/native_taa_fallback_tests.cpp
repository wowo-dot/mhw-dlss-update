#include "native_taa_fallback.hpp"
#include <windows.h>
#include <atomic>
#include <iostream>
#include <stdexcept>

namespace { std::atomic<unsigned> producer_calls{}; }
extern "C" __declspec(noinline) std::uintptr_t __fastcall mhw_taa_fixture_producer(std::uintptr_t args) {
  producer_calls.fetch_add(1); SetLastError(73); return args + 17;
}
extern "C" std::uintptr_t mhw_taa_fixture_call(std::uintptr_t);
extern "C" unsigned char mhw_taa_fixture_return;
int main() {
  namespace b = mhw::bridge_probe;
  try {
    auto check = [](bool value, const char *why) { if (!value) throw std::runtime_error(why); };
    check(!b::install_native_taa_fallback(0), "unverified target must reject");
    check(!b::install_native_taa_fallback(reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))), "foreign image must reject");
    check(b::install_owned_taa_fallback(reinterpret_cast<std::uintptr_t>(&mhw_taa_fixture_producer),
        reinterpret_cast<std::uintptr_t>(&mhw_taa_fixture_return)), "owned target hook");
    std::uintptr_t handle = 123, slot = reinterpret_cast<std::uintptr_t>(&handle);
    SetLastError(51);
    check(mhw_taa_fixture_call(reinterpret_cast<std::uintptr_t>(&slot)) == 0 && GetLastError() == 51,
          "known optional producer suppressed with error state preserved");
    check(producer_calls == 0 && handle == 123, "no original producer or host field write");
    handle = 0;
    check(mhw_taa_fixture_call(reinterpret_cast<std::uintptr_t>(&slot)) == 0, "empty handle suppression");
    check(mhw_taa_fixture_call(1) == 0, "unreadable args suppress work without a fault");
    check(mhw_taa_fixture_producer(100) == 117 && GetLastError() == 73 && producer_calls == 1,
          "unrelated caller keeps original return and side effects");
    const auto stats = b::native_taa_fallback_stats();
    check(stats.installed && stats.suppressed == 3 && stats.with_handle == 1 && stats.unreadable == 1 && stats.other_callers == 1,
          "bounded counter telemetry");
    check(b::remove_owned_taa_fallback(), "owned hook clean removal");
    check(mhw_taa_fixture_call(100) == 117 && producer_calls == 2, "original restored");
    std::cout << "PASS: real owned hook, exact caller routing, suppression, unreadable input, transparent other caller, removal\n";
    return 0;
  } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
