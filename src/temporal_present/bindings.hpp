#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <d3d12.h>
#include <memory>

namespace mhw::temporal_present {
enum class ArgumentKind { unset, table, constants, cbv, srv, uav };
struct Argument {
    bool operator==(const Argument &) const = default;
    ArgumentKind kind{};
    std::uint64_t address{}, mask{};
    std::array<std::uint32_t,64> values{};
};
struct RootBindings {
    bool operator==(const RootBindings &) const = default;
    std::uintptr_t signature{};
    std::array<Argument,64> arguments{};
};
// CPU-only mirror. Feed only successful native state-setting calls from the
// beginning of this command-list generation. No pointer is dereferenced here.
struct Bindings {
    bool operator==(const Bindings &) const = default;
    bool known{}, valid{true};
    std::uint64_t generation{};
    std::uintptr_t pipeline{};
    std::array<std::uintptr_t,2> heaps{};
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
}
