// Isolated public-API initialization ordering probe. Never opens a game process.
#include <d3d12.h>
#include <nvsdk_ngx.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
namespace fs=std::filesystem;
using Microsoft::WRL::ComPtr;
constexpr char project[]="956cdd19-7d3c-4660-b664-91c20d2bc9bd";
std::atomic_uint messages{};
void NVSDK_CONV logger(const char *,NVSDK_NGX_Logging_Level,NVSDK_NGX_Feature){++messages;}
void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
void hr(HRESULT result,const char *why){require(SUCCEEDED(result),why);}
void api(std::ostream &log,const char *name,NVSDK_NGX_Result result){
    log<<"{\"event\":\"api\",\"name\":\""<<name<<"\",\"result\":"<<static_cast<unsigned>(result)<<"}\n"<<std::flush;
    require(result==NVSDK_NGX_Result_Success,name);
}
void initialize(std::ostream &log,ID3D12Device *device,const fs::path &directory,const fs::path &data){
    fs::create_directories(data);
    const auto path=directory.wstring();const wchar_t *paths[]{path.c_str()};
    NVSDK_NGX_FeatureCommonInfo common{};
    if(!directory.empty())common.PathListInfo={paths,1};
    common.LoggingInfo={logger,NVSDK_NGX_LOGGING_LEVEL_ON,true};
    api(log,"Init_with_ProjectID",NVSDK_NGX_D3D12_Init_with_ProjectID(project,NVSDK_NGX_ENGINE_TYPE_CUSTOM,"0.1.0",data.c_str(),device,&common));
}
NVSDK_NGX_Parameter *inspect(std::ostream &log,ID3D12Device *device,const fs::path &modern,const fs::path &legacy){
    NVSDK_NGX_Parameter *parameters{};
    api(log,"GetCapabilityParameters",NVSDK_NGX_D3D12_GetCapabilityParameters(&parameters));
    require(parameters!=nullptr,"capability parameters");
    const char *keys[]{NVSDK_NGX_Parameter_SuperSampling_Available,NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver,
                      NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult};
    const char *names[]{"Available","NeedsUpdatedDriver","FeatureInitResult"};
    for(unsigned i=0;i<3;++i){int value{};const auto result=parameters->Get(keys[i],&value);
        log<<"{\"event\":\"capability\",\"name\":\""<<names[i]<<"\",\"result\":"<<static_cast<unsigned>(result)
           <<",\"value\":"<<value<<",\"value_u32\":"<<static_cast<unsigned>(value)<<"}\n"<<std::flush;}
    log<<std::boolalpha<<"{\"event\":\"modules\",\"modern_loaded\":"<<(GetModuleHandleW(modern.c_str())!=nullptr)
       <<",\"legacy_loaded\":"<<(GetModuleHandleW(legacy.c_str())!=nullptr)<<",\"log_messages\":"<<messages.load()<<"}\n"<<std::flush;
    hr(device->GetDeviceRemovedReason(),"device removed");
    return parameters;
}
void run(int argc,wchar_t **argv){
    require(argc==7,"usage: capability_probe modern_dll legacy_dll output luid_low luid_high mode");
    const auto modern=fs::canonical(argv[1]).make_preferred(),legacy=fs::canonical(argv[2]).make_preferred();
    const auto output=fs::absolute(argv[3]);const std::wstring mode=argv[6];
    require(mode==L"fresh"||mode==L"prior-empty"||mode==L"prior-legacy"||mode==L"prior-modern"||
            mode==L"prior-empty-reset"||mode==L"prior-modern-empty","probe mode");
    std::string mode_name;for(auto c:mode)mode_name+=static_cast<char>(c); // validated ASCII modes only
    require(!fs::exists(output),"preserve capability evidence");fs::create_directories(output);
    std::ofstream log(output/L"events.jsonl",std::ios::binary);require(bool(log),"probe log");
    const auto low=static_cast<UINT>(std::stoul(argv[4]));const auto high=static_cast<LONG>(std::stol(argv[5]));
    ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter1> adapter;ComPtr<ID3D12Device> device;
    hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"factory");hr(factory->EnumAdapterByLuid({low,high},IID_PPV_ARGS(&adapter)),"adapter");
    hr(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)),"device");
    log<<"{\"event\":\"start\",\"schema\":1,\"mode\":\""<<mode_name
       <<"\",\"api\":21,\"adapter_luid_low\":"<<low<<",\"adapter_luid_high\":"<<high
       <<",\"game_process_access\":false,\"feature_creation\":false,\"project_id\":\""<<project<<"\"}\n"<<std::flush;
    NVSDK_NGX_Parameter *prior{};
    if(mode!=L"fresh"){
        log<<"{\"event\":\"phase\",\"name\":\"prior\"}\n"<<std::flush;
        const auto directory=(mode==L"prior-empty"||mode==L"prior-empty-reset")?fs::path{}:mode==L"prior-legacy"?legacy.parent_path():modern.parent_path();
        initialize(log,device.Get(),directory,output/L"prior-data");prior=inspect(log,device.Get(),modern,legacy);
        if(mode==L"prior-empty-reset"){
            // This process owns the device and every SDK call. This is NOT a
            // recovery policy for the borrowed game device.
            api(log,"DestroyPriorParameters",NVSDK_NGX_D3D12_DestroyParameters(prior));prior=nullptr;
            api(log,"ShutdownPriorOwnedDevice",NVSDK_NGX_D3D12_Shutdown1(device.Get()));
        }
    }
    log<<"{\"event\":\"phase\",\"name\":\"second\"}\n"<<std::flush;
    initialize(log,device.Get(),mode==L"prior-modern-empty"?fs::path{}:modern.parent_path(),output/L"modern-data");
    auto *current=inspect(log,device.Get(),modern,legacy);
    api(log,"DestroyParameters",NVSDK_NGX_D3D12_DestroyParameters(current));
    if(prior&&prior!=current)api(log,"DestroyPriorParameters",NVSDK_NGX_D3D12_DestroyParameters(prior));
    api(log,"Shutdown1",NVSDK_NGX_D3D12_Shutdown1(device.Get()));
    log<<"{\"event\":\"finished\",\"feature_creation\":false,\"evaluation\":false}\n"<<std::flush;
}
}
int wmain(int argc,wchar_t **argv){try{run(argc,argv);return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
