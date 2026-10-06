#pragma once
// Private captured shader is supplied by path. No game process is opened and
// no proprietary shader is part of this source or the resulting executable.
#include "../src/scaled_temporal/dof_shader.hpp"
#include "../src/scaled_temporal/radial_shader.hpp"
#include <bit>
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

namespace dof_fixture {
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
struct Gpu {
    ComPtr<ID3D12Device> device;ComPtr<ID3D12InfoQueue> info;ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Fence> fence;UINT64 sequence{};
    unsigned vendor{};
    explicit Gpu(bool warp,bool gpu_validation=true){
        ComPtr<ID3D12Debug> debug;hr(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)),"debug");debug->EnableDebugLayer();
        ComPtr<ID3D12Debug1> validation;hr(debug.As(&validation),"validation");validation->SetEnableGPUBasedValidation(gpu_validation?TRUE:FALSE);
        ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter1> adapter;hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"factory");
        if(warp)hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)),"WARP");
        else {unsigned matches=0;for(unsigned i=0;;++i){ComPtr<IDXGIAdapter1> c;const auto h=factory->EnumAdapters1(i,&c);
            if(h==DXGI_ERROR_NOT_FOUND)break;hr(h,"adapter");DXGI_ADAPTER_DESC1 d{};hr(c->GetDesc1(&d),"description");
            if(d.VendorId==0x10de&&!(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)){adapter=c;++matches;}}require(matches==1,"one NVIDIA device");}
        DXGI_ADAPTER_DESC1 ad{};hr(adapter->GetDesc1(&ad),"description");vendor=ad.VendorId;
        hr(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)),"device");hr(device.As(&info),"info");
        D3D12_COMMAND_QUEUE_DESC q{};hr(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)),"queue");
        hr(device->CreateCommandAllocator(q.Type,IID_PPV_ARGS(&allocator)),"allocator");
        hr(device->CreateCommandList(0,q.Type,allocator.Get(),nullptr,IID_PPV_ARGS(&list)),"list");hr(list->Close(),"close");
        hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"fence");
    }
    void wait(){hr(queue->Signal(fence.Get(),++sequence),"signal");HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(event!=nullptr,"event");
        const auto result=fence->SetEventOnCompletion(sequence,event);const auto status=SUCCEEDED(result)?WaitForSingleObject(event,30000):WAIT_FAILED;CloseHandle(event);hr(result,"fence");require(status==WAIT_OBJECT_0,"wait");hr(device->GetDeviceRemovedReason(),"device health");}
    void messages(){for(UINT64 i=0;i<info->GetNumStoredMessages();++i){SIZE_T n{};hr(info->GetMessage(i,nullptr,&n),"message");std::vector<std::byte>b(n);auto m=reinterpret_cast<D3D12_MESSAGE *>(b.data());hr(info->GetMessage(i,m,&n),"message");
        if(m->Severity<=D3D12_MESSAGE_SEVERITY_WARNING){std::cerr<<m->pDescription<<'\n';throw std::runtime_error("GPU warning/error");}}}
};
}
