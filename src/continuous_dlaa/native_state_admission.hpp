#pragma once
#include "state_trace.hpp"
namespace mhw::continuous_dlaa {
// Supplement missing API transitions only through the pinned native planner's
// current-use contract. This is not an independent GPU-state query. Metadata
// is copied twice inside the owning consumer callback and never cached as a
// state authority across callbacks, recordings, frames or resource lifetimes.
inline unsigned native_use_decision(const StateTrace::Sample &s) noexcept {
    const auto &p=s.provenance;
    if(s.temporal||s.resources[0].proof.result>1||s.resources[1].proof.result!=1)return 1;
    if(!s.packet||!s.generation||!s.list||!s.native_complete||s.native_saved!=s.native_relevant||s.saved!=s.relevant)return 2;
    if(!s.resources[0].resource||!s.resources[1].resource||s.resources[0].resource==s.resources[1].resource||
       s.resources[0].backing==s.resources[1].backing||s.resources[0].object==s.resources[1].object)return 2;
    if(!p.readable||!p.scan_complete||!p.stable||p.matches!=1||p.index>=p.packets||s.epoch==UINT64_MAX||p.epoch!=s.epoch+1)return 3;
    const auto begin=p.index>p.earlier_capacity?p.index-p.earlier_capacity:0;
    if(p.earlier_count!=p.index-begin)return 3;
    for(unsigned i=0;i<p.earlier_count;++i){const auto &e=p.earlier[i];
        if(e.index!=begin+i||!e.packet||!e.complete||e.count!=e.relevant)return 3;
        for(unsigned j=0;j<i;++j)if(e.packet==p.earlier[j].packet)return 3;
    }
    const auto earlier=[&](std::uintptr_t packet)->const NativeStateTrace::Earlier *{
        for(unsigned i=0;i<p.earlier_count;++i)if(p.earlier[i].packet==packet)return &p.earlier[i];return nullptr;};
    const auto node=[&](const NativeStateTrace::Earlier &e,unsigned role,std::uint64_t span,unsigned before,unsigned after){
        unsigned matches{};for(unsigned i=0;i<e.count;++i){const auto &n=e.nodes[i];if(!(n[1]&role))continue;
            if(n[1]!=role||n[2]!=before||n[3]!=after||n[4]!=span)return false;++matches;}return matches==1;};
    // Validate the full retained chain, including later uses, but select the
    // current span by packet/ordinal. The final planned state is never used.
    for(unsigned role=0;role<2;++role){const auto &t=p.trackers[role];
        if(!t.readable||!t.stable||!t.complete||!t.count||t.count>p.span_capacity||t.epoch!=p.epoch||t.used_epoch!=p.epoch||t.initialized!=1||t.immediate_epoch)return 4;
        if(t.first!=t.spans[0][0]||t.last!=t.spans[t.count-1][0])return 4;
        for(unsigned i=0;i<t.count;++i){const auto &a=t.spans[i];
            if(!a[0]||!a[1]||a[2]>=p.packets||a[3]!=(i+1<t.count?t.spans[i+1][0]:0)||a[4]!=(i?t.spans[i-1][0]:0)||
               a[5]>0xffff||a[6]>0xffff||a[7]||a[5]!=(i?t.spans[i-1][6]:t.initial)||(i&&a[2]<=t.spans[i-1][2]))return 4;
            for(unsigned j=0;j<i;++j)if(a[0]==t.spans[j][0]||a[1]==t.spans[j][1])return 4;
        }
    }
    const auto &out=p.trackers[1];const auto &color=p.trackers[0];
    const std::array<std::uint64_t,8> *use{};unsigned use_index{};
    for(unsigned i=0;i<out.count;++i)if(out.spans[i][1]==s.packet){if(use)return 5;use=&out.spans[i];use_index=i;}
    if(!use||(*use)[2]!=p.index||(*use)[5]!=0x400||(*use)[6]!=0x400||use_index>1)return 5;
    unsigned native_out{},api_out{};
    for(unsigned i=0;i<s.native_saved;++i){const auto &n=s.native[i];if(!(n[1]&2))continue;
        if(n[1]!=2||n[2]!=0x400||n[3]!=0x400)return 5;++native_out;}
    for(unsigned i=0;i<s.saved;++i){const auto &b=s.barriers[i];if(!(b[6]&2))continue;
        if(b[1]!=D3D12_RESOURCE_BARRIER_TYPE_UAV||b[2]||b[6]!=2)return 5;++api_out;}
    if(native_out!=1||api_out!=1)return 5;
    if(use_index==0){if(out.initial!=0x400)return 5;
        for(unsigned i=0;i<p.earlier_count;++i)for(unsigned j=0;j<p.earlier[i].count;++j)if(p.earlier[i].nodes[j][1]&2)return 5;}
    else {const auto &first=out.spans[0];const auto *e=earlier(first[1]);
        if(out.initial||first[5]||first[6]!=0x400||!e||e->opcode!=6||e->index+2!=p.index||first[2]!=e->index||!node(*e,2,first[0],0,0x400))return 5;
        for(unsigned i=e->index+1-begin;i<p.earlier_count;++i)for(unsigned j=0;j<p.earlier[i].count;++j)if(p.earlier[i].nodes[j][1]&2)return 5;
    }
    if(s.resources[0].proof.result==1){
        const std::array<std::uint64_t,8> *read{};
        for(unsigned i=0;i<color.count;++i)if(color.spans[i][2]==p.index){if(read)return 6;read=&color.spans[i];}
        const auto *e=read?earlier((*read)[1]):nullptr;
        if(!read||!e||e->opcode!=6||e->index+2!=p.index||(*read)[5]!=0x100||(*read)[6]!=4||!node(*e,1,(*read)[0],0x100,4))return 6;
        for(unsigned i=0;i<s.native_saved;++i)if(s.native[i][1]&1)return 6;
        for(unsigned i=e->index+1-begin;i<p.earlier_count;++i)for(unsigned j=0;j<p.earlier[i].count;++j)if(p.earlier[i].nodes[j][1]&1)return 6;
    }
    return 0;
}

class NativeStateAdmission {
    StateTrace evidence_;
    NativeStateTrace reread_;
    struct Result {std::uint64_t ordinal{};unsigned reason{};bool stable{};};
    std::array<Result,8> first_{};Result last_{};
    std::uint64_t attempts_{},accepted_{};
    // Re-read the native nodes that the current converter consumed, requiring
    // exact backing identity as well as COM-resource identity. No pointer from
    // this check outlives the enclosing native consumer.
    static bool current_nodes(const temporal_bindings::Reader &r,const StateTrace::Sample &s) noexcept {
        std::uintptr_t n{};if(!NativeStateTrace::field(r,s.packet,8,n))return false;
        unsigned visited{},saved{};
        while(n&&visited<128){std::uintptr_t next{},backing{},resource{},span{};std::uint16_t before{},after{};
            if(!NativeStateTrace::field(r,n,0,next)||!NativeStateTrace::field(r,n,8,backing)||!NativeStateTrace::field(r,backing,0x20,resource)||
               !NativeStateTrace::field(r,n,0x10,span)||!NativeStateTrace::field(r,n,0x18,before)||!NativeStateTrace::field(r,n,0x1a,after))return false;
            unsigned mask{};for(unsigned i=0;i<3;++i)if(resource==s.resources[i].resource){if(backing!=s.resources[i].backing)return false;mask|=1U<<i;}
            if(mask&2){unsigned matched{};const auto &t=s.provenance.trackers[1];for(unsigned i=0;i<t.count;++i)
                if(t.spans[i][0]==span&&t.spans[i][1]==s.packet)++matched;if(matched!=1)return false;}
            if(mask){if(saved==s.native_saved||s.native[saved]!=std::array<unsigned,4>{visited,mask,before,after})return false;++saved;}
            ++visited;n=next;
        }
        return !n&&visited==s.native_nodes&&saved==s.native_saved;
    }
    static bool earlier_nodes(const temporal_bindings::Reader &r,const StateTrace::Sample &s) noexcept {
        for(unsigned i=0;i<s.provenance.earlier_count;++i){const auto &e=s.provenance.earlier[i];std::uintptr_t n{};
            if(!NativeStateTrace::field(r,e.packet,8,n))return false;unsigned visited{},saved{};
            while(n&&visited<128){std::uintptr_t next{},backing{},resource{},span{};std::uint16_t before{},after{};
                if(!NativeStateTrace::field(r,n,0,next)||!NativeStateTrace::field(r,n,8,backing)||!NativeStateTrace::field(r,backing,0x20,resource)||
                   !NativeStateTrace::field(r,n,0x10,span)||!NativeStateTrace::field(r,n,0x18,before)||!NativeStateTrace::field(r,n,0x1a,after))return false;
                unsigned mask{};for(unsigned role=0;role<3;++role)if(resource==s.resources[role].resource){if(backing!=s.resources[role].backing)return false;mask|=1U<<role;}
                if(mask){if(saved==e.count||e.nodes[saved]!=std::array<std::uint64_t,5>{visited,mask,before,after,span})return false;++saved;}
                ++visited;n=next;
            }
            if(n||visited!=e.visited||saved!=e.count)return false;
        }
        return true;
    }
public:
    bool attempt(const temporal_bindings::Reader &r,const temporal_bindings::Snapshot &source,std::uint64_t sequence,
                 ID3D12GraphicsCommandList *list,std::uint64_t generation,std::uintptr_t packet,std::uint64_t tick,
                 std::span<const D3D12_RESOURCE_BARRIER> barriers) noexcept {
        const auto &s=evidence_.capture(r,source,sequence,list,generation,packet,tick,false,barriers);
        auto reason=native_use_decision(s);bool stable=false;
        if(!reason){
            std::destroy_at(&reread_);std::construct_at(&reread_);reread_.capture(r,source,packet);
            stable=reread_==s.provenance&&current_nodes(r,s)&&earlier_nodes(r,s);
            for(unsigned role:{0U,4U}){temporal_bindings::Owner actual;
                if(!temporal_bindings::read_owner(r,source.inputs[role].object,actual)){stable=false;continue;}
                actual.id=source.inputs[role].id;if(actual!=source.inputs[role])stable=false;}
            if(!stable)reason=7;
        }
        Result result{++attempts_,reason,stable};if(attempts_<=first_.size())first_[static_cast<unsigned>(attempts_-1)]=result;else last_=result;
        if(!reason)++accepted_;return !reason;
    }
    void write(std::ostream &o)const {
        o<<"{\"schema\":1,\"policy\":\"native-current-use-v1\",\"attempts\":"<<attempts_<<",\"accepted\":"<<accepted_
         <<",\"rejected\":"<<attempts_-accepted_<<",\"results\":[";
        const auto emit=[&](const Result &r){o<<"{\"ordinal\":"<<r.ordinal<<",\"reason\":"<<r.reason<<",\"stable\":"<<r.stable<<'}';};
        for(unsigned i=0;i<std::min<std::uint64_t>(attempts_,first_.size());++i){if(i)o<<',';emit(first_[i]);}if(attempts_>first_.size()){o<<',';emit(last_);}
        o<<"],\"evidence\":";evidence_.write(o);o<<'}';
    }
};
}
