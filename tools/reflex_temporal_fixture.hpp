#pragma once
#include "../src/reflex/presentation.hpp"
#include "../src/reflex/frame_session.hpp"
#include "../src/reflex/driver_status.hpp"
#include <wrl/client.h>
#include <fstream>
#include <iostream>
#include <atomic>
#include <stdexcept>
#include <MinHook.h>
#include <d3d12sdklayers.h>

// Owned hardware only. The existing inline fixture checks reconstructed pixels,
// conversion, downstream bindings and retirement. This adds actual Reflex On
// Sleep/markers and routed native Present to the same reconstruction frames.
class ReflexTemporalFixture {
    using Chain=Microsoft::WRL::ComPtr<IDXGISwapChain3>;
    mhw::reflex::Streamline sdk_;
    mhw::reflex::Presentation routes_{sdk_};
    mhw::reflex::FrameSession frames_{sdk_};
    mhw::reflex::FrameStamp stamp_{};
    std::ofstream log_;
    Chain swap_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> present_queue_;
    Microsoft::WRL::ComPtr<ID3D12Fence> present_fence_;
    Microsoft::WRL::ComPtr<ID3D12InfoQueue1> messages_;
    DWORD message_callback_{};
    std::filesystem::path output_;
    HWND window_{};
    ID3D12Device* device_{};
    unsigned count_{};
    std::atomic_uint debug_failures_{};
    static void WINAPI message(D3D12_MESSAGE_CATEGORY,D3D12_MESSAGE_SEVERITY severity,D3D12_MESSAGE_ID id,const char* description,void* context){
        if(severity<=D3D12_MESSAGE_SEVERITY_WARNING){
            ++static_cast<ReflexTemporalFixture*>(context)->debug_failures_;
            std::cerr<<"DX12 message "<<unsigned(id)<<": "<<description<<'\n';
        }
    }
    static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
    static void api(sl::Result result,const char* why){require(result==sl::Result::eOk,why);}
    static bool present_begin(void* context,std::uintptr_t,unsigned flags) noexcept {
        auto& owner=*static_cast<ReflexTemporalFixture*>(context);
        return !owner.frames_.disabled()&&!(flags&DXGI_PRESENT_TEST)&&owner.frames_.present_begin(GetCurrentThreadId(),owner.stamp_)==sl::Result::eOk;
    }
    static void present_end(void* context,std::uintptr_t,unsigned,HRESULT result) noexcept {
        auto& owner=*static_cast<ReflexTemporalFixture*>(context);
        if(FAILED(result)||owner.frames_.present_end(GetCurrentThreadId(),owner.stamp_)!=sl::Result::eOk)owner.frames_.stop();
    }
public:
    ReflexTemporalFixture(ID3D12Device* device,IDXGIFactory4* factory,
                          const std::filesystem::path& runtime,const std::filesystem::path& output):
        log_(output/L"reflex.json",std::ios::binary),output_(output),device_(device){
        require(bool(log_),"coexistence fixture log");
        require(SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&messages_)))&&
                SUCCEEDED(messages_->RegisterMessageCallback(message,D3D12_MESSAGE_CALLBACK_FLAG_NONE,this,&message_callback_)),"owned debug callback");
        sdk_.initialize(runtime,output/L"reflex-sdk-logs");
        api(sdk_.attach(device),"coexistence SDK attach");
        api(sdk_.options(sl::ReflexMode::eOff),"coexistence initial Off");
        const auto baseline=mhw::reflex::driver_sleep_status(device);
        require(baseline.result==0&&!baseline.low_latency,"coexistence baseline Off");
        routes_.install(device,factory);
        routes_.observe({this,present_begin,present_end});
        api(sdk_.options(sl::ReflexMode::eLowLatency),"coexistence Reflex On");
    }
    void create_swap(ID3D12CommandQueue* queue,IDXGIFactory4* factory){
        present_queue_=queue;
        require(SUCCEEDED(device_->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&present_fence_))),"coexistence presentation fence");
        WNDCLASSW cls{};cls.lpfnWndProc=DefWindowProcW;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"MhwReflexTemporalFixture";
        require(RegisterClassW(&cls)!=0,"coexistence window class");
        window_=CreateWindowExW(0,cls.lpszClassName,L"Owned SDK coexistence",WS_OVERLAPPEDWINDOW,0,0,320,240,nullptr,nullptr,cls.hInstance,nullptr);
        require(window_!=nullptr,"coexistence hidden window");
        DXGI_SWAP_CHAIN_DESC1 desc{};desc.Width=320;desc.Height=240;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count=1;desc.BufferCount=2;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
        Microsoft::WRL::ComPtr<IDXGISwapChain1> swap;
        require(SUCCEEDED(factory->CreateSwapChainForHwnd(queue,window_,&desc,nullptr,nullptr,&swap))&&SUCCEEDED(swap.As(&swap_)),"coexistence native swap");
        const auto stats=routes_.snapshot();
        require(stats.native_device==reinterpret_cast<std::uintptr_t>(device_)&&stats.native_swapchain==reinterpret_cast<std::uintptr_t>(swap_.Get()),"coexistence native interfaces");
    }
    void begin(unsigned frame){
        const auto id=frame+1;api(frames_.begin(GetCurrentThreadId(),{frame,frame,frame&1U,true}),"coexistence Sleep/SimulationStart");
        stamp_={id,id,id&1U,true};api(frames_.render_begin(GetCurrentThreadId(),stamp_),"coexistence render markers");
    }
    void submitted(){
        api(frames_.render_end(GetCurrentThreadId(),stamp_),"coexistence RenderSubmitEnd");
        DXGI_PRESENT_PARAMETERS params{};
        require(SUCCEEDED((count_&1U)?swap_->Present1(0,0,&params):swap_->Present(0,0)),"coexistence native Present");
        api(frames_.end(GetCurrentThreadId()),"coexistence frame completion");++count_;
        require(SUCCEEDED(present_queue_->Signal(present_fence_.Get(),count_)),"coexistence post-Present signal");
    }
    void finish(unsigned evaluations){
        const auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        require(event&&SUCCEEDED(present_fence_->SetEventOnCompletion(count_,event)),"coexistence post-Present completion");
        const auto wait=WaitForSingleObject(event,10000);CloseHandle(event);
        require(wait==WAIT_OBJECT_0&&present_fence_->GetCompletedValue()==count_,"coexistence all presentations completed");
        require(count_==evaluations&&count_>=64&&frames_.completed()==count_&&frames_.tokens()==count_&&frames_.sleeps()==count_&&frames_.markers()==count_*6,"coexistence complete frame accounting");
        frames_.stop();const auto before=routes_.snapshot();
        require(before.presents==count_&&before.native_presents==count_&&!before.faults,"coexistence native/proxy counts");
        // Like the live game's continuing native renderer, submit one actual
        // unmarked Present after frame admission stops. This lets the driver
        // publish its final report without inventing a simulation token.
        require(SUCCEEDED(swap_->Present(0,0))&&SUCCEEDED(present_queue_->Signal(present_fence_.Get(),count_+1)),"coexistence unmarked drain Present");
        const auto drain_event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        require(drain_event&&SUCCEEDED(present_fence_->SetEventOnCompletion(count_+1,drain_event)),"coexistence drain Present completion");
        const auto drain_wait=WaitForSingleObject(drain_event,10000);CloseHandle(drain_event);
        const auto drained=routes_.snapshot();
        require(drain_wait==WAIT_OBJECT_0&&present_fence_->GetCompletedValue()==count_+1&&drained.presents==count_+1&&drained.native_presents==count_+1&&
                frames_.tokens()==count_&&frames_.markers()==count_*6,"coexistence native drain preserves marker counts");
        const auto on=mhw::reflex::driver_sleep_status(device_);require(on.result==0&&on.low_latency,"coexistence effective On");
        sl::ReflexState state{};api(sdk_.state(state),"coexistence driver reports");
        unsigned reports{},queries{};bool reports_valid{};const auto report_deadline=GetTickCount64()+1000;
        do{
            api(sdk_.state(state),"coexistence driver reports");++queries;reports=0;reports_valid=true;unsigned expected=count_-63;
            for(const auto& report:state.frameReport){
                if(!report.simStartTime||!report.simEndTime||!report.renderSubmitStartTime||!report.renderSubmitEndTime||!report.presentStartTime||!report.presentEndTime)continue;
                reports_valid=reports_valid&&report.frameID==expected++&&report.simStartTime<=report.simEndTime&&report.simEndTime<=report.renderSubmitStartTime&&report.renderSubmitStartTime<=report.renderSubmitEndTime&&report.renderSubmitEndTime<=report.presentStartTime&&report.presentStartTime<=report.presentEndTime;++reports;
            }
            reports_valid=reports_valid&&reports==64;
            if(!reports_valid)Sleep(10);
        }while(!reports_valid&&GetTickCount64()<report_deadline);
        {std::ofstream detail(output_/L"driver-reports.jsonl");
            for(const auto& report:state.frameReport)detail<<"{\"frame\":"<<report.frameID<<",\"times\":["<<report.simStartTime<<','<<report.simEndTime<<','<<report.renderSubmitStartTime<<','<<report.renderSubmitEndTime<<','<<report.presentStartTime<<','<<report.presentEndTime<<"]}\n";}
        require(reports_valid,"coexistence final ordered driver token alignment");
        require(routes_.stop(),"coexistence presentation restored");
        api(sdk_.options(sl::ReflexMode::eOff),"coexistence restore Off");
        const auto off=mhw::reflex::wait_driver_off(device_);require(off.status.result==0&&!off.status.low_latency,"coexistence effective Off");
        api(sdk_.shutdown(),"coexistence SDK shutdown");require(routes_.release_after_shutdown(),"coexistence proxy pins released");
        require(SUCCEEDED(swap_->Present(0,0)),"coexistence native Present after shutdown");
        require(SUCCEEDED(present_queue_->Signal(present_fence_.Get(),count_+2)),"coexistence native after-stop signal");
        const auto native_event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        require(native_event&&SUCCEEDED(present_fence_->SetEventOnCompletion(count_+2,native_event)),"coexistence native after-stop completion");
        const auto native_wait=WaitForSingleObject(native_event,10000);CloseHandle(native_event);
        require(native_wait==WAIT_OBJECT_0&&present_fence_->GetCompletedValue()==count_+2,"coexistence final native Present completed");
        swap_.Reset();
        DestroyWindow(window_);window_=nullptr;UnregisterClassW(L"MhwReflexTemporalFixture",GetModuleHandleW(nullptr));
        // Include swap-chain destruction in the GPU validation interval. The
        // subsequent process-exit live-object report is expected because NGX's
        // accepted initialization owner deliberately retains its native device.
        require(debug_failures_.load()==0,"coexistence cleanup DX12 validation");
        require(SUCCEEDED(messages_->UnregisterMessageCallback(message_callback_)),"coexistence debug callback removed");
        log_<<"{\"accepted\":true,\"game_access\":false,\"same_native_device\":true,\"NGX_owner_retained\":true,\"SDK_evaluations\":"<<evaluations<<",\"reflex_frames\":"<<count_
            <<",\"sleep_calls\":"<<frames_.sleeps()<<",\"markers\":"<<frames_.markers()<<",\"driver_reports\":"<<reports<<",\"report_queries\":"<<queries<<",\"routed_presents\":"<<before.presents
            <<",\"native_presents\":"<<before.native_presents<<",\"untracked_routed_presents\":1,\"reflex_off_confirmed\":true,\"SDK_shutdown\":true,\"presentation_hooks_restored\":12,\"native_after_stop\":true,\"post_Present_GPU_completed\":true,\"cleanup_debug_errors_warnings\":0}\n"<<std::flush;
        require(bool(log_),"coexistence result");
    }
};
