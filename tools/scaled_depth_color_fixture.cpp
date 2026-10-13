// Private captured shaders are supplied by path. No game process is opened and
// no proprietary shader is part of this source or the resulting executable.
//
// Oracle: the UNMODIFIED native depth-color PS at full output with inputs that
// are consistent at that size (scene color in the upper-left quarter of a
// double-size allocation so its render-space UVs address it, depth upsampled
// 2x2 by nearest, CBScreen for the full output) must equal the adapted PS fed
// what the game binds after DLSS (full-output color, render-size depth in the
// upper-left of its allocation, render CBScreen). Exact float equality.
#include "../src/scaled_temporal/depth_color_shader.hpp"
#include "../src/scaled_temporal/effects.hpp"
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <d3d11shader.h>
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
#include <span>
#include <stdexcept>
#include <string>
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
void reflection(std::span<const std::byte> a,std::span<const std::byte> b){
    ComPtr<ID3D11ShaderReflection> x,y;hr(D3DReflect(a.data(),a.size(),IID_PPV_ARGS(&x)),"native reflection");hr(D3DReflect(b.data(),b.size(),IID_PPV_ARGS(&y)),"adapted reflection");
    D3D11_SHADER_DESC xd{},yd{};hr(x->GetDesc(&xd),"native desc");hr(y->GetDesc(&yd),"adapted desc");
    require(xd.BoundResources==yd.BoundResources&&xd.ConstantBuffers==yd.ConstantBuffers&&xd.InputParameters==yd.InputParameters&&xd.OutputParameters==yd.OutputParameters,"reflection layout");
    for(unsigned i=0;i<xd.BoundResources;++i){D3D11_SHADER_INPUT_BIND_DESC p{},q{};hr(x->GetResourceBindingDesc(i,&p),"resource reflection");hr(y->GetResourceBindingDesc(i,&q),"adapted resource reflection");
        require(std::strcmp(p.Name,q.Name)==0&&p.Type==q.Type&&p.BindPoint==q.BindPoint&&p.BindCount==q.BindCount&&p.Dimension==q.Dimension&&p.ReturnType==q.ReturnType,"resource layout unchanged");}
}
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
    Pixels(ID3D12Device *d,const D3D12_RESOURCE_DESC &desc,D3D12_HEAP_TYPE type):height(desc.Height){
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
// Scene color and render-space depth: distances from 16/0.9 to 16/0.01 cross
// the configured start/end so every curve branch (before, inside, past) runs.
std::array<float,4> scene(unsigned x,unsigned y){
    return {float((x*17+y*11)%29+1)*.125F,float((x*3+y*7)%13+1)*.25F,float((x+y*5)%17+1)*.5F,1};}
float depth(unsigned x,unsigned y){return .01F+.89F*float((x*5+y*3)%23)/22.F;}
enum class Run {native,adapted,oracle};
struct Fixture {
    ComPtr<ID3D12Device> device;ComPtr<ID3D12InfoQueue> messages;ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12RootSignature> root;std::vector<std::byte> vertex_shader;UINT64 sequence{};LUID actual_luid{};unsigned vendor{},device_id{};
    Fixture(bool warp,LUID luid,std::vector<std::byte> vs):vertex_shader(std::move(vs)){
        ComPtr<ID3D12Debug> debug;hr(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)),"debug");debug->EnableDebugLayer();
        ComPtr<ID3D12Debug1> validation;hr(debug.As(&validation),"GPU validation");validation->SetEnableGPUBasedValidation(TRUE);
        ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter1> adapter;hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"factory");
        if(warp)hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)),"warp");
        else hr(factory->EnumAdapterByLuid(luid,IID_PPV_ARGS(&adapter)),"adapter");
        DXGI_ADAPTER_DESC1 adapter_desc{};hr(adapter->GetDesc1(&adapter_desc),"adapter description");actual_luid=adapter_desc.AdapterLuid;vendor=adapter_desc.VendorId;device_id=adapter_desc.DeviceId;
        hr(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)),"device");hr(device.As(&messages),"messages");
        D3D12_COMMAND_QUEUE_DESC q{};hr(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)),"queue");
        hr(device->CreateCommandAllocator(q.Type,IID_PPV_ARGS(&allocator)),"allocator");hr(device->CreateCommandList(0,q.Type,allocator.Get(),nullptr,IID_PPV_ARGS(&list)),"list");hr(list->Close(),"initial close");hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"fence");
        // b0 CBViewProjection, b1 CBScreen, b3 CBDepthColor, t0 scene, t1 depth, s0 SSFilter.
        D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,2,0,0,0};std::array<D3D12_ROOT_PARAMETER,4> parameters{};
        for(unsigned i=0;i<3;++i){parameters[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;parameters[i].Descriptor.ShaderRegister=i==2?3:i;parameters[i].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
        parameters[3].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[3].DescriptorTable={1,&range};parameters[3].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_NEVER;sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=4;rd.pParameters=parameters.data();rd.NumStaticSamplers=1;rd.pStaticSamplers=&sampler;
        rd.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        ComPtr<ID3DBlob> serial,error;hr(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&serial,&error),"root serialize");hr(device->CreateRootSignature(0,serial->GetBufferPointer(),serial->GetBufferSize(),IID_PPV_ARGS(&root)),"root");
    }
    std::vector<float> draw(std::span<const std::byte> shader,sf::FilterGeometry g,Run run,unsigned mode){
        const unsigned w=g.output_width,h=g.output_height,rw=g.render_width,rh=g.render_height;const float nan=std::numeric_limits<float>::quiet_NaN();
        const bool oracle=run==Run::oracle;
        hr(allocator->Reset(),"allocator reset");hr(list->Reset(allocator.Get(),nullptr),"list reset");
        D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=2;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ComPtr<ID3D12DescriptorHeap> heap,rtv;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)),"SRV heap");
        hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=1;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv)),"RTV heap");
        std::array<ComPtr<ID3D12Resource>,2> inputs;std::vector<Pixels> uploads;uploads.reserve(2);
        const std::array<DXGI_FORMAT,2> formats{DXGI_FORMAT_R32G32B32A32_FLOAT,DXGI_FORMAT_R32_FLOAT};
        const unsigned channels[]{4,1};const auto step=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        for(unsigned t=0;t<2;++t){
            // Native: the frame at render size in the upper-left of the output
            // allocation. Adapted: color fills the output (DLSS), depth stays.
            // Oracle: color at output size in the upper-left of a double-size
            // allocation (clamped edge copy beyond it); depth upsampled 2x2.
            const auto tw=t==0&&oracle&&rw!=w?2*w:w,th=t==0&&oracle&&rh!=h?2*h:h;
            const auto desc=texture(tw,th,formats[t]);
            inputs[t]=resource(device.Get(),desc,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
            uploads.emplace_back(device.Get(),desc,D3D12_HEAP_TYPE_UPLOAD);std::vector<float> data(std::size_t(tw)*th*channels[t],nan);
            for(unsigned y=0;y<th;++y)for(unsigned x=0;x<tw;++x){const auto at=(std::size_t(y)*tw+x)*channels[t];
                if(t==0){
                    if(run==Run::native&&(x>=rw||y>=rh))continue;
                    const auto c=scene(std::min(x,w-1),std::min(y,h-1));std::copy(c.begin(),c.end(),data.begin()+at);continue;}
                if(oracle){data[at]=depth(x*rw/w,y*rh/h);continue;}
                if(x<rw&&y<rh)data[at]=depth(x,y);
            }
            uploads.back().write(std::as_bytes(std::span(data)));uploads.back().copy(list.Get(),inputs[t].Get(),true);
            barrier(list.Get(),inputs[t].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            D3D12_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=formats[t];sd.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sd.Texture2D.MipLevels=1;
            device->CreateShaderResourceView(inputs[t].Get(),&sd,{heap->GetCPUDescriptorHandleForHeapStart().ptr+SIZE_T(t)*step});
        }
        const unsigned vw=run==Run::native?rw:w,vh=run==Run::native?rh:h;
        auto desc=texture(w,h,DXGI_FORMAT_R32G32B32A32_FLOAT);desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        auto output=resource(device.Get(),desc,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_RENDER_TARGET);device->CreateRenderTargetView(output.Get(),nullptr,rtv->GetCPUDescriptorHandleForHeapStart());
        Pixels pixels(device.Get(),desc,D3D12_HEAP_TYPE_READBACK);
        // b0 at 0 (20 registers), b1 at 512 (5 registers), b3 at 768 (259 registers).
        std::vector<float> constants((768+259*16)/4,0.F);
        // fProjI rows 16..19 (view z/w): z=-1-0.01x+0.02y, w=d/16+0.001x+0.0005y,
        // so the ray's screen position matters and distance = -z/w.
        auto c=[&](unsigned reg,unsigned k)->float &{return constants[reg*4+k];};
        c(16,2)=-.01F;c(16,3)=.001F;c(17,2)=.02F;c(17,3)=.0005F;c(18,2)=0;c(18,3)=1.F/16;c(19,2)=-1;c(19,3)=0;
        // CBScreen fViewOffset (cb1[3].xy) and fViewInverseSize (cb1[4].xy).
        const auto screen=[&](unsigned reg,unsigned k)->float &{return constants[128+reg*4+k];};
        screen(3,0)=0;screen(3,1)=0;screen(4,0)=1.F/(oracle?w:rw);screen(4,1)=1.F/(oracle?h:rh);
        const auto depth_color=[&](unsigned reg,unsigned k)->float &{return constants[192+reg*4+k];};
        std::memcpy(&depth_color(0,0),&mode,4);depth_color(0,1)=.75F;
        depth_color(1,0)=.3F;depth_color(1,1)=.5F;depth_color(1,2)=.9F;depth_color(1,3)=20;depth_color(2,0)=800;
        for(unsigned i=0;i<256;++i)depth_color(3+i,0)=i%7==3?0.F:float((i*37)%256)/255.F;
        auto cb=buffer(device.Get(),constants.size()*4,D3D12_HEAP_TYPE_UPLOAD);void *p{};D3D12_RANGE none{};hr(cb->Map(0,&none,&p),"CB map");std::memcpy(p,constants.data(),constants.size()*4);cb->Unmap(0,nullptr);
        // The production native and full-output vertices (the native triangle;
        // depth color keeps render-space UVs like haze).
        const auto geometry=sf::effect_vertices(sf::EffectStage::screen_filter,g,run!=Run::native);
        auto vertices=buffer(device.Get(),48,D3D12_HEAP_TYPE_UPLOAD);
        {void *mapped{};D3D12_RANGE empty{};hr(vertices->Map(0,&empty,&mapped),"vertices");std::memcpy(mapped,geometry.values.data(),48);vertices->Unmap(0,nullptr);}
        static constexpr D3D12_INPUT_ELEMENT_DESC elements[]{
         {"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
         {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.VS={vertex_shader.data(),vertex_shader.size()};pd.PS={shader.data(),shader.size()};
        pd.InputLayout={elements,2};
        pd.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;pd.SampleMask=UINT_MAX;
        pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
        pd.DepthStencilState.DepthEnable=FALSE;pd.DepthStencilState.StencilEnable=FALSE;pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pd.NumRenderTargets=1;pd.RTVFormats[0]=desc.Format;pd.SampleDesc.Count=1;ComPtr<ID3D12PipelineState> pipeline;
        const auto pipeline_hr=device->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&pipeline));
        if(FAILED(pipeline_hr))check_messages();hr(pipeline_hr,"depth color pipeline");
        ID3D12DescriptorHeap *heaps[]{heap.Get()};list->SetDescriptorHeaps(1,heaps);list->SetGraphicsRootSignature(root.Get());list->SetPipelineState(pipeline.Get());
        const unsigned cb_offsets[]{0,512,768};for(unsigned i=0;i<3;++i)list->SetGraphicsRootConstantBufferView(i,cb->GetGPUVirtualAddress()+cb_offsets[i]);
        list->SetGraphicsRootDescriptorTable(3,heap->GetGPUDescriptorHandleForHeapStart());
        D3D12_VIEWPORT viewport{0,0,float(vw),float(vh),0,1};D3D12_RECT rect{0,0,static_cast<LONG>(vw),static_cast<LONG>(vh)};
        list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&rect);const auto handle=rtv->GetCPUDescriptorHandleForHeapStart();
        list->OMSetRenderTargets(1,&handle,FALSE,nullptr);const float poison[]{-12345,-12345,-12345,-12345};list->ClearRenderTargetView(handle,poison,0,nullptr);
        const D3D12_VERTEX_BUFFER_VIEW vb{vertices->GetGPUVirtualAddress(),48,16};list->IASetVertexBuffers(0,1,&vb);
        list->IASetPrimitiveTopology(geometry.topology);list->DrawInstanced(geometry.count,1,0,0);
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
    const bool warp=argc==5&&std::wstring_view(argv[1])==L"--warp";
    require(warp||(argc==7&&std::wstring_view(argv[1])==L"--hardware"),"usage: --warp ps.dxbc vs.dxbc output-dir OR --hardware luid-low luid-high ps.dxbc vs.dxbc output-dir");
    const auto ps_path=std::filesystem::path(argv[warp?2:4]),vs_path=std::filesystem::path(argv[warp?3:5]),out=std::filesystem::path(argv[warp?4:6]);
    require(!std::filesystem::exists(out),"preserve results");std::filesystem::create_directories(out);
    const auto native=read(ps_path);const sf::FilterGeometry identity{64,32,64,32},scaled{64,32,128,64};
    const auto one=sf::make_depth_color_shader(native,identity),two=sf::make_depth_color_shader(native,scaled);
    reflection(native,one);reflection(native,two);save(out/"scaled.dxbc",two);disassemble(out/"scaled.asm",two);
    unsigned rejections=0;
    for(const auto g:std::array<sf::FilterGeometry,6>{{{0,32,64,64},{64,32,128,32},{64,32,96,48},{UINT_MAX,32,128,64},{64,32,32,16},{8192,8192,16385,16385}}}){
        rejects([&]{sf::make_depth_color_shader(native,g);});++rejections;}
    for(const auto offset:std::array<std::size_t,6>{0,4,24,28,3000,5567}){auto changed=native;changed[offset]^=std::byte{1};rejects([&]{sf::make_depth_color_shader(changed,scaled);});++rejections;}
    rejects([&]{sf::make_depth_color_shader(std::span(native).first(native.size()-1),scaled);});++rejections;
    LUID luid{};if(!warp){luid.LowPart=static_cast<DWORD>(std::stoul(argv[2]));luid.HighPart=static_cast<LONG>(std::stol(argv[3]));}
    Fixture f(warp,luid,read(vs_path));require(f.vertex_shader.size()==752,"captured depth color VS");
    unsigned comparisons=0,draws=0,tinted=0;std::ofstream report(out/"cases.jsonl");
    for(unsigned mode=0;mode<5;++mode){
        // Identity: the adapter changes nothing at 1x.
        const auto expected=f.draw(native,identity,Run::native,mode),actual=f.draw(one,identity,Run::adapted,mode);draws+=2;
        require(expected.size()==actual.size(),"identity lengths");
        for(std::size_t i=0;i<actual.size();++i){require(std::isfinite(actual[i])&&expected[i]==actual[i],"identity native comparison");++comparisons;}
        const auto reference=f.draw(native,scaled,Run::oracle,mode),pixels=f.draw(two,scaled,Run::adapted,mode);draws+=2;
        require(reference.size()==pixels.size(),"scaled lengths");
        for(std::size_t i=0;i<pixels.size();++i){
            if(!(std::isfinite(pixels[i])&&pixels[i]==reference[i]))std::cerr<<"mode="<<mode<<" index="<<i<<" actual="<<pixels[i]<<" oracle="<<reference[i]<<'\n';
            require(std::isfinite(pixels[i])&&pixels[i]==reference[i],"scaled native oracle");++comparisons;}
        // The tint must actually apply somewhere (the oracle is not vacuous).
        unsigned changed{};for(unsigned y=0;y<scaled.output_height;++y)for(unsigned x=0;x<scaled.output_width;++x){
            const auto s=scene(x,y);const auto *o=pixels.data()+(std::size_t(y)*scaled.output_width+x)*4;
            if(o[0]!=s[0]||o[1]!=s[1]||o[2]!=s[2])++changed;}
        require(mode==4?changed==0:changed>0,"tint coverage");tinted+=changed;
        report<<"{\"mode\":"<<mode<<",\"identity_exact\":true,\"scaled_exact\":true,\"tinted_pixels\":"<<changed<<"}\n";
        save(out/("mode-"+std::to_string(mode)+".rgba32f"),std::as_bytes(std::span(pixels)));
    }
    f.check_messages();
    std::ofstream summary(out/"result.json");summary<<"{\"passed\":true,\"warp\":"<<(warp?"true":"false")<<",\"adapter_luid\":["<<f.actual_luid.HighPart<<','<<f.actual_luid.LowPart<<"],\"vendor\":"<<f.vendor<<",\"device\":"<<f.device_id<<",\"draws\":"<<draws<<",\"rejections\":"<<rejections<<",\"float_comparisons\":"<<comparisons<<",\"tinted_pixels\":"<<tinted<<",\"gpu_warnings_errors\":0,\"reflection_unchanged\":true}\n";
    require(bool(summary),"result write");std::cout<<"PASS: "<<draws<<" draws; "<<comparisons<<" float comparisons; "<<rejections<<" rejected inputs; "<<tinted<<" tinted pixels\n";
}
}
int wmain(int argc,wchar_t **argv){try{run(argc,argv);return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
