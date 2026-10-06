#include "../src/scaled_temporal/stream_lifetime.hpp"
#include <cstdlib>
#include <iostream>
using namespace mhw::scaled_temporal;
void check(bool v,const char *why){if(!v){std::cerr<<why<<'\n';std::exit(1);}}
struct Object:IUnknown {
 ULONG references{1};unsigned &destroyed;explicit Object(unsigned &n):destroyed(n){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void **) override{return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef() override{return ++references;}
 ULONG STDMETHODCALLTYPE Release() override{const auto n=--references;if(!n){++destroyed;delete this;}return n;}
};
int main(){
 unsigned destroyed{};auto *list=new Object(destroyed);auto *resource=new Object(destroyed);
 StreamLifetime pins;std::array<IUnknown *,2> objects{resource,resource};
 check(!pins.retain(nullptr,1,objects)&&!pins.retain(list,0,objects),"reject invalid identities");
 check(pins.retain(list,1,objects)&&pins.retain(list,1,objects),"same recording merges references");
 check(resource->references==2&&list->references==2,"duplicates not pinned twice");
 check(!pins.reset(list,1),"unsubmitted Reset rejected");
 check(pins.submitted(list,1,4)&&!pins.submitted(list,1,5),"exactly one submission");
 check(!pins.retain(list,1,objects),"cannot append after submission");
 pins.collect(100);check(pins.live()==1,"GPU complete is not retirement");
 check(pins.reset(list,1),"successful Reset");pins.collect(3);pins.collect(UINT64_MAX);check(pins.live()==1,"pending or removed device retained");
 pins.collect(4);check(!pins.live()&&resource->references==1&&list->references==1,"both proofs release");
 for(unsigned i=2;i<=129;++i){check(pins.retain(list,i,objects),"128 pending generations");check(pins.submitted(list,i,i)&&pins.reset(list,i),"submitted generations");}
 check(!pins.retain(list,130,objects),"exhaustion never evicts");pins.collect(65);check(pins.live()==64,"fence covers only some generations");pins.collect(129);check(!pins.live(),"remaining references released");
 for(unsigned i=130;i<22130;++i){auto *output=new Object(destroyed);const std::array<IUnknown *,2> values{resource,output};
  check(pins.retain(list,i,values),"continuous reusable storage");output->Release();
  check(pins.submitted(list,i,i)&&pins.reset(list,i),"continuous generation");pins.collect(i);
 }
 check(destroyed==22000&&!pins.live()&&pins.retained()==22129&&pins.released()==22129&&pins.peak()==128,"22,000 changing outputs, no leak");
 // A rejected multi-pin admission cannot leave a partial entry or extra refs.
 std::array<IUnknown *,96> too_many;for(auto &o:too_many)o=new Object(destroyed);
 check(!pins.retain(list,30000,too_many)&&!pins.live(),"capacity admission is atomic including list pin");for(auto *o:too_many)o->Release();
 resource->Release();list->Release();check(destroyed==22098,"every reference returned");
 std::cout<<"stream lifetime passed: 22,000 frames plus capacity and retirement failures\n";
}
