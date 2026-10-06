#include "../src/temporal_bindings/jitter.hpp"
#ifdef MHW_AA_PROJECTION
#include "../src/aa_control/projection_link.hpp"
#endif
#include <windows.h>
#include <cstring>
#include <iostream>
#include <fstream>
#include <cstdlib>
namespace tb=mhw::temporal_bindings;
void check(bool v,const char *s){if(!v){std::cerr<<s<<'\n';std::exit(1);}}
template<class T>void put(std::uintptr_t p,T v){std::memcpy(reinterpret_cast<void *>(p),&v,sizeof(v));}
template<class T>T get(std::uintptr_t p){T v;std::memcpy(&v,reinterpret_cast<void *>(p),sizeof(v));return v;}
struct Block {std::uintptr_t p{};explicit Block(std::size_t n){p=reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr,n,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));check(p!=0,"allocation");}
  ~Block(){VirtualFree(reinterpret_cast<void *>(p),0,MEM_RELEASE);}};
std::string mode;
bool projection_owned{};std::uintptr_t owned_context{},owned_view{};
bool projection_admission(std::uintptr_t context,std::uintptr_t view) noexcept {
  return projection_owned && context==owned_context && view==owned_view;
}
#include "temporal_jitter_fixture.hpp"
bool invoke(tb::NativeFrame *f){__try{tb::jitter::dispatch(f);return true;}
  __except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return false;}}
int main(int argc,char **argv){
  check(argc==2 || argc==3,"arguments");mode=argv[1];
  Block image(0x55d3000),renderer(0x20000),backend(0x43000),view(0x1000);
  tb::Config cfg{image.p,renderer.p,backend.p,0x123456};
  put(image.p+0x51c4480,renderer.p);put(renderer.p+0x78,backend.p);put(backend.p+0x18,cfg.device);put(renderer.p+0x1f450,view.p);
  const bool scaled_render=mode=="jitter_scaled_render",scaled_allocation=mode=="jitter_scaled_allocation";
  JitterFixture fixture(cfg,scaled_render?1280.F:scaled_allocation?2560.F:17.F,
                        scaled_render?720.F:scaled_allocation?1440.F:13.F);const auto tick=GetTickCount64();
  const bool scope_wait=mode=="jitter_scope_wait",scope_lost=mode=="jitter_scope_lost";
  const bool scoped=scope_wait||scope_lost||scaled_render||scaled_allocation;
  owned_context=cfg.renderer+0x710;owned_view=view.p;projection_owned=!scope_wait;
  tb::jitter::fixture_start(cfg,reinterpret_cast<void *>(JitterFixture::forward),mode=="jitter_timeout"?tick-2001:tick,scoped?projection_admission:nullptr);
  if(scope_wait){
    for(unsigned n=0;n<2;++n){fixture.stage(3+n);auto f=fixture.frame();check(invoke(&f),"unowned projection forwards");}
    check(tb::jitter::healthy()&&!tb::jitter::wanted(0),"installation gap admits no baseline");
    check(get<std::uint64_t>(fixture.camera.p+0x38)==0,"installation gap writes no jitter");
    projection_owned=true;
  }
  const bool delay=mode=="jitter_selection_delay" || mode=="jitter_selection_xsmall" || mode=="jitter_selection_ysmall";
  const bool positive=mode=="jitter_success" || delay || mode=="jitter_selection_threshold" || mode=="jitter_selection_last" || scaled_render || scaled_allocation || scope_wait;
  const auto small=std::uint64_t{std::bit_cast<unsigned>(.09375F)}|(std::uint64_t{std::bit_cast<unsigned>(.125F)}<<32);
  if(delay){
    auto pair=small;
    if(mode=="jitter_selection_xsmall")pair=std::uint64_t{std::bit_cast<unsigned>(.312499F)}|(std::uint64_t{std::bit_cast<unsigned>(-.4F)}<<32);
    if(mode=="jitter_selection_ysmall")pair=std::uint64_t{std::bit_cast<unsigned>(.4F)}|(std::uint64_t{std::bit_cast<unsigned>(-.312499F)}<<32);
    put(fixture.table.p+0xe6a0+8*5,pair);fixture.stage(4);auto f=fixture.frame();check(invoke(&f),"deferred forward");
    check(!tb::jitter::wanted(0) && tb::jitter::healthy() && fixture.calls==1,"defer without admission");
    check(get<std::uint64_t>(fixture.camera.p+0x38)==0,"defer without write");
  }
  if(mode=="jitter_selection_threshold")put(fixture.table.p+0xe6a0+8*6,
    std::uint64_t{std::bit_cast<unsigned>(.3125F)}|(std::uint64_t{std::bit_cast<unsigned>(-.3125F)}<<32));
  if(mode=="jitter_selection_invalid")put(fixture.table.p+0xe6a0+8*6,std::uint64_t{0x7fc00000});
  if(mode=="jitter_selection_unreadable"){
    DWORD old{};check(VirtualProtect(reinterpret_cast<void *>(fixture.table.p+0xe000),0x1000,PAGE_NOACCESS,&old)!=0,"unreadable lookahead");
  }
  if(mode=="jitter_selection_limit" || mode=="jitter_selection_last"){
    for(unsigned n=0;n<20;++n)put(fixture.table.p+0xe6a0+8*n,small);
    for(unsigned n=0;n<(mode=="jitter_selection_last"?19U:20U);++n){
      fixture.stage(5+n);auto f=fixture.frame();check(invoke(&f),"selection bound forward");
      check(!tb::jitter::wanted(0) && get<std::uint64_t>(fixture.camera.p+0x38)==0,"no early stimulus");
    }
    if(mode=="jitter_selection_last")put(fixture.table.p+0xe6a0+8*5,
      std::uint64_t{std::bit_cast<unsigned>(.375F)}|(std::uint64_t{std::bit_cast<unsigned>(-.4375F)}<<32));
  }
  for(unsigned i=0;i<(positive?tb::capacity+1:tb::capacity);++i){
    if(scope_lost&&i==1)projection_owned=false;
    fixture.stage((mode=="jitter_selection_last"?24U:5U)+i+(mode=="jitter_frame_skip" && i?1:0));
    if(i==1 && mode=="jitter_nonzero")put(fixture.camera.p+0x38,std::uint64_t{1});
    if(i==1 && mode=="jitter_table")put(fixture.table.p+0xe6a0+8*6,std::uint64_t{0x7fc00000});
    if(i==1 && mode=="jitter_selection_changed")put(fixture.table.p+0xe6a0+8*6,
      std::uint64_t{std::bit_cast<unsigned>(-.375F)}|(std::uint64_t{std::bit_cast<unsigned>(.4375F)}<<32));
    if(i==7 && mode=="jitter_late_duplicate")put(fixture.table.p+0xe6a0+8*12,get<std::uint64_t>(fixture.table.p+0xe6a0+8*6));
    auto f=fixture.frame();if(mode=="jitter_foreign")++f.call.caller;
    const bool returned=invoke(&f);
    if((i==1 && mode=="jitter_exception") || (i==7 && mode=="jitter_late_exception"))check(!returned,"SEH propagated");else check(returned,"original returned");
    if(mode!="jitter_nonzero" && mode!="jitter_competing_write")check(get<std::uint64_t>(fixture.camera.p+0x38)==0,"scoped restoration");
    if(i<tb::capacity && tb::jitter::wanted(i)){
      auto s=fixture.snapshot();if(mode=="jitter_match" && i==1)++s.epoch;
#ifdef MHW_AA_PROJECTION
      mhw::aa_control::PostQueue q;q.renderer=cfg.renderer;q.context=s.context;q.view=s.view;
      q.frame=s.frame;q.epoch=s.epoch;q.bank=s.bank;
      tb::Snapshot linked;
      for(const auto invalid:{0U,1U,2U,3U}){
        auto bad=q;if(invalid==0)bad.taa=1;if(invalid==1)bad.fxaa=1;if(invalid==2)bad.context=1;if(invalid==3)bad.bank=2;
        check(!mhw::aa_control::projection_link(bad,s.async,linked),"invalid independent projection identity");
      }
      check(!mhw::aa_control::projection_link(q,2,linked),"invalid async mode");
      check(mhw::aa_control::projection_link(q,s.async,linked),"independent projection identity");
      check(linked==s,"independent immutable fields");
      s=linked;
      if(i==1){
        if(mode=="jitter_link_frame")++s.frame;
        if(mode=="jitter_link_epoch")++s.epoch;
        if(mode=="jitter_link_bank")s.bank^=1;
        if(mode=="jitter_link_async")s.async^=1;
        if(mode=="jitter_link_context")++s.context;
        if(mode=="jitter_link_view")++s.view;
      }
#endif
      if(mode!="jitter_unmatched"){
        const bool matched=tb::jitter::acknowledge(i,s);
        check(matched==!((mode=="jitter_match" || mode.starts_with("jitter_link_")) && i==1),"frame association");
        if(mode=="jitter_duplicate_ack" && i==1)check(!tb::jitter::acknowledge(i,s),"duplicate receipt rejected");
      }
    }
  }
  tb::jitter::HistorySample sample;check(!tb::jitter::history_sample(0,sample),"history unavailable before closure");
  check(tb::jitter::finish()==positive,"stimulus qualification");check(tb::jitter::settled(),"drained");
  for(unsigned i=0;i<tb::capacity;++i){check(tb::jitter::history_sample(i,sample)==positive,"history closure admission");
    if(positive)check(sample.frame==(mode=="jitter_selection_last"?24U:5U)+i && sample.epoch==sample.frame,"history frame identity");}
  check(!tb::jitter::history_sample(tb::capacity,sample),"history sample bound");
  tb::jitter::write(std::cout);std::cout<<'\n';
  if(argc==3){std::ofstream out(argv[2]);out<<std::boolalpha;tb::jitter::write(out);}
  std::cout<<mode<<" passed\n";
}
