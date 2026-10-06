#pragma once
#include "../temporal_bindings/inputs.hpp"
#include <algorithm>
#include <ostream>
namespace mhw::continuous_dlaa {
// Bounded evidence carrier: planner state may describe later packets. This
// reader itself grants no admission; native_state_admission.hpp interprets a
// narrow current-use contract. Stability remains observational, not atomicity.
struct NativeStateTrace {
    static constexpr unsigned span_capacity=16,packet_capacity=32768,earlier_capacity=32,node_capacity=16;
    struct Tracker {
        std::uintptr_t address{},first{},last{};
        std::uint64_t epoch{},used_epoch{},immediate_epoch{};
        unsigned initial{},final{},last_use{},initialized{},immediate{},count{};
        bool readable{},stable{},complete{};
        // address,packet,last-use ordinal,next,previous,before,after,first-use flag
        std::array<std::array<std::uint64_t,8>,span_capacity> spans{};
        bool operator==(const Tracker &)const=default;
    };
    struct Earlier {
        unsigned index{},opcode{},visited{},relevant{},count{};std::uintptr_t packet{};bool complete{};
        // chain index,role mask,before,after,planner span pointer
        std::array<std::array<std::uint64_t,5>,node_capacity> nodes{};
        bool operator==(const Earlier &)const=default;
    };
    std::uintptr_t renderer{},array{};
    std::uint64_t epoch{};
    unsigned packets{},scanned{},index{UINT_MAX},matches{},earlier_count{};
    bool readable{},scan_complete{},stable{};
    std::array<Tracker,3> trackers{};
    std::array<Earlier,earlier_capacity> earlier{};
    bool operator==(const NativeStateTrace &)const=default;
    template<class T>static bool field(const temporal_bindings::Reader &r,std::uintptr_t p,unsigned offset,T &value) noexcept {
        return p&&p<=UINTPTR_MAX-offset-sizeof(T)&&r.get(p+offset,value);
    }
    static bool tracker(const temporal_bindings::Reader &r,std::uintptr_t backing,Tracker &t) noexcept {
        std::uint16_t initial{},final{},immediate{};unsigned char initialized{};
        if(!field(r,backing,0x70,t.address)||!t.address||!field(r,t.address,0,t.first)||!field(r,t.address,8,t.last)||
           !field(r,t.address,0x10,t.epoch)||!field(r,t.address,0x18,initial)||!field(r,t.address,0x1a,final)||
           !field(r,t.address,0x1c,t.last_use)||!field(r,t.address,0x20,t.used_epoch)||!field(r,t.address,0x28,initialized)||
           !field(r,t.address,0x2a,immediate)||!field(r,t.address,0x30,t.immediate_epoch))return false;
        t.initial=initial;t.final=final;t.initialized=initialized;t.immediate=immediate;return true;
    }
    static bool same(const Tracker &a,const Tracker &b) noexcept {
        return a.address==b.address&&a.first==b.first&&a.last==b.last&&a.epoch==b.epoch&&a.initial==b.initial&&a.final==b.final&&
            a.last_use==b.last_use&&a.used_epoch==b.used_epoch&&a.initialized==b.initialized&&a.immediate==b.immediate&&a.immediate_epoch==b.immediate_epoch;
    }
    void capture(const temporal_bindings::Reader &r,const temporal_bindings::Snapshot &s,std::uintptr_t packet) noexcept {
        constexpr unsigned roles[]{0,4,5};
        for(unsigned i=0;i<3;++i){auto &t=trackers[i];t.readable=tracker(r,s.inputs[roles[i]].backing,t);if(!t.readable)continue;
            auto node=t.first;
            while(node&&t.count<span_capacity){std::array<std::uint64_t,8> row{};std::uint32_t last{};std::uint16_t before{},after{};unsigned char first{};
                row[0]=node;
                if(!field(r,node,0,row[1])||!field(r,node,8,last)||!field(r,node,0x10,row[3])||!field(r,node,0x18,row[4])||
                   !field(r,node,0x20,before)||!field(r,node,0x22,after)||!field(r,node,0x24,first))break;
                row[2]=last;row[5]=before;row[6]=after;row[7]=first;t.spans[t.count++]=row;node=row[3];
            }
            t.complete=!node;Tracker after{};t.stable=tracker(r,s.inputs[roles[i]].backing,after)&&same(t,after);
        }
        if(s.context<0x710)return;renderer=s.context-0x710;
        readable=field(r,renderer,0x1f208,epoch)&&field(r,renderer,0x7af18,array)&&field(r,renderer,0x7af28,packets);
        if(!readable||!array||!packets||packets>packet_capacity||array>UINTPTR_MAX-16ULL*packets)return;
        std::array<std::array<std::uint64_t,2>,64> chunk{};
        for(unsigned begin=0;begin<packets;){const auto count=std::min(64U,packets-begin);
            if(!r.read||!r.read(r.user,array+16ULL*begin,std::as_writable_bytes(std::span(chunk).first(count))))break;
            for(unsigned i=0;i<count;++i)if(chunk[i][1]==packet){index=begin+i;++matches;}
            begin+=count;scanned=begin;
        }
        scan_complete=scanned==packets;
        if(scan_complete&&matches==1){const auto begin=index>earlier_capacity?index-earlier_capacity:0;
            for(unsigned i=begin;i<index;++i){Earlier v;v.index=i;unsigned opcode{};std::uintptr_t node{};
                if(!field(r,array,16*i+8,v.packet)||!field(r,v.packet,0,opcode)||!field(r,v.packet,8,node))break;
                v.opcode=opcode&31;
                while(node&&v.visited<128){std::uintptr_t next{},backing{},resource{},span{};std::uint16_t before{},after{};
                    if(!field(r,node,0,next)||!field(r,node,8,backing)||!field(r,backing,0x20,resource)||!field(r,node,0x10,span)||
                       !field(r,node,0x18,before)||!field(r,node,0x1a,after))break;
                    unsigned mask{};for(unsigned role=0;role<3;++role)if(resource==s.inputs[roles[role]].resource)mask|=1U<<role;
                    if(mask){++v.relevant;if(v.count<node_capacity)v.nodes[v.count++]={v.visited,mask,before,after,span};}
                    ++v.visited;node=next;
                }
                v.complete=!node;earlier[earlier_count++]=v;
            }
        }
        std::uintptr_t next_array{};unsigned next_count{};std::uint64_t next_epoch{};
        stable=field(r,renderer,0x1f208,next_epoch)&&field(r,renderer,0x7af18,next_array)&&field(r,renderer,0x7af28,next_count)&&
            epoch==next_epoch&&array==next_array&&packets==next_count;
    }
    void write(std::ostream &o)const {
        o<<"{\"schema\":1,\"diagnostic_only\":true,\"renderer\":"<<renderer<<",\"epoch\":"<<epoch<<",\"array\":"<<array<<",\"packets\":"<<packets
         <<",\"scanned\":"<<scanned<<",\"index\":"<<index<<",\"matches\":"<<matches<<",\"readable\":"<<readable<<",\"scan_complete\":"<<scan_complete<<",\"stable\":"<<stable<<",\"trackers\":[";
        for(unsigned i=0;i<3;++i){const auto &t=trackers[i];o<<(i?",":"")<<"{\"address\":"<<t.address<<",\"first\":"<<t.first<<",\"last\":"<<t.last
            <<",\"epoch\":"<<t.epoch<<",\"used_epoch\":"<<t.used_epoch<<",\"immediate_epoch\":"<<t.immediate_epoch<<",\"initial\":"<<t.initial
            <<",\"final\":"<<t.final<<",\"last_use\":"<<t.last_use<<",\"initialized\":"<<t.initialized<<",\"immediate\":"<<t.immediate
            <<",\"readable\":"<<t.readable<<",\"stable\":"<<t.stable<<",\"complete\":"<<t.complete<<",\"spans\":[";
            for(unsigned j=0;j<t.count;++j){o<<(j?",[":"[");for(unsigned k=0;k<8;++k)o<<(k?",":"")<<t.spans[j][k];o<<']';}o<<"]}";}
        o<<"],\"earlier\":[";
        for(unsigned i=0;i<earlier_count;++i){const auto &p=earlier[i];o<<(i?",":"")<<"{\"index\":"<<p.index<<",\"packet\":"<<p.packet<<",\"opcode\":"<<p.opcode
            <<",\"visited\":"<<p.visited<<",\"relevant\":"<<p.relevant<<",\"complete\":"<<p.complete<<",\"nodes\":[";
            for(unsigned j=0;j<p.count;++j){o<<(j?",[":"[");for(unsigned k=0;k<5;++k)o<<(k?",":"")<<p.nodes[j][k];o<<']';}o<<"]}";}
        o<<"]}";
    }
};
}
