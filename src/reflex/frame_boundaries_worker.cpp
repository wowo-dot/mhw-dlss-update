#include "frame_boundaries.hpp"
#include "../projection_binding/recorder.hpp"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace {
HMODULE module{};
struct Handle { HANDLE value{}; ~Handle() { if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value); } };
std::wstring wide(const std::string& s) {
    const auto n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
    if(!n)return {};
    std::wstring w(static_cast<std::size_t>(n),L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),w.data(),n); return w;
}
std::string quote(const std::string& s) {
    std::ostringstream o; o<<'"';
    for(unsigned char c:s) {
        if(c=='"' || c=='\\')o<<'\\'<<c;
        else if(c<32)o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<unsigned>(c);
        else o<<c;
    }
    return o.str()+'"';
}
DWORD WINAPI worker(void*) noexcept {
    namespace b=mhw::reflex::boundaries; namespace pb=mhw::projection_binding;
    HMODULE pin{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(&worker),&pin))return 1;
    try {
        wchar_t path[32768]{};
        const auto n=GetModuleFileNameW(module,path,static_cast<DWORD>(std::size(path)));
        if(!n || n>=std::size(path))return 2;
        const std::filesystem::path dll(path); auto sidecar=dll; sidecar+=L".output-dir";
        if(!std::filesystem::exists(sidecar) || std::filesystem::file_size(sidecar)>4096)return 3;
        std::ifstream config(sidecar,std::ios::binary); std::string text; std::getline(config,text);
        if(!text.empty() && text.back()=='\r')text.pop_back();
        const std::filesystem::path folder(wide(text));
        if(text.empty() || !folder.is_absolute() || !std::filesystem::is_directory(folder))return 4;
        const auto output=std::filesystem::canonical(folder);
        wchar_t host[32768]{}; const auto hn=GetModuleFileNameW(nullptr,host,static_cast<DWORD>(std::size(host)));
        if(!hn || hn>=std::size(host) || output==std::filesystem::canonical(std::filesystem::path(host).parent_path()))return 5;
        const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)); std::string reason;
        if(!pb::host_identity(image,reason))return 6;
        FILETIME ct{},et{},kt{},ut{};
        if(!GetProcessTimes(GetCurrentProcess(),&ct,&et,&kt,&ut))return 7;
        const auto creation=(static_cast<std::uint64_t>(ct.dwHighDateTime)<<32)|ct.dwLowDateTime;
        const auto pid=GetCurrentProcessId();
#ifdef MHW_REFLEX_INPUT_TRACE
        constexpr auto version="0.2.0", experiment="reflex-input-boundaries", stem="reflex-input-";
        constexpr auto control_stem=L"Local\\MhwReflexInput-v2-";
        constexpr unsigned schema=2;
#else
        constexpr auto version="0.1.0", experiment="reflex-frame-boundaries", stem="reflex-boundaries-";
        constexpr auto control_stem=L"Local\\MhwReflexBoundaries-v1-";
        constexpr unsigned schema=1;
#endif
        const auto name=stem+std::to_string(pid)+"-"+std::to_string(creation);
        const auto file=output/(name+".jsonl");
        { Handle h{CreateFileW(file.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};
          if(h.value==INVALID_HANDLE_VALUE)return 8; }
        std::ofstream log(file,std::ios::binary|std::ios::app); if(!log)return 9;
        log<<std::boolalpha<<"{\"event\":\"start\",\"schema\":"<<schema<<",\"version\":"<<quote(version)
            <<",\"experiment\":"<<quote(experiment)<<",\"pid\":"<<pid
            <<",\"creation\":"<<creation<<",\"base\":"<<image<<",\"host_sha256\":"<<quote(pb::host_sha)
            <<",\"host_version\":\"0.0.0.0\",\"dll_sha256\":"<<quote(pb::file_sha256(dll.wstring()))
            <<",\"sdk_loaded\":false,\"reflex_sleep\":false,\"upscaling\":false}\n"<<std::flush;
        const auto prefix=control_stem+std::to_wstring(pid)+L"-"+std::to_wstring(creation)+L"-";
        Handle stop{CreateEventW(nullptr,FALSE,FALSE,(prefix+L"0").c_str())}; bool collision=GetLastError()==ERROR_ALREADY_EXISTS;
        Handle arm{CreateEventW(nullptr,FALSE,FALSE,(prefix+L"1").c_str())}; collision=collision || GetLastError()==ERROR_ALREADY_EXISTS;
        if(!stop.value || !arm.value || collision) { log<<"{\"event\":\"failed\",\"reason\":\"control-event\"}\n"<<std::flush; return 10; }
        if(!b::reserve(image,reason)) { log<<"{\"event\":\"failed\",\"reason\":"<<quote(reason)<<"}\n"<<std::flush; return 11; }
        log<<"{\"event\":\"ready\",\"hooks_installed\":false,\"duration_ms\":"<<b::duration_ms<<"}\n"<<std::flush;
        const HANDLE controls[]{stop.value,arm.value};
        if(WaitForMultipleObjects(2,controls,FALSE,1800000)!=WAIT_OBJECT_0+1) {
            log<<"{\"event\":\"stopped\",\"armed\":false,\"hooks_installed\":false}\n"<<std::flush; return 0;
        }
        const bool installed=b::install(image,reason);
        if(installed) {
            b::describe(log);
            WaitForSingleObject(stop.value,b::duration_ms);
        } else log<<"{\"event\":\"failed\",\"reason\":"<<quote(reason)<<"}\n"<<std::flush;
        const bool cleaned=b::finish(); b::write(log);
        log<<"{\"event\":\"worker_finished\",\"installed\":"<<installed<<",\"cleanup_passed\":"<<cleaned<<"}\n"<<std::flush;
        return installed && cleaned && log ? 0 : 12;
    } catch(...) { b::finish(); return 13; }
}
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {
        module=instance; DisableThreadLibraryCalls(instance);
        const auto thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr); if(thread)CloseHandle(thread);
    }
    return TRUE;
}
