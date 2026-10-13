// Private captured shader is supplied by path. No game process is opened and
// no proprietary shader is part of this source or the resulting executable.
#include "../src/scaled_temporal/haze_shader.hpp"
#include "../src/scaled_temporal/effects.hpp"
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include "../third_party/dxc_hash/DxilHash.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace {
using Microsoft::WRL::ComPtr;
namespace sf=mhw::scaled_temporal;
void require(bool v,const char *why){if(!v)throw std::runtime_error(why);}
void hr(HRESULT v,const char *why){if(FAILED(v))throw std::runtime_error(std::string(why)+" HRESULT="+std::to_string(static_cast<unsigned long>(v)));}
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::exception &){rejected=true;}require(rejected,"expected rejection");}
std::vector<std::byte> read(const std::filesystem::path &p){
    const auto size=std::filesystem::file_size(p);require(size<=262144,"bounded shader");
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));std::ifstream f(p,std::ios::binary);
    require(bool(f.read(reinterpret_cast<char *>(bytes.data()),static_cast<std::streamsize>(size))),"read shader");return bytes;
}
void save(const std::filesystem::path &p,std::span<const std::byte> b){require(!std::filesystem::exists(p),"preserve artifact");
    std::ofstream f(p,std::ios::binary);require(bool(f.write(reinterpret_cast<const char *>(b.data()),static_cast<std::streamsize>(b.size()))),"write artifact");}
void disassemble(const std::filesystem::path &p,std::span<const std::byte> b){ComPtr<ID3DBlob> v;
    hr(D3DDisassemble(b.data(),b.size(),0,nullptr,&v),"disassemble");save(p,{static_cast<const std::byte *>(v->GetBufferPointer()),v->GetBufferSize()});}
ComPtr<ID3DBlob> compile(const char *s){ComPtr<ID3DBlob> b,e;
    const auto result=D3DCompile(s,std::strlen(s),"fixture",nullptr,nullptr,"main","vs_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_WARNINGS_ARE_ERRORS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&b,&e);
    if(FAILED(result)&&e)std::cerr<<static_cast<const char *>(e->GetBufferPointer());hr(result,"vertex shader");return b;}
ComPtr<ID3D12Resource> resource(ID3D12Device *d,D3D12_RESOURCE_DESC desc,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state){
    D3D12_HEAP_PROPERTIES heap{};heap.Type=type;ComPtr<ID3D12Resource> r;
    D3D12_CLEAR_VALUE clear{};clear.Format=desc.Format;std::fill(std::begin(clear.Color),std::end(clear.Color),-12345.F);
    hr(d->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,state,
        (desc.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)?&clear:nullptr,IID_PPV_ARGS(&r)),"resource");return r;}
D3D12_RESOURCE_DESC texture(unsigned w,unsigned h,DXGI_FORMAT format){D3D12_RESOURCE_DESC v{};
    v.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;v.Width=w;v.Height=h;v.DepthOrArraySize=v.MipLevels=1;v.SampleDesc.Count=1;v.Format=format;return v;}
ComPtr<ID3D12Resource> buffer(ID3D12Device *d,UINT64 bytes,D3D12_HEAP_TYPE type){D3D12_RESOURCE_DESC v{};
    v.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;v.Width=bytes;v.Height=1;v.DepthOrArraySize=v.MipLevels=1;
    v.SampleDesc.Count=1;v.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return resource(d,v,type,type==D3D12_HEAP_TYPE_UPLOAD?D3D12_RESOURCE_STATE_GENERIC_READ:D3D12_RESOURCE_STATE_COPY_DEST);}
void barrier(ID3D12GraphicsCommandList *l,ID3D12Resource *r,D3D12_RESOURCE_STATES a,D3D12_RESOURCE_STATES b){
    D3D12_RESOURCE_BARRIER v{};v.Transition={r,UINT_MAX,a,b};l->ResourceBarrier(1,&v);}
struct Pixels {
    ComPtr<ID3D12Resource> data;D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 bytes{},row{};unsigned height{};
    Pixels(ID3D12Device *d,const D3D12_RESOURCE_DESC &desc,D3D12_HEAP_TYPE type):height(desc.Height*(desc.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE3D?desc.DepthOrArraySize:1)){
        d->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,&row,&bytes);data=buffer(d,bytes,type);}
    void write(std::span<const std::byte> b){require(b.size()==row*height,"upload size");void *p{};D3D12_RANGE none{};
        hr(data->Map(0,&none,&p),"upload map");for(unsigned y=0;y<height;++y)std::memcpy(static_cast<std::byte *>(p)+SIZE_T(y)*footprint.Footprint.RowPitch,b.data()+y*row,static_cast<std::size_t>(row));data->Unmap(0,nullptr);}
    void copy(ID3D12GraphicsCommandList *l,ID3D12Resource *r,bool upload){D3D12_TEXTURE_COPY_LOCATION a{},b{};
        a.pResource=data.Get();a.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;a.PlacedFootprint=footprint;
        b.pResource=r;b.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        if(upload)l->CopyTextureRegion(&b,0,0,0,&a,nullptr);else l->CopyTextureRegion(&a,0,0,0,&b,nullptr);}
    std::vector<float> read(){void *p{};D3D12_RANGE range{0,static_cast<SIZE_T>(bytes)};hr(data->Map(0,&range,&p),"readback map");
        std::vector<float> b(static_cast<std::size_t>(row)*height/4);for(unsigned y=0;y<height;++y)std::memcpy(b.data()+y*row/4,static_cast<std::byte *>(p)+SIZE_T(y)*footprint.Footprint.RowPitch,static_cast<std::size_t>(row));
        D3D12_RANGE none{};data->Unmap(0,&none);return b;}
};
void reflection(std::span<const std::byte> a,std::span<const std::byte> b){
    ComPtr<ID3D11ShaderReflection> x,y;hr(D3DReflect(a.data(),a.size(),IID_PPV_ARGS(&x)),"native reflection");hr(D3DReflect(b.data(),b.size(),IID_PPV_ARGS(&y)),"adapted reflection");
    D3D11_SHADER_DESC xd{},yd{};hr(x->GetDesc(&xd),"native desc");hr(y->GetDesc(&yd),"adapted desc");
    require(xd.BoundResources==yd.BoundResources&&xd.ConstantBuffers==yd.ConstantBuffers&&xd.InputParameters==yd.InputParameters&&xd.OutputParameters==yd.OutputParameters,"reflection layout");
    for(unsigned i=0;i<xd.BoundResources;++i){D3D11_SHADER_INPUT_BIND_DESC p{},q{};hr(x->GetResourceBindingDesc(i,&p),"resource reflection");hr(y->GetResourceBindingDesc(i,&q),"adapted resource reflection");
        require(std::strcmp(p.Name,q.Name)==0&&p.Type==q.Type&&p.BindPoint==q.BindPoint&&p.BindCount==q.BindCount&&p.Dimension==q.Dimension&&p.ReturnType==q.ReturnType,"resource layout unchanged");}
}
std::array<float,4> color(unsigned x,unsigned y,unsigned w,bool constant){return constant?std::array<float,4>{2,.5F,8,1}:
    std::array<float,4>{float((x*17+y*11)%29+1)*.125F,float((x*3+y*7)%13+1)*.25F,float((x+y*5)%17+1)*.5F,float(y*w+x+1)};}
struct Fixture {
    ComPtr<ID3D12Device> device;ComPtr<ID3D12InfoQueue> messages;ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12RootSignature> root;ComPtr<ID3DBlob> vs;std::vector<std::byte> native_vs;UINT64 sequence{};LUID actual_luid{};unsigned vendor{},device_id{};
    Fixture(bool warp,LUID luid){
        ComPtr<ID3D12Debug> debug;hr(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)),"debug");debug->EnableDebugLayer();
        ComPtr<ID3D12Debug1> validation;hr(debug.As(&validation),"GPU validation");validation->SetEnableGPUBasedValidation(TRUE);
        ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter1> adapter;hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"factory");
        if(warp)hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)),"warp");
        else if(luid.LowPart||luid.HighPart)hr(factory->EnumAdapterByLuid(luid,IID_PPV_ARGS(&adapter)),"adapter");
        else{unsigned matches=0;for(unsigned i=0;;++i){ComPtr<IDXGIAdapter1> candidate;const auto status=factory->EnumAdapters1(i,&candidate);if(status==DXGI_ERROR_NOT_FOUND)break;hr(status,"enumerate adapter");DXGI_ADAPTER_DESC1 desc{};hr(candidate->GetDesc1(&desc),"adapter description");
            if(desc.VendorId==0x10de&&!(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)){adapter=candidate;++matches;}}
            require(matches==1,"zero LUID requires one NVIDIA adapter");}
        DXGI_ADAPTER_DESC1 adapter_desc{};hr(adapter->GetDesc1(&adapter_desc),"selected adapter description");actual_luid=adapter_desc.AdapterLuid;vendor=adapter_desc.VendorId;device_id=adapter_desc.DeviceId;
        hr(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)),"device");hr(device.As(&messages),"messages");
        D3D12_COMMAND_QUEUE_DESC q{};hr(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)),"queue");
        hr(device->CreateCommandAllocator(q.Type,IID_PPV_ARGS(&allocator)),"allocator");hr(device->CreateCommandList(0,q.Type,allocator.Get(),nullptr,IID_PPV_ARGS(&list)),"list");hr(list->Close(),"initial close");hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"fence");
        D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,4,0,0,0};std::array<D3D12_ROOT_PARAMETER,5> parameters{};
        for(unsigned i=0;i<3;++i){parameters[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;parameters[i].Descriptor.ShaderRegister=i==2?3:i;parameters[i].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
        parameters[3].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[3].DescriptorTable={1,&range};parameters[3].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_NEVER;sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        parameters[4].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;parameters[4].Descriptor.ShaderRegister=2;parameters[4].ShaderVisibility=D3D12_SHADER_VISIBILITY_VERTEX;
        const std::array<D3D12_STATIC_SAMPLER_DESC,2> samplers{sampler,[&]{auto v=sampler;v.ShaderRegister=1;return v;}()};
        D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=5;rd.pParameters=parameters.data();rd.NumStaticSamplers=2;rd.pStaticSamplers=samplers.data();
        rd.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        ComPtr<ID3DBlob> serial,error;hr(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&serial,&error),"root serialize");hr(device->CreateRootSignature(0,serial->GetBufferPointer(),serial->GetBufferSize(),IID_PPV_ARGS(&root)),"root");
        vs=compile("cbuffer TestGeometry:register(b2){float2 scale;}; struct O {float4 p:SV_Position;float2 uv:TexCoord;float2 screen:ScreenPosition;}; O main(uint i:SV_VertexID){O o;o.p=float4(i==2?3:-1,i==1?-3:1,0,1);o.uv=float2(i==2?2:0,i==1?2:0)*scale;o.screen=o.p.xy;return o;}");
    }
    std::vector<float> draw(std::span<const std::byte> shader,sf::FilterGeometry g,unsigned scenario,bool distortion,bool expanded=false,bool actual_vs=false){
        const unsigned w=g.output_width,h=g.output_height;const float nan=std::numeric_limits<float>::quiet_NaN();
        hr(allocator->Reset(),"allocator reset");hr(list->Reset(allocator.Get(),nullptr),"list reset");
        D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=4;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ComPtr<ID3D12DescriptorHeap> heap,rtv;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)),"SRV heap");
        hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=1;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv)),"RTV heap");
        std::array<ComPtr<ID3D12Resource>,4> inputs;std::vector<Pixels> uploads;uploads.reserve(4);
        const std::array<DXGI_FORMAT,4> formats{DXGI_FORMAT_R32G32B32A32_FLOAT,DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R32G32B32A32_FLOAT,DXGI_FORMAT_R32G32B32A32_FLOAT};
        const unsigned channels[]{4,1,4,4};const auto step=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        for(unsigned t=0;t<4;++t){const auto tw=t==2?8U:t==0&&expanded?2*w:w,th=t==2?8U:t==0&&expanded?2*h:h,td=t==2?8U:1U;
            auto input_desc=texture(tw,th,formats[t]);if(t==2){input_desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE3D;input_desc.DepthOrArraySize=8;}
            inputs[t]=resource(device.Get(),input_desc,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
            uploads.emplace_back(device.Get(),input_desc,D3D12_HEAP_TYPE_UPLOAD);std::vector<float> data(std::size_t(tw)*th*td*channels[t],nan);
            for(unsigned z=0;z<td;++z)for(unsigned y=0;y<th;++y)for(unsigned x=0;x<tw;++x){const auto at=((std::size_t(z)*th+y)*tw+x)*channels[t];
                if(t==0){if(x<w&&y<h){const auto c=color(x,y,w,false);std::copy(c.begin(),c.end(),data.begin()+at);}continue;}
                if(t==2){for(unsigned k=0;k<4;++k)data[at+k]=float((x*3+y*5+z*7+k*2)%13+1)/16.F;continue;}
                // Native t3 uses unclamped bilinear UVs and legitimately reads one
                // border texel. Poison only outside that footprint; depth clamps.
                if(t==1&&(x>=g.render_width||y>=g.render_height))continue;
                if(t==3&&(x>g.render_width||y>g.render_height))continue;
                if(t==1)data[at]=.15F+.65F*float((x/7+y/5)%5)/4.F;
                if(t==3){data[at]=.1F+.8F*float((x*3+y*7)%17)/16.F;data[at+1]=data[at+2]=data[at+3]=0;}
            }
            uploads.back().write(std::as_bytes(std::span(data)));uploads.back().copy(list.Get(),inputs[t].Get(),true);
            barrier(list.Get(),inputs[t].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            D3D12_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=formats[t];sd.ViewDimension=t==2?D3D12_SRV_DIMENSION_TEXTURE3D:D3D12_SRV_DIMENSION_TEXTURE2D;sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            if(t==2)sd.Texture3D.MipLevels=1;else sd.Texture2D.MipLevels=1;
            device->CreateShaderResourceView(inputs[t].Get(),&sd,{heap->GetCPUDescriptorHandleForHeapStart().ptr+SIZE_T(t)*step});
        }
        auto desc=texture(w,h,DXGI_FORMAT_R32G32B32A32_FLOAT);desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        auto output=resource(device.Get(),desc,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_RENDER_TARGET);device->CreateRenderTargetView(output.Get(),nullptr,rtv->GetCPUDescriptorHandleForHeapStart());
        Pixels pixels(device.Get(),desc,D3D12_HEAP_TYPE_READBACK);
        // 512 bytes for b0, then independent 256-byte slots for b1, b3, b2.
        std::array<float,320> constants{};
        for(unsigned i=0;i<4;++i){constants[48+5*i]=1;constants[80+5*i]=1;}
        constants[96]=.1F;constants[97]=-.2F;constants[98]=.3F;
        auto screen=constants.data()+128;screen[4]=float(w);screen[5]=float(h);screen[6]=1.F/w;screen[7]=1.F/h;screen[14]=float(g.render_width);screen[15]=float(g.render_height);
        auto haze=constants.data()+192;std::memcpy(haze,&scenario,4);haze[1]=.05F;haze[2]=.8F;haze[3]=.2F;haze[4]=.4F;
        haze[8]=.3F;haze[9]=.5F;haze[10]=.2F;haze[11]=1.3F;haze[12]=distortion?.045F:0;
        constants[256]=float(g.render_width)/w;constants[257]=float(g.render_height)/h;
        auto cb=buffer(device.Get(),constants.size()*4,D3D12_HEAP_TYPE_UPLOAD);void *p{};D3D12_RANGE none{};hr(cb->Map(0,&none,&p),"CB map");std::memcpy(p,constants.data(),constants.size()*4);cb->Unmap(0,nullptr);
        ComPtr<ID3D12Resource> native_vertices;
        static constexpr D3D12_INPUT_ELEMENT_DESC elements[]{
         {"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
         {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
        if(actual_vs){
            require(native_vs.size()==712,"captured haze VS");native_vertices=buffer(device.Get(),64,D3D12_HEAP_TYPE_UPLOAD);
            const auto geometry=sf::effect_vertices(sf::EffectStage::haze,g,true);
            void *mapped{};D3D12_RANGE empty{};hr(native_vertices->Map(0,&empty,&mapped),"native vertices");
            std::memcpy(mapped,geometry.values.data(),64);native_vertices->Unmap(0,nullptr);
        }
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.VS={vs->GetBufferPointer(),vs->GetBufferSize()};pd.PS={shader.data(),shader.size()};
        if(actual_vs){pd.VS={native_vs.data(),native_vs.size()};pd.InputLayout={elements,2};}
        pd.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;pd.SampleMask=UINT_MAX;
        pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
        pd.DepthStencilState.DepthEnable=FALSE;pd.DepthStencilState.StencilEnable=FALSE;pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pd.NumRenderTargets=1;pd.RTVFormats[0]=desc.Format;pd.SampleDesc.Count=1;ComPtr<ID3D12PipelineState> pipeline;
        const auto pipeline_hr=device->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&pipeline));
        if(FAILED(pipeline_hr))check_messages();hr(pipeline_hr,"filter pipeline");
        ID3D12DescriptorHeap *heaps[]{heap.Get()};list->SetDescriptorHeaps(1,heaps);list->SetGraphicsRootSignature(root.Get());list->SetPipelineState(pipeline.Get());
        const unsigned cb_offsets[]{0,512,768};for(unsigned i=0;i<3;++i)list->SetGraphicsRootConstantBufferView(i,cb->GetGPUVirtualAddress()+cb_offsets[i]);
        list->SetGraphicsRootConstantBufferView(4,cb->GetGPUVirtualAddress()+1024);list->SetGraphicsRootDescriptorTable(3,heap->GetGPUDescriptorHandleForHeapStart());
        D3D12_VIEWPORT viewport{0,0,float(w),float(h),0,1};D3D12_RECT rect{0,0,static_cast<LONG>(w),static_cast<LONG>(h)};
        list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&rect);const auto handle=rtv->GetCPUDescriptorHandleForHeapStart();
        list->OMSetRenderTargets(1,&handle,FALSE,nullptr);const float poison[]{-12345,-12345,-12345,-12345};list->ClearRenderTargetView(handle,poison,0,nullptr);
        if(actual_vs){const D3D12_VERTEX_BUFFER_VIEW vb{native_vertices->GetGPUVirtualAddress(),64,16};list->IASetVertexBuffers(0,1,&vb);}
        list->IASetPrimitiveTopology(actual_vs?D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->DrawInstanced(3,1,0,0);
        barrier(list.Get(),output.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);pixels.copy(list.Get(),output.Get(),false);
        hr(list->Close(),"close");ID3D12CommandList *lists[]{list.Get()};queue->ExecuteCommandLists(1,lists);hr(queue->Signal(fence.Get(),++sequence),"signal");
        const HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(event!=nullptr,"event");const auto event_hr=fence->SetEventOnCompletion(sequence,event);const auto wait=SUCCEEDED(event_hr)?WaitForSingleObject(event,30000):WAIT_FAILED;CloseHandle(event);hr(event_hr,"event completion");require(wait==WAIT_OBJECT_0,"GPU completion");
        hr(device->GetDeviceRemovedReason(),"device health");auto result=pixels.read();
        hr(allocator->Reset(),"retire allocator");hr(list->Reset(allocator.Get(),nullptr),"retire recording");hr(list->Close(),"retired close");return result;
    }
    void check_messages(){unsigned failures=0;for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T bytes{};hr(messages->GetMessage(i,nullptr,&bytes),"message size");
        std::vector<std::byte> storage(bytes);auto m=reinterpret_cast<D3D12_MESSAGE *>(storage.data());hr(messages->GetMessage(i,m,&bytes),"message");
        if(m->Severity<=D3D12_MESSAGE_SEVERITY_WARNING){++failures;std::cerr<<m->pDescription<<'\n';}}
        require(failures==0,"GPU validation warning/error");}
};
void run(int argc,wchar_t **argv){
    const bool warp=(argc==4||argc==5)&&std::wstring_view(argv[1])==L"--warp";
    require(warp||((argc==6||argc==7)&&std::wstring_view(argv[1])==L"--hardware"),"usage: --warp shader.dxbc output-dir OR --hardware luid-low luid-high shader.dxbc output-dir");
    const auto shader_path=std::filesystem::path(argv[warp?2:4]),out=std::filesystem::path(argv[warp?3:5]);
    require(!std::filesystem::exists(out),"preserve results");std::filesystem::create_directories(out);
    const auto native=read(shader_path);const sf::FilterGeometry identity{64,32,64,32},scaled{64,32,128,64};
    const auto one=sf::make_haze_shader(native,identity),two=sf::make_haze_shader(native,scaled);
    reflection(native,one);reflection(native,two);save(out/"identity.dxbc",one);save(out/"scaled.dxbc",two);disassemble(out/"scaled.asm",two);
    unsigned rejections=0;
    for(const auto g:std::array<sf::FilterGeometry,6>{{{0,32,64,64},{64,32,128,32},{64,32,96,48},{UINT_MAX,32,128,64},{64,32,32,16},{8192,8192,16385,16385}}}){
        rejects([&]{sf::make_haze_shader(native,g);});++rejections;}
    for(const auto offset:std::array<std::size_t,6>{0,4,24,28,4000,14595}){auto changed=native;changed[offset]^=std::byte{1};rejects([&]{sf::make_haze_shader(changed,scaled);});++rejections;}
    rejects([&]{sf::make_haze_shader(std::span(native).first(native.size()-1),scaled);});++rejections;
    LUID luid{};if(!warp){luid.LowPart=static_cast<DWORD>(std::stoul(argv[2]));luid.HighPart=static_cast<LONG>(std::stol(argv[3]));}
    Fixture f(warp,luid);if(argc==(warp?5:7))f.native_vs=read(argv[argc-1]);disassemble(out/"vertex.asm",{static_cast<const std::byte *>(f.vs->GetBufferPointer()),f.vs->GetBufferSize()});unsigned comparisons=0,draws=0;std::ofstream report(out/"cases.jsonl");
    for(unsigned technique=0;technique<5;++technique)for(bool distortion:{false,true}){
        const auto expected=f.draw(native,identity,technique,distortion),actual=f.draw(one,identity,technique,distortion);draws+=2;
        require(expected.size()==actual.size(),"identity lengths");
        for(std::size_t i=0;i<actual.size();++i){require(std::isfinite(actual[i])&&expected[i]==actual[i],"identity native comparison");++comparisons;}
        // Independent oracle: the UNMODIFIED native shader samples a double-size
        // color allocation containing the full image in its upper-left quarter.
        // Depth/noise and all constants are identical. No adapter-generated
        // shader is used to calculate the reference result.
        const auto reference=f.draw(native,scaled,technique,distortion,true),pixels=f.draw(two,scaled,technique,distortion);draws+=2;
        require(reference.size()==pixels.size(),"scaled lengths");
        for(std::size_t i=0;i<pixels.size();++i){require(std::isfinite(pixels[i])&&pixels[i]==reference[i],"scaled native texture-layout oracle");++comparisons;}
        if(!f.native_vs.empty()){
            const auto geometry_pixels=f.draw(two,scaled,technique,distortion,false,true);++draws;
            require(geometry_pixels==pixels,"captured native VS and production geometry agree with independent VS");
            comparisons+=static_cast<unsigned>(pixels.size());
        }
        if(!distortion){
            for(unsigned y=0;y<scaled.output_height;++y)for(unsigned x=0;x<scaled.output_width;++x){
                const double px=std::min(double(x)+.5,double(scaled.output_width)-1)-.5;
                const double py=std::min(double(y)+.5,double(scaled.output_height)-1)-.5;
                const auto x0=static_cast<unsigned>(std::floor(px)),y0=static_cast<unsigned>(std::floor(py));
                const auto a=color(x0,y0,scaled.output_width,false),b=color(std::min(x0+1,scaled.output_width-1),y0,scaled.output_width,false);
                const auto c=color(x0,std::min(y0+1,scaled.output_height-1),scaled.output_width,false),d=color(std::min(x0+1,scaled.output_width-1),std::min(y0+1,scaled.output_height-1),scaled.output_width,false);
                for(unsigned k=0;k<3;++k){const double ref=(1-(py-y0))*((1-(px-x0))*a[k]+(px-x0)*b[k])+(py-y0)*((1-(px-x0))*c[k]+(px-x0)*d[k]);
                    const auto delta=std::abs(pixels[(std::size_t(y)*scaled.output_width+x)*4+k]-ref);
                    if(delta>.00003)std::cerr<<"CPU oracle technique="<<technique<<" x="<<x<<" y="<<y<<" channel="<<k<<" actual="<<pixels[(std::size_t(y)*scaled.output_width+x)*4+k]<<" expected="<<ref<<'\n';
                    require(delta<=.00003,"no-distortion CPU color oracle");++comparisons;}
            }
        }
        report<<"{\"technique\":"<<technique<<",\"distortion\":"<<(distortion?"true":"false")<<",\"identity_exact\":true,\"scaled_exact\":true}\n";
        save(out/("technique-"+std::to_string(technique)+(distortion?"-distorted":"-plain")+".rgba32f"),std::as_bytes(std::span(pixels)));
    }
    f.check_messages();hr(f.allocator->Reset(),"final allocator retirement");hr(f.list->Reset(f.allocator.Get(),nullptr),"final recording retirement");hr(f.list->Close(),"final close");
    std::ofstream summary(out/"result.json");summary<<"{\"passed\":true,\"warp\":"<<(warp?"true":"false")<<",\"adapter_luid\":["<<f.actual_luid.HighPart<<','<<f.actual_luid.LowPart<<"],\"vendor\":"<<f.vendor<<",\"device\":"<<f.device_id<<",\"draws\":"<<draws<<",\"rejections\":"<<rejections<<",\"float_comparisons\":"<<comparisons<<",\"gpu_warnings_errors\":0,\"reflection_unchanged\":true,\"recordings_retired\":true,\"native_geometry_verified\":"<<(!f.native_vs.empty()?"true":"false")<<"}\n";
    require(bool(summary),"result write");std::cout<<"PASS: "<<draws<<" draws; "<<comparisons<<" float comparisons; "<<rejections<<" rejected inputs\n";
}
}
int wmain(int argc,wchar_t **argv){try{run(argc,argv);return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
