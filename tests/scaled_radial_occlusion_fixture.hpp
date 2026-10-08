#pragma once
// Radial occlusion (role 2) admission on actual textures: the native format-7
// RGBA8 texture, and the 1x1 R32 float (engine format 16) that single radial
// frames in the Alatreon fight bind (game-45). A larger R32, another 1x1 float
// format or a mismatched engine format still fails check 7.
void check_radial_occlusion(fx::Fixture &f){
 using stream_plan_fixture::Arena;using stream_plan_fixture::put;using fx::require;
 Arena a(0x4000);const auto saved=route::cfg;route::cfg.device=reinterpret_cast<std::uintptr_t>(f.device.Get());
 const auto admit=[&](unsigned engine,DXGI_FORMAT format,unsigned w,unsigned h){
  auto r=fx::resource(f.device.Get(),fx::texture(w,h,format),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COMMON);
  const auto owner=a.take(0x100),backend=a.take(0x40);
  put(owner,std::uintptr_t{1});put(owner+0x60,backend);put(owner+0x88,w+(h<<16));put(owner+0x90,engine);
  put(backend+0x20,reinterpret_cast<std::uintptr_t>(r.Get()));
  mhw::temporal_bindings::Owner o;require(mhw::temporal_bindings::read_owner(route::reader,owner,o),"radial occlusion owner");
  Microsoft::WRL::ComPtr<ID3D12Resource> held;const bool ok=route::dependency_texture(o,2,held);
  require(ok==(held.Get()==r.Get()),"an admitted texture is the owner's resource");
  return ok?0U:route::dependency_texture_check;};
 require(admit(7,DXGI_FORMAT_R8G8B8A8_UNORM,64,32)==0,"native RGBA8 occlusion");
 require(admit(16,DXGI_FORMAT_R32_FLOAT,1,1)==0,"1x1 R32 float occlusion (game-45)");
 require(admit(16,DXGI_FORMAT_R32_FLOAT,2,2)==7,"larger R32 occlusion stays rejected");
 require(admit(16,DXGI_FORMAT_R16_FLOAT,1,1)==7,"another 1x1 float format stays rejected");
 require(admit(7,DXGI_FORMAT_R32_FLOAT,1,1)==7,"the engine format must match");
 route::cfg=saved;f.messages();
 std::cout<<"PASS radial occlusion: RGBA8 and the 1x1 R32 float texel admitted; 2x2 R32, 1x1 R16F and a mismatched engine format rejected\n";
}
