#pragma once
#include "bindings.hpp"
#include "../astera/astera_hooks.hpp"
#include <string>
#include <vector>

namespace mhw::temporal_present::observer {
inline constexpr unsigned hook_count=18, list_limit=128;
struct RasterBindings {
    std::uint64_t generation{};
    D3D12_VERTEX_BUFFER_VIEW vertex_buffer{};
    std::array<D3D12_VERTEX_BUFFER_VIEW,32> vertex_buffers{};
    unsigned vertex_mask{};
    D3D12_INDEX_BUFFER_VIEW index_buffer{};
    D3D12_VIEWPORT viewport{};
    D3D12_RECT scissor{};
    D3D_PRIMITIVE_TOPOLOGY topology{};
    std::uintptr_t render_target{};
    bool output_known{};
    bool vertex_known{},viewport_known{},scissor_known{},topology_known{};
    bool index_known{};
    bool ready() const noexcept{return generation&&vertex_known&&viewport_known&&scissor_known&&topology_known;}
};
struct Predicate {std::uintptr_t buffer{};std::uint64_t offset{};D3D12_PREDICATION_OP operation{};bool known{};};
using Resolver=bool(*)(std::uintptr_t,unsigned,std::uintptr_t &) noexcept;
using ViewportListener=void(*)(ID3D12GraphicsCommandList *,unsigned,const D3D12_VIEWPORT *,std::uintptr_t) noexcept;
// AL installed the detours astera_hooks() registered (the full set: bindings,
// raster, predicate, indexed); install() checks them against the resolved
// methods and starts them. The listener sees admitted, successful native
// viewport calls; it never forwards the API. It must live until this observer
// is disabled and all callbacks are drained.
bool install(ID3D12Device *,Resolver,std::string &,ViewportListener=nullptr);
void begin() noexcept;
// The owner supplies only successful, validated DIRECT-list Reset generations,
// and pins every admitted command list until it calls forget() for it.
void reset(ID3D12GraphicsCommandList *,std::uint64_t,ID3D12PipelineState *) noexcept;
// The owner's pin was the list's last reference (its creator released it): no
// thread can record with it again. Its slot is freed before the owner drops
// the pin, so a new list at the same address starts clean.
void forget(ID3D12GraphicsCommandList *) noexcept;
bool matches_current(ID3D12GraphicsCommandList *,const Bindings &) noexcept;
bool current(ID3D12GraphicsCommandList *,Bindings &) noexcept;
// Diagnostic copy including an incomplete binding set. Never authorizes work;
// current()/Bindings::ready() remain the admission check.
bool inspect(ID3D12GraphicsCommandList *,Bindings &) noexcept;
// Optional API-derived IA/RS snapshot; never inferred from native caches.
bool raster_current(ID3D12GraphicsCommandList *,RasterBindings &) noexcept;
bool predicate_current(ID3D12GraphicsCommandList *,Predicate &) noexcept;
void stop() noexcept;
bool settled() noexcept;
bool healthy() noexcept;
bool hooks_disabled() noexcept;
bool finish() noexcept;
// All observer detours with the storage AL fills, registered at PRE_MAIN.
void astera_hooks(std::vector<astera::Hook>&);
}
