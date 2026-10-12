#pragma once
#include "../src/continuous_dlaa/depth_consumer_admission.hpp"
#include <map>
#include <vector>
#include <cstring>
#include <iostream>
#include <sstream>
#include <cstdlib>
namespace cd=mhw::continuous_dlaa;namespace tb=mhw::temporal_bindings;namespace dr=tb::depth_routes;
inline void check(bool ok,const char *why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
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
struct ConsumerFixture {
    static constexpr std::uintptr_t renderer=0x100000,schedule=0x200000,packets=0x300000,backing=0x400000,
        tracker=0x500000,spans=0x600000,owner=0x700000,nodes=0x800000,viewport=0x900000,nested=0xa00000,
        source_owner=0xd00000,source_backing=0xd01000;
    static constexpr unsigned writer_index=2,consumer_index=56,next_index=59;
    static constexpr std::uintptr_t writer_object=packets+writer_index*0x100,consumer_object=packets+consumer_index*0x100;
    Memory m;tb::Snapshot source;tb::Owner depth;dr::CopyPacket writer;
    ConsumerFixture(){
        m.block(renderer,0x7b000);m.block(schedule,16*64);m.block(packets,0x100*64);m.block(backing,0x100);
        m.block(tracker,0x40);m.block(spans,0x100);m.block(owner,0x100);m.block(nodes,0x100);m.block(viewport,0x100);m.block(nested,0x80);
        m.block(source_owner,0x100);m.block(source_backing,0x100);
        m.put(source_owner+0x60,source_backing);m.put(source_backing+0xb0,2560U);m.put(source_backing+0xb4,1440U);
        source.context=renderer+0x710;source.frame=source.epoch=9;source.rectangle={0,0,1280,720};
        m.put(renderer+0x1f208,std::uint64_t{10});m.put(renderer+0x7af18,schedule);m.put(renderer+0x7af28,61U);
        for(unsigned i=0;i<61;++i){m.put(schedule+16*i+8,packets+0x100*i);m.put(packets+0x100*i,6U);}
        m.put(owner,std::uintptr_t{0xb00000});m.put(owner+0x60,backing);m.put(owner+0x88,2560U|(1440U<<16));m.put(owner+0x90,41U);m.put(owner+0x94,41U);
        m.put(backing+0x20,std::uintptr_t{0xc00000});m.put(backing+0x70,tracker);m.put(backing+0xb0,2560U);m.put(backing+0xb4,1440U);
        check(tb::read_owner(m.reader(),owner,depth),"fixture owner");
        m.put(writer_object,10U);m.put(writer_object+0x10,8U);m.put(writer_object+0x18,owner);m.put(writer_object+0x20,std::uintptr_t{0xd00000});
        m.put(writer_object+0x28,std::array<unsigned,6>{0,0,1280,720,0,0});m.put(writer_object+0x40,nested);
        m.put(nested,0U);m.put(nested+8,nodes);m.put(nested+0x10,viewport);m.put(viewport+0x4c,source.rectangle);
        check(dr::packet(m.reader(),writer_object,writer),"fixture writer");
        m.put(tracker,spans);m.put(tracker+8,spans+0x28);m.put(tracker+0x10,std::uint64_t{10});m.put(tracker+0x20,std::uint64_t{10});
        m.put(tracker+0x28,static_cast<unsigned char>(1));m.put(tracker+0x18,std::uint16_t{12});m.put(tracker+0x1a,std::uint16_t{8});
        m.put(spans,nested);m.put(spans+8,writer_index);m.put(spans+0x10,spans+0x28);m.put(spans+0x20,std::uint16_t{12});m.put(spans+0x22,std::uint16_t{0x100});
        m.put(spans+0x28,packets+next_index*0x100);m.put(spans+0x30,next_index);m.put(spans+0x40,spans);m.put(spans+0x48,std::uint16_t{0x100});m.put(spans+0x4a,std::uint16_t{8});
        node(nodes,spans,12,0x100);node(nodes+0x20,spans+0x28,0x100,8);m.put(packets+next_index*0x100+8,nodes+0x20);
    }
    void node(std::uintptr_t n,std::uintptr_t span,unsigned before,unsigned after){m.put(n+8,backing);m.put(n+0x10,span);m.put(n+0x18,static_cast<std::uint16_t>(before));m.put(n+0x1a,static_cast<std::uint16_t>(after));}
    void hoist(unsigned index=3){m.put(packets+next_index*0x100+8,std::uintptr_t{});m.put(packets+index*0x100+8,nodes+0x20);}
    void read_span(unsigned first,unsigned last,unsigned native,unsigned transition){
        m.put(packets+next_index*0x100+8,std::uintptr_t{});
        m.put(spans+0x28,packets+first*0x100);m.put(spans+0x30,last);m.put(spans+0x4a,static_cast<std::uint16_t>(native));
        m.put(tracker+0x1a,static_cast<std::uint16_t>(native));
        node(nodes+0x20,spans+0x28,0x100,native);m.put(packets+transition*0x100+8,nodes+0x20);
    }
    void third_span(unsigned first,unsigned last,unsigned before,unsigned after,unsigned transition){
        m.put(tracker+8,spans+0x50);m.put(tracker+0x1a,static_cast<std::uint16_t>(after));
        m.put(spans+0x38,spans+0x50);m.put(spans+0x50,packets+first*0x100);m.put(spans+0x58,last);
        m.put(spans+0x68,spans+0x28);m.put(spans+0x70,static_cast<std::uint16_t>(before));m.put(spans+0x72,static_cast<std::uint16_t>(after));
        node(nodes+0x40,spans+0x50,before,after);m.put(packets+transition*0x100+8,nodes+0x40);
    }
    static constexpr std::uintptr_t other_copy=packets+4*0x100,other_child=0xf00000,other_viewport=0xf01000;
    void direct_copy(unsigned kind=8){
        m.block(other_child,0x80);m.block(other_viewport,0x100);
        m.put(other_copy,10U);m.put(other_copy+0x10,kind);
        m.put(other_copy+0x18,owner);m.put(other_copy+0x20,source_owner);m.put(other_copy+0x40,other_child);
        // Deliberately poisonous unused header: native direct-copy execution
        // must not inspect child opcode/state nodes. The viewport is live.
        m.put(other_child,0xdeadbeefU);m.put(other_child+8,std::uintptr_t{1440});
        m.put(other_child+0x10,other_viewport);m.put(other_viewport+0x4c,std::array<unsigned,4>{0,0,2560,1440});
    }
    // Render scale 1.0 (game-75 survey): the writer is a whole copy of the
    // full rectangle (no child draw executes); its own span ends in COPY_DEST
    // (2048) with the transition hoisted before it (index 1); the next span's
    // 2048 -> read transition is hoisted between writer and consumer.
    static constexpr unsigned whole_read_at=30;
    void whole_copy(unsigned read=8,unsigned hoisted_at=whole_read_at){
        source.rectangle={0,0,2560,1440};
        m.put(writer_object+0x28,std::array<unsigned,6>{0,0,2560,1440,0,0});m.put(viewport+0x4c,source.rectangle);
        m.put(nested,0xdeadbeefU);m.put(nested+8,std::uintptr_t{1440});  // never inspected on the whole-copy path
        check(dr::packet(m.reader(),writer_object,writer),"whole-copy writer");
        m.put(spans,writer_object);m.put(spans+0x20,std::uint16_t{12});m.put(spans+0x22,std::uint16_t{2048});
        m.put(spans+0x48,std::uint16_t{2048});m.put(spans+0x4a,static_cast<std::uint16_t>(read));m.put(tracker+0x1a,static_cast<std::uint16_t>(read));
        node(nodes,spans,12,2048);m.put(packets+1*0x100+8,nodes);
        // One placement only: a repeated call moves the hoisted read transition.
        for(unsigned i=0;i<61;++i){std::uintptr_t head{};check(Memory::read(&m,packets+i*0x100+8,std::as_writable_bytes(std::span(&head,1))),"packet node head");
            if(head==nodes+0x20)m.put(packets+i*0x100+8,std::uintptr_t{});}
        node(nodes+0x20,spans+0x28,2048,read);m.put(packets+hoisted_at*0x100+8,nodes+0x20);
    }
    unsigned attempt(cd::DepthConsumerAdmission &proof,std::span<const D3D12_RESOURCE_BARRIER> b={},bool available=true,bool description=true,bool whole=false){
        return proof.attempt(m.reader(),source,depth,writer_object,writer,consumer_object,b,6,0xe00000,7,100,available,description,whole);
    }
};
