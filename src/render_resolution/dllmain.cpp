#include "runtime.hpp"
#include "../projection_binding/recorder.hpp"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cmath>

namespace {
HMODULE module{};
constexpr const char *version="1.22.1";
struct Handle {HANDLE value{};~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
std::wstring wide(const std::string &s){
    const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
    if(!n)return {};std::wstring result(static_cast<std::size_t>(n),L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),result.data(),n);return result;
}
DWORD WINAPI worker(void *) noexcept {
    namespace rr=mhw::render_resolution;namespace pb=mhw::projection_binding;
    HMODULE pinned{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&worker),&pinned))return 1;
    try {
        wchar_t dll_path[32768]{},host_path[32768]{};
        const auto dn=GetModuleFileNameW(module,dll_path,static_cast<DWORD>(std::size(dll_path)));
        const auto hn=GetModuleFileNameW(nullptr,host_path,static_cast<DWORD>(std::size(host_path)));
        if(!dn||dn>=std::size(dll_path)||!hn||hn>=std::size(host_path))return 2;
        const std::filesystem::path dll(dll_path),host(host_path);auto sidecar=dll;sidecar+=L".output-dir";
        if(!std::filesystem::exists(sidecar)||std::filesystem::file_size(sidecar)>4096)return 3;
        std::ifstream config(sidecar,std::ios::binary);std::string folder;std::getline(config,folder);if(!folder.empty()&&folder.back()=='\r')folder.pop_back();
        const auto given=std::filesystem::path(wide(folder));
        if(!given.is_absolute())return 4;
        const auto output=std::filesystem::weakly_canonical(given);
        if(!std::filesystem::is_directory(output)||output==std::filesystem::weakly_canonical(host.parent_path()))return 4;
        const auto scale_path=output/"render-scale.txt";
        if(!std::filesystem::exists(scale_path)||std::filesystem::file_size(scale_path)>32)return 5;
        std::ifstream scale_file(scale_path);float scale{};std::string extra;
        if(!(scale_file>>scale)||(scale_file>>extra)||!std::isfinite(scale)||scale<.5F||scale>=1.F)return 5;
        const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));std::string reason;
        if(!pb::host_identity(base,reason))return 6;
        FILETIME ct{},et{},kt{},ut{};if(!GetProcessTimes(GetCurrentProcess(),&ct,&et,&kt,&ut))return 7;
        const auto creation=(std::uint64_t(ct.dwHighDateTime)<<32)|ct.dwLowDateTime;const auto pid=GetCurrentProcessId();
        const auto name="resolution-"+std::to_string(pid)+"-"+std::to_string(creation);const auto path=output/(name+".jsonl");
        {Handle h{CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};if(h.value==INVALID_HANDLE_VALUE)return 8;}
        std::ofstream log(path,std::ios::binary|std::ios::app);if(!log)return 8;
        log<<std::boolalpha<<"{\"event\":\"start\",\"schema\":1,\"version\":\""<<version<<"\",\"experiment\":\"native-render-resolution\",\"pid\":"<<pid<<",\"creation\":"<<creation<<",\"base\":"<<base
           <<",\"host_sha256\":\""<<pb::host_sha<<"\",\"host_version\":\"0.0.0.0\",\"dll_sha256\":\""<<pb::file_sha256(dll.wstring())
           <<"\",\"graphics_api\":\"D3D12\",\"requested_scale\":"<<scale<<",\"runtime_limit_ms\":300000,\"dlss_evaluation\":false}\n"<<std::flush;
        const auto prefix="Local\\MhwRenderResolution-v1-"+std::to_string(pid)+"-"+std::to_string(creation)+"-";
        Handle stop{CreateEventW(nullptr,FALSE,FALSE,wide(prefix+"0").c_str())};const bool collision=GetLastError()==ERROR_ALREADY_EXISTS;
        Handle arm{CreateEventW(nullptr,FALSE,FALSE,wide(prefix+"1").c_str())};
        if(!stop.value||!arm.value||collision||GetLastError()==ERROR_ALREADY_EXISTS)return 9;
        const bool reserved=pb::reserve_startup(base,reason);
        log<<"{\"event\":\"reservation\",\"ready\":"<<reserved<<",\"slot\":"<<pb::pool_state().slot<<",\"hooks_installed\":false}\n"<<std::flush;
        if(!reserved){log<<"{\"event\":\"failed\",\"reason\":\"reservation\"}\n"<<std::flush;return 10;}
        log<<"{\"event\":\"ready\",\"hooks_installed\":false}\n"<<std::flush;
        const HANDLE events[]{stop.value,arm.value};
        if(WaitForMultipleObjects(2,events,FALSE,1800000)!=WAIT_OBJECT_0+1){log<<"{\"event\":\"stopped\",\"armed\":false}\n"<<std::flush;return 0;}
        const bool installed=rr::install(base,scale,reason);
        log<<"{\"event\":\"armed\",\"installed\":"<<installed<<",\"reason\":"<<std::quoted(reason)<<"}\n"<<std::flush;
        const auto start=GetTickCount64();unsigned samples{};const char *stop_reason="install-or-fault";
        if(installed)while(rr::healthy()){
            if(WaitForSingleObject(stop.value,1000)!=WAIT_TIMEOUT){stop_reason="requested";break;}
            ++samples;
            {std::ofstream status(output/(name+".status.json"),std::ios::binary|std::ios::trunc);rr::write(status);if(!status){stop_reason="status-write";break;}}
            if(samples<=12||(samples&(samples-1))==0)rr::write(log);
            if(!log){stop_reason="log-write";break;}
            if(GetTickCount64()-start>=300000){stop_reason="deadline";break;}
        }
        const bool cleaned=rr::finish();rr::write(log);
        log<<"{\"event\":\"stopped\",\"armed\":"<<installed<<",\"healthy\":"<<rr::healthy()<<",\"cleaned\":"<<cleaned<<",\"reason\":\""<<stop_reason<<"\",\"dll_pinned\":true}\n"<<std::flush;
        return cleaned&&rr::healthy()?0:11;
    }catch(...){mhw::render_resolution::finish();return 12;}
}
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){module=instance;DisableThreadLibraryCalls(instance);const auto thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);if(thread)CloseHandle(thread);}return TRUE;
}
