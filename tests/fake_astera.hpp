#pragma once
// A stand-in for Astera Loader in tests, on MinHook: its "al.hooks" table for
// game functions and its install of the registered al.d3d12 method detours.
// The shipped code under test then runs its Astera Loader path, the only one
// the DLL has. Tests only: the shipped DLL links no hooking library.
#include "../src/astera/astera_hooks.hpp"
#include <cstdint>
#include <vector>

namespace mhw::test::fake_astera {
// Initializes MinHook and hands the shipped code the fake loader:
// astera::active() is true and astera::game_hook() reaches the fake al.hooks.
// Like AL, a game hook's pattern must match the bytes at its address.
void start();
// AL's install of the al.d3d12 detours registered at PRE_MAIN (what DEVICE_CREATED
// does): each hook's method (family, slot) is detoured on the matching
// object's vtable and its *next receives the original. A family without an
// object fails.
struct Objects {
    void *factory{}, *swapchain{}, *device{}, *queue{}, *command_list{};
};
bool install_methods(const std::vector<astera::Hook> &, const Objects &);
// Game hooks the fake has active (registered and not disabled).
unsigned active_game_hooks();
} // namespace mhw::test::fake_astera
