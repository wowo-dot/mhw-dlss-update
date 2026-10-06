#include "probe.hpp"
#include "consumer.hpp"
#include "depth_routes.hpp"
#include "owned_inputs.hpp"
#ifdef MHW_TEMPORAL_SCALED
#include "../scaled_temporal/runtime.hpp"
#include "../scaled_temporal/postfilter.hpp"
#endif
#ifdef MHW_TEMPORAL_INLINE
#include "inline_pass.hpp"
#endif
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#ifdef MHW_TEMPORAL_DLSS
#include "../ngx_startup/startup.hpp"
#endif
namespace {
HMODULE module{};
#if defined(MHW_SCALED_OUTPUT)
constexpr const char *version="1.24.5";
constexpr unsigned jitter_writes=mhw::temporal_bindings::stimulus_count,hooks=49;
#elif defined(MHW_TEMPORAL_SCALED)
constexpr const char *version="1.23.12";
constexpr unsigned jitter_writes=mhw::temporal_bindings::stimulus_count,hooks=26;
#elif defined(MHW_TEMPORAL_DLSS)
#ifdef MHW_TEMPORAL_PRESENT_STATE
#ifdef MHW_TEMPORAL_INLINE
constexpr const char *version="1.17.0";
#else
constexpr const char *version="1.16.0";
#endif
constexpr unsigned jitter_writes=mhw::temporal_bindings::stimulus_count,hooks=29;
#else
constexpr const char *version="1.15.3";
constexpr unsigned jitter_writes=mhw::temporal_bindings::stimulus_count,hooks=11;
#endif
#else
constexpr const char *version="1.10.0";
constexpr unsigned jitter_writes=0,hooks=10;
#endif
struct Handle {HANDLE value{};~Handle(){if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
std::wstring wide(const std::string &s) {
  const auto n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
  if(!n)return {};std::wstring result(static_cast<std::size_t>(n),L'\0');
  MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),result.data(),n);return result;
}
std::string quote(const std::string &s) {
  std::ostringstream o;o<<'"';for(unsigned char c:s){if(c=='"' || c=='\\')o<<'\\'<<c;
    else if(c<32)o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<unsigned>(c);else o<<c;}return o.str()+'"';
}
DWORD WINAPI worker(void *) noexcept {
  namespace tb=mhw::temporal_bindings;namespace pb=mhw::projection_binding;
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
    if(!pb::host_identity(base,reason))return 5;
    FILETIME ct{},et{},kt{},ut{};if(!GetProcessTimes(GetCurrentProcess(),&ct,&et,&kt,&ut))return 6;
    const auto creation=(static_cast<std::uint64_t>(ct.dwHighDateTime)<<32)|ct.dwLowDateTime;const auto pid=GetCurrentProcessId();
    const auto name="inputs-"+std::to_string(pid)+"-"+std::to_string(creation);const auto path=output/(name+".jsonl");
    {Handle h{CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};if(h.value==INVALID_HANDLE_VALUE)return 7;}
    std::ofstream log(path,std::ios::binary|std::ios::app);if(!log)return 8;
    log<<std::boolalpha<<"{\"event\":\"start\",\"schema\":1,\"version\":\""<<version<<"\",\"pid\":"<<pid<<",\"creation\":"<<creation<<",\"base\":"<<base
       <<",\"host_sha256\":"<<quote(pb::host_sha)<<",\"host_version\":\"0.0.0.0\",\"dll_sha256\":"<<quote(pb::file_sha256(dll.wstring()))
       <<",\"experiment\":\"ordinary-taa-input-requests\",\"capacity\":"<<tb::capacity<<",\"window_ms\":2000,\"jitter_writes\":"<<jitter_writes<<",\"gpu_commands\":"<<12*tb::capacity<<",\"hooks\":"<<hooks
       <<",\"scene_depth_samples\":"<<tb::capacity<<",\"scene_depth_copies\":"<<tb::capacity<<",\"descriptor_writes\":0"
       <<",\"owned_copy_limit\":"<<3*tb::capacity<<",\"owned_descriptor_writes\":"<<6*tb::capacity<<",\"owned_set_byte_limit\":314572800"
       <<",\"owned_execute_limit\":"<<tb::capacity<<",\"owned_signal_limit\":"<<tb::capacity<<",\"owned_dispatch_limit\":"<<tb::capacity<<",\"owned_readback_limit\":"<<117440512ULL*tb::capacity<<""
       <<",\"depth_copy_limit\":"<<tb::capacity<<",\"depth_byte_limit\":16777216,\"depth_selected_scope\":true"
       <<",\"depth_route_limit\":64,\"depth_route_barrier_limit\":32,\"depth_route_call_limit\":8"
       <<",\"motion_copy_limit\":"<<tb::capacity<<",\"motion_byte_limit\":16777216,\"motion_prefix\":"<<quote(name)
       <<",\"color_copy_limit\":"<<tb::capacity<<",\"color_byte_limit\":16777216,\"total_readback_limit\":"<<50331648ULL*tb::capacity<<""
       <<",\"barrier_limit\":128,\"barrier_call_limit\":16"
       <<",\"warmup_ms\":150,\"event_limit\":4096,\"list_limit\":128,\"allocator_limit\":128,\"queue_limit\":8,\"batch_limit\":128,\"submitted_limit\":8192,\"private_signal_limit\":"<<tb::capacity<<""
       <<",\"shader_limit\":262144,\"owner_limit\":64,\"binding_limit\":128,\"shader_file\":"<<quote(name+".cs.dxbc");
#ifdef MHW_TEMPORAL_DLSS
#ifndef MHW_TEMPORAL_SCALED
    log<<",\"pre_unhook_cutoff\":true,\"journal_drain\":true,\"packet_generations\":true,\"projection_samples\":"<<tb::capacity<<",\"projection_byte_limit\":4096,\"jitter_restorations\":"<<tb::stimulus_count<<",\"jitter_min_abs\":0.3125,\"jitter_selection_limit\":20";
#endif
#ifdef MHW_TEMPORAL_PRESENT_STATE
#ifdef MHW_SCALED_OUTPUT
    log<<",\"binding_observer\":true,\"binding_hooks\":23,\"binding_lists\":128,\"binding_arguments\":64,\"binding_constant_words\":64";
#else
    log<<",\"binding_observer\":true,\"binding_hooks\":18,\"binding_lists\":128,\"binding_arguments\":64,\"binding_constant_words\":64";
#endif
#ifdef MHW_TEMPORAL_INLINE
    log<<",\"presentation_writes\":1,\"inline_receipt\":"<<tb::inline_pass::selected_receipt<<",\"inline_same_list\":true";
#else
    log<<",\"presentation_writes\":0";
#endif
#endif
#ifdef MHW_TEMPORAL_INLINE
    log<<",\"sdk_evaluation_limit\":1,\"sdk_create_limit\":1,\"sdk_image_byte_limit\":419430400,\"sdk_output_limit\":67108864,\"sdk_shutdown_deferred\":true";
#else
    log<<",\"sdk_evaluation_limit\":"<<tb::capacity<<",\"sdk_create_limit\":1,\"sdk_image_byte_limit\":150994944,\"sdk_output_limit\":"<<67108864ULL*tb::capacity<<",\"sdk_shutdown_deferred\":true";
#endif
#endif
#ifdef MHW_TEMPORAL_SCALED
    log<<",\"projection_activation_scope\":true,\"depth_projection_gate\":true";
#ifdef MHW_SCALED_OUTPUT
    log<<",\"full_output_route\":true,\"output_receipt\":2,\"output_draw_limit\":3,\"output_copy_limit\":1,\"native_taa_replacement_limit\":1";
    log<<",\"viewport_hook_owner\":\"binding-observer\",\"resolution_viewport_listener\":true";
    log<<",\"binding_hook_control\":\"owned-targets-only\"";
#endif
    log<<",\"final_handoff_observer\":true,\"final_handoff_samples\":3,\"final_handoff_mutations\":0";
    log<<",\"downstream_observer\":true,\"downstream_samples\":3,\"downstream_packet_limit\":8,\"downstream_filter_limit\":16";
    log<<",\"postfilter_observer\":true,\"postfilter_samples\":3,\"postfilter_command_limit\":32,\"postfilter_shader_limit\":262144,\"postfilter_mutations\":0";
    log<<",\"draw_execution_observer\":true,\"draw_execution_samples\":9,\"draw_execution_shader_limit\":262144";
    log<<",\"colorgrade_observer\":true,\"colorgrade_scan_limit\":65535,\"colorgrade_shader_limit\":262144";
    log<<",\"scaled_temporal\":true,\"depth_draw_capture\":true,\"depth_draw_hooks\":1,\"scale\":0.5,\"pre_unhook_cutoff\":true,\"journal_drain\":true,\"packet_generations\":true,\"color_state_admission\":\"native-current-use-v1\",\"color_state_proof_limit\":3";
#ifndef MHW_SCALED_OUTPUT
    log<<",\"sdk_evaluation_limit\":0";
#endif
    log<<",\"scaled_projection\":true,\"projection_samples\":"<<tb::capacity<<",\"projection_byte_limit\":4096,\"jitter_restorations\":"<<tb::stimulus_count<<",\"jitter_min_abs\":0.3125,\"jitter_selection_limit\":20";
#endif
    log<<"}\n"<<std::flush;
#ifdef MHW_TEMPORAL_DLSS
    const auto sdk_config=output/L"sdk-runtime-dir.txt";
    if(!std::filesystem::exists(sdk_config)||std::filesystem::file_size(sdk_config)>4096)return 13;
    std::ifstream sdk_file(sdk_config,std::ios::binary);std::string sdk_folder;std::getline(sdk_file,sdk_folder);
    if(!sdk_folder.empty()&&sdk_folder.back()=='\r')sdk_folder.pop_back();
    const auto sdk_path=std::filesystem::path(wide(sdk_folder));
    if(!sdk_path.is_absolute()||!std::filesystem::is_directory(sdk_path))return 14;
    tb::owned::configure_sdk(std::filesystem::canonical(sdk_path).wstring());
#endif
    const auto prefix="Local\\MhwTemporalInputs-v1-"+std::to_string(pid)+"-"+std::to_string(creation)+"-";
    Handle stop{CreateEventW(nullptr,FALSE,FALSE,wide(prefix+"0").c_str())};const bool collision=GetLastError()==ERROR_ALREADY_EXISTS;
    Handle arm{CreateEventW(nullptr,FALSE,FALSE,wide(prefix+"1").c_str())};
    if(!stop.value || !arm.value || collision || GetLastError()==ERROR_ALREADY_EXISTS) {log<<"{\"event\":\"failed\",\"reason\":\"event-collision\"}\n";return 9;}
    const bool reserved=pb::reserve_startup(base,reason);
    log<<"{\"event\":\"reservation\",\"ready\":"<<reserved<<",\"slot\":"<<pb::pool_state().slot<<",\"hooks_installed\":false}\n";
    if(!reserved){log<<"{\"event\":\"failed\",\"reason\":"<<quote(reason)<<"}\n"<<std::flush;return 10;}
#ifdef MHW_TEMPORAL_DLSS
    if(!mhw::ngx_startup::run(module,version,true)){
      log<<"{\"event\":\"failed\",\"reason\":\"startup-initialization\"}\n"<<std::flush;return 15;
    }
#endif
    log<<"{\"event\":\"ready\",\"hooks_installed\":false}\n"<<std::flush;
    const HANDLE events[]={stop.value,arm.value};
    const auto request=WaitForMultipleObjects(2,events,FALSE,1800000);
    if(request!=WAIT_OBJECT_0+1){log<<"{\"event\":\"stopped\",\"armed\":false,\"hooks_disabled\":true}\n";return 0;}
#ifdef MHW_TEMPORAL_SCALED
    const bool installed=mhw::scaled_temporal::install(base,reason);
#else
    const bool installed=tb::install(base,reason);
#endif
    log<<"{\"event\":\"armed\",\"installed\":"<<installed<<",\"reason\":"<<quote(reason)<<",\"tick\":"<<GetTickCount64()<<"}\n"<<std::flush;
    if(installed)while(!tb::done()) {
#ifdef MHW_TEMPORAL_SCALED
      if(!mhw::scaled_temporal::healthy()){tb::cancel();break;}
#endif
      const auto w=WaitForMultipleObjects(2,events,FALSE,10);
      if(w!=WAIT_TIMEOUT || !log){tb::cancel();break;}
    }
    const bool capture_cleaned=tb::finish();
#ifdef MHW_TEMPORAL_SCALED
    const bool scale_cleaned=mhw::scaled_temporal::finish();
#else
    constexpr bool scale_cleaned=true;
#endif
    bool accepted=capture_cleaned && scale_cleaned && tb::save_shader((output/(name+".cs.dxbc")).wstring()) && tb::gpu::save_motion((output/name).wstring()) && tb::depth_routes::save_pixels((output/name).wstring()) && tb::owned::convert_save((output/name).wstring());
#ifdef MHW_TEMPORAL_SCALED
    if(accepted)accepted=mhw::scaled_temporal::postfilter::save((output/name).wstring());
#endif
#ifdef MHW_TEMPORAL_DLSS
#ifdef MHW_TEMPORAL_INLINE
    if(accepted)accepted=tb::inline_pass::save((output/name).wstring());
#else
    if(accepted)accepted=tb::owned::evaluate_save((output/name).wstring());
#endif
#endif
    if(tb::settled())tb::write(log);
#ifdef MHW_TEMPORAL_SCALED
    mhw::scaled_temporal::write(log);
#endif
    log<<"{\"event\":\"stopped\",\"armed\":"<<installed<<",\"accepted\":"<<accepted<<",\"settled\":"<<tb::settled()<<",\"dll_pinned\":true,\"trampolines_retained\":true}\n"<<std::flush;
    return accepted?0:11;
  }catch(...){tb::cancel();tb::finish();
#ifdef MHW_TEMPORAL_SCALED
    mhw::scaled_temporal::finish();
#endif
    return 12;}
}
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) {
  if(reason==DLL_PROCESS_ATTACH){module=instance;DisableThreadLibraryCalls(instance);const auto t=CreateThread(nullptr,0,worker,nullptr,0,nullptr);if(t)CloseHandle(t);}
  return TRUE;
}
