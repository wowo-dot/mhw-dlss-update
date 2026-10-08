#include "memory.hpp"
#include "code_gates.hpp"
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include "../perf/toggles.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <string_view>
namespace mhw::projection_binding {
namespace {
using Microsoft::WRL::ComPtr;
// perf::cached_methods: positive answers by target address. [0] holds
// d3d12.dll/D3D12Core.dll targets (valid for every caller), [1] dxgi.dll ones.
// The device pins both modules, so a remembered target cannot change module.
std::array<std::array<std::atomic<std::uintptr_t>,256>,2> system_targets{};
unsigned target_hash(std::uintptr_t at) noexcept {return static_cast<unsigned>(((at>>4)*0x9E3779B97F4A7C15ULL)>>56);}
bool remembered(std::uintptr_t at,bool dxgi) noexcept {
  for(unsigned k=0;k<(dxgi?2U:1U);++k)for(unsigned i=0,h=target_hash(at);i<8;++i){
    const auto v=system_targets[k][(h+i)&255].load(std::memory_order_acquire);if(v==at)return true;if(!v)break;}
  return false;
}
void remember(std::uintptr_t at,bool dxgi_module) noexcept {
  for(unsigned i=0,h=target_hash(at);i<8;++i){std::uintptr_t seen{};
    if(system_targets[dxgi_module][(h+i)&255].compare_exchange_strong(seen,at,std::memory_order_acq_rel)||seen==at)return;}
}
bool system_target(std::uintptr_t at,bool dxgi=false) noexcept {
  const bool cached=perf::on(perf::cached_methods);
  if(cached&&at&&remembered(at,dxgi))return true;
  MEMORY_BASIC_INFORMATION m{};
  if(!at || VirtualQuery(reinterpret_cast<void *>(at),&m,sizeof(m))!=sizeof(m) ||
     m.Type!=MEM_IMAGE || m.State!=MEM_COMMIT || (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
  const auto p=m.Protect&255;
  if(p!=PAGE_EXECUTE_READ && p!=PAGE_EXECUTE_READWRITE && p!=PAGE_EXECUTE_WRITECOPY)return false;
#ifdef MHW_BINDING_FIXTURE
  if(m.AllocationBase==GetModuleHandleW(nullptr))return true;
#endif
  wchar_t path[32768]{},directory[32768]{};
  const auto n=GetModuleFileNameW(static_cast<HMODULE>(m.AllocationBase),path,static_cast<DWORD>(std::size(path)));
  const auto d=GetSystemDirectoryW(directory,static_cast<UINT>(std::size(directory)));
  if(!n || n>=std::size(path) || !d || d>=std::size(directory))return false;
  const std::wstring_view file(path,n),root(directory,d);
  if(file.size()<=root.size()+1 || _wcsnicmp(file.data(),root.data(),root.size()) || file[root.size()]!=L'\\')return false;
  const auto tail=file.substr(root.size()+1);
#ifdef MHW_TEMPORAL_FIXTURE
  // Owned WARP tests enable the OS debug layer. Production discovery retains
  // its original allowlist; no game debug settings or gates are changed.
  if(!_wcsicmp(tail.data(),L"d3d12SDKLayers.dll"))return true;
#endif
  const bool d3d=!_wcsicmp(tail.data(),L"d3d12.dll") || !_wcsicmp(tail.data(),L"D3D12Core.dll");
  const bool ok=d3d || (dxgi && !_wcsicmp(tail.data(),L"dxgi.dll"));
  if(ok&&cached)remember(at,!d3d);
  return ok;
}
bool method(std::uintptr_t object,unsigned slot,std::uintptr_t &target,bool dxgi=false) noexcept {
  std::uintptr_t table{};
  return object && read(object,table) && table && table<=UINTPTR_MAX-8*(slot+1) && read(table+8*slot,target) && system_target(target,dxgi);
}
HRESULT pin(std::uintptr_t object,REFIID iid,void **out,bool dxgi=false) noexcept {
  std::uintptr_t qi{};
  if(!method(object,0,qi,dxgi))return E_NOINTERFACE;
  __try {return reinterpret_cast<IUnknown *>(object)->QueryInterface(iid,out);}
  __except(EXCEPTION_EXECUTE_HANDLER) {return E_FAIL;}
}
bool discover_objects(Runtime &r,std::string &reason) {
  ComPtr<ID3D12Device> device;ComPtr<IDXGISwapChain> swap;
  ComPtr<ID3D12Resource> cpu,gpu;
  if(FAILED(pin(r.device,IID_PPV_ARGS(&device))) || FAILED(pin(r.swapchain,IID_PPV_ARGS(&swap),true)) ||
     FAILED(pin(r.cpu_resource,IID_PPV_ARGS(&cpu))) || FAILED(pin(r.gpu_resource,IID_PPV_ARGS(&gpu)))) {reason="com-identity";return false;}
  // QueryInterface must retain the exact base-interface identity that is used
  // in the mapped engine fields. No undocumented adjusted interface accepted.
  if(reinterpret_cast<std::uintptr_t>(device.Get())!=r.device || reinterpret_cast<std::uintptr_t>(cpu.Get())!=r.cpu_resource ||
     reinterpret_cast<std::uintptr_t>(gpu.Get())!=r.gpu_resource) {reason="com-adjusted-interface";return false;}
  ComPtr<ID3D12Device> cpu_device,gpu_device;
  if(FAILED(cpu->GetDevice(IID_PPV_ARGS(&cpu_device))) || FAILED(gpu->GetDevice(IID_PPV_ARGS(&gpu_device))) ||
     cpu_device.Get()!=device.Get() || gpu_device.Get()!=device.Get()) {reason="resource-device";return false;}
  const auto cd=cpu->GetDesc(),gd=gpu->GetDesc();
  if(cd.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER || gd.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER ||
     !cd.Width || !gd.Width || cd.Format!=DXGI_FORMAT_UNKNOWN || gd.Format!=DXGI_FORMAT_UNKNOWN) {reason="arena-resource-desc";return false;}
  r.cpu_bytes=cd.Width;r.gpu_bytes=gd.Width;r.cpu_gpu_base=cpu->GetGPUVirtualAddress();r.gpu_base=gpu->GetGPUVirtualAddress();
  if(!r.cpu_gpu_base || !r.gpu_base) {reason="arena-gpu-address";return false;}
  DXGI_SWAP_CHAIN_DESC desc{};
  if(FAILED(swap->GetDesc(&desc)) || !desc.BufferDesc.Width || !desc.BufferDesc.Height) {reason="swap-chain-desc";return false;}
  r.format=desc.BufferDesc.Format;r.width=desc.BufferDesc.Width;r.height=desc.BufferDesc.Height;r.buffers=desc.BufferCount;
  const auto luid=device->GetAdapterLuid();r.luid_low=luid.LowPart;r.luid_high=luid.HighPart;
  ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;
  if(FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))) ||
     FAILED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list))) ||
     FAILED(list->Close())) {reason="owned-list-discovery";return false;}
  // The owned list is never submitted. Its implementation targets are checked
  // again against the actual engine object's vtable in every admitted API hook.
  for(unsigned i=root_cbv;i<kind_count;++i) {
    if(i==rollover)continue;
    const auto object=(i==create_cbv || i==create_pipeline)?r.device:reinterpret_cast<std::uintptr_t>(list.Get());
    if(!method(object,method_slots[i],r.targets[i])) {reason="system-api-target-"+std::to_string(i);return false;}
  }
  return true;
}
}
bool system_method(std::uintptr_t object,unsigned slot,std::uintptr_t &target) noexcept {return method(object,slot,target);}
bool discover(std::uintptr_t base,Runtime &r,std::string &reason) {
  if(!host_identity(base,reason))return false;
  for(const auto &s:code_spans)if(!verify_code(base,s.rva,s.size,s.sha)) {reason="live-code-"+std::to_string(s.rva);return false;}
  for(const auto vt:{0x350d188U,0x33f82a8U}) {
    std::uintptr_t p{};if(!read(base+vt+0x48,p) || p!=base+0x228e770) {reason="camera-vtable";return false;}
  }
  unsigned char api{};std::uintptr_t renderer{},wrapper{};unsigned sizing{};
  if(!read(base+0x51cc080,api) || !api || !read(base+0x51c4480,renderer) || !renderer ||
     renderer>UINTPTR_MAX-0x1f570 || !read(renderer+0x78,r.backend) || !r.backend || r.backend>UINTPTR_MAX-0x42708 ||
     !read(renderer+0xb4,sizing) || (sizing&1) || !read(r.backend+0x42700,wrapper) || wrapper ||
     !read(r.backend+0x18,r.device) || !read(r.backend+0x1470,r.swapchain)) {reason="dx12-provider-gate";return false;}
  Arena a{};
  if(!read_arena(r.backend,a)) {reason="arena-read";return false;}
  r.mode=a.mode;r.cpu_resource=a.cpu_resource;r.gpu_resource=a.gpu_resource;
  if(!discover_objects(r,reason))return false;
  // MHW's aspect-ratio option (e.g. 21:9 in a 16:9 window) renders the scene
  // into a rectangle: renderer+0x198 holds the scene size, while the config
  // keeps the window size as its scale basis with the override flag set
  // (game-17: scene 2560x1080, basis 2560x1440). Without the option the scene
  // is the window.
  r.window_width=r.width;r.window_height=r.height;r.scene_override=0;
  {
    std::uintptr_t config{};unsigned char override_width{};unsigned scene_width{},scene_height{},basis_width{},basis_height{};
    if(!read(renderer+0x198,scene_width) || !read(renderer+0x19c,scene_height) || !read(base+0x51c5320,config) || !config ||
       config>UINTPTR_MAX-0x2cdb || !read(config+0x2cda,override_width)) {reason="scene-extent-read";return false;}
    if(scene_width!=r.width || scene_height!=r.height) {
      if(override_width!=1 || !read(config+0x2cd0,basis_width) || !read(config+0x2cd4,basis_height) ||
         basis_width!=r.window_width || basis_height!=r.window_height || !scene_width || !scene_height ||
         scene_width>r.window_width || scene_height>r.window_height) {reason="scene-extent";return false;}
      r.width=scene_width;r.height=scene_height;r.scene_override=1;
    }
  }
  Arena b{};std::uintptr_t after{},device_after{},swap_after{};
  if(!read(renderer+0x78,after) || after!=r.backend || !read_arena(r.backend,b) ||
     !read(r.backend+0x18,device_after) || device_after!=r.device || !read(r.backend+0x1470,swap_after) || swap_after!=r.swapchain ||
     a.mode!=b.mode || a.stride!=b.stride || a.cpu!=b.cpu || a.gpu!=b.gpu ||
     a.cpu_resource!=b.cpu_resource || a.gpu_resource!=b.gpu_resource || b.gpu[0]!=r.gpu_base ||
     static_cast<std::uint64_t>(b.stride)*3>r.cpu_bytes || static_cast<std::uint64_t>(b.stride)*3>r.gpu_bytes) {reason="arena-changed";return false;}
  for(unsigned i=0;i<9;++i)r.targets[i]=base+native_rvas[i];
  r.targets[rollover]=base+native_rvas[9];
  for(unsigned i=0;i<kind_count;++i)for(unsigned j=0;j<i;++j)if(r.targets[i]==r.targets[j]) {reason="aliased-hook-target";return false;}
  return true;
}
}
