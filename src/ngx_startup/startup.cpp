#include "startup.hpp"
#include "../projection_binding/memory.hpp"
#include <MinHook.h>
#include <nvsdk_ngx.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <intrin.h>
#include <atomic>
#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <string>

namespace mhw::ngx_startup {
namespace {
using Fn=unsigned(__cdecl *)(std::uint64_t,const wchar_t *,ID3D12Device *,unsigned);
using Microsoft::WRL::ComPtr;
Config cfg;Fn original{};std::ostream *out{};
std::atomic_uint count{},pending{},messages{},native_result{};
std::atomic_bool finished{},allowed{},stopping{};
bool installed{},initialized{};
HMODULE pinned_provider{};
ComPtr<ID3D12Device> device; // retained by the pinned diagnostic until process exit
constexpr char provider_sha[]="be6e434a94ca32499515eb62ca0e6c274526055d568d0426e4c652dcdfb6ee6e";
void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
// The game's own DLSS 1 (decline_game_dlss()). 0x23842a0 calls the game's NGX
// init (hook() below), checks SuperSampling and ImageSignalProcessing and
// builds a 72-byte wrapper (0x2383760: NGX parameters, a critical section at
// +16, no features at +56/+64) that cD3DRender::initForD3D12 stores at
// backend +0x42700. Its release (0x23837a0) also shuts NGX down, which this
// mod's own NGX must survive: the wrapper is freed as there, without the
// shutdown, the state the game's own refusals leave (NGX initialized, no
// wrapper; 0x51c6c28 = 2 or 3, the RTX 50 case).
constexpr unsigned game_dlss_rva=0x23842a0,generic_free_rva=0x2175260;
using GameDlss=void *(*)(void *);
using GenericFree=void (*)(void *,std::size_t);
GameDlss game_dlss_original{};GenericFree generic_free{};
void *game_dlss_target{};
bool decline_requested{},game_dlss_hooked{};
std::atomic_uint game_dlss_state{};
bool free_wrapper(void *wrapper) noexcept {
    const auto at=reinterpret_cast<std::uintptr_t>(wrapper);std::uint64_t features[2]{};
    if(!projection_binding::read(at+56,features[0])||!projection_binding::read(at+64,features[1])||features[0]||features[1])return false;
    __try{DeleteCriticalSection(reinterpret_cast<CRITICAL_SECTION *>(at+16));generic_free(wrapper,72);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void *game_dlss_hook(void *game_device){
    pending.fetch_add(1);void *wrapper{};
    __try{
        wrapper=game_dlss_original(game_device);
        if(!wrapper)game_dlss_state=2;
        else if(free_wrapper(wrapper)){wrapper=nullptr;game_dlss_state=3;}
        else game_dlss_state=4;
    }__finally{pending.fetch_sub(1);}
    return wrapper;
}
void NVSDK_CONV logger(const char *,NVSDK_NGX_Logging_Level,NVSDK_NGX_Feature){++messages;}
bool zero_slots() noexcept {
    for(unsigned i=0;i<7;++i){std::uint64_t value{};
        if(!projection_binding::read(cfg.client_slots+i*8,value)||value)return false;}
    return true;
}
HRESULT pin_swap(std::uintptr_t address,IDXGISwapChain **result) noexcept {
    __try{return reinterpret_cast<IUnknown *>(address)->QueryInterface(IID_PPV_ARGS(result));}
    __except(EXCEPTION_EXECUTE_HANDLER){return E_FAIL;}
}
HRESULT swap_desc(IDXGISwapChain *swap,DXGI_SWAP_CHAIN_DESC *desc) noexcept {
    __try{return swap->GetDesc(desc);}__except(EXCEPTION_EXECUTE_HANDLER){return E_FAIL;}
}
void graphics(ID3D12Device *expected){
    require(cfg.backend_slot&&expected,"startup-no-graphics");
    std::uintptr_t backend{},native_device{},swap_address{},table{},qi{},desc_method{};
    require(projection_binding::read(cfg.backend_slot,backend)&&backend&&backend<UINTPTR_MAX-0x42720&&
        projection_binding::read(backend+0x18,native_device)&&native_device==reinterpret_cast<std::uintptr_t>(expected)&&
        projection_binding::read(backend+0x1470,swap_address)&&swap_address&&projection_binding::read(swap_address,table)&&
        table&&table<UINTPTR_MAX-13*8&&projection_binding::read(table,qi)&&projection_binding::read(table+12*8,desc_method),"startup-graphics-pointers");
    const auto dxgi=GetModuleHandleW(L"dxgi.dll");require(dxgi!=nullptr,"startup-dxgi-module");
    for(const auto address:{qi,desc_method}){HMODULE owner{};
        MEMORY_BASIC_INFORMATION memory{};const bool queried=VirtualQuery(reinterpret_cast<void *>(address),&memory,sizeof(memory))==sizeof(memory);
        const auto protection=memory.Protect&255;
        require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(address),&owner)&&owner==dxgi&&queried&&memory.State==MEM_COMMIT&&memory.Type==MEM_IMAGE&&
            !(memory.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&(protection==PAGE_EXECUTE_READ||protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY),"startup-swap-method");}
    ComPtr<IDXGISwapChain> swap;
    require(SUCCEEDED(pin_swap(swap_address,&swap))&&
        reinterpret_cast<std::uintptr_t>(swap.Get())==swap_address,"startup-swap-identity");
    DXGI_SWAP_CHAIN_DESC desc{};require(SUCCEEDED(swap_desc(swap.Get(),&desc))&&desc.BufferDesc.Width&&desc.BufferDesc.Height&&desc.BufferDesc.Format!=DXGI_FORMAT_UNKNOWN,"startup-swap-desc");
    std::uintptr_t stable{};require(projection_binding::read(backend+0x1470,stable)&&stable==swap_address,"startup-swap-changed");
    *out<<"{\"event\":\"graphics\",\"api\":\"D3D12\",\"backend\":"<<backend<<",\"device\":"<<native_device<<",\"swapchain\":"<<swap_address
        <<",\"width\":"<<desc.BufferDesc.Width<<",\"height\":"<<desc.BufferDesc.Height<<",\"swapchain_format\":"<<unsigned(desc.BufferDesc.Format)<<"}\n"<<std::flush;
}
// FG builds: Streamline may have loaded the provider from this exact runtime
// folder first. Accept only that path with the pinned hash.
bool shared_provider_verified() noexcept {
    const auto provider=cfg.runtime/L"nvngx_dlss.dll";const auto module=GetModuleHandleW(provider.c_str());
    if(!cfg.shared_provider||!module)return false;
    wchar_t loaded[32768]{};const auto length=GetModuleFileNameW(module,loaded,32768);std::error_code error;
    return length&&length<32768&&std::filesystem::equivalent(loaded,provider,error)&&!error&&
        projection_binding::file_sha256(loaded)==provider_sha;
}
bool capabilities(const char *phase){
    NVSDK_NGX_Parameter *p{};const auto get=NVSDK_NGX_D3D12_GetCapabilityParameters(&p);
    int values[3]{};NVSDK_NGX_Result results[3]{NVSDK_NGX_Result_Fail,NVSDK_NGX_Result_Fail,NVSDK_NGX_Result_Fail};
    const char *keys[]{NVSDK_NGX_Parameter_SuperSampling_Available,NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver,
                      NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult};
    if(get==NVSDK_NGX_Result_Success&&p)for(unsigned i=0;i<3;++i)results[i]=p->Get(keys[i],&values[i]);
    const auto destroy=p?NVSDK_NGX_D3D12_DestroyParameters(p):NVSDK_NGX_Result_Fail;
    const auto provider=cfg.runtime/L"nvngx_dlss.dll";HMODULE loaded{};
    const bool found=GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN,provider.c_str(),&loaded)!=0;
    wchar_t path[32768]{};const auto length=found?GetModuleFileNameW(loaded,path,32768):0;
    std::error_code error;
    const bool identity=length&&length<32768&&std::filesystem::equivalent(path,provider,error)&&!error&&projection_binding::file_sha256(path)==provider_sha;
    if(identity)pinned_provider=loaded;
    // Finish all queries before emitting a complete record, including failures.
    *out<<"{\"event\":\"capabilities\",\"phase\":\""<<phase<<"\",\"get_result\":"<<unsigned(get)
        <<",\"get_values\":["<<unsigned(results[0])<<','<<unsigned(results[1])<<','<<unsigned(results[2])
        <<"],\"values_u32\":["<<unsigned(values[0])<<','<<unsigned(values[1])<<','<<unsigned(values[2])<<']'
        <<",\"destroy_result\":"<<unsigned(destroy)<<",\"modern_loaded\":"<<found<<",\"provider_identity_verified\":"<<identity
        <<",\"log_messages\":"<<messages.load()<<"}\n"<<std::flush;
    return get==NVSDK_NGX_Result_Success&&results[0]==1&&results[1]==1&&results[2]==1&&
        values[0]==1&&values[1]==0&&values[2]==1&&destroy==1&&found&&identity;
}
void before(std::uint64_t app,const wchar_t *path,ID3D12Device *d,unsigned api,std::uintptr_t caller) noexcept {
    try {
        const bool slots_zero=zero_slots();
        *out<<"{\"event\":\"native_enter\",\"caller\":"<<caller<<",\"device\":"<<reinterpret_cast<std::uintptr_t>(d)
            <<",\"api\":"<<api<<",\"slots_zero\":"<<slots_zero<<",\"tick\":"<<GetTickCount64()<<"}\n"<<std::flush;
        require(caller==cfg.caller&&app==native_app&&api==0x12&&d&&path&&slots_zero,"startup-entry-contract");
        require(d->GetNodeCount()==1&&SUCCEEDED(d->GetDeviceRemovedReason()),"startup-device");
        const auto provider=cfg.runtime/L"nvngx_dlss.dll";
        if(GetModuleHandleW(provider.c_str())){
            require(shared_provider_verified(),"modern-provider-already-loaded");
            *out<<"{\"event\":\"provider_preloaded\",\"shared\":true,\"path_and_hash_verified\":true}\n"<<std::flush;
        }
        const auto luid=d->GetAdapterLuid();
        device=d;const auto search=cfg.runtime.wstring();const wchar_t *paths[]{search.c_str()};
        NVSDK_NGX_FeatureCommonInfo info{};info.PathListInfo={paths,1};info.LoggingInfo={logger,NVSDK_NGX_LOGGING_LEVEL_ON,true};
        const auto result=NVSDK_NGX_D3D12_Init_with_ProjectID("956cdd19-7d3c-4660-b664-91c20d2bc9bd",NVSDK_NGX_ENGINE_TYPE_CUSTOM,
            "0.1.0",cfg.data.c_str(),d,&info);
        *out<<"{\"event\":\"public_init\",\"result\":"<<unsigned(result)<<",\"api\":21,\"adapter_luid_low\":"<<luid.LowPart
            <<",\"adapter_luid_high\":"<<luid.HighPart<<",\"runtime_sha256\":\""<<provider_sha
            <<"\",\"project_id\":\"956cdd19-7d3c-4660-b664-91c20d2bc9bd\",\"tick\":"<<GetTickCount64()<<"}\n"<<std::flush;
        initialized=result==NVSDK_NGX_Result_Success;
        require(initialized,"public-init-failed");
        allowed=capabilities("before_native");
    }catch(const std::exception &e){*out<<"{\"event\":\"rejected\",\"reason\":"<<std::quoted(e.what())<<"}\n"<<std::flush;}
}
void after(unsigned result) noexcept {
    native_result=result;
    try {
        *out<<"{\"event\":\"native_return\",\"result\":"<<result<<",\"arguments_unchanged\":true,\"result_unchanged\":true,\"tick\":"<<GetTickCount64()<<"}\n"<<std::flush;
        if(initialized){const bool valid=capabilities("after_native");allowed=allowed.load()&&valid;}
    }catch(...){allowed=false;}
    finished=true;
}
unsigned __cdecl hook(std::uint64_t app,const wchar_t *path,ID3D12Device *d,unsigned api){
    pending.fetch_add(1);
    const auto index=count.fetch_add(1);unsigned result{};
    const bool observe=index==0&&!stopping.load();
    // A reentrant/unexpected call never reinitializes the SDK or changes its result.
    __try {
        if(observe)before(app,path,d,api,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
        result=original(app,path,d,api);
        if(observe)after(result);
    } __finally {pending.fetch_sub(1);}
    return result;
}
}
bool install(const Config &config,std::ostream &log){
    require(!installed&&!original&&config.target&&config.caller&&config.client_slots,"startup-install-contract");
    cfg=config;out=&log;log<<std::boolalpha;
    require(cfg.runtime.is_absolute()&&cfg.data.is_absolute(),"startup-paths");
    {
        // v0.28.5: the provider's path and why its hash is missing, if it is
        // (0.28.4 under MO2 looked in overwrite's copy of the folder; worker.cpp).
        const auto path=cfg.runtime/L"nvngx_dlss.dll";const auto digest=projection_binding::file_digest(path.wstring());
        const auto utf8=path.u8string();
        log<<"{\"event\":\"provider_check\",\"path\":"<<std::quoted(std::string(utf8.begin(),utf8.end()))<<",\"stage\":\""<<digest.stage
           <<"\",\"error\":"<<digest.error<<",\"size\":"<<digest.size<<",\"read\":"<<digest.read<<",\"matches\":"<<(digest.sha==provider_sha)<<"}\n"<<std::flush;
        require(digest.sha==provider_sha,"startup-provider-sha256");
    }
    require(zero_slots(),"native-init-already-observed");
    require(!GetModuleHandleW((cfg.runtime/L"nvngx_dlss.dll").c_str())||shared_provider_verified(),"modern-provider-already-loaded");
    std::filesystem::create_directories(cfg.data);
    const auto status=MH_Initialize();
    require(status==(cfg.hook_pool_reserved?MH_ERROR_ALREADY_INITIALIZED:MH_OK),"startup-minhook-init");
    require(MH_CreateHook(cfg.target,reinterpret_cast<void *>(&hook),reinterpret_cast<void **>(&original))==MH_OK,"startup-hook-create");
    // Before the init hook: the game builds the wrapper right after its init.
    if(decline_requested){
        // The wrapper's builder, constructor and release, and the allocator's
        // free (clean image): otherwise the game's DLSS 1 stays as it is.
        const auto base=reinterpret_cast<std::uintptr_t>(cfg.target)-entry_rva;
        const bool code=projection_binding::verify_code(base,0x23842a0,344,"920c2df8717dcb2cf809d06f7d17c6e75b1d540b3e810e01b25b8a9e57d5ad2c")&&
            projection_binding::verify_code(base,0x2383760,54,"1ff21702d0d9ef19421ba8132849d21a53e75be539b8574a1a26686f124c8eeb")&&
            projection_binding::verify_code(base,0x23837a0,130,"066bbfc555bcbeb999b5e7b0688877cc9ec2d07f9155a2c6af94ce98d71c9b30")&&
            projection_binding::verify_code(base,0x2175260,50,"d678c8b941af2d1e2bcd5e4a5926d070ac5e983003ee2b51f85b2f53388b72ce");
        game_dlss_target=reinterpret_cast<void *>(base+game_dlss_rva);generic_free=reinterpret_cast<GenericFree>(base+generic_free_rva);
        game_dlss_hooked=code&&MH_CreateHook(game_dlss_target,reinterpret_cast<void *>(&game_dlss_hook),reinterpret_cast<void **>(&game_dlss_original))==MH_OK&&
            MH_EnableHook(game_dlss_target)==MH_OK;
        game_dlss_state=game_dlss_hooked?1:5;
        log<<"{\"event\":\"game_dlss\",\"declining\":"<<game_dlss_hooked<<",\"code\":"<<code<<"}\n"<<std::flush;
    }
    // Write before enable: callback events may occur immediately after enabling.
    log<<"{\"event\":\"installing\",\"tick\":"<<GetTickCount64()<<",\"sdk_initializations_limit\":1,\"gpu_commands\":0}\n"<<std::flush;
    require(MH_EnableHook(cfg.target)==MH_OK,"startup-hook-enable");installed=true;return true;
}
void decline_game_dlss() noexcept{decline_requested=true;}
unsigned game_dlss() noexcept{return game_dlss_state.load();}
bool done() noexcept{return finished.load();}
// v0.28.6: why a removal failed (closed beta tifa-2: hook_disabled false on one
// PC only, with SharpPluginLoader installed).
// Both removals are tried, so each pair describes its own hook.
std::array<int,4> disable_results{};
bool disable() noexcept{
    stopping=true;bool removed=true;
    if(installed){const auto s=MH_DisableHook(cfg.target);disable_results[0]=s;disable_results[1]=s==MH_OK?0:static_cast<int>(GetLastError());
        if(s==MH_OK)installed=false;else removed=false;}
    if(game_dlss_hooked){const auto s=MH_DisableHook(game_dlss_target);disable_results[2]=s;disable_results[3]=s==MH_OK?0:static_cast<int>(GetLastError());
        if(s==MH_OK)game_dlss_hooked=false;else removed=false;}
    return removed; // keep the trampolines allocated; a suspended entry may still forward
}
std::array<int,4> disable_result() noexcept{return disable_results;}
bool settled() noexcept{return pending.load()==0;}
bool supported() noexcept{return allowed.load();}
unsigned calls() noexcept{return count.load();}
bool write_graphics_snapshot() noexcept {
    try{graphics(device.Get());return true;}catch(...){return false;}
}
namespace {
bool reusable() noexcept {
    return !installed&&stopping&&finished&&pending==0&&count==1&&allowed&&native_result==1&&initialized&&pinned_provider&&device;
}
HMODULE validate_initialization(ID3D12Device *expected,const std::filesystem::path &runtime) noexcept {
    try {
        if(!reusable()||!expected||expected!=device.Get())return nullptr;
        const auto provider=runtime/L"nvngx_dlss.dll";
        if(!std::filesystem::equivalent(provider,cfg.runtime/L"nvngx_dlss.dll")||
           GetModuleHandleW(provider.c_str())!=pinned_provider||projection_binding::file_sha256(provider.wstring())!=provider_sha)return nullptr;
        return pinned_provider;
    }catch(...){return nullptr;}
}
const dlss_sdk::BorrowedInitialization retained_initialization{validate_initialization};
}
const dlss_sdk::BorrowedInitialization *borrowed_initialization() noexcept {
    return reusable()?&retained_initialization:nullptr;
}
}
