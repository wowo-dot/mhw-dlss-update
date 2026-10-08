#include "../src/aa_control/gpu_trace.hpp"
#include "../src/aa_control/input_readback.hpp"
#ifdef MHW_AA_BINDINGS
#include "../src/temporal_present/observer.hpp"
#include <d3dcompiler.h>
#endif
#include <dxgi1_4.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#include <MinHook.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <thread>
using Microsoft::WRL::ComPtr;
namespace g=mhw::aa_control::gpu_trace;namespace sub=mhw::temporal_bindings::submission;
namespace rb=mhw::aa_control::input_readback;
void check(bool v,const char *why){if(!v){std::cerr<<why<<'\n';std::exit(1);}}
HANDLE close_entered{},close_release{};
void hold_close(){SetEvent(close_entered);check(WaitForSingleObject(close_release,3000)==WAIT_OBJECT_0,"pending close released");}
void transition(ID3D12GraphicsCommandList *l,ID3D12Resource *r,D3D12_RESOURCE_STATES a,D3D12_RESOURCE_STATES b){
    D3D12_RESOURCE_BARRIER barrier{};barrier.Transition={r,UINT_MAX,a,b};l->ResourceBarrier(1,&barrier);
}
void drain(ID3D12Device *device,ID3D12CommandQueue *queue){
    ComPtr<ID3D12Fence> fence;check(SUCCEEDED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)))&&SUCCEEDED(queue->Signal(fence.Get(),1)),"drain signal");
    const auto until=GetTickCount64()+4000;while(fence->GetCompletedValue()!=1&&GetTickCount64()<until)Sleep(1);
    check(fence->GetCompletedValue()==1,"drain completion");
}
int main(int argc,char **argv){
    const std::string mode=argc>1?argv[1]:"success";
    const bool positive=mode=="success"||mode=="trailing_traffic"||mode=="delayed_gpu"||mode=="late_disable"||mode=="pending_close";
    const bool stress=positive&&mode!="success";
    ComPtr<ID3D12Debug> debug;check(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))),"debug");debug->EnableDebugLayer();
    ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter> warp;ComPtr<ID3D12Device> device;
    check(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)))&&
        SUCCEEDED(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))),"WARP device");
    ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    check(SUCCEEDED(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))),"queue");
    std::array<ComPtr<ID3D12CommandAllocator>,9> marker_allocators,producer_allocators;
    for(auto *set:{&marker_allocators,&producer_allocators})for(auto &a:*set)
        check(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&a))),"allocator");
    ComPtr<ID3D12CommandAllocator> init_allocator;ComPtr<ID3D12GraphicsCommandList> init,producer,marker;
    check(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&init_allocator)))&&
        SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,init_allocator.Get(),nullptr,IID_PPV_ARGS(&init))),"init list");
    check(SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,producer_allocators[0].Get(),nullptr,IID_PPV_ARGS(&producer)))&&SUCCEEDED(producer->Close()),"producer list");
    check(SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,marker_allocators[0].Get(),nullptr,IID_PPV_ARGS(&marker)))&&SUCCEEDED(marker->Close()),"marker list");
    constexpr std::array<DXGI_FORMAT,3> formats={DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R32_UINT};
    constexpr std::array<unsigned,3> values={0x781e03c0,0x3f000000,0x81234567};
    std::array<ComPtr<ID3D12Resource>,3> textures,uploads;std::array<std::uintptr_t,3> addresses{};
    for(unsigned role=0;role<3;++role){
        D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=2560;d.Height=1440;
        d.DepthOrArraySize=d.MipLevels=1;d.Format=formats[role];d.SampleDesc.Count=1;
        d.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if(role==0)d.Flags|=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        check(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&textures[role]))),"texture");
        addresses[role]=reinterpret_cast<std::uintptr_t>(textures[role].Get());
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT rows{};UINT64 row_bytes{},bytes{};
        device->GetCopyableFootprints(&d,0,1,0,&footprint,&rows,&row_bytes,&bytes);
        check(bytes==14745600&&row_bytes==10240&&rows==1440,"footprint");
        heap.Type=D3D12_HEAP_TYPE_UPLOAD;d={};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=1;
        d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        check(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&uploads[role]))),"upload");
        void *mapped{};D3D12_RANGE empty{};check(SUCCEEDED(uploads[role]->Map(0,&empty,&mapped)),"upload map");
        auto *words=static_cast<unsigned *>(mapped);for(unsigned pixel=0;pixel<2560*1440;++pixel)words[pixel]=values[role];uploads[role]->Unmap(0,nullptr);
        D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=uploads[role].Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=footprint;
        dst.pResource=textures[role].Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        init->CopyTextureRegion(&dst,0,0,0,&src,nullptr);transition(init.Get(),textures[role].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    check(SUCCEEDED(init->Close()),"init close");ID3D12CommandList *initial[]={init.Get()};queue->ExecuteCommandLists(1,initial);drain(device.Get(),queue.Get());
    check(MH_Initialize()==MH_OK,"MinHook");std::string reason;
#ifdef MHW_AA_BINDINGS
    namespace obs=mhw::temporal_present::observer;
    ComPtr<ID3DBlob> signature,shader,error;ComPtr<ID3D12RootSignature> root;ComPtr<ID3D12PipelineState> pipeline;
    D3D12_ROOT_PARAMETER parameter{};parameter.ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameter.Constants.Num32BitValues=1;
    D3D12_ROOT_SIGNATURE_DESC root_desc{};root_desc.NumParameters=1;root_desc.pParameters=&parameter;
    check(SUCCEEDED(D3D12SerializeRootSignature(&root_desc,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&error))&&
        SUCCEEDED(device->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&root))),"fixture root");
    constexpr char source[]="[numthreads(1,1,1)] void main() {}";
    check(SUCCEEDED(D3DCompile(source,sizeof(source),nullptr,nullptr,nullptr,"main","cs_5_0",0,0,&shader,&error)),"fixture shader");
    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_desc{};pipeline_desc.pRootSignature=root.Get();pipeline_desc.CS={shader->GetBufferPointer(),shader->GetBufferSize()};
    check(SUCCEEDED(device->CreateComputePipelineState(&pipeline_desc,IID_PPV_ARGS(&pipeline))),"fixture pipeline");
#endif
    check(g::install({0,0,0,reinterpret_cast<std::uintptr_t>(device.Get())},addresses,reason),reason.c_str());g::begin();
    ComPtr<ID3D12Fence> gpu_gate;
    if(mode=="delayed_gpu"){
        check(SUCCEEDED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&gpu_gate)))&&
            SUCCEEDED(queue->Wait(gpu_gate.Get(),1)),"hold actual GPU completion");
    }
    if(mode=="signal_failure")sub::fixture_signal_failure();
    for(unsigned i=0;i<8;++i){
#ifdef MHW_AA_BINDINGS
        if(mode=="missing_bindings_generation")obs::stop();
#endif
        check(SUCCEEDED(marker->Reset(marker_allocators[i].Get(),nullptr)),"marker reset");
#ifdef MHW_AA_BINDINGS
        if(mode=="missing_bindings_generation")obs::begin();
        marker->SetPipelineState(pipeline.Get());
        if(mode!="missing_compute_root"){
            marker->SetComputeRootSignature(root.Get());marker->SetComputeRoot32BitConstant(0,1234+i,0);
        }
        marker->SetGraphicsRootSignature(root.Get());marker->SetGraphicsRoot32BitConstant(0,9876+i,0);
#endif
        transition(marker.Get(),textures[0].Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
        if(mode!="missing_depth"){
            transition(marker.Get(),textures[1].Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST);
            transition(marker.Get(),textures[1].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        check(g::mark(i,100+i,1000+i,marker.Get()),"marker");
        ID3D12Fence *not_completed{};
        check(!sub::completed_receipt(i,&not_completed)&&!not_completed,"recorded work is not completion proof");
        // CPU records this producer later; GPU executes it first. Its state must
        // never be borrowed merely because it was most recently recorded.
        check(SUCCEEDED(producer->Reset(producer_allocators[i].Get(),nullptr)),"producer reset");
        transition(producer.Get(),textures[2].Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
        check(SUCCEEDED(producer->Close()),"producer close");
        std::array<D3D12_RESOURCE_BARRIER,2> pair{};
        pair[0].Transition={textures[0].Get(),UINT_MAX,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
        pair[1].Transition={textures[2].Get(),UINT_MAX,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
        if(mode=="wrong_pair"){marker->ResourceBarrier(1,pair.data());marker->ResourceBarrier(1,pair.data()+1);}
        else marker->ResourceBarrier(2,pair.data());
#ifdef MHW_AA_BINDINGS
        if(positive){
            mhw::temporal_present::Bindings captured;check(obs::sample(i,captured)&&obs::matches_current(marker.Get(),captured),"binding snapshot unchanged");
            check(captured.ready()&&captured.pipeline==reinterpret_cast<std::uintptr_t>(pipeline.Get())&&
                captured.compute.signature==reinterpret_cast<std::uintptr_t>(root.Get())&&captured.graphics.signature==captured.compute.signature&&
                captured.compute.arguments[0].values[0]==1234+i&&captured.graphics.arguments[0].values[0]==9876+i,"exact native binding values");
        }
#endif
        check(SUCCEEDED(marker->Close()),"marker close");ID3D12CommandList *batch[]={producer.Get(),marker.Get()};queue->ExecuteCommandLists(2,batch);
        if(!g::healthy())break;
    }
    if(positive||mode=="active_overflow")check(SUCCEEDED(marker->Reset(marker_allocators[8].Get(),nullptr))&&SUCCEEDED(marker->Close()),"retire");
    const auto traffic=[&](bool barriers){
        // More than the unchanged 4096-event budget; original APIs still run.
        for(unsigned i=0;i<2500;++i){
            check(SUCCEEDED(marker->Reset(marker_allocators[8].Get(),nullptr)),"trailing reset");
            if(barriers){
                transition(marker.Get(),textures[0].Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
                transition(marker.Get(),textures[0].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
#ifdef MHW_AA_BINDINGS
                marker->SetComputeRootSignature(root.Get());marker->SetComputeRoot32BitConstant(0,2222,0);
#endif
            }
            check(SUCCEEDED(marker->Close()),"trailing close");
        }
    };
    if(mode=="active_overflow"){
        traffic(false);check(!g::healthy()&&sub::recorded_events()==4096,"active journal overflow remains a failure");
    }
    if(gpu_gate){
        g::poll();check(g::healthy()&&!g::complete()&&sub::admission_open(),"incomplete fences keep observation open");
        check(SUCCEEDED(gpu_gate->Signal(1)),"release GPU gate");
    }
    const auto until=GetTickCount64()+700;while(g::healthy()&&!g::complete()&&GetTickCount64()<until){g::poll();Sleep(1);}
    if(positive){ID3D12Fence *completed{};check(sub::completed_receipt(7,&completed)&&completed&&completed->GetCompletedValue()==1,"borrow exact completed/retired fence");completed->Release();}
    if(mode=="pending_close"){
        check(g::complete(),"completed before pending unrelated close");
        close_entered=CreateEventW(nullptr,TRUE,FALSE,nullptr);close_release=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        check(close_entered&&close_release,"close events");
        check(SUCCEEDED(producer->Reset(producer_allocators[8].Get(),nullptr)),"pending list reset");
        sub::fixture_before_close_return(hold_close);
        std::thread native([&]{check(SUCCEEDED(producer->Close()),"pending native close");});
        check(WaitForSingleObject(close_entered,1000)==WAIT_OBJECT_0,"admitted close entered");
        std::thread release([]{
            const auto end=GetTickCount64()+1000;while(sub::admission_open()&&GetTickCount64()<end)Sleep(1);
            check(!sub::admission_open(),"close admission reached");Sleep(30);SetEvent(close_release);
        });
        const auto start=GetTickCount64();check(g::close_admission()&&sub::settled()&&GetTickCount64()-start>=20,"admitted close drains before removal");
        native.join();release.join();sub::fixture_before_close_return(nullptr);CloseHandle(close_entered);CloseHandle(close_release);
    }
    if(stress||mode=="early_cutoff"){
        check(g::complete()==positive,"completion before cutoff");
        check(g::close_admission()&&g::close_admission()&&!g::disabled()&&!sub::admission_open(),"close is idempotent and leaves hooks installed");
        const auto recorded=sub::recorded_events();traffic(true);
        check(sub::recorded_events()==recorded&&g::healthy(),"post-cutoff traffic cannot grow or fault the journal");
        check(g::complete()==positive,"later Reset cannot manufacture retirement after early cutoff");
        if(mode=="late_disable")Sleep(2100); // Beyond the recording deadline, after a valid cutoff.
    }
    check(g::finish()&&g::disabled(),"finish");check(g::qualified()==positive,"qualification");
    ID3D12Fence *released_fence{};
    check(!sub::completed_receipt(7,&released_fence)&&!released_fence,"retired journal cannot lend released fence");
    // A process ID alone repeats across runs; with the tick the folder is new.
    // An automatic folder is removed after the run (127 MB per run otherwise).
    const bool own_folder=argc<=2;
    const auto folder=own_folder?std::filesystem::current_path()/("aa-input-fixture-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64())):std::filesystem::path(argv[2]);
    std::filesystem::create_directories(folder);const auto prefix=folder/L"input";
    const bool saved=rb::save(prefix.wstring());check(saved==positive,"safe save");
    if(saved)for(auto receipt:rb::receipts)for(unsigned role=0;role<3;++role){
        const auto path=prefix.wstring()+L"-"+std::to_wstring(receipt)+L"-"+std::to_wstring(role)+L".bin";
        check(std::filesystem::file_size(path)==14745600,"saved extent");std::ifstream in(path,std::ios::binary);
        std::vector<unsigned> words(2560*1440);check(bool(in.read(reinterpret_cast<char *>(words.data()),14745600)),"saved read");
        for(auto value:words)check(value==values[role],"pixel mismatch");
    }
    if(own_folder){std::error_code ignored;std::filesystem::remove_all(folder,ignored);}
    std::cout<<std::boolalpha;g::write(std::cout);rb::write(std::cout);drain(device.Get(),queue.Get());
    ComPtr<ID3D12InfoQueue> info;check(SUCCEEDED(device.As(&info)),"debug info queue");
    for(UINT64 i=0;i<info->GetNumStoredMessagesAllowedByRetrievalFilter();++i){SIZE_T bytes{};check(SUCCEEDED(info->GetMessage(i,nullptr,&bytes)),"message size");
        std::vector<std::byte> storage(bytes);auto *message=reinterpret_cast<D3D12_MESSAGE *>(storage.data());check(SUCCEEDED(info->GetMessage(i,message,&bytes)),"message");
        if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){std::cerr<<message->pDescription<<'\n';check(false,"D3D12 error");}}
}
