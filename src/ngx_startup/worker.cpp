#include "startup.hpp"
#include "readiness.hpp"
#include "../projection_binding/memory.hpp"
#include "../hooking/thread_snapshot.hpp"
#include <windows.h>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <stdexcept>
namespace {
namespace fs=std::filesystem;namespace pb=mhw::projection_binding;namespace startup=mhw::ngx_startup;
void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
// Each startup hook's removal (startup.hpp) and MinHook's thread lists: taken
// with Windows PSS, or with Toolhelp where PSS failed (v0.28.6).
std::ostream &removal(std::ostream &log){
    const auto r=startup::disable_result();const auto threads=al::hooks::thread_snapshot_counts();
    return log<<"\"disable_result\":["<<r[0]<<','<<r[1]<<','<<r[2]<<','<<r[3]<<"],\"thread_lists\":["<<threads.attempts<<','<<threads.succeeded
              <<"],\"hook_service\":\""<<startup::hook_service()<<'"';
}
fs::path preset_output,preset_runtime;  // use_paths() (alpha); empty: research sidecars
startup::Slots slots(std::uintptr_t base) noexcept {
    bool unreadable=false;
    for(unsigned i=0;i<7;++i){std::uintptr_t value{};
        if(!pb::read(base+0x51d8808+8*i,value))unreadable=true;
        else if(value)return startup::Slots::populated;
    }
    return unreadable?startup::Slots::unreadable:startup::Slots::empty;
}
void wait_for_code(std::uintptr_t base,std::ostream &log){
    const auto began=GetTickCount64();unsigned polls{},first_mask{};
    while(true){
        unsigned mask{};
        if(startup::verify_entry(base,0x273f230,64,"bae1e1ebc8439c799b1ab900fe0802e1845f6480160c796b074a32e4b9947e8c",startup::entry_head))mask|=1;
        if(pb::verify_code(base,0x273f270,413,"e74b34a3d4d93abe149584291f6aeb91e6d7ab066bbfca81277ec1b76adeed7d"))mask|=2;
        if(pb::verify_code(base,0x273f40d,23,"8951f13564e74f1eb66250638c6af29c5df54571475b81c3e0a8f2e52e195ba4"))mask|=4;
        if(startup::verify_entry(base,0x23842a0,344,"920c2df8717dcb2cf809d06f7d17c6e75b1d540b3e810e01b25b8a9e57d5ad2c",startup::game_dlss_head))mask|=8;
        if(!polls)first_mask=mask;++polls;
        const auto now=GetTickCount64();const auto slot_state=slots(base);const auto admission=startup::admit(mask,slot_state,now-began);
        if(admission!=startup::Admission::wait){
            log<<"{\"event\":\"code_readiness\",\"started_tick\":"<<began<<",\"tick\":"<<now<<",\"polls\":"<<polls
               <<",\"first_mask\":"<<first_mask<<",\"final_mask\":"<<mask<<",\"slots_empty\":"<<(slot_state==startup::Slots::empty)
               <<",\"timeout_ms\":"<<startup::readiness_timeout_ms<<",\"ready\":"<<(admission==startup::Admission::ready)<<",\"hooks_installed\":false}\n"<<std::flush;
            require(admission!=startup::Admission::missed,"native-init-already-observed");
            require(admission==startup::Admission::ready,"startup-code-timeout");return;
        }
        Sleep(10);
    }
}
fs::path read_path(const fs::path &file){
    require(fs::file_size(file)>0&&fs::file_size(file)<=4096,"config-size");
    std::ifstream in(file,std::ios::binary);std::string value;std::getline(in,value);
    if(!value.empty()&&value.back()=='\r')value.pop_back();
    const auto n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);
    require(n>0,"config-utf8");std::wstring wide(static_cast<std::size_t>(n),L'\0');
    require(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),wide.data(),n)==n,"config-utf8");
    const fs::path p(wide);require(p.is_absolute()&&fs::is_directory(p),"config-directory");return fs::canonical(p).make_preferred();
}
}
void mhw::ngx_startup::use_paths(const fs::path &output,const fs::path &runtime) noexcept {
    try{preset_output=output;preset_runtime=runtime;}catch(...){preset_output.clear();preset_runtime.clear();}
}
bool mhw::ngx_startup::run(HMODULE module,const char *version,bool hook_pool_reserved,bool shared_provider) noexcept {
    HMODULE pinned{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&run),&pinned))return false;
    std::ofstream log;
    try {
        wchar_t dll_name[32768]{},host_name[32768]{};
        require(GetModuleFileNameW(module,dll_name,32768)>0&&GetModuleFileNameW(nullptr,host_name,32768)>0,"module-path");
        // The alpha's paths (use_paths) are absolute and lexically normal, from
        // the DLL's own folder, and stay that way (v0.28.5). Under MO2 a game
        // folder that also exists in overwrite (the log folder's parent, after
        // the first launch) canonicalizes to overwrite's copy, which has no
        // runtime: the provider was "missing" from the second launch on.
        const fs::path dll(dll_name),host(host_name);const auto preset=[](const fs::path &p){require(p.is_absolute()&&fs::is_directory(p),"config-directory");return p.lexically_normal().make_preferred();};
        const auto folder=preset_output.empty()?read_path(fs::path(dll.wstring()+L".output-dir")):preset(preset_output);
        const auto game=preset_output.empty()?fs::canonical(host.parent_path()):host.parent_path().lexically_normal().make_preferred();
        require(_wcsicmp(folder.c_str(),game.c_str())!=0,"output-game-directory");
        const auto runtime=preset_runtime.empty()?read_path(folder/L"sdk-runtime-dir.txt"):preset(preset_runtime);
        const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));std::string reason;
        require(pb::host_identity(base,reason),"host-identity");
        FILETIME ct{},et{},kt{},ut{};require(GetProcessTimes(GetCurrentProcess(),&ct,&et,&kt,&ut)!=0,"process-times");
        const auto creation=(std::uint64_t(ct.dwHighDateTime)<<32)|ct.dwLowDateTime;
        const auto name="startup-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(creation);
        const auto path=folder/(name+".jsonl");
        const auto file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        require(file!=INVALID_HANDLE_VALUE,"preserve-log");require(CloseHandle(file)!=0,"close-log");
        log.open(path,std::ios::binary|std::ios::app);require(bool(log),"open-log");
        log<<std::boolalpha<<"{\"event\":\"start\",\"schema\":1,\"version\":\""<<version<<"\",\"experiment\":\"ngx-startup-capabilities\",\"pid\":"<<GetCurrentProcessId()
           <<",\"creation\":"<<creation<<",\"base\":"<<base<<",\"host_sha256\":\""<<pb::host_sha<<"\",\"host_version\":\"0.0.0.0\",\"dll_sha256\":\""<<pb::file_sha256(dll.wstring())
           <<"\",\"graphics_api\":\"D3D12\",\"swapchain_observed\":false,\"gpu_commands\":0,\"feature_creations\":0,\"hooks\":1}\n"<<std::flush;
        require(reinterpret_cast<std::uintptr_t>(GetProcAddress(GetModuleHandleW(nullptr),"NVSDK_NGX_D3D12_Init"))==base+startup::entry_rva,"client-export");
        wait_for_code(base,log);
        startup::install({reinterpret_cast<void *>(base+startup::entry_rva),base+startup::caller_rva,base+0x51d8808,runtime,folder/(name+".sdk-data"),base+0x51c7050,hook_pool_reserved,shared_provider},log);
        const auto began=GetTickCount64();while(!startup::done()&&GetTickCount64()-began<600000)Sleep(50);
        const bool disabled=startup::disable();const auto end=GetTickCount64();
        while(!startup::settled()&&GetTickCount64()-end<30000)Sleep(10);
        bool graphics{};const auto display_wait=GetTickCount64();
        if(disabled&&startup::settled()&&startup::done())
            while(!(graphics=startup::write_graphics_snapshot())&&GetTickCount64()-display_wait<120000)Sleep(100);
        log<<"{\"event\":\"stopped\",\"transaction_complete\":"<<startup::done()<<",\"capabilities_supported\":"<<startup::supported()
           <<",\"calls\":"<<startup::calls()<<",\"hook_disabled\":"<<disabled<<',';removal(log)<<",\"settled\":"<<startup::settled()
           <<",\"swapchain_observed\":"<<graphics<<",\"game_dlss\":"<<startup::game_dlss()
           <<",\"dll_pinned\":true,\"trampoline_retained\":true,\"sdk_shutdown_deferred\":true,\"sdk_evaluation_verified\":false}\n"<<std::flush;
        // If a callback is still active, the stream must outlive this worker.
        if(!disabled||!startup::settled()){while(true)Sleep(1000);}
        return disabled&&startup::settled()&&startup::done()&&graphics&&startup::borrowed_initialization()!=nullptr;
    }catch(const std::exception &e){
        const bool disabled=startup::disable();
        if(log){log<<"{\"event\":\"failed\",\"reason\":"<<std::quoted(e.what())<<",\"hook_disabled\":"<<disabled<<',';removal(log)<<"}\n"<<std::flush;}
        if(!disabled||!startup::settled()){while(true)Sleep(1000);}
        return false;
    }
}
