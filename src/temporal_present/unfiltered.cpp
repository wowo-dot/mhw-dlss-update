#include "unfiltered.hpp"
#include <wrl/client.h>

namespace mhw::temporal_present {
namespace {
bool texture(const D3D12_RESOURCE_DESC &d,unsigned w,unsigned h) noexcept {
    return d.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D && d.Width==w && d.Height==h &&
        d.DepthOrArraySize==1 && d.MipLevels==1 && d.Format==DXGI_FORMAT_R11G11B10_FLOAT &&
        d.SampleDesc.Count==1 && d.SampleDesc.Quality==0 && !(d.Flags&D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
}
void transition(ID3D12GraphicsCommandList *list,ID3D12Resource *r,D3D12_RESOURCE_STATES a,D3D12_RESOURCE_STATES b) noexcept {
    D3D12_RESOURCE_BARRIER barrier{};barrier.Transition={r,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,a,b};
    list->ResourceBarrier(1,&barrier);
}
}
bool can_copy_unfiltered(ID3D12GraphicsCommandList *list,ID3D12Resource *source,ID3D12Resource *destination,
                     unsigned w,unsigned h,D3D12_RESOURCE_STATES state) noexcept {
    if(!list||!source||!destination||source==destination||!w||!h||w>8192||h>8192||UINT64(w)*h>16777216||
       (state!=D3D12_RESOURCE_STATE_COPY_DEST && state!=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE &&
        state!=(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE))||
       list->GetType()!=D3D12_COMMAND_LIST_TYPE_DIRECT)return false;
    Microsoft::WRL::ComPtr<ID3D12Device> a,b,c;
    if(FAILED(list->GetDevice(IID_PPV_ARGS(&a)))||FAILED(source->GetDevice(IID_PPV_ARGS(&b)))||
       FAILED(destination->GetDevice(IID_PPV_ARGS(&c)))||a.Get()!=b.Get()||a.Get()!=c.Get())return false;
    const auto src=source->GetDesc(),dst=destination->GetDesc();
    if(!texture(src,w,h)||!texture(dst,w,h)||!(dst.Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))return false;
    return true;
}
bool copy_unfiltered(ID3D12GraphicsCommandList *list,ID3D12Resource *source,ID3D12Resource *destination,
                     unsigned w,unsigned h,D3D12_RESOURCE_STATES state) noexcept {
    if(!can_copy_unfiltered(list,source,destination,w,h,state))return false;
    transition(list,source,state,D3D12_RESOURCE_STATE_COPY_SOURCE);
    transition(list,destination,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyResource(destination,source);
    transition(list,destination,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list,source,D3D12_RESOURCE_STATE_COPY_SOURCE,state);
    return true;
}
}
