#include "../src/scaled_temporal/prepared_queue.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>
namespace pq=mhw::scaled_temporal::prepared_queue;
namespace tb=mhw::temporal_bindings;
void check(bool ok,const char *why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct Memory {
    std::map<std::uintptr_t,std::vector<std::byte>> blocks;
    std::uintptr_t mutate{};unsigned mutations{};
    void block(std::uintptr_t p,unsigned size){blocks[p].resize(size);}
    template<class T>void put(std::uintptr_t p,T value){auto i=blocks.upper_bound(p);check(i!=blocks.begin(),"fixture pointer");--i;
        check(p-i->first+sizeof(T)<=i->second.size(),"fixture extent");std::memcpy(i->second.data()+p-i->first,&value,sizeof(value));}
    static bool read(void *self,std::uintptr_t p,std::span<std::byte> bytes){auto &m=*static_cast<Memory *>(self);auto i=m.blocks.upper_bound(p);
        if(i==m.blocks.begin())return false;--i;
        if(p-i->first>i->second.size()||bytes.size()>i->second.size()-(p-i->first))return false;
        std::memcpy(bytes.data(),i->second.data()+p-i->first,bytes.size());
        if(p==m.mutate&&++m.mutations==2)bytes[0]^=std::byte{1};return true;}
    tb::Reader reader(){return {this,read};}
};
struct Fixture {
    static constexpr std::uintptr_t image=0x10000000,renderer=0x20000000,context=renderer+0x710,
        manager=0x30000000,view=0x40000000,taa=0x50000000,entries=0x60000000,
        owners=0x70000000,array=0x80000000,packets=0x90000000;
    Memory m;tb::Config cfg{image,renderer,0,0};
    Fixture(){
        m.block(image+0x51cc238,8);m.block(renderer,0x20000);m.block(manager,0x100);m.block(view,0x700);
        m.block(taa,0x200);m.block(entries,0x1000);m.block(owners,0x1000);m.block(array,16*32);m.block(packets,0x1000);
        m.put(image+0x51cc238,manager);m.put(context+0x1d0,view);m.put(renderer+0x1f450,view);
        m.put(renderer+0x1f210,std::uint64_t{123});m.put(renderer+0x1f208,std::uint64_t{122});m.put(renderer+0x93,static_cast<unsigned char>(1));
        m.put(context+0x5c,std::array<unsigned,4>{0,0,1280,720});
        m.put(view+0x6b0,std::array<float,3>{.5F,1.F,.5F});m.put(manager+0x3c,5U);
        m.put(manager+0xc0,owners);m.put(manager+0xc8,owners+0x200);
        for(unsigned i=0;i<4;++i){const auto p=owners+i*0x200; m.put(p,image+0x3300000+i*8);m.put(p+0x60,p+0x100);
            m.put(p+0x88,2560U|(1440U<<16));m.put(p+0x90,26U);m.put(p+0x94,26U);m.put(p+0x120,p+0x180);}
        // A setup descriptor, temporal resolve, two arbitrary unknown units,
        // then the logical endpoint whose draw was queued by an earlier unit.
        entry(0,entries,0,taa+0x1c0,owners,owners);
        entry(1,taa+0x140,2,taa,owners,owners+0x200);
        entry(2,entries+0x100,100,taa+0x1d0,owners+0x200,owners);
        entry(3,entries+0x200,17000,taa+0x1e0,owners,owners+0x200);
        entry(4,entries+0x300,0x7fffff1,0,owners+0x200,owners+0x400);
        m.put(entries+0x320,std::array<unsigned char,3>{1,0,0});
        m.put(context+0x2b78,array);m.put(context+0x2b80,5U);
        // CPU insertion order deliberately differs from sorted GPU order.
        packet(0,0x80000000,0,2560,1440);
        packet(1,0x7ffffff1,0,1280,720);
        packet(2,0x78000064,0,1280,720);
        packet(3,0x78004268,0,1280,720);
        packet(4,0x78000002,13,0,0);
    }
    void entry(unsigned index,std::uintptr_t p,unsigned order,std::uintptr_t unit,std::uintptr_t in,std::uintptr_t out){
        m.put(manager+0x40+8*index,p);m.put(p,15U);m.put(p+4,order);m.put(p+8,unit);m.put(p+0x10,in);m.put(p+0x18,out);
        if(unit)m.put(unit,image+0x3300000+0x1000*index);
    }
    void packet(unsigned index,unsigned key,unsigned opcode,unsigned w,unsigned h){const auto p=packets+0x100*index;
        m.put(array+16*index,key);m.put(array+16*index+8,p);m.put(p,opcode);
        if(opcode==0){m.put(p+0x10,p+0x80);m.put(p+0xcc,std::array<unsigned,4>{0,0,w,h});}
    }
    pq::Result capture(pq::Capture &c){return pq::capture(m.reader(),cfg,manager,context,view,taa,c);}
};
int main(){
    {Fixture detail;constexpr std::uintptr_t meta=0xe000000;const auto packet=Fixture::packets+2*0x100;
     detail.m.block(meta,0x1000);detail.m.put(packet+0x20,meta+0x200);detail.m.put(packet+0x40,std::array<unsigned,2>{4,7});
     detail.m.put(packet+0x80,meta);detail.m.put(packet+0xc8,1U);detail.m.put(meta+0x60,meta+0x100);
     detail.m.put(meta+0x118,std::uintptr_t{123456});detail.m.put(meta+0x110,std::uintptr_t{456789});detail.m.put(meta+0x120,26U);
     detail.m.put(meta+0x208,meta+0x300);detail.m.put(meta+0x300,meta+0x400);detail.m.put(meta+0x458,meta+0x500);
     detail.m.put(meta+0x510,meta+0x600);detail.m.put(meta+0x608,meta+0x700);detail.m.put(meta+0x600,52U);
     pq::Capture sample;check(detail.capture(sample)==pq::Result::ok,"draw detail capture");
     const auto &d=sample.entries[2].commands.details[0];
     check(d.readable&&d.target[3]==123456&&d.target[4]==456789&&d.pixel[6]&&d.pixel[4]==meta+0x700&&d.arguments[1]==7,"target and shader metadata");
     detail.m.put(meta+0x608,std::uintptr_t{});check(detail.capture(sample)==pq::Result::ok&&!sample.entries[2].commands.details[0].pixel[6],"unknown shader remains diagnostic only");}
    Fixture f;pq::Capture c;check(f.capture(c)==pq::Result::ok,"generic queue captured");
    check(pq::chain_breaks(c)==0,"versioned scratch reuse remains a connected chain");
    check(c.entries[4].unit==0&&c.entries[4].commands.opcodes[0]==1,"endpoint has GPU work without a callback");
    check(c.entries[1].commands.opcodes[13]==1&&c.final_commands.opcodes[0]==1,"copy and earlier-enqueued final draw accounted");
    check(c.entries[2].commands.draws[0][4]==1280&&c.final_commands.draws[0][4]==2560,"active and final dimensions remain separate");
    check(c.entries[2].input.width==2560&&c.rectangle[2]==1280,"allocation extent is not active extent");
    auto base=f.m.blocks;
    const auto negative=[&](std::uintptr_t p,auto value,pq::Result expected){f.m.blocks=base;f.m.put(p,value);pq::Capture out;out.frame=987;
        check(f.capture(out)==expected,"reject malformed capture");check(out.frame==987,"no partial publication");};
    negative(Fixture::manager+0x3c,17U,pq::Result::bounds);
    negative(Fixture::context+0x2b80,65536U,pq::Result::bounds);
    negative(Fixture::manager+0x39,static_cast<unsigned char>(1),pq::Result::identity);
    negative(Fixture::renderer+0x1f450,Fixture::view+8,pq::Result::identity);
    negative(Fixture::manager+0x50,Fixture::taa+0x140,pq::Result::descriptor);
    negative(Fixture::taa+0x144,3U,pq::Result::descriptor);
    negative(Fixture::entries+0x200,32U,pq::Result::descriptor);
    negative(Fixture::view+0x6b0,0x7fc00000U,pq::Result::bounds);
    negative(Fixture::context+0x2b78,UINTPTR_MAX-16,pq::Result::bounds);
    negative(Fixture::array+8,std::uintptr_t{1},pq::Result::packet);
    f.m.blocks=base;f.m.mutate=Fixture::renderer+0x1f210;check(f.capture(c)==pq::Result::changed,"reject frame torn between captures");
    f.m.mutate=0;f.m.blocks=base;f.m.put(Fixture::entries+0x210,Fixture::owners+0x400);
    check(f.capture(c)==pq::Result::ok&&pq::chain_breaks(c)==1,"retain unfamiliar disconnected path as evidence");
    f.m.blocks=base;f.m.put(Fixture::owners+0x120,std::uintptr_t{});
    check(f.capture(c)==pq::Result::ok&&!c.scratch[0].readable&&c.scratch[0].owner==Fixture::owners,"failed owner metadata remains explicitly unknown");
    f.m.blocks=base;pq::Recorder recorder;
    for(unsigned i=0;i<128*20;++i)recorder.observe(f.m.reader(),f.cfg,Fixture::manager,Fixture::context,Fixture::view,Fixture::taa);
    std::ostringstream o;recorder.write(o);const auto s=o.str();
    check(s.find("\"visits\":2560,\"attempts\":20,\"accepted\":20")!=std::string::npos,"fixed sampling budget");
    check(s.find("\"attempt\":17,")==std::string::npos&&s.find("\"attempt\":20,")!=std::string::npos,"first sixteen plus latest");
    std::cout<<s<<'\n';
}
