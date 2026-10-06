#pragma once
#include <cstdint>
#include <string>

namespace mhw::jitter_writer {
inline constexpr unsigned capacity = 64;
inline constexpr std::uint64_t window_ms = 180000, visit_limit = 1000000;
inline constexpr char host_sha[] = "c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea";
enum class Stop : unsigned { idle, recording, requested, deadline, capacity, visits, invalid };
struct Layout { std::uintptr_t name{}, layer_vtable{}, track_caller{}; };
struct Snapshot {
  std::uintptr_t name{}, owner{}, type{}, target{};
  unsigned flags{}, index{}, value{}, failure{};
};
struct TrackSnapshot {
  bool applicable{};
  std::uintptr_t record{}, header{}, owner{}, keys{}, values{};
  unsigned kind{}, count{}, index{}, key_word{}, raw_value{}, failure{};
};
struct Call {
  std::uint64_t token{}, tick{}, exit_tick{}, entry_order{}, exit_order{};
  std::uintptr_t descriptor{}, caller{};
  unsigned input{}, thread{};
  Snapshot before{}, after{};
  TrackSnapshot track_before{}, track_after{};
  bool complete{};
};
struct State {
  Stop stop{};
  unsigned reserved{}, pending{};
  std::uint64_t started{}, visits{}, named{}, other_type{}, dropped{}, read_failures{};
};
bool install_verified(std::uintptr_t base, std::string &reason);
bool arm(std::uint64_t tick = 0) noexcept;
void poll(std::uint64_t tick = 0) noexcept;
void stop() noexcept;
State state() noexcept;
bool copy_call(unsigned index, Call &out) noexcept;
bool disable() noexcept;
std::string file_sha256(const std::wstring &path);
bool host_identity(std::uintptr_t base, std::string &reason);
#ifdef MHW_WRITER_OWNED_FIXTURE
bool install_owned(std::uintptr_t target, Layout layout) noexcept;
bool remove_owned() noexcept; // Fixture joins all callers before this operation.
#endif
}
