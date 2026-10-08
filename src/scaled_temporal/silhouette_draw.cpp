#include "silhouette_draw.hpp"
#include <stdexcept>

namespace mhw::scaled_temporal {
SilhouetteDraw::SilhouetteDraw(ID3D12Device *device,const SilhouetteRecipe &r):geometry_(r.geometry){
    const auto &n=r.native;
    if(!device||!valid_filter_geometry(geometry_)||!n.pRootSignature||!n.VS.pShaderBytecode||!n.VS.BytecodeLength||
       n.VS.BytecodeLength>131072||n.GS.BytecodeLength||n.HS.BytecodeLength||n.DS.BytecodeLength||
       n.StreamOutput.NumEntries||n.InputLayout.NumElements>32||n.NumRenderTargets<1||n.NumRenderTargets>3||
       n.SampleDesc.Count!=1||n.SampleDesc.Quality||n.PrimitiveTopologyType!=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE||
       n.DepthStencilState.DepthEnable||n.DepthStencilState.StencilEnable||n.BlendState.AlphaToCoverageEnable||
       n.BlendState.RenderTarget[0].LogicOpEnable||!n.BlendState.RenderTarget[0].RenderTargetWriteMask)
        throw std::runtime_error("silhouette private recipe");
    Microsoft::WRL::ComPtr<ID3D12Device> actual;
    if(FAILED(n.pRootSignature->GetDevice(IID_PPV_ARGS(&actual)))||actual.Get()!=device)
        throw std::runtime_error("silhouette root device");
    const auto native=std::span(static_cast<const std::byte *>(n.PS.pShaderBytecode),n.PS.BytecodeLength);
    auto adapted=r.candidate?adapt_silhouette_shader(native,geometry_):make_silhouette_shader(native,geometry_);
    auto desc=n;desc.PS={adapted.data(),adapted.size()};desc.CachedPSO={};
    if(FAILED(device->CreateGraphicsPipelineState(&desc,IID_PPV_ARGS(&pipeline_))))
        throw std::runtime_error("silhouette private PSO");
    root_=n.pRootSignature;
}
bool SilhouetteDraw::record(ID3D12GraphicsCommandList *list,ID3D12PipelineState *native,
                           const D3D12_VIEWPORT &v,const D3D12_RECT &s,unsigned count,unsigned first,int base) const noexcept {
    const auto &g=geometry_;
    if(!list||!native||!count||v.TopLeftX||v.TopLeftY||v.Width!=g.render_width||v.Height!=g.render_height||
       v.MinDepth!=0||v.MaxDepth!=1||s.left||s.top||s.right!=static_cast<LONG>(g.render_width)||s.bottom!=static_cast<LONG>(g.render_height))return false;
    const D3D12_VIEWPORT full{0,0,float(g.output_width),float(g.output_height),0,1};
    const D3D12_RECT clip{0,0,LONG(g.output_width),LONG(g.output_height)};
    // No allocation or throwing operation follows the first command.
    list->SetPipelineState(pipeline_.Get());list->RSSetViewports(1,&full);list->RSSetScissorRects(1,&clip);
    list->DrawIndexedInstanced(count,1,first,base,0);
    list->SetPipelineState(native);list->RSSetViewports(1,&v);list->RSSetScissorRects(1,&s);
    return true;
}
}
