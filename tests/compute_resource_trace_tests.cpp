#include "../src/scaled_temporal/compute_resource_trace.hpp"
#include <cstdlib>
#include <iostream>
using Trace=mhw::scaled_temporal::ComputeResourceTrace;
void check(bool ok,const char *why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
int main(){
 Trace t;check(!t.watch(0)&&t.watch(10)&&t.watch(10),"unique nonzero watch");
 t.observe({1,1,11,0,0,0,0,8,0},true);check(t.events().empty(),"ignore unwatched");
 t.observe({1,1,10,0,0,0,0,8,0},true);
 check(t.get(1,1,10).known&&t.get(1,1,10).value==8,"observed whole transition");
 check(!t.get(2,1,10).known&&!t.get(1,2,10).known,"no cross-list or generation inheritance");
 t.observe({1,1,10,0,0,0,4,16,0},true);
 check(!t.get(1,1,10).known&&t.events().back().result==3,"contradiction invalidates");
 t.observe({1,1,10,0,0,0,16,64,0},true);check(t.get(1,1,10).value==64,"new complete evidence");
 t.observe({1,1,10,0,1,0,64,8,0},false);check(!t.get(1,1,10).known,"split transition unknown");
 t.observe({1,1,10,0,0,0,64,8,0},true);
 t.observe({1,1,10,1,0,0,0,0,0},false);check(!t.get(1,1,10).known,"alias unknown");
 t.reset(1,1);t.observe({1,2,10,0,0,0,0,64,0},true);check(t.get(1,2,10).known,"reset retires prior journal");
 t.reset(1,3);check(t.stopped()&&t.lost()==1&&!t.get(1,2,10).known,"bad reset does not claim state");
 Trace bounded;check(bounded.watch(10),"watch");
 for(unsigned i=0;i<Trace::event_limit;++i)bounded.observe({1,1,10,0,0,0,0,0,0},true);
 check(bounded.events().size()==Trace::event_limit&&!bounded.stopped(),"bounded journal");
 bounded.observe({1,1,10,0,0,0,0,0,0},true);
 check(bounded.stopped()&&bounded.lost()==1&&!bounded.get(1,1,10).known,"overflow stops diagnostic state");
 Trace capacity;for(unsigned i=1;i<=Trace::resource_limit;++i)check(capacity.watch(i),"resource capacity");
 check(!capacity.watch(99),"resource overflow");capacity.stop();check(!capacity.watch(1),"stop blocks additions");
 std::cout<<"PASS bounded compute resource state observation\n";
}
