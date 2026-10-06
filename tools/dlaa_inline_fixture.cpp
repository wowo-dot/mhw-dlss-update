// Same-list raw conversion, SDK DLAA, native output handoff and downstream state proof.
// Uses archived raw captures only; never opens the game.
#include "../src/temporal_present/output.hpp"
#include "../src/temporal_present/bindings.hpp"
#include "../src/temporal_present/observer.hpp"
#include "../src/dlss_sdk/inline.hpp"
#include "../src/continuous_dlaa/retention.hpp"
#include "../src/continuous_dlaa/resolve.hpp"
#include "../src/scaled_temporal/resolve.hpp"
#include "../tests/scaled_effect_fixture.hpp"
#include "../src/aa_control/policy.hpp"
#include "../src/temporal_present/unfiltered.hpp"
#include "../src/temporal_gpu_inputs/live.hpp"
#include "../src/ngx_startup/startup.hpp"
#ifdef MHW_REFLEX_TEMPORAL_FIXTURE
#include "reflex_temporal_fixture.hpp"
#endif
#include <MinHook.h>
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>

extern "C" {
unsigned fixture_native_calls{},fixture_native_result{1};
std::array<std::uintptr_t,4> fixture_arguments{};
unsigned fixture_native(std::uint64_t,const wchar_t *,ID3D12Device *,unsigned);
unsigned fixture_invoke(std::uint64_t,const wchar_t *,ID3D12Device *,unsigned);
void fixture_return();
}
namespace {
using Microsoft::WRL::ComPtr;
namespace tp=mhw::temporal_present;
void require(bool v,const char *why){if(!v)throw std::runtime_error(why);}
void hr(HRESULT v,const char *why){require(SUCCEEDED(v),why);}
bool fixture_method(std::uintptr_t object,unsigned slot,std::uintptr_t &target) noexcept {
    target=(*reinterpret_cast<std::uintptr_t **>(object))[slot];return target!=0;
}
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::exception &){rejected=true;}require(rejected,"expected rejection");}
ComPtr<ID3D12Resource> resource(ID3D12Device *d,const D3D12_RESOURCE_DESC &desc,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state){
    ComPtr<ID3D12Resource> r;D3D12_HEAP_PROPERTIES heap{};heap.Type=type;
    hr(d->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,state,nullptr,IID_PPV_ARGS(&r)),"resource");return r;
}
D3D12_RESOURCE_DESC texture(unsigned w,unsigned h,DXGI_FORMAT f){
    D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=w;d.Height=h;
    d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Format=f;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;return d;
}
ComPtr<ID3D12Resource> buffer(ID3D12Device *d,UINT64 bytes,D3D12_HEAP_TYPE heap){
    D3D12_RESOURCE_DESC b{};b.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;b.Width=bytes;b.Height=1;b.DepthOrArraySize=b.MipLevels=1;
    b.SampleDesc.Count=1;b.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return resource(d,b,heap,heap==D3D12_HEAP_TYPE_UPLOAD?D3D12_RESOURCE_STATE_GENERIC_READ:D3D12_RESOURCE_STATE_COPY_DEST);
}
void transition(ID3D12GraphicsCommandList *l,ID3D12Resource *r,D3D12_RESOURCE_STATES a,D3D12_RESOURCE_STATES b){
    D3D12_RESOURCE_BARRIER v{};v.Transition={r,UINT_MAX,a,b};l->ResourceBarrier(1,&v);
}
ComPtr<ID3DBlob> compile(const char *s,const char *entry,const char *profile){
    ComPtr<ID3DBlob> c,e;auto result=D3DCompile(s,std::strlen(s),"fixture",nullptr,nullptr,entry,profile,
        D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_WARNINGS_ARE_ERRORS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&c,&e);
    if(FAILED(result)&&e)std::cerr.write(static_cast<const char *>(e->GetBufferPointer()),static_cast<std::streamsize>(e->GetBufferSize()));
    hr(result,"shader");return c;
}
// Independent arithmetic quantizer, including nearest-even halfway cases.
unsigned round_even(double x){const auto low=static_cast<unsigned>(std::floor(x));const auto fraction=x-low;return low+(fraction>.5||(fraction==.5&&(low&1)));}
unsigned channel(float v,unsigned bits){
    const double maximum=std::ldexp(2.0-std::ldexp(1.0,-static_cast<int>(bits)),15);
    const auto x=std::clamp(double(v),0.0,maximum);
    if(x<std::ldexp(1.,-14))return round_even(std::ldexp(x,14+static_cast<int>(bits)));
    int exponent{};const auto fraction=std::frexp(x,&exponent);const auto mantissa=round_even(std::ldexp(fraction,bits+1));
    return (static_cast<unsigned>(exponent+14)<<bits)+mantissa-(1U<<bits);
}
unsigned packed(const float *v){return channel(v[0],6)|(channel(v[1],6)<<11)|(channel(v[2],5)<<22);}
float unpack(unsigned v,unsigned bits){const auto e=v>>bits,m=v&((1U<<bits)-1);return e?std::ldexp(float((1U<<bits)+m),static_cast<int>(e)-15-static_cast<int>(bits)):std::ldexp(float(m),-14-static_cast<int>(bits));}
struct Readback {
    ComPtr<ID3D12Resource> value;D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 bytes{},row{};unsigned height{};
    Readback(ID3D12Device *d,const D3D12_RESOURCE_DESC &desc):height(desc.Height){
        d->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,&row,&bytes);value=buffer(d,bytes,D3D12_HEAP_TYPE_READBACK);
    }
    void copy(ID3D12GraphicsCommandList *l,ID3D12Resource *source){D3D12_TEXTURE_COPY_LOCATION a{},b{};
        a.pResource=source;a.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;b.pResource=value.Get();b.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;b.PlacedFootprint=footprint;
        l->CopyTextureRegion(&b,0,0,0,&a,nullptr);
    }
    std::vector<std::byte> read(){void *p{};D3D12_RANGE range{0,static_cast<SIZE_T>(bytes)};hr(value->Map(0,&range,&p),"readback map");
        std::vector<std::byte> out(static_cast<std::size_t>(row)*height);for(unsigned y=0;y<height;++y)std::memcpy(out.data()+row*y,static_cast<std::byte *>(p)+SIZE_T(footprint.Footprint.RowPitch)*y,static_cast<SIZE_T>(row));
        D3D12_RANGE none{};value->Unmap(0,&none);return out;
    }
};

void run(int argc,wchar_t **argv){
    namespace fs=std::filesystem;
#ifdef MHW_REFLEX_TEMPORAL_FIXTURE
    const int fixture_argc=argc-1;
#else
    const int fixture_argc=argc;
#endif
    require(fixture_argc==12||fixture_argc==13,"usage: inline_fixture runtime output legacy source prefix receipt luid_low luid_high width height case [native-effect-capture-prefix] [coexistence: Streamline-runtime]");
    const fs::path runtime=fs::canonical(argv[1]),output=fs::absolute(argv[2]),legacy=fs::canonical(argv[3]),source=fs::canonical(argv[4]);
    const std::wstring prefix=argv[5],mode=argv[11];const unsigned receipt=static_cast<unsigned>(std::stoul(argv[6]));
    const auto low=static_cast<DWORD>(std::stoul(argv[7]));const auto high=static_cast<LONG>(std::stol(argv[8]));
    const auto w=static_cast<unsigned>(std::stoul(argv[9])),h=static_cast<unsigned>(std::stoul(argv[10]));
    const bool scaled_stream=mode==L"upscale-resolve-continuous";
    const bool scaled_resolve=scaled_stream||mode==L"upscale-resolve-success"||mode==L"upscale-resolve-off"||
        mode==L"upscale-resolve-after-conversion"||mode==L"upscale-resolve-evaluate-result"||mode==L"upscale-resolve-after-handoff";
    const bool upscale=scaled_resolve||mode==L"upscale-performance"||mode==L"upscale-poison";
    const auto geometry=upscale?mhw::dlss_sdk::Geometry{{w,h},{w/2,h/2},{w,h},mhw::dlss_sdk::ReconstructionMode::performance}:
        mhw::dlss_sdk::Geometry::dlaa(w,h);
    require(mhw::dlss_sdk::valid_geometry(geometry)&&(!upscale||(!(w%2)&&!(h%2))),"fixture reconstruction geometry");
    const bool native_resolve=mode==L"resolve-success"||mode==L"resolve-continuous"||mode==L"resolve-stop"||
        mode==L"resolve-after-conversion"||mode==L"resolve-evaluate-result"||mode==L"resolve-after-handoff";
    const bool off_only=mode==L"aa-off"||mode==L"aa-off-srv"||mode==L"aa-off-all-srv"||mode==L"resolve-stop"||mode==L"upscale-resolve-off";
    const bool exclusive=scaled_resolve||native_resolve||mode==L"aa-success"||mode==L"aa-continuous"||off_only||mode==L"aa-after-conversion"||mode==L"aa-evaluate-result"||mode==L"aa-after-handoff"||mode==L"aa-missing-state";
    require(upscale||exclusive||mode==L"success"||mode==L"missing-state"||mode==L"bad-output"||mode==L"bad-jitter"||mode==L"continuous"||mode==L"continuous-async"||mode==L"continuous-retirement","fixture mode");
    const bool retirement=scaled_stream||mode==L"continuous-retirement"||mode==L"aa-continuous"||mode==L"resolve-continuous",asynchronous=mode==L"continuous-async"||retirement,continuous=mode==L"continuous"||asynchronous||exclusive;
    const bool success=scaled_resolve?(scaled_stream||mode==L"upscale-resolve-success"):upscale||(exclusive?(mode==L"aa-success"||mode==L"aa-continuous"||mode==L"resolve-success"||mode==L"resolve-continuous"):(mode==L"success"||continuous));
    const unsigned frame_count=scaled_stream?96:scaled_resolve?3:asynchronous?600:(continuous&&!exclusive?240:1);
    require(w>=32&&h>=32&&UINT64(w)*h<=4194304&&receipt<9,"fixture extent/receipt");
    require(!fs::exists(output),"preserve fixture");fs::create_directories(output);
    require(LoadLibraryExW(legacy.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)!=nullptr,"legacy provider");
    ComPtr<ID3D12Debug> debug;hr(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)),"debug");debug->EnableDebugLayer();
    ComPtr<ID3D12Debug1> validation;hr(debug.As(&validation),"GPU validation");validation->SetEnableGPUBasedValidation(TRUE);
    ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter1> adapter;ComPtr<ID3D12Device> device;
    hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"factory");hr(factory->EnumAdapterByLuid({low,high},IID_PPV_ARGS(&adapter)),"adapter");
    hr(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)),"device");
    ComPtr<ID3D12InfoQueue> messages;hr(device.As(&messages),"messages");
    hr(MH_Initialize()==MH_OK?S_OK:E_FAIL,"hooks");
#ifdef MHW_REFLEX_TEMPORAL_FIXTURE
    require(continuous&&success&&frame_count>=64,"coexistence requires continuous reconstruction");
    // One process-lived owner survives exception unwinding until process exit.
    auto* reflex=new ReflexTemporalFixture(device.Get(),factory.Get(),fs::canonical(argv[argc-1]),output);
#endif
    std::ofstream startup_log(output/L"startup.jsonl");std::array<std::uintptr_t,7> slots{};
    namespace startup=mhw::ngx_startup;
    startup::install({reinterpret_cast<void *>(&fixture_native),reinterpret_cast<std::uintptr_t>(&fixture_return),
        reinterpret_cast<std::uintptr_t>(slots.data()),runtime,output/L"startup-data",0,true},startup_log);
    require(fixture_invoke(startup::native_app,L"unchanged fixture path",device.Get(),18)==1&&fixture_native_calls==1,"native startup");
    require(startup::disable()&&startup::settled()&&startup::done(),"startup closure");
    const auto *initialization=startup::borrowed_initialization();require(initialization!=nullptr,"startup ownership");
    D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Fence> fence;
    hr(device->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)),"queue");hr(device->CreateCommandAllocator(qd.Type,IID_PPV_ARGS(&allocator)),"allocator");
    hr(device->CreateCommandList(0,qd.Type,allocator.Get(),nullptr,IID_PPV_ARGS(&list)),"list");hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"fence");
    std::array<ComPtr<ID3D12CommandAllocator>,3> allocator_pool;
    std::array<ComPtr<ID3D12GraphicsCommandList>,3> list_pool;
    ComPtr<ID3D12CommandQueue> second_queue;
    allocator_pool[0]=allocator;list_pool[0]=list;
    hr(device->CreateCommandQueue(&qd,IID_PPV_ARGS(&second_queue)),"second queue");
#ifdef MHW_REFLEX_TEMPORAL_FIXTURE
    reflex->create_swap(queue.Get(),factory.Get());
#endif
    if(asynchronous)for(unsigned i=1;i<3;++i){
        hr(device->CreateCommandAllocator(qd.Type,IID_PPV_ARGS(&allocator_pool[i])),"pool allocator");
        hr(device->CreateCommandList(0,qd.Type,allocator_pool[i].Get(),nullptr,IID_PPV_ARGS(&list_pool[i])),"pool list");
        hr(list_pool[i]->Close(),"pool initial close");
    }
    mhw::temporal_gpu_inputs::Live raw(device.Get(),w,h);
    std::array<ID3D12Resource *,3> raw_inputs{raw.input(0),raw.input(1),raw.input(2)};
    mhw::dlss_sdk::InlineFrame pass(device.Get(),geometry,raw_inputs,runtime,*initialization,continuous,retirement);
    std::unique_ptr<tp::Output> recovery;
    if(scaled_resolve)recovery=std::make_unique<tp::Output>(device.Get(),tp::SpatialSource{w,h,w/2,h/2},w,h,true,retirement);
    const auto private_images=pass.private_images();
    for(unsigned i=0;i<5;++i){const auto extent=i<3?geometry.render:geometry.output;const auto desc=private_images[i]->GetDesc();
        require(desc.Width==extent.width&&desc.Height==extent.height,"separate SDK image extents");}
    mhw::continuous_dlaa::RecordingPins recording_pins;
    std::array<std::uint64_t,3> pool_generation{};unsigned native_allocations=1;
    std::array<ComPtr<ID3D12Resource>,3> uploads;
    const wchar_t *roles[]{L"color",L"depth",L"motion"};
    for(unsigned i=0;i<3;++i){const auto path=source/(prefix+L"."+roles[i]+L"-"+std::to_wstring(receipt)+(i==1?L".f32":L".u32"));
        require(fs::file_size(path)==UINT64(w)*h*4,"raw extent");std::vector<std::byte> bytes(std::size_t(w)*h*4);std::ifstream f(path,std::ios::binary);
        require(bool(f.read(reinterpret_cast<char *>(bytes.data()),static_cast<std::streamsize>(bytes.size()))),"raw read");
        // A second run poisons every inactive pixel. An identical final image
        // demonstrates that padding cannot feed the reconstruction converter.
        if(mode==L"upscale-poison"||scaled_resolve)for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)
            if(x>=geometry.render.width||y>=geometry.render.height){const unsigned poison=i==1?0x7fc00000U:0xffffffffU;
                std::memcpy(bytes.data()+(SIZE_T(y)*w+x)*4,&poison,4);}
        const auto desc=raw_inputs[i]->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};UINT64 size{};
        device->GetCopyableFootprints(&desc,0,1,0,&layout,nullptr,nullptr,&size);uploads[i]=buffer(device.Get(),size,D3D12_HEAP_TYPE_UPLOAD);
        void *mapped{};D3D12_RANGE none{};hr(uploads[i]->Map(0,&none,&mapped),"upload map");
        for(unsigned y=0;y<h;++y)std::memcpy(static_cast<std::byte *>(mapped)+SIZE_T(y)*layout.Footprint.RowPitch,bytes.data()+SIZE_T(y)*w*4,SIZE_T(w)*4);
        uploads[i]->Unmap(0,nullptr);D3D12_TEXTURE_COPY_LOCATION from{},to{};from.pResource=uploads[i].Get();from.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;from.PlacedFootprint=layout;
        to.pResource=raw_inputs[i];to.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;list->CopyTextureRegion(&to,0,0,0,&from,nullptr);
    }
    const auto sd=texture(w,h,DXGI_FORMAT_R32G32B32A32_FLOAT),nd=texture(w,h,DXGI_FORMAT_R11G11B10_FLOAT);
    auto native=resource(device.Get(),nd,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    auto follow=resource(device.Get(),sd,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    auto gd=sd;gd.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;auto graphics=resource(device.Get(),gd,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_RENDER_TARGET);
    Readback native_pixels(device.Get(),nd),follow_pixels(device.Get(),sd),graphics_pixels(device.Get(),gd);
    D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=2;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ComPtr<ID3D12DescriptorHeap> heap,sampler,rtv;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)),"native heap");
    hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;hd.NumDescriptors=1;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&sampler)),"sampler heap");
    hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv)),"RTV heap");
    device->CreateRenderTargetView(graphics.Get(),nullptr,rtv->GetCPUDescriptorHandleForHeapStart());
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=nd.Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;
    device->CreateShaderResourceView(native.Get(),&srv,heap->GetCPUDescriptorHandleForHeapStart());
    const auto stride=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=sd.Format;uav.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(follow.Get(),nullptr,&uav,{heap->GetCPUDescriptorHandleForHeapStart().ptr+stride});
    D3D12_SAMPLER_DESC samp{};samp.Filter=D3D12_FILTER_MIN_MAG_MIP_POINT;samp.AddressU=samp.AddressV=samp.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;samp.MaxLOD=D3D12_FLOAT32_MAX;samp.ComparisonFunc=D3D12_COMPARISON_FUNC_NEVER;
    device->CreateSampler(&samp,sampler->GetCPUDescriptorHandleForHeapStart());

    std::array<D3D12_DESCRIPTOR_RANGE,4> ranges{{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,1},
        {D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,1,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,1,0,0}}};
    std::array<D3D12_ROOT_PARAMETER,7> cp{};
    cp[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;cp[0].DescriptorTable={2,ranges.data()};
    cp[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;cp[1].DescriptorTable={1,&ranges[2]};
    unsigned reg{};for(unsigned i:{2U,3U,5U,6U}){cp[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;cp[i].Descriptor={reg++,0};}
    cp[4].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;cp[4].Constants={9,0,1};
    std::array<D3D12_ROOT_PARAMETER,17> gp{};
    for(unsigned i=0;i<gp.size();++i){gp[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;gp[i].Constants={10+i,0,1};}
    gp[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;gp[0].DescriptorTable={1,&ranges[3]};
    gp[1]=cp[1];gp[16]=cp[0];gp[5].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;gp[5].Descriptor={0,0};
    auto make_root=[&](std::span<const D3D12_ROOT_PARAMETER> params){D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=static_cast<UINT>(params.size());rd.pParameters=params.data();
        ComPtr<ID3DBlob> blob,errors;ComPtr<ID3D12RootSignature> root;hr(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors),"native root serialization");
        hr(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)),"native root");return root;};
    auto root=make_root(cp),graphics_root=make_root(gp);
    constexpr char compute_code[]=R"(
cbuffer C:register(b0){uint width;uint height;} cbuffer R:register(b1){uint red;} cbuffer A:register(b2){uint alpha;} cbuffer Z:register(b3){uint zero;}
Texture2D<float3> src:register(t0); RWTexture2D<float4> outp:register(u0); SamplerState samp:register(s0);
[numthreads(16,16,1)]void cs(uint3 i:SV_DispatchThreadID){if(i.x<width&&i.y<height)outp[i.xy]=float4(src.Load(int3(i.xy,0))+float3(red,zero,zero),alpha);}
)";
    constexpr char graphic_code[]=R"(
cbuffer C:register(b0){uint width;uint height;uint red;uint alpha;} Texture2D<float3> src:register(t0);Texture2D<float3> second:register(t1);SamplerState samp:register(s0);
float4 vs(uint i:SV_VertexID):SV_Position{return float4(i==2?3:-1,i==1?-3:1,0,1);}
float4 ps(float4 p:SV_Position):SV_Target{return float4(src.SampleLevel(samp,p.xy/float2(width,height),0).r+red,second.Load(int3(p.xy,0)).gb,alpha);}
)";
    auto cs=compile(compute_code,"cs","cs_5_0"),vs=compile(graphic_code,"vs","vs_5_0"),ps=compile(graphic_code,"ps","ps_5_0");
    D3D12_COMPUTE_PIPELINE_STATE_DESC cd{};cd.pRootSignature=root.Get();cd.CS={cs->GetBufferPointer(),cs->GetBufferSize()};ComPtr<ID3D12PipelineState> pipeline,graphic_pipeline;
    hr(device->CreateComputePipelineState(&cd,IID_PPV_ARGS(&pipeline)),"native compute pipeline");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};pd.pRootSignature=graphics_root.Get();pd.VS={vs->GetBufferPointer(),vs->GetBufferSize()};pd.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
    pd.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
    pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
    pd.DepthStencilState.DepthEnable=FALSE;pd.DepthStencilState.StencilEnable=FALSE;pd.SampleMask=UINT_MAX;
    pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;pd.NumRenderTargets=1;pd.RTVFormats[0]=sd.Format;pd.SampleDesc.Count=1;
    hr(device->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&graphic_pipeline)),"native graphics pipeline");

    auto constants=buffer(device.Get(),1280,D3D12_HEAP_TYPE_UPLOAD);void *mapped{};D3D12_RANGE none{};hr(constants->Map(0,&none,&mapped),"constants map");
    std::memset(mapped,0,1280);const unsigned dims[]{w,h},red=3,alpha=11,draw_constants[]{w,h,7,11};
    std::memcpy(mapped,dims,sizeof(dims));std::memcpy(static_cast<std::byte *>(mapped)+256,&red,4);std::memcpy(static_cast<std::byte *>(mapped)+512,&alpha,4);
    std::memcpy(static_cast<std::byte *>(mapped)+1024,draw_constants,sizeof(draw_constants));constants->Unmap(0,nullptr);
    std::string reason;require(tp::observer::install(device.Get(),fixture_method,reason,fixture_argc==13),reason.c_str());tp::observer::begin();
    std::uint64_t record_ms{};unsigned native_resolves{},unfiltered_copies{},sdk_attempts{};
    std::vector<std::vector<std::byte>> recovery_frames;
    for(unsigned frame=0;frame<frame_count;++frame){
#ifdef MHW_REFLEX_TEMPORAL_FIXTURE
    reflex->begin(frame);
#endif
    if(retirement&&frame&&frame%10==0){
        const auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(event!=nullptr,"turnover event");
        hr(fence->SetEventOnCompletion(frame,event),"turnover completion");
        require(WaitForSingleObject(event,15000)==WAIT_OBJECT_0,"turnover drain");CloseHandle(event);
        // All uses of the shared descriptor heap completed. Older recording
        // generations can still hold the old destination until their Reset.
        native=resource(device.Get(),nd,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);++native_allocations;
        device->CreateShaderResourceView(native.Get(),&srv,heap->GetCPUDescriptorHandleForHeapStart());
    }
    if(retirement){
        mhw::continuous_dlaa::ResourcePins owned;for(unsigned i=0;i<3;++i)owned[i]=raw_inputs[i];owned[3]=native;
        recording_pins.collect(fence->GetCompletedValue());
        require(recording_pins.retain(list.Get(),frame+1,owned),"SDK recording ownership");pool_generation[frame%3]=frame+1;
    }
    tp::observer::reset(list.Get(),frame+1,nullptr);
    ID3D12DescriptorHeap *heaps[]{heap.Get(),sampler.Get()};list->SetDescriptorHeaps(2,heaps);list->SetComputeRootSignature(root.Get());list->SetGraphicsRootSignature(graphics_root.Get());list->SetPipelineState(pipeline.Get());
    list->SetComputeRootDescriptorTable(0,heap->GetGPUDescriptorHandleForHeapStart());list->SetComputeRootDescriptorTable(1,sampler->GetGPUDescriptorHandleForHeapStart());
    unsigned offset{};for(unsigned i:{2U,3U,5U,6U}){list->SetComputeRootConstantBufferView(i,constants->GetGPUVirtualAddress()+offset);offset+=256;}
    list->SetGraphicsRootDescriptorTable(0,heap->GetGPUDescriptorHandleForHeapStart());list->SetGraphicsRootDescriptorTable(1,sampler->GetGPUDescriptorHandleForHeapStart());
    list->SetGraphicsRootDescriptorTable(16,heap->GetGPUDescriptorHandleForHeapStart());list->SetGraphicsRootConstantBufferView(5,constants->GetGPUVirtualAddress()+1024);
    const D3D12_VIEWPORT viewport{0,0,float(w),float(h),0,1};const D3D12_RECT scissor{0,0,static_cast<LONG>(w),static_cast<LONG>(h)};
    list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&scissor);auto rt=rtv->GetCPUDescriptorHandleForHeapStart();list->OMSetRenderTargets(1,&rt,FALSE,nullptr);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    tp::Bindings observed;require(tp::observer::current(list.Get(),observed),"native sample");auto supplied=observed;if(mode==L"missing-state"||mode==L"aa-missing-state")supplied.invalidate();
    // Native TAA stand-in: initialize its output from the captured native input.
    // Exclusive AA cases deliberately leave this output uninitialized instead.
    if(!exclusive&&!upscale){++native_resolves;
    transition(list.Get(),raw_inputs[0],D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);
    transition(list.Get(),native.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyResource(native.Get(),raw_inputs[0]);
    transition(list.Get(),native.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list.Get(),raw_inputs[0],D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COPY_DEST);
    }
    // Synthetic metadata exercises transport. The independent live reviewer must
    // match its actual frame's jitter before this can be used in the game.
    mhw::dlss_sdk::HistoryFrame history{23122+receipt,23122+receipt,.1875F,.21875F};if(mode==L"bad-jitter")history.jitter_x=1;
    // The archived half-scale source had no injected jitter. Reset-only replay
    // checks SDK transport/output; it does not establish live temporal quality.
    if(upscale){history.frame=history.epoch=15673+receipt;history.jitter_x=history.jitter_y=0;}
    history.frame+=frame;history.epoch+=frame;
    const auto start=GetTickCount64();
    using mhw::dlss_sdk::RecordFault;
    const auto fault=(mode==L"aa-after-conversion"||mode==L"resolve-after-conversion"||mode==L"upscale-resolve-after-conversion")?RecordFault::after_conversion:
        (mode==L"aa-evaluate-result"||mode==L"resolve-evaluate-result"||mode==L"upscale-resolve-evaluate-result")?RecordFault::evaluate_result:
        (mode==L"aa-after-handoff"||mode==L"resolve-after-handoff"||mode==L"upscale-resolve-after-handoff")?RecordFault::after_handoff:RecordFault::none;
    bool recorded{};
    const auto evaluate=[&]{++sdk_attempts;recorded=continuous?
        pass.record_continuous(list.Get(),native.Get(),supplied,history,frame==0||frame==120,frame%2?16.F:17.F,frame+1==frame_count,fault):
        pass.record(list.Get(),mode==L"bad-output"?follow.Get():native.Get(),supplied,history);return recorded;};
    if(scaled_resolve){
        namespace sr=mhw::scaled_temporal;namespace cd=mhw::continuous_dlaa;
        if(!frame){
            auto invalid=supplied;invalid.invalidate();
            require(sr::record_resolve(pass,*recovery,list.Get(),native.Get(),invalid,history,true,17.F).outcome==cd::ResolveOutcome::rejected,"scaled invalid bindings");
            require(sr::record_resolve(pass,*recovery,list.Get(),follow.Get(),supplied,history,true,17.F).outcome==cd::ResolveOutcome::rejected,"scaled invalid output");
            require(sr::record_resolve(pass,*recovery,list.Get(),raw_inputs[0],supplied,history,true,17.F).outcome==cd::ResolveOutcome::rejected,"scaled aliased output");
            tp::Output wrong_geometry(device.Get(),tp::SpatialSource{w,h,w/4,h/4},w,h,true,retirement);
            require(sr::record_resolve(pass,wrong_geometry,list.Get(),native.Get(),supplied,history,true,17.F).outcome==cd::ResolveOutcome::rejected,"scaled geometry mismatch");
            require(!pass.last_attempt().commands&&tp::observer::matches_current(list.Get(),observed),"scaled preflight records no SDK/binding commands");
        }
        const auto transaction=cd::dispatch_resolve(true,[&]{
            if(!off_only)++sdk_attempts;
            return sr::record_resolve(pass,*recovery,list.Get(),native.Get(),supplied,history,frame==0,17.F,!off_only,frame+1==frame_count,fault);
        },[&]{++native_resolves;});
        const auto &result=transaction.resolve;
        recorded=result.outcome==cd::ResolveOutcome::reconstructed;
        const auto expected=success?cd::ResolveOutcome::reconstructed:off_only?cd::ResolveOutcome::unfiltered:cd::ResolveOutcome::recovered;
        require(transaction.claimed&&result.output_written()&&result.outcome==expected,"scaled resolve complete output");
        if(!recorded)++unfiltered_copies;
    }else if(native_resolve){
        namespace cd=mhw::continuous_dlaa;
        if(!frame){
            auto invalid=supplied;invalid.invalidate();
            const auto rejected=cd::record_resolve(pass,list.Get(),raw_inputs[0],native.Get(),w,h,invalid,history,true,17.F);
            require(rejected.outcome==cd::ResolveOutcome::rejected&&!rejected.output_written()&&!pass.last_attempt().commands,"incomplete bindings reject before SDK commands");
            const auto wrong_output=cd::record_resolve(pass,list.Get(),raw_inputs[0],follow.Get(),w,h,supplied,history,true,17.F);
            require(wrong_output.outcome==cd::ResolveOutcome::rejected&&!pass.last_attempt().commands,"wrong output format rejects before SDK commands");
            const auto alias=cd::record_resolve(pass,list.Get(),native.Get(),native.Get(),w,h,supplied,history,true,17.F);
            require(alias.outcome==cd::ResolveOutcome::rejected&&!pass.last_attempt().commands,"aliased recovery source rejected before SDK commands");
            require(tp::observer::matches_current(list.Get(),observed),"rejected resolve preserves bindings");
        }
        const auto result=cd::dispatch_resolve(true,[&]{
            if(!off_only)++sdk_attempts;
            return cd::record_resolve(pass,list.Get(),raw_inputs[0],native.Get(),w,h,supplied,history,
                frame==0||frame==120,frame%2?16.F:17.F,!off_only,frame+1==frame_count,fault);
        },[&]{++native_resolves;});
        recorded=result.resolve.outcome==cd::ResolveOutcome::reconstructed;
        require(result.claimed&&result.resolve.output_written(),"native resolve transaction writes full output");
        if(!recorded)++unfiltered_copies;
        const auto expected=success?cd::ResolveOutcome::reconstructed:off_only?cd::ResolveOutcome::unfiltered:cd::ResolveOutcome::recovered;
        require(result.resolve.outcome==expected,"native resolve outcome");
    }else if(exclusive){
        const auto rejected_copy=[&](ID3D12Resource *src,ID3D12Resource *dst,unsigned width,D3D12_RESOURCE_STATES state){
            require(!tp::copy_unfiltered(list.Get(),src,dst,width,h,state),"reject invalid AA-Off copy");};
        if(!frame){
            rejected_copy(nullptr,native.Get(),w,D3D12_RESOURCE_STATE_COPY_DEST);
            rejected_copy(native.Get(),native.Get(),w,D3D12_RESOURCE_STATE_COPY_DEST);
            rejected_copy(raw_inputs[0],follow.Get(),w,D3D12_RESOURCE_STATE_COPY_DEST);
            rejected_copy(raw_inputs[0],native.Get(),w-1,D3D12_RESOURCE_STATE_COPY_DEST);
            rejected_copy(raw_inputs[0],native.Get(),w,D3D12_RESOURCE_STATE_COMMON);
        }
        namespace aa=mhw::aa_control;
        const aa::Decision plan{off_only?aa::Path::unfiltered:aa::Path::reconstruct};
        require(aa::route(plan,evaluate,[&]{++unfiltered_copies;const auto attempt=pass.last_attempt();
            require(!attempt.commands||attempt.bindings_restored,"failure must restore bindings before AA-Off handoff");
            const auto source_state=mode==L"aa-off-srv"?D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE:mode==L"aa-off-all-srv"?
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:D3D12_RESOURCE_STATE_COPY_DEST;
            if(source_state!=D3D12_RESOURCE_STATE_COPY_DEST)transition(list.Get(),raw_inputs[0],D3D12_RESOURCE_STATE_COPY_DEST,source_state);
            const bool copied=tp::copy_unfiltered(list.Get(),raw_inputs[0],native.Get(),w,h,source_state);
            if(source_state!=D3D12_RESOURCE_STATE_COPY_DEST)transition(list.Get(),raw_inputs[0],source_state,D3D12_RESOURCE_STATE_COPY_DEST);
            return copied;
        },[&]{++native_resolves;}),"exclusive AA output");
    }else evaluate();
    record_ms+=GetTickCount64()-start;
    {std::ofstream f(output/L"record.json");f<<std::boolalpha;pass.write(f);}
    require(recorded==success,"inline result");require(!pass.record(list.Get(),native.Get(),observed,history),"inline one-shot");
    
    tp::Bindings after;require(tp::observer::current(list.Get(),after)&&after==observed,"restored API binding equality");
    rejects([&]{pass.collect(fence.Get(),frame+1,true);});
    transition(list.Get(),native.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);native_pixels.copy(list.Get(),native.Get());
    transition(list.Get(),native.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    list->Dispatch((w+15)/16,(h+15)/16,1); // No root/heap/PSO rebind: restored compute state must work.
    transition(list.Get(),follow.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);follow_pixels.copy(list.Get(),follow.Get());
    transition(list.Get(),native.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    list->SetPipelineState(graphic_pipeline.Get());list->DrawInstanced(3,1,0,0); // Graphics tables and constants must survive too.
    transition(list.Get(),graphics.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);graphics_pixels.copy(list.Get(),graphics.Get());
    hr(list->Close(),"close");ID3D12CommandList *lists[]{list.Get()};
    auto *submission_queue=asynchronous&&(frame%2)?second_queue.Get():queue.Get();
    if(asynchronous&&frame)hr(submission_queue->Wait(fence.Get(),frame),"cross-queue order");
    submission_queue->ExecuteCommandLists(1,lists);hr(submission_queue->Signal(fence.Get(),frame+1),"signal");
#ifdef MHW_REFLEX_TEMPORAL_FIXTURE
    reflex->submitted();
#endif
    if(retirement)require(recording_pins.submitted(list.Get(),frame+1,frame+1),"SDK recording submission");
    // Three recordings in flight; alternate queues with explicit GPU waits.
    const auto wait_value=(!asynchronous||frame+1==frame_count)?frame+1:(frame>=2?frame-1:0);
    if(wait_value){
        const auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(event!=nullptr,"event");
        hr(fence->SetEventOnCompletion(wait_value,event),"completion");
        require(WaitForSingleObject(event,15000)==WAIT_OBJECT_0&&fence->GetCompletedValue()>=wait_value&&fence->GetCompletedValue()!=UINT64_MAX,"complete");CloseHandle(event);
    }
    hr(device->GetDeviceRemovedReason(),"device removed");rejects([&]{pass.collect(fence.Get(),frame+1,false);});
    if(scaled_resolve&&!success)recovery_frames.push_back(native_pixels.read());
    if(asynchronous){allocator=allocator_pool[(frame+1)%3];list=list_pool[(frame+1)%3];}
    hr(allocator->Reset(),"retire allocator");hr(list->Reset(allocator.Get(),nullptr),"retire list");
    if(retirement){require(recording_pins.reset(list.Get(),pool_generation[(frame+1)%3]),"SDK recording reset");recording_pins.collect(fence->GetCompletedValue());}
    if(frame+1<frame_count){
        if(!(retirement&&(frame+1)%10==0))transition(list.Get(),native.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        transition(list.Get(),follow.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        transition(list.Get(),graphics.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
    }else hr(list->Close(),"retired close");
    }
    if(asynchronous)for(unsigned i=0;i<3;++i)if(list_pool[i].Get()!=list.Get()){
        hr(allocator_pool[i]->Reset(),"final pool allocator");hr(list_pool[i]->Reset(allocator_pool[i].Get(),nullptr),"final pool reset");hr(list_pool[i]->Close(),"final pool close");
        if(retirement)require(recording_pins.reset(list_pool[i].Get(),pool_generation[i]),"final recording retirement");}
    if(retirement){recording_pins.collect(fence->GetCompletedValue());require(!recording_pins.live()&&recording_pins.released()==frame_count,"all SDK recordings released");}
    if(recovery&&recovery->recorded())recovery->retire(fence.Get(),frame_count,true);
    auto a=native_pixels.read(),b=follow_pixels.read(),c=graphics_pixels.read();
    std::array<std::vector<std::byte>,5> result;
    if(success){result=pass.collect(fence.Get(),frame_count,true);require(result[4]==a,"native output after inline handoff");
        const wchar_t *names[]{L"color.f32",L"depth.f32",L"motion.f32",L"dlaa.rgba32f",L"native.u32"};
        for(unsigned i=0;i<5;++i){std::ofstream f(output/names[i],std::ios::binary);f.write(reinterpret_cast<const char *>(result[i].data()),static_cast<std::streamsize>(result[i].size()));require(bool(f),"save pixels");}
        const auto independent=raw.convert();const std::span<const float> expected[]{independent.color,independent.depth,independent.motion};
        if(!upscale){for(unsigned i=0;i<3;++i)require(result[i].size()==expected[i].size_bytes()&&!std::memcmp(result[i].data(),expected[i].data(),result[i].size()),"independent conversion equality");}
        else {constexpr unsigned channels[]{4,1,2};
            for(unsigned i=0;i<3;++i){require(result[i].size()==SIZE_T(geometry.render.width)*geometry.render.height*channels[i]*sizeof(float),"active conversion size");
                for(unsigned y=0;y<geometry.render.height;++y)for(unsigned x=0;x<geometry.render.width;++x)for(unsigned component=0;component<channels[i];++component){
                    float actual{};const auto offset=(SIZE_T(y)*geometry.render.width+x)*channels[i]+component;
                    std::memcpy(&actual,result[i].data()+offset*sizeof(float),sizeof(float));
                    const float reference=expected[i][(SIZE_T(y)*w+x)*channels[i]+component]*(i==2?.5F:1.F);
                    require(std::isfinite(actual)&&actual==reference,"active crop / render-pixel motion equality");
                }
            }
        }
    }else if(scaled_resolve){
        const auto reference=raw.convert();
        require(recovery_frames.size()==frame_count,"all recovery frames retained");
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){
            const auto px=std::clamp((float(x)+.5F)*.5F-.5F,0.F,float(w/2-1));
            const auto py=std::clamp((float(y)+.5F)*.5F-.5F,0.F,float(h/2-1));
            const auto x0=static_cast<unsigned>(px),y0=static_cast<unsigned>(py),x1=std::min(x0+1,w/2-1),y1=std::min(y0+1,h/2-1);
            std::array<float,4> rgb{};
            for(unsigned j=0;j<3;++j){
                const auto at=[&](unsigned xx,unsigned yy){return reference.color[(SIZE_T(yy)*w+xx)*4+j];};
                const float upper=at(x0,y0)+(at(x1,y0)-at(x0,y0))*(px-float(x0));
                const float lower=at(x0,y1)+(at(x1,y1)-at(x0,y1))*(px-float(x0));
                rgb[j]=upper+(lower-upper)*(py-float(y0));
            }
            for(const auto &pixels:recovery_frames){unsigned actual{};std::memcpy(&actual,pixels.data()+(SIZE_T(y)*w+x)*4,4);
                require(actual==packed(rgb.data()),"scaled recovery independent full-output pixels");}
        }
        for(unsigned i=0;i<recovery_frames.size();++i){const auto &pixels=recovery_frames[i];
            std::ofstream f(output/(L"recovery-"+std::to_wstring(i)+L".u32"),std::ios::binary);
            f.write(reinterpret_cast<const char *>(pixels.data()),static_cast<std::streamsize>(pixels.size()));require(bool(f),"save recovery frame");}
        std::ofstream f(output/L"recovery.u32",std::ios::binary);f.write(reinterpret_cast<const char *>(a.data()),static_cast<std::streamsize>(a.size()));require(bool(f),"save recovery pixels");
    }else {std::ifstream f(source/(prefix+L".color-"+std::to_wstring(receipt)+L".u32"),std::ios::binary);std::vector<std::byte> original(a.size());
        require(bool(f.read(reinterpret_cast<char *>(original.data()),static_cast<std::streamsize>(original.size())))&&original==a,"rejected path preserves native output");}
    for(std::size_t i=0;i<std::size_t(w)*h;++i){unsigned actual{};std::memcpy(&actual,a.data()+i*4,4);
        if(success){std::array<float,4> rgba{};std::memcpy(rgba.data(),result[3].data()+i*16,16);
            for(unsigned j=0;j<3;++j)require(std::isfinite(rgba[j])&&rgba[j]!=65504,"complete finite SDK output");
            require(actual==packed(rgba.data()),"packed SDK reference");}
        const float native_rgb[]{unpack(actual&2047,6),unpack((actual>>11)&2047,6),unpack(actual>>22,5)};
        std::array<float,4> compute{},draw{};std::memcpy(compute.data(),b.data()+i*16,16);std::memcpy(draw.data(),c.data()+i*16,16);
        require(compute==std::array<float,4>{native_rgb[0]+3,native_rgb[1],native_rgb[2],11},"downstream compute bindings/pixels");
        require(draw==std::array<float,4>{native_rgb[0]+7,native_rgb[1],native_rgb[2],11},"downstream graphics bindings/pixels");
    }
    unsigned warnings{},errors_count{};
    for(UINT64 i=0;i<messages->GetNumStoredMessagesAllowedByRetrievalFilter();++i){SIZE_T size{};hr(messages->GetMessage(i,nullptr,&size),"message size");std::vector<std::byte> bytes(size);auto *m=reinterpret_cast<D3D12_MESSAGE *>(bytes.data());hr(messages->GetMessage(i,m,&size),"message");
        if(m->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)++errors_count;if(m->Severity==D3D12_MESSAGE_SEVERITY_WARNING)++warnings;
        if(m->Severity<=D3D12_MESSAGE_SEVERITY_WARNING&&warnings+errors_count<=16)std::cerr<<m->pDescription<<'\n';}
    require(!warnings&&!errors_count,"GPU validation diagnostics");
    if(exclusive)require(!native_resolves&&unfiltered_copies==(success?0:frame_count)&&sdk_attempts==(off_only?0:frame_count),"exclusive AA route counts");
    if(fixture_argc==13){
        require(scaled_resolve,"native-effect integration requires scaled resolve");
        namespace ef=mhw::fixtures::effects;
        // SDK work is already fenced and retired above. Feed the actual native
        // packed GPU resource directly into both effect routes. No readback is
        // used as an input, and every inter-stage transfer stays on the GPU.
        ef::Fixture effects(false,{low,high},argv[12],device.Get());
        effects.track_raster=true;
        ComPtr<ID3D12Resource> actual_source=native,reference_source=native;
        UINT64 compared{};const mhw::scaled_temporal::FilterGeometry effect_geometry{w/2,h/2,w,h};
        for(unsigned stage=0;stage<3;++stage){
            const auto input_state=stage?D3D12_RESOURCE_STATE_COPY_SOURCE:D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            const auto actual=effects.draw(stage,effect_geometry,1,true,false,{},actual_source.Get(),input_state);actual_source=effects.last_output;
            const auto expected=effects.draw(stage,effect_geometry,1,false,false,{},reference_source.Get(),input_state);reference_source=effects.last_output;
            require(actual==expected,"SDK native-effect chain exact full output");compared+=actual.size();
            if(stage==2){std::ofstream pixels(output/L"native-effect-chain.u32",std::ios::binary);pixels.write(reinterpret_cast<const char *>(actual.data()),static_cast<std::streamsize>(actual.size()*4));require(bool(pixels),"native-effect output save");}
        }
        effects.messages();
        std::ofstream report(output/L"native-effects.json");report<<"{\"accepted\":true,\"direct_gpu_source\":true,\"full_output_copy\":true,\"stages\":3,\"draws\":6,\"packed_pixels_compared\":"<<compared<<",\"exact\":true,\"gpu_warnings_errors\":0,\"recordings_retired\":true,\"synthetic_effect_constants\":true,\"live_game_verified\":false}\n";require(bool(report),"effect report");
    }

    require(tp::observer::finish(),"state closure");
#ifdef MHW_REFLEX_TEMPORAL_FIXTURE
    reflex->finish(sdk_attempts);
#endif
    if(upscale&&!scaled_resolve)require(!native_resolves&&!unfiltered_copies&&sdk_attempts==1,"upscale output written only by SDK handoff");
    std::ofstream out(output/L"fixture.json");out<<std::boolalpha<<"{\"accepted\":true,\"upscale\":"<<upscale<<",\"scaled_resolve\":"<<scaled_resolve<<",\"recovery_frames_checked\":"<<recovery_frames.size()<<",\"recovery_retired\":"<<(recovery&&recovery->recorded()&&recovery->retired())<<",\"poisoned_padding\":"<<(mode==L"upscale-poison"||scaled_resolve)<<",\"rejected_case\":"<<!success<<",\"width\":"<<w<<",\"height\":"<<h<<",\"pixels\":"<<UINT64(w)*h
        <<",\"bindings_equal\":true,\"downstream_compute_exact\":true,\"downstream_graphics_exact\":true,\"gpu_validation\":true,\"debug_errors\":0,\"debug_warnings\":0,\"game_process_access\":false,\"exclusive_aa\":"<<exclusive<<",\"native_resolves\":"<<native_resolves<<",\"unfiltered_copies\":"<<unfiltered_copies<<",\"sdk_attempts\":"<<sdk_attempts<<",\"record_ms\":"<<record_ms<<",\"native_allocations\":"<<native_allocations<<",\"recordings\":";recording_pins.write(out,fence->GetCompletedValue());out<<",\"inline\":";pass.write(out);out<<"}\n";require(bool(out),"save result");
    std::ofstream observer(output/L"bindings.json");observer<<std::boolalpha;tp::observer::write(observer);
    std::cout<<"Inline fixture accepted: "<<UINT64(w)*h<<" pixels, record "<<record_ms<<" ms, downstream bindings exact, clean GPU validation\n";
}
}
int wmain(int argc,wchar_t **argv){try{run(argc,argv);return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
