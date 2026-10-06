#pragma once
// Offline native-effect routing fixture. Private game shaders are supplied by
// path and never embedded. Both comparison routes use real GPU draws.
#include "../src/scaled_temporal/effects.hpp"
#include "../src/temporal_present/observer.hpp"
#include <MinHook.h>
#include <windows.h>
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace mhw::fixtures::effects {
using Microsoft::WRL::ComPtr;namespace sf=mhw::scaled_temporal;
void require(bool v,const char *s){if(!v)throw std::runtime_error(s);}
void hr(HRESULT v,const char *s){if(FAILED(v))throw std::runtime_error(std::string(s)+" HRESULT="+std::to_string(static_cast<unsigned>(v)));}
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::exception &){rejected=true;}require(rejected,"expected rejection");}
std::vector<std::byte> read(const std::filesystem::path &p){const auto n=std::filesystem::file_size(p);require(n<=262144,"shader bound");std::vector<std::byte> b(static_cast<std::size_t>(n));
    std::ifstream f(p,std::ios::binary);require(bool(f.read(reinterpret_cast<char *>(b.data()),static_cast<std::streamsize>(n))),"shader read");return b;}
D3D12_RESOURCE_DESC texture(unsigned w,unsigned h,DXGI_FORMAT format){D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=w;d.Height=h;d.MipLevels=d.DepthOrArraySize=1;d.SampleDesc.Count=1;d.Format=format;return d;}
ComPtr<ID3D12Resource> resource(ID3D12Device *device,const D3D12_RESOURCE_DESC &d,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state){
    D3D12_HEAP_PROPERTIES h{};h.Type=type;D3D12_CLEAR_VALUE clear{d.Format,{64,32,16,1}};ComPtr<ID3D12Resource> p;
    hr(device->CreateCommittedResource(&h,D3D12_HEAP_FLAG_NONE,&d,state,(d.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)?&clear:nullptr,IID_PPV_ARGS(&p)),"resource");return p;}
ComPtr<ID3D12Resource> buffer(ID3D12Device *device,UINT64 bytes,D3D12_HEAP_TYPE type){D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=1;d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return resource(device,d,type,type==D3D12_HEAP_TYPE_UPLOAD?D3D12_RESOURCE_STATE_GENERIC_READ:D3D12_RESOURCE_STATE_COPY_DEST);}
void upload(ID3D12Resource *resource,std::span<const std::byte> bytes){void *p{};D3D12_RANGE none{};hr(resource->Map(0,&none,&p),"upload map");std::memcpy(p,bytes.data(),bytes.size());resource->Unmap(0,nullptr);}
void barrier(ID3D12GraphicsCommandList *list,ID3D12Resource *resource,D3D12_RESOURCE_STATES a,D3D12_RESOURCE_STATES b){D3D12_RESOURCE_BARRIER x{};x.Transition={resource,UINT_MAX,a,b};list->ResourceBarrier(1,&x);}
struct Pixels {
    ComPtr<ID3D12Resource> data;D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 row{},bytes{};unsigned rows{},slices{};
    Pixels(ID3D12Device *d,const D3D12_RESOURCE_DESC &desc,D3D12_HEAP_TYPE type){d->GetCopyableFootprints(&desc,0,1,0,&footprint,&rows,&row,&bytes);slices=footprint.Footprint.Depth;data=buffer(d,bytes,type);}
    void write(std::span<const std::byte> b){require(b.size()==row*rows*slices,"pixels input size");void *p{};D3D12_RANGE none{};hr(data->Map(0,&none,&p),"pixel upload");
        for(unsigned z=0;z<slices;++z)for(unsigned y=0;y<rows;++y)std::memcpy(static_cast<std::byte *>(p)+(SIZE_T(z)*rows+y)*footprint.Footprint.RowPitch,b.data()+(SIZE_T(z)*rows+y)*row,static_cast<std::size_t>(row));data->Unmap(0,nullptr);}
    void copy(ID3D12GraphicsCommandList *list,ID3D12Resource *image,bool in){D3D12_TEXTURE_COPY_LOCATION a{},b{};a.pResource=data.Get();a.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;a.PlacedFootprint=footprint;b.pResource=image;b.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        if(in)list->CopyTextureRegion(&b,0,0,0,&a,nullptr);else list->CopyTextureRegion(&a,0,0,0,&b,nullptr);}
    std::vector<unsigned> read(){std::vector<unsigned> b(static_cast<std::size_t>(row)*rows*slices/4);void *p{};D3D12_RANGE range{0,static_cast<SIZE_T>(bytes)};hr(data->Map(0,&range,&p),"pixel readback");
        for(unsigned z=0;z<slices;++z)for(unsigned y=0;y<rows;++y)std::memcpy(b.data()+(SIZE_T(z)*rows+y)*row/4,static_cast<std::byte *>(p)+(SIZE_T(z)*rows+y)*footprint.Footprint.RowPitch,static_cast<std::size_t>(row));
        D3D12_RANGE none{};data->Unmap(0,&none);return b;}
};
sf::EffectVertices vertices(sf::EffectStage stage,sf::FilterGeometry g,bool full){
    // Independent fixture specification, rather than calling the production
    // geometry builder to define its own expected result.
    sf::EffectVertices v;
    if(stage==sf::EffectStage::final_screen){const float u=2.F*(full?g.output_width:g.render_width)/g.output_width-1.F/g.output_width;
        const float t=2.F*(full?g.output_height:g.render_height)/g.output_height-1.F/g.output_height;
        v.values={-1,1,0,0,-1,-3,0,t,3,1,u,0};v.count=3;v.topology=D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    }else{float u=1,t=1;if(stage==sf::EffectStage::color_grade&&!full){u=float(g.render_width)/g.output_width;t=float(g.render_height)/g.output_height;}
        // Ordered native tuples from the independently captured color-grade
        // draw; their middle two vertices differ from the motion-filter draw.
        if(stage==sf::EffectStage::color_grade)v.values={-1,1,0,0,1,1,u,0,-1,-1,0,t,1,-1,u,t};
        else v.values={-1,1,0,0,-1,-1,0,t,1,1,u,0,1,-1,u,t};
        v.count=4;v.topology=D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;}
    return v;
}
struct ViewportFeed {
    static inline unsigned calls{},count{},other_lists{};static inline ID3D12GraphicsCommandList *command{},*expected{};
    static inline const D3D12_VIEWPORT *address{};static inline std::uintptr_t caller{};static inline D3D12_VIEWPORT first{};
    static void observe(ID3D12GraphicsCommandList *list,unsigned n,const D3D12_VIEWPORT *v,std::uintptr_t at) noexcept {
        // The validation layer can forward through another hooked list object.
        // Check exactly-once delivery for the application's explicit list.
        if(list!=expected){++other_lists;return;}
        ++calls;count=n;command=list;address=v;caller=at;if(n&&v)first=*v;
    }
};
sf::EffectRecipe recipe(sf::EffectStage stage,sf::FilterGeometry g,ID3D12RootSignature *root,std::span<const std::byte> vs,std::span<const std::byte> ps,std::span<const float> v){
    static constexpr D3D12_INPUT_ELEMENT_DESC layout[]{{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
    sf::EffectRecipe r{stage,g,root,vs,ps};r.input_layout=layout;r.native_vertices=v;r.blend.IndependentBlendEnable=TRUE;
    for(auto &b:r.blend.RenderTarget){b.SrcBlend=b.SrcBlendAlpha=D3D12_BLEND_ONE;b.DestBlend=b.DestBlendAlpha=D3D12_BLEND_ZERO;b.BlendOp=b.BlendOpAlpha=D3D12_BLEND_OP_ADD;b.LogicOp=D3D12_LOGIC_OP_CLEAR;b.RenderTargetWriteMask=15;}
    r.raster.FillMode=D3D12_FILL_MODE_SOLID;r.raster.CullMode=D3D12_CULL_MODE_NONE;r.raster.FrontCounterClockwise=TRUE;
    r.depth.DepthFunc=D3D12_COMPARISON_FUNC_NEVER;r.depth.FrontFace=r.depth.BackFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_NEVER};return r;
}
ComPtr<ID3D12PipelineState> pipeline(ID3D12Device *device,const sf::EffectRecipe &r,std::span<const std::byte> pixel){D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
    d.pRootSignature=r.root;d.VS={r.vertex_shader.data(),r.vertex_shader.size()};d.PS={pixel.data(),pixel.size()};d.BlendState=r.blend;d.RasterizerState=r.raster;d.DepthStencilState=r.depth;
    d.InputLayout={r.input_layout.data(),static_cast<UINT>(r.input_layout.size())};d.SampleMask=UINT_MAX;d.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;d.NumRenderTargets=1;d.RTVFormats[0]=DXGI_FORMAT_R11G11B10_FLOAT;d.SampleDesc.Count=1;
    ComPtr<ID3D12PipelineState> p;hr(device->CreateGraphicsPipelineState(&d,IID_PPV_ARGS(&p)),"fixture PSO");return p;}
struct Fixture {
    ComPtr<ID3D12Device> device;ComPtr<ID3D12InfoQueue> info;ComPtr<ID3D12CommandQueue> queue;ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Fence> fence;ComPtr<ID3D12RootSignature> root;
    std::array<std::vector<std::byte>,3> vs,ps;ComPtr<ID3D12Resource> last_output;UINT64 sequence{},generation{};LUID luid{};unsigned vendor{},device_id{},rejections{};bool track_raster{};
    Fixture(bool warp,LUID requested,const std::filesystem::path &prefix,ID3D12Device *existing=nullptr){
        if(!existing){ComPtr<ID3D12Debug> debug;hr(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)),"debug");debug->EnableDebugLayer();ComPtr<ID3D12Debug1> gpu;hr(debug.As(&gpu),"validation");gpu->SetEnableGPUBasedValidation(TRUE);}
        ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter1> adapter;hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"factory");
        if(warp)hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)),"warp");else hr(factory->EnumAdapterByLuid(requested,IID_PPV_ARGS(&adapter)),"adapter");
        DXGI_ADAPTER_DESC1 ad{};hr(adapter->GetDesc1(&ad),"adapter identity");luid=ad.AdapterLuid;vendor=ad.VendorId;device_id=ad.DeviceId;
        if(existing){device=existing;const auto id=device->GetAdapterLuid();require(id.LowPart==luid.LowPart&&id.HighPart==luid.HighPart,"borrowed adapter");}
        else hr(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)),"device");hr(device.As(&info),"info");
        D3D12_COMMAND_QUEUE_DESC q{};hr(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)),"queue");hr(device->CreateCommandAllocator(q.Type,IID_PPV_ARGS(&allocator)),"allocator");hr(device->CreateCommandList(0,q.Type,allocator.Get(),nullptr,IID_PPV_ARGS(&list)),"list");hr(list->Close(),"close");hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"fence");
        D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,5,0,0,0};std::array<D3D12_ROOT_PARAMETER,5> parameters{};
        for(unsigned i=0;i<4;++i){parameters[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;parameters[i].Descriptor.ShaderRegister=i;parameters[i].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
        parameters[4].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[4].DescriptorTable={1,&range};parameters[4].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        std::array<D3D12_STATIC_SAMPLER_DESC,2> samplers{};for(unsigned i=0;i<2;++i){auto &s=samplers[i];s.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;s.AddressU=s.AddressV=s.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;s.MaxLOD=D3D12_FLOAT32_MAX;s.ShaderRegister=i;s.ComparisonFunc=D3D12_COMPARISON_FUNC_NEVER;s.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
        D3D12_ROOT_SIGNATURE_DESC rd{5,parameters.data(),2,samplers.data(),D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};ComPtr<ID3DBlob> serial,error;
        hr(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&serial,&error),"root serialization");hr(device->CreateRootSignature(0,serial->GetBufferPointer(),serial->GetBufferSize(),IID_PPV_ARGS(&root)),"root");
        for(unsigned i=0;i<3;++i){vs[i]=read(prefix.wstring()+L".draw-"+std::to_wstring(i)+L".vs.dxbc");ps[i]=read(prefix.wstring()+L".draw-"+std::to_wstring(i)+L".ps.dxbc");}
    }
    void messages(){unsigned errors=0;for(UINT64 i=0;i<info->GetNumStoredMessages();++i){SIZE_T n{};hr(info->GetMessage(i,nullptr,&n),"message size");std::vector<std::byte> b(n);auto m=reinterpret_cast<D3D12_MESSAGE *>(b.data());hr(info->GetMessage(i,m,&n),"message");if(m->Severity<=D3D12_MESSAGE_SEVERITY_WARNING){std::cerr<<m->pDescription<<'\n';++errors;}}require(!errors,"GPU warnings/errors");}
    void wait(){hr(queue->Signal(fence.Get(),++sequence),"signal");HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(event!=nullptr,"event");const auto status=fence->SetEventOnCompletion(sequence,event);const auto result=SUCCEEDED(status)?WaitForSingleObject(event,30000):WAIT_FAILED;CloseHandle(event);hr(status,"fence event");require(result==WAIT_OBJECT_0,"GPU wait");hr(device->GetDeviceRemovedReason(),"device health");}
    void viewport_listener_tests(){
        namespace obs=mhw::temporal_present::observer;
        ViewportFeed::expected=list.Get();
        hr(allocator->Reset(),"listener allocator");hr(list->Reset(allocator.Get(),nullptr),"listener list");
        const std::array<D3D12_VIEWPORT,2> v{{{0,0,128,64,0,1},{0,0,64,32,0,1}}};
        const auto before=ViewportFeed::calls;list->RSSetViewports(1,v.data());require(ViewportFeed::calls==before,"listener dormant before admission");
        obs::begin();obs::reset(list.Get(),++generation,nullptr);
        for(unsigned count:{1U,2U}){const auto previous=ViewportFeed::calls;list->RSSetViewports(count,v.data());
            require(ViewportFeed::calls==previous+1&&ViewportFeed::command==list.Get()&&ViewportFeed::count==count&&ViewportFeed::address==v.data()&&
                ViewportFeed::caller&&!std::memcmp(&ViewportFeed::first,v.data(),sizeof(D3D12_VIEWPORT)),"one unchanged viewport notification");}
        obs::stop();const auto stopped=ViewportFeed::calls;list->RSSetViewports(1,v.data());require(ViewportFeed::calls==stopped,"listener cutoff");
        obs::begin();obs::reset(list.Get(),++generation,nullptr);hr(list->Close(),"listener close");
        hr(list->Reset(allocator.Get(),nullptr),"listener discard");hr(list->Close(),"listener discarded");messages();
    }
    unsigned raster_tests(){
        namespace obs=mhw::temporal_present::observer;require(track_raster,"raster tests require observer");
        hr(allocator->Reset(),"raster allocator");hr(list->Reset(allocator.Get(),nullptr),"raster list");
        obs::reset(list.Get(),++generation,nullptr);obs::RasterBindings state;unsigned checks{};
        const auto unavailable=[&]{require(!obs::raster_current(list.Get(),state),"incomplete raster state rejected");++checks;};
        unavailable();auto vb=buffer(device.Get(),256,D3D12_HEAP_TYPE_UPLOAD);
        const D3D12_VERTEX_BUFFER_VIEW view{vb->GetGPUVirtualAddress(),256,16};
        const std::array<D3D12_VIEWPORT,2> viewport{{{0,0,128,64,0,1},{0,0,64,32,0,1}}};
        const std::array<D3D12_RECT,2> scissor{{{0,0,128,64},{0,0,64,32}}};
        const auto bind=[&]{list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);list->IASetVertexBuffers(0,1,&view);
            list->RSSetViewports(1,viewport.data());list->RSSetScissorRects(1,scissor.data());};
        const auto available=[&]{require(obs::raster_current(list.Get(),state)&&state.generation==generation&&state.vertex_buffer.BufferLocation==view.BufferLocation&&
            state.vertex_buffer.SizeInBytes==view.SizeInBytes&&state.vertex_buffer.StrideInBytes==16&&state.topology==D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP&&
            !std::memcmp(&state.viewport,&viewport[0],sizeof(state.viewport))&&!std::memcmp(&state.scissor,&scissor[0],sizeof(state.scissor)),"raster state restored after rebind");++checks;};
        bind();available();list->IASetVertexBuffers(1,1,&view);available();
        const std::array<D3D12_VERTEX_BUFFER_VIEW,3> extra_views{{
            {vb->GetGPUVirtualAddress()+16,240,32},{vb->GetGPUVirtualAddress()+32,224,16},{vb->GetGPUVirtualAddress()+48,208,8}}};
        list->IASetVertexBuffers(1,3,extra_views.data());available();
        require(state.vertex_mask==15&&!std::memcmp(state.vertex_buffers.data()+1,extra_views.data(),sizeof(extra_views)),"four API vertex streams retained");++checks;
        list->IASetVertexBuffers(2,1,nullptr);available();require(state.vertex_mask==11,"null stream invalidates exactly its slot");++checks;
        const D3D12_INDEX_BUFFER_VIEW indices{vb->GetGPUVirtualAddress()+64,192,DXGI_FORMAT_R16_UINT};list->IASetIndexBuffer(&indices);available();
        require(state.index_known&&!std::memcmp(&state.index_buffer,&indices,sizeof(indices)),"API index binding retained");++checks;
        obs::Predicate predicate;require(obs::predicate_current(list.Get(),predicate)&&predicate.known&&!predicate.buffer,"predicate remains available with indexed observer");++checks;
        list->IASetIndexBuffer(nullptr);available();require(!state.index_known,"null index binding invalidated");++checks;
        list->IASetIndexBuffer(&indices);
        D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=2;
        ComPtr<ID3D12DescriptorHeap> targets;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&targets)),"OM test heap");
        auto td=texture(128,64,DXGI_FORMAT_R11G11B10_FLOAT);td.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        auto target=resource(device.Get(),td,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_RENDER_TARGET);
        auto target2=resource(device.Get(),td,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_RENDER_TARGET);
        const auto first=targets->GetCPUDescriptorHandleForHeapStart();const D3D12_CPU_DESCRIPTOR_HANDLE second{first.ptr+device->GetDescriptorHandleIncrementSize(hd.Type)};
        device->CreateRenderTargetView(target.Get(),nullptr,first);device->CreateRenderTargetView(target2.Get(),nullptr,second);
        const auto output=[&](bool known,std::uintptr_t handle=0){require(obs::raster_current(list.Get(),state)&&state.output_known==known&&(!known||state.render_target==handle),"OM binding identity");++checks;};
        output(false);list->OMSetRenderTargets(1,&first,FALSE,nullptr);output(true,first.ptr);
        list->OMSetRenderTargets(1,&second,TRUE,nullptr);output(true,second.ptr);
        list->OMSetRenderTargets(0,nullptr,FALSE,nullptr);output(false);
        list->OMSetRenderTargets(2,&first,TRUE,nullptr);output(false);
        list->OMSetRenderTargets(1,&first,FALSE,nullptr);output(true,first.ptr);
        list->RSSetViewports(2,viewport.data());unavailable();list->RSSetViewports(1,viewport.data());available();
        list->RSSetScissorRects(2,scissor.data());unavailable();list->RSSetScissorRects(1,scissor.data());available();
        list->IASetVertexBuffers(0,1,nullptr);unavailable();list->IASetVertexBuffers(0,1,&view);available();
        list->ClearState(nullptr);unavailable();bind();available();output(false);require(!state.index_known&&state.vertex_mask==1,"ClearState removes all indexed/extra-stream observations");++checks;
        ComPtr<ID3D12CommandAllocator> bundle_allocator;ComPtr<ID3D12GraphicsCommandList> bundle;
        hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE,IID_PPV_ARGS(&bundle_allocator)),"bundle allocator");
        hr(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_BUNDLE,bundle_allocator.Get(),nullptr,IID_PPV_ARGS(&bundle)),"bundle list");
        bundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);hr(bundle->Close(),"bundle close");list->ExecuteBundle(bundle.Get());unavailable();
        bind();unavailable(); // A partial rebind cannot erase unknown bundle mutations.
        hr(list->Close(),"raster close");hr(list->Reset(allocator.Get(),nullptr),"raster reset");obs::reset(list.Get(),++generation,nullptr);
        unavailable();bind();available();output(false);require(!state.index_known&&state.vertex_mask==1,"Reset removes prior index generation");++checks;hr(list->Close(),"raster finish");
        // Neither recording was submitted; discard the second before dropping its VB.
        hr(list->Reset(allocator.Get(),nullptr),"raster discard");hr(list->Close(),"raster discard close");messages();return checks;
    }
    std::vector<unsigned> draw(unsigned stage,sf::FilterGeometry g,unsigned scenario,bool actual,bool guard,std::span<const unsigned> source={},
        ID3D12Resource *gpu_source=nullptr,D3D12_RESOURCE_STATES source_state=D3D12_RESOURCE_STATE_COPY_SOURCE){
        const auto role=static_cast<sf::EffectStage>(stage);const auto w=g.output_width,h=g.output_height;
        hr(allocator->Reset(),"reset allocator");hr(list->Reset(allocator.Get(),nullptr),"reset list");
        if(track_raster){mhw::temporal_present::observer::reset(list.Get(),++generation,nullptr);mhw::temporal_present::observer::RasterBindings empty;
            require(!mhw::temporal_present::observer::raster_current(list.Get(),empty),"reset invalidates raster bindings");}
        D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=5;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;ComPtr<ID3D12DescriptorHeap> heap,rtv;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)),"heap");hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=1;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv)),"rtv heap");
        const auto step=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        std::array<ComPtr<ID3D12Resource>,5> inputs;std::vector<Pixels> uploads;uploads.reserve(5);
        constexpr DXGI_FORMAT formats[]{DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R32_UINT,DXGI_FORMAT_R32G32_FLOAT,DXGI_FORMAT_R32G32B32A32_FLOAT};
        constexpr unsigned channels[]{1,1,1,2,4};
        for(unsigned t=0;t<5;++t){auto d=texture(w,h,formats[t]);unsigned c=channels[t];if(stage==1&&t==1){d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE3D;d.Width=d.Height=d.DepthOrArraySize=4;d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;c=4;}
            inputs[t]=resource(device.Get(),d,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);uploads.emplace_back(device.Get(),d,D3D12_HEAP_TYPE_UPLOAD);
            std::vector<unsigned> data(static_cast<std::size_t>(d.Width)*d.Height*d.DepthOrArraySize*c,0x7fc00000);
            for(unsigned z=0;z<d.DepthOrArraySize;++z)for(unsigned y=0;y<d.Height;++y)for(unsigned x=0;x<d.Width;++x){const auto at=((std::size_t(z)*d.Height+y)*d.Width+x)*c;
                if(t==0){data[at]=source.empty()?((15U*64+(x*3+y*5)%64)|((14U*64+(x*7+y*11)%64)<<11)|((16U*32+(x+y*3)%32)<<22)):source[std::size_t(y)*w+x];continue;}
                if(stage==1&&t==1){data[at]=std::bit_cast<unsigned>(.25F+x*.125F);data[at+1]=std::bit_cast<unsigned>(.25F+y*.125F);data[at+2]=std::bit_cast<unsigned>(.25F+z*.125F);data[at+3]=std::bit_cast<unsigned>(1.F);continue;}
                if(x>=g.render_width||y>=g.render_height)continue;for(unsigned j=0;j<c;++j)data[at+j]=t==1?std::bit_cast<unsigned>(1.F):0;
            }
            if(!t&&gpu_source){const auto input=gpu_source->GetDesc();require(input.Dimension==d.Dimension&&input.Width==d.Width&&input.Height==d.Height&&input.Format==d.Format&&input.MipLevels==1&&input.DepthOrArraySize==1&&input.SampleDesc.Count==1,"GPU source geometry");
                if(source_state!=D3D12_RESOURCE_STATE_COPY_SOURCE)barrier(list.Get(),gpu_source,source_state,D3D12_RESOURCE_STATE_COPY_SOURCE);
                list->CopyResource(inputs[t].Get(),gpu_source);
                if(source_state!=D3D12_RESOURCE_STATE_COPY_SOURCE)barrier(list.Get(),gpu_source,D3D12_RESOURCE_STATE_COPY_SOURCE,source_state);
            }else{uploads.back().write(std::as_bytes(std::span(data)));uploads.back().copy(list.Get(),inputs[t].Get(),true);}
            barrier(list.Get(),inputs[t].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            D3D12_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=d.Format;sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sd.ViewDimension=d.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE3D?D3D12_SRV_DIMENSION_TEXTURE3D:D3D12_SRV_DIMENSION_TEXTURE2D;sd.Texture2D.MipLevels=1;device->CreateShaderResourceView(inputs[t].Get(),&sd,{heap->GetCPUDescriptorHandleForHeapStart().ptr+SIZE_T(t)*step});
        }
        auto d=texture(w,h,DXGI_FORMAT_R11G11B10_FLOAT);d.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;auto output=resource(device.Get(),d,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_RENDER_TARGET);Pixels readback(device.Get(),d,D3D12_HEAP_TYPE_READBACK);device->CreateRenderTargetView(output.Get(),nullptr,rtv->GetCPUDescriptorHandleForHeapStart());
        std::array<float,256> cb{};if(stage==0){cb[42]=cb[46]=1;auto s=cb.data()+64;s[4]=float(w);s[5]=float(h);s[6]=1.F/w;s[7]=1.F/h;s[14]=float(g.render_width);s[15]=float(g.render_height);
            auto b=cb.data()+192;const unsigned samples=8;std::memcpy(b+2,&samples,4);b[3]=1.F/samples;b[4]=b[5]=1;b[6]=.01F;b[8]=(g.render_width-.5F)/w;b[9]=(g.render_height-.5F)/h;
        }else if(stage==1){unsigned type=scenario==1?5U:1U;std::memcpy(cb.data()+192,&type,4);if(scenario==2){const unsigned enabled=1,size=4;std::memcpy(cb.data()+204,&enabled,4);std::memcpy(cb.data()+201,&size,4);}if(scenario==3){const unsigned enabled=1;std::memcpy(cb.data()+140,&enabled,4);cb[143]=2.F;}}
        auto constants=buffer(device.Get(),sizeof(cb),D3D12_HEAP_TYPE_UPLOAD);upload(constants.Get(),std::as_bytes(std::span(cb)));
        const auto native=vertices(role,g,false),full=vertices(role,g,true);auto r=recipe(role,g,root.Get(),vs[stage],ps[stage],std::span(native.values).first(native.count*4));
        if(actual&&!guard){
            auto bad=r;bad.stage=static_cast<sf::EffectStage>(3);rejects([&]{sf::Effect x(device.Get(),bad);});++rejections;
            bad=r;bad.geometry.render_width=0;rejects([&]{sf::Effect x(device.Get(),bad);});++rejections;
            bad=r;bad.geometry.output_width+=1;rejects([&]{sf::Effect x(device.Get(),bad);});++rejections;
            bad=r;bad.root=nullptr;rejects([&]{sf::Effect x(device.Get(),bad);});++rejections;
            bad=r;bad.depth.DepthEnable=TRUE;rejects([&]{sf::Effect x(device.Get(),bad);});++rejections;
            bad=r;bad.blend.RenderTarget[0].BlendEnable=TRUE;rejects([&]{sf::Effect x(device.Get(),bad);});++rejections;
            bad=r;bad.raster.CullMode=D3D12_CULL_MODE_BACK;rejects([&]{sf::Effect x(device.Get(),bad);});++rejections;
            auto wrong_vertices=native.values;wrong_vertices[2]+=.25F;bad=r;bad.native_vertices=std::span(wrong_vertices).first(native.count*4);rejects([&]{sf::Effect x(device.Get(),bad);});++rejections;
            if(stage==1){auto wrong_order=native.values;for(unsigned i=0;i<4;++i)std::swap(wrong_order[4+i],wrong_order[8+i]);
                bad=r;bad.native_vertices=wrong_order;rejects([&]{sf::Effect x(device.Get(),bad);});++rejections;}
            std::array<D3D12_INPUT_ELEMENT_DESC,2> layout{};std::copy(r.input_layout.begin(),r.input_layout.end(),layout.begin());layout[1].AlignedByteOffset=12;bad=r;bad.input_layout=layout;rejects([&]{sf::Effect x(device.Get(),bad);});++rejections;
            auto wrong_pixel=ps[stage];wrong_pixel.back()^=std::byte{1};bad=r;bad.pixel_shader=wrong_pixel;rejects([&]{sf::Effect x(device.Get(),bad);});++rejections;
            auto wrong_vertex=vs[stage];wrong_vertex.back()^=std::byte{1};bad=r;bad.vertex_shader=wrong_vertex;rejects([&]{sf::Effect x(device.Get(),bad);});++rejections;
        }
        sf::Effect effect(device.Get(),r);auto original=pipeline(device.Get(),r,ps[stage]);const auto adapted=stage==0?sf::make_filter_shader(ps[0],g):ps[stage];auto reference=pipeline(device.Get(),r,adapted);
        // Give the native draw a nonzero first vertex and larger buffer, so a
        // wrong offset restoration is observable by the following native draw.
        std::array<float,24> original_vertices{};std::copy(native.values.begin(),native.values.end(),original_vertices.begin()+8);auto vb=buffer(device.Get(),sizeof(original_vertices),D3D12_HEAP_TYPE_UPLOAD);upload(vb.Get(),std::as_bytes(std::span(original_vertices)));
        auto ref_vb=buffer(device.Get(),sizeof(full.values),D3D12_HEAP_TYPE_UPLOAD);upload(ref_vb.Get(),std::as_bytes(std::span(full.values)));
        sf::EffectBindings b{list.Get(),original.Get(),root.Get(),output.Get(),vb.Get(),{vb->GetGPUVirtualAddress(),sizeof(original_vertices),16},
            {0,0,float(stage==2?w:g.render_width),float(stage==2?h:g.render_height),0,1},{0,0,static_cast<LONG>(stage==2?w:g.render_width),static_cast<LONG>(stage==2?h:g.render_height)},native.topology,native.count,2};
        ID3D12DescriptorHeap *heaps[]{heap.Get()};list->SetDescriptorHeaps(1,heaps);list->SetGraphicsRootSignature(root.Get());for(unsigned i=0;i<4;++i)list->SetGraphicsRootConstantBufferView(i,constants->GetGPUVirtualAddress()+256*i);list->SetGraphicsRootDescriptorTable(4,heap->GetGPUDescriptorHandleForHeapStart());
        list->SetPipelineState(original.Get());list->IASetPrimitiveTopology(native.topology);list->IASetVertexBuffers(0,1,&b.vertex_buffer);list->RSSetViewports(1,&b.viewport);list->RSSetScissorRects(1,&b.scissor);
        auto handle=rtv->GetCPUDescriptorHandleForHeapStart();list->OMSetRenderTargets(1,&handle,FALSE,nullptr);const float poison[]{64,32,16,1};
        // Poison untouched output and match the resource's optimized clear.
        list->ClearRenderTargetView(handle,poison,0,nullptr);
        const auto check_raster=[&]{if(!track_raster)return;namespace obs=mhw::temporal_present::observer;obs::RasterBindings state;mhw::temporal_present::Bindings roots;
            require(obs::raster_current(list.Get(),state)&&state.generation==generation&&state.topology==b.topology&&
                !std::memcmp(&state.viewport,&b.viewport,sizeof(b.viewport))&&!std::memcmp(&state.scissor,&b.scissor,sizeof(b.scissor))&&
                !std::memcmp(&state.vertex_buffer,&b.vertex_buffer,sizeof(b.vertex_buffer)),"API-derived raster bindings");
            require(obs::inspect(list.Get(),roots)&&roots.known&&roots.valid&&roots.graphics.signature==reinterpret_cast<std::uintptr_t>(b.root)&&roots.pipeline==reinterpret_cast<std::uintptr_t>(b.pipeline),"API-derived graphics root/pipeline");};
        check_raster();
        if(actual){
            auto bad=b;bad.viewport.Width+=1;require(!effect.record(bad),"reject changed viewport");++rejections;
            bad=b;bad.root=nullptr;require(!effect.record(bad),"reject changed root");++rejections;
            bad=b;bad.first=UINT_MAX;require(!effect.record(bad),"reject VB overrun");++rejections;
            bad=b;bad.output=inputs[0].Get();require(!effect.record(bad),"reject non-RT output");++rejections;
            bad=b;bad.topology=D3D_PRIMITIVE_TOPOLOGY_LINELIST;require(!effect.record(bad),"reject wrong topology");++rejections;
            if(stage==2){bad=b;bad.topology=D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;require(!effect.record(bad),"reject non-native screen triangle topology");++rejections;}
            require(!effect.recorded(),"rejections precede commands");require(effect.record(b),"effect record");check_raster();
            rejects([&]{effect.retire(fence.Get(),sequence+1,false);});++rejections;
            rejects([&]{effect.retire(fence.Get(),sequence+1,true);});++rejections;
        }else if(!guard){D3D12_VIEWPORT viewport{0,0,float(w),float(h),0,1};D3D12_RECT rect{0,0,static_cast<LONG>(w),static_cast<LONG>(h)};D3D12_VERTEX_BUFFER_VIEW v{ref_vb->GetGPUVirtualAddress(),sizeof(full.values),16};list->SetPipelineState(reference.Get());list->IASetVertexBuffers(0,1,&v);list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&rect);list->DrawInstanced(full.count,1,0,0);}
        if(guard){if(actual)list->ClearRenderTargetView(handle,poison,0,nullptr);list->DrawInstanced(native.count,1,2,0);}
        barrier(list.Get(),output.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);readback.copy(list.Get(),output.Get(),false);
        hr(list->Close(),"close draw");ID3D12CommandList *lists[]{list.Get()};queue->ExecuteCommandLists(1,lists);wait();auto result=readback.read();
        hr(allocator->Reset(),"retire allocator");hr(list->Reset(allocator.Get(),nullptr),"retire recording");hr(list->Close(),"retire close");if(actual){effect.retire(fence.Get(),sequence,true);require(effect.retired(),"effect retired");require(!effect.record(b),"retired effect rejects");++rejections;}
        last_output=output;return result;
    }
};
}
