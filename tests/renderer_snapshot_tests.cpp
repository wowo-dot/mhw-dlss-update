#include "renderer_snapshot.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>
#include <windows.h>

namespace {
namespace bridge = mhw::bridge_probe;
int fail(const char *message) {
  std::cerr << message << '\n';
  return 1;
}
struct Fixture {
  static constexpr std::uintptr_t slot = 0x1000, root = 0x100000,
                                  backend = 0x200000, view = 0x300000;
  std::uintptr_t singleton{root};
  std::vector<std::byte> renderer = std::vector<std::byte>(128088);
  std::vector<std::byte> device = std::vector<std::byte>(272160);
  std::vector<std::byte> main_view = std::vector<std::byte>(96);
  std::uint32_t calls{}, fail_on{};
  bool change_root{}, change_backend{};
  bridge::ScalarReadResult failure{bridge::ScalarReadResult::fault};
  template <typename T>
  void put(std::vector<std::byte> &bytes, std::size_t offset, T value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
  }
  Fixture() {
    put(renderer, 120, backend);
    put(renderer, 165, std::uint8_t{1});
    put(renderer, 180, std::uint32_t{1});
    put(renderer, 176, 1.0F);
    put(renderer, 408, std::uint32_t{2560});
    put(renderer, 412, std::uint32_t{1440});
    put(renderer, 516, 1.0F);
    put(renderer, 127504, std::uint64_t{100});
    put(renderer, 128080, view);
    put(main_view, 88, std::uint32_t{0});
    put(device, 272152, std::uint8_t{1});
    put(device, 272128, std::uintptr_t{0x400000});
  }
  static bridge::ScalarReadResult read(void *opaque, std::uintptr_t address,
                                       std::span<std::byte> output) noexcept {
    auto &f = *static_cast<Fixture *>(opaque);
    ++f.calls;
    if (f.calls == f.fail_on)
      return f.failure;
    if (address == slot && output.size() == sizeof(f.singleton)) {
      const auto value = f.change_root && f.calls > 1 ? root + 16 : f.singleton;
      std::memcpy(output.data(), &value, output.size());
      return bridge::ScalarReadResult::success;
    }
    if (f.change_backend && f.calls > 13 && address == root + 120) {
      const auto value = backend + 16;
      std::memcpy(output.data(), &value, output.size());
      return bridge::ScalarReadResult::success;
    }
    for (const auto &[base, bytes] :
         std::array<std::pair<std::uintptr_t, const std::vector<std::byte> *>,
                    3>{std::pair{root, &f.renderer},
                       std::pair{backend, &f.device},
                       std::pair{view, &f.main_view}}) {
      if (address >= base && address - base <= bytes->size() &&
          output.size() <= bytes->size() - (address - base)) {
        std::memcpy(output.data(), bytes->data() + (address - base),
                    output.size());
        return bridge::ScalarReadResult::success;
      }
    }
    return bridge::ScalarReadResult::unreadable;
  }
  bridge::RendererSnapshot capture() {
    return bridge::capture_renderer_snapshot(slot, read, this);
  }
};
} // namespace

int main() {
  using bridge::RendererSnapshotStatus;
  Fixture good;
  const auto baseline = good.capture();
  if (baseline.status != RendererSnapshotStatus::gates_ready ||
      baseline.reads != 16 || good.calls != 16 || !baseline.pointers_stable ||
      baseline.frame_index != 100 || baseline.present_width != 2560)
    return fail("valid graph was not captured within the scalar budget");
  for (std::uint32_t ordinal = 1; ordinal <= baseline.reads; ++ordinal) {
    Fixture f;
    f.fail_on = ordinal;
    auto s = f.capture();
    if (s.status != RendererSnapshotStatus::read_failed ||
        s.read_result != bridge::ScalarReadResult::fault ||
        f.calls != ordinal || s.failed_field == bridge::RendererReadField::none)
      return fail("capture continued reading after a fault");
  }
  Fixture absent;
  absent.singleton = 0;
  if (absent.capture().status != RendererSnapshotStatus::renderer_absent ||
      absent.calls != 1)
    return fail("null renderer was dereferenced");
  Fixture changed;
  changed.change_root = true;
  if (changed.capture().status != RendererSnapshotStatus::pointers_changed ||
      changed.calls != 14)
    return fail("replaced singleton was treated as stable or followed");
  Fixture changed_backend;
  changed_backend.change_backend = true;
  if (changed_backend.capture().status !=
      RendererSnapshotStatus::pointers_changed)
    return fail("replaced backend was treated as stable");
  for (int scenario = 0; scenario < 5; ++scenario) {
    Fixture f;
    if (scenario == 0)
      f.put(f.renderer, 165, std::uint8_t{0});
    if (scenario == 1)
      f.put(f.renderer, 180, std::uint32_t{2});
    if (scenario == 2)
      f.put(f.renderer, 120, std::uintptr_t{0});
    if (scenario == 3)
      f.put(f.device, 272152, std::uint8_t{0});
    if (scenario == 4)
      f.put(f.device, 272128, std::uintptr_t{0});
    const auto s = f.capture();
    if (s.status != RendererSnapshotStatus::gates_blocked ||
        !(s.block_mask & (1U << scenario)))
      return fail("a missing render prerequisite was classified ready");
  }
  Fixture no_view;
  no_view.put(no_view.renderer, 128080, std::uintptr_t{0});
  const auto no_view_snapshot = no_view.capture();
  if (no_view_snapshot.main_view_present || no_view_snapshot.reads != 15 ||
      no_view_snapshot.status != RendererSnapshotStatus::gates_ready)
    return fail("main-view context must remain separate from the four DLSS "
                "selector gates");
  for (int scenario = 0; scenario < 6; ++scenario) {
    Fixture f;
    if (scenario == 0)
      f.put(f.renderer, 165, std::uint8_t{2});
    if (scenario == 1)
      f.put(f.device, 272152, std::uint8_t{2});
    if (scenario == 2)
      f.put(f.renderer, 408, std::uint32_t{0});
    if (scenario == 3)
      f.put(f.renderer, 412, std::uint32_t{16385});
    if (scenario == 4)
      f.put(f.renderer, 176, std::numeric_limits<float>::quiet_NaN());
    if (scenario == 5)
      f.put(f.renderer, 516, std::numeric_limits<float>::infinity());
    if (f.capture().status != RendererSnapshotStatus::invalid_values)
      return fail("invalid scalar graph was accepted");
  }
  auto advanced = baseline;
  advanced.frame_index += 100;
  if (!bridge::same_renderer_gate_values(baseline, advanced))
    return fail("frame progression should not flood gate logs");
  advanced.sizing_flags = 0;
  if (bridge::same_renderer_gate_values(baseline, advanced))
    return fail("changed selector flags were suppressed");
  Fixture overflow;
  overflow.singleton = std::numeric_limits<std::uintptr_t>::max() - 100;
  if (overflow.capture().status != RendererSnapshotStatus::read_failed ||
      overflow.calls != 1)
    return fail("address arithmetic wrapped into a foreign read");

  SYSTEM_INFO system{};
  GetSystemInfo(&system);
  const auto page = static_cast<std::size_t>(system.dwPageSize);
  auto *memory = static_cast<std::byte *>(VirtualAlloc(
      nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
  if (!memory)
    return fail("test allocation failed");
  struct Owner {
    void *address;
    ~Owner() { VirtualFree(address, 0, MEM_RELEASE); }
  } owner{memory};
  const std::uint64_t value = 0x1234567890ABCDEFULL;
  std::memcpy(memory, &value, sizeof(value));
  std::array<std::byte, 8> readback{};
  auto read = [&](std::uintptr_t p) {
    return bridge::read_bounded_scalar(nullptr, p, readback);
  };
  if (read(reinterpret_cast<std::uintptr_t>(memory)) !=
          bridge::ScalarReadResult::success ||
      std::memcmp(readback.data(), &value, sizeof(value)) != 0)
    return fail("readable committed scalar failed");
  DWORD previous{};
  if (!VirtualProtect(memory + page, page, PAGE_NOACCESS, &previous))
    return fail("test protection failed");
  if (read(reinterpret_cast<std::uintptr_t>(memory + page - 4)) !=
          bridge::ScalarReadResult::unreadable ||
      read(reinterpret_cast<std::uintptr_t>(memory + page)) !=
          bridge::ScalarReadResult::unreadable ||
      read(0) != bridge::ScalarReadResult::unreadable)
    return fail("unsafe memory boundary was read");
  if (!VirtualProtect(memory, page, PAGE_READWRITE | PAGE_GUARD, &previous))
    return fail("test guard failed");
  if (read(reinterpret_cast<std::uintptr_t>(memory)) !=
      bridge::ScalarReadResult::unreadable)
    return fail("guard page was consumed");
  MEMORY_BASIC_INFORMATION information{};
  VirtualQuery(memory, &information, sizeof(information));
  if (!(information.Protect & PAGE_GUARD))
    return fail("guard flag changed during read rejection");
  std::array<std::byte, 9> too_large{};
  if (bridge::read_bounded_scalar(
          nullptr, reinterpret_cast<std::uintptr_t>(memory), too_large) !=
      bridge::ScalarReadResult::unreadable)
    return fail("scalar size budget was bypassed");
  std::cout
      << "Renderer gates, bounded reads and interrupted snapshots passed\n";
  return 0;
}
