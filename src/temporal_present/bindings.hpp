#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <d3d12.h>
#include <memory>

namespace mhw::temporal_present {
enum class ArgumentKind { unset, table, constants, cbv, srv, uav };
// An argument's constants are valid where `mask` marks them; other entries of
// `values` may be left over from earlier calls (v0.23.8: no 256-byte clear on
// each call), so equality compares the marked constants only.
struct Argument {
    bool operator==(const Argument &o) const noexcept {
        if (kind != o.kind || address != o.address || mask != o.mask) return false;
        for (auto m = mask; m; m &= m - 1) {
            const auto i = static_cast<unsigned>(std::countr_zero(m));
            if (values[i] != o.values[i]) return false;
        }
        return true;
    }
    void clear() noexcept {
        kind = ArgumentKind::unset;
        address = 0;
        mask = 0;
    }
    ArgumentKind kind{};
    std::uint64_t address{}, mask{};
    std::array<std::uint32_t, 64> values{};
};
// `set` marks the arguments that are not unset, so a root-signature change or
// a Reset clears only those (v0.23.8; it cleared the whole 18 KB before, on
// each of thousands of calls a frame, game-92). Equality does not rely on it.
struct RootBindings {
    bool operator==(const RootBindings &o) const noexcept {
        return signature == o.signature && arguments == o.arguments;
    }
    void clear(std::uintptr_t next_signature = 0) noexcept {
        for (auto m = set; m; m &= m - 1) arguments[static_cast<unsigned>(std::countr_zero(m))].clear();
        set = 0;
        signature = next_signature;
    }
    std::uintptr_t signature{};
    std::uint64_t set{};
    std::array<Argument, 64> arguments{};
};
// CPU-only mirror. Feed only successful native state-setting calls from the
// beginning of this command-list generation. No pointer is dereferenced here.
struct Bindings {
    bool operator==(const Bindings &) const = default;
    bool known{}, valid{true};
    std::uint64_t generation{};
    std::uintptr_t pipeline{};
    std::array<std::uintptr_t, 2> heaps{};
    unsigned heap_count{};
    RootBindings compute, graphics;
    void reset(std::uint64_t next_generation, std::uintptr_t initial_pipeline = 0) noexcept;
    void root(bool graphics_stage, std::uintptr_t signature) noexcept;
    void descriptor_heaps(std::span<const std::uintptr_t>) noexcept;
    void descriptor(bool graphics_stage, unsigned index, ArgumentKind, std::uint64_t) noexcept;
    void constants(bool graphics_stage, unsigned index, unsigned offset, std::span<const std::uint32_t>) noexcept;
    void invalidate() noexcept { valid = false; }
    bool ready() const noexcept;
    // ready() for a graphics-only recording that has not set a compute root
    // signature in this generation (an FG final scene on a worker list).
    bool ready_graphics() const noexcept;
};
// Restores both root signatures and their recorded arguments, shared PSO/heaps.
// This is NOT a blanket guarantee about arbitrary vendor SDK state changes.
// With compute_optional, a state that never set a compute root signature is
// accepted (ready_graphics) and restored by unbinding the compute signature.
class RestoreBindings {
public:
    RestoreBindings(ID3D12GraphicsCommandList *, const Bindings &, bool compute_optional = false);
    ~RestoreBindings();
    RestoreBindings(const RestoreBindings &) = delete;
    RestoreBindings &operator=(const RestoreBindings &) = delete;
    void apply() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mhw::temporal_present
