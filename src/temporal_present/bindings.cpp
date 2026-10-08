#include "bindings.hpp"
#include <wrl/client.h>
#include <stdexcept>

namespace mhw::temporal_present {
void Bindings::reset(std::uint64_t next, std::uintptr_t initial) noexcept {
    *this = {}; known = next != 0; generation = next; pipeline = initial;
}
void Bindings::root(bool g, std::uintptr_t signature) noexcept {
    if (!known) { valid = false; return; }
    auto &r = g ? graphics : compute;
    if (r.signature != signature) { r = {}; r.signature = signature; }
}
void Bindings::descriptor_heaps(std::span<const std::uintptr_t> values) noexcept {
    if (!known || values.size() > heaps.size()) { valid = false; return; }
    std::array<std::uintptr_t,2> next{};
    for (unsigned i=0; i<values.size(); ++i) {
        if (!values[i] || (i && values[i] == values[0])) { valid = false; return; }
        next[i] = values[i];
    }
    // Order is immaterial: at most one resource heap and one sampler heap.
    bool changed = values.size() != heap_count;
    if (!changed) for (unsigned i=0; i<heap_count; ++i)
        if (heaps[i] != next[0] && (values.size() != 2 || heaps[i] != next[1])) changed = true;
    if (changed) for (auto *r : {&compute, &graphics})
        for (auto &a : r->arguments) if (a.kind == ArgumentKind::table) a = {};
    heaps = next; heap_count = static_cast<unsigned>(values.size());
}
void Bindings::descriptor(bool g, unsigned index, ArgumentKind kind, std::uint64_t address) noexcept {
    auto &r = g ? graphics : compute;
    if (!known || !r.signature || index >= r.arguments.size() ||
        (kind != ArgumentKind::table && kind != ArgumentKind::cbv && kind != ArgumentKind::srv && kind != ArgumentKind::uav) ||
        (kind == ArgumentKind::table && (!heap_count || !address))) { valid = false; return; }
    auto &a = r.arguments[index]; a = {}; a.kind = kind; a.address = address;
}
void Bindings::constants(bool g, unsigned index, unsigned offset, std::span<const std::uint32_t> values) noexcept {
    auto &r = g ? graphics : compute;
    if (!known || !r.signature || index >= r.arguments.size() || offset > 64 || values.size() > 64-offset) { valid = false; return; }
    if (values.empty()) return;
    auto &a = r.arguments[index];
    if (a.kind != ArgumentKind::constants) a = {};
    a.kind = ArgumentKind::constants;
    for (unsigned i=0; i<values.size(); ++i) { a.values[offset+i] = values[i]; a.mask |= std::uint64_t(1) << (offset+i); }
}
bool Bindings::ready() const noexcept { return known && valid && generation && pipeline && compute.signature; }
bool Bindings::ready_graphics() const noexcept { return known && valid && generation && pipeline && graphics.signature; }

using Microsoft::WRL::ComPtr;
struct RestoreBindings::Impl {
    Bindings state;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12PipelineState> pipeline;
    ComPtr<ID3D12RootSignature> compute, graphics;
    std::array<ComPtr<ID3D12DescriptorHeap>,2> heaps;
};
RestoreBindings::RestoreBindings(ID3D12GraphicsCommandList *list, const Bindings &state, bool compute_optional) : impl_(std::make_unique<Impl>()) {
    const bool complete = state.ready() || (compute_optional && !state.compute.signature && state.ready_graphics());
    if (!list || !complete || list->GetType() != D3D12_COMMAND_LIST_TYPE_DIRECT) throw std::runtime_error("presentation binding snapshot incomplete");
    auto &p=*impl_; p.state=state; p.list=list;
    p.pipeline=reinterpret_cast<ID3D12PipelineState *>(state.pipeline);
    p.compute=reinterpret_cast<ID3D12RootSignature *>(state.compute.signature);
    p.graphics=reinterpret_cast<ID3D12RootSignature *>(state.graphics.signature);
    for (unsigned i=0;i<state.heap_count;++i) p.heaps[i]=reinterpret_cast<ID3D12DescriptorHeap *>(state.heaps[i]);
}
RestoreBindings::~RestoreBindings() = default;
void RestoreBindings::apply() noexcept {
    const auto &p=*impl_; auto *list=p.list.Get();
    ID3D12DescriptorHeap *heaps[]{p.heaps[0].Get(),p.heaps[1].Get()};
    list->SetDescriptorHeaps(p.state.heap_count,heaps);
    // A null compute signature (compute_optional) unbinds ours: the list then
    // matches a recording that never set one, and the game sets its own
    // signature before any compute work, as it must after Reset.
    list->SetComputeRootSignature(p.compute.Get());
    list->SetGraphicsRootSignature(p.graphics.Get());
    list->SetPipelineState(p.pipeline.Get());
    for (unsigned i=0;i<p.state.compute.arguments.size();++i) {
        const auto &a=p.state.compute.arguments[i];
        switch (a.kind) {
        case ArgumentKind::table: list->SetComputeRootDescriptorTable(i,{a.address}); break;
        case ArgumentKind::cbv: list->SetComputeRootConstantBufferView(i,a.address); break;
        case ArgumentKind::srv: list->SetComputeRootShaderResourceView(i,a.address); break;
        case ArgumentKind::uav: list->SetComputeRootUnorderedAccessView(i,a.address); break;
        case ArgumentKind::constants:
            for (unsigned n=0;n<64;++n) if (a.mask & (std::uint64_t(1)<<n)) list->SetComputeRoot32BitConstant(i,a.values[n],n);
            break;
        case ArgumentKind::unset: break;
        }
    }
    for (unsigned i=0;i<p.state.graphics.arguments.size();++i) {
        const auto &a=p.state.graphics.arguments[i];
        switch (a.kind) {
        case ArgumentKind::table: list->SetGraphicsRootDescriptorTable(i,{a.address}); break;
        case ArgumentKind::cbv: list->SetGraphicsRootConstantBufferView(i,a.address); break;
        case ArgumentKind::srv: list->SetGraphicsRootShaderResourceView(i,a.address); break;
        case ArgumentKind::uav: list->SetGraphicsRootUnorderedAccessView(i,a.address); break;
        case ArgumentKind::constants:
            for (unsigned n=0;n<64;++n) if (a.mask & (std::uint64_t(1)<<n)) list->SetGraphicsRoot32BitConstant(i,a.values[n],n);
            break;
        case ArgumentKind::unset: break;
        }
    }
}
}
