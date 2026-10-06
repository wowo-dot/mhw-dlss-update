#include "jitter_sequence.hpp"
#include <iostream>
#include <map>
#include <vector>
#include <stdexcept>
#include <limits>
namespace b = mhw::bridge_probe;
using A = std::uintptr_t;
void check(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
struct Fixture {
  static constexpr A base=0x10000000, args=0x20000000, layer=0x30000000, wrapper=0x40000000, renderer=0x50000000, view=0x60000000;
  std::map<A,std::byte> memory;
  std::vector<A> reads;
  unsigned fail{}, mutate{};
  b::ProducerFrame frame{};
  template<class T> void put(A at,T value) {const auto bytes=std::as_bytes(std::span(&value,1));for(std::size_t i=0;i<bytes.size();++i)memory[at+i]=bytes[i];}
  Fixture() {
    frame.valid=true;frame.renderer=renderer;frame.view=view;
    put(args,wrapper);put(args+8,layer);put(wrapper,A{777});put(layer,base+A{0x351bda8});
    put(layer+0xc,8U);put(layer+0x14,0x12U);put(layer+0x180,.95f);put(layer+0x184,static_cast<unsigned char>(1));
    put(renderer+0xa5,static_cast<unsigned char>(1));put(view+0x58,0U);put(renderer+0x1f450,view);
    put(base+0x503c3e8,0.f);put(base+0x503c3ec,0.f);
  }
  static b::ScalarReadResult read(void *ctx,A at,std::span<std::byte> out) noexcept {
    auto &f=*static_cast<Fixture *>(ctx);f.reads.push_back(at);
    if(f.fail==f.reads.size())return b::ScalarReadResult::fault;
    for(std::size_t i=0;i<out.size();++i){auto it=f.memory.find(at+i);if(it==f.memory.end())return b::ScalarReadResult::fault;out[i]=it->second;}
    if(f.mutate==f.reads.size())out[0]^=std::byte{1};
    return b::ScalarReadResult::success;
  }
  b::JitterGates capture(){reads.clear();return b::capture_jitter_gates(base,args,frame,read,this);}
};
void gates() {
  Fixture f;const auto good=f.capture();const auto order=f.reads;
  check(good.valid&&good.predicate&&good.reads==26,"all native gates captured");
  for(unsigned i=1;i<=good.reads;++i){Fixture x;x.fail=i;check(!x.capture().valid,"each failed read rejected");}
  std::map<A,unsigned> seen;
  for(unsigned i=0;i<order.size();++i)if(++seen[order[i]]==2){Fixture x;x.mutate=i+1;check(!x.capture().valid,"each changed reread rejected");}
  for(const auto flags:{0x40000U,0x800000U}){Fixture x;x.put(Fixture::view+0x58,flags);const auto s=x.capture();check(s.valid&&!s.predicate,"view flags block jitter without invalidating observation");}
  {Fixture x;x.put(Fixture::layer+0x180,0.f);check(!x.capture().predicate,"zero blend gate");}
  {Fixture x;x.put(Fixture::layer+0x184,static_cast<unsigned char>(0));check(!x.capture().predicate,"disabled jitter gate");}
  {Fixture x;x.put(Fixture::layer+0xc,7U);check(!x.capture().predicate,"layer type gate");}
  {Fixture x;x.put(Fixture::layer+0x14,2U);check(!x.capture().predicate,"layer flags gate");}
  {Fixture x;x.put(Fixture::layer,A{1});check(x.capture().rejection==4,"unknown layer vtable retained and rejected");}
  {Fixture x;x.put(Fixture::layer+0x180,std::numeric_limits<float>::quiet_NaN());check(x.capture().rejection==5,"nonfinite gate invalid");}
  {Fixture x;x.put(Fixture::wrapper,A{0});const auto s=x.capture();check(s.valid&&!s.wrapper,"no wrapper is valid Off observation");}
}
void complete(b::JitterSequence &q,b::JitterSample s,std::uint64_t frame,bool valid=true) {
  s.frame.valid=valid;s.frame.frame=frame;s.frame.projection.valid=valid;s.gates.valid=valid;s.gates.wrapper=s.phase?0:1;q.commit(s);
}
void budgets() {
  b::JitterSequence q; b::JitterSample s;
  check(!q.reserve(1,false,s),"no pre-activation reads");
  check(!q.arm_scene(999),"cannot arm before initial burst");
  for(unsigned phase=0;phase<2;++phase)for(unsigned i=0;i<16;++i){
    const auto tick=1000ULL+phase*2000ULL+i*100;
    if(phase==1 && i==0) {
      check(!q.reserve(tick,true,s)&&!q.reserve(tick,false,s),"both wrapper states wait for explicit trigger");
      check(!q.arm_scene(2499)&&q.arm_scene(tick)&&!q.arm_scene(tick),"one scene trigger after initial burst");
    }
    check(q.reserve(tick,phase==0,s),"phase reservation");
    b::JitterSample nested;check(!q.reserve(tick+1,phase==0,nested),"no overlapping reservation");
    complete(q,s,i+1);
    check(!q.reserve(tick+1,phase==0,nested),"rate limit independent of callbacks");
  }
  check(q.state().count==32&&q.state().stop==1&&q.state().initial==16&&q.state().scene==16,"fixed phase budgets");
  check(!q.arm_scene(100000),"trigger never replenishes completed sequence");
  check(!q.reserve(100000,true,s)&&!q.reserve(100001,false,s),"recreation never replenishes samples");
  check(q.sample(0,s)&&s.valid&&s.frame.frame==1,"immutable first sample");s.frame.frame=99;
  check(q.sample(0,s)&&s.frame.frame==1,"reader gets copy");
  b::JitterSequence duplicates;
  for(unsigned i=0;i<3;++i){check(duplicates.reserve(1000+i*100,true,s),"duplicate reservation");complete(duplicates,s,i==1?8:9);}
  check(duplicates.sample(1,s)&&s.duplicate&&!s.valid,"regression rejected");
  check(duplicates.sample(2,s)&&s.duplicate&&!s.valid,"regression cannot lower high-water mark");
  b::JitterSequence faults;
  for(unsigned i=0;i<16;++i){check(faults.reserve(1000+i*100,true,s),"invalid reservation");complete(faults,s,i,false);}
  check(!faults.reserve(5000,true,s),"failures consume budget");
  faults.poll(121000);check(faults.state().stop==2&&!faults.reserve(121001,false,s)&&!faults.arm_scene(121001),"deadline closes missing scene phase");
  b::JitterSequence backwards;check(backwards.reserve(1000,true,s),"clock start");complete(backwards,s,1);backwards.poll(999);check(backwards.state().stop==2,"backward clock stops");
  b::JitterSequence closed;closed.close();check(!closed.reserve(1000,true,s)&&closed.state().stop==3,"closed trace cannot start");
  b::JitterSequence mixed;
  for(unsigned i=0;i<16;++i){check(mixed.reserve(1000+i*100,i==0,s),"initial burst survives wrapper loss");complete(mixed,s,i);}
  check(mixed.arm_scene(3000),"scene starts independently of wrapper presence");
  check(mixed.reserve(3000,true,s),"scene accepts present wrapper");
  s.frame.valid=s.frame.projection.valid=s.gates.valid=true;s.frame.frame=20;s.gates.wrapper=77;mixed.commit(s);
  check(mixed.sample(16,s)&&s.valid&&s.gates.wrapper==77,"scene classification does not imply Off");
  check(!mixed.reserve(2999,true,s)&&mixed.state().stop==2,"clock before trigger stops");
}
int main(){try{gates();budgets();std::cout<<"Jitter gates and bounded sequence checks passed\n";}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
