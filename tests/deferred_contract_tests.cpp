#include "../src/aa_control/deferred_contract.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
namespace d=mhw::aa_control::deferred;
void check(bool value,const char *why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
int main(){
    std::array<d::Marker,8> requests;
    for(unsigned i=0;i<requests.size();++i){auto &p=requests[i];p.generation=i+1;
        for(unsigned j=0;j<requests.size();++j)check(d::valid(p,j)==(i==j),"generation prevents cross-frame match");
        auto bad=p;bad.count=1;check(!d::valid(bad,i),"coalesced native parameter cannot be marker");
        bad=p;bad.barriers=8;check(!d::valid(bad,i),"unexpected resource plan rejected");
        bad=p;bad.opcode=10;check(!d::valid(bad,i),"resource-copy opcode rejected");
        bad=p;bad.cookie=0;check(!d::valid(bad,i),"packet corruption rejected");
    }
    check(d::admit(200,328,500,79,0),"exact queue margin");
    for(const auto count: {201U,328U,329U,0xffffffffU})check(!d::admit(count,328,500,79,0),"no wrap/full/low margin");
    check(!d::admit(200,501,500,79,0)&&!d::admit(200,328,1048577,79,0),"queue bounds");
    for(unsigned phase=0;phase<32;++phase)for(unsigned order=0;order<128;++order)
        check(d::admit(200,328,500,(order<<5)|phase,0)==(phase==15&&order==2),"only intended downstream position");
    check(!d::admit(200,328,500,79,32),"offset cannot silently change order");
    for(unsigned flags=0;flags<32;++flags)check(d::key(79,flags)==0x78000002,"offset flag bits preserve key");
    std::cout<<"Deferred packet identity, placement, queue limits passed\n";
}
