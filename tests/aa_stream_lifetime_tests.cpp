#include "../src/aa_control/stream_lifetime.hpp"
#include <iostream>
#include <cstdlib>
using mhw::aa_control::StreamLifetime;
void check(bool v,const char *why){if(!v){std::cerr<<why<<'\n';std::exit(1);}}
int main(){
    StreamLifetime s;
    for(unsigned i=1;i<=10000;++i){check(s.reserve(i),"reserve after recycle");check(!s.reserve(i+1),"no overwrite");
        check(!s.mark(i),"no consume before queue");check(s.queue(),"queue");check(!s.queue(),"no duplicate publication");
        check(s.mark(i),"mark");check(!s.record(),"wait for native consumer return");s.consumer_returned=true;check(s.record(),"record");
        check(!s.submit(i+1,i),"generation mismatch");check(!s.reset(i),"no early retirement");check(s.submit(i,i),"submit");
        check(!s.submit(i,i+1),"duplicate submission");check(!s.recyclable(i),"fence alone insufficient");check(s.reset(i),"retirement");
        check(!s.recyclable(i),"CPU producer still holds descriptor");s.producer_returned=true;
        check(!s.recyclable(i),"Signal return still pending");check(s.confirm(i),"Signal completed");check(!s.confirm(i),"duplicate signal receipt");
        check(!s.recyclable(i-1)&&!s.recyclable(UINT64_MAX),"GPU pending/device removed");check(s.recyclable(i),"all independent receipts");s={};}
    std::cout<<"10,000 reusable marker lifetimes passed\n";
}
