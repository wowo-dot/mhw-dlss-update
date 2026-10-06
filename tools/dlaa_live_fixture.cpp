// Offline borrowed-device fixture: same conversion and SDK code as the live DLL.
#include "../src/temporal_gpu_inputs/live.hpp"
#include "../src/dlss_sdk/live.hpp"
#include "../src/dlss_sdk/contract.hpp"
#include "../src/ngx_startup/startup.hpp"
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <MinHook.h>
extern "C" {
unsigned fixture_native_calls{},fixture_native_result{1};
std::array<std::uintptr_t,4> fixture_arguments{};
unsigned fixture_native(std::uint64_t,const wchar_t *,ID3D12Device *,unsigned);
unsigned fixture_invoke(std::uint64_t,const wchar_t *,ID3D12Device *,unsigned);
void fixture_return();
}
namespace {
namespace fs=std::filesystem;using Microsoft::WRL::ComPtr;
void require(bool v,const char *s){if(!v)throw std::runtime_error(s);}
void hr(HRESULT v,const char *s){require(SUCCEEDED(v),s);}
void run(int argc,wchar_t **argv){
    require(argc==11,"usage: fixture runtime output legacy_dll source_folder prefix luid_low luid_high width height case");
    const fs::path runtime=fs::canonical(argv[1]),output=fs::absolute(argv[2]),legacy=fs::canonical(argv[3]),source=fs::canonical(argv[4]);
    require(!fs::exists(output),"preserve fixture");fs::create_directories(output);const std::wstring prefix=argv[5],mode=argv[10];
    const auto low=static_cast<UINT>(std::stoul(argv[6]));const auto high=static_cast<LONG>(std::stol(argv[7]));
    const auto w=static_cast<unsigned>(std::stoul(argv[8])),h=static_cast<unsigned>(std::stoul(argv[9]));
    require(w>=32&&h>=32&&w<=8192&&h<=8192&&UINT64(w)*h<=4194304,"fixture extent");
    const bool startup_mode=mode.starts_with(L"startup");
    const bool long_mode=mode.starts_with(L"startup-long");
    const unsigned frame_count=mode==L"startup-long-count"?4U:long_mode?9U:3U;
    const bool temporal=long_mode||mode==L"startup-history"||mode==L"startup-history-zero"||mode==L"startup-history-gap";
    const bool success=mode==L"startup-long-history"||mode==L"success"||mode==L"startup"||mode==L"startup-reserved"||mode==L"startup-history"||mode==L"startup-history-zero";
    require(success||temporal||mode==L"no_retire"||mode==L"incomplete"||mode==L"alias"||mode==L"format"||
        mode==L"startup-native-failure"||mode==L"startup-unowned"||mode==L"startup-rejected","fixture case");
    const auto old=LoadLibraryExW(legacy.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    require(old!=nullptr,"load original legacy provider"); // retained through fixture exit
    ComPtr<ID3D12Debug> debug;hr(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)),"debug");debug->EnableDebugLayer();
    ComPtr<ID3D12Debug1> validation;hr(debug.As(&validation),"validation");validation->SetEnableGPUBasedValidation(TRUE);
    ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter1> adapter;ComPtr<ID3D12Device> device;
    hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"factory");hr(factory->EnumAdapterByLuid({low,high},IID_PPV_ARGS(&adapter)),"adapter");
    hr(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)),"device");
    ComPtr<ID3D12InfoQueue> messages;hr(device.As(&messages),"info queue");
    std::ofstream startup_log;std::array<std::uintptr_t,7> native_slots{};
    const mhw::dlss_sdk::BorrowedInitialization *initialization{};
    if(startup_mode){
        namespace startup=mhw::ngx_startup;
        startup_log.open(output/L"startup.jsonl",std::ios::binary);require(bool(startup_log),"startup log");
        const bool reserved=mode==L"startup-reserved"||temporal;
        if(reserved)require(MH_Initialize()==MH_OK,"fixture prior hook initialization");
        startup::install({reinterpret_cast<void *>(&fixture_native),reinterpret_cast<std::uintptr_t>(&fixture_return),
            reinterpret_cast<std::uintptr_t>(native_slots.data()),runtime,output/L"startup-data",0,reserved},startup_log);
        require(!startup::borrowed_initialization(),"premature startup ownership");
        if(mode==L"startup-native-failure")fixture_native_result=0xbad00002;
        const auto original=fixture_invoke(startup::native_app,L"unchanged fixture path",device.Get(),18);
        require(original==fixture_native_result&&fixture_native_calls==1&&startup::calls()==1,"startup original result/count");
        require(!startup::borrowed_initialization(),"active startup hook ownership");
        require(startup::disable()&&startup::settled()&&startup::done(),"startup closure");
        initialization=startup::borrowed_initialization();
        require(bool(initialization)==(mode!=L"startup-native-failure"),"startup native-result admission");
        if(initialization){
            require(initialization->validate(device.Get(),runtime)!=nullptr,"retained initialization");
            require(!initialization->validate(nullptr,runtime)&&!initialization->validate(device.Get(),output),"foreign startup ownership");
        }
    }
    ComPtr<ID3D12CommandQueue> queue;ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Fence> fence;
    D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)),"queue");hr(device->CreateCommandAllocator(q.Type,IID_PPV_ARGS(&allocator)),"allocator");
    hr(device->CreateCommandList(0,q.Type,allocator.Get(),nullptr,IID_PPV_ARGS(&list)),"list");hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"fence");
    const auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(event!=nullptr,"event");
    std::array<std::unique_ptr<mhw::temporal_gpu_inputs::Live>,mhw::dlss_sdk::max_frames> live;
    std::array<std::array<ComPtr<ID3D12Resource>,3>,mhw::dlss_sdk::max_frames> uploads;
    constexpr std::array<const wchar_t *,3> roles{L"color",L"depth",L"motion"};
    std::array<mhw::dlss_sdk::GpuFrame,mhw::dlss_sdk::max_frames> frames;
    for(unsigned i=0;i<frame_count;++i){live[i]=std::make_unique<mhw::temporal_gpu_inputs::Live>(device.Get(),w,h);
        require(!live[i]->converted(0)&&!live[i]->completed_fence(),"premature conversion exposure");
        for(unsigned j=0;j<3;++j){
            const auto path=source/(prefix+L"."+roles[j]+L"-"+std::to_wstring(i%3)+(j==1?L".f32":L".u32"));
            require(fs::file_size(path)==UINT64(w)*h*4,"raw input extent");
            std::vector<std::byte> bytes(std::size_t(w)*h*4);std::ifstream file(path,std::ios::binary);
            require(bool(file.read(reinterpret_cast<char *>(bytes.data()),static_cast<std::streamsize>(bytes.size()))),"raw input");
            auto *dest=live[i]->input(j);const auto desc=dest->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};UINT64 size{};
            device->GetCopyableFootprints(&desc,0,1,0,&layout,nullptr,nullptr,&size);
            D3D12_RESOURCE_DESC rb{};rb.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;rb.Width=size;rb.Height=1;rb.DepthOrArraySize=rb.MipLevels=1;
            rb.SampleDesc.Count=1;rb.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_UPLOAD;
            hr(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&rb,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&uploads[i][j])),"upload");
            void *mapped{};D3D12_RANGE none{};hr(uploads[i][j]->Map(0,&none,&mapped),"map");
            for(unsigned y=0;y<h;++y)std::memcpy(static_cast<std::byte *>(mapped)+std::size_t(y)*layout.Footprint.RowPitch,bytes.data()+std::size_t(y)*w*4,std::size_t(w)*4);
            uploads[i][j]->Unmap(0,nullptr);
            D3D12_TEXTURE_COPY_LOCATION from{},to{};from.pResource=uploads[i][j].Get();from.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;from.PlacedFootprint=layout;
            to.pResource=dest;to.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;list->CopyTextureRegion(&to,0,0,0,&from,nullptr);
        }
    }
    hr(list->Close(),"close upload");ID3D12CommandList *lists[]{list.Get()};queue->ExecuteCommandLists(1,lists);hr(queue->Signal(fence.Get(),1),"signal");
    hr(fence->SetEventOnCompletion(1,event),"completion");require(WaitForSingleObject(event,15000)==WAIT_OBJECT_0&&fence->GetCompletedValue()==1,"upload completion");
    hr(allocator->Reset(),"retire upload allocator");hr(list->Reset(allocator.Get(),nullptr),"retire upload list");hr(list->Close(),"close empty list");CloseHandle(event);
    for(unsigned i=0;i<frame_count;++i){
        const auto converted=live[i]->convert();std::string why;
        require(mhw::dlss_sdk::validate({w,h,converted.color,converted.depth,converted.motion},why),"converted value contract");
        auto &f=frames[i];f.width=w;f.height=h;f.producer_fence=live[i]->completed_fence();f.producer_value=converted.fence_value;
        f.producer_retired=converted.completed&&converted.allocator_retired;
        for(unsigned j=0;j<3;++j)f.inputs[j]=live[i]->converted(j);
    }
    ComPtr<ID3D12Fence> incomplete;
    if(mode==L"no_retire")frames[0].producer_retired=false;
    if(mode==L"incomplete"){hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&incomplete)),"incomplete fence");frames[0].producer_fence=incomplete.Get();}
    if(mode==L"alias")frames[1].inputs[0]=frames[0].inputs[0];
    if(mode==L"format")frames[0].inputs[0]=live[0]->input(0);
    std::ofstream log(output/L"events.jsonl",std::ios::binary);require(bool(log),"log");
    bool rejected{};
    try {
        const mhw::dlss_sdk::BorrowedInitialization reject{[](ID3D12Device *,const fs::path &) noexcept -> HMODULE {return nullptr;}};
        if(mode==L"startup-native-failure"||mode==L"startup-rejected")initialization=&reject;
        if(mode==L"startup-unowned")initialization=nullptr;
        // Synthetic temporal metadata tests API transport/retention only: the
        // archived input pixels were not rendered with this synthetic jitter.
        std::array<mhw::dlss_sdk::HistoryFrame,mhw::dlss_sdk::max_frames> history;
        for(unsigned i=0;i<frame_count;++i)history[i]={25383+i,25383+i,i&&i+1<frame_count?.1875F:0,i&&i+1<frame_count?.21875F:0};
        if(mode==L"startup-history-zero")history[1].jitter_x=history[1].jitter_y=0;
        if(mode==L"startup-long-gap")++history[7].frame;
        if(mode==L"startup-history-gap")++history[1].frame;
        const auto result=mhw::dlss_sdk::evaluate_bounded(device.Get(),std::span(frames).first(frame_count),runtime,output/L"sdk-data",true,log,initialization,temporal?std::span<const mhw::dlss_sdk::HistoryFrame>(history).first(mode==L"startup-long-short"?8:frame_count):std::span<const mhw::dlss_sdk::HistoryFrame>{});
        require(success&&result.complete&&result.shutdown_deferred&&result.feature_released&&result.parameters_destroyed,"result");
        for(unsigned i=0;i<frame_count;++i){std::ofstream pixels(output/(L"output-"+std::to_wstring(i)+L".rgba32f"),std::ios::binary);
            pixels.write(reinterpret_cast<const char *>(result.output[i].data()),static_cast<std::streamsize>(result.output[i].size()*4));require(bool(pixels),"save");}
    }catch(const std::exception &e){if(success)throw;rejected=true;std::cout<<e.what()<<'\n';}
    require(success||rejected,"expected rejection");
    unsigned errors{},warnings{};
    for(UINT64 i=0;i<messages->GetNumStoredMessagesAllowedByRetrievalFilter();++i){SIZE_T size{};hr(messages->GetMessage(i,nullptr,&size),"diagnostic size");
        std::vector<std::byte> data(size);auto *m=reinterpret_cast<D3D12_MESSAGE *>(data.data());hr(messages->GetMessage(i,m,&size),"diagnostic");
        if(m->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)++errors;if(m->Severity==D3D12_MESSAGE_SEVERITY_WARNING)++warnings;
        if(m->Severity<=D3D12_MESSAGE_SEVERITY_WARNING&&errors+warnings<=12)std::cerr<<m->pDescription<<'\n';}
    require(errors==0&&warnings==0,"GPU validation");
    std::ofstream checks(output/L"fixture.json");checks<<"{\"success\":true,\"rejection_case\":"<<(rejected?"true":"false")
        <<",\"debug_errors\":0,\"debug_warnings\":0,\"gpu_validation\":true,\"legacy_preloaded\":true,\"game_process_access\":false}\n";
}
}
int wmain(int argc,wchar_t **argv){try{run(argc,argv);return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
