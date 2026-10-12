#include "../src/continuous_dlaa/depth_consumer_copy.hpp"
#include "../src/continuous_dlaa/retention.hpp"
#include <windows.h>
#include <dxgi1_4.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstring>
using Microsoft::WRL::ComPtr;
namespace cd=mhw::continuous_dlaa;
void require(bool v,const char *s){if(!v)throw std::runtime_error(s);}
void hr(HRESULT v,const char *s){require(SUCCEEDED(v),s);}
void WINAPI barriers(ID3D12GraphicsCommandList *l,UINT n,const D3D12_RESOURCE_BARRIER *b){l->ResourceBarrier(n,b);}
void WINAPI copy(ID3D12GraphicsCommandList *l,ID3D12Resource *d,ID3D12Resource *s){l->CopyResource(d,s);}
void transition(ID3D12GraphicsCommandList *l,ID3D12Resource *r,D3D12_RESOURCE_STATES a,D3D12_RESOURCE_STATES b){D3D12_RESOURCE_BARRIER v{};v.Transition={r,UINT_MAX,a,b};l->ResourceBarrier(1,&v);}
int wmain(int argc,wchar_t **argv){try{
    require(argc==4,"usage: warp|hardware output.json adapter-luid-low");const bool warp=std::wstring_view(argv[1])==L"warp";
    const std::filesystem::path out=argv[2];require(!std::filesystem::exists(out),"preserve output");
    ComPtr<ID3D12Debug> debug;hr(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)),"debug");debug->EnableDebugLayer();
    ComPtr<ID3D12Debug1> validation;hr(debug.As(&validation),"validation");validation->SetEnableGPUBasedValidation(TRUE);
    ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter1> adapter;hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"factory");
    if(warp)hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)),"warp");else hr(factory->EnumAdapterByLuid({std::stoul(argv[3]),0},IID_PPV_ARGS(&adapter)),"adapter");
    DXGI_ADAPTER_DESC1 ad{};hr(adapter->GetDesc1(&ad),"adapter identity");ComPtr<ID3D12Device> device;
    hr(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)),"device");
    ComPtr<ID3D12InfoQueue> info;hr(device.As(&info),"info");ComPtr<ID3D12Fence> fence;hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"fence");
    std::array<ComPtr<ID3D12CommandQueue>,2> queues;std::array<ComPtr<ID3D12CommandAllocator>,2> allocators;
    std::array<ComPtr<ID3D12GraphicsCommandList>,2> lists;
    for(unsigned i=0;i<2;++i){D3D12_COMMAND_QUEUE_DESC q{};hr(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queues[i])),"queue");
        hr(device->CreateCommandAllocator(q.Type,IID_PPV_ARGS(&allocators[i])),"allocator");
        hr(device->CreateCommandList(0,q.Type,allocators[i].Get(),nullptr,IID_PPV_ARGS(&lists[i])),"list");hr(lists[i]->Close(),"close");}
    D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=128;d.Height=64;d.DepthOrArraySize=d.MipLevels=1;
    d.Format=DXGI_FORMAT_R32_FLOAT;d.SampleDesc.Count=1;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;ComPtr<ID3D12Resource> source,destination,readback;
    D3D12_CLEAR_VALUE initial_clear{DXGI_FORMAT_R32_FLOAT,{0,0,0,0}};
    hr(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_RENDER_TARGET,&initial_clear,IID_PPV_ARGS(&source)),"source");
    d.Flags=D3D12_RESOURCE_FLAG_NONE;hr(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&destination)),"destination");
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 bytes{};device->GetCopyableFootprints(&d,0,1,0,&footprint,nullptr,nullptr,&bytes);
    auto buffer=d;buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=bytes;buffer.Height=1;buffer.Format=DXGI_FORMAT_UNKNOWN;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    heap.Type=D3D12_HEAP_TYPE_READBACK;hr(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)),"readback");
    ComPtr<ID3D12DescriptorHeap> rtv;D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=1;
    hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv)),"rtv");device->CreateRenderTargetView(source.Get(),nullptr,rtv->GetCPUDescriptorHandleForHeapStart());
    cd::RecordingPins pins;std::uint64_t pixels{},signals{};unsigned switches{};
    for(unsigned frame=1;frame<=96;++frame){
        // Rotate the native source after the prior recording has retired. Its
        // optimized clear matches this frame, avoiding fixture-only warnings.
        const float value=float(frame)/128.F;D3D12_CLEAR_VALUE clear{DXGI_FORMAT_R32_FLOAT,{value,value,value,value}};
        auto target_desc=d;target_desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;D3D12_HEAP_PROPERTIES target_heap{};target_heap.Type=D3D12_HEAP_TYPE_DEFAULT;
        source.Reset();hr(device->CreateCommittedResource(&target_heap,D3D12_HEAP_FLAG_NONE,&target_desc,D3D12_RESOURCE_STATE_RENDER_TARGET,&clear,IID_PPV_ARGS(&source)),"rotating source");
        device->CreateRenderTargetView(source.Get(),nullptr,rtv->GetCPUDescriptorHandleForHeapStart());
        for(unsigned i=0;i<2;++i){hr(allocators[i]->Reset(),"reset allocator");hr(lists[i]->Reset(allocators[i].Get(),nullptr),"reset list");}
        auto *consumer=lists[1].Get();auto *writer=lists[0].Get();
        // 0-3: the scaled writer (render target), then no or a hoisted read.
        // 4-5: the whole-copy writer (render scale 1.0) leaves COPY_DEST, then
        // no read transition before the consumer (game-76) or a hoisted one (game-75).
        constexpr D3D12_RESOURCE_STATES states[]{D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
        const unsigned kind=(frame-1)%6;const auto state=states[kind];const bool whole=kind>=4,hoisted=kind!=0&&kind!=4;
        const cd::ResourcePins resources{source,destination,source,destination};require(pins.retain(consumer,frame,resources),"retain consumer recording");
        require(!cd::record_consumer_depth(consumer,source.Get(),source.Get(),128,64,state,barriers,copy),"reject self copy");
        require(!cd::record_consumer_depth(consumer,source.Get(),destination.Get(),127,64,state,barriers,copy),"reject extent mismatch");
        require(!cd::record_consumer_depth(consumer,source.Get(),destination.Get(),128,64,D3D12_RESOURCE_STATE_COMMON,barriers,copy),"reject unknown state");
        require(!cd::record_consumer_depth(consumer,source.Get(),destination.Get(),128,64,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,barriers,copy),"reject writable state");
        require(cd::record_consumer_depth(consumer,source.Get(),destination.Get(),128,64,state,barriers,copy),"consumer copy");
        transition(consumer,destination.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION dst{},src{};dst.pResource=readback.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=footprint;
        src.pResource=destination.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;consumer->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
        transition(consumer,destination.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COPY_DEST);
        // The native sequence continues from the restored state.
        if(!hoisted)transition(consumer,source.Get(),state,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        transition(consumer,source.Get(),hoisted?state:D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
        hr(consumer->Close(),"close consumer first");
        // Deliberately record the native writer after the consumer CPU callback.
        const float color[]{value,value,value,value};
        writer->ClearRenderTargetView(rtv->GetCPUDescriptorHandleForHeapStart(),color,0,nullptr);
        if(whole){transition(writer,source.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_DEST);
            if(hoisted)transition(writer,source.Get(),D3D12_RESOURCE_STATE_COPY_DEST,state);}
        else if(hoisted)transition(writer,source.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,state);
        hr(writer->Close(),"close writer last");
        const unsigned queue_index=((frame-1)/8)%2;auto *queue=queues[queue_index].Get();
        if(frame>1&&(frame-1)%8==0){hr(queue->Wait(fence.Get(),signals),"queue chain");++switches;}
        ID3D12CommandList *batch[]{writer,consumer};queue->ExecuteCommandLists(2,batch);hr(queue->Signal(fence.Get(),++signals),"signal");
        require(pins.submitted(consumer,frame,signals),"submitted consumer");HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(event!=nullptr,"event");
        hr(fence->SetEventOnCompletion(signals,event),"fence event");const auto waited=WaitForSingleObject(event,30000);CloseHandle(event);require(waited==WAIT_OBJECT_0,"GPU wait");
        hr(device->GetDeviceRemovedReason(),"device health");pins.collect(fence->GetCompletedValue());require(pins.live()==1,"fence alone cannot retire recording");
        void *mapped{};D3D12_RANGE range{0,static_cast<SIZE_T>(bytes)};hr(readback->Map(0,&range,&mapped),"map");
        for(unsigned y=0;y<64;++y)for(unsigned x=0;x<128;++x){float actual{};std::memcpy(&actual,static_cast<std::byte *>(mapped)+y*footprint.Footprint.RowPitch+x*4,4);require(actual==value,"exact depth pixels");++pixels;}
        D3D12_RANGE none{};readback->Unmap(0,&none);
        hr(allocators[1]->Reset(),"retire allocator");hr(consumer->Reset(allocators[1].Get(),nullptr),"retire consumer");hr(consumer->Close(),"retire close");
        require(pins.reset(consumer,frame),"retire generation");pins.collect(fence->GetCompletedValue());require(!pins.live(),"release after GPU and reset");
    }
    unsigned messages{};for(UINT64 i=0;i<info->GetNumStoredMessages();++i){SIZE_T n{};hr(info->GetMessage(i,nullptr,&n),"message size");std::vector<std::byte> b(n);
        auto *m=reinterpret_cast<D3D12_MESSAGE *>(b.data());hr(info->GetMessage(i,m,&n),"message");if(m->Severity<=D3D12_MESSAGE_SEVERITY_WARNING){std::cerr<<m->pDescription<<'\n';++messages;}}
    require(!messages,"GPU warnings/errors");std::ofstream log(out);log<<"{\"accepted\":true,\"frames\":64,\"pixels\":"<<pixels<<",\"signals\":"<<signals
        <<",\"queue_switches\":"<<switches<<",\"consumer_recorded_first\":true,\"hoisted_frames\":64,\"states\":[4,64,128,192,1024,128],\"whole_copy_kinds\":[4,5],\"frames_per_state\":16,\"adapter_luid\":["<<ad.AdapterLuid.HighPart<<','<<ad.AdapterLuid.LowPart
        <<"],\"vendor\":"<<ad.VendorId<<",\"gpu_warnings_errors\":0,\"retention\":";pins.write(log,fence->GetCompletedValue());log<<"}\n";
    require(bool(log),"report");std::cout<<"PASS 96 reversed-recording frames; 786432 exact depth pixels; six writer/state kinds (two whole-copy); "<<switches<<" queue switches; GPU validation and retirement\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
