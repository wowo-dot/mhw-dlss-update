#include "live.hpp"
#include <windows.h>
#include <wrl/client.h>
#include <bcrypt.h>
#include <nvsdk_ngx_helpers.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>

namespace mhw::dlss_sdk {
namespace {
namespace fs=std::filesystem;
using Microsoft::WRL::ComPtr;
constexpr char runtime_sha[]="be6e434a94ca32499515eb62ca0e6c274526055d568d0426e4c652dcdfb6ee6e";
constexpr char project[]="956cdd19-7d3c-4660-b664-91c20d2bc9bd";
std::atomic_flag used=ATOMIC_FLAG_INIT;
std::atomic_uint messages{};
void NVSDK_CONV logger(const char *,NVSDK_NGX_Logging_Level,NVSDK_NGX_Feature){++messages;}
void require(bool v,const char *why){if(!v)throw std::runtime_error(why);}
void hr(HRESULT v,const char *why){if(FAILED(v))throw std::runtime_error(std::string(why)+" HRESULT "+std::to_string(static_cast<unsigned>(v)));}
std::string quote(const char *s){std::ostringstream o;o<<'"';for(unsigned char c:std::string(s)){
    if(c=='"'||c=='\\')o<<'\\'<<c;else if(c<32)o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(c);else o<<c;}return o.str()+'"';}
std::string hash(const fs::path &path){
    std::ifstream in(path,std::ios::binary);require(bool(in),"SDK hash input");
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE handle{};
    require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"SDK hash provider");
    if(BCryptCreateHash(algorithm,&handle,nullptr,0,nullptr,0,0)<0){BCryptCloseAlgorithmProvider(algorithm,0);throw std::runtime_error("SDK hash handle");}
    std::array<char,65536> bytes{};std::array<unsigned char,32> digest{};bool ok=true;
    while(in.read(bytes.data(),bytes.size())||in.gcount())if(BCryptHashData(handle,reinterpret_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(in.gcount()),0)<0){ok=false;break;}
    ok=ok&&!in.bad()&&BCryptFinishHash(handle,digest.data(),static_cast<ULONG>(digest.size()),0)>=0;
    BCryptDestroyHash(handle);BCryptCloseAlgorithmProvider(algorithm,0);require(ok,"SDK hash read");
    std::ostringstream o;o<<std::hex<<std::setfill('0');for(auto b:digest)o<<std::setw(2)<<unsigned(b);return o.str();
}
fs::path module_path(HMODULE module){wchar_t p[32768]{};const auto n=GetModuleFileNameW(module,p,static_cast<DWORD>(std::size(p)));
    require(n&&n<std::size(p),"SDK module path");return fs::canonical(p);}
struct Context {
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12DescriptorHeap> gpu_heap,cpu_heap;
    std::array<std::array<ComPtr<ID3D12Resource>,3>,max_frames> inputs;
    std::array<ComPtr<ID3D12Fence>,max_frames> producers;
    ComPtr<ID3D12Resource> output,readback;
    NVSDK_NGX_Parameter *parameters{};NVSDK_NGX_Handle *feature{};
    HANDLE event{};UINT64 value{};bool retired{},initialized{};
    ~Context(){if(event)CloseHandle(event);}
};
void api(std::ostream &o,const char *name,NVSDK_NGX_Result r){
    o<<"{\"event\":\"api\",\"name\":\""<<name<<"\",\"result\":"<<static_cast<unsigned>(r)<<"}\n"<<std::flush;
    require(r==NVSDK_NGX_Result_Success,name);
}
void capability(std::ostream &o,NVSDK_NGX_Parameter *parameters,const char *name,const char *key,int &value){
    const auto r=parameters->Get(key,&value);
    o<<"{\"event\":\"api\",\"name\":\""<<name<<"\",\"result\":"<<static_cast<unsigned>(r)
     <<",\"value\":"<<value<<",\"value_u32\":"<<static_cast<unsigned>(value)<<"}\n"<<std::flush;
    require(r==NVSDK_NGX_Result_Success,name);
}
void transition(Context &c,ID3D12Resource *r,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after){
    D3D12_RESOURCE_BARRIER b{};b.Transition={r,UINT_MAX,before,after};c.list->ResourceBarrier(1,&b);
}
void complete(Context &c,std::ostream &o,const char *phase){
    c.retired=false;hr(c.list->Close(),"SDK close");ID3D12CommandList *lists[]{c.list.Get()};
    c.queue->ExecuteCommandLists(1,lists);hr(c.queue->Signal(c.fence.Get(),++c.value),"SDK signal");
    hr(c.fence->SetEventOnCompletion(c.value,c.event),"SDK completion event");
    require(WaitForSingleObject(c.event,15000)==WAIT_OBJECT_0,"SDK completion timeout");
    require(c.fence->GetCompletedValue()==c.value,"SDK fence/device removal");hr(c.device->GetDeviceRemovedReason(),"SDK device removal");
    hr(c.allocator->Reset(),"SDK allocator retirement");hr(c.list->Reset(c.allocator.Get(),nullptr),"SDK list retirement");c.retired=true;
    o<<"{\"event\":\"completed\",\"phase\":\""<<phase<<"\",\"fence_value\":"<<c.value<<",\"recording_retired\":true}\n"<<std::flush;
}
void same_device(ID3D12DeviceChild *object,ID3D12Device *expected){
    ComPtr<ID3D12Device> actual;hr(object->GetDevice(IID_PPV_ARGS(&actual)),"SDK input device");
    ComPtr<IUnknown> a,b;hr(actual.As(&a),"SDK canonical device");hr(expected->QueryInterface(IID_PPV_ARGS(&b)),"SDK expected device");
    require(a.Get()==b.Get(),"SDK foreign input device");
}
}

LiveResult evaluate_bounded(ID3D12Device *device,std::span<const GpuFrame> frames,
    const fs::path &runtime,const fs::path &data,bool borrowed,std::ostream &log,const BorrowedInitialization *existing,std::span<const HistoryFrame> history){
    require(!used.test_and_set(),"SDK bounded run is one-shot");
    auto c=std::make_unique<Context>();LiveResult result;
    try {
        const bool temporal=!history.empty();
        require(valid_frame_count(frames.size()),"SDK frame count");
        require((frames.size()==3||temporal)&&(!temporal||(existing&&history.size()==frames.size()&&valid_history(history))),"SDK history plan");
        require(device && device->GetNodeCount()==1,"SDK single-node device");c->device=device;
        const auto w=frames[0].width,h=frames[0].height;
        require(w>=32&&h>=32&&w<=8192&&h<=8192&&UINT64(w)*h<=4194304,"SDK extent");
        const auto provider=fs::canonical(runtime/L"nvngx_dlss.dll").make_preferred();
        require(hash(provider)==runtime_sha,"SDK pinned provider");
        const auto retained=existing&&existing->validate?existing->validate(device,runtime):nullptr;
        require(!existing||(borrowed&&retained&&GetModuleHandleW(provider.c_str())==retained),"SDK startup ownership");
        require(existing||!GetModuleHandleW(provider.c_str()),"SDK provider already in use");
        const auto observed=GetModuleHandleW(L"nvngx_dlss.dll");
        const auto old=observed!=retained?observed:nullptr;
        const auto old_path=old?module_path(old):fs::path{};
        const auto old_hash=old?hash(old_path):std::string{};
        fs::create_directory(data);
        const auto luid=device->GetAdapterLuid();
        log<<std::boolalpha<<"{\"event\":\"sdk_start\",\"version\":"<<(frames.size()==9?5:temporal?4:existing?3:2)<<",\"borrowed_device\":"<<borrowed
           <<",\"device\":"<<reinterpret_cast<std::uintptr_t>(device)<<",\"adapter_luid_low\":"<<luid.LowPart<<",\"adapter_luid_high\":"<<luid.HighPart
           <<",\"width\":"<<w<<",\"height\":"<<h<<",\"api\":21,\"runtime_sha256\":\""<<runtime_sha<<"\",\"project_id\":\""<<project
           <<"\",\"legacy_present\":"<<(old!=nullptr)<<",\"legacy_sha256\":\""<<old_hash<<"\",\"formats\":[2,41,16],\"output_format\":2"
           <<",\"create_flags\":75,\"mode\":5,\"preset\":11,\"reset_every_frame\":"<<!temporal<<",\"jitter\":"<<(temporal?"null":"[0,0]")<<",\"mv_scale\":[1,1],\"pre_exposure\":1"
           <<",\"frame_count\":"<<frames.size()<<",\"device_debug_settings_changed\":false,\"native_bindings_changed\":false,\"presentation_changed\":false}\n"<<std::flush;
        constexpr std::array<DXGI_FORMAT,3> formats{DXGI_FORMAT_R32G32B32A32_FLOAT,DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R32G32_FLOAT};
        std::set<ID3D12Resource *> unique;
        for(unsigned i=0;i<frames.size();++i){const auto &f=frames[i];
            require(f.width==w&&f.height==h&&f.producer_retired&&f.producer_fence&&f.producer_value==1,"SDK producer receipt");
            same_device(f.producer_fence,device);const auto value=f.producer_fence->GetCompletedValue();
            require(value==f.producer_value,"SDK producer not complete/device removed");c->producers[i]=f.producer_fence;
            for(unsigned j=0;j<3;++j){auto *r=f.inputs[j];require(r&&unique.insert(r).second,"SDK input alias/null");same_device(r,device);
                const auto d=r->GetDesc();require(d.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D&&d.Width==w&&d.Height==h&&d.Format==formats[j]&&
                    d.DepthOrArraySize==1&&d.MipLevels==1&&d.SampleDesc.Count==1&&d.SampleDesc.Quality==0&&d.Flags==D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,"SDK input layout");
                c->inputs[i][j]=r;
            }
            // Validate the entire set before emitting a JSON record, so a
            // rejected resource cannot leave a partial line in the failure log.
            log<<"{\"event\":\"sdk_input\",\"receipt\":"<<i<<",\"producer_fence\":"<<reinterpret_cast<std::uintptr_t>(f.producer_fence)
               <<",\"producer_value\":"<<value<<",\"producer_retired\":true,\"inputs\":[";
            for(unsigned j=0;j<3;++j){if(j)log<<',';log<<reinterpret_cast<std::uintptr_t>(f.inputs[j]);}
            log<<"],\"input_state\":2048}\n"<<std::flush;
        }
        D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
        hr(device->CreateCommandQueue(&q,IID_PPV_ARGS(&c->queue)),"SDK private queue");
        hr(device->CreateCommandAllocator(q.Type,IID_PPV_ARGS(&c->allocator)),"SDK allocator");
        hr(device->CreateCommandList(0,q.Type,c->allocator.Get(),nullptr,IID_PPV_ARGS(&c->list)),"SDK list");
        hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&c->fence)),"SDK fence");
        c->event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(c->event!=nullptr,"SDK event");
        D3D12_RESOURCE_DESC out{};out.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;out.Width=w;out.Height=h;
        out.DepthOrArraySize=out.MipLevels=1;out.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;out.SampleDesc.Count=1;out.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT rows{};UINT64 row_bytes{},bytes{};
        device->GetCopyableFootprints(&out,0,1,0,&footprint,&rows,&row_bytes,&bytes);
        require(footprint.Offset==0&&rows==h&&row_bytes==UINT64(w)*16&&footprint.Footprint.RowPitch==((row_bytes+255)&~255ULL)&&
            bytes==UINT64(footprint.Footprint.RowPitch)*(h-1)+row_bytes&&bytes<=80ULL*1024*1024,"SDK output layout/budget");
        D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
        hr(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&out,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&c->output)),"SDK output");
        const auto allocation=device->GetResourceAllocationInfo(0,1,&out).SizeInBytes+bytes;
        require(allocation<=144ULL*1024*1024,"SDK private image budget");
        D3D12_RESOURCE_DESC rb{};rb.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;rb.Width=bytes;rb.Height=1;rb.DepthOrArraySize=rb.MipLevels=1;
        rb.SampleDesc.Count=1;rb.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;heap.Type=D3D12_HEAP_TYPE_READBACK;
        hr(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&rb,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&c->readback)),"SDK readback");
        D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=1;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&c->gpu_heap)),"SDK GPU clear view");hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&c->cpu_heap)),"SDK CPU clear view");
        D3D12_UNORDERED_ACCESS_VIEW_DESC view{};view.Format=out.Format;view.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(c->output.Get(),nullptr,&view,c->gpu_heap->GetCPUDescriptorHandleForHeapStart());
        device->CreateUnorderedAccessView(c->output.Get(),nullptr,&view,c->cpu_heap->GetCPUDescriptorHandleForHeapStart());
        log<<"{\"event\":\"sdk_objects\",\"objects\":["<<reinterpret_cast<std::uintptr_t>(c->queue.Get())<<','<<reinterpret_cast<std::uintptr_t>(c->allocator.Get())
           <<','<<reinterpret_cast<std::uintptr_t>(c->list.Get())<<','<<reinterpret_cast<std::uintptr_t>(c->fence.Get())<<','<<reinterpret_cast<std::uintptr_t>(c->gpu_heap.Get())
           <<','<<reinterpret_cast<std::uintptr_t>(c->cpu_heap.Get())<<','<<reinterpret_cast<std::uintptr_t>(c->output.Get())<<','<<reinterpret_cast<std::uintptr_t>(c->readback.Get())
           <<"],\"allocation_bytes\":"<<allocation<<"}\n"<<std::flush;
        const auto search_path=provider.parent_path().wstring();const wchar_t *search[]{search_path.c_str()};
        NVSDK_NGX_FeatureCommonInfo info{};info.PathListInfo={search,1};info.LoggingInfo={logger,NVSDK_NGX_LOGGING_LEVEL_ON,true};
        if(existing){
            require(existing->validate(device,runtime)==retained,"SDK startup ownership changed");
            log<<"{\"event\":\"sdk_reuse\",\"device\":"<<reinterpret_cast<std::uintptr_t>(device)
               <<",\"module\":"<<reinterpret_cast<std::uintptr_t>(retained)<<",\"ownership_verified\":true,\"initialization_calls\":0}\n"<<std::flush;
        }else api(log,"Init_with_ProjectID",NVSDK_NGX_D3D12_Init_with_ProjectID(project,NVSDK_NGX_ENGINE_TYPE_CUSTOM,"0.1.0",data.c_str(),device,&info));
        c->initialized=true;
        api(log,"GetCapabilityParameters",NVSDK_NGX_D3D12_GetCapabilityParameters(&c->parameters));
        int available{},update{},init{};
        capability(log,c->parameters,"Available",NVSDK_NGX_Parameter_SuperSampling_Available,available);
        capability(log,c->parameters,"NeedsUpdatedDriver",NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver,update);
        capability(log,c->parameters,"FeatureInitResult",NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult,init);
        require(available==1&&update==0&&init==1,"SDK capability");
        HMODULE loaded{};require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN,provider.c_str(),&loaded)!=0,"SDK explicit provider module");
        require(fs::equivalent(module_path(loaded),provider)&&hash(module_path(loaded))==runtime_sha&&loaded!=old,"SDK loaded provider identity");
        log<<"{\"event\":\"sdk_provider\",\"verified\":true,\"distinct_from_legacy\":true,\"module\":"<<reinterpret_cast<std::uintptr_t>(loaded)<<"}\n"<<std::flush;
        NVSDK_NGX_DLSS_Create_Params create{};create.Feature={w,h,w,h,NVSDK_NGX_PerfQuality_Value_DLAA};
        create.InFeatureCreateFlags=NVSDK_NGX_DLSS_Feature_Flags_IsHDR|NVSDK_NGX_DLSS_Feature_Flags_MVLowRes|NVSDK_NGX_DLSS_Feature_Flags_DepthInverted|NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;
        require(create.InFeatureCreateFlags==75,"SDK flags");
        c->parameters->Set(NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA,static_cast<unsigned>(NVSDK_NGX_DLSS_Hint_Render_Preset_K));
        api(log,"CreateFeature",NGX_D3D12_CREATE_DLSS_EXT(c->list.Get(),1,1,&c->feature,c->parameters,&create));require(c->feature,"SDK feature handle");complete(*c,log,"create");
        for(unsigned i=0;i<frames.size();++i){
            c->retired=false;
            for(auto &r:c->inputs[i])transition(*c,r.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            if(i)transition(*c,c->output.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            ID3D12DescriptorHeap *heaps[]{c->gpu_heap.Get()};c->list->SetDescriptorHeaps(1,heaps);const float sentinel[]{65504,65504,65504,65504};
            c->list->ClearUnorderedAccessViewFloat(c->gpu_heap->GetGPUDescriptorHandleForHeapStart(),c->cpu_heap->GetCPUDescriptorHandleForHeapStart(),c->output.Get(),sentinel,0,nullptr);
            D3D12_RESOURCE_BARRIER u{};u.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;u.UAV.pResource=c->output.Get();c->list->ResourceBarrier(1,&u);
            NVSDK_NGX_D3D12_DLSS_Eval_Params eval{};eval.Feature.pInColor=c->inputs[i][0].Get();eval.Feature.pInOutput=c->output.Get();
            eval.pInDepth=c->inputs[i][1].Get();eval.pInMotionVectors=c->inputs[i][2].Get();eval.InReset=temporal?(i==0):1;eval.InMVScaleX=eval.InMVScaleY=1;
            eval.InPreExposure=eval.InExposureScale=1;eval.InRenderSubrectDimensions={w,h};eval.InFrameTimeDeltaInMsec=diagnostic_frame_ms;
            if(temporal){const auto &p=history[i];eval.InJitterOffsetX=p.jitter_x;eval.InJitterOffsetY=p.jitter_y;
                log<<std::setprecision(9)<<"{\"event\":\"sdk_temporal\",\"receipt\":"<<i<<",\"frame\":"<<p.frame<<",\"epoch\":"<<p.epoch
                   <<",\"reset\":"<<(eval.InReset!=0)<<",\"jitter\":["<<p.jitter_x<<','<<p.jitter_y<<"],\"frame_ms\":"<<diagnostic_frame_ms
                   <<",\"time_policy\":\"fixed-diagnostic\",\"mv_jittered\":false}\n"<<std::flush;
            }
            api(log,"EvaluateFeature",NGX_D3D12_EVALUATE_DLSS_EXT(c->list.Get(),c->feature,c->parameters,&eval));
            for(auto &r:c->inputs[i])transition(*c,r.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE);
            transition(*c,c->output.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
            D3D12_TEXTURE_COPY_LOCATION from{},to{};from.pResource=c->output.Get();from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            to.pResource=c->readback.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=footprint;
            c->list->CopyTextureRegion(&to,0,0,0,&from,nullptr);complete(*c,log,"evaluate");
            auto &pixels=result.output[i];pixels.resize(std::size_t(w)*h*4);void *mapped{};D3D12_RANGE read{0,static_cast<SIZE_T>(bytes)};
            hr(c->readback->Map(0,&read,&mapped),"SDK map");
            for(unsigned y=0;y<h;++y)std::memcpy(pixels.data()+std::size_t(y)*w*4,static_cast<std::byte *>(mapped)+std::size_t(y)*footprint.Footprint.RowPitch,std::size_t(w)*16);
            D3D12_RANGE none{};c->readback->Unmap(0,&none);
            for(std::size_t n=0;n<pixels.size();n+=4)for(unsigned j=0;j<3;++j)require(std::isfinite(pixels[n+j])&&pixels[n+j]!=65504,"SDK output incomplete/nonfinite");
            log<<"{\"event\":\"sdk_output\",\"receipt\":"<<i<<",\"bytes\":"<<pixels.size()*4<<",\"inputs_restored\":true,\"rgb_finite\":true,\"rgb_written\":true}\n"<<std::flush;
        }
        require(c->retired,"SDK cleanup retirement");hr(c->list->Close(),"SDK close retired list");
        api(log,"ReleaseFeature",NVSDK_NGX_D3D12_ReleaseFeature(c->feature));c->feature=nullptr;result.feature_released=true;
        api(log,"DestroyParameters",NVSDK_NGX_D3D12_DestroyParameters(c->parameters));c->parameters=nullptr;result.parameters_destroyed=true;
        if(!borrowed){api(log,"Shutdown1",NVSDK_NGX_D3D12_Shutdown1(device));c->initialized=false;}
        if(old)require(GetModuleHandleW(old_path.c_str())==old&&hash(module_path(old))==old_hash,"SDK legacy module changed");
        result.shutdown_deferred=borrowed;result.complete=true;
        log<<"{\"event\":\"sdk_success\",\"evaluations\":"<<frames.size()<<",\"feature_released\":true,\"parameters_destroyed\":true,\"shutdown_deferred\":"<<borrowed
           <<",\"context_retained\":"<<borrowed<<",\"legacy_unchanged\":true,\"log_messages\":"<<messages.load()<<",\"temporal_quality_verified\":false}\n"<<std::flush;
        if(borrowed)static_cast<void>(c.release()); // pinned diagnostic: retain SDK/device/private resources until process exit
        return result;
    }catch(const std::exception &e){
        log<<"{\"event\":\"sdk_failure\",\"reason\":"<<quote(e.what())<<",\"log_messages\":"<<messages.load()<<",\"context_retained\":true}\n"<<std::flush;
        static_cast<void>(c.release());throw; // retain partial recordings and every input reference on failure
    }
}
}
