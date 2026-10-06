#include "../src/scaled_temporal/dependency_binding_trace.hpp"
#include <iostream>
#include <stdexcept>
using namespace mhw::scaled_temporal;
void require(bool b){if(!b)throw std::runtime_error("dependency binding trace contract");}
int main(){try{
 BindingCheck empty;require(!empty.accepted());
 BindingCheck ok;ok.observed=ok.readable=ok.contract=ok.equal=true;require(ok.accepted());
 auto changed=ok;changed.equal=false;
 for(unsigned i=0;i<20;++i)changed.difference(3,i,100+i,200+i);
 changed.difference(9,0,5,6);changed.difference(1,0,1,1);
 require(changed.stored==16&&changed.differences==21&&changed.fields==((1U<<3)|(1U<<9)));
 require(changed.changes[0]==std::array<std::uint64_t,4>{3,0,100,200}&&changed.changes[15][1]==15);
 DependencyBindingTrace t;t.entry(ok);t.draw({1,1,1,4,1,10,20,30},ok,ok);require(t.count==0);
 for(unsigned i=0;i<5;++i){t.entry(ok);t.draw({2+i,2+i,1,5,2,11,21,31},ok,changed);}
 require(t.count==3&&t.draw_checks==6&&t.draw_rejections==5&&t.entry_checks==6&&!t.entry_rejections);
 require(t.samples[0].at[1]==2&&t.samples[2].at[1]==4&&t.samples[0].entry.accepted());
 std::cout<<std::boolalpha;t.write(std::cout);std::cout<<'\n';
 DependencyBindingTrace reads;auto failed=ok;failed.readable=failed.contract=failed.equal=false;failed.read={3,0};
 reads.entry(failed);reads.draw({7,7,1,5,2,11,21,31},failed,failed);reads.write(std::cout);std::cout<<'\n';
 require(reads.entry_rejections==1&&reads.draw_rejections==1);return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
