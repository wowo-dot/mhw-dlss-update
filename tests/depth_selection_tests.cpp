#include "depth_selection.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {
namespace b = mhw::bridge_probe;
using A = std::uintptr_t;
void check(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
template<class T, class B> void put(B &bytes, std::size_t offset, T v) { std::memcpy(bytes.data() + offset, &v, sizeof(v)); }
template<class T, class B> T get(const B &bytes, std::size_t offset) { T v{}; std::memcpy(&v, bytes.data() + offset, sizeof(v)); return v; }
template<class B> A address(const B &bytes) { return reinterpret_cast<A>(bytes.data()); }
struct Fixture {
  static constexpr A base = 0x140000000;
  std::array<std::byte, 280> owner{};
  std::array<std::byte, 128> backing{};
  std::array<std::byte, 48> tracker{};
  unsigned reads{}, fail_on{};
  bool change_primary{};
  Fixture() {
    put(owner, 0, base + 0x351c870); put(owner, 88, std::uint32_t{5});
    put(owner, 96, address(backing)); put(owner, 112, std::uint32_t{0x600000});
    put(owner, 136, std::uint32_t{2560 | (1440 << 16)});
    put(owner, 144, std::uint32_t{16}); put(owner, 148, std::uint32_t{0xffffffff});
    put(backing, 32, A{0x12345678}); put(backing, 112, address(tracker));
  }
  b::DepthSelectionRequest request() const {
    return {base, base + 0x229e89e, 0, address(owner), address(owner), 0x1000, 4, true};
  }
  static b::ScalarReadResult read(void *opaque, A a, std::span<std::byte> out) noexcept {
    auto &f = *static_cast<Fixture *>(opaque);
    ++f.reads;
    if (f.reads == f.fail_on) return b::ScalarReadResult::unreadable;
    if (f.change_primary && f.reads > 10 && a == address(f.owner) + 96) {
      A changed = address(f.backing) + 16;
      std::memcpy(out.data(), &changed, out.size()); return b::ScalarReadResult::success;
    }
    for (auto region : {std::span<const std::byte>(f.owner), std::span<const std::byte>(f.backing)}) {
      A lo = address(region);
      if (a >= lo && a - lo <= region.size() && out.size() <= region.size() - (a - lo)) {
        std::memcpy(out.data(), region.data() + a - lo, out.size()); return b::ScalarReadResult::success;
      }
    }
    return b::ScalarReadResult::unreadable;
  }
  b::DepthSelectionResult select() { return b::select_depth_backing(request(), read, this); }
};

void selector_tests() {
  Fixture good;
  auto before_owner = good.owner; auto before_backing = good.backing;
  const auto result = good.select();
  check(result.action == b::DepthSelectionAction::primary && result.backing == address(good.backing), "primary selection");
  check(result.reads == 15 && good.owner == before_owner && good.backing == before_backing, "bounded read-only selection");
  for (unsigned ordinal = 1; ordinal <= result.reads; ++ordinal) {
    Fixture f; f.fail_on = ordinal;
    check(f.select().action == b::DepthSelectionAction::reject_work, "failed read must reject optional work");
  }
  for (unsigned variant = 0; variant != 10; ++variant) {
    Fixture f;
    switch (variant) {
    case 0: put(f.owner, 0, A{1}); break;
    case 1: put(f.owner, 88, std::uint32_t{6}); break;
    case 2: put(f.owner, 96, A{0}); break;
    case 3: put(f.owner, 112, std::uint32_t{0}); break;
    case 4: put(f.owner, 136, std::uint32_t{0}); break;
    case 5: put(f.owner, 144, std::uint32_t{17}); break;
    case 6: put(f.owner, 240, A{2}); break;
    case 7: put(f.backing, 32, A{0}); break;
    case 8: put(f.backing, 112, A{0}); break;
    case 9: f.change_primary = true; break;
    }
    check(f.select().action == b::DepthSelectionAction::reject_work, "layout/lifetime mismatch");
  }
  for (unsigned variant = 0; variant != 5; ++variant) {
    Fixture f; auto r = f.request();
    if (variant == 0) r.host_contract_verified = false;
    if (variant == 1) r.requested_state = 8;
    if (variant == 2) r.view_depth_owner = 1;
    if (variant == 3) r.raw_depth_owner = r.owner;
    if (variant == 4) r.host_base = ~A{};
    check(b::select_depth_backing(r, Fixture::read, &f).action == b::DepthSelectionAction::reject_work, "request mismatch");
  }
  auto r = good.request(); r.return_address++;
  check(b::select_depth_backing(r, Fixture::read, &good).action == b::DepthSelectionAction::unchanged, "other callers unchanged");
  r = good.request(); r.supplied_backing = 0x3000;
  const auto keep = b::select_depth_backing(r, Fixture::read, &good);
  check(keep.action == b::DepthSelectionAction::unchanged && keep.backing == 0x3000 && !keep.reads, "existing backing unchanged");
}

// Optional machine-local acceptance fixture. No host code is embedded in this
// repository. The caller supplies the verified, self-contained helper bytes;
// it executes only against owned memory in this child process, with no game.
struct Executable {
  void *p{};
  explicit Executable(const char *path) {
    std::ifstream stream(path, std::ios::binary);
    std::vector<char> bytes{std::istreambuf_iterator<char>(stream), {}};
    check(bytes.size() == 0x2d1, "helper byte length");
    std::array<UCHAR, 32> digest{};
    BCRYPT_ALG_HANDLE algorithm{};
    check(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0, "SHA provider");
    const auto status = BCryptHash(algorithm, nullptr, 0, reinterpret_cast<PUCHAR>(bytes.data()),
        static_cast<ULONG>(bytes.size()), digest.data(), static_cast<ULONG>(digest.size()));
    BCryptCloseAlgorithmProvider(algorithm, 0);
    check(status == 0, "helper digest");
    const char digits[] = "0123456789abcdef"; std::string hex;
    for (auto v : digest) { hex += digits[v >> 4]; hex += digits[v & 15]; }
    // Filled from the pinned, independently decoded host capture during preparation.
    check(hex == "d27e289f7fdc9f5205e6e8e61615586366a18d0e725235801690a448cddd8ba2", "helper identity");
    p = VirtualAlloc(nullptr, bytes.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    check(p != nullptr, "owned executable allocation");
    std::memcpy(p, bytes.data(), bytes.size());
    DWORD prior{};
    if (!VirtualProtect(p, bytes.size(), PAGE_EXECUTE_READ, &prior) ||
        !FlushInstructionCache(GetCurrentProcess(), p, bytes.size())) {
      VirtualFree(p, 0, MEM_RELEASE); p = nullptr; throw std::runtime_error("owned helper protection");
    }
  }
  ~Executable() { if (p) VirtualFree(p, 0, MEM_RELEASE); }
  Executable(const Executable &) = delete;
  Executable &operator=(const Executable &) = delete;
};
void original_helper_tests(const char *path) {
  Executable code(path);
  using Helper = void(__fastcall *)(A *, A, std::uint16_t);
  const auto helper = reinterpret_cast<Helper>(code.p);
  for (std::uint16_t state : {std::uint16_t{4}, std::uint16_t{8}, std::uint16_t{0x800}}) {
    Fixture f;
    std::array<std::byte, 40> active{};
    std::array<std::byte, 64> previous_descriptor{}, current_descriptor{};
    std::array<std::byte, 64> prior_barriers{};
    std::array<std::byte, 160> records{};
    std::array<std::byte, 128> barriers{};
    std::vector<std::byte> arena(0x7b000);
    A descriptor = address(current_descriptor), epoch = 22774, tail = 0;
    std::uint32_t operation = 1396;
    std::uint8_t force = 0, omit = 0;
    std::array<A, 7> context{reinterpret_cast<A>(&descriptor), reinterpret_cast<A>(&operation),
      reinterpret_cast<A>(&epoch), reinterpret_cast<A>(&force), address(arena),
      reinterpret_cast<A>(&tail), reinterpret_cast<A>(&omit)};
    put(f.tracker, 0, address(active)); put(f.tracker, 8, address(active));
    put(f.tracker, 16, epoch); put(f.tracker, 26, state);
    put(active, 0, address(previous_descriptor)); put(active, 8, std::uint32_t{1378});
    put(active, 32, std::uint16_t{0x800}); put(active, 34, state);
    put(previous_descriptor, 8, address(prior_barriers));
    put(prior_barriers, 0, address(prior_barriers) + 32);
    put(prior_barriers, 8, A{0x999}); // Unrelated node must remain untouched.
    put(prior_barriers, 26, std::uint16_t{0x80});
    put(prior_barriers, 40, address(f.backing)); put(prior_barriers, 58, state);
    put(arena, 62952 * 8, address(records)); put(arena, 62955 * 8, address(barriers));
    const auto selected = f.select();
    check(selected.action == b::DepthSelectionAction::primary, "original helper receives selected backing");
    const auto owner_before = f.owner;
    const auto backing_before = f.backing;
    helper(context.data(), selected.backing, 4);
    check(f.owner == owner_before && f.backing == backing_before, "selection never aliases owner fields");
    check(get<std::uint16_t>(prior_barriers, 26) == 0x80, "unrelated earlier barrier preserved");
    if (state == 4) {
      check(get<std::uint32_t>(active, 8) == operation && get<std::uint16_t>(active, 34) == 4,
            "existing read updates operation only");
      check(get<std::uint32_t>(arena, 125908 * 4) == 0 && tail == 0, "no allocation for existing read");
    } else if (state == 8) {
      check(get<std::uint16_t>(active, 34) == 12 && get<std::uint16_t>(f.tracker, 26) == 12,
            "actual helper merges read bits");
      check(get<std::uint16_t>(prior_barriers, 58) == 12 && get<std::uint32_t>(active, 8) == operation,
            "actual helper finds and updates earlier descriptor barrier");
      check(get<std::uint32_t>(arena, 125908 * 4) == 0 && tail == 0, "merge does not append record");
    } else {
      check(get<A>(f.tracker, 8) == address(records) && get<A>(active, 16) == address(records), "new active record linked");
      check(get<std::uint16_t>(records, 32) == 0x800 && get<std::uint16_t>(records, 34) == 4,
            "new record preserves before/requested states");
      check(get<A>(barriers, 8) == selected.backing && get<std::uint16_t>(barriers, 26) == 4 &&
            get<A>(current_descriptor, 8) == address(barriers), "new barrier retains same selected backing");
      check(get<std::uint32_t>(arena, 125908 * 4) == 1 && get<std::uint32_t>(arena, 125914 * 4) == 1,
            "one record and one barrier allocated");
    }
  }
}
}
int main(int argc, char **argv) {
  try {
    selector_tests();
    if (argc == 3 && std::string_view(argv[1]) == "--owned-host-helper") original_helper_tests(argv[2]);
    else check(argc == 1, "usage: depth_selection_tests [--owned-host-helper local-helper.bin]");
    std::cout << "PASS: bounded selector, rejected work, unchanged unrelated calls";
    if (argc == 3) std::cout << "; actual host helper 0x4/0x8/0x800 in owned memory";
    std::cout << '\n'; return 0;
  } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
