#include "../src/continuous_dlaa/depth_schedule_trace.hpp"
#include <map>
#include <vector>
#include <cstring>
#include <iostream>
#include <sstream>
#include <cstdlib>
namespace cd=mhw::continuous_dlaa;namespace tb=mhw::temporal_bindings;namespace dr=tb::depth_routes;
void check(bool ok,const char *why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
struct Memory {
    std::map<std::uintptr_t,std::vector<std::byte>> blocks;
    std::uintptr_t mutate{};unsigned reads{};
    void block(std::uintptr_t p,std::size_t n){blocks[p].resize(n);}
    template<class T>void put(std::uintptr_t p,T v){auto i=blocks.upper_bound(p);check(i!=blocks.begin(),"fixture address");--i;check(p-i->first+sizeof(v)<=i->second.size(),"fixture extent");std::memcpy(i->second.data()+p-i->first,&v,sizeof(v));}
    static bool read(void *context,std::uintptr_t p,std::span<std::byte> out){auto &m=*static_cast<Memory *>(context);
        auto i=m.blocks.upper_bound(p);if(i==m.blocks.begin())return false;--i;
        if(p-i->first>i->second.size()||out.size()>i->second.size()-(p-i->first))return false;
        std::memcpy(out.data(),i->second.data()+p-i->first,out.size());
        if(p==m.mutate&&out.size()==8){const std::uint64_t epoch=10+m.reads++;std::memcpy(out.data(),&epoch,8);}return true;}
    tb::Reader reader(){return {this,read};}
};
struct Fixture {
    static constexpr std::uintptr_t renderer=0x100000,schedule=0x200000,packets=0x300000,backings=0x400000,trackers=0x500000,spans=0x600000,owners=0x700000;
    Memory m;tb::Snapshot source;tb::SceneDepth depth;dr::CopyPacket writer;
    Fixture(){m.block(renderer,0x7b000);m.block(schedule,16*8);m.block(packets,0x100*8);m.block(backings,0x80*3);m.block(trackers,0x40*3);m.block(spans,0x28*2);m.block(owners,0x100*3);
        source.context=renderer+0x710;source.frame=source.epoch=9;source.inputs[0].object=owners+0x100;source.inputs[4].object=owners+0x200;
        m.put(renderer+0x1f208,std::uint64_t{10});m.put(renderer+0x7af18,schedule);m.put(renderer+0x7af28,7U);
        for(unsigned i=0;i<7;++i){m.put(schedule+16*i+8,packets+0x100*i);m.put(packets+0x100*i,6U);}
        for(unsigned i=0;i<3;++i){const auto o=owners+0x100*i,b=backings+0x80*i,t=trackers+0x40*i;
            m.put(o,std::uintptr_t{0x800000});m.put(o+0x60,b);m.put(o+0x88,2560U|(1440U<<16));m.put(o+0x90,41U);m.put(o+0x94,41U);
            m.put(b+0x20,std::uintptr_t{0x900000+0x100*i});m.put(b+0x70,t);m.put(t+0x10,std::uint64_t{10});m.put(t+0x20,std::uint64_t{10});m.put(t+0x28,static_cast<unsigned char>(1));}
        check(tb::read_owner(m.reader(),owners,depth.texture)&&tb::read_owner(m.reader(),owners+0x100,source.inputs[0])&&tb::read_owner(m.reader(),owners+0x200,source.inputs[4]),"owners");
        m.put(packets+0x200,10U);m.put(packets+0x210,8U);m.put(packets+0x218,owners);m.put(packets+0x220,std::uintptr_t{0xa00000});
        m.put(packets+0x228,std::array<unsigned,6>{0,0,1280,720,0,0});m.put(packets+0x240,packets+0x700);
        check(dr::packet(m.reader(),packets+0x200,writer),"writer packet");
        m.put(trackers,spans);m.put(trackers+8,spans+0x28);m.put(trackers+0x18,std::uint16_t{0x40});m.put(trackers+0x1a,std::uint16_t{0x80});m.put(trackers+0x1c,6U);
        m.put(spans,packets+0x200);m.put(spans+8,2U);m.put(spans+0x10,spans+0x28);m.put(spans+0x20,std::uint16_t{0x40});m.put(spans+0x22,std::uint16_t{4});
        m.put(spans+0x28,packets+0x600);m.put(spans+0x30,6U);m.put(spans+0x40,spans);m.put(spans+0x48,std::uint16_t{4});m.put(spans+0x4a,std::uint16_t{0x80});
    }
    void observe(cd::DepthScheduleTrace &trace,std::uint64_t tick,bool missing){trace.observe(m.reader(),source,depth,packets+0x200,writer,5,6,0xb00000,7,packets+0x400,tick,missing,{});}
};
void emit(const char *name,const cd::DepthScheduleTrace &trace){std::cout<<std::boolalpha<<"{\"case\":\""<<name<<"\",\"trace\":";trace.write(std::cout);std::cout<<"}\n";}
int main(){
    {Fixture f;cd::NativeStateTrace plan;auto mapped=f.source;mapped.inputs[0]=f.depth.texture;plan.capture(f.m.reader(),mapped,Fixture::packets+0x400);
        cd::DepthSchedulePositions positions;positions.capture(f.m.reader(),plan,Fixture::packets+0x200);
        check(positions.complete&&positions.stable&&positions.writer==cd::DepthSchedulePositions::Position{1,2},"writer before consumer despite callback timing");
        check(plan.index==4&&positions.spans[0][0][1]==2&&positions.spans[0][1][1]==6&&plan.trackers[0].final==0x80&&plan.trackers[0].spans[0][6]==4,"current interval distinct from final planned state");
        f.m.put(Fixture::schedule+16*6+8,Fixture::packets+0x200);cd::DepthSchedulePositions duplicate;duplicate.capture(f.m.reader(),plan,Fixture::packets+0x200);check(duplicate.writer[0]==2,"duplicate writer remains ambiguous");
    }
    {Fixture f;auto trace=std::make_unique<cd::DepthScheduleTrace>();f.observe(*trace,100,false);for(unsigned i=0;i<100;++i)f.observe(*trace,101+i,false);
        f.observe(*trace,200,true);f.observe(*trace,201,true);f.observe(*trace,1200,true);f.observe(*trace,1201,true);emit("bounded",*trace);}
    for(const auto *name:{"changed-writer","changed-owner","unstable-plan","late-writer","missing-writer","oversized-plan"}){
        Fixture f;auto trace=std::make_unique<cd::DepthScheduleTrace>();
        if(std::strcmp(name,"changed-writer")==0)f.m.put(Fixture::packets+0x230,1279U);
        if(std::strcmp(name,"changed-owner")==0)f.m.put(Fixture::owners+0x94,26U);
        if(std::strcmp(name,"unstable-plan")==0)f.m.mutate=Fixture::renderer+0x1f208;
        if(std::strcmp(name,"late-writer")==0){f.m.put(Fixture::schedule+16*2+8,Fixture::packets+0x600);f.m.put(Fixture::schedule+16*6+8,Fixture::packets+0x200);}
        if(std::strcmp(name,"missing-writer")==0)f.m.put(Fixture::schedule+16*2+8,Fixture::packets+0x700);
        if(std::strcmp(name,"oversized-plan")==0)f.m.put(Fixture::renderer+0x7af28,32769U);
        f.observe(*trace,300,true);emit(name,*trace);
    }
}
