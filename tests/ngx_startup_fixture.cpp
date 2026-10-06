#include "../src/ngx_startup/startup.hpp"
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
extern "C" {
unsigned fixture_native_calls{};
unsigned fixture_native_result{0xbad00002};
std::array<std::uintptr_t,4> fixture_arguments{};
unsigned fixture_native(std::uint64_t,const wchar_t *,ID3D12Device *,unsigned);
unsigned fixture_invoke(std::uint64_t,const wchar_t *,ID3D12Device *,unsigned);
void fixture_return();
}
namespace {
void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
}
int wmain(int argc,wchar_t **argv){try{
    namespace startup=mhw::ngx_startup;namespace fs=std::filesystem;using Microsoft::WRL::ComPtr;
    require(argc==6,"usage: startup_fixture runtime output luid_low luid_high case");
    const auto runtime=fs::canonical(argv[1]);const auto output=fs::absolute(argv[2]);const std::wstring mode=argv[5];
    require(mode==L"success"||mode==L"caller"||mode==L"api"||mode==L"device"||mode==L"slots","case");
    require(!fs::exists(output),"preserve fixture");fs::create_directories(output);
    std::ofstream log(output/L"events.jsonl",std::ios::binary);
    ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter1> adapter;ComPtr<ID3D12Device> device;
    require(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))),"factory");
    require(SUCCEEDED(factory->EnumAdapterByLuid({static_cast<UINT>(std::stoul(argv[3])),static_cast<LONG>(std::stol(argv[4]))},IID_PPV_ARGS(&adapter))),"adapter");
    require(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))),"device");
    std::array<std::uint64_t,7> slots{};
    std::array<std::uintptr_t,0x1480/8> backend{};const auto backend_address=reinterpret_cast<std::uintptr_t>(backend.data());
    backend[0x18/8]=reinterpret_cast<std::uintptr_t>(device.Get());
    startup::install({reinterpret_cast<void *>(&fixture_native),reinterpret_cast<std::uintptr_t>(&fixture_return),
        reinterpret_cast<std::uintptr_t>(slots.data()),runtime,output/L"sdk-data",reinterpret_cast<std::uintptr_t>(&backend_address)},log);
    const wchar_t *path=L"fixture path kept unchanged";auto *input=mode==L"device"?nullptr:device.Get();const unsigned api=mode==L"api"?21:18;
    if(mode==L"slots")slots[0]=1;
    const auto result=mode==L"caller"?fixture_native(startup::native_app,path,input,api):fixture_invoke(startup::native_app,path,input,api);
    require(result==0xbad00002&&fixture_native_calls==1,"first native result/count");
    require(fixture_arguments==std::array<std::uintptr_t,4>{startup::native_app,reinterpret_cast<std::uintptr_t>(path),reinterpret_cast<std::uintptr_t>(input),api},"native arguments changed");
    require(startup::done()&&startup::settled()&&startup::supported()==(mode==L"success"),"startup transaction");
    // A second call must only forward, even when the first was rejected.
    require(fixture_invoke(startup::native_app,path,device.Get(),18)==0xbad00002&&fixture_native_calls==2,"repeat forwarding");
    require(startup::calls()==2&&startup::disable()&&startup::settled(),"hook closure");
    require(!startup::borrowed_initialization(),"failed/repeated native call must not grant SDK ownership");
    require(fixture_invoke(startup::native_app,path,device.Get(),18)==0xbad00002&&fixture_native_calls==3&&startup::calls()==2,"disabled forwarding");
    require(!startup::write_graphics_snapshot(),"missing swapchain must be rejected");
    if(mode==L"success"){
        ComPtr<IDXGIFactory2> composition;require(SUCCEEDED(factory.As(&composition)),"composition factory");
        ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC q{};
        require(SUCCEEDED(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))),"composition queue");
        DXGI_SWAP_CHAIN_DESC1 desc{};desc.Width=desc.Height=64;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;
        desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;desc.AlphaMode=DXGI_ALPHA_MODE_IGNORE;
        ComPtr<IDXGISwapChain1> swap;
        require(SUCCEEDED(composition->CreateSwapChainForComposition(queue.Get(),&desc,nullptr,&swap)),"composition swapchain");
        backend[0x1470/8]=reinterpret_cast<std::uintptr_t>(swap.Get());
        require(startup::write_graphics_snapshot(),"graphics identity/format capture");
        backend[0x1470/8]=0;
    }
    log<<"{\"event\":\"fixture_pass\",\"original_calls\":3,\"arguments_unchanged\":true,\"native_error_preserved\":true,\"hook_disabled\":true,\"capabilities_supported\":"<<startup::supported()<<"}\n";
    std::cout<<"Startup fixture passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
