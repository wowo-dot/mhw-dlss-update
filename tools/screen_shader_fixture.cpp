// Generated screen-filter shaders (screen_shader.cpp) against native ones.
// Private captured shaders are supplied by path; --synthetic compiles small
// public HLSL stand-ins so the same checks run in the test suite on WARP.
//
// Per shader, with point samplers and every texture filled (scene, auxiliary,
// LUT): (1) at 1x the generated PS equals the native PS bit for bit; (2) the
// render-lattice oracle: the generated PS at full output, fed the scene
// upsampled 2x2, against the native PS at render size, per output pixel
// (x/2, y/2) — exact for pixel-local filters, within a bound where a value
// depends continuously on the sub-pixel position; (3) negative control: the
// unadapted PS at full output must fail that oracle whenever edits were made;
// (4) optional pairs: the generated PS equals a hand-written adapter exactly.
#include "../src/scaled_temporal/screen_shader.hpp"
#include "../src/scaled_temporal/depth_color_shader.hpp"
#include "../src/scaled_temporal/filter_shader.hpp"
#include "../src/scaled_temporal/effects.hpp"
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <d3d12shader.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Microsoft::WRL::ComPtr;
namespace sf=mhw::scaled_temporal;
void require(bool v,const std::string &why){if(!v)throw std::runtime_error(why);}
void hr(HRESULT v,const char *why){if(FAILED(v))throw std::runtime_error(std::string(why)+" HRESULT="+std::to_string(static_cast<unsigned long>(v)));}
std::vector<std::byte> read(const std::filesystem::path &p){
    const auto size=std::filesystem::file_size(p);require(size<=1048576,"bounded shader");
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));std::ifstream f(p,std::ios::binary);
    require(bool(f.read(reinterpret_cast<char *>(bytes.data()),static_cast<std::streamsize>(size))),"read shader");return bytes;
}
std::vector<std::byte> blob(ID3DBlob *b){const auto *p=static_cast<const std::byte *>(b->GetBufferPointer());return {p,p+b->GetBufferSize()};}
std::vector<std::byte> compile(const char *source,const char *target){ComPtr<ID3DBlob> b,e;
    const auto r=D3DCompile(source,std::strlen(source),"synthetic",nullptr,nullptr,"main",target,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&b,&e);
    if(FAILED(r)&&e)std::cerr<<static_cast<const char *>(e->GetBufferPointer());hr(r,"compile");return blob(b.Get());}
float hashed(unsigned a,unsigned b,unsigned c,unsigned k){unsigned v=a*73856093U^b*19349663U^c*83492791U^k*2654435761U;v^=v>>13;v*=0x5bd1e995U;v^=v>>15;
    return .05F+.95F*float(v&0xffff)/65535.F;}
struct Variable {std::string name;unsigned offset{},size{};bool used{},integer{};};
struct Binding {D3D_SHADER_INPUT_TYPE type;unsigned point;D3D_SRV_DIMENSION dimension;unsigned size;bool comparison;std::string name;std::vector<Variable> variables;};
std::vector<Binding> reflect(std::span<const std::byte> ps){
    ComPtr<ID3D12ShaderReflection> r;hr(D3DReflect(ps.data(),ps.size(),IID_PPV_ARGS(&r)),"reflect");D3D12_SHADER_DESC d{};hr(r->GetDesc(&d),"shader desc");
    std::vector<Binding> out;
    for(unsigned i=0;i<d.BoundResources;++i){D3D12_SHADER_INPUT_BIND_DESC b{};hr(r->GetResourceBindingDesc(i,&b),"binding");
        require(b.BindCount==1&&b.Space==0,"single bindings");Binding x{b.Type,b.BindPoint,b.Dimension,0,(b.uFlags&D3D_SIF_COMPARISON_SAMPLER)!=0,b.Name,{}};
        if(b.Type==D3D_SIT_CBUFFER){auto *cb=r->GetConstantBufferByName(b.Name);D3D12_SHADER_BUFFER_DESC c{};hr(cb->GetDesc(&c),"cbuffer");x.size=c.Size;
            for(unsigned v=0;v<c.Variables;++v){auto *var=cb->GetVariableByIndex(v);D3D12_SHADER_VARIABLE_DESC vd{};hr(var->GetDesc(&vd),"variable");
                D3D12_SHADER_TYPE_DESC td{};hr(var->GetType()->GetDesc(&td),"variable type");
                x.variables.push_back({vd.Name,vd.StartOffset,vd.Size,(vd.uFlags&D3D_SVF_USED)!=0,td.Type==D3D_SVT_INT||td.Type==D3D_SVT_UINT||td.Type==D3D_SVT_BOOL});}}
        else require(b.Type==D3D_SIT_TEXTURE||b.Type==D3D_SIT_SAMPLER,"textures, samplers and cbuffers only");
        out.push_back(x);}
    return out;
}
ComPtr<ID3D12Resource> resource(ID3D12Device *d,const D3D12_RESOURCE_DESC &desc,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state){
    D3D12_HEAP_PROPERTIES heap{};heap.Type=type;ComPtr<ID3D12Resource> r;D3D12_CLEAR_VALUE clear{};clear.Format=desc.Format;
    std::fill(std::begin(clear.Color),std::end(clear.Color),-12345.F);
    hr(d->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,state,(desc.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)?&clear:nullptr,IID_PPV_ARGS(&r)),"resource");return r;}
ComPtr<ID3D12Resource> buffer(ID3D12Device *d,UINT64 bytes,D3D12_HEAP_TYPE type){D3D12_RESOURCE_DESC v{};v.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    v.Width=bytes;v.Height=1;v.DepthOrArraySize=v.MipLevels=1;v.SampleDesc.Count=1;v.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return resource(d,v,type,type==D3D12_HEAP_TYPE_UPLOAD?D3D12_RESOURCE_STATE_GENERIC_READ:D3D12_RESOURCE_STATE_COPY_DEST);}
void barrier(ID3D12GraphicsCommandList *l,ID3D12Resource *r,D3D12_RESOURCE_STATES a,D3D12_RESOURCE_STATES b){D3D12_RESOURCE_BARRIER v{};v.Transition={r,UINT_MAX,a,b};l->ResourceBarrier(1,&v);}
// render: the frame in the upper-left render rectangle of an output-size
// allocation; upsampled: every output texel from render texel (x/2, y/2);
// padded: the upsampled frame in the upper-left of a double-size allocation
// (render-space UVs then address it), edge texels repeated beyond it.
enum class Scene {render,upsampled,padded};
struct Inputs {Scene scene{};std::array<bool,128> upsample{};bool full_screen{};};
// CBScreen as the game fills it for a draw at size (w, h); unknown fields keep test data.
bool screen_field(const std::string &n){
    for(const char *k:{"fScreenOffset","fScreenScale","fScreenSize","fScreenInverseSize","iViewOffset","iViewSize","fViewOffset","fViewSize","fViewInverseSize","bCheckerboard"})if(n==k)return true;
    return false;}
void screen(std::byte *cb,const Variable &v,unsigned w,unsigned h){
    const auto put=[&](std::array<float,2> f){std::memcpy(cb+v.offset,f.data(),std::min<std::size_t>(v.size,8));};
    const auto puti=[&](std::array<unsigned,2> u){std::memcpy(cb+v.offset,u.data(),std::min<std::size_t>(v.size,8));};
    if(v.name=="fScreenOffset")put({1,-1});else if(v.name=="fScreenScale")put({.5F,-.5F});
    else if(v.name=="fScreenSize"||v.name=="fViewSize")put({float(w),float(h)});
    else if(v.name=="fScreenInverseSize"||v.name=="fViewInverseSize")put({1.F/w,1.F/h});
    else if(v.name=="fViewOffset")put({0,0});else if(v.name=="iViewOffset")puti({0,0});else if(v.name=="iViewSize")puti({w,h});
    else if(v.name=="bCheckerboard")puti({0,0});}
struct Gpu {
    ComPtr<ID3D12Device> device;ComPtr<ID3D12InfoQueue> messages;ComPtr<ID3D12CommandQueue> queue;ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Fence> fence;UINT64 sequence{};
    explicit Gpu(bool warp,LUID luid){
        ComPtr<ID3D12Debug> debug;hr(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)),"debug");debug->EnableDebugLayer();
        ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter1> adapter;hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"factory");
        if(warp)hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)),"warp");else hr(factory->EnumAdapterByLuid(luid,IID_PPV_ARGS(&adapter)),"adapter");
        hr(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)),"device");hr(device.As(&messages),"messages");
        D3D12_COMMAND_QUEUE_DESC q{};hr(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)),"queue");hr(device->CreateCommandAllocator(q.Type,IID_PPV_ARGS(&allocator)),"allocator");
        hr(device->CreateCommandList(0,q.Type,allocator.Get(),nullptr,IID_PPV_ARGS(&list)),"list");hr(list->Close(),"close");hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"fence");
    }
    void check(){unsigned failures=0;for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T bytes{};hr(messages->GetMessage(i,nullptr,&bytes),"message size");
        std::vector<std::byte> s(bytes);auto m=reinterpret_cast<D3D12_MESSAGE *>(s.data());hr(messages->GetMessage(i,m,&bytes),"message");
        if(m->Severity<=D3D12_MESSAGE_SEVERITY_WARNING){++failures;std::cerr<<m->pDescription<<'\n';}}messages->ClearStoredMessages();require(!failures,"D3D12 warnings/errors");}
    // Draws ps over a viewport (vw x vh) of an ow x oh target. Scene register
    // `scene` holds the render frame (render: upper-left rw x rh of the ow x oh
    // allocation; upsampled: every output texel from render texel (x/2, y/2)).
    std::vector<float> draw(std::span<const std::byte> ps,std::span<const std::byte> vs,const std::vector<Binding> &bindings,unsigned scene,
                            sf::FilterGeometry g,unsigned vw,unsigned vh,const Inputs &in){
        const unsigned ow=g.output_width,oh=g.output_height,rw=g.render_width,rh=g.render_height;
        hr(allocator->Reset(),"allocator reset");hr(list->Reset(allocator.Get(),nullptr),"list reset");
        unsigned textures=0;for(const auto &b:bindings)if(b.type==D3D_SIT_TEXTURE)textures=std::max(textures,b.point+1);
        std::vector<D3D12_ROOT_PARAMETER> parameters;D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,std::max(textures,1U),0,0,0};
        std::vector<D3D12_STATIC_SAMPLER_DESC> samplers;std::vector<std::pair<unsigned,unsigned>> cbs;
        for(const auto &b:bindings){
            if(b.type==D3D_SIT_CBUFFER){D3D12_ROOT_PARAMETER p{};p.ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;p.Descriptor.ShaderRegister=b.point;p.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
                parameters.push_back(p);cbs.push_back({b.point,b.size});}
            if(b.type==D3D_SIT_SAMPLER){D3D12_STATIC_SAMPLER_DESC s{};s.Filter=b.comparison?D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT:D3D12_FILTER_MIN_MAG_MIP_POINT;
                s.AddressU=s.AddressV=s.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;s.MaxLOD=D3D12_FLOAT32_MAX;s.ComparisonFunc=b.comparison?D3D12_COMPARISON_FUNC_LESS:D3D12_COMPARISON_FUNC_NEVER;
                s.ShaderRegister=b.point;s.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;samplers.push_back(s);}
        }
        const auto table=static_cast<unsigned>(parameters.size());
        {D3D12_ROOT_PARAMETER p{};p.ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;p.DescriptorTable={1,&range};p.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;parameters.push_back(p);}
        D3D12_ROOT_SIGNATURE_DESC rd{static_cast<UINT>(parameters.size()),parameters.data(),static_cast<UINT>(samplers.size()),samplers.data(),D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
        ComPtr<ID3DBlob> serial,error;hr(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&serial,&error),"root serialize");
        ComPtr<ID3D12RootSignature> root;hr(device->CreateRootSignature(0,serial->GetBufferPointer(),serial->GetBufferSize(),IID_PPV_ARGS(&root)),"root");
        D3D12_DESCRIPTOR_HEAP_DESC hd{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,std::max(textures,1U),D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,0};
        ComPtr<ID3D12DescriptorHeap> heap,rtv;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)),"SRV heap");
        hd={D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1,D3D12_DESCRIPTOR_HEAP_FLAG_NONE,0};hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv)),"RTV heap");
        const auto step=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        std::vector<ComPtr<ID3D12Resource>> keep;
        for(const auto &b:bindings){if(b.type!=D3D_SIT_TEXTURE)continue;
            D3D12_RESOURCE_DESC desc{};desc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;desc.SampleDesc.Count=1;desc.MipLevels=1;desc.DepthOrArraySize=1;
            D3D12_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=desc.Format;sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            const bool padded=b.point==scene&&in.scene==Scene::padded;
            if(b.dimension==D3D_SRV_DIMENSION_TEXTURE2D){desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=padded?2*ow:ow;desc.Height=padded?2*oh:oh;sd.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sd.Texture2D.MipLevels=1;}
            else if(b.dimension==D3D_SRV_DIMENSION_TEXTURE3D){desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE3D;desc.Width=desc.Height=16;desc.DepthOrArraySize=16;sd.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE3D;sd.Texture3D.MipLevels=1;}
            else if(b.dimension==D3D_SRV_DIMENSION_TEXTURE1D){desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE1D;desc.Width=256;desc.Height=1;sd.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE1D;sd.Texture1D.MipLevels=1;}
            else throw std::runtime_error("texture dimension");
            auto texture=resource(device.Get(),desc,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT rows{};UINT64 row{},bytes{};device->GetCopyableFootprints(&desc,0,1,0,&fp,&rows,&row,&bytes);
            auto upload=buffer(device.Get(),bytes,D3D12_HEAP_TYPE_UPLOAD);void *p{};D3D12_RANGE none{};hr(upload->Map(0,&none,&p),"texture map");
            const unsigned depth=desc.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE3D?16:1;
            for(unsigned z=0;z<depth;++z)for(unsigned y=0;y<desc.Height;++y){auto *rowp=reinterpret_cast<float *>(static_cast<std::byte *>(p)+fp.Footprint.RowPitch*(SIZE_T(z)*desc.Height+y));
                for(unsigned x=0;x<desc.Width;++x)for(unsigned k=0;k<4;++k){
                    unsigned sx=x,sy=y;
                    if(b.dimension==D3D_SRV_DIMENSION_TEXTURE2D){
                        if(b.point==scene){
                            if(in.scene==Scene::render&&(x>=rw||y>=rh)){rowp[4*x+k]=std::numeric_limits<float>::quiet_NaN();continue;}
                            if(in.scene!=Scene::render){sx=std::min(x,ow-1)*rw/ow;sy=std::min(y,oh-1)*rh/oh;}}
                        else if(b.point<in.upsample.size()&&in.upsample[b.point]){sx=x*rw/ow;sy=y*rh/oh;}}
                    rowp[4*x+k]=hashed(sx,sy,z+b.point*97,k);}}
            upload->Unmap(0,nullptr);D3D12_TEXTURE_COPY_LOCATION a{},c{};a.pResource=upload.Get();a.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;a.PlacedFootprint=fp;
            c.pResource=texture.Get();c.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;list->CopyTextureRegion(&c,0,0,0,&a,nullptr);
            barrier(list.Get(),texture.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            device->CreateShaderResourceView(texture.Get(),&sd,{heap->GetCPUDescriptorHandleForHeapStart().ptr+SIZE_T(b.point)*step});
            keep.push_back(texture);keep.push_back(upload);}
        // Constant buffers: deterministic bounded floats (same in every run).
        std::vector<ComPtr<ID3D12Resource>> constants;
        for(const auto &[reg,size]:cbs){const auto bytes=(std::max(size,16U)+255)&~255U;auto cb=buffer(device.Get(),bytes,D3D12_HEAP_TYPE_UPLOAD);void *p{};D3D12_RANGE none{};
            hr(cb->Map(0,&none,&p),"cb map");for(unsigned i=0;i<bytes/4;++i)static_cast<float *>(p)[i]=hashed(i,reg,7,3);
            for(const auto &b:bindings)if(b.type==D3D_SIT_CBUFFER&&b.point==reg&&b.name=="CBScreen")
                for(const auto &v:b.variables)screen(static_cast<std::byte *>(p),v,in.full_screen?ow:rw,in.full_screen?oh:rh);
            cb->Unmap(0,nullptr);constants.push_back(cb);}
        auto out_desc=D3D12_RESOURCE_DESC{};out_desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;out_desc.Width=ow;out_desc.Height=oh;out_desc.DepthOrArraySize=out_desc.MipLevels=1;
        out_desc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;out_desc.SampleDesc.Count=1;out_desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        auto output=resource(device.Get(),out_desc,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_RENDER_TARGET);device->CreateRenderTargetView(output.Get(),nullptr,rtv->GetCPUDescriptorHandleForHeapStart());
        const auto geometry=sf::effect_vertices(sf::EffectStage::depth_color,g,false);
        auto vertices=buffer(device.Get(),48,D3D12_HEAP_TYPE_UPLOAD);{void *p{};D3D12_RANGE none{};hr(vertices->Map(0,&none,&p),"vb");std::memcpy(p,geometry.values.data(),48);vertices->Unmap(0,nullptr);}
        static constexpr D3D12_INPUT_ELEMENT_DESC elements[]{{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.VS={vs.data(),vs.size()};pd.PS={ps.data(),ps.size()};pd.InputLayout={elements,2};
        pd.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;pd.SampleMask=UINT_MAX;pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
        pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;pd.NumRenderTargets=1;pd.RTVFormats[0]=out_desc.Format;pd.SampleDesc.Count=1;
        ComPtr<ID3D12PipelineState> pipeline;const auto created=device->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&pipeline));if(FAILED(created))check();hr(created,"pipeline");
        ID3D12DescriptorHeap *heaps[]{heap.Get()};list->SetDescriptorHeaps(1,heaps);list->SetGraphicsRootSignature(root.Get());list->SetPipelineState(pipeline.Get());
        for(unsigned i=0;i<cbs.size();++i)list->SetGraphicsRootConstantBufferView(i,constants[i]->GetGPUVirtualAddress());
        list->SetGraphicsRootDescriptorTable(table,heap->GetGPUDescriptorHandleForHeapStart());
        D3D12_VIEWPORT viewport{0,0,float(vw),float(vh),0,1};D3D12_RECT rect{0,0,LONG(vw),LONG(vh)};list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&rect);
        const auto handle=rtv->GetCPUDescriptorHandleForHeapStart();list->OMSetRenderTargets(1,&handle,FALSE,nullptr);
        const float poison[]{-12345,-12345,-12345,-12345};list->ClearRenderTargetView(handle,poison,0,nullptr);
        const D3D12_VERTEX_BUFFER_VIEW vb{vertices->GetGPUVirtualAddress(),48,16};list->IASetVertexBuffers(0,1,&vb);list->IASetPrimitiveTopology(geometry.topology);list->DrawInstanced(3,1,0,0);
        barrier(list.Get(),output.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT64 row{},bytes{};device->GetCopyableFootprints(&out_desc,0,1,0,&fp,nullptr,&row,&bytes);
        auto readback=buffer(device.Get(),bytes,D3D12_HEAP_TYPE_READBACK);D3D12_TEXTURE_COPY_LOCATION a{},c{};a.pResource=readback.Get();a.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;a.PlacedFootprint=fp;
        c.pResource=output.Get();c.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;list->CopyTextureRegion(&a,0,0,0,&c,nullptr);
        hr(list->Close(),"close");ID3D12CommandList *lists[]{list.Get()};queue->ExecuteCommandLists(1,lists);hr(queue->Signal(fence.Get(),++sequence),"signal");
        const HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);hr(fence->SetEventOnCompletion(sequence,event),"event");require(WaitForSingleObject(event,60000)==WAIT_OBJECT_0,"GPU completion");CloseHandle(event);
        hr(device->GetDeviceRemovedReason(),"device health");void *p{};D3D12_RANGE read_range{0,SIZE_T(bytes)};hr(readback->Map(0,&read_range,&p),"readback");
        std::vector<float> result(std::size_t(ow)*oh*4);for(unsigned y=0;y<oh;++y)std::memcpy(result.data()+std::size_t(y)*ow*4,static_cast<std::byte *>(p)+fp.Footprint.RowPitch*y,std::size_t(ow)*16);
        D3D12_RANGE empty{};readback->Unmap(0,&empty);check();return result;
    }
};
bool same(float a,float b){return std::isnan(a)?std::isnan(b):a==b;}
struct Lattice {std::size_t exact{},within{},outside{},total{};double worst{};};
// Generated output pixel (x, y) against native render pixel (x/2, y/2).
Lattice lattice(const std::vector<float> &full,const std::vector<float> &native,sf::FilterGeometry g,double tolerance){
    Lattice l;for(unsigned y=0;y<g.output_height;++y)for(unsigned x=0;x<g.output_width;++x)for(unsigned k=0;k<4;++k){
        const auto a=full[(std::size_t(y)*g.output_width+x)*4+k],b=native[(std::size_t(y*g.render_height/g.output_height)*g.output_width+x*g.render_width/g.output_width)*4+k];++l.total;
        if(same(a,b)){++l.exact;continue;}const double d=std::isnan(a)||std::isnan(b)?1e30:std::abs(double(a)-b)/(1.+std::abs(double(b)));
        l.worst=std::max(l.worst,d);if(d<=tolerance)++l.within;else ++l.outside;}
    return l;
}
// control: 1 = the unadapted shader must fail the oracle; 0 = report only (an
// edit can be a no-op, e.g. a raster position that only feeds scene loads).
struct Case {std::string name;std::vector<std::byte> ps,vs;unsigned scene;sf::ScreenShaderEdits expected{};bool check_expected{};double tolerance{};int pair{-1};int control{};};
struct Report {unsigned shaders{},draws{},failures{};};
void run_case(Gpu &gpu,const Case &c,std::ostream &out,Report &report){
    const sf::FilterGeometry identity{64,32,64,32},scaled{64,32,128,64};
    sf::ScreenShaderEdits e1,e2;const auto one=sf::make_screen_shader(c.ps,identity,c.scene,e1),two=sf::make_screen_shader(c.ps,scaled,c.scene,e2);
    require(e1==e2,c.name+": edits depend on geometry");
    if(c.check_expected)require(e2.position_reads==c.expected.position_reads&&e2.scene_samples==c.expected.scene_samples&&e2.scene_loads==c.expected.scene_loads,
        c.name+": unexpected edit counts");
    const auto bindings=reflect(c.ps);require(reflect(two).size()==bindings.size(),c.name+": reflection changed");
    const Inputs render{Scene::render},upsampled{Scene::upsampled};
    const auto native_identity=gpu.draw(c.ps,c.vs,bindings,c.scene,identity,64,32,render);
    const auto generated_identity=gpu.draw(one,c.vs,bindings,c.scene,identity,64,32,render);
    for(std::size_t i=0;i<native_identity.size();++i)require(same(native_identity[i],generated_identity[i]),c.name+": identity differs from native");
    const auto native=gpu.draw(c.ps,c.vs,bindings,c.scene,scaled,64,32,render);
    const auto generated=gpu.draw(two,c.vs,bindings,c.scene,scaled,128,64,upsampled);
    const auto unadapted=gpu.draw(c.ps,c.vs,bindings,c.scene,scaled,128,64,upsampled);report.draws+=5;
    const auto good=lattice(generated,native,scaled,c.tolerance),control=lattice(unadapted,native,scaled,c.tolerance);
    // Exact oracle: the UNMODIFIED native PS at full output with inputs that are
    // consistent there (padded scene, raster-read auxiliaries upsampled 2x2,
    // CBScreen for the output). It applies when every texture is read one way
    // and every used CBScreen field has a known size meaning.
    bool exact_oracle=true;Inputs oracle{Scene::padded,{},true};
    for(unsigned k=0;k<e2.access.size();++k){const auto a=e2.access[k];if(!a)continue;
        if(a&sf::access_other)exact_oracle=false;
        if(k!=c.scene){if(a==(sf::access_sample|sf::access_raster))exact_oracle=false;if(a==sf::access_raster)oracle.upsample[k]=true;}}
    for(const auto &b:bindings)if(b.type==D3D_SIT_CBUFFER&&b.name=="CBScreen")for(const auto &v:b.variables)if(v.used&&!screen_field(v.name))exact_oracle=false;
    std::size_t oracle_differences=SIZE_MAX;
    if(exact_oracle){const auto reference=gpu.draw(c.ps,c.vs,bindings,c.scene,scaled,128,64,oracle);++report.draws;oracle_differences=0;
        for(std::size_t i=0;i<reference.size();++i)if(!same(reference[i],generated[i]))++oracle_differences;}
    const bool pass=(exact_oracle?oracle_differences==0:good.outside==0)&&(c.control!=1||control.outside>0);
    out<<"{\"name\":\""<<c.name<<"\",\"edits\":["<<e2.position_reads<<','<<e2.scene_samples<<','<<e2.scene_loads<<"],\"identity_exact\":true"
       <<",\"lattice\":{\"exact\":"<<good.exact<<",\"within\":"<<good.within<<",\"outside\":"<<good.outside<<",\"total\":"<<good.total<<",\"worst\":"<<good.worst
       <<"},\"unadapted\":{\"exact\":"<<control.exact<<",\"outside\":"<<control.outside<<"},\"exact_oracle\":"<<(exact_oracle?"true":"false");
    if(exact_oracle)out<<",\"oracle_differences\":"<<oracle_differences;
    if(c.pair>=0){
        // The same full-output inputs through a hand-written adapter: identical bits.
        const auto hand=c.pair==0?sf::make_depth_color_shader(c.ps,scaled):sf::make_filter_shader(c.ps,scaled);
        const auto reference=gpu.draw(hand,c.vs,bindings,c.scene,scaled,128,64,upsampled);++report.draws;
        std::size_t differ{};for(std::size_t i=0;i<reference.size();++i)if(!same(reference[i],generated[i]))++differ;
        out<<",\"hand_adapter_differences\":"<<differ;if(differ){std::cerr<<c.name<<": differs from the hand adapter in "<<differ<<" values\n";++report.failures;}
    }
    out<<",\"pass\":"<<(pass?"true":"false")<<"}\n";++report.shaders;
    if(!pass){++report.failures;std::cerr<<c.name<<": exact_oracle="<<exact_oracle<<" differences="<<oracle_differences<<" lattice outside="<<good.outside<<" worst="<<good.worst<<" control outside="<<control.outside<<'\n';}
}
// Public stand-ins for the census families (no game shader is embedded).
const char *vs_source="struct O{float4 p:SV_Position;float2 uv:TexCoord0;};O main(float2 p:POSITION,float2 uv:TEXCOORD){O o;o.p=float4(p,0,1);o.uv=uv;return o;}";
std::vector<Case> synthetic(){
    const auto vs=compile(vs_source,"vs_5_0");std::vector<Case> cases;
    const auto add=[&](const char *name,const char *ps,unsigned scene,sf::ScreenShaderEdits expected,double tolerance,int control){
        cases.push_back({name,compile(ps,"ps_5_0"),vs,scene,expected,true,tolerance,-1,control});};
    // Depth color shape: scene UV sample, depth load at the raster position, view ray from the raster position.
    add("depth_tint","Texture2D<float4> s:register(t0);Texture2D<float> d:register(t1);SamplerState f:register(s0);"
        "cbuffer CBScreen:register(b1){float2 fScreenOffset,fScreenScale,fScreenSize,fScreenInverseSize;uint2 iViewOffset,iViewSize;float2 fViewOffset,fViewSize,fViewInverseSize;};"
        "float4 main(float4 p:SV_Position,float2 uv:TexCoord0):SV_Target{float4 x=s.Sample(f,uv);float z=d.Load(int3(p.xy,0));float2 n=(p.xy-fViewOffset)*fViewInverseSize;"
        "return float4(x.rgb*z,n.x*.001+n.y*.001);}",0,{2,1,0},1e-3,1);
    // ColorCorrectCube shape: scene load at the raster position, then a 3-D LUT.
    add("cube","Texture2D<float4> s:register(t0);Texture3D<float4> l:register(t1);SamplerState f:register(s0);"
        "float4 main(float4 p:SV_Position):SV_Target{float4 x=s.Load(int3(p.xy,0));return float4(l.Sample(f,saturate(x.rgb)).rgb,x.a);}",0,{1,0,1},0,0);
    // Depth sampled at the native UV (render space, unchanged), scene at the raster position.
    add("cube_table","Texture2D<float4> s:register(t0);Texture2D<float> d:register(t1);Texture3D<float4> l:register(t2);SamplerState f:register(s0);"
        "float4 main(float4 p:SV_Position,float2 uv:TexCoord0):SV_Target{float z=d.SampleLevel(f,uv,0);float4 x=s.Load(int3(p.xy,0));return float4(lerp(x.rgb,l.Sample(f,saturate(x.rgb)).rgb,z),x.a);}",0,{1,0,1},0,0);
    // Vignette shape: scene UV sample plus a radial term from the raster position and CBScreen.
    add("vignette","Texture2D<float4> s:register(t0);SamplerState f:register(s1);cbuffer CBScreen:register(b1){float2 fScreenOffset,fScreenScale,fScreenSize,fScreenInverseSize;};"
        "float4 main(float4 p:SV_Position,float2 uv:TexCoord0):SV_Target{float4 x=s.Sample(f,uv);float2 c=p.xy*fScreenInverseSize-.5;return float4(x.rgb*(1-dot(c,c)*.5),x.a);}",0,{1,1,0},1e-3,1);
    // Blink shape: no textures; screen math on the native UV only.
    add("blink","cbuffer B:register(b3){float4 k;};float4 main(float4 p:SV_Position,float2 uv:TexCoord0):SV_Target{float2 c=uv*2-1;return float4(saturate(1-length(c)*k.xyz),1);}",
        UINT_MAX,{0,0,0},1e-2,0);
    return cases;
}
template<class F>bool rejected(F f){try{f();}catch(const std::exception &){return true;}return false;}
void run(int argc,wchar_t **argv){
    std::vector<std::wstring> args(argv+1,argv+argc);
    require(args.size()>=2,"usage: --warp|--hardware lo hi  <out-dir>  --synthetic | (--shader name ps vs scene|none [depth_color|motion])...");
    std::size_t i=0;const bool warp=args[i]==L"--warp";LUID luid{};
    if(!warp){require(args[i]==L"--hardware"&&args.size()>=4,"adapter");luid.LowPart=std::stoul(args[1]);luid.HighPart=std::stol(args[2]);i=2;}
    // "-": no artifacts (test suite); otherwise a new folder for the reports.
    const std::filesystem::path out=args[++i];const bool artifacts=out!=L"-";
    if(artifacts){require(!std::filesystem::exists(out),"preserve results");std::filesystem::create_directories(out);}++i;
    std::vector<Case> cases;
    for(;i<args.size();){
        if(args[i]==L"--synthetic"){auto s=synthetic();cases.insert(cases.end(),s.begin(),s.end());++i;continue;}
        require(args[i]==L"--shader"&&i+4<args.size(),"--shader name ps vs scene");
        Case c;c.name=std::filesystem::path(args[i+1]).string();c.ps=read(args[i+2]);c.vs=read(args[i+3]);
        c.scene=args[i+4]==L"none"?UINT_MAX:std::stoul(args[i+4]);c.tolerance=1e-2;i+=5;
        if(i<args.size()&&(args[i]==L"depth_color"||args[i]==L"motion")){c.pair=args[i]==L"depth_color"?0:1;++i;}
        cases.push_back(std::move(c));
    }
    require(!cases.empty(),"no cases");
    // Unsupported constructs are refused, never approximated.
    const auto vs=compile(vs_source,"vs_5_0");(void)vs;unsigned refusals=0;const sf::FilterGeometry g{64,32,128,64};sf::ScreenShaderEdits e;
    for(const char *ps:{"Texture2D s:register(t0);SamplerState f;float4 main(float2 uv:TexCoord0):SV_Target{return s.Sample(f,uv,int2(1,0));}",
        "Texture2D s:register(t0);float4 main(float4 p:SV_Position):SV_Target{uint w,h;s.GetDimensions(w,h);return float4(w,h,0,1);}",
        "Texture2D s:register(t0);float4 main(float2 uv:TexCoord0):SV_Target{return s.Load(int3(uv*100,0));}",
        "Texture2D s:register(t0);SamplerState f;float4 main(float2 uv:TexCoord0):SV_Target{return s.SampleGrad(f,uv,ddx(uv),ddy(uv));}"}){
        const auto bytes=compile(ps,"ps_5_0");require(rejected([&]{sf::make_screen_shader(bytes,g,0,e);}),"unsupported scene access must be refused");++refusals;}
    Gpu gpu(warp,luid);Report report;std::ofstream file;if(artifacts)file.open(out/"cases.jsonl");std::ostream &cases_out=artifacts?static_cast<std::ostream &>(file):std::cout;
    for(const auto &c:cases){try{run_case(gpu,c,cases_out,report);}catch(const std::exception &x){std::cerr<<c.name<<": "<<x.what()<<'\n';cases_out<<"{\"name\":\""<<c.name<<"\",\"error\":\""<<x.what()<<"\"}\n";++report.failures;}}
    std::ofstream summary;if(artifacts)summary.open(out/"result.json");summary<<"{\"passed\":"<<(report.failures?"false":"true")<<",\"warp\":"<<(warp?"true":"false")<<",\"shaders\":"<<report.shaders
        <<",\"draws\":"<<report.draws<<",\"refusals\":"<<refusals<<",\"failures\":"<<report.failures<<"}\n";
    std::cout<<(report.failures?"FAIL: ":"PASS: ")<<report.shaders<<" shaders, "<<report.draws<<" draws, "<<refusals<<" refusals, "<<report.failures<<" failures\n";
    require(!report.failures,"screen shader fixture failures");
}
}
int wmain(int argc,wchar_t **argv){try{run(argc,argv);return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
