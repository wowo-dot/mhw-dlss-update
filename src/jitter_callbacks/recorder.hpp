#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace mhw::jitter {
inline constexpr unsigned capacity = 256;
inline constexpr unsigned window_ms = 1500;
enum class Stop : unsigned { idle, recording, deadline, capacity, invalid_sample, requested };
struct Layout {
  std::uintptr_t renderer_slot{}, camera_slot{}, manager_slot{}, layer_vtable{};
  std::array<std::uintptr_t, 2> camera_vtables{};
};
struct Snapshot {
  unsigned failure{}, reads{};
  std::uintptr_t renderer{}, camera{}, manager{}, view{}, layer{}, camera_type{}, layer_type{};
  std::uint64_t frame{}, frame_after{}, camera_jitter{}, setter_input{}, layer_mask{}, manager_mask{};
  unsigned epoch{}, epoch_after{}, camera_epoch{}, view_flags{}, reset_mode{};
  unsigned layer_state{}, layer_flags{}, blend_bits{}, taa_bytes{};
  bool stable{}, has_layer{}, has_input{};
};
struct Call {
  std::uint64_t token{}, parent{}, tick{}, exit_tick{}, entry_order{}, exit_order{}, previous_layer{};
  std::uintptr_t argument{}, input{}, caller{};
  unsigned kind{}, thread{};
  Snapshot before{}, after{};
  bool complete{};
};
struct State {
  unsigned phase{}, reserved{}, pending{};
  std::uint64_t started{}, dropped{};
  Stop stop{};
};
// Control/reading functions are worker-owned; hook entries use bounded storage.
bool install_verified(std::uintptr_t base, std::string &reason);
bool arm(unsigned phase, std::uint64_t tick = 0) noexcept;
void poll(std::uint64_t tick = 0) noexcept;
void stop() noexcept;
State state() noexcept;
bool copy_call(unsigned index, Call &out) noexcept;
// Restore target bytes. Keep DLL and trampolines resident until process exit:
// an already-entered wrapper may still need its original function.
bool disable() noexcept;
std::string file_sha256(const std::wstring &path);
bool host_identity(std::uintptr_t base, std::string &reason);
std::string event_name(unsigned pid, std::uint64_t creation, unsigned phase);
inline constexpr char host_sha[] = "c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea";
#ifdef MHW_JITTER_OWNED_FIXTURE
bool install_owned(std::array<std::uintptr_t, 3> targets, Layout layout) noexcept;
bool remove_owned() noexcept; // Only after fixture callers have joined.
#endif
}
