#include "../src/scaled_temporal/handoff.hpp"
namespace sh=mhw::scaled_temporal::handoff;
// Combined fixture: the real TAA recorder publishes the selected frame, then
// the owned native-shaped final helper queues a copy before GPU consumption.
struct HandoffFixture {
  Block memory{0x1000};ComPtr<ID3D12Resource> destination;
  std::uintptr_t owner{},attachment{};
  HandoffFixture(ID3D12Device *device,std::uintptr_t source) {
    owner=memory.p;attachment=memory.p+0x200;
    auto backing=get<std::uintptr_t>(source+0xf0);if(!backing)backing=get<std::uintptr_t>(source+0x60);
    auto resource=reinterpret_cast<ID3D12Resource *>(get<std::uintptr_t>(backing+0x20));auto desc=resource->GetDesc();
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
    check(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&destination))),"handoff destination");
    std::memcpy(reinterpret_cast<void *>(owner),reinterpret_cast<void *>(source),256);
    put(owner+0x60,memory.p+0x400);put(owner+0xf0,std::uintptr_t{});put(memory.p+0x420,reinterpret_cast<std::uintptr_t>(destination.Get()));
    put(attachment+0x58,owner);
    sh::fixture_start(cfg,reinterpret_cast<void *>(forward));
  }
  static std::uintptr_t forward(std::uintptr_t,std::uintptr_t context,std::uintptr_t source,std::uintptr_t attachment) {
    const auto index=get<unsigned>(context+0x2b80);const auto packet=packet_at+packet_stride*index;
    put(packet,13U);put(packet+0x10,get<std::uintptr_t>(attachment+0x58));put(packet+0x18,source);
    put(packet+0x20,0U);put(packet+0x24,0U);put(packet+0x28,0U);put(packet+0x30,std::array<unsigned,2>{});
    put(packet+0x38,get<std::array<unsigned,4>>(context+0x5c));put(packet+0x48,std::uintptr_t{});
    put(array_at+16*index+8,packet);put(context+0x2b80,index+1);return 0x123456789abcdef0ULL;
  }
  void invoke(std::uintptr_t layer,std::uintptr_t source) {
    mhw::projection_binding::Frame f{};f.caller=cfg.image+sh::caller_rva;f.gpr[1]=layer;
    f.gpr[2]=cfg.renderer+0x710;f.gpr[3]=source;f.gpr[4]=attachment;sh::dispatch(&f);
    check(f.gpr[0]==0x123456789abcdef0ULL && sh::healthy(),"handoff forwarding and qualification");
  }
};
