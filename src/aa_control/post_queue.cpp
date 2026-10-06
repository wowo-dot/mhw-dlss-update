#include "post_queue.hpp"
#include <ostream>
namespace mhw::aa_control {
namespace {
bool range(std::uintptr_t p,std::uintptr_t n) {return p && p<=UINTPTR_MAX-n;}
}
bool read_post_queue(const temporal_bindings::Reader &r,std::uintptr_t image,
                    std::uintptr_t manager,std::uintptr_t context,std::uintptr_t view,PostQueue &q) {
    q={};q.manager=manager;q.context=context;q.view=view;
    std::uintptr_t table{},primary{},active{},owner{};unsigned char api{};
    if(!range(image,0x55d3000)||!range(manager,0xd0)||!range(context,0x2b84)||!range(view,0x630)||
       !r.get(image+0x51cc080,api)||api!=1||!r.get(image+0x51cc238,active)||active!=manager||
       !r.get(image+0x51c4480,q.renderer)||!range(q.renderer,0x1f570)||context!=q.renderer+0x710||
       !r.get(q.renderer+0x1f450,primary)||primary!=view||!r.get(context+0x1d0,primary)||primary!=view||
       !r.get(image+0x51c4368,table)||!range(table,0xe9e6)||!r.get(table+0xe9e4,q.taa)||q.taa>1||
       !r.get(table+0xe9e5,q.fxaa)||q.fxaa>1||!r.get(q.renderer+0x1f210,q.frame)||!q.frame||
       !r.get(q.renderer+0x1f208,q.epoch)||!r.get(q.renderer+0x1f200,q.bank)||q.bank>1||
       !r.get(manager+0x38,q.enabled)||q.enabled>1||!r.get(manager+0x39,q.collecting)||q.collecting>1||
       !r.get(manager+0x3c,q.count)||q.count>post_queue_limit||!r.get(context+0x2b80,q.packet_count))return false;
    for(unsigned i=0;i<4;++i)if(!r.get(context+0x5c+4*i,q.rectangle[i]))return false;
    if(q.rectangle[0]>=q.rectangle[2]||q.rectangle[1]>=q.rectangle[3]||q.rectangle[2]>8192||q.rectangle[3]>8192)return false;
    for(unsigned i=0;i<2;++i) {
        if(!r.get(manager+0xc0+8*i,q.scratch[i])||!r.get(view+0x3e0+0x40*i,owner))return false;
        if(owner&&(!range(owner,0x60)||!r.get(owner+0x58,q.attachments[i])))return false;
    }
    for(unsigned i=0;i<q.count;++i) {
        auto &n=q.nodes[i];
        if(!r.get(manager+0x40+8*i,n.descriptor)||!range(n.descriptor,0x23)||
           !r.get(n.descriptor,n.phase)||!r.get(n.descriptor+4,n.order)||
           !r.get(n.descriptor+8,n.unit)||!r.get(n.descriptor+0x10,n.input)||!r.get(n.descriptor+0x18,n.output))return false;
        if(n.unit&&(!range(n.unit,8)||!r.get(n.unit,n.vtable)))return false;
        for(unsigned j=0;j<3;++j)if(!r.get(n.descriptor+0x20+j,n.flags[j])||n.flags[j]>1)return false;
        for(unsigned j=0;j<i;++j)if(n.descriptor==q.nodes[j].descriptor)return false;
    }
    return true;
}
bool same_post_frame(const PostQueue &a,const PostQueue &b) noexcept {
    return a.manager==b.manager&&a.context==b.context&&a.view==b.view&&a.renderer==b.renderer&&
        a.frame==b.frame&&a.epoch==b.epoch&&a.bank==b.bank&&a.rectangle==b.rectangle;
}
void write_post_queue(std::ostream &out,const PostQueue &q){
    out<<"{\"manager\":"<<q.manager<<",\"context\":"<<q.context<<",\"view\":"<<q.view<<",\"renderer\":"<<q.renderer
       <<",\"frame\":"<<q.frame<<",\"epoch\":"<<q.epoch<<",\"bank\":"<<q.bank<<",\"count\":"<<q.count<<",\"packets\":"<<q.packet_count
       <<",\"enabled\":"<<unsigned(q.enabled)<<",\"collecting\":"<<unsigned(q.collecting)<<",\"taa\":"<<unsigned(q.taa)<<",\"fxaa\":"<<unsigned(q.fxaa)<<",\"rectangle\":[";
    for(unsigned i=0;i<4;++i)out<<(i?",":"")<<q.rectangle[i];
    out<<"],\"scratch\":["<<q.scratch[0]<<','<<q.scratch[1]<<"],\"attachments\":["<<q.attachments[0]<<','<<q.attachments[1]<<"],\"nodes\":[";
    for(unsigned i=0;i<q.count&&i<post_queue_limit;++i){const auto &n=q.nodes[i];out<<(i?",":"")<<"{\"descriptor\":"<<n.descriptor
        <<",\"unit\":"<<n.unit<<",\"vtable\":"<<n.vtable<<",\"phase\":"<<n.phase<<",\"order\":"<<n.order<<",\"input\":"<<n.input<<",\"output\":"<<n.output
        <<",\"flags\":["<<unsigned(n.flags[0])<<','<<unsigned(n.flags[1])<<','<<unsigned(n.flags[2])<<"]}";}
    out<<"]}";
}
QueueAdmission qualify_owned_slot(const PostQueue &q,std::uintptr_t image,std::uintptr_t owned) noexcept {
    if(!image||!q.manager||!q.context||!q.view||!owned||q.count>post_queue_limit||image>UINTPTR_MAX-0x55d3000)return QueueAdmission::invalid;
    if(!q.enabled||!q.collecting)return QueueAdmission::wrong_phase;
    for(unsigned i=0;i<q.count;++i) {
        if(q.nodes[i].descriptor==owned)return QueueAdmission::duplicate;
        const auto vt=q.nodes[i].vtable;
        if(vt==image+0x351bda8||vt==image+0x351c1a8||vt==image+0x354dd78||vt==image+0x354df48)return QueueAdmission::native_aa_present;
    }
    return q.count<post_queue_limit?QueueAdmission::ready:QueueAdmission::full;
}
}
