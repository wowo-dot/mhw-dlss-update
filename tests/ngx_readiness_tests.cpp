#include "../src/ngx_startup/readiness.hpp"
#include <iostream>
using namespace mhw::ngx_startup;
static_assert(admit(0,Slots::empty,0)==Admission::wait);
static_assert(admit(15,Slots::empty,1200)==Admission::ready);
static_assert(admit(15,Slots::populated,1200)==Admission::missed);
static_assert(admit(15,Slots::unreadable,1200)==Admission::wait);
static_assert(admit(15,Slots::empty,readiness_timeout_ms)==Admission::expired);
static_assert(admit(0,Slots::empty,readiness_timeout_ms)==Admission::expired);
static_assert(admit(16,Slots::empty,1)==Admission::wait);
int main(){
    // Every missing gate independently prevents installation.
    for(unsigned bit=0;bit<4;++bit)if(admit(15^(1U<<bit),Slots::empty,100)!=Admission::wait)return 1;
    // Readiness later in loading is admissible; a native-init race is not.
    const Admission delayed[]{admit(0,Slots::empty,0),admit(7,Slots::empty,10),admit(15,Slots::empty,20)};
    if(delayed[0]!=Admission::wait||delayed[1]!=Admission::wait||delayed[2]!=Admission::ready)return 2;
    if(admit(7,Slots::empty,10)!=Admission::wait||admit(15,Slots::populated,20)!=Admission::missed)return 3;
    std::cout<<"Readiness admission controls passed\n";return 0;
}
