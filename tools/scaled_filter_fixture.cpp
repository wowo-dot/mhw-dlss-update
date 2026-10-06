// Private captured shader is supplied by path. No game process is opened and
// no proprietary shader is part of this source or the resulting executable.
#include "../src/scaled_temporal/filter_shader.hpp"
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
void reflection(std::span<const std::byte> a,std::span<const std::byte> b){
    ComPtr<ID3D11ShaderReflection> x,y;hr(D3DReflect(a.data(),a.size(),IID_PPV_ARGS(&x)),"native reflection");hr(D3DReflect(b.data(),b.size(),IID_PPV_ARGS(&y)),"adapted reflection");
    D3D11_SHADER_DESC xd{},yd{};hr(x->GetDesc(&xd),"native desc");hr(y->GetDesc(&yd),"adapted desc");
    require(xd.BoundResources==yd.BoundResources&&xd.ConstantBuffers==yd.ConstantBuffers&&xd.InputParameters==yd.InputParameters&&xd.OutputParameters==yd.OutputParameters,"reflection layout");
    for(unsigned i=0;i<xd.BoundResources;++i){D3D11_SHADER_INPUT_BIND_DESC p{},q{};hr(x->GetResourceBindingDesc(i,&p),"resource reflection");hr(y->GetResourceBindingDesc(i,&q),"adapted resource reflection");
        require(std::strcmp(p.Name,q.Name)==0&&p.Type==q.Type&&p.BindPoint==q.BindPoint&&p.BindCount==q.BindCount&&p.Dimension==q.Dimension&&p.ReturnType==q.ReturnType,"resource layout unchanged");}
}
// Independent texture-layout oracle: disable color-coordinate expansion, then
// place the same full-size image in the top-left quarter of a double-size t0.
// Hardware normalized sampling must address the same texels. Integer color
// loads still address the full raster pixel. This exercises varying color in
// both blur loops without reimplementing the native blur/depth equations.
std::vector<std::byte> expanded_texture_reference(std::span<const std::byte> shader){
    std::vector<unsigned> words(shader.size()/4);std::memcpy(words.data(),shader.data(),shader.size());unsigned edits=0;
    for(unsigned i=0;i<words[7];++i){const auto chunk=words[8+i]/4;if(words[chunk]!=0x58454853)continue;
        const auto end=chunk+2+words[chunk+1]/4;
        for(auto at=chunk+4;at<end;){const auto n=(words[at]>>24)&127;require(n&&at+n<=end,"reference instruction length");
            if(n==10&&words[at]==0x0a000038&&words[at+1]==0x00100032&&words[at+2]==11){
                require(words[at+5]==0x00004002&&words[at+6]==0x40000000&&words[at+7]==0x40000000,"reference color expansion");
                words[at+6]=words[at+7]=0x3f800000;++edits;}
            at+=n;}
    }
    require(edits==4,"reference four sampled reads");auto bytes=reinterpret_cast<BYTE *>(words.data());
    ComputeHashRetail(bytes+20,static_cast<UINT32>(shader.size()-20),bytes+4);
    std::vector<std::byte> result(shader.size());std::memcpy(result.data(),words.data(),result.size());return result;
}
std::array<float,4> color(unsigned x,unsigned y,unsigned w,bool constant){return constant?std::array<float,4>{2,.5F,8,1}:
    std::array<float,4>{float((x*17+y*11)%29+1)*.125F,float((x*3+y*7)%13+1)*.25F,float((x+y*5)%17+1)*.5F,float(y*w+x+1)};}
struct Fixture {
    ComPtr<ID3D12Device> device;ComPtr<ID3D12InfoQueue> messages;ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12RootSignature> root;ComPtr<ID3DBlob> vs;UINT64 sequence{};LUID actual_luid{};unsigned vendor{},device_id{};
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
        D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,5,0,0,0};std::array<D3D12_ROOT_PARAMETER,4> parameters{};
        for(unsigned i=0;i<3;++i){parameters[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;parameters[i].Descriptor.ShaderRegister=i==2?3:i;parameters[i].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
        parameters[3].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[3].DescriptorTable={1,&range};parameters[3].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_NEVER;sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=4;rd.pParameters=parameters.data();rd.NumStaticSamplers=1;rd.pStaticSamplers=&sampler;
        ComPtr<ID3DBlob> serial,error;hr(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&serial,&error),"root serialize");hr(device->CreateRootSignature(0,serial->GetBufferPointer(),serial->GetBufferSize(),IID_PPV_ARGS(&root)),"root");
        vs=compile("struct O {float4 p:SV_Position;float2 uv:TexCoord0;float2 uv1:TexCoord1;float4 c:Color;}; O main(uint i:SV_VertexID){O o;o.p=float4(i==2?3:-1,i==1?-3:1,0,1);o.uv=float2(i==2?2:0,i==1?2:0);o.uv1=0;o.c=0;return o;}");
    }
    std::vector<float> draw(std::span<const std::byte> shader,sf::FilterGeometry g,unsigned scenario,bool constant,bool expanded=false){
        const unsigned w=g.output_width,h=g.output_height;const float nan=std::numeric_limits<float>::quiet_NaN();
        hr(allocator->Reset(),"allocator reset");hr(list->Reset(allocator.Get(),nullptr),"list reset");
        D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=5;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ComPtr<ID3D12DescriptorHeap> heap,rtv;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)),"SRV heap");
        hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=1;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv)),"RTV heap");
        std::array<ComPtr<ID3D12Resource>,5> inputs;std::vector<Pixels> uploads;uploads.reserve(5);
        const std::array<DXGI_FORMAT,5> formats{DXGI_FORMAT_R32G32B32A32_FLOAT,DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R32_UINT,DXGI_FORMAT_R32G32_FLOAT,DXGI_FORMAT_R32G32B32A32_FLOAT};
        const unsigned channels[]{4,1,1,2,4};const auto step=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        for(unsigned t=0;t<5;++t){const auto tw=t==0&&expanded?2*w:w,th=t==0&&expanded?2*h:h;
            const auto desc=texture(tw,th,formats[t]);inputs[t]=resource(device.Get(),desc,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
            uploads.emplace_back(device.Get(),desc,D3D12_HEAP_TYPE_UPLOAD);std::vector<float> data(std::size_t(tw)*th*channels[t],nan);
            for(unsigned y=0;y<th;++y)for(unsigned x=0;x<tw;++x){const auto at=(std::size_t(y)*tw+x)*channels[t];
                if(t==0){if(x<w&&y<h){const auto c=color(x,y,w,constant);std::copy(c.begin(),c.end(),data.begin()+at);}continue;}
                if(x>=g.render_width||y>=g.render_height)continue;
                if(t==1)data[at]=.25F;
                if(t==2){const unsigned packed=(scenario==1||scenario==2)?1:0;std::memcpy(data.data()+at,&packed,4);}
                if(t==3){data[at]=scenario==2?.08F:0;data[at+1]=scenario==2?-.04F:0;}
                if(t==4){data[at]=scenario==3?1.F:0;data[at+1]=data[at+2]=data[at+3]=0;}
            }
            uploads.back().write(std::as_bytes(std::span(data)));uploads.back().copy(list.Get(),inputs[t].Get(),true);
            barrier(list.Get(),inputs[t].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            D3D12_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=formats[t];sd.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sd.Texture2D.MipLevels=1;
            device->CreateShaderResourceView(inputs[t].Get(),&sd,{heap->GetCPUDescriptorHandleForHeapStart().ptr+SIZE_T(t)*step});
        }
        auto desc=texture(w,h,DXGI_FORMAT_R32G32B32A32_FLOAT);desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        auto output=resource(device.Get(),desc,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_RENDER_TARGET);device->CreateRenderTargetView(output.Get(),nullptr,rtv->GetCPUDescriptorHandleForHeapStart());
        Pixels pixels(device.Get(),desc,D3D12_HEAP_TYPE_READBACK);
        std::array<float,192> constants{};constants[10*4+2]=1;constants[11*4+2]=1;
        auto screen=constants.data()+64;screen[4]=float(w);screen[5]=float(h);screen[6]=1.F/w;screen[7]=1.F/h;screen[14]=float(g.render_width);screen[15]=float(g.render_height);
        auto blur=constants.data()+128;const unsigned samples=8;std::memcpy(blur+2,&samples,4);blur[3]=1.F/samples;blur[4]=blur[5]=1;blur[6]=.01F;
        blur[8]=(g.render_width-.5F)/w;blur[9]=(g.render_height-.5F)/h;
        auto cb=buffer(device.Get(),constants.size()*4,D3D12_HEAP_TYPE_UPLOAD);void *p{};D3D12_RANGE none{};hr(cb->Map(0,&none,&p),"CB map");std::memcpy(p,constants.data(),constants.size()*4);cb->Unmap(0,nullptr);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.VS={vs->GetBufferPointer(),vs->GetBufferSize()};pd.PS={shader.data(),shader.size()};
        pd.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;pd.SampleMask=UINT_MAX;
        pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
        pd.DepthStencilState.DepthEnable=FALSE;pd.DepthStencilState.StencilEnable=FALSE;pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pd.NumRenderTargets=1;pd.RTVFormats[0]=desc.Format;pd.SampleDesc.Count=1;ComPtr<ID3D12PipelineState> pipeline;
        const auto pipeline_hr=device->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&pipeline));
        if(FAILED(pipeline_hr))check_messages();hr(pipeline_hr,"filter pipeline");
        ID3D12DescriptorHeap *heaps[]{heap.Get()};list->SetDescriptorHeaps(1,heaps);list->SetGraphicsRootSignature(root.Get());list->SetPipelineState(pipeline.Get());
        for(unsigned i=0;i<3;++i)list->SetGraphicsRootConstantBufferView(i,cb->GetGPUVirtualAddress()+UINT64(i)*256);list->SetGraphicsRootDescriptorTable(3,heap->GetGPUDescriptorHandleForHeapStart());
        D3D12_VIEWPORT viewport{0,0,float(w),float(h),0,1};D3D12_RECT rect{0,0,static_cast<LONG>(w),static_cast<LONG>(h)};
        list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&rect);const auto handle=rtv->GetCPUDescriptorHandleForHeapStart();
        list->OMSetRenderTargets(1,&handle,FALSE,nullptr);const float poison[]{-12345,-12345,-12345,-12345};list->ClearRenderTargetView(handle,poison,0,nullptr);
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->DrawInstanced(3,1,0,0);
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
    const bool warp=argc==4&&std::wstring_view(argv[1])==L"--warp";
    require(warp||(argc==6&&std::wstring_view(argv[1])==L"--hardware"),"usage: --warp shader.dxbc output-dir OR --hardware luid-low luid-high shader.dxbc output-dir");
    const auto shader_path=std::filesystem::path(argv[warp?2:4]),out=std::filesystem::path(argv[warp?3:5]);
    require(!std::filesystem::exists(out),"preserve results");std::filesystem::create_directories(out);
    const auto native=read(shader_path);const sf::FilterGeometry identity{64,32,64,32},scaled{64,32,128,64};
    auto one=sf::make_filter_shader(native,identity),two=sf::make_filter_shader(native,scaled),reference=expanded_texture_reference(two);
    reflection(native,one);reflection(native,two);save(out/"identity.dxbc",one);save(out/"scaled.dxbc",two);disassemble(out/"scaled.asm",two);
    save(out/"expanded-texture-reference.dxbc",reference);disassemble(out/"expanded-texture-reference.asm",reference);
    unsigned rejections=0;
    for(const auto g:std::array<sf::FilterGeometry,6>{{{0,32,64,64},{64,32,128,32},{64,32,96,48},{UINT_MAX,32,128,64},{64,32,32,16},{4096,4096,8192,8192}}}){
        rejects([&]{sf::make_filter_shader(native,g);});++rejections;}
    for(const auto offset:std::array<std::size_t,6>{0,4,24,28,4000,10007}){auto changed=native;changed[offset]^=std::byte{1};rejects([&]{sf::make_filter_shader(changed,scaled);});++rejections;}
    rejects([&]{sf::make_filter_shader(std::span(native).first(native.size()-1),scaled);});++rejections;
    LUID luid{};if(!warp){luid.LowPart=static_cast<DWORD>(std::stoul(argv[2]));luid.HighPart=static_cast<LONG>(std::stol(argv[3]));}
    Fixture f(warp,luid);unsigned comparisons=0;float maximum=0;std::ofstream report(out/"cases.jsonl");
    for(unsigned scenario=0;scenario<4;++scenario){
        const auto expected=f.draw(native,identity,scenario,false),actual=f.draw(one,identity,scenario,false);
        require(expected.size()==actual.size(),"identity lengths");for(std::size_t i=0;i<actual.size();++i){
            require(std::isfinite(actual[i])&&expected[i]==actual[i],"identity native comparison");++comparisons;}
        report<<"{\"case\":\"identity-"<<scenario<<"\",\"compared\":"<<actual.size()<<",\"exact\":true}\n";
        const bool constant=scenario>=2;const auto pixels=f.draw(two,scaled,scenario,constant);float error=0;
        for(unsigned y=0;y<scaled.output_height;++y)for(unsigned x=0;x<scaled.output_width;++x){
            // Independent bilinear color oracle, including the retained native
            // half-texel edge clamp and full-resolution integer alpha read.
            const double px=std::min(double(x)+.5,double(scaled.output_width)-1)-.5;
            const double py=std::min(double(y)+.5,double(scaled.output_height)-1)-.5;
            const auto x0=static_cast<unsigned>(std::floor(px)),y0=static_cast<unsigned>(std::floor(py));
            const auto a=color(x0,y0,scaled.output_width,constant),b=color(std::min(x0+1,scaled.output_width-1),y0,scaled.output_width,constant);
            const auto c=color(x0,std::min(y0+1,scaled.output_height-1),scaled.output_width,constant),d=color(std::min(x0+1,scaled.output_width-1),std::min(y0+1,scaled.output_height-1),scaled.output_width,constant);
            for(unsigned k=0;k<4;++k){const double ref=k==3?color(x,y,scaled.output_width,constant)[3]:
                (1-(py-y0))*((1-(px-x0))*a[k]+(px-x0)*b[k])+(py-y0)*((1-(px-x0))*c[k]+(px-x0)*d[k]);
                const auto v=pixels[(std::size_t(y)*scaled.output_width+x)*4+k];require(std::isfinite(v),"finite full output / poisoned auxiliary padding");
                error=std::max(error,static_cast<float>(std::abs(v-ref)));++comparisons;}
        }
        maximum=std::max(maximum,error);report<<"{\"case\":\"scaled-"<<scenario<<"\",\"compared\":"<<pixels.size()<<",\"max_abs_error\":"<<error<<"}\n";report.flush();
        require(error<=.00003F,"scaled pixel oracle");
        save(out/("scaled-"+std::to_string(scenario)+".rgba32f"),std::as_bytes(std::span(pixels)));
        if(scenario>=2){const auto original_layout=f.draw(reference,scaled,scenario,false,true),changed_layout=f.draw(two,scaled,scenario,false);
            require(original_layout.size()==changed_layout.size(),"expanded texture lengths");
            for(std::size_t i=0;i<changed_layout.size();++i){require(std::isfinite(changed_layout[i])&&changed_layout[i]==original_layout[i],"expanded texture varying-color blur reference");++comparisons;}
            report<<"{\"case\":\"expanded-texture-blur-"<<scenario<<"\",\"compared\":"<<changed_layout.size()<<",\"exact\":true}\n";
            save(out/("blur-"+std::to_string(scenario)+".rgba32f"),std::as_bytes(std::span(changed_layout)));
        }
    }
    f.check_messages();hr(f.allocator->Reset(),"final allocator retirement");hr(f.list->Reset(f.allocator.Get(),nullptr),"final recording retirement");hr(f.list->Close(),"final close");
    std::ofstream summary(out/"result.json");summary<<"{\"passed\":true,\"warp\":"<<(warp?"true":"false")<<",\"adapter_luid\":["<<f.actual_luid.HighPart<<','<<f.actual_luid.LowPart<<"],\"vendor\":"<<f.vendor<<",\"device\":"<<f.device_id<<",\"draws\":16,\"rejections\":"<<rejections<<",\"float_comparisons\":"<<comparisons<<",\"max_abs_error\":"<<maximum<<",\"gpu_warnings_errors\":0,\"reflection_unchanged\":true,\"recordings_retired\":true}\n";
    require(bool(summary),"result write");std::cout<<"PASS: 16 draws; "<<comparisons<<" float comparisons; "<<rejections<<" rejected inputs; max error "<<maximum<<"\n";
}
}
int wmain(int argc,wchar_t **argv){try{run(argc,argv);return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
