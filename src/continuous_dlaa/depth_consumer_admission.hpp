#pragma once
#include "native_state_trace.hpp"
#include "depth_read_state.hpp"
#include "../temporal_bindings/depth_routes.hpp"
#include "../temporal_bindings/copy_execution.hpp"
#include <d3d12.h>
#include <memory>
namespace mhw::continuous_dlaa {
// Current-callback proof against the pinned native planner/converter contract.
// No cached pointer, final tracker state, or CPU writer callback grants admission.
//
// Two writer shapes. Scaled (render < texture): the kind-8 copy runs its nested
// draw, whose span transitions 12 -> 0x100 (RENDER_TARGET) at the writer.
// Whole copy (render scale 1.0, v0.19.3): the opcode-10 executor takes the
// whole-copy path (a native CopyResource, no child draw). Its span is the outer
// writer's own; it ends in COPY_DEST (native 2048) and its transition may be
// hoisted before the writer. At the consumer the depth is in COPY_DEST, or in
// the next span's read state when that transition was hoisted between writer
// and consumer (game-75: hoisted 2048 -> 8 before the TAA; game-76: none, the
// span starts after the TAA). As in the scaled mode, a span that starts at or
// before the consumer without its transition rejects.
class DepthConsumerAdmission {
    static constexpr unsigned native_copy_destination=2048;  // converter 0x259ba50: COPY_DEST
    using Tracker=NativeStateTrace::Tracker;
    using Reader=temporal_bindings::Reader;
    using Owner=temporal_bindings::Owner;
    using CopyPacket=temporal_bindings::depth_routes::CopyPacket;
    template<class T>static bool field(const Reader &r,std::uintptr_t p,unsigned o,T &v) noexcept {return NativeStateTrace::field(r,p,o,v);}
    struct Node {
        std::uintptr_t packet{},address{},next{},backing{},resource{},span{};
        unsigned index{},before{},after{};
        bool operator==(const Node &)const=default;
    };
    struct Packet {
        std::uintptr_t object{},head{},nested{},nested_head{};unsigned opcode{},visited{},nested_visited{};
        temporal_bindings::CopyExecution copy_execution;
        bool operator==(const Packet &)const=default;
    };
    struct Transition {
        unsigned first{},last{},node{},before{},after{};
        bool operator==(const Transition &)const=default;
    };
    struct Snapshot {
        Owner owner;CopyPacket writer;Tracker tracker;
        std::uintptr_t viewport{};std::array<unsigned,4> rectangle{};unsigned backing_width{},backing_height{},nested_opcode{};
        std::uintptr_t renderer{},array{};std::uint64_t epoch{};
        unsigned count{},writer_index{UINT_MAX},consumer_index{UINT_MAX},next_index{UINT_MAX},span_index{UINT_MAX};
        unsigned writer_matches{},consumer_matches{},next_matches{},nodes{},packets{},state{},reason{};
        std::array<Packet,257> interval{};std::array<Node,16> relevant{};
        std::array<unsigned,16> starts{},matches{};
        std::array<Transition,16> transitions{};unsigned applied{};
        std::array<unsigned,5> copy_paths{};
        // Bounded failure site only; no extra process reads or changed admission.
        std::array<std::uint64_t,14> detail{};
        bool operator==(const Snapshot &)const=default;
    } first_,second_;
    struct Receipt {std::uint64_t ordinal{},frame{},epoch{},sequence{},tick{};std::uintptr_t list{};
        std::uint64_t generation{};unsigned reason{},writer{UINT_MAX},consumer{UINT_MAX},next{UINT_MAX},state{},nodes{};bool stable{};
        std::array<Transition,16> transitions{};unsigned applied{};std::array<std::uint64_t,14> detail{};std::array<unsigned,5> copy_paths{};};
    std::array<Receipt,8> initial_{};Receipt latest_;
    std::array<Receipt,8> failures_{};Receipt last_failure_;std::uint64_t failure_count_{};
    std::uint64_t attempts_{},accepted_{},copies_{},read_states_{},whole_attempts_{},whole_accepted_{};std::array<std::uint64_t,12> reasons_{};
    static bool chain(const Reader &r,const Owner &owner,Tracker &t) noexcept {
        if(!NativeStateTrace::tracker(r,owner.backing,t))return false;t.readable=true;
        auto n=t.first;
        while(n&&t.count<t.spans.size()){
            auto &a=t.spans[t.count];a[0]=n;unsigned last{};std::uint16_t before{},after{};unsigned char first{};
            if(!field(r,n,0,a[1])||!field(r,n,8,last)||!field(r,n,0x10,a[3])||!field(r,n,0x18,a[4])||
               !field(r,n,0x20,before)||!field(r,n,0x22,after)||!field(r,n,0x24,first))return false;
            a[2]=last;a[5]=before;a[6]=after;a[7]=first;++t.count;n=a[3];
        }
        t.complete=!n;Tracker again;t.stable=NativeStateTrace::tracker(r,owner.backing,again)&&NativeStateTrace::same(t,again);
        return t.complete&&t.stable;
    }
    static bool nodes(const Reader &r,Snapshot &s,std::uintptr_t packet,unsigned index,std::uintptr_t &head,unsigned &visited) noexcept {
        const auto note=[&](unsigned code,const Node &a){s.detail={code,index,s.detail[2],packet,head,a.address,a.next,a.backing,a.resource,a.span,a.before,a.after,visited,s.nodes};return false;};
        if(!field(r,packet,8,head))return note(20,{});auto n=head;
        while(n&&visited<128){Node a;a.packet=packet;a.address=n;a.index=index;std::uint16_t before{},after{};
            if(!field(r,n,0,a.next))return note(21,a);
            if(!field(r,n,8,a.backing))return note(22,a);
            if(!field(r,a.backing,0x20,a.resource))return note(23,a);
            if(!field(r,n,0x10,a.span))return note(24,a);
            if(!field(r,n,0x18,before))return note(25,a);a.before=before;
            if(!field(r,n,0x1a,after))return note(26,a);a.after=after;
            if(a.resource==s.owner.resource||a.backing==s.owner.backing){
                if(a.resource!=s.owner.resource||a.backing!=s.owner.backing||s.nodes==s.relevant.size())return note(27,a);
                s.relevant[s.nodes++]=a;
            }
            ++visited;n=a.next;
        }if(n){Node a;a.address=n;return note(28,a);}return true;
    }
    static unsigned capture(const Reader &r,const temporal_bindings::Snapshot &source,const Owner &owner,std::uintptr_t writer,
        const CopyPacket &expected,std::uintptr_t consumer,Snapshot &s,bool whole) noexcept {
        if(!writer||!consumer||writer==consumer||source.context<0x710||source.epoch==UINT64_MAX||
           !owner.object||!owner.backing||!owner.resource||expected.destination!=owner.object||!expected.nested||
           expected.rectangle!=std::array<unsigned,6>{0,0,source.rectangle[2],source.rectangle[3],0,0}||
           !source.rectangle[2]||!source.rectangle[3]||
           (whole?source.rectangle[2]!=owner.width||source.rectangle[3]!=owner.height:
                  source.rectangle[2]>=owner.width||source.rectangle[3]>=owner.height))return 1;
        if(!temporal_bindings::read_owner(r,owner.object,s.owner)||s.owner!=owner||
           !temporal_bindings::depth_routes::packet(r,writer,s.writer)||s.writer!=expected)return 2;
        if(!field(r,owner.backing,0xb0,s.backing_width)||s.backing_width!=owner.width||
           !field(r,owner.backing,0xb4,s.backing_height)||s.backing_height!=owner.height)return 2;
        if(whole){
            // The pinned executor's own branch: same-size kind-8 copy of the full rectangle.
            temporal_bindings::CopyExecution copy;
            if(!temporal_bindings::read_copy_execution(r,writer,copy)||copy.kind!=8||copy.path!=temporal_bindings::CopyExecutionPath::whole_copy||
               copy.destination!=owner.object||copy.child!=expected.nested)return 2;
            s.rectangle=copy.rectangle;s.nested_opcode=UINT_MAX;
        }else if(!field(r,expected.nested,0,s.nested_opcode)||(s.nested_opcode&31)!=0||
           !field(r,expected.nested,0x10,s.viewport)||!field(r,s.viewport,0x4c,s.rectangle))return 2;
        if(s.rectangle!=source.rectangle)return 2;
        s.renderer=source.context-0x710;
        if(!field(r,s.renderer,0x1f208,s.epoch)||s.epoch!=source.epoch+1||!field(r,s.renderer,0x7af18,s.array)||
           !field(r,s.renderer,0x7af28,s.count)||!s.array||!s.count||s.count>32768||s.array>UINTPTR_MAX-16ULL*s.count)return 3;
        auto &t=s.tracker;
        if(!chain(r,owner,t)||!t.count||t.epoch!=s.epoch||t.used_epoch!=s.epoch||t.initialized!=1||t.immediate_epoch||
           t.first!=t.spans[0][0]||t.last!=t.spans[t.count-1][0])return 4;
        for(unsigned i=0;i<t.count;++i){const auto &a=t.spans[i];
            if(!a[0]||!a[1]||a[2]>=s.count||a[3]!=(i+1<t.count?t.spans[i+1][0]:0)||a[4]!=(i?t.spans[i-1][0]:0)||
               a[7]||a[5]!=(i?t.spans[i-1][6]:t.initial)||(i&&a[2]<=t.spans[i-1][2]))return 4;
            for(unsigned j=0;j<i;++j)if(a[0]==t.spans[j][0]||a[1]==t.spans[j][1])return 4;
            if(a[1]==(whole?writer:expected.nested)){if(s.span_index!=UINT_MAX)return 4;s.span_index=i;}
        }
        if(s.span_index==UINT_MAX||s.span_index+1>=t.count)return 5;
        const auto &use=t.spans[s.span_index];const auto &next=t.spans[s.span_index+1];
        if(whole?use[6]!=native_copy_destination||next[5]!=native_copy_destination:use[5]!=12||use[6]!=0x100||next[5]!=0x100)return 5;
        if(!depth_shader_read_state(static_cast<unsigned>(next[6])))return 5;
        std::array<std::array<std::uint64_t,2>,64> block{};
        for(unsigned begin=0;begin<s.count;){const auto n=std::min(64U,s.count-begin);
            if(!r.read||!r.read(r.user,s.array+16ULL*begin,std::as_writable_bytes(std::span(block).first(n))))return 3;
            for(unsigned i=0;i<n;++i){auto p=block[i][1];
                if(p==writer){++s.writer_matches;s.writer_index=begin+i;}
                if(p==consumer){++s.consumer_matches;s.consumer_index=begin+i;}
                if(p==next[1]){++s.next_matches;s.next_index=begin+i;}
                for(unsigned j=s.span_index+1;j<t.count;++j)if(p==t.spans[j][1]){++s.matches[j];s.starts[j]=begin+i;}
                if(p==expected.nested)return 5; // Only the qualified parent/nested representation.
            }begin+=n;
        }
        if(s.writer_matches!=1||s.consumer_matches!=1||s.next_matches!=1||use[2]!=s.writer_index||
           s.writer_index>=s.consumer_index||s.consumer_index-s.writer_index>256)return 5;
        // A shader-read span may already have begun before reconstruction.
        // Validate every following span's schedule identity, without using its
        // planned final state as the current D3D state.
        for(unsigned j=s.span_index+1;j<t.count;++j){const auto &a=t.spans[j];
            if(s.matches[j]!=1||s.starts[j]<=t.spans[j-1][2]||s.starts[j]>a[2])return 5;
            if(s.starts[j]<=s.consumer_index&&!depth_shader_read_state(static_cast<unsigned>(a[6])))return 5;
        }
        // Read every packet between the writer and reconstruction, including
        // their state-node lists. Hoisted future transitions can be anywhere in
        // this gap; the diagnostic's preceding-32 window is not sufficient.
        for(unsigned index=s.writer_index;index<=s.consumer_index;++index){auto &p=s.interval[s.packets++];
            s.detail={10,index};
            unsigned raw{};
            if(!field(r,s.array,index*16+8,p.object)||!p.object||!field(r,p.object,0,raw)){s.detail[3]=p.object;return 6;}p.opcode=raw&31;
            s.detail={11,index,p.opcode,p.object};
            const auto before=s.nodes;
            if(!nodes(r,s,p.object,index,p.head,p.visited))return 6;
            if(p.opcode==10){
                s.detail[0]=33;
                if(!temporal_bindings::read_copy_execution(r,p.object,p.copy_execution))return 6;
                ++s.copy_paths[static_cast<unsigned>(p.copy_execution.path)];
                p.nested=p.copy_execution.child;
                if(p.copy_execution.path==temporal_bindings::CopyExecutionPath::child_draw){
                    s.detail[0]=30;
                    if(!nodes(r,s,p.nested,index,p.nested_head,p.nested_visited))return 6;
                }
            }
            // Other opcode-specific nested execution has not been qualified.
            if(s.nodes!=before&&p.opcode!=0&&p.opcode!=6&&p.opcode!=10){s.detail[0]=31;return 6;}
        }
        s.detail[0]=32;
        if(s.interval[0].object!=writer||s.interval[0].nested!=expected.nested||
           s.interval[0].copy_execution.path!=(whole?temporal_bindings::CopyExecutionPath::whole_copy:temporal_bindings::CopyExecutionPath::child_draw)||
           s.interval[s.packets-1].object!=consumer)return 6;
        // Scaled: exactly one writer transition, on the nested draw. Whole copy:
        // the writer's transition is usually hoisted before it; if it is on the
        // writer itself it must be the writer span's own, at most once.
        unsigned writer_nodes{},current_span=s.span_index;
        s.state=whole?D3D12_RESOURCE_STATE_COPY_DEST:D3D12_RESOURCE_STATE_RENDER_TARGET;
        for(unsigned i=0;i<s.nodes;++i){const auto &n=s.relevant[i];
            s.detail={40,n.index,0,n.packet,0,n.address,n.next,n.backing,n.resource,n.span,n.before,n.after,i,s.nodes};
            if(n.index==s.writer_index&&(!whole||(n.packet==writer&&n.span==use[0]))){
                if(n.packet!=(whole?writer:expected.nested)||n.span!=use[0]||n.before!=use[5]||n.after!=use[6]||
                   (!whole&&(n.before!=12||n.after!=0x100))||++writer_nodes!=1)return 6;
                s.transitions[s.applied++]={s.writer_index,s.writer_index,n.index,n.before,n.after};
            }else{
                s.detail[0]=41;
                if((!whole&&writer_nodes!=1)||current_span+1>=t.count)return 6;
                const auto &a=t.spans[current_span+1];const auto state=depth_shader_read_state(n.after);
                s.detail[0]=42;
                if(!state||n.span!=a[0]||n.before!=a[5]||n.after!=a[6]||n.before!=t.spans[current_span][6]||
                   n.index<=t.spans[current_span][2]||n.index>s.starts[current_span+1])return 6;
                ++current_span;s.state=state;
                s.transitions[s.applied++]={s.starts[current_span],static_cast<unsigned>(a[2]),n.index,n.before,n.after};
            }
        }
        // Every use already reached in the schedule must have its transition
        // in the complete interval. Future transitions may be hoisted here,
        // but only after the preceding span's last use and only into reads.
        s.detail[0]=43;
        if((!whole&&writer_nodes!=1)||(current_span+1<t.count&&s.starts[current_span+1]<=s.consumer_index))return 6;
        s.detail[0]=50;
        std::uintptr_t array{};unsigned count{};std::uint64_t epoch{};Owner again;CopyPacket packet;
        if(!field(r,s.renderer,0x7af18,array)||array!=s.array||!field(r,s.renderer,0x7af28,count)||count!=s.count||
           !field(r,s.renderer,0x1f208,epoch)||epoch!=s.epoch||!temporal_bindings::read_owner(r,owner.object,again)||again!=owner||
           !temporal_bindings::depth_routes::packet(r,writer,packet)||packet!=expected)return 7;
        s.detail={};return 0;
    }
public:
    unsigned attempt(const Reader &r,const temporal_bindings::Snapshot &source,const Owner &owner,std::uintptr_t writer,
        const CopyPacket &expected,std::uintptr_t consumer,std::span<const D3D12_RESOURCE_BARRIER> barriers,
        std::uint64_t sequence,std::uintptr_t list,std::uint64_t generation,std::uint64_t tick,bool available,bool description,bool whole=false) noexcept {
        std::destroy_at(&first_);std::construct_at(&first_);std::destroy_at(&second_);std::construct_at(&second_);
        if(whole)++whole_attempts_;
        unsigned reason=!available?9U:!description?10U:capture(r,source,owner,writer,expected,consumer,first_,whole);bool stable=false;
        if(!reason){reason=capture(r,source,owner,writer,expected,consumer,second_,whole);stable=!reason&&first_==second_;if(!stable)reason=7;}
        if(!reason){bool known=false;unsigned state{};
            for(const auto &b:barriers){
                if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING&&(!b.Aliasing.pResourceBefore||!b.Aliasing.pResourceAfter||
                   reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceBefore)==owner.resource||reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceAfter)==owner.resource)){reason=8;break;}
                if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_UAV&&(!b.UAV.pResource||reinterpret_cast<std::uintptr_t>(b.UAV.pResource)==owner.resource)){reason=8;break;}
                if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION&&reinterpret_cast<std::uintptr_t>(b.Transition.pResource)==owner.resource){
                    if(b.Flags||b.Transition.Subresource!=UINT_MAX||(known&&state!=unsigned(b.Transition.StateBefore))){reason=8;break;}
                    known=true;state=b.Transition.StateAfter;
                }
            }if(known&&state!=first_.state)reason=8;
        }
        Receipt receipt{++attempts_,source.frame,source.epoch,sequence,tick,list,generation,reason,
            first_.writer_index,first_.consumer_index,first_.next_index,first_.state,first_.nodes,stable,first_.transitions,first_.applied,
            second_.detail[0]?second_.detail:first_.detail,first_.copy_paths};
        ++reasons_[reason];if(!reason){++accepted_;if(first_.applied>1)++read_states_;if(whole)++whole_accepted_;}
        if(reason){++failure_count_;if(failure_count_<=failures_.size())failures_[static_cast<unsigned>(failure_count_-1)]=receipt;last_failure_=receipt;}
        if(attempts_<=initial_.size())initial_[static_cast<unsigned>(attempts_-1)]=receipt;latest_=receipt;
        return reason?0:first_.state;
    }
    void copied() noexcept {++copies_;}
    void write(std::ostream &o)const {
        o<<"{\"schema\":2,\"policy\":\"scheduled-read-interval-depth-copy-v2\",\"copy_execution_policy\":\"native-opcode10-branches-v1\",\"writer_copy_disabled\":true,\"attempts\":"<<attempts_
         <<",\"accepted\":"<<accepted_<<",\"rejected\":"<<attempts_-accepted_<<",\"copies\":"<<copies_<<",\"read_state_copies\":"<<read_states_
         <<",\"whole_copy\":["<<whole_attempts_<<','<<whole_accepted_<<"],\"reasons\":[";
        for(unsigned i=0;i<reasons_.size();++i)o<<(i?",":"")<<reasons_[i];o<<"],\"samples\":[";
        const auto emit=[&](const Receipt &s){o<<"{\"ordinal\":"<<s.ordinal<<",\"at\":["<<s.tick<<','<<s.frame<<','<<s.epoch<<','<<s.sequence<<','<<s.list<<','<<s.generation
          <<"],\"reason\":"<<s.reason<<",\"indices\":["<<s.writer<<','<<s.consumer<<','<<s.next<<"],\"state\":"<<s.state<<",\"nodes\":"<<s.nodes<<",\"stable\":"<<s.stable<<",\"transitions\":[";
          for(unsigned i=0;i<s.applied;++i){const auto &a=s.transitions[i];o<<(i?",":"")<<'['<<a.first<<','<<a.last<<','<<a.node<<','<<a.before<<','<<a.after<<']';}
          o<<"],\"detail\":[";for(unsigned i=0;i<s.detail.size();++i)o<<(i?",":"")<<s.detail[i];
          o<<"],\"copy_paths\":[";for(unsigned i=0;i<s.copy_paths.size();++i)o<<(i?",":"")<<s.copy_paths[i];o<<"]}";};
        for(unsigned i=0;i<std::min<std::uint64_t>(attempts_,initial_.size());++i){if(i)o<<',';emit(initial_[i]);}
        if(attempts_>initial_.size()){o<<',';emit(latest_);}o<<"],\"failure_diagnostics\":{\"schema\":1,\"count\":"<<failure_count_<<",\"capacity\":8,\"samples\":[";
        for(unsigned i=0;i<std::min<std::uint64_t>(failure_count_,failures_.size());++i){if(i)o<<',';emit(failures_[i]);}
        if(failure_count_>failures_.size()){o<<',';emit(last_failure_);}o<<"]}}";
    }
};
}
