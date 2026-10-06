#pragma once
// Reproduce a color transition scheduled two entries before the TAA packet.
// The real WARP transition is outside the observed consumer; its metadata is
// deliberately independent of the API barrier log used by the recorder.
bool admission_fixture{},admission_stale{},admission_alias{};
struct ColorAdmissionFixture {
  Block schedule{16*8},packets{0x30*8},trackers{0x38*3},spans{0x28*7},nodes{0x20*4},alias{0x80};
  ComPtr<ID3D12Resource> output;
  std::uintptr_t color_backing{},output_backing{},motion_backing{};
  std::uint64_t epoch{};
  ColorAdmissionFixture(ID3D12Device *device,std::uintptr_t owners,std::uintptr_t backing) {
    auto d=color_resource->GetDesc();D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;hp.CreationNodeMask=hp.VisibleNodeMask=1;
    check(SUCCEEDED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&output))),"admission output");
    color_backing=backing+0x40;output_backing=backing+0x100;motion_backing=backing;
    put(owners+4*256+0x60,output_backing);put(output_backing+0x20,reinterpret_cast<std::uintptr_t>(output.Get()));
    put(color_backing+0x70,trackers.p);put(output_backing+0x70,trackers.p+0x38);put(motion_backing+0x70,trackers.p+0x70);
  }
  void enter(std::uintptr_t current) {
    epoch=get<std::uint64_t>(cfg.renderer+0x1f208);put(cfg.renderer+0x1f208,epoch+1);
    put(cfg.renderer+0x7af18,schedule.p);put(cfg.renderer+0x7af28,8U);
    const auto packet=[&](unsigned i){return i==4?current:packets.p+0x30*i;};
    const auto span=[&](unsigned i){return spans.p+0x28*i;};
    const auto node=[&](unsigned i){return nodes.p+0x20*i;};
    for(unsigned i=0;i<8;++i){put(schedule.p+16*i+8,packet(i));if(i!=4)put(packet(i),6U);}
    for(unsigned i=0;i<3;++i){const auto t=trackers.p+0x38*i;put(t+0x10,epoch+1);put(t+0x20,epoch+1);put(t+0x28,static_cast<unsigned char>(1));}
    const auto use=[&](unsigned i,unsigned p,unsigned ordinal,unsigned before,unsigned after,std::uintptr_t previous,std::uintptr_t next){
      put(span(i),packet(p));put(span(i)+8,ordinal);put(span(i)+0x10,next);put(span(i)+0x18,previous);
      put(span(i)+0x20,static_cast<std::uint16_t>(before));put(span(i)+0x22,static_cast<std::uint16_t>(after));};
    use(0,0,0,8,0x100,0,span(1));use(1,2,4,0x100,4,span(0),span(2));use(2,6,6,4,0x100,span(1),span(3));use(3,7,7,0x100,8,span(2),0);
    put(trackers.p,span(0));put(trackers.p+8,span(3));put(trackers.p+0x18,std::uint16_t{8});put(trackers.p+0x1a,std::uint16_t{8});
    use(4,2,2,0,0x400,0,span(5));use(5,4,4,0x400,0x400,span(4),span(6));use(6,5,5,0x400,0x20,span(5),0);
    const auto out=trackers.p+0x38;put(out,span(4));put(out+8,span(6));put(out+0x1a,std::uint16_t{0x20});
    const auto link=[&](unsigned i,std::uintptr_t backing,unsigned use_index,unsigned before,unsigned after,std::uintptr_t next){
      put(node(i),next);put(node(i)+8,backing);put(node(i)+0x10,span(use_index));
      put(node(i)+0x18,static_cast<std::uint16_t>(before));put(node(i)+0x1a,static_cast<std::uint16_t>(after));};
    link(0,color_backing,0,8,0x100,0);link(1,color_backing,1,0x100,4,node(2));link(2,output_backing,4,0,0x400,0);link(3,output_backing,5,0x400,0x400,0);
    put(packet(0)+8,node(0));put(packet(2)+8,node(1));put(current+8,node(3));
    if(admission_stale)put(trackers.p+0x10,epoch);
    if(admission_alias){put(alias.p+0x20,reinterpret_cast<std::uintptr_t>(color_resource));put(node(1)+8,alias.p);}
    D3D12_RESOURCE_BARRIER b{};b.Transition={color_resource,UINT_MAX,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};
    native_list->ResourceBarrier(1,&b);
  }
  void leave(){put(cfg.renderer+0x1f208,epoch);}
};
std::unique_ptr<ColorAdmissionFixture> color_admission_fixture;
