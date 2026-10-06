#include "../src/aa_control/gpu_trace.hpp"
#include <dxgi1_4.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#include <MinHook.h>
#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
using Microsoft::WRL::ComPtr;
namespace g=mhw::aa_control::gpu_trace;namespace sub=mhw::temporal_bindings::submission;
void check(bool value,const char *why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
int main(int argc,char **argv){
    const std::string mode=argc>1?argv[1]:"success";
    ComPtr<ID3D12Debug> debug;check(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))),"debug interface");debug->EnableDebugLayer();
    ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter> warp;ComPtr<ID3D12Device> device;
    check(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)))&&
          SUCCEEDED(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))),"WARP device");
    std::array<ComPtr<ID3D12Resource>,3> inputs;std::array<std::uintptr_t,3> addresses{};
    constexpr std::array<DXGI_FORMAT,3> formats={DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R32_UINT};
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
    for(unsigned i=0;i<3;++i){D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=2560;d.Height=1440;
        d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Format=formats[i];d.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
        check(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&inputs[i]))),"input resource");
        addresses[i]=reinterpret_cast<std::uintptr_t>(inputs[i].Get());
    }
    std::array<ComPtr<ID3D12CommandAllocator>,9> allocators;
    for(auto &a:allocators)check(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&a))),"allocator");
    ComPtr<ID3D12GraphicsCommandList> list;check(SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocators[0].Get(),nullptr,IID_PPV_ARGS(&list)))&&SUCCEEDED(list->Close()),"list");
    ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    check(SUCCEEDED(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))),"queue");
    check(MH_Initialize()==MH_OK,"MinHook");std::string reason;
    check(g::install({0,0,0,reinterpret_cast<std::uintptr_t>(device.Get())},addresses,reason),reason.c_str());g::begin();
    if(mode=="signal_failure")sub::fixture_signal_failure();
    for(unsigned i=0;i<8;++i){
        check(SUCCEEDED(list->Reset(allocators[i].Get(),nullptr)),"tracked reset");
        std::array<D3D12_RESOURCE_BARRIER,3> barriers{};
        for(unsigned j=0;j<3;++j){auto &b=barriers[j];b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b.Transition={inputs[j].Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_SOURCE};}
        const unsigned n=(mode=="missing_after_reset"&&i==4)?2U:3U;
        list->ResourceBarrier(n,barriers.data());
        check(g::mark(i,100+i,1000+i,list.Get()),"marker");
        for(auto &b:barriers){b.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_SOURCE;b.Transition.StateAfter=D3D12_RESOURCE_STATE_COMMON;}
        list->ResourceBarrier(n,barriers.data());check(SUCCEEDED(list->Close()),"tracked close");
        ID3D12CommandList *batch[]{list.Get()};queue->ExecuteCommandLists(1,batch);
        if(mode=="signal_failure")break;
    }
    if(mode!="no_retire"&&mode!="signal_failure")check(SUCCEEDED(list->Reset(allocators[8].Get(),nullptr))&&SUCCEEDED(list->Close()),"final retirement");
    const auto until=GetTickCount64()+500;
    while(g::healthy()&&!g::complete()&&GetTickCount64()<until){g::poll();Sleep(1);}
    check(g::finish()&&g::disabled(),"all hooks drained and disabled");
    check(g::qualified()==(mode=="success"),"qualification outcome");
    check(g::safe_release()==(mode=="success"||mode=="missing_after_reset"),"lifetime release outcome");
    std::cout<<std::boolalpha;g::write(std::cout);
    // Drain test GPU work independently even in deliberately failed traces.
    ComPtr<ID3D12Fence> finish;check(SUCCEEDED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&finish)))&&SUCCEEDED(queue->Signal(finish.Get(),1)),"fixture drain");
    const auto deadline=GetTickCount64()+2000;while(finish->GetCompletedValue()!=1&&GetTickCount64()<deadline)Sleep(1);
    check(finish->GetCompletedValue()==1,"fixture GPU finished");
    ComPtr<ID3D12InfoQueue> info;check(SUCCEEDED(device.As(&info)),"debug info queue");
    for(UINT64 i=0;i<info->GetNumStoredMessagesAllowedByRetrievalFilter();++i){SIZE_T bytes{};
        check(SUCCEEDED(info->GetMessage(i,nullptr,&bytes)),"debug message size");std::vector<std::byte> storage(bytes);
        auto *message=reinterpret_cast<D3D12_MESSAGE *>(storage.data());check(SUCCEEDED(info->GetMessage(i,message,&bytes)),"debug message");
        if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){std::cerr<<message->pDescription<<'\n';check(false,"D3D12 validation error");}
    }
}
