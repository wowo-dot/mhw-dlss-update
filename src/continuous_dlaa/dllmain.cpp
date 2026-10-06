#include "runtime.hpp"
#include "../ngx_startup/startup.hpp"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace {
HMODULE module{};
#ifdef MHW_CONTINUOUS_SCALED
constexpr const char *version="1.25.44",*experiment="continuous-scaled-native-resolve";
#else
constexpr const char *version="1.21.5",*experiment="native-temporal-resolve";
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
  namespace cd=mhw::continuous_dlaa;namespace pb=mhw::projection_binding;
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
       <<",\"experiment\":"<<quote(experiment)<<",\"graphics_api\":\"D3D12\",\"frame_limit\":null,\"replacement_scope\":\"qualified-native-resolve\",\"exclusive_mode\":false,\"retention\":\"reset-and-fence-v1\",\"request_retirement\":\"validated-native-return-v1\",\"activation\":\"native-postqueue-preparation-v2\",\"reconstruction\":\"suspend-or-latch-unfiltered-v1\",\"state_trace\":\"consumer-local-state-v1\",\"native_state_trace\":\"scheduled-state-evidence-v1\",\"state_admission\":\"native-current-use-v1\",\"copy_telemetry\":\"single-fence-observation-v1\",\"depth_eligibility\":\"depth-eligibility-observation-v1\",\"depth_destination\":\"depth-destination-identity-v1\"}\n"<<std::flush;
    const auto sdk_config=output/L"sdk-runtime-dir.txt";
    if(!std::filesystem::exists(sdk_config)||std::filesystem::file_size(sdk_config)>4096)return 13;
    std::ifstream sdk_file(sdk_config,std::ios::binary);std::string sdk_folder;std::getline(sdk_file,sdk_folder);
    if(!sdk_folder.empty()&&sdk_folder.back()=='\r')sdk_folder.pop_back();
    const auto sdk_path=std::filesystem::path(wide(sdk_folder));
    if(!sdk_path.is_absolute()||!std::filesystem::is_directory(sdk_path))return 14;
    const auto prefix="Local\\MhwTemporalInputs-v1-"+std::to_string(pid)+"-"+std::to_string(creation)+"-";
    Handle stop{CreateEventW(nullptr,FALSE,FALSE,wide(prefix+"0").c_str())};const bool collision=GetLastError()==ERROR_ALREADY_EXISTS;
    Handle arm{CreateEventW(nullptr,FALSE,FALSE,wide(prefix+"1").c_str())};
    if(!stop.value||!arm.value||collision||GetLastError()==ERROR_ALREADY_EXISTS)return 9;
    const bool reserved=pb::reserve_startup(base,reason);
    log<<"{\"event\":\"reservation\",\"ready\":"<<reserved<<",\"slot\":"<<pb::pool_state().slot<<",\"hooks_installed\":false}\n"<<std::flush;
    if(!reserved||!mhw::ngx_startup::run(module,version,true)){log<<"{\"event\":\"failed\",\"reason\":\"startup\"}\n"<<std::flush;return 15;}
    log<<"{\"event\":\"ready\",\"hooks_installed\":false}\n"<<std::flush;
    const HANDLE events[]{stop.value,arm.value};
    if(WaitForMultipleObjects(2,events,FALSE,1800000)!=WAIT_OBJECT_0+1){log<<"{\"event\":\"stopped\",\"armed\":false}\n"<<std::flush;return 0;}
    const bool installed=cd::install(base,sdk_path,reason);
    log<<"{\"event\":\"armed\",\"installed\":"<<installed<<",\"reason\":"<<quote(reason)<<"}\n"<<std::flush;
    cd::write(log);unsigned samples{};
    if(installed)while(cd::healthy()){
        if(WaitForSingleObject(stop.value,5000)!=WAIT_TIMEOUT)break;
        ++samples;
        cd::save_compute_depth(output/name);
        // Constant-size latest status plus logarithmically sampled durable log.
        {std::ofstream status(output/(name+".status.json"),std::ios::binary|std::ios::trunc);cd::write(status);if(!status)break;}
        if(samples<=12||(samples&(samples-1))==0)cd::write(log);
        if(!log)break;
    }
    cd::finish();cd::save_compute_depth(output/name);cd::save_rejected_shader(output/(name+"-rejected-taa.dxbc"));cd::write(log);
    log<<"{\"event\":\"stopped\",\"armed\":"<<installed<<",\"healthy\":"<<cd::healthy()<<",\"dll_pinned\":true}\n"<<std::flush;
    return cd::healthy()?0:11;
  }catch(...){mhw::continuous_dlaa::finish();return 12;}
}
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){module=instance;DisableThreadLibraryCalls(instance);const auto t=CreateThread(nullptr,0,worker,nullptr,0,nullptr);if(t)CloseHandle(t);}return TRUE;
}
