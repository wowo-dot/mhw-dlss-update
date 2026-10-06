#pragma once
#include "../src/temporal_bindings/jitter.hpp"
#include <bit>
// Owned virtual memory only; models CPU matrix publication, never game code.
struct JitterFixture {
  Block camera{0x1000},table{0xf000},metadata_table{0x1000},metadata{0x1000},constants{0x1000};
  tb::Config c;unsigned calls{};float width,height;
  static inline JitterFixture *active{};
  explicit JitterFixture(const tb::Config &config,float w=17,float h=13):c(config),width(w),height(h){
    active=this;put(c.image+0x51c4400,camera.p);put(c.image+0x51c4368,table.p);
    put(camera.p,c.image+0x350d188);put(camera.p+0x70,static_cast<unsigned char>(1));
    const auto context=c.renderer+0x710;put(context+0x2b18,metadata_table.p);put(c.image+0x4be75f4,350U);
    put(metadata_table.p+8*350,metadata.p);put(metadata.p+0x10,80U);put(context+0x710+8*350,constants.p);
    put(context+0x230,width);put(context+0x234,height);
    for(unsigned i=0;i<3;++i)put(c.image+mhw::projection_binding::matrix_id_rvas[i],(i*16<<12)|0xf55U);
    put(c.image+0x4be7604,(48U<<12)|0xf55U);put(c.image+0x4be763c,(64U<<12)|0xf55U);
    const auto pair=std::uint64_t{std::bit_cast<unsigned>(.375F)}|(std::uint64_t{std::bit_cast<unsigned>(-.4375F)}<<32);
    for(unsigned i=0;i<20;++i)put(table.p+0xe6a0+8*i,pair);
#if defined(MHW_TEMPORAL_LONG_HISTORY) || defined(MHW_AA_GPU_TRACE)
    for(unsigned i=0;i<20;++i){const float x=.3125F+float(i)/128,y=-.3125F-float(i)/128;
      put(table.p+0xe6a0+8*i,std::uint64_t{std::bit_cast<unsigned>(x)}|(std::uint64_t{std::bit_cast<unsigned>(y)}<<32));}
#endif
  }
  static std::uint64_t forward(std::uintptr_t projection,std::uintptr_t context){
    auto &s=*active;++s.calls;check(projection==s.camera.p+0x50 && context==s.c.renderer+0x710,"jitter ABI");
    const auto frame=get<std::uint64_t>(s.c.renderer+0x1f210);
    if(frame==12 && mode=="jitter_late_exception")RaiseException(0xe0424242,0,0,nullptr);
    if(frame==6 && mode=="jitter_exception")RaiseException(0xe0424242,0,0,nullptr);
    if(frame==6 && mode=="jitter_stop")tb::jitter::stop();
    if(frame==6 && mode=="jitter_provider")put(s.c.renderer+0xb4,1U);
    if(frame==6 && mode=="jitter_competing_write")put(s.camera.p+0x38,std::uint64_t{1});
    if(frame==6 && mode=="jitter_reentrant" && s.calls==2){auto f=s.frame();tb::jitter::dispatch(&f);}
    std::array<float,16> plain{},view{};for(unsigned i=0;i<4;++i)plain[5*i]=view[5*i]=1;
    auto jittered=plain;jittered[12]=get<float>(s.camera.p+0x38)/s.width;jittered[13]=get<float>(s.camera.p+0x3c)/s.height;
    if(frame==6 && mode=="jitter_matrix")jittered[12]+=.01F;
    for(unsigned i=0;i<5;++i){const auto &v=(i==0 || i==3)?jittered:i==1?view:plain;
      std::memcpy(reinterpret_cast<void *>(s.constants.p+64*i),v.data(),64);}
    return frame==6 && mode=="jitter_false_return"?0x123400ULL:0x123401ULL;
  }
  tb::NativeFrame frame(){tb::NativeFrame f{};f.call.caller=c.image+0x228e834;f.call.gpr[1]=camera.p+0x50;f.call.gpr[2]=c.renderer+0x710;return f;}
  void stage(std::uint64_t n){put(c.renderer+0x1f210,n);put(c.renderer+0x1f208,n);put(c.renderer+0x1f200,static_cast<unsigned>(n&1));put(camera.p+0x40,static_cast<unsigned>(n));}
  tb::Snapshot snapshot(){tb::Snapshot s{};s.frame=get<std::uint64_t>(c.renderer+0x1f210);s.epoch=get<std::uint64_t>(c.renderer+0x1f208);
    s.bank=get<unsigned>(c.renderer+0x1f200);s.async=get<unsigned char>(c.renderer+0x93);s.context=c.renderer+0x710;s.view=get<std::uintptr_t>(c.renderer+0x1f450);return s;}
};
