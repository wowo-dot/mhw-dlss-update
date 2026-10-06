// Actual vendor SDK, native AA-Off states, private output, and downstream GPU
// binding restoration. Runs in its own process; never opens the game.
#include "../src/aa_control/sdk_pass.hpp"
#include "../src/ngx_startup/startup.hpp"
#include "../src/temporal_present/observer.hpp"
#include <MinHook.h>
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
extern "C" {
unsigned fixture_native_calls{},fixture_native_result{1};
std::array<std::uintptr_t,4> fixture_arguments{};
unsigned fixture_native(std::uint64_t,const wchar_t *,ID3D12Device *,unsigned);
unsigned fixture_invoke(std::uint64_t,const wchar_t *,ID3D12Device *,unsigned);
void fixture_return();
}
namespace {
using Microsoft::WRL::ComPtr;
namespace aa=mhw::aa_control;namespace observer=mhw::temporal_present::observer;
void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
void hr(HRESULT result,const char *why){require(SUCCEEDED(result),why);}
bool method(std::uintptr_t object,unsigned slot,std::uintptr_t &address) noexcept {address=(*reinterpret_cast<std::uintptr_t **>(object))[slot];return address!=0;}
void WINAPI barrier(ID3D12GraphicsCommandList *l,UINT n,const D3D12_RESOURCE_BARRIER *b){l->ResourceBarrier(n,b);}
void transition(ID3D12GraphicsCommandList *l,ID3D12Resource *r,D3D12_RESOURCE_STATES a,D3D12_RESOURCE_STATES b){
    D3D12_RESOURCE_BARRIER item{};item.Transition={r,UINT_MAX,a,b};l->ResourceBarrier(1,&item);
}
ComPtr<ID3D12Resource> resource(ID3D12Device *d,D3D12_RESOURCE_DESC desc,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state){
    ComPtr<ID3D12Resource> r;D3D12_HEAP_PROPERTIES h{};h.Type=type;hr(d->CreateCommittedResource(&h,D3D12_HEAP_FLAG_NONE,&desc,state,nullptr,IID_PPV_ARGS(&r)),"resource");return r;
}
D3D12_RESOURCE_DESC buffer(UINT64 bytes,D3D12_RESOURCE_FLAGS flags=D3D12_RESOURCE_FLAG_NONE){
    D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=1;d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;d.Flags=flags;return d;
}
void run(int argc,wchar_t **argv){
    namespace fs=std::filesystem;namespace startup=mhw::ngx_startup;
    require(argc==7,"usage: aa_sdk_fixture runtime output legacy case width height");
    const auto runtime=fs::canonical(argv[1]),output=fs::absolute(argv[2]),legacy=fs::canonical(argv[3]);
    const std::wstring mode=argv[4];const auto w=static_cast<unsigned>(std::stoul(argv[5])),h=static_cast<unsigned>(std::stoul(argv[6]));
    require(mode==L"success"||mode==L"previous-incomplete"||mode==L"missing-state"||mode==L"after-conversion"||mode==L"evaluate-failure"||mode==L"no-retirement","case");
    require(!fs::exists(output),"preserve output");fs::create_directories(output);
    require(LoadLibraryExW(legacy.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)!=nullptr,"legacy");
    ComPtr<ID3D12Debug> debug;hr(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)),"debug");debug->EnableDebugLayer();
    ComPtr<ID3D12Debug1> validation;hr(debug.As(&validation),"validation");validation->SetEnableGPUBasedValidation(TRUE);
    ComPtr<IDXGIFactory4> factory;hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"factory");
    ComPtr<IDXGIAdapter1> adapter;ComPtr<ID3D12Device> device;
    for(unsigned i=0;factory->EnumAdapters1(i,&adapter)!=DXGI_ERROR_NOT_FOUND;++i){DXGI_ADAPTER_DESC1 desc{};hr(adapter->GetDesc1(&desc),"adapter");
        if(desc.VendorId==0x10de&&SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))))break;adapter.Reset();}
    require(device!=nullptr,"NVIDIA device");ComPtr<ID3D12InfoQueue> messages;hr(device.As(&messages),"messages");
    require(MH_Initialize()==MH_OK,"MinHook");std::array<std::uintptr_t,7> slots{};std::ofstream start(output/L"startup.jsonl");
    require(startup::install({reinterpret_cast<void *>(&fixture_native),reinterpret_cast<std::uintptr_t>(&fixture_return),reinterpret_cast<std::uintptr_t>(slots.data()),runtime,output/L"sdk-data",0,true},start),"startup install");
    require(fixture_invoke(startup::native_app,L"fixture",device.Get(),18)==1&&fixture_native_calls==1,"startup invoke");
    require(startup::disable()&&startup::settled()&&startup::borrowed_initialization(),"startup close");
    D3D12_COMMAND_QUEUE_DESC qd{};ComPtr<ID3D12CommandQueue> queue;ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Fence> fence;
    hr(device->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)),"queue");hr(device->CreateCommandAllocator(qd.Type,IID_PPV_ARGS(&allocator)),"allocator");
    hr(device->CreateCommandList(0,qd.Type,allocator.Get(),nullptr,IID_PPV_ARGS(&list)),"list");hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"fence");
    auto submit=[&](UINT64 value){hr(list->Close(),"close");ID3D12CommandList *lists[]{list.Get()};queue->ExecuteCommandLists(1,lists);hr(queue->Signal(fence.Get(),value),"signal");
        const auto until=GetTickCount64()+20000;while(fence->GetCompletedValue()<value&&GetTickCount64()<until)Sleep(1);require(fence->GetCompletedValue()==value,"completion");
        hr(allocator->Reset(),"allocator retirement");hr(list->Reset(allocator.Get(),nullptr),"list retirement");};
    std::array<ComPtr<ID3D12Resource>,3> native,uploads,readbacks;
    std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT,3> layouts;
    constexpr std::array<DXGI_FORMAT,3> formats={DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R32_UINT};
    constexpr std::array<unsigned,3> words={0x781e03c0,std::bit_cast<unsigned>(.1F),0};
    for(unsigned i=0;i<3;++i){D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=w;d.Height=h;d.DepthOrArraySize=d.MipLevels=1;d.Format=formats[i];d.SampleDesc.Count=1;
        native[i]=resource(device.Get(),d,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);UINT64 size{};
        device->GetCopyableFootprints(&d,0,1,0,&layouts[i],nullptr,nullptr,&size);
        uploads[i]=resource(device.Get(),buffer(size),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        readbacks[i]=resource(device.Get(),buffer(size),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        void *p{};D3D12_RANGE none{};hr(uploads[i]->Map(0,&none,&p),"upload map");
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)std::memcpy(static_cast<char *>(p)+SIZE_T(y)*layouts[i].Footprint.RowPitch+x*4,&words[i],4);
        uploads[i]->Unmap(0,nullptr);D3D12_TEXTURE_COPY_LOCATION a{},b{};a.pResource=uploads[i].Get();a.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;a.PlacedFootprint=layouts[i];b.pResource=native[i].Get();b.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        list->CopyTextureRegion(&b,0,0,0,&a,nullptr);transition(list.Get(),native[i].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);}
    submit(1);
    aa::SdkPass pass(device.Get(),{native[0].Get(),native[1].Get(),native[2].Get()},runtime,*startup::borrowed_initialization());
    D3D12_ROOT_PARAMETER parameters[2]{};parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;parameters[0].Descriptor={0,0};
    parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[1].Constants={0,0,1};
    D3D12_ROOT_SIGNATURE_DESC rd{2,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};ComPtr<ID3DBlob> blob,error,code;ComPtr<ID3D12RootSignature> root;
    hr(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error),"root blob");hr(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)),"root");
    constexpr char shader[]="RWStructuredBuffer<uint> output:register(u0);cbuffer C:register(b0){uint value;}[numthreads(1,1,1)]void main(){output[0]=value;}";
    hr(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_WARNINGS_ARE_ERRORS,0,&code,&error),"compile");
    D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.CS={code->GetBufferPointer(),code->GetBufferSize()};ComPtr<ID3D12PipelineState> pipeline;hr(device->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pipeline)),"pipeline");
    auto downstream=resource(device.Get(),buffer(256,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COMMON);
    auto proof=resource(device.Get(),buffer(256),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    std::string reason;require(observer::install(device.Get(),method,reason),"observer install");observer::begin();
    const auto count=(mode==L"success"||mode==L"no-retirement")?8U:(mode==L"previous-incomplete"?2U:1U);
    unsigned evaluations{};
    for(unsigned i=0;i<count;++i){observer::reset(list.Get(),i+1,nullptr);list->SetComputeRootSignature(root.Get());list->SetGraphicsRootSignature(root.Get());list->SetPipelineState(pipeline.Get());
        list->SetComputeRootUnorderedAccessView(0,downstream->GetGPUVirtualAddress());list->SetComputeRoot32BitConstant(1,0x123400+i,0);
        list->SetGraphicsRoot32BitConstant(1,0x567800+i,0);mhw::temporal_present::Bindings saved;require(observer::current(list.Get(),saved),"binding snapshot");auto supplied=saved;
        if(mode==L"missing-state")supplied.valid=false;
        const auto fault=mode==L"after-conversion"?mhw::dlss_sdk::RecordFault::after_conversion:mode==L"evaluate-failure"?mhw::dlss_sdk::RecordFault::evaluate_result:mhw::dlss_sdk::RecordFault::none;
        const std::array<unsigned,2> jitter=(i>0&&i<7)?std::array<unsigned,2>{std::bit_cast<unsigned>(.375F),std::bit_cast<unsigned>(-.3125F)}:std::array<unsigned,2>{0,0};
        const bool ok=pass.record(i,list.Get(),barrier,supplied,{100+i,100+i,jitter},mode!=L"previous-incomplete",fault);
        const bool wanted=mode==L"success"||mode==L"no-retirement"||(mode==L"previous-incomplete"&&i==0);
        require(ok==wanted,"record result");evaluations+=ok;
        require(observer::matches_current(list.Get(),saved),"native bindings restored");
        list->Dispatch(1,1,1);transition(list.Get(),downstream.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
        list->CopyResource(proof.Get(),downstream.Get());transition(list.Get(),downstream.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        for(unsigned j=0;j<3;++j){transition(list.Get(),native[j].Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE);
            D3D12_TEXTURE_COPY_LOCATION a{},b{};a.pResource=native[j].Get();a.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;b.pResource=readbacks[j].Get();b.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;b.PlacedFootprint=layouts[j];
            list->CopyTextureRegion(&b,0,0,0,&a,nullptr);transition(list.Get(),native[j].Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);}
        submit(i+2);void *p{};D3D12_RANGE range{0,256};hr(proof->Map(0,&range,&p),"proof map");const auto value=*static_cast<const unsigned *>(p);D3D12_RANGE none{};proof->Unmap(0,&none);require(value==0x123400+i,"downstream compute pixels");
        for(unsigned j=0;j<3;++j){hr(readbacks[j]->Map(0,nullptr,&p),"input map");bool equal=true;
            for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){unsigned word{};std::memcpy(&word,static_cast<const char *>(p)+SIZE_T(y)*layouts[j].Footprint.RowPitch+x*4,4);equal=equal&&word==words[j];}
            readbacks[j]->Unmap(0,&none);require(equal,"native input pixels unchanged");}
    }
    hr(list->Close(),"final empty close");require(observer::finish(),"observer close");
    // The adapter requires its selected fence value to be exactly 1. This
    // completion fence covers all preceding work, including every retirement.
    ComPtr<ID3D12Fence> final_fence;hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&final_fence)),"final fence");hr(queue->Signal(final_fence.Get(),1),"final signal");
    while(!final_fence->GetCompletedValue())Sleep(1);
    require(pass.finish(final_fence.Get(),mode!=L"no-retirement")== (mode==L"success"),"finish result");
    if(mode==L"success")require(pass.save((output/L"sdk").wstring()),"save outputs");
    unsigned errors{},warnings{};std::ofstream diagnostics(output/L"debug.txt");
    for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T size{};hr(messages->GetMessage(i,nullptr,&size),"debug size");std::vector<std::byte> bytes(size);auto *m=reinterpret_cast<D3D12_MESSAGE *>(bytes.data());hr(messages->GetMessage(i,m,&size),"debug read");
        if(m->Severity<=D3D12_MESSAGE_SEVERITY_WARNING)diagnostics<<m->ID<<' '<<m->pDescription<<'\n';
        errors+=m->Severity<=D3D12_MESSAGE_SEVERITY_ERROR;warnings+=m->Severity==D3D12_MESSAGE_SEVERITY_WARNING;}
    std::ofstream log(output/L"fixture.jsonl");log<<std::boolalpha;pass.write(log);log<<"{\"event\":\"fixture\",\"evaluations\":"<<evaluations<<",\"native_pixels_unchanged\":true,\"downstream_compute_exact\":true,\"bindings_equal\":true,\"gpu_validation\":true,\"debug_errors\":"<<errors<<",\"debug_warnings\":"<<warnings<<"}\n";
    require(!errors&&!warnings,"D3D12 validation");std::cout<<"AA SDK fixture passed\n";
}
}
int wmain(int argc,wchar_t **argv){try{run(argc,argv);return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
