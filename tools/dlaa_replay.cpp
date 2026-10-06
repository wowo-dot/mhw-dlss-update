// Standalone public-NGX lifecycle test on previously captured private inputs.
// No game process access, graphics hooks, native exports, or redistributable SDK files.
#include "../src/dlss_sdk/contract.hpp"
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <bcrypt.h>
#include <nvsdk_ngx_helpers.h>
#include <array>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace {
namespace fs=std::filesystem;
using Microsoft::WRL::ComPtr;
constexpr char project_id[]="956cdd19-7d3c-4660-b664-91c20d2bc9bd";
constexpr char runtime_sha[]="be6e434a94ca32499515eb62ca0e6c274526055d568d0426e4c652dcdfb6ee6e";
std::atomic_uint log_count{};
void NVSDK_CONV sdk_log(const char *,NVSDK_NGX_Logging_Level,NVSDK_NGX_Feature) {++log_count;}
void require(bool v,const char *reason){if(!v)throw std::runtime_error(reason);}
void hr(HRESULT v,const char *reason){if(FAILED(v))throw std::runtime_error(std::string(reason)+" HRESULT "+std::to_string(static_cast<unsigned>(v)));}
std::string json_string(const char *value) {
    std::ostringstream out;out<<'"';
    for(const unsigned char ch:std::string(value)) {
        if(ch=='"' || ch=='\\')out<<'\\'<<ch;
        else if(ch<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(ch);
        else out<<ch;
    }
    out<<'"';return out.str();
}
std::string hash_file(const fs::path &path) {
    std::ifstream in(path,std::ios::binary);require(bool(in),"hash input");
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};
    require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"SHA provider");
    const auto created=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0);
    if(created<0){BCryptCloseAlgorithmProvider(algorithm,0);throw std::runtime_error("SHA creation");}
    std::array<char,65536> bytes{};std::array<unsigned char,32> digest{};bool ok=true;
    while(in.read(bytes.data(),bytes.size()) || in.gcount())
        if(BCryptHashData(hash,reinterpret_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(in.gcount()),0)<0){ok=false;break;}
    ok=ok && !in.bad() && BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0)>=0;
    BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);require(ok,"SHA read");
    std::ostringstream s;s<<std::hex<<std::setfill('0');for(auto b:digest)s<<std::setw(2)<<unsigned(b);return s.str();
}
struct Context {
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12InfoQueue> messages;
    ComPtr<ID3D12DescriptorHeap> heap,cpu_heap;
    std::array<ComPtr<ID3D12Resource>,3> inputs,uploads;
    ComPtr<ID3D12Resource> output,readback;
    NVSDK_NGX_Parameter *parameters{};
    NVSDK_NGX_Handle *feature{};
    HANDLE event{};
    UINT64 fence_value{};
    bool initialized{},safe{true},sdk_recording{},submitted{};
    ~Context(){if(event)CloseHandle(event);}
};
struct Layout {D3D12_PLACED_SUBRESOURCE_FOOTPRINT placed{};UINT rows{};UINT64 row_bytes{},bytes{};};
void api(std::ostream &o,const char *name,NVSDK_NGX_Result result) {
    o<<"{\"event\":\"api\",\"name\":\""<<name<<"\",\"result\":"<<static_cast<unsigned>(result)<<"}\n"<<std::flush;
    require(NVSDK_NGX_SUCCEED(result),name);
}
D3D12_RESOURCE_DESC texture(unsigned w,unsigned h,DXGI_FORMAT f,bool uav=false) {
    D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=w;d.Height=h;
    d.DepthOrArraySize=d.MipLevels=1;d.Format=f;d.SampleDesc.Count=1;
    d.Flags=uav?D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS:D3D12_RESOURCE_FLAG_NONE;return d;
}
ComPtr<ID3D12Resource> resource(Context &c,const D3D12_RESOURCE_DESC &d,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state) {
    D3D12_HEAP_PROPERTIES p{};p.Type=type;ComPtr<ID3D12Resource> r;
    hr(c.device->CreateCommittedResource(&p,D3D12_HEAP_FLAG_NONE,&d,state,nullptr,IID_PPV_ARGS(&r)),"CreateCommittedResource");return r;
}
ComPtr<ID3D12Resource> buffer(Context &c,UINT64 bytes,bool upload) {
    require(bytes && bytes<=80ULL*1024*1024,"buffer budget");
    D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=1;
    d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return resource(c,d,upload?D3D12_HEAP_TYPE_UPLOAD:D3D12_HEAP_TYPE_READBACK,
        upload?D3D12_RESOURCE_STATE_GENERIC_READ:D3D12_RESOURCE_STATE_COPY_DEST);
}
Layout layout(Context &c,const D3D12_RESOURCE_DESC &d,unsigned components) {
    Layout l;c.device->GetCopyableFootprints(&d,0,1,0,&l.placed,&l.rows,&l.row_bytes,&l.bytes);
    require(l.placed.Offset==0 && l.rows==d.Height && l.row_bytes==d.Width*components*4 &&
        l.placed.Footprint.Format==d.Format && l.placed.Footprint.Width==d.Width && l.placed.Footprint.Height==d.Height &&
        l.placed.Footprint.Depth==1 && l.placed.Footprint.RowPitch==((l.row_bytes+255)&~255ULL) &&
        l.bytes==UINT64(l.placed.Footprint.RowPitch)*(d.Height-1)+l.row_bytes,"copy layout");return l;
}
void barrier(Context &c,ID3D12Resource *r,D3D12_RESOURCE_STATES from,D3D12_RESOURCE_STATES to) {
    D3D12_RESOURCE_BARRIER b{};b.Transition={r,UINT_MAX,from,to};c.list->ResourceBarrier(1,&b);
}
void complete(Context &c,std::ostream &o,const char *phase) {
    c.safe=false;hr(c.list->Close(),"Close private list");
    ID3D12CommandList *lists[]{c.list.Get()};c.submitted=true;c.queue->ExecuteCommandLists(1,lists);
    hr(c.queue->Signal(c.fence.Get(),++c.fence_value),"Signal private completion");
    hr(c.fence->SetEventOnCompletion(c.fence_value,c.event),"private event");
    require(WaitForSingleObject(c.event,15000)==WAIT_OBJECT_0,"private GPU timeout");
    const auto value=c.fence->GetCompletedValue();
    require(value==c.fence_value && value!=UINT64_MAX,"private completion/device removed");
    hr(c.device->GetDeviceRemovedReason(),"device removed");
    hr(c.allocator->Reset(),"retire completed allocator");hr(c.list->Reset(c.allocator.Get(),nullptr),"retire completed list");
    c.safe=true;c.submitted=false;c.sdk_recording=false;
    o<<"{\"event\":\"completed\",\"phase\":\""<<phase<<"\",\"fence_value\":"<<value<<",\"recording_retired\":true}\n"<<std::flush;
}
std::vector<float> read(const fs::path &p,std::size_t count) {
    require(fs::file_size(p)==count*sizeof(float),"input file extent");
    std::vector<float> values(count);std::ifstream in(p,std::ios::binary);
    in.read(reinterpret_cast<char *>(values.data()),static_cast<std::streamsize>(values.size()*4));
    require(bool(in),"input file read");return values;
}
void save(const fs::path &p,const std::vector<float> &data) {
    const auto f=CreateFileW(p.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    require(f!=INVALID_HANDLE_VALUE,"preserve output");DWORD bytes{};
    const auto n=static_cast<DWORD>(data.size()*4);
    const bool wrote=WriteFile(f,data.data(),n,&bytes,nullptr) && bytes==n;
    const bool flushed=FlushFileBuffers(f)!=0,closed=CloseHandle(f)!=0;require(wrote && flushed && closed,"output write");
}
void diagnostics(Context &c,std::ostream &o) {
    unsigned errors{},warnings{};
    for(UINT64 i=0;i<c.messages->GetNumStoredMessagesAllowedByRetrievalFilter();++i) {
        SIZE_T size{};hr(c.messages->GetMessage(i,nullptr,&size),"message size");std::vector<std::byte> data(size);
        auto *m=reinterpret_cast<D3D12_MESSAGE *>(data.data());hr(c.messages->GetMessage(i,m,&size),"message read");
        if(m->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)++errors;
        if(m->Severity==D3D12_MESSAGE_SEVERITY_WARNING)++warnings;
        if(m->Severity<=D3D12_MESSAGE_SEVERITY_WARNING && errors+warnings<=24)
            o<<"{\"event\":\"d3d12_diagnostic\",\"id\":"<<m->ID<<",\"severity\":"<<m->Severity<<",\"message\":"<<json_string(m->pDescription)<<"}\n";
    }
    o<<"{\"event\":\"debug_summary\",\"errors\":"<<errors<<",\"warnings\":"<<warnings<<"}\n"<<std::flush;
    require(errors==0 && warnings==0,"D3D12 debug diagnostics");
}
void cleanup(Context &c,std::ostream &o) {
    require(c.safe && !c.sdk_recording && !c.submitted,"cleanup requires completed retired recording");
    hr(c.list->Close(),"close retired list");
    if(c.feature){api(o,"ReleaseFeature",NVSDK_NGX_D3D12_ReleaseFeature(c.feature));c.feature=nullptr;}
    if(c.parameters){api(o,"DestroyParameters",NVSDK_NGX_D3D12_DestroyParameters(c.parameters));c.parameters=nullptr;}
    if(c.initialized){api(o,"Shutdown1",NVSDK_NGX_D3D12_Shutdown1(c.device.Get()));c.initialized=false;}
}
void run(int argc,wchar_t **argv) {
    require(argc==10,"usage: dlaa_replay SDK_RUNTIME_DIR OUTPUT_DIR LUID_LOW LUID_HIGH WIDTH HEIGHT COLOR_RGBA32 DEPTH_R32 MOTION_RG32");
    const auto sdk=fs::canonical(argv[1]),out=fs::absolute(argv[2]);
    require(!fs::exists(out),"preserve output directory");
    const auto low=static_cast<UINT>(std::stoul(argv[3]));const auto high=static_cast<LONG>(std::stol(argv[4]));
    const auto w=static_cast<unsigned>(std::stoul(argv[5])),h=static_cast<unsigned>(std::stoul(argv[6]));
    require(w>=32 && h>=32 && w<=8192 && h<=8192 && UINT64(w)*h<=4194304,"bounded extent");
    const auto n=std::size_t(w)*h;
    const auto color=read(argv[7],n*4),depth=read(argv[8],n),motion=read(argv[9],n*2);
    std::string reason;
    if(!mhw::dlss_sdk::validate({w,h,color,depth,motion},reason))throw std::runtime_error(reason);
    const std::string expected=runtime_sha;
    require(hash_file(sdk/L"nvngx_dlss.dll")==expected,"pinned signed DLSS runtime hash");
    fs::create_directories(out);fs::create_directory(out/L"ngx");
    std::ofstream log(out/L"events.jsonl",std::ios::binary);require(bool(log),"event log");log<<std::boolalpha;
    auto c=std::make_unique<Context>();
    try {
        ComPtr<ID3D12Debug> debug;hr(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)),"debug layer");debug->EnableDebugLayer();
        ComPtr<ID3D12Debug1> validation;hr(debug.As(&validation),"GPU validation");validation->SetEnableGPUBasedValidation(TRUE);
        ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter1> adapter;
        hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"DXGI factory");
        hr(factory->EnumAdapterByLuid({low,high},IID_PPV_ARGS(&adapter)),"captured adapter");
        DXGI_ADAPTER_DESC1 ad{};hr(adapter->GetDesc1(&ad),"adapter identity");require(ad.VendorId==0x10de,"NVIDIA adapter");
        hr(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&c->device)),"D3D12 device");
        hr(c->device.As(&c->messages),"debug info queue");
        D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
        hr(c->device->CreateCommandQueue(&q,IID_PPV_ARGS(&c->queue)),"private queue");
        hr(c->device->CreateCommandAllocator(q.Type,IID_PPV_ARGS(&c->allocator)),"private allocator");
        hr(c->device->CreateCommandList(0,q.Type,c->allocator.Get(),nullptr,IID_PPV_ARGS(&c->list)),"private list");
        hr(c->device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&c->fence)),"private fence");
        c->event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(c->event!=nullptr,"private event");
        log<<"{\"event\":\"start\",\"version\":1,\"graphics_api\":\"D3D12\",\"width\":"<<w<<",\"height\":"<<h
           <<",\"adapter_luid_low\":"<<low<<",\"adapter_luid_high\":"<<high<<",\"vendor\":"<<ad.VendorId<<",\"device\":"<<ad.DeviceId
           <<",\"sdk_version\":"<<NVSDK_NGX_Version_API<<",\"runtime_sha256\":\""<<expected<<"\",\"project_id\":\""<<project_id
           <<"\",\"input_formats\":[2,41,16],\"output_format\":2,\"reset\":true,\"jitter\":[0,0],\"mv_scale\":[1,1],\"pre_exposure\":1"
           <<",\"auto_exposure\":true,\"depth_inverted\":true,\"mv_low_res\":true,\"mv_jittered\":false,\"is_hdr\":true"
           <<",\"preset\":11,\"mode\":5,\"game_process_access\":false,\"temporal_accumulation_test\":false,\"gpu_validation\":true}\n"<<std::flush;
        const std::array<std::span<const float>,3> planes{color,depth,motion};
        constexpr std::array<DXGI_FORMAT,3> formats{DXGI_FORMAT_R32G32B32A32_FLOAT,DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R32G32_FLOAT};
        constexpr std::array<unsigned,3> components{4,1,2};
        for(unsigned i=0;i<3;++i) {
            const auto d=texture(w,h,formats[i]);const auto l=layout(*c,d,components[i]);
            c->inputs[i]=resource(*c,d,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);c->uploads[i]=buffer(*c,l.bytes,true);
            void *mapped{};D3D12_RANGE none{};hr(c->uploads[i]->Map(0,&none,&mapped),"upload Map");
            for(unsigned y=0;y<h;++y)std::memcpy(static_cast<std::byte *>(mapped)+std::size_t(y)*l.placed.Footprint.RowPitch,
                planes[i].data()+std::size_t(y)*w*components[i],static_cast<std::size_t>(l.row_bytes));
            c->uploads[i]->Unmap(0,nullptr);
            D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=c->uploads[i].Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=l.placed;
            dst.pResource=c->inputs[i].Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            c->list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);barrier(*c,c->inputs[i].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }
        complete(*c,log,"upload");
        const auto d=texture(w,h,DXGI_FORMAT_R32G32B32A32_FLOAT,true);const auto read_layout=layout(*c,d,4);
        c->output=resource(*c,d,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);c->readback=buffer(*c,read_layout.bytes,false);
        D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=1;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        hr(c->device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&c->heap)),"sentinel heap");
        hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        hr(c->device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&c->cpu_heap)),"CPU sentinel heap");
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=d.Format;uav.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;
        c->device->CreateUnorderedAccessView(c->output.Get(),nullptr,&uav,c->heap->GetCPUDescriptorHandleForHeapStart());
        c->device->CreateUnorderedAccessView(c->output.Get(),nullptr,&uav,c->cpu_heap->GetCPUDescriptorHandleForHeapStart());
        const auto data_path=(out/L"ngx").wstring(),sdk_path=sdk.wstring();const wchar_t *search[]{sdk_path.c_str()};
        NVSDK_NGX_FeatureCommonInfo info{};info.PathListInfo={search,1};
        info.LoggingInfo={sdk_log,NVSDK_NGX_LOGGING_LEVEL_ON,true};
        api(log,"Init_with_ProjectID",NVSDK_NGX_D3D12_Init_with_ProjectID(project_id,NVSDK_NGX_ENGINE_TYPE_CUSTOM,"0.1.0",data_path.c_str(),c->device.Get(),&info));
        c->initialized=true;api(log,"GetCapabilityParameters",NVSDK_NGX_D3D12_GetCapabilityParameters(&c->parameters));
        int available{},update{},init_result{};api(log,"SuperSampling.Available",c->parameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available,&available));
        api(log,"NeedsUpdatedDriver",c->parameters->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver,&update));
        api(log,"FeatureInitResult",c->parameters->Get(NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult,&init_result));
        log<<"{\"event\":\"capabilities\",\"available\":"<<available<<",\"needs_driver_update\":"<<update<<",\"feature_init_result\":"<<init_result<<"}\n"<<std::flush;
        require(available==1 && update==0 && static_cast<unsigned>(init_result)==1,"DLSS capability");
        wchar_t loaded[32768]{};const auto module=GetModuleHandleW(L"nvngx_dlss.dll");
        require(module && GetModuleFileNameW(module,loaded,static_cast<DWORD>(std::size(loaded))),"loaded DLSS provider");
        require(fs::equivalent(fs::path(loaded),sdk/L"nvngx_dlss.dll") && hash_file(loaded)==expected,"loaded provider identity");
        log<<"{\"event\":\"provider_verified\",\"runtime_sha256\":\""<<expected<<"\"}\n"<<std::flush;
        NVSDK_NGX_DLSS_Create_Params create{};create.Feature={w,h,w,h,NVSDK_NGX_PerfQuality_Value_DLAA};
        create.InFeatureCreateFlags=NVSDK_NGX_DLSS_Feature_Flags_IsHDR|NVSDK_NGX_DLSS_Feature_Flags_MVLowRes|
            NVSDK_NGX_DLSS_Feature_Flags_DepthInverted|NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;
        c->parameters->Set(NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA,static_cast<unsigned>(NVSDK_NGX_DLSS_Hint_Render_Preset_K));
        c->safe=false;c->sdk_recording=true;
        api(log,"CreateFeature",NGX_D3D12_CREATE_DLSS_EXT(c->list.Get(),1,1,&c->feature,c->parameters,&create));
        require(c->feature!=nullptr,"created handle");complete(*c,log,"create");
        ID3D12DescriptorHeap *heaps[]{c->heap.Get()};c->list->SetDescriptorHeaps(1,heaps);
        const float sentinel[]{65504,65504,65504,65504};
        c->list->ClearUnorderedAccessViewFloat(c->heap->GetGPUDescriptorHandleForHeapStart(),c->cpu_heap->GetCPUDescriptorHandleForHeapStart(),c->output.Get(),sentinel,0,nullptr);
        D3D12_RESOURCE_BARRIER u{};u.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;u.UAV.pResource=c->output.Get();c->list->ResourceBarrier(1,&u);
        NVSDK_NGX_D3D12_DLSS_Eval_Params eval{};eval.Feature.pInColor=c->inputs[0].Get();eval.Feature.pInOutput=c->output.Get();
        eval.pInDepth=c->inputs[1].Get();eval.pInMotionVectors=c->inputs[2].Get();eval.InReset=1;eval.InMVScaleX=eval.InMVScaleY=1;
        eval.InPreExposure=eval.InExposureScale=1;eval.InRenderSubrectDimensions={w,h};eval.InFrameTimeDeltaInMsec=16.666666F;
        c->safe=false;c->sdk_recording=true;
        api(log,"EvaluateFeature",NGX_D3D12_EVALUATE_DLSS_EXT(c->list.Get(),c->feature,c->parameters,&eval));
        barrier(*c,c->output.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=c->output.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.pResource=c->readback.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=read_layout.placed;
        c->list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);complete(*c,log,"evaluate");
        diagnostics(*c,log);
        std::vector<float> output(n*4);void *mapped{};D3D12_RANGE range{0,static_cast<SIZE_T>(read_layout.bytes)};
        hr(c->readback->Map(0,&range,&mapped),"output Map");
        for(unsigned y=0;y<h;++y)std::memcpy(output.data()+std::size_t(y)*w*4,
            static_cast<std::byte *>(mapped)+std::size_t(y)*read_layout.placed.Footprint.RowPitch,std::size_t(w)*16);
        D3D12_RANGE none{};c->readback->Unmap(0,&none);
        std::size_t changed{};double absolute_error{};
        for(std::size_t i=0;i<n;++i)for(unsigned k=0;k<3;++k){
            const auto value=output[i*4+k];require(std::isfinite(value) && value!=65504,"unwritten/nonfinite output RGB");
            if(value!=color[i*4+k])++changed;absolute_error+=std::abs(double(value)-color[i*4+k]);
        }
        require(changed>n,"DLAA output indistinguishable from input control");
        save(out/L"output.rgba32f",output);cleanup(*c,log);
        log<<"{\"event\":\"success\",\"changed_rgb_values\":"<<changed<<",\"rgb_mean_absolute_difference\":"<<absolute_error/double(n*3)
           <<",\"output_sha256\":\""<<hash_file(out/L"output.rgba32f")<<"\",\"output_bytes\":"<<output.size()*4
           <<",\"sdk_log_messages\":"<<log_count.load()<<",\"cleanup_complete\":true,\"offscreen_sdk_evaluation_verified\":true"
           <<",\"temporal_quality_verified\":false,\"game_integration_verified\":false}\n"<<std::flush;
    }catch(const std::exception &e){
        if(c->messages)try{diagnostics(*c,log);}catch(const std::exception &){}
        log<<"{\"event\":\"failure\",\"reason\":"<<json_string(e.what())<<",\"quarantined\":true}\n"<<std::flush;
        // Conservative standalone failure policy: retain SDK and D3D objects
        // until process exit, including unsubmitted partial SDK recordings.
        static_cast<void>(c.release());throw;
    }
}
}
int wmain(int argc,wchar_t **argv){try{run(argc,argv);return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
