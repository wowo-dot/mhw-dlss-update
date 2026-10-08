#include "effects.hpp"
#include "haze_shader.hpp"
#include "radial_shader.hpp"
#include "dof_shader.hpp"
#include "screen_filters.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

namespace mhw::scaled_temporal {
namespace {
using Microsoft::WRL::ComPtr;
void require(bool v,const char *why){if(!v)throw std::runtime_error(why);}
void hr(HRESULT v,const char *why){require(SUCCEEDED(v),why);}
bool same_device(ID3D12DeviceChild *child,ID3D12Device *device){
    if(!child||!device)return false;
    ComPtr<ID3D12Device> actual;ComPtr<IUnknown> a,b;
    return SUCCEEDED(child->GetDevice(IID_PPV_ARGS(&actual)))&&SUCCEEDED(actual.As(&a))&&
        SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&b)))&&a==b;
}
bool shader(std::span<const std::byte> bytes,std::size_t size,const char *expected){
    if(bytes.size()!=size)return false;std::array<unsigned char,32> hash{};
    if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<std::byte *>(bytes.data())),
        static_cast<ULONG>(bytes.size()),hash.data(),static_cast<ULONG>(hash.size()))<0)return false;
    constexpr char hex[]="0123456789abcdef";
    for(unsigned i=0;i<32;++i)if(hex[hash[i]>>4]!=expected[2*i]||hex[hash[i]&15]!=expected[2*i+1])return false;
    return true;
}
unsigned index(EffectStage stage){const auto i=static_cast<unsigned>(stage);require(i<8,"effect stage");return i;}
// Pinned native shader identities per stage (pixel size/hash, vertex size/hash).
constexpr const char *ps_hash[]{"3eed9b57bbbdf38d14d9a94596fc93a107aa99eab275af7118b95febbbc1ba18",
    "174a09e5b23c7e01fb6c47ce86b61250d695113109abb4ffae7219117c31a8cb","be6127ba48008790fd0e93558b8248cb8acd7c0577899bf50d28e5618ae37b79",
    "4f890ffa89ea59a218db7e191d8aab48e9d5fe4be200c50a606b22594bb57f69",
    "be6127ba48008790fd0e93558b8248cb8acd7c0577899bf50d28e5618ae37b79",
    "90f3f4d5594357e5350acf3d5bda57db4641b50e6aaa5787d3ae341463860741",
    "9f04832cb0a0ff800169964919f7ebecba353e7e3c147c2b314e76af7372e3aa"};
constexpr unsigned ps_size[]{10008,5824,1080,14596,1080,6444,4604};
unsigned vs_size(unsigned stage){return stage==5?764:stage==3?712:(stage==2||stage==4||stage==6)?644:752;}
const char *vs_hash(unsigned stage){
    return stage==5?"b980e376575d82610809066f45c6ecd6efb090bbee76968d9ee5203d2459a1d2":
        stage==3?"c7c73b6ac661514c34c53681e217dc3fe7014e21a99a41adb0fcb7befc1f5de6":
        (stage==2||stage==4||stage==6)?"730b7d23bcdca40477ed973c29c8fff8de7ab7244b05d142d5f9458fbfec5fdd":"abddc634c053c157f295842964b023610ff64a7505d4b253edbe1612680ea937";
}
// Stages 0-6 pin one pixel shader each; stage 7 takes any approved screen filter.
bool identity(const EffectRecipe &r,unsigned stage){
    if(stage==7)return screen_filter(r.pixel_shader,r.vertex_shader)!=nullptr;
    return shader(r.pixel_shader,ps_size[stage],ps_hash[stage])&&shader(r.vertex_shader,vs_size(stage),vs_hash(stage));
}
// Packed dependency recipes (radial, DOF composite) use 28-byte vertices.
bool packed(unsigned stage){return stage==5||stage==6;}
bool dependent(EffectStage s){return s==EffectStage::scene_copy||s==EffectStage::radial||s==EffectStage::dof_composite;}
DXGI_FORMAT output_format(EffectStage s){return s==EffectStage::scene_copy?DXGI_FORMAT_R10G10B10A2_UNORM:DXGI_FORMAT_R11G11B10_FLOAT;}
bool layout(std::span<const D3D12_INPUT_ELEMENT_DESC> elements){
    if(elements.size()!=2)return false;
    for(unsigned i=0;i<2;++i){const auto &e=elements[i];
        if(!e.SemanticName||std::strcmp(e.SemanticName,i?"TEXCOORD":"POSITION")||e.SemanticIndex||
           e.Format!=DXGI_FORMAT_R32G32_FLOAT||e.InputSlot||e.AlignedByteOffset!=8*i||
           e.InputSlotClass!=D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA||e.InstanceDataStepRate)return false;}
    return true;
}
bool metadata(const EffectRecipe &r){
    if(dependent(r.stage)){
        // Preserve native blending (the radial pass may blend into scene),
        // but never admit depth, stencil, logic ops or multisampled coverage.
        // Its R11G11B10 output has no alpha channel. The observed RGB-only
        // mask is valid there; the RGBA temporary copy still requires all four.
        const auto mask=r.blend.RenderTarget[0].RenderTargetWriteMask;
        return !r.blend.AlphaToCoverageEnable&&!r.blend.RenderTarget[0].LogicOpEnable&&
            (mask==15||(r.stage!=EffectStage::scene_copy&&mask==7))&&
            (r.stage==EffectStage::radial||!r.blend.RenderTarget[0].BlendEnable)&&
            !r.depth.DepthEnable&&!r.depth.StencilEnable&&
            r.raster.FillMode==D3D12_FILL_MODE_SOLID&&r.raster.CullMode==D3D12_CULL_MODE_NONE&&
            !r.raster.DepthBias&&r.raster.DepthBiasClamp==0&&r.raster.SlopeScaledDepthBias==0&&
            !r.raster.MultisampleEnable&&!r.raster.AntialiasedLineEnable&&!r.raster.ForcedSampleCount&&
            r.raster.ConservativeRaster==D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    }
    // Restrict the first implementation to the actual admitted recipe, rather
    // than accepting arbitrary root/shader combinations or reinterpretation.
    D3D12_BLEND_DESC blend{};blend.IndependentBlendEnable=TRUE;
    for(auto &b:blend.RenderTarget){b.SrcBlend=D3D12_BLEND_ONE;b.DestBlend=D3D12_BLEND_ZERO;b.BlendOp=D3D12_BLEND_OP_ADD;
        b.SrcBlendAlpha=D3D12_BLEND_ONE;b.DestBlendAlpha=D3D12_BLEND_ZERO;b.BlendOpAlpha=D3D12_BLEND_OP_ADD;
        b.LogicOp=D3D12_LOGIC_OP_CLEAR;b.RenderTargetWriteMask=15;}
    D3D12_RASTERIZER_DESC raster{};raster.FillMode=D3D12_FILL_MODE_SOLID;raster.CullMode=D3D12_CULL_MODE_NONE;raster.FrontCounterClockwise=TRUE;
    D3D12_DEPTH_STENCIL_DESC depth{};depth.DepthFunc=D3D12_COMPARISON_FUNC_NEVER;
    depth.FrontFace=depth.BackFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_NEVER};
    // Compare fields rather than struct padding (StencilRead/WriteMask padding
    // can differ even for semantically identical descriptors).
    bool ok=r.blend.AlphaToCoverageEnable==blend.AlphaToCoverageEnable&&r.blend.IndependentBlendEnable==blend.IndependentBlendEnable;
    for(unsigned i=0;i<8;++i){const auto &a=r.blend.RenderTarget[i],&b=blend.RenderTarget[i];
        ok=ok&&a.BlendEnable==b.BlendEnable&&a.LogicOpEnable==b.LogicOpEnable&&a.SrcBlend==b.SrcBlend&&a.DestBlend==b.DestBlend&&
            a.BlendOp==b.BlendOp&&a.SrcBlendAlpha==b.SrcBlendAlpha&&a.DestBlendAlpha==b.DestBlendAlpha&&a.BlendOpAlpha==b.BlendOpAlpha&&
            a.LogicOp==b.LogicOp&&a.RenderTargetWriteMask==b.RenderTargetWriteMask;}
    const auto face=[](const D3D12_DEPTH_STENCILOP_DESC &a){return a.StencilFailOp==D3D12_STENCIL_OP_KEEP&&
        a.StencilDepthFailOp==D3D12_STENCIL_OP_KEEP&&a.StencilPassOp==D3D12_STENCIL_OP_KEEP&&a.StencilFunc==D3D12_COMPARISON_FUNC_NEVER;};
    return ok&&!r.depth.DepthEnable&&r.depth.DepthWriteMask==D3D12_DEPTH_WRITE_MASK_ZERO&&r.depth.DepthFunc==depth.DepthFunc&&
        !r.depth.StencilEnable&&!r.depth.StencilReadMask&&!r.depth.StencilWriteMask&&face(r.depth.FrontFace)&&face(r.depth.BackFace)&&
        r.raster.FillMode==raster.FillMode&&r.raster.CullMode==raster.CullMode&&r.raster.FrontCounterClockwise==TRUE&&
        !r.raster.DepthBias&&r.raster.DepthBiasClamp==0&&r.raster.SlopeScaledDepthBias==0&&!r.raster.DepthClipEnable&&
        !r.raster.MultisampleEnable&&!r.raster.AntialiasedLineEnable&&!r.raster.ForcedSampleCount&&r.raster.ConservativeRaster==D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
}
}
bool dependency_geometry(const EffectRecipe &r) noexcept {
    if(!dependent(r.stage)||!valid_filter_geometry(r.geometry))return false;
    const bool radial=r.stage!=EffectStage::scene_copy;const unsigned stride=radial?28U:16U,count=radial?4U:2U;
    if(r.input_layout.size()!=count||r.dependency_vertices.size()!=4*stride)return false;
    for(unsigned i=0;i<count;++i){const auto &e=r.input_layout[i];
        const char *name=i==0?"POSITION":i==3?"COLOR":"TEXCOORD";
        if(!e.SemanticName||_stricmp(e.SemanticName,name)||e.SemanticIndex!=(i==2?1U:0U)||
           e.Format!=(i==3?DXGI_FORMAT_R8G8B8A8_UNORM:DXGI_FORMAT_R32G32_FLOAT)||e.InputSlot||
           e.AlignedByteOffset!=8*i||e.InputSlotClass!=D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA||e.InstanceDataStepRate)return false;
    }
    const auto expected=dependency_vertices(r.geometry,false);
    for(unsigned i=0;i<4;++i){DependencyVertex actual;
        std::memcpy(&actual,r.dependency_vertices.data()+i*stride,stride);
        for(unsigned j=0;j<2;++j){
            if(!std::isfinite(actual.position[j])||!std::isfinite(actual.scene_uv[j])||
               std::abs(actual.position[j]-expected[i].position[j])>0.000001F||
               std::abs(actual.scene_uv[j]-expected[i].scene_uv[j])>0.000001F)return false;
            if(radial&&(!std::isfinite(actual.auxiliary_uv[j])||std::abs(actual.auxiliary_uv[j]-expected[i].auxiliary_uv[j])>0.000001F))return false;
        }
        if(radial&&actual.color!=0xffffffffU)return false;
    }
    return true;
}
EffectVertices effect_vertices(EffectStage stage,FilterGeometry g,bool full){
    index(stage);require(valid_filter_geometry(g),"effect geometry");
    if(dependent(stage)){
        const auto vertices=dependency_vertices(g,stage==EffectStage::scene_copy&&full);EffectVertices result;
        result.count=4;result.topology=D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        for(unsigned i=0;i<4;++i){std::copy(vertices[i].position.begin(),vertices[i].position.end(),result.values.begin()+4*i);
            std::copy(vertices[i].scene_uv.begin(),vertices[i].scene_uv.end(),result.values.begin()+4*i+2);}
        return result;
    }
    const float x=float(full?g.output_width:g.render_width)/g.output_width,y=float(full?g.output_height:g.render_height)/g.output_height;
    // Preserve the reduced UV footprint for native depth/noise sampling. The
    // haze PS adapter expands only the final color lookup, never these inputs.
    if(stage==EffectStage::haze||stage==EffectStage::screen_filter)return {{{-1,1,0,0,-1,-3,0,2.F*g.render_height/g.output_height,3,1,2.F*g.render_width/g.output_width,0}},3,D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP};
    // The native three-vertex screen triangle is also submitted as a strip.
    // Preserve that binding; a triangle-list assumption fails exact admission.
    if(stage==EffectStage::final_screen)return {{{-1,1,0,0,-1,-3,0,2*y-1.F/g.output_height,3,1,2*x-1.F/g.output_width,0}},3,D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP};
    // Native color grading traverses the top row first; the motion filter
    // traverses the left column first. Preserve each draw's ordered positions/UVs.
    if(stage==EffectStage::color_grade)return {{{-1,1,0,0,1,1,x,0,-1,-1,0,y,1,-1,x,y}},4,D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP};
    const auto u=stage==EffectStage::motion_filter?1.F:x,v=stage==EffectStage::motion_filter?1.F:y;
    return {{{-1,1,0,0,-1,-1,0,v,1,1,u,0,1,-1,u,v}},4,D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP};
}
bool effect_admitted(const EffectRecipe &r) noexcept {
    try{
        const auto stage=index(r.stage);
        if(!valid_filter_geometry(r.geometry)||!metadata(r)||!(dependent(r.stage)?dependency_geometry(r):layout(r.input_layout)))return false;
        if(!dependent(r.stage)){const auto native=effect_vertices(r.stage,r.geometry,false);
            if(r.native_vertices.size()!=4*native.count)return false;
            for(unsigned i=0;i<4*native.count;++i)if(!std::isfinite(r.native_vertices[i])||std::abs(r.native_vertices[i]-native.values[i])>0.000001F)return false;}
        return identity(r,stage);
    }catch(...){return false;}
}
struct Effect::Impl {
    ComPtr<ID3D12Device> device;ComPtr<ID3D12RootSignature> root;ComPtr<ID3D12PipelineState> pipeline;ComPtr<ID3D12Resource> vertices;
    EffectStage stage{};FilterGeometry geometry{};EffectVertices full{};D3D12_VERTEX_BUFFER_VIEW view{};
    struct Use {ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12PipelineState> pipeline;ComPtr<ID3D12Resource> output,vertices;};
    std::array<Use,16> uses;unsigned count{};bool issued{},retired{},caller_retained{};
};
Effect::Effect(ID3D12Device *device,const EffectRecipe &r,bool caller_retained):impl_(std::make_unique<Impl>()){
    const auto stage=index(r.stage);require(device&&same_device(r.root,device),"effect root device");
    require(valid_filter_geometry(r.geometry)&&metadata(r)&&(dependent(r.stage)?dependency_geometry(r):layout(r.input_layout)),"effect native recipe");
    const auto native=effect_vertices(r.stage,r.geometry,false);
    if(!dependent(r.stage)){
    require(r.native_vertices.size()==4*native.count,"effect vertex count");
    for(unsigned i=0;i<4*native.count;++i)require(std::isfinite(r.native_vertices[i])&&std::abs(r.native_vertices[i]-native.values[i])<=0.000001F,"effect native vertices");
    }
    require(identity(r,stage),"effect shader identity");
    auto &p=*impl_;p.caller_retained=caller_retained;p.device=device;p.root=r.root;p.stage=r.stage;p.geometry=r.geometry;p.full=effect_vertices(r.stage,r.geometry,true);
    auto adapted=stage==0?make_filter_shader(r.pixel_shader,r.geometry):stage==3?make_haze_shader(r.pixel_shader,r.geometry):stage==5?make_radial_shader(r.pixel_shader,r.geometry):stage==6?make_dof_shader(r.pixel_shader,r.geometry):
        stage==7?make_screen_filter_shader(r.pixel_shader,r.geometry):std::vector<std::byte>{};
    const auto pixel=adapted.empty()?r.pixel_shader:std::span<const std::byte>(adapted);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};d.pRootSignature=r.root;d.VS={r.vertex_shader.data(),r.vertex_shader.size()};d.PS={pixel.data(),pixel.size()};
    d.BlendState=r.blend;d.RasterizerState=r.raster;d.DepthStencilState=r.depth;d.SampleMask=UINT_MAX;
    d.InputLayout={r.input_layout.data(),static_cast<UINT>(r.input_layout.size())};d.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets=1;d.RTVFormats[0]=output_format(r.stage);d.SampleDesc.Count=1;
    hr(device->CreateGraphicsPipelineState(&d,IID_PPV_ARGS(&p.pipeline)),"effect private PSO");
    const auto radial_vertices=dependency_vertices(r.geometry,false);
    const std::span<const std::byte> vertex_bytes=packed(stage)?std::as_bytes(std::span<const DependencyVertex>(radial_vertices)):std::as_bytes(std::span<const float>(p.full.values));
    D3D12_RESOURCE_DESC b{};b.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;b.Width=vertex_bytes.size();b.Height=1;b.DepthOrArraySize=b.MipLevels=1;
    b.SampleDesc.Count=1;b.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_UPLOAD;
    hr(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&b,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&p.vertices)),"effect immutable vertices");
    void *mapped{};D3D12_RANGE none{};hr(p.vertices->Map(0,&none,&mapped),"effect vertex map");std::memcpy(mapped,vertex_bytes.data(),vertex_bytes.size());p.vertices->Unmap(0,nullptr);
    p.view={p.vertices->GetGPUVirtualAddress(),static_cast<UINT>(vertex_bytes.size()),packed(stage)?28U:16U};
}
Effect::~Effect(){if(impl_&&impl_->issued&&!impl_->retired)(void)impl_.release();}
bool Effect::record(const EffectBindings &b) noexcept{
    auto &p=*impl_;if(p.retired||b.root!=p.root.Get()||!same_device(b.list,p.device.Get())||!same_device(b.pipeline,p.device.Get())||
        !same_device(b.output,p.device.Get())||!same_device(b.vertex_resource,p.device.Get())||b.list->GetType()!=D3D12_COMMAND_LIST_TYPE_DIRECT)return false;
    const auto d=b.output->GetDesc(),v=b.vertex_resource->GetDesc();const auto &g=p.geometry;
    if(d.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||d.Width!=g.output_width||d.Height!=g.output_height||d.DepthOrArraySize!=1||d.MipLevels!=1||
       d.Format!=output_format(p.stage)||d.SampleDesc.Count!=1||d.SampleDesc.Quality||!(d.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)||
       v.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||b.vertex_buffer.StrideInBytes!=p.view.StrideInBytes||b.topology!=p.full.topology||b.count!=p.full.count)return false;
    const auto address=b.vertex_resource->GetGPUVirtualAddress();const auto &vb=b.vertex_buffer;
    if(!address||vb.BufferLocation<address||vb.BufferLocation-address>v.Width||vb.SizeInBytes>v.Width-(vb.BufferLocation-address)||
       (UINT64(b.first)+b.count)*p.view.StrideInBytes>vb.SizeInBytes)return false;
    const unsigned w=p.stage==EffectStage::final_screen?g.output_width:g.render_width,h=p.stage==EffectStage::final_screen?g.output_height:g.render_height;
    if(b.viewport.TopLeftX!=0||b.viewport.TopLeftY!=0||b.viewport.Width!=w||b.viewport.Height!=h||b.viewport.MinDepth!=0||b.viewport.MaxDepth!=1||
       b.scissor.left||b.scissor.top||b.scissor.right!=static_cast<LONG>(w)||b.scissor.bottom!=static_cast<LONG>(h))return false;
    if(!p.caller_retained){
        unsigned slot{};for(;slot<p.count;++slot){const auto &u=p.uses[slot];if(u.list.Get()==b.list&&u.pipeline.Get()==b.pipeline&&u.output.Get()==b.output&&u.vertices.Get()==b.vertex_resource)break;}
        if(slot==p.count){if(p.count==p.uses.size())return false;p.uses[p.count++]={b.list,b.pipeline,b.output,b.vertex_resource};}
    }
    // No allocating/throwing operations follow the first command. Root args,
    // descriptor heaps, OM targets and topology stay bound throughout.
    p.issued=true;D3D12_VIEWPORT viewport{0,0,float(g.output_width),float(g.output_height),0,1};
    D3D12_RECT scissor{0,0,static_cast<LONG>(g.output_width),static_cast<LONG>(g.output_height)};
    b.list->SetPipelineState(p.pipeline.Get());b.list->IASetVertexBuffers(0,1,&p.view);b.list->RSSetViewports(1,&viewport);b.list->RSSetScissorRects(1,&scissor);
    b.list->DrawInstanced(p.full.count,1,0,0);
    b.list->SetPipelineState(b.pipeline);b.list->IASetVertexBuffers(0,1,&b.vertex_buffer);b.list->RSSetViewports(1,&b.viewport);b.list->RSSetScissorRects(1,&b.scissor);
    return true;
}
void Effect::retire(ID3D12Fence *fence,std::uint64_t value,bool recordings_retired){
    auto &p=*impl_;require(!p.retired&&p.issued&&value&&recordings_retired&&same_device(fence,p.device.Get()),"effect retirement proof");
    const auto completed=fence->GetCompletedValue();require(completed!=UINT64_MAX&&completed>=value,"effect completion");
    p.uses={};p.count=0;p.retired=true;
}
bool Effect::recorded() const noexcept{return impl_->issued;}
bool Effect::retired() const noexcept{return impl_->retired;}
}
