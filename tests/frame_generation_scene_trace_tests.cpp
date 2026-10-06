#include "frame_generation/scene_trace.hpp"
#include <stdexcept>
#include <sstream>
#include <memory>
using Trace=mhw::frame_generation::SceneTrace;
using Buffers=mhw::frame_generation::PresentationBuffers;
void require(bool ok){if(!ok)throw std::runtime_error("scene trace contract");}
void buffer_map(){
    mhw::frame_generation::PresentationBufferMap m;
    require(m.snapshot().generation==0&&!m.snapshot().known_mask);
    m.buffer(9,0,100);require(m.snapshot().foreign==1&&!m.snapshot().known_mask); // No chain yet.
    m.chain(9);require(m.snapshot().generation==1&&m.snapshot().swap==9);
    m.buffer(9,0,100);m.buffer(9,1,101);m.buffer(9,2,102);m.buffer(9,2,0);
    std::uint64_t label{};
    require(m.snapshot().known_mask==7&&m.snapshot().label(2,label)&&label==102&&!m.snapshot().label(3,label));
    require(m.snapshot().contains(101)&&!m.snapshot().contains(0)&&!m.snapshot().contains(999));
    m.buffer(9,1,201);require(m.snapshot().conflicts==1&&m.snapshot().labels[1]==201);
    m.buffer(9,Buffers::capacity,1);require(m.snapshot().out_of_range==1);
    m.buffer(8,0,1);m.game_index(8,0);m.present(8,0);m.resized(8);require(m.snapshot().foreign==5);
    // presents-1 identifies a host index query in the frame being presented.
    m.game_index(9,2);m.present(9,2);
    require(m.snapshot().game_index==2&&m.snapshot().game_index_present==0&&m.snapshot().presents==1&&m.snapshot().present_index==2);
    m.resized(9);const auto r=m.snapshot();
    require(r.generation==2&&!r.known_mask&&!r.contains(100)&&r.game_index==Buffers::none&&r.present_index==Buffers::none&&
            r.conflicts==1&&r.foreign==5&&r.out_of_range==1&&r.presents==0);
    m.chain(0);require(m.snapshot().generation==3&&!m.snapshot().swap);
}
void diagnostic_scope(Trace::Identity id,Trace::Event a,std::span<const std::array<std::uint64_t,2>> batch){
    const auto storage=std::make_unique<Trace>();auto &t=*storage;
    Buffers request;request.swap=9;request.generation=4;request.known_mask=3;request.labels[0]=500;request.labels[1]=501;
    require(t.request(1,request)&&t.begin(id));
    require(t.frame(0).request_buffers.generation==4&&t.match(70)==Trace::followed&&t.match(501)==Trace::presentation&&
            t.match(502)==Trace::unmatched&&t.match(0)==Trace::unmatched);
    a.packet=id.packet;const auto final=t.event(id.frame,id.epoch,a);t.returned(id.frame,id.epoch,final,true,true);
    auto depth=a;depth.stage=Trace::none;depth.diagnostic=true;depth.output_scope=1;depth.dsv=77;depth.list=400;depth.qpc=2;
    const auto pending=t.event(id.frame,id.epoch,depth);t.returned(id.frame,id.epoch,pending,true,false);
    auto unreturned=depth;unreturned.list=100;const auto open=t.event(id.frame,id.epoch,unreturned);
    // A presentation-only copy is diagnostic and never extends the followed path.
    auto copy=a;copy.kind=4;copy.stage=Trace::none;copy.diagnostic=copy.presentation_label=true;copy.copy[0]=501;copy.copy[1]=600;copy.resource=501;
    const auto edge=t.event(id.frame,id.epoch,copy);t.returned(id.frame,id.epoch,edge,true,false);require(!t.tracked(600));
    require(t.frame(0).diagnostic_count==3&&t.frame(0).diagnostic_events==3);
    for(unsigned i=0;i<Trace::diagnostic_capacity;++i)(void)t.event(id.frame,id.epoch,depth);
    require(t.frame(0).diagnostic_count==Trace::diagnostic_capacity&&t.frame(0).diagnostic_overflow==3&&!t.frame(0).overflow);
    // Gating capacity is preserved after diagnostic saturation.
    auto barrier=a;barrier.kind=2;barrier.stage=Trace::none;barrier.list=100;
    require(t.event(id.frame,id.epoch,barrier)!=Trace::none);
    t.submit(300,3,batch);t.submit(300,4,batch);
    require(t.frame(0).duplicate_submissions>0); // Gating list 100 submitted twice remains a gate failure.
    const auto gate_storage=std::make_unique<Trace>();auto &g=*gate_storage;
    require(g.request(1,request)&&g.begin(id));
    const auto f2=g.event(id.frame,id.epoch,a);g.returned(id.frame,id.epoch,f2,true,true);
    const auto late=g.event(id.frame,id.epoch,depth);g.returned(id.frame,id.epoch,late,true,false); // list 400: never submitted.
    auto lost=unreturned;lost.list=200;(void)g.event(id.frame,id.epoch,lost); // Executor never returns.
    g.reset(400,1);require(g.frame(0).diagnostic_premature_resets==1&&!g.frame(0).premature_resets);
    g.submit(300,3,batch);
    const std::array<std::array<std::uint64_t,2>,1> repeat{{{200,1}}};g.submit(300,4,repeat);
    require(g.frame(0).diagnostic_duplicate_submissions==1&&!g.frame(0).duplicate_submissions);
    g.choice(id.frame,id.epoch,2);
    Buffers at_present=request;at_present.presents=8;at_present.present_index=1;
    g.present(id.frame+1,21,1,4,5,6,at_present);
    // Diagnostic events cannot block or authorize the preserved closure.
    require(g.frame(0).presented&&g.frame(0).diagnostic_unreturned_at_present==1&&g.frame(0).diagnostic_unsubmitted_at_present==1&&
            g.frame(0).present_buffers.present_index==1&&g.frame(0).present_buffers.presents==8);
    g.reset(400,1);require(g.frame(0).diagnostic_premature_resets==1); // Closed captures do not count later resets.
    require(g.match(501)==Trace::unmatched&&g.event(id.frame,id.epoch,depth)==Trace::none);
    std::ostringstream out;out<<std::boolalpha;g.write(out);const auto text=out.str();
    require(text.find("\"policy\":\"submitted-scene-depth-presentation-map-v5\"")!=std::string::npos);
    require(text.find("\"diagnostic_capacity\":512")!=std::string::npos&&text.find("\"output_scope\":1")!=std::string::npos);
    require(text.find("\"present_buffers\":{\"swap\":9,\"generation\":4")!=std::string::npos);
    require(text.find("\"labels\":[500,501,0,")!=std::string::npos&&text.find("\"present_index\":1}")!=std::string::npos);
    (void)open;
}
int main(){
    // Production storage is static. Multiple bounded trace arrays in this
    // single test must not exhaust the default Windows thread stack.
    const auto storage=std::make_unique<Trace>();auto &t=*storage;
    require(!t.request(0)&&!t.request(2));require(t.request(1)&&!t.request(1)&&!t.request(2));
    Trace::Identity id{10,20,30,40,50,60,70,2560,1440,87,0};
    auto bad=id;bad.target=bad.source;require(!t.begin(bad));require(t.wants_frame()&&t.begin(id));
    require(!t.active(11,20)&&!t.active(10,21));
    Trace::Event a;a.list=100;a.generation=1;a.packet=50;a.resource=70;a.qpc=1;
    a.stage=2;a.api_state_matched=a.pipeline_read=true;a.native_frame=11;a.native_epoch=21;
    t.present(10,20,1,4,5,1);require(!t.frame(0).presented); // No submitted final draw yet.
    const auto first=t.event(10,20,a);require(first==0);t.returned(10,20,first,true,true);
    auto b=a;b.list=200;b.qpc=2;b.stage=Trace::none;require(t.event(10,20,b)==1);t.returned(10,20,1,true,false);
    t.present(11,21,1,4,5,3);require(!t.frame(0).presented);
    std::array batch{std::array<std::uint64_t,2>{200,1},std::array<std::uint64_t,2>{100,1}};
    t.submit(300,3,batch);require(t.frame(0).observations[0].batch_index==1&&t.frame(0).observations[1].batch_index==0);
    t.present(11,21,1,4,5,6);require(!t.frame(0).presented); // Submission without sealed choice.
    t.choice(10,20,2);t.present(11,21,1,4,5,6);
    require(t.frame(0).presented&&t.frame(0).choice==2&&t.frame(0).present_frame==11&&t.frame(0).present_epoch==21);
    require(t.event(10,20,a)==Trace::none);t.submit(300,7,batch);require(!t.frame(0).duplicate_submissions);
    t.reset(100,1);require(t.frame(0).observations[0].reset&&!t.frame(0).premature_resets);
    require(t.request(2));id.frame++;require(t.begin(id));
    for(unsigned i=0;i<Trace::event_capacity+1;++i)(void)t.event(11,20,a);
    require(t.frame(1).events==Trace::event_capacity+1&&t.frame(1).overflow==1);
    t.reset(100,1);require(t.frame(1).premature_resets==Trace::event_capacity);
    t.submit(300,8,batch);t.submit(300,9,batch);require(t.frame(1).duplicate_submissions==Trace::event_capacity);
    t.choice(11,20,2);t.present(12,21,2,4,5,10);require(!t.frame(1).presented&&!t.request(3));
    const auto complete_storage=std::make_unique<Trace>();auto &complete=*complete_storage;
    for(unsigned i=1;i<=Trace::capacity;++i){require(complete.request(i));id.frame++;require(complete.begin(id));
        a.packet=id.packet;const auto event=complete.event(id.frame,id.epoch,a);
        complete.returned(id.frame,id.epoch,event,true,true);complete.submit(300,3,batch);
        complete.choice(id.frame,id.epoch,2);complete.present(id.frame+1,21,i,4,5,6);require(complete.frame(i-1).presented);}
    require(!complete.request(Trace::capacity+1));
    const auto return_storage=std::make_unique<Trace>();auto &return_gate=*return_storage;
    require(return_gate.request(1)&&return_gate.begin(id));
    const auto unreturned=return_gate.event(id.frame,id.epoch,a);return_gate.choice(id.frame,id.epoch,2);return_gate.submit(300,3,batch);
    return_gate.present(id.frame+1,21,1,4,5,6);require(!return_gate.frame(0).presented);
    return_gate.returned(id.frame,id.epoch,unreturned,true,false);
    return_gate.present(id.frame+1,21,1,4,5,6);require(!return_gate.frame(0).presented);
    return_gate.returned(id.frame,id.epoch,unreturned,true,true);
    return_gate.present(id.frame+1,21,1,4,5,6);require(return_gate.frame(0).presented);
    const auto copy_storage=std::make_unique<Trace>();auto &copies=*copy_storage;
    require(copies.request(1)&&copies.begin(id)&&copies.tracked(70)&&!copies.tracked(80));
    const auto final_copy=copies.event(id.frame,id.epoch,a);copies.returned(id.frame,id.epoch,final_copy,true,true);
    auto copy=a;copy.kind=4;copy.stage=Trace::none;copy.copy[0]=70;copy.copy[1]=80;copy.resource=70;copy.qpc=2;
    const auto edge=copies.event(id.frame,id.epoch,copy);
    copies.returned(id.frame,id.epoch,edge,false,false);require(!copies.tracked(80));
    copies.returned(id.frame,id.epoch,edge,true,true);require(!copies.tracked(80));
    copies.returned(id.frame,id.epoch,edge,true,false);require(copies.tracked(80));
    copies.choice(id.frame,id.epoch,2);copies.present(id.frame+1,21,1,4,5,6);require(!copies.frame(0).presented);
    for(unsigned i=2;i<Trace::resource_capacity;++i){copy.copy[0]=80;copy.copy[1]=80+i;copy.resource=80;
        const auto e=copies.event(id.frame,id.epoch,copy);copies.returned(id.frame,id.epoch,e,true,false);}
    copy.copy[1]=999;const auto excess=copies.event(id.frame,id.epoch,copy);copies.returned(id.frame,id.epoch,excess,true,false);
    require(!copies.tracked(999)&&copies.frame(0).resource_count==Trace::resource_capacity&&copies.frame(0).resource_overflow==1);
    auto unknown=a;unknown.list=999;unknown.generation=0;
    unknown.native_cache_read_mask=3;unknown.native_cache[0]=123;unknown.native_cache[1]=999;
    unknown.raster_observation={1,1,1,1,1,1,1,1,15,1,1,1,0,1,1,123,1,1,456};
    for(unsigned i=0;i<50;++i)copies.reject(0,unknown);
    copies.reject(13,unknown);copies.reject(Trace::rejection_reasons,unknown);
    require(copies.frame(0).rejected[0]==50&&copies.frame(0).sample_count==3);
    require(copies.frame(0).samples[0].native_cache_read_mask==3&&copies.frame(0).samples[0].raster_observation[18]==456);
    copies.submit(300,3,batch);copies.present(id.frame+1,21,1,4,5,6);
    require(copies.frame(0).presented&&!copies.frame(0).samples[0].batch); // Incomplete samples cannot prevent closure.
    std::ostringstream out;out<<std::boolalpha;t.write(out);
    require(out.str().find("\"gpu_work_added\":false")!=std::string::npos);
    require(out.str().find("\"schema\":5")!=std::string::npos);
    buffer_map();diagnostic_scope(id,a,batch);
    std::ostringstream diagnostic;diagnostic<<std::boolalpha;copies.write(diagnostic);
    require(diagnostic.str().find("\"native_cache_read_mask\":3")!=std::string::npos);
    require(diagnostic.str().find("\"raster_observation\":[1,1,1,1,1,1,1,1,15,1,1,1,0,1,1,123,1,1,456]")!=std::string::npos);
}
