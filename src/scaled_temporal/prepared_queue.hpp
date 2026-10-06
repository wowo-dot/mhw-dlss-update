#pragma once
#include "../temporal_bindings/inputs.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <ostream>

namespace mhw::scaled_temporal::prepared_queue {
// Read-only CPU preparation evidence. This is deliberately not a GPU resource
// admission proof: engine dimensions are not GetDesc, keys are not submission
// receipts, and a descriptor can describe work queued by another callback.
namespace tb=temporal_bindings;
inline constexpr unsigned capacity=16, sample_capacity=16, packet_limit=65535;
template<class T>bool field(const tb::Reader &r,std::uintptr_t p,unsigned offset,T &v){
    return p&&p<=UINTPTR_MAX-offset-sizeof(T)&&r.get(p+offset,v);
}
struct Texture {
    std::uintptr_t owner{},resource{};
    unsigned width{},height{},format{},gpu_format{},readable{};
    bool operator==(const Texture &)const=default;
};
inline Texture texture(const tb::Reader &r,std::uintptr_t object){
    Texture t;t.owner=object;tb::Owner o;
    if(object&&tb::read_owner(r,object,o))
        t={object,o.resource,o.width,o.height,o.format,o.gpu_format,1};
    return t;
}
struct Commands {
    std::array<unsigned,32> opcodes{};
    // First two prepared draws: packet, attachment, left, top, right, bottom.
    // A prefix only; opcodes accounts for every matching packet in the queue.
    std::array<std::array<std::uint64_t,6>,2> draws{};
    struct Detail {
        // Native packet bindings, vertex count/first, attachment target, and
        // pixel-shader descriptor. Metadata only; no shader bytes or COM.
        std::array<std::uintptr_t,4> bindings{};
        std::array<unsigned,2> arguments{};
        std::array<std::uint64_t,6> target{}; // flags, view, backend, resource, RTV, format
        std::array<std::uint64_t,7> pixel{}; // handle, set, entry, descriptor, data, bytes, readable
        bool readable{};
        bool operator==(const Detail &)const=default;
    };
    std::array<Detail,2> details{};
    unsigned draw_samples{};
    bool operator==(const Commands &)const=default;
};
struct Entry {
    std::uintptr_t descriptor{},unit{},type{};
    unsigned stage{},order{};
    std::array<unsigned char,3> flags{};
    Texture input,output;
    Commands commands;
    bool operator==(const Entry &)const=default;
};
struct Capture {
    std::uint64_t frame{},epoch{};
    std::uintptr_t context{},view{},manager{},taa{},array{};
    unsigned bank{},async{},count{},packets{};
    std::array<unsigned,4> rectangle{};
    std::array<unsigned,3> scale_bits{};
    std::array<Texture,2> scratch{};
    std::array<Entry,capacity> entries{};
    Commands final_commands;
    bool operator==(const Capture &)const=default;
};
enum class Result:unsigned { ok, identity, bounds, descriptor, packet, changed };
inline Result read(const tb::Reader &r,const tb::Config &cfg,std::uintptr_t manager,
                   std::uintptr_t context,std::uintptr_t view,std::uintptr_t taa,Capture &c){
    c={};c.manager=manager;c.context=context;c.view=view;c.taa=taa;
    std::uintptr_t check{};unsigned char collecting{},async{};
    if(!field(r,cfg.image,0x51cc238,check)||check!=manager||!manager||
       context!=cfg.renderer+0x710||!field(r,context,0x1d0,check)||check!=view||!view||
       !field(r,cfg.renderer,0x1f450,check)||check!=view||
       !field(r,cfg.renderer,0x1f210,c.frame)||!c.frame||!field(r,cfg.renderer,0x1f208,c.epoch)||
       !field(r,cfg.renderer,0x1f200,c.bank)||c.bank>1||!field(r,cfg.renderer,0x93,async)||async>1||
       !field(r,manager,0x39,collecting)||collecting||!taa)return Result::identity;
    c.async=async;
    if(!field(r,manager,0x3c,c.count)||!c.count||c.count>capacity||
       !field(r,context,0x5c,c.rectangle)||!field(r,view,0x6b0,c.scale_bits)||
       !field(r,context,0x2b80,c.packets)||c.packets>packet_limit||
       !field(r,context,0x2b78,c.array)||!c.array||c.array>UINTPTR_MAX-16ULL*c.packets)return Result::bounds;
    if(c.rectangle[0]>=c.rectangle[2]||c.rectangle[1]>=c.rectangle[3]||
       c.rectangle[2]>16384||c.rectangle[3]>16384)return Result::bounds;
    for(auto bits:c.scale_bits){const auto scale=std::bit_cast<float>(bits);
        if(!std::isfinite(scale)||scale<=0||scale>4)return Result::bounds;}
    for(unsigned i=0;i<2;++i){if(!field(r,manager,0xc0+8*i,check))return Result::descriptor;c.scratch[i]=texture(r,check);}
    unsigned temporal{};
    for(unsigned i=0;i<c.count;++i){auto &e=c.entries[i];std::uintptr_t input{},output{};
        if(!field(r,manager,0x40+8*i,e.descriptor)||!e.descriptor||
           !field(r,e.descriptor,0,e.stage)||e.stage>31||!field(r,e.descriptor,4,e.order)||e.order>0x7ffffff||
           !field(r,e.descriptor,8,e.unit)||!field(r,e.descriptor,0x10,input)||
           !field(r,e.descriptor,0x18,output)||!field(r,e.descriptor,0x20,e.flags)||
           (e.unit&&!field(r,e.unit,0,e.type)))return Result::descriptor;
        for(unsigned j=0;j<i;++j)if(e.descriptor==c.entries[j].descriptor)return Result::descriptor;
        // Types and flag combinations are evidence, never a filter whitelist.
        e.input=texture(r,input);e.output=texture(r,output);
        if(e.descriptor==taa+0x140){if(e.unit!=taa||e.stage!=15||e.order!=2)return Result::descriptor;++temporal;}
    }
    if(temporal!=1)return Result::descriptor;
    struct Packet {unsigned key,padding;std::uintptr_t object;};static_assert(sizeof(Packet)==16);
    std::array<Packet,256> block{};
    const auto observe=[&](Commands &commands,const Packet &p)->bool{
        unsigned raw{};if(!field(r,p.object,0,raw))return false;
        const auto opcode=raw&31;++commands.opcodes[opcode];
        if(opcode==0&&commands.draw_samples<commands.draws.size()){
            std::uintptr_t attachment{};std::array<unsigned,4> rect{};
            if(!field(r,p.object,0x10,attachment)||!field(r,attachment,0x4c,rect))return false;
            const auto index=commands.draw_samples++;
            commands.draws[index]={p.object,attachment,rect[0],rect[1],rect[2],rect[3]};
            auto &d=commands.details[index];unsigned flags{},format{},bytes{};
            d.readable=field(r,p.object,0x10,d.bindings)&&field(r,p.object,0x40,d.arguments)&&
                field(r,attachment,0x48,flags)&&field(r,attachment,0,d.target[1])&&
                field(r,d.target[1],0x60,d.target[2])&&field(r,d.target[2],0x18,d.target[3])&&
                field(r,d.target[2],0x10,d.target[4])&&field(r,d.target[2],0x20,format)&&d.target[3]&&d.target[4];
            d.target[0]=flags;d.target[5]=format;
            d.pixel[6]=field(r,d.bindings[2],8,d.pixel[0])&&field(r,d.pixel[0],0,d.pixel[1])&&
                field(r,d.pixel[1],0x58,d.pixel[2])&&field(r,d.pixel[2],0x10,d.pixel[3])&&
                field(r,d.pixel[3],8,d.pixel[4])&&d.pixel[4]&&field(r,d.pixel[3],0,bytes)&&bytes>=52&&bytes<=262144;
            d.pixel[5]=bytes;
        }return true;
    };
    for(unsigned base=0;base<c.packets;base+=256){const auto n=(std::min)(256U,c.packets-base);
        if(!r.read||!r.read(r.user,c.array+16ULL*base,std::as_writable_bytes(std::span(block).first(n))))return Result::packet;
        for(unsigned j=0;j<n;++j){const auto &p=block[j];
            for(unsigned i=0;i<c.count;++i){auto &e=c.entries[i];
                if(p.key==((e.stage<<27)|e.order)&&!observe(e.commands,p))return Result::packet;}
            if(p.key==0x80000000&&!observe(c.final_commands,p))return Result::packet;
        }
    }
    return Result::ok;
}
inline Result capture(const tb::Reader &r,const tb::Config &cfg,std::uintptr_t manager,
                      std::uintptr_t context,std::uintptr_t view,std::uintptr_t taa,Capture &out){
    Capture a,b;auto result=read(r,cfg,manager,context,view,taa,a);
    if(result!=Result::ok)return result;
    result=read(r,cfg,manager,context,view,taa,b);
    if(result!=Result::ok)return result;
    if(a!=b)return Result::changed;
    out=a;return Result::ok;
}
// Dataflow only, from the selected temporal descriptor to the queue tail.
// Scratch A -> B -> A is valid; pointer equality across time is not feedback.
// Endpoint/no-callback entries still describe a logical edge. No assumption
// that one descriptor means one draw or that callbacks run in GPU order.
inline unsigned chain_breaks(const Capture &c){
    bool started=false;std::uintptr_t current{};unsigned breaks{};
    for(unsigned i=0;i<c.count;++i){const auto &e=c.entries[i];
        if(e.descriptor==c.taa+0x140){started=true;current=e.output.owner;continue;}
        if(!started)continue;
        if(!current||e.input.owner!=current||!e.output.owner)++breaks;
        current=e.output.owner;
    }return breaks;
}
template<class T>void array(std::ostream &o,const T &a){o<<'[';bool comma=false;for(auto x:a){if(comma)o<<',';o<<+x;comma=true;}o<<']';}
inline void write_texture(std::ostream &o,const Texture &t){
    array(o,std::array<std::uint64_t,7>{t.owner,t.resource,t.width,t.height,t.format,t.gpu_format,t.readable});
}
inline void write_commands(std::ostream &o,const Commands &c){
    o<<"{\"opcodes\":";array(o,c.opcodes);o<<",\"draws\":[";
    for(unsigned i=0;i<c.draw_samples;++i){if(i)o<<',';array(o,c.draws[i]);}o<<"],\"details\":[";
    for(unsigned i=0;i<c.draw_samples;++i){if(i)o<<',';const auto &d=c.details[i];
        o<<"{\"readable\":"<<(d.readable?"true":"false")<<",\"bindings\":";array(o,d.bindings);o<<",\"arguments\":";array(o,d.arguments);
        o<<",\"target\":";array(o,d.target);o<<",\"pixel\":";array(o,d.pixel);o<<'}';}o<<"]}";
}
inline void write(std::ostream &o,const Capture &c){
    o<<"{\"stamp\":";array(o,std::array<std::uint64_t,9>{c.frame,c.epoch,c.context,c.view,c.manager,c.taa,c.bank,c.async,c.array});
    o<<",\"packets\":"<<c.packets<<",\"rectangle\":";array(o,c.rectangle);o<<",\"scale_bits\":";array(o,c.scale_bits);
    o<<",\"scratch\":[";write_texture(o,c.scratch[0]);o<<',';write_texture(o,c.scratch[1]);
    o<<"],\"chain_breaks\":"<<chain_breaks(c)<<",\"entries\":[";
    for(unsigned i=0;i<c.count;++i){if(i)o<<',';const auto &e=c.entries[i];
        o<<"{\"identity\":";array(o,std::array<std::uint64_t,5>{e.descriptor,e.stage,e.order,e.unit,e.type});
        o<<",\"flags\":";array(o,e.flags);o<<",\"input\":";write_texture(o,e.input);o<<",\"output\":";write_texture(o,e.output);
        o<<",\"commands\":";write_commands(o,e.commands);o<<'}';
    }o<<"],\"final_commands\":";write_commands(o,c.final_commands);o<<'}';
}
class Recorder {
    std::uint64_t visits_{},attempts_{},accepted_{};
    std::array<std::uint64_t,6> results_{};
    struct Sample {std::uint64_t visit{},attempt{};Capture value;};
    std::array<Sample,sample_capacity> first_{};Sample latest_{};
public:
    // One bounded sample per 128 admitted queue completions, first included.
    // Keep first sixteen plus latest. No allocations, COM, extra hooks or GPU
    // work. Capture failures do not change renderer/feature admission.
    void observe(const tb::Reader &r,const tb::Config &cfg,std::uintptr_t manager,
                 std::uintptr_t context,std::uintptr_t view,std::uintptr_t taa){
        if((visits_++%128)!=0)return;
        ++attempts_;Capture value;
        const auto result=capture(r,cfg,manager,context,view,taa,value);++results_[static_cast<unsigned>(result)];
        if(result!=Result::ok)return;
        latest_={visits_,attempts_,value};
        if(accepted_<sample_capacity)first_[static_cast<unsigned>(accepted_)]=latest_;
        ++accepted_;
    }
    void write(std::ostream &o)const{
        o<<"{\"schema\":1,\"policy\":\"native-prepared-color-chain-v1\",\"diagnostic_only\":true,\"interval\":128,\"capacity\":16"
         <<",\"visits\":"<<visits_<<",\"attempts\":"<<attempts_<<",\"accepted\":"<<accepted_<<",\"results\":";array(o,results_);
        o<<",\"samples\":[";const auto sample=[&](const Sample &s){o<<"{\"visit\":"<<s.visit<<",\"attempt\":"<<s.attempt<<",\"capture\":";prepared_queue::write(o,s.value);o<<'}';};
        for(unsigned i=0;i<sample_capacity&&i<accepted_;++i){if(i)o<<',';sample(first_[i]);}
        if(accepted_>sample_capacity){o<<',';sample(latest_);}o<<"]}";
    }
};
}
