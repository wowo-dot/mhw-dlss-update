#include "recorder.hpp"
#include "code_gates.hpp"
#include "provenance.hpp"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace {
HMODULE module{};
struct Handle {HANDLE value{};~Handle(){if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
std::wstring wide(const std::string &s) {
  const auto n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
  if(!n)return {};std::wstring result(static_cast<std::size_t>(n),L'\0');
  MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),result.data(),n);return result;
}
std::string quote(const std::string &s) {
  std::ostringstream o;o<<'"';for(unsigned char c:s) {if(c=='"' || c=='\\')o<<'\\'<<c;
    else if(c<32)o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<unsigned>(c);else o<<c;}return o.str()+'"';
}
DWORD WINAPI worker(void *) noexcept {
  using namespace mhw::projection_binding;
  HMODULE pinned{};
  if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&worker),&pinned))return 1;
  try {
    wchar_t dll_path[32768]{},host_path[32768]{};
    const auto dn=GetModuleFileNameW(module,dll_path,static_cast<DWORD>(std::size(dll_path)));
    const auto hn=GetModuleFileNameW(nullptr,host_path,static_cast<DWORD>(std::size(host_path)));
    if(!dn || dn>=std::size(dll_path) || !hn || hn>=std::size(host_path))return 2;
    const std::filesystem::path dll(dll_path),host(host_path);auto sidecar=dll;sidecar+=L".output-dir";
    if(!std::filesystem::exists(sidecar) || std::filesystem::file_size(sidecar)>4096)return 3;
    std::ifstream config(sidecar,std::ios::binary);std::string folder;std::getline(config,folder);if(!folder.empty() && folder.back()=='\r')folder.pop_back();
    const auto output=std::filesystem::weakly_canonical(std::filesystem::path(wide(folder)));
    if(folder.empty() || !std::filesystem::path(wide(folder)).is_absolute() || !std::filesystem::is_directory(output) || output==std::filesystem::weakly_canonical(host.parent_path()))return 4;
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));std::string reason;
    if(!host_identity(base,reason))return 5;
    FILETIME ct{},et{},kt{},ut{};if(!GetProcessTimes(GetCurrentProcess(),&ct,&et,&kt,&ut))return 6;
    const auto creation=(static_cast<std::uint64_t>(ct.dwHighDateTime)<<32)|ct.dwLowDateTime;const auto pid=GetCurrentProcessId();
    const auto name="binding-"+std::to_string(pid)+"-"+std::to_string(creation);const auto path=output/(name+".jsonl");
    {Handle h{CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};if(h.value==INVALID_HANDLE_VALUE)return 7;}
    std::ofstream log(path,std::ios::binary|std::ios::app);if(!log)return 8;
    log<<std::boolalpha<<"{\"event\":\"start\",\"schema\":1,\"version\":\"0.9.1\",\"pid\":"<<pid<<",\"creation\":"<<creation<<",\"base\":"<<base
      <<",\"host_sha256\":"<<quote(host_sha)<<",\"host_version\":\"0.0.0.0\",\"dll_sha256\":"<<quote(file_sha256(dll.wstring()))
      <<",\"experiment\":\"projection-jitter-depth-sequence\",\"depth_frame_policy\":\"selected-source-request\",\"jitter_writes\":3,\"injection_budget\":3,\"cpu_samples\":5,\"max_draw_waits\":64,\"max_recovery_deferrals\":"<<max_recovery_deferrals
      <<",\"gpu_copy_budget\":5,\"queue_signal_budget\":5,\"window_ms\":"<<window_ms
      <<",\"max_barriers\":"<<max_barriers<<",\"max_resources\":"<<max_resources
      <<",\"max_constants\":"<<max_constants<<",\"max_shader\":"<<max_shader<<",\"max_blobs\":"<<max_blobs<<",\"max_api\":"<<max_api<<"}\n";
    const auto prefix="Local\\MhwProjectionBinding-v1-"+std::to_string(pid)+"-"+std::to_string(creation)+"-";
    Handle stop_event{CreateEventW(nullptr,FALSE,FALSE,wide(prefix+"0").c_str())};const bool collision=GetLastError()==ERROR_ALREADY_EXISTS;
    Handle arm_event{CreateEventW(nullptr,FALSE,FALSE,wide(prefix+"1").c_str())};
    if(!stop_event.value || !arm_event.value || collision || GetLastError()==ERROR_ALREADY_EXISTS) {log<<"{\"event\":\"failed\",\"reason\":\"event-collision\"}\n";return 9;}
    const bool reserved=reserve_startup(base,reason);const auto &pool=pool_state();
    log<<"{\"event\":\"trampoline_reservation\",\"ready\":"<<reserved<<",\"origin\":"<<pool.origin<<",\"slot\":"<<pool.slot<<",\"page\":"<<pool.page
      <<",\"policy_available\":"<<pool.policy_available<<",\"policy_flags\":"<<pool.policy_flags<<",\"status\":"<<pool.status<<",\"win32\":"<<pool.win32
      <<",\"native_targets\":10,\"block_bytes\":4096,\"free_slots\":"<<(reserved?62:0)<<",\"hooks_installed\":false}\n"<<std::flush;
    if(!reserved) {
      log<<"{\"event\":\"failed\",\"reason\":"<<quote(reason)<<"}\n"
         <<"{\"event\":\"stopped\",\"fatal\":true,\"hooks_disabled\":true,\"pending\":0}\n"<<std::flush;return 12;
    }
    const auto watch_deadline=GetTickCount64()+120000;auto watch=provenance::Start::waiting;
    while(watch==provenance::Start::waiting && GetTickCount64()<watch_deadline) {
      watch=provenance::start_watch(base,reason);
      if(watch==provenance::Start::waiting)Sleep(25);
    }
    if(watch!=provenance::Start::active) {
      provenance::stop();provenance::disable();
      log<<"{\"event\":\"failed\",\"reason\":"<<quote(reason)<<"}\n"<<std::flush;return 13;
    }
    provenance::write_ready(log);
    log<<"{\"event\":\"ready\",\"hooks_installed\":true,\"core_hooks_installed\":false,\"pool_ready\":true}\n"<<std::flush;
    const HANDLE events[]={stop_event.value,arm_event.value};bool armed=false,fatal=false;const auto ready=GetTickCount64();
    while(!armed && !fatal && GetTickCount64()-ready<1800000) {
      provenance::poll_watch();if(!provenance::healthy()){reason="provenance-watch-stopped";fatal=true;break;}
      const auto w=WaitForMultipleObjects(2,events,FALSE,50);if(w==WAIT_OBJECT_0)break;
      if(w==WAIT_FAILED){fatal=true;break;}
      if(w==WAIT_OBJECT_0+1) {
        const bool core_ready=log && prepare(base,reason);
        const bool depth_ready=core_ready && provenance::prepare_readback(runtime());
        if(core_ready && !depth_ready)reason="depth-readback-prepare";
        if(!depth_ready || !arm(reason)) {
          const auto i=install_status();log<<"{\"event\":\"failed\",\"reason\":"<<quote(reason)<<",\"stage\":"<<i.stage<<",\"kind\":"<<i.kind<<",\"status\":"<<i.status<<",\"win32\":"<<i.win32<<"}\n"<<std::flush;fatal=true;break;
        }
        const auto &r=runtime();
        log<<"{\"event\":\"armed\",\"graphics_api\":\"D3D12\",\"live_spans\":"<<std::size(code_spans)<<",\"backend\":"<<r.backend<<",\"device\":"<<r.device
         <<",\"swapchain\":"<<r.swapchain<<",\"format\":"<<r.format<<",\"width\":"<<r.width<<",\"height\":"<<r.height<<",\"buffers\":"<<r.buffers
         <<",\"adapter_luid_low\":"<<r.luid_low<<",\"adapter_luid_high\":"<<r.luid_high<<",\"arena_mode\":"<<r.mode
         <<",\"cpu_resource\":"<<r.cpu_resource<<",\"gpu_resource\":"<<r.gpu_resource<<",\"cpu_gpu_base\":"<<r.cpu_gpu_base<<",\"gpu_base\":"<<r.gpu_base
         <<",\"cpu_bytes\":"<<r.cpu_bytes<<",\"gpu_bytes\":"<<r.gpu_bytes<<",\"targets\":[";
        for(unsigned i=0;i<kind_count;++i){if(i)log<<',';log<<r.targets[i];}log<<"]}\n"<<std::flush;armed=true;
      }
    }
    while(armed && status().stop==Stop::recording) {
      const auto w=WaitForMultipleObjects(2,events,FALSE,25);if(w==WAIT_OBJECT_0)stop();
      if(w==WAIT_FAILED || w==WAIT_OBJECT_0+1 || !log){stop();fatal=true;}poll();
    }
    // Freeze the core draw capture but retain the shared Reset/Barrier hooks
    // through its queue signal/completion read or the same two-second bound.
    while(!fatal && status().stop==Stop::complete && provenance::awaiting_submission()) {
      const auto w=WaitForMultipleObjects(2,events,FALSE,10);
      if(w!=WAIT_TIMEOUT || !log){provenance::stop();fatal=true;break;}
      provenance::poll_watch();
    }
    stop();provenance::stop();const auto until=GetTickCount64()+2000;
    while((!settled() || !provenance::settled()) && GetTickCount64()<until)Sleep(10);
    const bool drained=settled() && provenance::settled();
    const bool core_disabled=disable(),watch_disabled=provenance::disable(),disabled=core_disabled && watch_disabled;
    const auto s=status();fatal=fatal || !drained || !disabled || !provenance::healthy() || s.fault!=Fault::none || s.drops!=0;
    if(const auto c=finished_capture())write_capture(log,*c,(output/name).wstring());
    if(drained) {
      const auto pixels=output/(name+"-depth.f32");
      const bool saved=provenance::save_readback(pixels.wstring());
      if(!saved)fatal=true;
      log<<"{\"event\":\"depth_readback\",\"saved\":"<<saved<<",\"file\":"<<quote(name+"-depth.f32")
        <<",\"sha256\":"<<quote(saved?file_sha256(pixels.wstring()):"")<<",\"details\":";
      provenance::write_readback(log);log<<"}\n";
      for(unsigned stage=0;stage<sequence_samples;++stage)if(stage!=1) {
        const auto control_name=name+"-depth-control-"+std::to_string(stage)+".f32";
        const auto control=output/control_name;
        const bool control_saved=provenance::save_readback(control.wstring(),stage);
        if(!control_saved)fatal=true;
        log<<"{\"event\":\"depth_control\",\"stage\":"<<stage<<",\"saved\":"<<control_saved<<",\"file\":"<<quote(control_name)
          <<",\"sha256\":"<<quote(control_saved?file_sha256(control.wstring()):"")<<",\"details\":";
        provenance::write_readback(log,stage);log<<"}\n";
      }
    }
    if(provenance::settled()) {
      const auto evidence=output/(name+"-provenance.json");
      if(std::filesystem::exists(evidence))fatal=true;
      else {
        std::ofstream trace(evidence,std::ios::binary);provenance::write_result(trace);trace.close();
        if(!trace)fatal=true;
        else log<<"{\"event\":\"provenance_saved\",\"file\":"<<quote(name+"-provenance.json")
          <<",\"sha256\":"<<quote(file_sha256(evidence.wstring()))<<"}\n";
      }
    }
    log<<"{\"event\":\"capture_complete\",\"stop\":"<<static_cast<unsigned>(s.stop)<<",\"phase\":"<<static_cast<unsigned>(s.phase)
      <<",\"fault\":"<<static_cast<unsigned>(s.fault)<<",\"pending\":"<<s.pending<<",\"drained\":"<<drained<<",\"drops\":"<<s.drops<<",\"visits\":"<<s.visits
      <<",\"contention_site\":"<<s.contention_site<<",\"contention_thread\":"<<s.contention_thread<<",\"owner_site\":"<<s.owner_site<<",\"owner_thread\":"<<s.owner_thread
      <<",\"gpu_contents_verified\":false,\"submission_verified\":false,\"raster_jitter_verified\":false}\n"
      <<"{\"event\":\"stopped\",\"fatal\":"<<fatal<<",\"hooks_disabled\":"<<disabled<<",\"pending\":"<<s.pending<<",\"trampolines_retained\":true,\"dll_pinned\":true}\n"<<std::flush;
    return fatal || !log?10:0;
  } catch(...) {provenance::stop();provenance::disable();disable();return 11;}
}
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) {
  if(reason==DLL_PROCESS_ATTACH) {module=instance;DisableThreadLibraryCalls(instance);const auto thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);if(thread)CloseHandle(thread);}
  return TRUE;
}
