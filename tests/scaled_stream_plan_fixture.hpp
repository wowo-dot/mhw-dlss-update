#pragma once
#include <thread>
// Bounded synthetic game structures with real COM textures and captured shaders.
// Exercises discovery before runtime admission; no game process is required.
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
unsigned check_plan(Fixture &f,std::span<const std::byte> haze_vs={},std::span<const std::byte> haze_ps={},std::span<const std::byte> overlay_ps={},std::span<const std::byte> overlay_vs={},const std::filesystem::path &dependency_dir={},const std::filesystem::path &dof_dir={}){
 namespace tb=mhw::temporal_bindings;Arena image(0x5200000),a(0x50000);const auto renderer=a.take(0x21000),context=renderer+0x710,view=a.take(0x1000),layer=a.take(0x200),motion=a.take(0x1000),grade=a.take(0x1000),filters=a.take(0x200);
 const auto saved=route::cfg;route::cfg={image.base,renderer,0,reinterpret_cast<std::uintptr_t>(f.device.Get())};route::width=128;route::height=64;
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
 const auto vertices=a.take(3*64);put(context+0x2b50,vertices);
 const auto depth=a.take(0x200),blend=a.take(0x300),raster=a.take(0x200),depth_pointer=a.take(8);
 put(image.base+0x51c73d0,renderer);put(renderer+0x2480,depth_pointer);put(depth_pointer,depth);
 put(depth+0x60,depth+0x100);put(blend+0x98,blend+0x100);put(raster+0x68,raster+0x100);
 std::array<std::uintptr_t,3> packets{},rectangles{},shader_desc{};
 for(unsigned i=0;i<3;++i){
  const auto v=sf::effect_vertices(static_cast<sf::EffectStage>(i),{64,32,128,64},false);
  const auto r=recipe(static_cast<sf::EffectStage>(i),{64,32,128,64},f.root.Get(),f.vs[i],f.ps[i],std::span(v.values).first(v.count*4));
  put(depth+0x110,r.depth);put(blend+0x110,r.blend);put(raster+0x110,r.raster);
  const auto packet=a.take(0x50),ia=a.take(0x50),graphics=a.take(0x30),render=a.take(0x30),handle=a.take(8),set=a.take(0x90),layout=a.take(0x20),elements=a.take(2*sizeof(D3D12_INPUT_ELEMENT_DESC)),rect=a.take(0x60);
  const auto view_owner=a.take(0x80),native_view=a.take(0x30);put(rect,view_owner);put(rect+0x48,1U);put(view_owner+0x60,native_view);put(native_view+0x18,reinterpret_cast<std::uintptr_t>(resources[i+2].Get()));put(native_view+0x20,26U);
  packets[i]=packet;rectangles[i]=rect;put(queue+16*(i+1),i==0?0x78000064U:i==1?0x7ffffff1U:0x80000000U);put(queue+16*(i+1)+8,packet);
  put(packet+0x10,rect);put(packet+0x18,ia);put(packet+0x20,graphics);put(packet+0x28,render);put(packet+0x40,v.count);put(ia+0x20,static_cast<unsigned char>(16));put(ia+0x24,64*i);
  put(rect+0x4c,std::array<int,4>{0,0,i==2?128:64,i==2?64:32});put(vertices+64*i,v.values);
  put(graphics+8,handle);put(handle,set);put(graphics+0x20,layout);put(layout+0x10,elements);put(layout+0x18,2U);std::memcpy(reinterpret_cast<void *>(elements),r.input_layout.data(),2*sizeof(D3D12_INPUT_ELEMENT_DESC));
  put(render+8,blend);put(render+0x10,depth);put(render+0x18,raster);
  for(unsigned j=0;j<2;++j){const auto entry=a.take(0x20),d=a.take(0x10);put(set+0x50+8*j,entry);put(entry+0x10,d);auto bytes=j?std::span(f.ps[i]):std::span(f.vs[i]);
   put(d,static_cast<unsigned>(bytes.size()));put(d+8,reinterpret_cast<std::uintptr_t>(bytes.data()));if(j)shader_desc[i]=d;}
 }
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
 if(!route::build_plan(p,motion)){
  for(unsigned i=0;i<3;++i){auto &r=p.draws[i];std::cerr<<"stage "<<i<<" geometry "<<route::geometry(r,context,i)<<" pin "<<route::pin(r.output,p.resources[i+2])<<" source "<<route::source(r)<<" pipeline "<<mhw::projection_binding::snapshot_native_pipeline(route::cfg.image,r.draw.packet,r.before.pipeline)<<" layout "<<r.before.pipeline.input_count<<"\n";}
  throw std::runtime_error("CPU graph admission "+std::to_string(route::plan_rejection));}
 for(unsigned i=0;i<3;++i){require(p.draws[i].draw.packet==packets[i]&&p.resources[i+2]==resources[i+2],"exact draw ownership");const auto r=route::recipe(p.draws[i],i,f.root.Get());sf::Effect proof(f.device.Get(),r,true);}
 unsigned rejected{};const auto bad=[&]{auto candidate=std::make_unique<route::Plan>();auto &q=*candidate;q.stamp=s;require(!route::build_plan(q,motion),"malformed native graph rejected");++rejected;};
 put(copy+0x38,std::array<unsigned,4>{0,0,63,32});bad();put(copy+0x38,s.rectangle);
 put(queue+16,0x78000002U);bad();put(queue+16,0x78000064U);
 put(grade+0x248,owners[0]);bad();put(grade+0x248,owners[2]);
 put(grade+0x258,static_cast<unsigned char>(0));bad();put(grade+0x258,static_cast<unsigned char>(1));
 put(vertices+64,2.F);bad();put(vertices+64,-1.F);
 put(rectangles[2]+0x54,64);bad();put(rectangles[2]+0x54,128);
 put(shader_desc[0],20000U);bad();put(shader_desc[0],static_cast<unsigned>(f.ps[0].size()));
 put(renderer+0x1f210,s.frame+1);bad();put(renderer+0x1f210,s.frame);
 put(context+0x2b80,65535U);bad();put(context+0x2b80,4U);
 auto &final=route::final_quads[final_slot];const auto captured=final;
 final.valid=false;bad();final=captured;
 ++final.stamp.frame;bad();final=captured;
 final.packet=packets[0];bad();final=captured;
 put(attachment+0x58,owners[0]);bad();put(attachment+0x58,owners[4]);
 put(renderer+0x93,static_cast<unsigned char>(0));bad();put(renderer+0x93,static_cast<unsigned char>(1));
 require(route::build_plan(p,motion),"restored native graph");
 check_queue_completion(s,filters,motion,4);
 if(!dependency_dir.empty()){
 #include "scaled_dependency_plan_fixture.inl"
 }
 if(!dof_dir.empty()){
 #include "scaled_dof_plan_fixture.inl"
 }
 if(!overlay_ps.empty()){
  const auto overlay=a.take(0x200),packet=a.take(0x50),graphics=a.take(0x100),handle=a.take(8),set=a.take(0x90),entry=a.take(0x20),descriptor=a.take(0x10);
  std::uintptr_t original_graphics{},original_set{},original_handle{};
  require(route::field(packets[0],0x20,original_graphics)&&route::field(original_graphics,8,original_handle)&&route::field(original_handle,0,original_set),"overlay original structures");
  std::memcpy(reinterpret_cast<void *>(packet),reinterpret_cast<void *>(packets[0]),0x50);
  std::memcpy(reinterpret_cast<void *>(graphics),reinterpret_cast<void *>(original_graphics),0x30);
  std::memcpy(reinterpret_cast<void *>(set),reinterpret_cast<void *>(original_set),0x90);
  put(packet+0x20,graphics);put(graphics+8,handle);put(handle,set);put(set+0x58,entry);put(entry+0x10,descriptor);
  put(descriptor,static_cast<unsigned>(overlay_ps.size()));put(descriptor+8,reinterpret_cast<std::uintptr_t>(overlay_ps.data()));
  const auto vs_entry=a.take(0x20),vs_descriptor=a.take(0x10);put(set+0x50,vs_entry);put(vs_entry+0x10,vs_descriptor);put(vs_descriptor,static_cast<unsigned>(overlay_vs.size()));put(vs_descriptor+8,reinterpret_cast<std::uintptr_t>(overlay_vs.data()));
  put(overlay,image.base+0x3456780);put(overlay+0x140,15U);put(overlay+0x144,1024U);put(overlay+0x148,overlay);put(overlay+0x158,owners[2]);put(overlay+0x160,std::array<unsigned char,3>{0,1,0});
  put(filters+0x3c,5U);put(filters+0x60,overlay+0x140);put(context+0x2b80,5U);put(queue+64,0x78000400U);put(queue+72,packet);
  route::Record overlay_record;
  const auto accept=[&]{auto candidate=std::make_unique<route::Plan>();auto &q=*candidate;q.stamp=s;if(!route::build_plan(q,motion)){std::cerr<<"overlay graph reason="<<route::plan_rejection<<" count="<<q.overlay_count<<" geometry="<<route::geometry(q.draws[4],context,4)<<" inputs="<<route::overlay_inputs(q,q.draws[4])<<"\n";throw std::runtime_error("generic overlay graph admitted");}require(q.overlay_count==1&&q.order[0]==0&&q.order[1]==4&&q.order[2]==1&&q.order[3]==2&&q.targets[4]==2,"overlay ordered between native producer and consumer");overlay_record=q.draws[4];};
  accept();const auto reject_overlay=[&]{auto candidate=std::make_unique<route::Plan>();auto &q=*candidate;q.stamp=s;require(!route::build_plan(q,motion)&&route::plan_rejection==17,"unqualified overlay rejected");};
  put(set+0x88,65U);reject_overlay();require(route::plan_qualification==((4U<<16)|11U),"overlay input rejection identified");put(set+0x88,0U);
  std::uintptr_t overlay_original_ia{};require(route::field(packet,0x18,overlay_original_ia),"overlay IA identity");
  const auto overlay_ia=a.take(0x30);std::memcpy(reinterpret_cast<void *>(overlay_ia),reinterpret_cast<void *>(overlay_original_ia),0x30);put(packet+0x18,overlay_ia);
  put(overlay_ia+0x20,static_cast<unsigned char>(8));reject_overlay();require(route::plan_qualification==((4U<<16)|3U),"overlay vertex rejection identified");
  require(route::plan_vertex_gate==std::array<std::uint64_t,10>{packet,overlay_ia,context,4,0,1,8,0,0,2},"stride receipt records only successful read");
  put(packet+0x18,std::uintptr_t{});reject_overlay();require(route::plan_vertex_gate[5]==0&&route::plan_vertex_gate[9]==1,"unreadable IA is not a zero stride");put(packet+0x18,overlay_ia);
  unsigned saved_offset{};require(route::field(overlay_ia,0x24,saved_offset),"fixture vertex offset");
  put(overlay_ia+0x20,static_cast<unsigned char>(16));put(overlay_ia+0x24,16777217U);reject_overlay();
  require(route::plan_vertex_gate[5]==3&&route::plan_vertex_gate[7]==16777217&&route::plan_vertex_gate[8]==0&&route::plan_vertex_gate[9]==4,"offset bound stops before arena read");put(overlay_ia+0x24,saved_offset);
  put(context+0x2b50,std::uintptr_t{});reject_overlay();require(route::plan_vertex_gate[5]==7&&route::plan_vertex_gate[9]==6,"zero arena is a successful read");
  put(context+0x2b50,UINTPTR_MAX-33554431);reject_overlay();require(route::plan_vertex_gate[9]==7,"arena overflow rejected without vertex read");put(context+0x2b50,vertices);
  put(overlay_ia+0x20,static_cast<unsigned char>(16));accept();require(route::plan_qualification==0,"accepted geometry clears rejection detail");
  require(route::plan_vertex_gate==std::array<std::uint64_t,10>{},"accepted geometry clears operand receipt");
  // Exercise real graph discovery with padded/reordered attributes, a byte
  // offset and nonzero first vertex. Other stages keep their packed vertices.
  std::uintptr_t original_layout{};require(route::field(graphics,0x20,original_layout),"original overlay layout");
  const auto padded=a.take(6*28),padded_layout=a.take(0x20),padded_elements=a.take(3*sizeof(D3D12_INPUT_ELEMENT_DESC));
  const std::array<D3D12_INPUT_ELEMENT_DESC,3> attributes{{
   {"TexCoord",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
   {"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
   {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,16,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}}};
  put(padded_elements,attributes);put(padded_layout+0x10,padded_elements);put(padded_layout+0x18,3U);put(graphics+0x20,padded_layout);
  const auto canonical=overlay_record.vertices;
  for(unsigned i=0;i<4;++i){put(padded+(i+2)*28,std::array<float,2>{canonical[4*i+2],canonical[4*i+3]});put(padded+(i+2)*28+16,std::array<float,2>{canonical[4*i],canonical[4*i+1]});}
  put(overlay_ia+0x20,static_cast<unsigned char>(28));put(overlay_ia+0x24,static_cast<unsigned>(padded-vertices));put(packet+0x44,2U);accept();
  require(overlay_record.vertices==canonical&&overlay_record.overlay_layout.position==16&&overlay_record.overlay_layout.uv==0,"graph uses actual attribute offsets");
  put(padded+2*28+16,0.F);reject_overlay();require((route::plan_qualification&65535)==5,"padded partial quad rejected");put(padded+2*28+16,-1.F);
  put(overlay_ia+0x20,static_cast<unsigned char>(255));put(packet+0x44,1048576U);reject_overlay();require((route::plan_qualification&65535)==4,"expanded stride cannot escape bounded arena");
  put(overlay_ia+0x20,static_cast<unsigned char>(28));put(packet+0x44,2U);accept();
  // Native GPU view must agree with the very same qualified stride/offset and
  // contain every drawn vertex. Retain the actual native buffer on success.
  const auto vb_bytes=overlay_record.vertex_offset+6*28;
  auto native_vb=buffer(f.device.Get(),vb_bytes,D3D12_HEAP_TYPE_UPLOAD);
  const auto vb_owner=a.take(0x80),vb_backend=a.take(0x50);put(overlay_ia,vb_owner);put(vb_owner+0x60,vb_backend);
  put(vb_backend+0x20,reinterpret_cast<std::uintptr_t>(native_vb.Get()));put(vb_backend+0x40,native_vb->GetGPUVirtualAddress());put(vb_backend+0x10,vb_bytes);
  D3D12_VERTEX_BUFFER_VIEW vb_view{native_vb->GetGPUVirtualAddress()+overlay_record.vertex_offset,6*28,28};
  ComPtr<ID3D12Resource> held;held.Attach(route::vertex_resource(overlay_record,vb_view));require(held==native_vb,"padded native GPU buffer retained");held.Reset();
  auto bad_view=vb_view;bad_view.StrideInBytes=16;require(!route::vertex_resource(overlay_record,bad_view),"GPU stride mismatch rejected");
  put(overlay_ia+0x24,overlay_record.vertex_offset+4);require(!route::vertex_resource(overlay_record,vb_view),"changed byte offset rejected");put(overlay_ia+0x24,overlay_record.vertex_offset);
  ++overlay_record.arguments[1];require(!route::vertex_resource(overlay_record,vb_view),"last vertex beyond native buffer rejected");--overlay_record.arguments[1];
  put(graphics+0x20,original_layout);put(overlay_ia+0x20,static_cast<unsigned char>(16));put(overlay_ia+0x24,saved_offset);put(packet+0x44,0U);accept();
  put(packet,13U);reject_overlay();
  const auto &opcode_receipt=route::diagnostics.commands.sample(route::diagnostics.commands.size()-1);
  require(opcode_receipt.gate[0]==mhw::scaled_temporal::OutputCommands::unsupported_opcode&&opcode_receipt.gate[4]==13&&
   opcode_receipt.stable&&opcode_receipt.matched==1&&opcode_receipt.opcodes[13]==1&&opcode_receipt.has_focus,"actual rejected opcode captured before filtering");
  put(packet,0U);
  put(context+0x2b80,6U);put(queue+80,0x78000400U);put(queue+88,packet);reject_overlay();
  const auto &duplicate_receipt=route::diagnostics.commands.sample(route::diagnostics.commands.size()-1);
  require(duplicate_receipt.gate[0]==mhw::scaled_temporal::OutputCommands::duplicate_packet&&duplicate_receipt.gate[3]==5&&
   duplicate_receipt.gate[6]==1&&duplicate_receipt.stable&&duplicate_receipt.matched==2&&duplicate_receipt.opcodes[0]==2,"duplicate draw count and exact rejection recorded");
  const auto state_packet=a.take(0x50);put(state_packet,11U);put(queue+88,state_packet);accept();
  // Reproduce 11,0,11,0 under one key. The higher packet address comes first;
  // native stable-key order, not address or first-vertex order, must win.
  const auto second_packet=a.take(0x50),second_ia=a.take(0x30),multi_vertices=a.take(16*16);
  std::memcpy(reinterpret_cast<void *>(second_packet),reinterpret_cast<void *>(packet),0x50);
  std::memcpy(reinterpret_cast<void *>(second_ia),reinterpret_cast<void *>(overlay_ia),0x30);
  put(multi_vertices+12*16,canonical);put(second_ia+0x24,static_cast<unsigned>(multi_vertices-vertices));
  put(second_packet+0x18,second_ia);put(second_packet+0x44,12U);
  const auto queue_entry=[&](unsigned index,std::uintptr_t value){put(queue+16*index,0x78000400U);put(queue+16*index+8,value);};
  queue_entry(4,state_packet);queue_entry(5,second_packet);queue_entry(6,state_packet);queue_entry(7,packet);put(context+0x2b80,8U);
  const auto accept_multi=[&](unsigned expected){auto q=std::make_unique<route::Plan>();q->stamp=s;
   require(route::build_plan(*q,motion)&&q->overlay_count==expected,"multiple qualified draws share a descriptor");
   require(q->order[0]==0&&q->order[expected+1]==1&&q->order[expected+2]==2,"multiple overlays retain producer and consumer order");
   for(unsigned i=0;i<expected;++i)require(q->order[1+i]==4+i&&q->targets[4+i]==2,"equal-key draws retain queue order and target");
   return q;};
  auto multi=accept_multi(2);
  require(multi->draws[4].draw.packet==second_packet&&multi->draws[5].draw.packet==packet&&
   multi->draws[4].arguments[1]==12&&multi->draws[5].arguments[1]==0,"distinct native draw arguments retained in insertion order");
  put(packet+0x40,3U);reject_overlay();require((route::plan_qualification>>16)==5,"second draw qualifies independently");put(packet+0x40,4U);
  put(packet,13U);reject_overlay();put(packet,0U);accept_multi(2);
  // All eight bounded draw slots can belong to one descriptor; the ninth
  // still refuses the whole frame before preparation or any GPU mutation.
  for(unsigned i=0;i<9;++i){const auto clone=a.take(0x50);std::memcpy(reinterpret_cast<void *>(clone),reinterpret_cast<void *>(packet),0x50);queue_entry(4+i,clone);}
  put(context+0x2b80,12U);accept_multi(8);
  put(context+0x2b80,13U);reject_overlay();
  const auto &capacity_receipt=route::diagnostics.commands.sample(route::diagnostics.commands.size()-1);
  require(capacity_receipt.gate[0]==mhw::scaled_temporal::OutputCommands::draw_capacity&&capacity_receipt.gate[7]==8,"ninth overlay draw is bounded");
  queue_entry(4,packet);queue_entry(5,state_packet);put(context+0x2b80,6U);
  put(queue+72,std::uintptr_t{1});auto unread_candidate=std::make_unique<route::Plan>();unread_candidate->stamp=s;
  require(!route::build_plan(*unread_candidate,motion)&&route::plan_rejection==10,"unreadable packet keeps original rejection");
  const auto &unread_receipt=route::diagnostics.commands.sample(route::diagnostics.commands.size()-1);
  require(unread_receipt.gate[0]==mhw::scaled_temporal::OutputCommands::packet_read&&unread_receipt.unreadable==1&&unread_receipt.opcodes[11]==1,"unreadable opcode still captures remaining commands");
  put(queue+72,packet);put(context+0x2b80,5U);accept();
  const auto words=a.take(4);put(set,words);put(set+0x14,std::array<unsigned short,2>{1,0});put(set+0x88,1U);put(words,2U);put(graphics+0x30,owners[2]);reject_overlay();
  put(set+0x14,std::array<unsigned short,2>{0,0});put(set+0x88,0U);accept();
  put(filters+0x3c,4U);put(context+0x2b80,4U);
 }
 if(!haze_vs.empty()){
  // The capture is observational: feed a synthetic native queue recipe while
  // existing route admission remains unchanged. No game process is opened.
  const auto haze=a.take(0x200),graphics=a.take(0x230),handle=a.take(8),set=a.take(0x90),ids=a.take(32),modes=a.take(8),words=a.take(64);
  std::uintptr_t old_graphics{},old_handle{},old_set{};
  require(route::field(packets[0],0x20,old_graphics)&&route::field(old_graphics,8,old_handle)&&route::field(old_handle,0,old_set),"fixture original graphics");
  std::memcpy(reinterpret_cast<void *>(graphics),reinterpret_cast<void *>(old_graphics),0x30);
  std::memcpy(reinterpret_cast<void *>(set),reinterpret_cast<void *>(old_set),0x90);
  put(packets[0]+0x20,graphics);put(graphics+8,handle);put(handle,set);put(graphics+0x10,modes);
  // Real v1.25.8 failure: non-range metadata includes 0xff00/0xffff.
  // These must be preserved, never traversed as extra binding ranges.
  put(set,words);put(set+8,std::array<unsigned short,6>{0,0,0,65280,65535,1});
  put(set+0x14,std::array<unsigned short,6>{4,0,4,4,255,0});put(set+0x80,ids);put(set+0x88,4U);
  const std::array<std::uintptr_t,4> objects{owners[1],owners[0],owners[3],owners[4]};
  for(unsigned i=0;i<4;++i){put(ids+4*i,i+10);put(modes+i,static_cast<unsigned char>(1));put(graphics+0x30+8*i,objects[i]);put(words+4*i,2U+(i<<4)+(i<<24));
   const auto srv=a.take(0x20);put(objects[i]+0x78,srv);put(srv+0x10,std::uintptr_t{100+i});
   put(image.base+route::haze_ids[i],i+10);put(context+0x710+8*(i+10),objects[i]);}
  put(image.base+route::haze_ids[4],UINT_MAX);
  route::HazeBindings bindings{};
  require(route::haze_bindings(set,graphics,bindings),"real sentinel metadata accepted");
  require(bindings.metadata[0]==std::array<unsigned char,8>{0,0,0,255,255,255,1,0},"opaque VS metadata retained");
  require(bindings.metadata[1]==std::array<unsigned char,8>{4,0,4,0,255,0,0,0},"opaque PS metadata retained");
  put(set+10,static_cast<unsigned short>(65535));
  require(route::haze_bindings(set,graphics,bindings),"empty stage does not dereference unused start");put(set+10,static_cast<unsigned short>(0));
  for(unsigned i=0;i<4;++i)put(words+4*(i+4),2U+(i<<4)+(i<<24));
  put(set+0x16,static_cast<unsigned short>(4));
  require(route::haze_bindings(set,graphics,bindings)&&bindings.words[1][3]==(2U+(3U<<4)+(3U<<24)),"nonzero range start");put(set+0x16,static_cast<unsigned short>(0));
  put(set+0x14,static_cast<unsigned short>(129));require(!route::haze_bindings(set,graphics,bindings),"oversized true range rejected");put(set+0x14,static_cast<unsigned short>(4));
  put(set+0x16,static_cast<unsigned short>(4093));require(!route::haze_bindings(set,graphics,bindings),"true range end rejected");put(set+0x16,static_cast<unsigned short>(0));
  for(unsigned i=0;i<2;++i){const auto entry=a.take(0x20),d=a.take(16);put(set+0x50+8*i,entry);put(entry+0x10,d);
   const auto bytes=i?haze_ps:haze_vs;put(d,static_cast<unsigned>(bytes.size()));put(d+8,reinterpret_cast<std::uintptr_t>(bytes.data()));}
  put(haze,image.base+0x35372b0);put(haze+0x140,15U);put(haze+0x144,20000U);put(haze+0x148,haze);put(haze+0x150,owners[1]);put(haze+0x158,owners[2]);
  put(filters+0x3c,5U);put(filters+0x60,haze+0x140);put(queue+16,0x78004e20U);
  frame.caller=image.base+0x24c7ff9;frame.gpr[1]=renderer;
  unsigned rejection_cases{};
  const auto next=[&]{s.frame+=60;s.epoch=s.frame;put(renderer+0x1f210,s.frame);put(renderer+0x1f208,s.epoch);put(context+0x2b80,1U);};
  const auto rejected_at=[&](unsigned why){require(route::haze_capture.reason==why&&!route::haze_capture.entered,"haze rejection phase");++rejection_cases;};
  const auto invoke=[&]{require(route::haze_before(frame),"haze callback entry");put(context+0x2b80,2U);};
  frame.caller++;require(!route::haze_before(frame)&&!route::haze_capture.attempts,"foreign caller ignored");frame.caller--;
  next();put(renderer+0x1f450,std::uintptr_t{});require(!route::haze_before(frame),"foreign view rejected");rejected_at(2);put(renderer+0x1f450,view);
  next();put(filters+0x3c,6U);put(filters+0x68,haze+0x140);require(!route::haze_before(frame),"duplicate haze rejected");rejected_at(3);put(filters+0x3c,5U);
  next();put(context+0x710+80,owners[0]);require(!route::haze_before(frame),"context base mismatch rejected");rejected_at(4);put(context+0x710+80,owners[1]);
  next();invoke();route::haze_after(true,true);require(route::haze_capture.count==1,"first haze recipe captured");
  next();invoke();put(renderer+0x1f210,s.frame+1);route::haze_after(true,true);rejected_at(5);
  next();invoke();put(context+0x2b80,3U);route::haze_after(true,true);rejected_at(5);
  std::uintptr_t ia{};require(route::field(packets[0],0x18,ia),"fixture IA");
  next();invoke();put(ia+0x20,static_cast<unsigned char>(8));route::haze_after(true,true);rejected_at(6);put(ia+0x20,static_cast<unsigned char>(16));
  std::uintptr_t ps_entry{},ps_desc{};require(route::field(set,0x58,ps_entry)&&route::field(ps_entry,0x10,ps_desc),"PS metadata");
  next();invoke();put(ps_desc,static_cast<unsigned>(haze_ps.size()-1));route::haze_after(true,true);rejected_at(7);put(ps_desc,static_cast<unsigned>(haze_ps.size()));
  next();invoke();put(words,2U+(64U<<4));route::haze_after(true,true);rejected_at(8);put(words,2U);
  next();invoke();put(graphics+0x30,owners[0]);route::haze_after(true,true);rejected_at(9);put(graphics+0x30,owners[1]);
  next();invoke();route::haze_after(true,false);rejected_at(5);
  next();invoke();route::haze_after(true,true);require(route::haze_capture.count==2,"second haze recipe captured");
  next();invoke();route::haze_after(true,true);require(route::haze_capture.count==3,"third haze recipe captured");
  next();require(!route::haze_before(frame)&&route::haze_capture.attempts==13,"capture stops at capacity");
  require(rejection_cases==10&&route::healthy(),"diagnostic rejections do not change rendering health");
  require(route::haze_capture.samples[0].bindings.textures[0].object==owners[1],"recorded source binding");
  std::cout<<"PASS haze capture: 3 accepted recipes, 10 malformed inputs, foreign caller and capacity bounds\n";
#include "scaled_haze_plan_fixture.inl"
 }
 route::cfg=saved;return rejected;
}
}
