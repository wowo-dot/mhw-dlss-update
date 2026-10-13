#pragma once
#include <thread>
// Bounded synthetic game structures with real COM textures. Exercises the
// identity plan's discovery (v0.26.0: the only plan) and full_post before
// runtime admission; no game process is required.
namespace stream_plan_fixture {
using namespace mhw::fixtures::effects;
namespace route=mhw::scaled_temporal::stream_route;
struct Arena {
 std::uintptr_t base;std::size_t used{},size;
 explicit Arena(std::size_t n):base(reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr,n,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE))),size(n){require(base!=0,"fixture arena");}
 ~Arena(){VirtualFree(reinterpret_cast<void *>(base),0,MEM_RELEASE);}
 std::uintptr_t take(std::size_t n){n=(n+15)&~std::size_t(15);require(used+n<=size,"fixture arena bounds");auto p=base+used;used+=n;return p;}
};
template<class T>void put(std::uintptr_t p,const T &v){std::memcpy(reinterpret_cast<void *>(p),&v,sizeof(v));}
#include "scaled_queue_completion_fixture.hpp"
unsigned check_plan(Fixture &f){
 namespace tb=mhw::temporal_bindings;Arena image(0x5200000),a(0x50000);const auto renderer=a.take(0x21000),context=renderer+0x710,view=a.take(0x1000),layer=a.take(0x200),motion=a.take(0x1000),grade=a.take(0x1000),filters=a.take(0x200);
 const auto final_copy=a.take(0x50);
 const auto saved=route::cfg;route::cfg={image.base,renderer,0,reinterpret_cast<std::uintptr_t>(f.device.Get())};route::width=128;route::height=64;route::render_w=64;route::render_h=32;
 auto storage=std::make_unique<route::Plan>();auto &p=*storage;auto &s=p.stamp;s.context=context;s.view=view;s.layer=layer;s.frame=77;s.epoch=77;s.rectangle={0,0,64,32};s.async=1;
 // Native +93 is one byte; its neighbours are unrelated nonzero flags.
 put(renderer+0x93,0xa55a0101U);
 put(renderer+0x1f210,s.frame);put(renderer+0x1f208,s.epoch);put(renderer+0x1f450,view);put(context+0x1d0,view);
 auto desc=texture(128,64,DXGI_FORMAT_R11G11B10_FLOAT);desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
 std::array<ComPtr<ID3D12Resource>,5> resources;std::array<std::uintptr_t,5> owners;
 for(unsigned i=0;i<5;++i){resources[i]=resource(f.device.Get(),desc,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COMMON);owners[i]=a.take(0x100);const auto backend=a.take(0x40);
  put(owners[i],image.base+0x3537870);put(owners[i]+0x60,backend);put(owners[i]+0x88,128U+(64U<<16));put(owners[i]+0x90,34U);put(backend+0x20,reinterpret_cast<std::uintptr_t>(resources[i].Get()));}
 require(tb::read_owner(route::reader,owners[0],s.inputs[4]),"fixture resolve owner");s.inputs[0].object=owners[2];
 put(image.base+0x51cc238,filters);put(filters+0x3c,4U);
 const std::array<std::uintptr_t,4> entries{layer+0x140,motion+0x140,grade+0x140,grade+0x238};
 for(unsigned i=0;i<4;++i)put(filters+0x40+8*i,entries[i]);
 put(layer+0x140,15U);put(layer+0x144,2U);put(layer+0x148,layer);put(layer+0x150,owners[2]);put(layer+0x158,owners[1]);
 put(motion,image.base+0x3537a70);put(motion+0x140,15U);put(motion+0x144,100U);put(motion+0x148,motion);put(motion+0x150,owners[1]);put(motion+0x158,owners[2]);
 put(grade,image.base+0x3538400);put(grade+0x140,15U);put(grade+0x148,grade);put(grade+0x238,15U);put(grade+0x23c,0x7fffff1U);put(grade+0x248,owners[2]);put(grade+0x250,owners[3]);put(grade+0x258,static_cast<unsigned char>(1));
 put(image.base+0x4be7450,1U);put(context+0x718,owners[3]);const auto attachment=a.take(0x60);put(view+0x460,attachment);put(attachment+0x58,owners[4]);
 const auto queue=a.take(16*32),copy=a.take(0x50);put(context+0x2b78,queue);put(context+0x2b80,4U);
 put(queue,0x78000002U);put(queue+8,copy);put(copy,13U);put(copy+0x10,owners[1]);put(copy+0x18,owners[0]);put(copy+0x38,s.rectangle);
 // The final quad's draw (stage 16, opcode 0), where the game appends it.
 const auto quad=a.take(0x50);put(queue+48,0x80000000U);put(queue+56,quad);
 const auto vertices=a.take(3*64);put(context+0x2b50,vertices);
 const auto depth=a.take(0x200),blend=a.take(0x300),raster=a.take(0x200),depth_pointer=a.take(8);
 put(image.base+0x51c73d0,renderer);put(renderer+0x2480,depth_pointer);put(depth_pointer,depth);
 put(depth+0x60,depth+0x100);put(blend+0x98,blend+0x100);put(raster+0x68,raster+0x100);
 // The final quad is enqueued first. Later native filters replace tBaseMap
 // in the mutable CPU context before the continuous plan is built.
 const std::array<unsigned,4> final_rect{0,0,64,32};const std::array<unsigned,2> dimensions{128,64};
 const std::array<float,2> offset{-.5F/128,-.5F/64};mhw::projection_binding::Frame frame;
 frame.caller=image.base+0x23c7b08;frame.gpr[2]=context;frame.gpr[3]=reinterpret_cast<std::uintptr_t>(final_rect.data());
 frame.gpr[4]=reinterpret_cast<std::uintptr_t>(dimensions.data());frame.stack[0]=reinterpret_cast<std::uintptr_t>(offset.data());frame.stack[1]=1;
 put(context+0x5c,std::array<unsigned,4>{0,0,128,64});put(context+0x2b80,3U);
 const auto final_slot=route::final_before(frame);require(final_slot==s.frame%32,"final quad entry");
 put(context+0x2b80,4U);route::final_after(final_slot,true);require(route::final_quads[final_slot].valid,"final quad completed");
 require(route::final_quads[final_slot].stamp.async==1,"one-byte async identity");
 put(context+0x718,owners[1]); // Same tBaseMap parameter, later motion input.
 // v0.26.0: the identity plan from the completed queue. TAA's copy (15, 2) and
 // the final copy (stage 16: the grade's output into the view's attachment),
 // each exactly once and of the expected shape; the four owners pinned at the
 // output size. At a reduced render scale only a frame full_post armed is
 // planned; at render scale 1.0 every frame is.
 put(final_copy,13U);put(final_copy+0x10,owners[4]);put(final_copy+0x18,owners[3]);put(final_copy+0x38,std::array<unsigned,4>{0,0,128,64});
 put(queue+16,0x80000000U);put(queue+24,final_copy);put(context+0x2b80,2U);
 route::full_frame.store(s.frame);
 if(!route::build_plan(p))throw std::runtime_error("identity plan admission "+std::to_string(route::plan_rejection)+" detail "+std::to_string(route::plan_detail));
 require(p.identity&&p.copy==copy&&p.final_copy==final_copy&&p.resources[0]==resources[0]&&p.resources[1]==resources[1]&&
         p.resources[2]==resources[3]&&p.resources[3]==resources[4]&&route::handoff_count(p)==3,"identity plan: TAA copy, final copy, four owners");
 unsigned rejected{};const auto bad=[&](unsigned why){auto candidate=std::make_unique<route::Plan>();auto &q=*candidate;q.stamp=s;
  require(!route::build_plan(q)&&route::plan_rejection==why,"malformed identity graph rejected");++rejected;};
 put(copy+0x38,std::array<unsigned,4>{0,0,63,32});bad(13);put(copy+0x38,s.rectangle);
 put(queue+32,0x78000002U);put(queue+40,copy);put(context+0x2b80,3U);bad(12);
 put(queue+32,0x80000000U);put(queue+40,final_copy);bad(12);put(queue+32,0U);put(queue+40,std::uintptr_t{});put(context+0x2b80,2U);
 put(context+0x2b80,1U);bad(15);put(context+0x2b80,2U);
 put(final_copy+0x38,std::array<unsigned,4>{0,0,64,32});bad(13);put(final_copy+0x38,std::array<unsigned,4>{0,0,128,64});
 put(grade+0x250,owners[2]);bad(15);put(grade+0x250,owners[3]);
 put(attachment+0x58,owners[0]);bad(15);put(attachment+0x58,owners[4]);
 put(layer+0x150,owners[0]);bad(3);put(layer+0x150,owners[2]);
 put(grade,std::uintptr_t{});bad(6);put(grade,image.base+0x3538400);
 put(renderer+0x1f210,s.frame+1);bad(1);put(renderer+0x1f210,s.frame);
 put(context+0x2b80,65535U);bad(8);put(context+0x2b80,2U);
 {const auto unarmed=route::unarmed;route::full_frame.store(UINT64_MAX);auto q=std::make_unique<route::Plan>();q->stamp=s;
  require(!route::build_plan(*q)&&route::unarmed==unarmed+1,"a scaled frame full_post did not arm is not planned");
  route::render_w=128;route::render_h=64;q=std::make_unique<route::Plan>();q->stamp=s;
  require(route::build_plan(*q)&&q->identity,"render scale 1.0: planned without arming");route::render_w=64;route::render_h=32;}
 route::full_frame.store(s.frame);require(route::build_plan(p),"restored identity graph");route::full_frame.store(UINT64_MAX);
 std::cout<<"PASS identity plan: TAA copy and final copy discovered, "<<rejected<<" malformed graphs rejected, unarmed scaled frames left native\n";
 // v0.24.10 post census: the queue as recorded, without changing it.
 {route::census=route::PostCensus{};route::census_observe(s.frame,context,&s);
  const auto &cs=route::census.samples[0];
  require(route::census.captured==1&&cs.commands==2&&cs.packet_count==2&&cs.descriptor_count==4&&cs.render_w==64&&cs.width==128,"census sample");
  const auto resource_of=[&](unsigned index){return index<cs.resource_count?cs.resources[index]:route::PostCensus::Resource{};};
  const auto &copy_packet=cs.packets[0],&final_packet=cs.packets[1];
  require(copy_packet.key==0x78000002&&copy_packet.opcode==13&&copy_packet.rect==std::array<int,4>{0,0,64,32}&&
          resource_of(copy_packet.target).pointer==reinterpret_cast<std::uintptr_t>(resources[1].Get())&&
          resource_of(copy_packet.source).pointer==reinterpret_cast<std::uintptr_t>(resources[0].Get()),"census copy");
  require(final_packet.key==0x80000000U&&final_packet.opcode==13&&final_packet.rect==std::array<int,4>{0,0,128,64}&&
          resource_of(final_packet.target).pointer==reinterpret_cast<std::uintptr_t>(resources[4].Get())&&
          resource_of(final_packet.source).pointer==reinterpret_cast<std::uintptr_t>(resources[3].Get()),"census final copy");
  require(cs.descriptors[1].order==100&&cs.descriptors[1].type==0x3537a70&&cs.descriptors[2].type==0x3538400&&cs.descriptors[3].order==0x7fffff1,"census descriptors");
  require(cs.inputs[4][4]<cs.resource_count&&cs.resources[cs.inputs[4][4]].pointer==reinterpret_cast<std::uintptr_t>(resources[0].Get()),"census TAA bindings");
  route::census_observe(s.frame,context,&s);require(route::census.captured==2,"census latest sample");
  route::census_observe(s.frame,context,&s);require(route::census.captured==2,"census bounded");
  std::ostringstream first,second;route::write_summary(first);route::census_write(second);
  require(first.str().find("\"post_census\":{\"captured\":2,\"samples\":[{")!=std::string::npos&&first.str().find("\"packets\":[[2013265922,13,0,0,64,32,")!=std::string::npos&&second.str()=="{\"captured\":2,\"samples\":[]}","census written once, in the route summary");
  std::cout<<"PASS post census: descriptors, the TAA copy and the final copy of the native queue, written once\n";}
 // The full_post block records its own DLAA copy as the final copy.
 put(queue+16,0U);put(queue+24,std::uintptr_t{});put(context+0x2b80,4U);
 // full_post (v0.25.8): the final quad stays native and is the frame's
 // candidate. A queue the runtime admitted, with TAA's callback in the
 // registration list, enters the scheduler; after TAA's callback, for a frame
 // the runtime published there, the DLAA copy is recorded in the quad's key and
 // the frame armed, and the chain switches to the full rectangle. The render
 // rectangle is restored at the exit. The game's setViewport and copy helper
 // are stand-ins here; the helper appends packets as the game's does (key
 // (stage << 27) | ((+0x154 >> 5) + (+0x15c >> 5))).
 {struct Call {std::uintptr_t context{},destination{},source{};std::array<unsigned,4> rect{};unsigned key{},order{};};
  static std::vector<Call> copies;static std::vector<std::array<int,4>> viewports;copies.clear();viewports.clear();
  static Arena *helper_arena{};helper_arena=&a;
  route::full_copy=[](std::uintptr_t c,std::uintptr_t d,int ds,const unsigned *xy,std::uintptr_t s_,int ss,const unsigned *r,const unsigned *o){
   require(!o,"no optional copy argument");unsigned key{},sub{},count{};std::uintptr_t array{};
   require(route::field(c,0x154,key)&&route::field(c,0x15c,sub)&&route::field(c,0x2b80,count)&&route::field(c,0x2b78,array)&&count<32,"helper context");
   copies.push_back({c,d,s_,{r[0],r[1],r[2],r[3]},key,sub});
   const auto packet=helper_arena->take(0x50);put(packet,13U);put(packet+0x10,d);put(packet+0x18,s_);put(packet+0x24,ds);put(packet+0x28,ss);
   put(packet+0x30,std::array<unsigned,2>{xy[0],xy[1]});put(packet+0x38,std::array<unsigned,4>{r[0],r[1],r[2],r[3]});
   put(array+16ULL*count,((key&31U)<<27)|(((key>>5)+(sub>>5))&0x7ffffffU));put(array+16ULL*count+8,packet);put(c+0x2b80,count+1);};
  route::full_viewport=[](std::uintptr_t c,const int *r){viewports.push_back({r[0],r[1],r[2],r[3]});std::memcpy(reinterpret_cast<void *>(c+0x5c),r,16);};
  {std::string why;require(route::full_inputs.prepare(f.device.Get(),128,64,why),"full_post inputs prepared");}
  put(layer,image.base+0x351bda8);  // TAA's unit, for the registration list
  const std::array<int,4> render_rect{0,0,64,32},full_rect{0,0,128,64};const unsigned taa_key=15U|(2U<<5);
  std::uint64_t next_frame=s.frame+1;std::uintptr_t source_object{};
  // The runtime's publication at TAA's producer (sr::publish).
  const auto publish=[&](std::uint64_t armed){auto stamp=s;stamp.frame=armed;route::publish(stamp,tb::Packet{});};
  // The final quad, recorded natively (the queue grows by its draw) in key
  // (16, 0); the scheduler's context then holds TAA's key.
  const auto candidate=[&](bool published=true){const auto armed=next_frame++;if(published)publish(armed);
   put(renderer+0x1f210,armed);put(context+0x2b80,3U);put(context+0x5c,full_rect);put(context+0x154,16U);put(context+0x15c,0x12U);
   const auto quad=route::final_before(frame);require(quad!=UINT_MAX,"final quad entry");require(route::field(context,0x718,source_object),"final tBaseMap");
   route::full_candidate(frame,quad);put(context+0x2b80,4U);route::final_after(quad,true);require(route::final_quads[quad].valid,"the final quad stays native");
   put(context+0x5c,render_rect);put(context+0x154,taa_key);put(context+0x15c,0U);return armed;};
  const auto armed_frame=candidate();require(copies.empty()&&route::full_frame.load()!=armed_frame,"nothing armed at the final quad");
  // The final-quad helper also records other copies before the scheduler
  // (refraction, other views): they leave the candidate.
  route::full_candidate(frame,UINT_MAX);
  route::full_post_enter(context,true);require(copies.empty()&&route::full_frame.load()!=armed_frame,"nothing recorded at the scheduler's entry");
  route::full_candidate(frame,UINT_MAX);  // a filter's own copy through the same helper, inside the chain
  route::full_post_taa(context);
  unsigned key{};require(route::field(context,0x154,key)&&key==taa_key,"the scheduler's key restored");
  require(copies.size()==1&&copies[0].context==context&&copies[0].destination==owners[4]&&copies[0].source==source_object&&
          copies[0].rect==std::array<unsigned,4>{0,0,128,64}&&copies[0].key==16&&copies[0].order==0x12&&route::full_frame.load()==armed_frame,
          "the DLAA copy in the final quad's key after TAA's callback");
  {const auto &r=route::full_finals[armed_frame%route::full_finals.size()];
   require(r.frame==armed_frame&&r.packet&&!r.executed&&r.destination.object==owners[4]&&r.source.object==source_object&&
           route::full_counts.final_unverified.load()==0,"the DLAA copy registered");}
  std::array<int,4> now{};require(route::field(context,0x5c,now)&&now==full_rect&&viewports.size()==1,"full rectangle after TAA");
  route::full_post_taa(context);require(viewports.size()==1,"one switch per frame");
  route::full_post_exit(context);require(route::field(context,0x5c,now)&&now==render_rect&&viewports.size()==2,"render rectangle restored at the exit");
  route::full_post_enter(context,true);route::full_post_taa(context);require(viewports.size()==2&&copies.size()==1,"disarmed after the exit");
  // TAA's callback not in this frame's list (a transition), a rectangle other
  // than the render one, or a stale frame: nothing armed, the frame native.
  {const auto no_taa=route::full_counts.no_taa.load();put(layer+0x160,static_cast<unsigned char>(1));const auto unarmed=candidate();
   route::full_post_enter(context,true);route::full_post_taa(context);route::full_post_exit(context);put(layer+0x160,static_cast<unsigned char>(0));
   require(copies.size()==1&&viewports.size()==2&&route::full_counts.no_taa.load()==no_taa+1&&route::full_frame.load()!=unarmed,"TAA without a callback: not armed");}
  {const auto mismatches=route::full_counts.rect_mismatch.load();candidate();put(context+0x5c,full_rect);
   route::full_post_enter(context,true);route::full_post_taa(context);route::full_post_exit(context);
   require(copies.size()==1&&viewports.size()==2&&route::full_counts.rect_mismatch.load()==mismatches+1,"rectangle mismatch refused");}
  {const auto stale=route::full_counts.stale.load();const auto f2=candidate();put(renderer+0x1f210,f2+1);
   route::full_post_enter(context,true);route::full_post_taa(context);route::full_post_exit(context);
   require(copies.size()==1&&route::full_counts.stale.load()==stale+1,"a stale candidate refused");}
  // At the scheduler's entry the list is in registration order: the colour
  // grade's endpoint (no unit) before TAA's descriptor still arms (game-120).
  // An unreadable list is counted apart from TAA's absence.
  {put(filters+0x40,grade+0x238);put(filters+0x58,layer+0x140);const auto f2=candidate();route::full_post_enter(context,true);route::full_post_taa(context);
   require(copies.size()==2&&route::full_frame.load()==f2,"an endpoint without a unit before TAA's descriptor: armed");
   route::full_post_exit(context);require(viewports.size()==4,"switched and restored");
   const auto unreadable=route::full_counts.taa_unreadable.load(),no_taa=route::full_counts.no_taa.load();put(filters+0x3c,65U);candidate();
   route::full_post_enter(context,true);route::full_post_taa(context);route::full_post_exit(context);put(filters+0x3c,4U);
   require(copies.size()==2&&route::full_counts.taa_unreadable.load()==unreadable+1&&route::full_counts.no_taa.load()==no_taa,"an unreadable list: not armed");
   put(filters+0x40,layer+0x140);put(filters+0x58,grade+0x238);
   // v0.25.8: a frame the runtime did not publish at TAA's producer (game-121:
   // the tent menu), or a queue it did not admit, records nothing.
   {const auto unpublished=route::full_counts.unpublished.load();const auto f3=candidate(false);
    route::full_post_enter(context,true);route::full_post_taa(context);route::full_post_exit(context);
    require(copies.size()==2&&viewports.size()==4&&route::full_counts.unpublished.load()==unpublished+1&&route::full_frame.load()!=f3,"unpublished: native");
    const auto unadmitted=route::full_counts.unadmitted.load();const auto f4=candidate();
    route::full_post_enter(context,false);route::full_post_taa(context);route::full_post_exit(context);
    require(copies.size()==2&&viewports.size()==4&&route::full_counts.unadmitted.load()==unadmitted+1&&route::full_frame.load()!=f4,"unadmitted: native");}
   // At execution, the registered DLAA copy of a frame without a prepared plan
   // drops its copy (the final quad's upscale stays); a planned one runs as at
   // DLAA; an address a later packet reuses is not taken.
   {auto &r=route::full_finals[f2%route::full_finals.size()];mhw::projection_binding::Frame call{};call.gpr[3]=r.packet;
    const auto fallbacks=route::full_counts.fallbacks.load();const auto entry=route::full_final_before(call);
    require(entry==f2%route::full_finals.size()&&r.executed,"an unplanned DLAA copy taken");
    D3D12_TEXTURE_COPY_LOCATION d{reinterpret_cast<ID3D12Resource *>(r.destination.resource),D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,{}},
                                source{reinterpret_cast<ID3D12Resource *>(r.source.resource),D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,{}};
    const D3D12_BOX box{0,0,0,128,64,1};
    route::full_final_scope=entry;route::full_final_calls=0;route::full_final_copy(nullptr,&d,0,0,0,&source,&box,route::cfg.image+0x259caf0);
    route::full_final_scope=UINT_MAX;
    require(route::full_counts.fallbacks.load()==fallbacks+1&&route::full_final_before(call)==UINT_MAX,"its copy dropped, once");
    r.executed=false;auto &plan=route::plans[f2%route::plans.size()];plan.prepared=true;plan.identity=true;plan.final_copy=r.packet;
    require(route::full_final_before(call)==UINT_MAX&&r.executed,"a planned DLAA copy runs as at DLAA");
    plan.prepared=false;plan.identity=false;plan.final_copy=0;
    r.executed=false;const auto stale=route::full_counts.final_stale.load();put(r.packet+0x10,owners[1]);
    require(route::full_final_before(call)==UINT_MAX&&route::full_counts.final_stale.load()==stale+1&&!r.packet,"a reused address is not the DLAA copy");}
   // The scene capture (sScene image+0x51c4368, requested for this epoch, pass
   // 15, the main view's pass-15 target the quad's tBaseMap): its quad, after
   // the final quad, has the render region's texture coordinates; once the
   // armed frame's plan is prepared they cover the whole texture.
   {const auto scene=a.take(0xf000),target=a.take(0x80),capture=a.take(0x50),state=a.take(0x40),arena=a.take(0x100);
    std::uint64_t epoch{};require(route::field(renderer,0x1f208,epoch),"renderer epoch");std::uintptr_t saved_arena{},saved_target{};
    require(route::field(context,0x2b50,saved_arena)&&route::field(view,0x420,saved_target),"saved");
    put(image.base+0x51c4368,scene);put(scene+0xe988,a.take(0x10));put(scene+0xe990,15U);put(scene+0xe994,~0U);put(scene+0xe998,epoch);
    put(view+0x420,target);put(target+0x58,source_object);put(context+0x2b50,arena);
    put(capture,0U);put(capture+0x18,state);put(capture+0x40,4U);put(capture+0x44,1U);put(state+0x20,static_cast<unsigned char>(16));put(state+0x24,0x20U);
    const std::array<float,16> native{-1.F,1.F,0.F,0.F,-1.F,-1.F,0.F,.5F,1.F,1.F,.5F,0.F,1.F,-1.F,.5F,.5F};
    const std::array<float,16> whole{-1.F,1.F,0.F,0.F,-1.F,-1.F,0.F,1.F,1.F,1.F,1.F,0.F,1.F,-1.F,1.F,1.F};
    const auto capture_run=[&](bool keep_native){const auto f5=candidate();put(arena+0x30,native);put(queue+16*4,0x7fffffffU);put(queue+16*4+8,capture);put(context+0x2b80,5U);
     route::full_post_enter(context,true);route::full_post_taa(context);route::full_post_exit(context);
     const auto &c=route::full_captures[f5%route::full_captures.size()];require(route::full_frame.load()==f5&&c.valid&&c.frame==f5&&c.packet==capture,"capture found");
     if(!keep_native)put(arena+0x30+8,.25F);route::full_capture_apply(f5);std::array<float,16> now{};require(route::field(arena,0x30,now),"capture read");return now;};
    const auto widened=route::full_counts.captures.load();
    require(capture_run(true)==whole&&route::full_counts.captures.load()==widened+1,"the capture covers the whole chain output");
    const auto changed=route::full_counts.capture_refusals[4].load();
    require(capture_run(false)[2]==.25F&&route::full_counts.capture_refusals[4].load()==changed+1,"a changed capture left as it is");
    const auto other=route::full_counts.capture_refusals[2].load();put(target+0x58,owners[2]);const auto f6=candidate();
    route::full_post_enter(context,true);route::full_post_taa(context);route::full_post_exit(context);
    require(route::full_counts.capture_refusals[2].load()==other+1&&!route::full_captures[f6%route::full_captures.size()].valid,"a capture of another texture left as it is");
    put(image.base+0x51c4368,std::uintptr_t{});put(view+0x420,saved_target);put(context+0x2b50,saved_arena);route::full_captures={};}}
  // v0.25.2: the inputs' upscale slot. After TAA's callback, in its key, the
  // scheduler's context records an engine copy from TAA's output (the TAA
  // copy's source) into GBuffer5 at (15, 3); the armed frame's identity plan
  // takes it between the TAA copy and the final copy.
  {
   // The scene depth (view +0x628 -> +0xA0) and GBuffer4/5 (view +0x620 -> +0x78/+0x80).
   const auto container=a.take(0x100),gbuffer=a.take(0x100);std::array<std::uintptr_t,3> input_owners{},input_backings{},input_resources{};
   for(unsigned i=0;i<3;++i){input_owners[i]=a.take(0x100);input_backings[i]=a.take(0x80);input_resources[i]=a.take(0x10);
    put(input_owners[i],image.base+0x3537870);put(input_owners[i]+0x58,2U);put(input_owners[i]+0x60,input_backings[i]);put(input_owners[i]+0x88,128U+(64U<<16));
    put(input_owners[i]+0x90,i==0?16U:i==1?38U:40U);put(input_backings[i]+0x20,input_resources[i]);}
   put(view+0x628,container);put(container+0xa0,input_owners[0]);put(view+0x620,gbuffer);put(gbuffer+0x78,input_owners[1]);put(gbuffer+0x80,input_owners[2]);
   // The final quad's tBaseMap is the grade's output here, as in the game.
   const auto arm=[&](std::uint64_t armed,unsigned sub){
    publish(armed);put(renderer+0x1f210,armed);put(context+0x2b80,3U);
    put(context+0x154,16U);put(context+0x15c,0x12U);put(context+0x5c,full_rect);put(context+0x718,owners[3]);
    const auto quad=route::final_before(frame);require(quad!=UINT_MAX,"final quad for the slot");route::full_candidate(frame,quad);
    put(context+0x2b80,4U);route::final_after(quad,true);
    put(context+0x718,owners[1]);put(context+0x5c,render_rect);put(context+0x154,taa_key);put(context+0x15c,sub);
    route::full_post_enter(context,true);route::full_post_taa(context);route::full_post_exit(context);};
   const auto plan_for=[&](std::uint64_t armed){auto plan=std::make_unique<route::Plan>();plan->stamp=s;plan->stamp.frame=armed;plan->stamp.inputs[5].object=input_owners[1];
    require(route::build_plan(*plan)&&plan->identity,"armed frame planned as at DLAA");return plan;};
   const auto armed_slot=next_frame+1;const auto slots=route::full_counts.slots.load();arm(armed_slot,0x12U);
   const auto &q=route::full_slots[armed_slot%route::full_slots.size()];
   unsigned slot_key{},count{};std::uintptr_t array{},entry_packet{};std::uint32_t entry_key{};
   require(route::field(context,0x154,slot_key)&&slot_key==taa_key,"TAA's key restored");
   require(route::full_counts.slots.load()==slots+1&&q.valid&&q.frame==armed_slot&&q.context==context&&!q.executed&&
           q.depth.resource==input_resources[0]&&q.gbuffer4.resource==input_resources[1]&&q.gbuffer5.resource==input_resources[2],"slot recorded and verified");
   require(copies.back().destination==input_owners[2]&&copies.back().source==owners[0]&&copies.back().rect==std::array<unsigned,4>{0,0,1,1}&&
           q.source.object==owners[0],"TAA's output into GBuffer5, 1x1");
   require(route::field(context,0x2b80,count)&&count==6&&route::field(context,0x2b78,array)&&route::field(array+16ULL*5,0,entry_key)&&entry_key==0x78000003U&&
           route::field(array+16ULL*5,8,entry_packet)&&entry_packet==q.packet,"slot packet at key (15, 3)");
   {const auto plan=plan_for(armed_slot);
    require(plan->full_slot==q.packet&&route::handoff_count(*plan)==4&&route::final_node(*plan)==3,"the plan takes the slot before the final copy");
    // With late draws (the silhouettes): TAA, its copy, the slot, the late
    // draws, the final copy.
    plan->late_count=2;require(route::handoff_indexed(*plan,0)==3&&route::handoff_indexed(*plan,1)==4&&route::final_node(*plan)==5&&
     route::handoff_count(*plan)==6,"late draws between the slot and the final copy");
    plan->full_slot=0;require(route::handoff_indexed(*plan,0)==2&&route::final_node(*plan)==4&&route::handoff_count(*plan)==5,"late draws without a slot");
    plan->full_slot=q.packet;plan->late_count=0;
    // v0.25.4: the inputs' states at the slot are the barrier plan's: the
    // after-state of each one's last node up to the slot (the slot's own
    // included), or its tracker's initial state.
    Arena planner(0x80000);const auto saved_renderer=route::cfg.renderer;route::cfg.renderer=planner.base;
    const auto planned=a.take(16*5),taa=a.take(0x40),after_slot=a.take(0x40),other=a.take(0x40),other_backing=a.take(0x80);
    put(taa,6U);put(other,6U);put(after_slot,6U);put(other_backing+0x20,a.take(0x10));
    const std::array<std::uintptr_t,5> order{other,taa,copy,q.packet,after_slot};
    for(unsigned i=0;i<5;++i)put(planned+16*i+8,order[i]);
    const std::uint64_t epoch=s.epoch+1;put(planner.base+0x7af18,planned);put(planner.base+0x7af28,5U);put(planner.base+0x1f208,epoch);
    plan->temporal.object=taa;
    // Trackers (backing +0x70): epoch, initial state, used epoch, initialized, immediate epoch.
    std::array<std::uintptr_t,3> trackers{};const std::array<std::uint16_t,3> initial{8,0x100,0x100};
    for(unsigned i=0;i<3;++i){trackers[i]=a.take(0x40);put(trackers[i]+0x10,epoch);put(trackers[i]+0x18,initial[i]);put(trackers[i]+0x20,epoch);
     put(trackers[i]+0x28,static_cast<unsigned char>(1));put(trackers[i]+0x30,std::uint64_t{});put(input_backings[i]+0x70,trackers[i]);}
    const auto node=[&](std::uintptr_t next,std::uintptr_t backing,std::uint16_t after){const auto n=a.take(0x20);put(n,next);put(n+8,backing);put(n+0x1a,after);return n;};
    route::PlannerView v;std::array<unsigned,2> at{};
    require(route::planner_view(*plan,v)&&route::planner_find(v,std::array<std::uintptr_t,2>{taa,q.packet},at)&&at==std::array<unsigned,2>{1,3},"plan positions of TAA and the slot");
    const auto state_at=[&](unsigned index,unsigned role){const route::FullSlot &r=q;const tb::Owner &o=role==0?r.depth:role==1?r.gbuffer4:r.gbuffer5;
     unsigned value{};return route::planner_state(v,index,o,value)?value:UINT_MAX;};
    require(state_at(3,0)==0x80&&state_at(3,1)==4&&state_at(3,2)==4,"no node: the trackers' initial states");
    // GBuffer4 read from TAA (its own transition), the depth's next read state
    // hoisted between TAA and the slot, GBuffer5 into COPY_DEST at the slot, and
    // a later node that the slot must not see.
    put(taa+8,node(node(0,other_backing,0x400),input_backings[1],12));put(copy+8,node(0,input_backings[0],12));
    put(q.packet+8,node(node(0,input_backings[2],2048),other_backing,0x20));put(after_slot+8,node(0,input_backings[0],0x400));
    require(state_at(1,0)==0x80&&state_at(1,1)==0xc0&&state_at(0,1)==4,"TAA: GBuffer4 read, depth still initial");
    require(state_at(3,0)==0xc0&&state_at(3,1)==0xc0&&state_at(3,2)==0x400&&state_at(4,0)==8,"slot: hoisted depth read, GBuffer4 read, GBuffer5 COPY_DEST; later node unseen");
    // Refusals: a node naming the backing with another resource, an
    // immediate state change, another epoch, an unreadable nested copy.
    {const auto mixed=a.take(0x80);put(mixed+0x20,a.take(0x10));put(copy+8,node(0,mixed,12));put(mixed+0x20,input_resources[0]);
     require(state_at(3,0)==UINT_MAX,"backing and resource must agree");put(copy+8,node(0,input_backings[0],12));}
    put(trackers[0]+0x30,epoch);require(state_at(3,0)==UINT_MAX,"immediate state change refused");put(trackers[0]+0x30,std::uint64_t{});
    put(trackers[0]+0x10,s.epoch);require(state_at(3,0)==UINT_MAX,"tracker of another epoch refused");put(trackers[0]+0x10,epoch);
    put(copy,10U);require(state_at(3,0)==UINT_MAX,"unreadable nested copy refused");put(copy,13U);
    put(planner.base+0x1f208,s.epoch);require(!route::planner_view(*plan,v),"another planner epoch refused");
    for(const auto packet:{taa,copy,q.packet,after_slot})put(packet+8,std::uintptr_t{});route::cfg.renderer=saved_renderer;}
   // Refusals record no packet; the chain still switches.
   const auto refused=[&](std::uint64_t armed,unsigned reason,unsigned sub,unsigned grade_order,const char *why){
    const auto before=route::full_counts.slot_refusals[reason].load(),switches=route::full_counts.switches.load();
    put(grade+0x144,grade_order);arm(armed,sub);put(grade+0x144,0U);
    require(route::full_counts.slot_refusals[reason].load()==before+1&&route::full_counts.switches.load()==switches+1&&
            route::field(context,0x2b80,count)&&count==5,why);
    const auto plan=plan_for(armed);require(!plan->full_slot&&route::handoff_count(*plan)==3&&route::final_node(*plan)==2,"no slot in the plan");};
   // Without TAA's copy there is no slot source.
   {const auto before=route::full_counts.slot_refusals[6].load();put(queue,0x78000001U);arm(armed_slot+3,0x12U);put(queue,0x78000002U);
    require(route::full_counts.slot_refusals[6].load()==before+1&&route::field(context,0x2b80,count)&&count==5,"no TAA copy refuses the slot");}
   // A slot whose source is not the TAA output stays out of the plan.
   {arm(armed_slot+4,0x12U);auto &r=route::full_slots[(armed_slot+4)%route::full_slots.size()];require(r.valid,"slot recorded");
    const auto real=r.source.object;r.source.object=owners[1];const auto plan=plan_for(armed_slot+4);r.source.object=real;
    require(!plan->full_slot&&route::handoff_count(*plan)==3,"a slot on another source is not planned");}
   refused(armed_slot+1,5,0x12U,3U,"a filter at order 3 refuses the slot");
   refused(armed_slot+2,4,0x32U,0U,"an added order refuses the slot");
   put(context+0x2b80,4U);put(context+0x154,0U);put(context+0x15c,0U);put(view+0x620,std::uintptr_t{});put(view+0x628,std::uintptr_t{});route::full_slots={};
   std::ostringstream summary;route::full_post_write(summary);
   require(summary.str().find("\"inputs\":{\"ready\":1,\"failure\":\"\",\"bytes\":")!=std::string::npos,"inputs in the summary");
   std::cout<<"PASS full_post inputs: slot at (15, 3) in TAA's key, planned before the final copy, plan states, refusals\n";}
  put(renderer+0x1f210,s.frame);put(context+0x5c,std::array<unsigned,4>{0,0,128,64});put(layer,std::uintptr_t{});
  route::full_frame.store(UINT64_MAX);route::full_viewport=nullptr;route::full_copy=nullptr;route::full_finals={};
  std::cout<<"PASS full_post: admitted with TAA registered, armed after TAA's callback when published, the DLAA copy in the quad's key, "
             "full rectangle after TAA, restored at the exit, unplanned copies dropped, the scene capture widened, refusals\n";}
 check_queue_completion(s,filters,final_copy,4);
 route::cfg=saved;return rejected;
}
}
