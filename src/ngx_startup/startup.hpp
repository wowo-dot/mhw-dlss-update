#pragma once
#include <d3d12.h>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include "../dlss_sdk/live.hpp"

namespace mhw::ngx_startup {
inline constexpr unsigned entry_rva=0x273f230, caller_rva=0x23842dc;
inline constexpr std::uint64_t native_app=0x05f8a85f;
struct Config {
    void *target{};
    std::uintptr_t caller{},client_slots{};
    std::filesystem::path runtime,data;
    std::uintptr_t backend_slot{}; // production renderer slot; zero only in the isolated fixture
    bool hook_pool_reserved{}; // only the same DLL's successful startup reservation
};
// One startup transaction. Pinned caller retains the configuration and stream.
// Only disable the hook; retain the trampoline and SDK/device until process exit.
bool install(const Config &,std::ostream &);
bool done() noexcept;
bool disable() noexcept;
bool settled() noexcept;
bool supported() noexcept;
unsigned calls() noexcept;
bool write_graphics_snapshot() noexcept;
const dlss_sdk::BorrowedInitialization *borrowed_initialization() noexcept;
bool run(HMODULE,const char *version,bool hook_pool_reserved=false) noexcept;
}
