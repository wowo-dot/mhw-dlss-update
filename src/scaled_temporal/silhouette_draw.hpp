#pragma once
#include "silhouette_shader.hpp"
#include <d3d12.h>
#include <wrl/client.h>

namespace mhw::scaled_temporal {
// Immutable private PSO. Native root arguments, heaps, IA buffers, render
// targets and resource states are reused without modification. The owning
// route qualifies them and pins all objects through Reset AND GPU completion.
struct SilhouetteRecipe {
    FilterGeometry geometry;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC native{};
    // Fixture only: adapt a census candidate without the allowlist check, to
    // decide whether it may join the allowlist. The route never sets this.
    bool candidate{};
};
class SilhouetteDraw {
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    FilterGeometry geometry_;

public:
    SilhouetteDraw(ID3D12Device *, const SilhouetteRecipe &);
    ID3D12PipelineState *pipeline() const noexcept { return pipeline_.Get(); }
    ID3D12RootSignature *root() const noexcept { return root_.Get(); }
    // Caller has already qualified/pinned the native indexed draw and disabled
    // predicate. A surrounding selector may predicate this replacement.
    bool record(ID3D12GraphicsCommandList *, ID3D12PipelineState *native, const D3D12_VIEWPORT &, const D3D12_RECT &,
                unsigned count, unsigned first, int base) const noexcept;
};
} // namespace mhw::scaled_temporal
