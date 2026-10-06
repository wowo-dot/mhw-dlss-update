#include "reflex/presentation_ownership.hpp"
#include <cstdio>
#include <cstdlib>
using namespace mhw::reflex;
void require(bool value,const char* why){if(!value){std::fprintf(stderr,"%s\n",why);std::exit(1);}}
int main(){
    PresentationOwnership t;
    OwnershipCall v{5,9,17,0,0,123,456,789,111,{0,1}};
    for(unsigned n=0;n<100000;++n)t.record(v);
    require(t.calls==100000 && t.bucket_count==1 && t.buckets[0].count==100000,"exact counts after sampling");
    require(t.sample_count==18 && !t.dropped_samples && !t.dropped_buckets,"bounded exponential samples");
    require(t.samples[0].phase==0 && t.samples[0].returned==111 && t.samples[0].args[1]==1,"metadata preserved; recorder phase owns input");
    require(t.samples[4].sequence==8 && t.samples[17].sequence==65536,"sample schedule");
    t.phase=1;t.record(v);v.thread=18;t.record(v);v.caller=457;t.record(v);v.method=10;t.record(v);
    require(t.bucket_count==5 && t.buckets[1].phase==1,"phase, thread, caller, method independent");
    for(unsigned n=0;n<80;++n){v.caller=1000+n;t.record(v);}
    require(t.bucket_count==t.bucket_limit && t.dropped_buckets==21,"bucket saturation reported");
    PresentationOwnership saturated;
    for(unsigned n=0;n<64;++n){v.caller=n;for(unsigned k=0;k<65536;++k)saturated.record(v);}
    require(saturated.sample_count==saturated.sample_limit && saturated.dropped_samples==640,"sample saturation reported");
    require(!saturated.dropped_buckets && saturated.calls==64ULL*65536 && saturated.buckets[63].count==65536,"counts survive sample saturation");
}
