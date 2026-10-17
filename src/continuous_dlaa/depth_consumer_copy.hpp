#pragma once
#include <d3d12.h>
#include "depth_read_state.hpp"
namespace mhw::continuous_dlaa {
using DepthBarrierFn = void(WINAPI *)(ID3D12GraphicsCommandList *, UINT, const D3D12_RESOURCE_BARRIER *);
using DepthCopyFn = void(WINAPI *)(ID3D12GraphicsCommandList *, ID3D12Resource *, ID3D12Resource *);
inline bool depth_copy_description(const D3D12_RESOURCE_DESC &d, unsigned width, unsigned height,
                                   bool source) noexcept {
    return width && height && d.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && d.Width == width &&
           d.Height == height && d.DepthOrArraySize == 1 && d.MipLevels == 1 && d.SampleDesc.Count == 1 &&
           !d.SampleDesc.Quality && d.Format == DXGI_FORMAT_R32_FLOAT &&
           d.Flags == (source ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE);
}
// The caller proves native ordering/state and retains source + destination for
// this recording. The private destination enters and leaves in COPY_DEST.
inline bool record_consumer_depth(ID3D12GraphicsCommandList *list, ID3D12Resource *source, ID3D12Resource *destination,
                                  unsigned width, unsigned height, D3D12_RESOURCE_STATES state, DepthBarrierFn barrier,
                                  DepthCopyFn copy) {
    if (!list || !source || !destination || source == destination || !barrier || !copy || !depth_copy_state(state) ||
        !depth_copy_description(source->GetDesc(), width, height, true) ||
        !depth_copy_description(destination->GetDesc(), width, height, false))
        return false;
    D3D12_RESOURCE_BARRIER b{};
    b.Transition = {source, UINT_MAX, state, D3D12_RESOURCE_STATE_COPY_SOURCE};
    barrier(list, 1, &b);
    copy(list, destination, source);
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    b.Transition.StateAfter = state;
    barrier(list, 1, &b);
    return true;
}
} // namespace mhw::continuous_dlaa
