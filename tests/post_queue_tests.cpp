#include "../src/aa_control/post_queue.hpp"
#include <map>
#include <iostream>
#include <cstdlib>
namespace aa=mhw::aa_control;
namespace tb=mhw::temporal_bindings;
void check(bool value,const char *message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
struct Memory {
    std::map<std::uintptr_t,std::byte> bytes;
    template<class T>void put(std::uintptr_t at,T value){for(auto b:std::as_bytes(std::span(&value,1)))bytes[at++]=b;}
    static bool read(void *p,std::uintptr_t at,std::span<std::byte> out){auto &m=*static_cast<Memory *>(p);
        for(auto &b:out){const auto found=m.bytes.find(at++);if(found==m.bytes.end())return false;b=found->second;}return true;}
    tb::Reader reader(){return {this,read};}
};
constexpr std::uintptr_t image=0x140000000,manager=0x100000,renderer=0x200000,context=renderer+0x710,view=0x300000,table=0x400000;
Memory fixture(){
    Memory m;const auto byte=[&](auto at,unsigned char n){m.put(at,n);};
    byte(image+0x51cc080,1);m.put(image+0x51cc238,manager);m.put(image+0x51c4480,renderer);m.put(image+0x51c4368,table);
    m.put(renderer+0x1f450,view);m.put(context+0x1d0,view);byte(table+0xe9e4,0);byte(table+0xe9e5,0);
    m.put(renderer+0x1f210,std::uint64_t{42});m.put(renderer+0x1f208,std::uint64_t{43});m.put(renderer+0x1f200,1U);
    byte(manager+0x38,1);byte(manager+0x39,1);m.put(manager+0x3c,2U);m.put(context+0x2b80,123U);
    for(unsigned i=0;i<4;++i)m.put(context+0x5c+4*i,std::array<unsigned,4>{0,0,2560,1440}[i]);
    for(unsigned i=0;i<2;++i){const auto d=std::uintptr_t{0x500000}+i*0x100,unit=d+0x40;
        m.put(manager+0xc0+8*i,d+0x80);m.put(view+0x3e0+0x40*i,d+0x90);m.put(d+0x90+0x58,d+0xa0);
        m.put(manager+0x40+8*i,d);m.put(d,15U);m.put(d+4,i);m.put(d+8,unit);m.put(d+0x10,d+0x80);m.put(d+0x18,d+0xa0);
        m.put(unit,image+0x1000+i*8);for(unsigned j=0;j<3;++j)byte(d+0x20+j,0);
    }return m;
}
int main(){
    auto m=fixture();aa::PostQueue q;
    check(aa::read_post_queue(m.reader(),image,manager,context,view,q),"valid queue");
    check(q.count==2&&q.frame==42&&q.taa==0&&q.fxaa==0,"Off queue decoded");
    check(aa::qualify_owned_slot(q,image,0x700000)==aa::QueueAdmission::ready,"space independent of AA selection");
    for(const auto &[at,value]:m.bytes){auto bad=m;bad.bytes.erase(at);aa::PostQueue out;
        check(!aa::read_post_queue(bad.reader(),image,manager,context,view,out),"every required byte is checked");}
    for(auto count:{17U,UINT_MAX}){auto bad=m;bad.put(manager+0x3c,count);aa::PostQueue out;
        check(!aa::read_post_queue(bad.reader(),image,manager,context,view,out),"bounded queue");}
    auto bad=m;bad.put(manager+0x48,std::uintptr_t{0x500000});aa::PostQueue out;
    check(!aa::read_post_queue(bad.reader(),image,manager,context,view,out),"duplicate descriptor rejected");
    for(auto vt:{0x351bda8U,0x351c1a8U,0x354dd78U,0x354df48U}){auto copy=q;copy.nodes[1].vtable=image+vt;
        check(aa::qualify_owned_slot(copy,image,0x700000)==aa::QueueAdmission::native_aa_present,"no stacked native AA");}
    for(unsigned taa=0;taa<2;++taa)for(unsigned fxaa=0;fxaa<2;++fxaa){auto copy=q;copy.taa=static_cast<unsigned char>(taa);copy.fxaa=static_cast<unsigned char>(fxaa);
        check(aa::qualify_owned_slot(copy,image,0x700000)==aa::QueueAdmission::ready,"choice is not scheduling prerequisite");}
    check(aa::qualify_owned_slot(q,image,q.nodes[1].descriptor)==aa::QueueAdmission::duplicate,"no double registration");
    auto copy=q;copy.count=16;check(aa::qualify_owned_slot(copy,image,0x700000)==aa::QueueAdmission::full,"no seventeenth node");
    copy=q;copy.collecting=0;check(aa::qualify_owned_slot(copy,image,0x700000)==aa::QueueAdmission::wrong_phase,"cannot modify consuming queue");
    copy=q;copy.packet_count++;copy.collecting=0;check(aa::same_post_frame(q,copy),"packet production expected during phase");
    copy.frame++;check(!aa::same_post_frame(q,copy),"cross-frame rejected");
    std::cout<<"Post queue bounds, AA-independent admission, duplicate/native-AA rejection and frame matching passed\n";
}
