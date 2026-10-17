// Observer list slots: forget() frees the slot of a list its owner released,
// a new list reuses it, and a thread's cached lookup of the forgotten list
// misses. A table of live lists still fails closed when full.
#include "../src/temporal_present/observer.hpp"
#include "fake_astera.hpp"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
namespace obs=mhw::temporal_present::observer;
using Microsoft::WRL::ComPtr;
void check(bool v,const char *s){if(!v){std::cerr<<s<<'\n';std::exit(1);}}
int main(){
    ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter> adapter;ComPtr<ID3D12Device> device;
    check(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)))&&
          SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))),"WARP device");
    ComPtr<ID3D12CommandAllocator> allocator;
    check(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))),"allocator");
    const auto create=[&]{ComPtr<ID3D12GraphicsCommandList> l;
        check(SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&l)))&&SUCCEEDED(l->Close()),"list");
        return l;};
    // The loader installs the observer's registered detours, then the observer
    // checks them, with every hook the DLL installs.
    mhw::test::fake_astera::start();std::vector<mhw::astera::Hook> hooks;obs::astera_hooks(hooks);
    const auto vtable=create();
    check(mhw::test::fake_astera::install_methods(hooks,{.command_list=vtable.Get()}),"loader installs the observer's detours");
    std::string reason;
    const auto resolver=[](std::uintptr_t p,unsigned slot,std::uintptr_t &target)noexcept{target=(*reinterpret_cast<std::uintptr_t **>(p))[slot];return target!=0;};
    check(obs::install(device.Get(),resolver,reason,true,nullptr,true,true),reason.c_str());obs::begin();
    std::vector<ComPtr<ID3D12GraphicsCommandList>> lists;
    for(unsigned i=0;i<obs::list_limit;++i){lists.push_back(create());obs::reset(lists.back().Get(),i+1,nullptr);}
    mhw::temporal_present::Bindings s;
    check(obs::healthy()&&obs::inspect(lists[5].Get(),s)&&s.generation==6,"full table, cached lookup");
    // The owner reports list 5 released; this thread's cache still names it.
    auto *forgotten=lists[5].Get();obs::forget(forgotten);
    check(!obs::inspect(forgotten,s),"forgotten list misses, also through this thread's cache");
    bool other{};std::thread([&]{other=!obs::inspect(forgotten,s);}).join();check(other,"forgotten list misses on another thread");
    auto reused=create();obs::reset(reused.Get(),500,nullptr);
    check(obs::healthy()&&obs::inspect(reused.Get(),s)&&s.generation==500,"a new list takes the freed slot");
    check(obs::inspect(lists[6].Get(),s)&&s.generation==7,"other lists keep their entries");
    // Every slot holds a live list again: one more fails closed.
    auto extra=create();obs::reset(extra.Get(),600,nullptr);
    check(!obs::healthy(),"a full table of live lists fails closed");
    std::cout<<"Observer slots: forget, reuse and full table passed\n";
    obs::stop();return 0;
}
