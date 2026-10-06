#include "owned_pass.hpp"
#include <algorithm>
namespace mhw::aa_control {
namespace {
constexpr std::array<unsigned,5> orders={0,100,1024,134217713,20000};
constexpr std::array<unsigned,5> vtables={0x3538400,0x3537a70,0x3536c98,0,0x35372b0};
constexpr std::array<std::array<unsigned char,3>,5> flags={{{0,0,0},{0,0,0},{0,1,0},{1,0,0},{0,0,0}}};
const PostNode *node(const PostQueue &q,std::uintptr_t descriptor) noexcept {
    for(unsigned i=0;i<q.count;++i)if(q.nodes[i].descriptor==descriptor)return &q.nodes[i];
    return nullptr;
}
bool native_nodes_match(const OwnedPlan &p,const PostQueue &q) noexcept {
    if(!same_post_frame(p.before,q)||q.scratch!=p.before.scratch||q.attachments!=p.before.attachments||
       q.taa!=p.before.taa||q.fxaa!=p.before.fxaa||q.enabled!=1||q.collecting!=0||
       q.packet_count<p.before.packet_count)return false;
    for(unsigned i=0;i<p.assigned.count;++i){
        const auto &b=p.assigned.nodes[i];const auto *a=node(q,b.descriptor);
        if(!a||*a!=b)return false;
    }
    return true;
}
}
bool plan_owned_pass(const PostQueue &q,std::uintptr_t image,std::uintptr_t descriptor,
                     std::uintptr_t unit,std::uintptr_t vtable,OwnedPlan &p) noexcept {
    p={};
    if(qualify_owned_slot(q,image,descriptor)!=QueueAdmission::ready || !unit || !vtable ||
       (q.count!=4&&q.count!=5) || q.taa || q.fxaa || q.rectangle!=std::array<unsigned,4>{0,0,2560,1440})return false;
    std::array<std::uintptr_t,4> owners={q.attachments[0],q.scratch[0],q.scratch[1],q.attachments[1]};
    for(unsigned i=0;i<4;++i){
        if(!owners[i])return false;
        for(unsigned j=0;j<i;++j)if(owners[i]==owners[j])return false;
    }
    unsigned found{};auto assigned=q;
    for(unsigned i=0;i<q.count;++i){
        const auto &n=q.nodes[i];const auto it=std::find(orders.begin(),orders.end(),n.order);
        if(it==orders.end())return false;
        const auto k=static_cast<unsigned>(it-orders.begin());
        if((found&(1U<<k))||!n.descriptor||n.phase!=15||n.flags!=flags[k]||
           n.vtable!=(vtables[k]?image+vtables[k]:0)||bool(n.unit)!=(k!=3))return false;
        for(unsigned j=0;j<i;++j)if(q.nodes[j].descriptor==n.descriptor)return false;
        // All admitted shapes have both endpoints overwritten before any
        // callback by scheduler 0x24f1fc0. Never dereference collection values.
        // Haze swaps scratch1 -> scratch0, so the final copy consumes scratch0.
        auto &expected=assigned.nodes[i];
        constexpr unsigned input_owner[]={0,1,0,2,2};
        constexpr unsigned output_owner[]={1,2,2,3,1};
        expected.input=k==2?0:owners[k==3&&q.count==5?1:input_owner[k]];
        expected.output=owners[output_owner[k]];
        found|=1U<<k;
    }
    if(found!=(q.count==4?15U:31U))return false;
    p={descriptor,unit,vtable,q.scratch[0],q,assigned};return true;
}
bool owned_callback_matches(const OwnedPlan &p,const PostQueue &q) noexcept {
    if(q.count!=p.before.count+1||!native_nodes_match(p,q))return false;
    const auto *n=node(q,p.descriptor);
    if(!n||n->unit!=p.unit||n->vtable!=p.vtable||n->phase!=15||n->order!=2||n->input||
       n->output!=p.color||n->flags!=std::array<unsigned char,3>{0,1,0})return false;
    for(unsigned i=1;i<q.count;++i)if(q.nodes[i-1].order>=q.nodes[i].order)return false;
    return true;
}
bool owned_return_matches(const OwnedPlan &p,const PostQueue &q) noexcept {return owned_callback_matches(p,q);}
bool owned_restored_matches(const OwnedPlan &p,const PostQueue &q) noexcept {
    if(q.count!=p.before.count||!native_nodes_match(p,q))return false;
    for(unsigned i=0;i<q.count;++i)if(q.nodes[i]!=p.assigned.nodes[i])return false;
    return true;
}
}
