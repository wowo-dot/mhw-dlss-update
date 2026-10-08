#include "temporal_coexistence.hpp"
#include "../continuous_dlaa/runtime.hpp"
#include "../ngx_startup/startup.hpp"
#include "../perf/toggles.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace mhw::reflex::temporal {
namespace {
namespace cd=mhw::continuous_dlaa;
namespace pb=mhw::projection_binding;
#ifdef MHW_CONTINUOUS_SCALED
constexpr const char* temporal_version="1.25.44",*experiment="continuous-scaled-native-resolve";
#else
constexpr const char* temporal_version="1.21.5",*experiment="native-temporal-resolve";
#endif
// Process-lived log owner: no vendor call or resource destruction in DllMain.
auto& events=*new std::ofstream;
std::filesystem::path directory,runtime;
std::string name;
std::uintptr_t image{};
bool prepared{},attempted{},installed{},finished{},clean{};
unsigned samples{};
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
std::string quote(const std::string& value){
    std::ostringstream out;out<<'"';
    for(unsigned char c:value){if(c=='"'||c=='\\')out<<'\\'<<c;
        else if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(c);else out<<c;}
    return out.str()+'"';
}
std::wstring wide(const std::string& value){
    const auto size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);
    require(size>0,"NGX-runtime-UTF8");std::wstring result(static_cast<std::size_t>(size),L'\0');
    require(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),result.data(),size)==size,"NGX-runtime-path");
    return result;
}
}
const char* version() noexcept{return temporal_version;}
void prepare(HMODULE module,const std::filesystem::path& output,std::uintptr_t base,const std::filesystem::path& ngx_runtime){
    require(!events.is_open(),"duplicate-temporal-startup");directory=output;image=base;
    wchar_t path[32768]{};const auto length=GetModuleFileNameW(module,path,static_cast<DWORD>(std::size(path)));
    require(length&&length<std::size(path),"temporal-module-path");
    FILETIME ct{},et{},kt{},ut{};require(GetProcessTimes(GetCurrentProcess(),&ct,&et,&kt,&ut)!=FALSE,"temporal-process-identity");
    const auto creation=(static_cast<std::uint64_t>(ct.dwHighDateTime)<<32)|ct.dwLowDateTime;
    const auto pid=GetCurrentProcessId();name="inputs-"+std::to_string(pid)+"-"+std::to_string(creation);
    const auto file=CreateFileW((directory/(name+".jsonl")).c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    require(file!=INVALID_HANDLE_VALUE,"preserve-temporal-log");CloseHandle(file);
    events.open(directory/(name+".jsonl"),std::ios::binary|std::ios::app);require(bool(events),"temporal-log");
    events<<std::boolalpha<<"{\"event\":\"start\",\"schema\":1,\"version\":\""<<temporal_version<<"\",\"pid\":"<<pid<<",\"creation\":"<<creation<<",\"base\":"<<base
       <<",\"host_sha256\":"<<quote(pb::host_sha)<<",\"host_version\":\"0.0.0.0\",\"dll_sha256\":"<<quote(pb::file_sha256(path))
       <<",\"experiment\":"<<quote(experiment)<<",\"graphics_api\":\"D3D12\",\"frame_limit\":null,\"replacement_scope\":\"qualified-native-resolve\",\"exclusive_mode\":false,\"retention\":\"reset-and-fence-v1\",\"request_retirement\":\"validated-native-return-v1\",\"activation\":\"native-postqueue-preparation-v2\",\"reconstruction\":\"suspend-or-latch-unfiltered-v1\",\"state_trace\":\"consumer-local-state-v1\",\"native_state_trace\":\"scheduled-state-evidence-v1\",\"state_admission\":\"native-current-use-v1\",\"copy_telemetry\":\"single-fence-observation-v1\",\"depth_eligibility\":\"depth-eligibility-observation-v1\",\"depth_destination\":\"depth-destination-identity-v1\"}\n"<<std::flush;
    if(!ngx_runtime.empty())runtime=ngx_runtime;
    else{
        const auto config=directory/L"sdk-runtime-dir.txt";
        require(std::filesystem::file_size(config)<=4096,"bounded-NGX-configuration");
        std::ifstream input(config,std::ios::binary);std::string folder;std::getline(input,folder);
        if(!folder.empty()&&folder.back()=='\r')folder.pop_back();
        runtime=std::filesystem::path(wide(folder));
    }
    require(runtime.is_absolute()&&std::filesystem::is_directory(runtime),"NGX-runtime-directory");
    // The Reflex worker reserved the shared MinHook pool before enabling any
    // API hook. All temporal native targets fit in that pool's reach interval.
    require(pb::pool_state().attempted&&pb::pool_state().slot,"shared-hook-pool");
    events<<"{\"event\":\"reservation\",\"ready\":true,\"slot\":"<<pb::pool_state().slot<<",\"hooks_installed\":false}\n"<<std::flush;
#ifdef MHW_FG_GAME
    // Streamline initializes NGX first when DLSS-G is loaded; both share one runtime folder.
#ifdef MHW_ALPHA
    mhw::ngx_startup::use_paths(directory,runtime);  // mhw_dlss.ini, no research sidecars
#endif
    require(mhw::ngx_startup::run(module,temporal_version,true,true),"coexistence-NGX-startup");
#else
    require(mhw::ngx_startup::run(module,temporal_version,true),"coexistence-NGX-startup");
#endif
    events<<"{\"event\":\"ready\",\"hooks_installed\":false}\n"<<std::flush;prepared=true;
}
bool ready(std::string& reason){
    if(!prepared||attempted){reason="temporal-arm-state";return false;}
    return cd::ready(image,reason);
}
void arm(ID3D12Device* device){
    require(prepared&&!attempted&&device,"temporal-arm-state");
    const auto* owner=mhw::ngx_startup::borrowed_initialization();
    require(owner&&owner->validate&&owner->validate(device,runtime),"same-native-device-NGX-owner");
    attempted=true;std::string reason;installed=cd::install(image,runtime,reason);
    events<<"{\"event\":\"armed\",\"installed\":"<<installed<<",\"reason\":"<<quote(reason)<<"}\n"<<std::flush;
    cd::write(events);require(installed,reason.c_str());require(bool(events),"temporal-arm-log");
}
bool healthy() noexcept{return installed&&!finished&&cd::healthy()&&bool(events);}
bool scene_extent(unsigned &scene_width,unsigned &scene_height,unsigned &window_width,unsigned &window_height) noexcept{
    return installed&&cd::scene_extent(scene_width,scene_height,window_width,window_height);}
void sample(){
    require(healthy(),"temporal-runtime-fault");++samples;
    // perf::quiet_status: the dumps hold the render locks while they format and write.
    if(mhw::perf::on(mhw::perf::quiet_status))return;
    cd::save_compute_depth(directory/name);
    {std::ofstream status(directory/(name+".status.json"),std::ios::binary|std::ios::trunc);cd::write(status);require(bool(status),"temporal-status");}
    if(samples<=12||(samples&(samples-1))==0)cd::write(events);
    require(bool(events),"temporal-heartbeat-log");
}
void summary(std::ostream& out){
    if(!installed){out<<"null";return;}
    cd::write_summary(out);cd::save_inserted_effect(directory/name);
}
void fault_snapshot() noexcept {
    try{
        if(!installed||!events.is_open())return;
        std::ofstream status(directory/(name+".status.json"),std::ios::binary|std::ios::trunc);cd::write(status);
        events<<"{\"event\":\"fault_snapshot\"}\n";cd::write(events);events<<std::flush;
    }catch(...){}
}
bool finish() noexcept {
    if(finished)return clean;
    finished=true;clean=!attempted;
    try{
        if(attempted){
            cd::finish();
            // Keep Reflex presentation routes live until all temporal GPU
            // recordings and callbacks retire. Never infer completion from
            // CPU hook removal or from a visually normal scene.
            const auto deadline=GetTickCount64()+5000;
            while(!cd::quiescent()&&cd::healthy()&&GetTickCount64()<deadline)Sleep(1);
            cd::save_compute_depth(directory/name);
            cd::save_rejected_shader(directory/(name+"-rejected-taa.dxbc"));
            cd::write(events);clean=cd::quiescent();
        }
        if(events.is_open())events<<"{\"event\":\"stopped\",\"armed\":"<<installed<<",\"healthy\":"<<(clean&&(!attempted||cd::healthy()))<<",\"dll_pinned\":true}\n"<<std::flush;
        return clean&&(!events.is_open()||bool(events));
    }catch(...){clean=false;return false;}
}
void receipt(std::ostream& log,const char* phase,ID3D12Device* device){
    log<<"{\"event\":\"temporal_coexistence\",\"phase\":"<<quote(phase)<<",\"version\":"<<quote(temporal_version)
       <<",\"input_log\":"<<quote(name+".jsonl")<<",\"startup_log\":"<<quote("startup-"+name.substr(7)+".jsonl")
       <<",\"device\":"<<reinterpret_cast<std::uintptr_t>(device)<<",\"prepared\":"<<prepared<<",\"installed\":"<<installed
       <<",\"finished\":"<<finished<<",\"quiescent\":"<<clean<<",\"NGX_shutdown_deferred\":true}\n"<<std::flush;
}
}
