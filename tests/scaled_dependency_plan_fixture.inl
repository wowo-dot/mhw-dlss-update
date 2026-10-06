// Runs inside check_plan with its synthetic native queue and real device.
const auto copy_vs=read(dependency_dir/L"730b7d23bcdca40477ed973c29c8fff8de7ab7244b05d142d5f9458fbfec5fdd.vs.dxbc");
const auto copy_ps=read(dependency_dir/L"be6127ba48008790fd0e93558b8248cb8acd7c0577899bf50d28e5618ae37b79.ps.dxbc");
const auto radial_vs=read(dependency_dir/L"b980e376575d82610809066f45c6ecd6efb090bbee76968d9ee5203d2459a1d2.vs.dxbc");
const auto radial_ps=read(dependency_dir/L"90f3f4d5594357e5350acf3d5bda57db4641b50e6aaa5787d3ae341463860741.ps.dxbc");
std::array<ComPtr<ID3D12Resource>,3> extra;
std::array<std::uintptr_t,3> extra_owners{};
for(unsigned i=0;i<3;++i){auto d=texture(i==1?1:128,i==0?64:i==1?1:128,i?DXGI_FORMAT_R8G8B8A8_UNORM:DXGI_FORMAT_R10G10B10A2_UNORM);if(!i)d.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
 extra[i]=resource(f.device.Get(),d,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COMMON);
 const auto o=a.take(0x100),backend=a.take(0x40);extra_owners[i]=o;put(o,std::uintptr_t{1});put(o+0x60,backend);put(backend+0x20,reinterpret_cast<std::uintptr_t>(extra[i].Get()));
 put(o+0x88,static_cast<unsigned>(d.Width)+(d.Height<<16));put(o+0x90,i==0?6U:i==1?7U:23U);
}
for(auto o:{owners[2],extra_owners[0],extra_owners[1],extra_owners[2]}){const auto v=a.take(0x20);put(o+0x78,v);put(v+0x10,v);}
const auto effect=a.take(0x200);put(effect+0x140,15U);put(effect+0x144,4100U);put(effect+0x148,effect);put(effect+0x158,owners[2]);put(effect+0x160,std::array<unsigned char,3>{0,1,0});put(filters+0x60,effect+0x140);put(filters+0x3c,5U);
const std::array<D3D12_INPUT_ELEMENT_DESC,4> attributes{{
 {"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
 {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
 {"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,16,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
 {"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,24,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}}};
std::array<std::uintptr_t,2> dependency_packets{},graphics_objects{},mode_arrays{},vertex_starts{};
for(unsigned i=0;i<2;++i){
 const auto packet=a.take(0x50),graphics=a.take(0x90),handle=a.take(8),set=a.take(0x90),ia=a.take(0x30),layout=a.take(0x20),elements=a.take(sizeof(attributes)),rect=a.take(0x60),vo=a.take(0x80),nv=a.take(0x30);
 dependency_packets[i]=packet;graphics_objects[i]=graphics;
 std::memcpy(reinterpret_cast<void *>(packet),reinterpret_cast<void *>(packets[0]),0x50);
 put(packet+0x10,rect);put(packet+0x18,ia);put(packet+0x20,graphics);put(graphics+8,handle);put(handle,set);put(graphics+0x20,layout);put(layout+0x10,elements);put(layout+0x18,i?4U:2U);put(elements,attributes);
 put(rect,vo);put(vo+0x60,nv);put(nv+0x18,reinterpret_cast<std::uintptr_t>(i?resources[2].Get():extra[0].Get()));put(nv+0x20,i?26U:24U);put(rect+0x48,1U);put(rect+0x4c,std::array<int,4>{0,0,64,32});
 for(unsigned j=0;j<2;++j){const auto entry=a.take(0x20),d=a.take(0x10);const auto &bytes=i?(j?radial_ps:radial_vs):(j?copy_ps:copy_vs);
  put(set+0x50+8*j,entry);put(entry+0x10,d);put(d,static_cast<unsigned>(bytes.size()));put(d+8,reinterpret_cast<std::uintptr_t>(bytes.data()));}
 const auto raw=a.take(i?112:256);vertex_starts[i]=raw;put(ia+0x20,static_cast<unsigned char>(i?28:16));put(ia+0x24,static_cast<unsigned>(raw-vertices));put(packet+0x44,i?0U:12U);
 const auto native=sf::effect_vertices(sf::EffectStage::scene_copy,{64,32,128,64},false);
 if(i){const auto v=sf::dependency_vertices(sf::FilterGeometry{64,32,128,64},false);put(raw,v);}else put(raw+192,native.values);
 const auto ids=a.take(64),modes=a.take(16),words=a.take(64);mode_arrays[i]=modes;
 put(set,words);put(set+0x80,ids);put(set+0x88,i?10U:3U);put(set+0x14,std::array<unsigned short,2>{static_cast<unsigned short>(i?10:3),0});put(graphics+0x10,modes);
 for(unsigned n=0;n<(i?10U:3U);++n){put(ids+4*n,n+1);put(graphics+0x30+8*n,set);}
 if(i){put(words,std::array<unsigned,10>{663553,17481745,37736481,790578,17985602,35754066,16826464,54517872,69357696,88076432});for(unsigned j=0;j<3;++j)put(graphics+0x48+8*j,extra_owners[j]);}
 else{put(words,std::array<unsigned,3>{1921025,790546,52301856});put(graphics+0x38,owners[2]);}
 put(queue+64+16*i,0x78001004U);put(queue+72+16*i,packet);
}
put(context+0x2b80,6U);
const auto accept_dependency=[&](unsigned extras=3){auto q=std::make_unique<route::Plan>();q->stamp=s;
 if(!route::build_plan(*q,motion))throw std::runtime_error("dependency graph "+std::to_string(route::plan_rejection)+":"+std::to_string(route::plan_qualification)+":"+std::to_string(route::dependency_rejection));
 require(q->overlay_count==2&&q->draws[4].dependency==1&&q->draws[5].dependency==2&&q->extra_resources==extras,"dependency classification and pin set");
 require(q->draws[4].output.resource==reinterpret_cast<std::uintptr_t>(extra[0].Get())&&q->draws[5].output.resource==reinterpret_cast<std::uintptr_t>(resources[2].Get()),"actual RTV, not advertised descriptor");
 require(q->draws[4].reads[0]==2&&q->draws[5].reads[0]==q->targets[4]&&q->targets[5]==2,"scene to temporary to scene edge");
 for(unsigned i=4;i<6;++i){auto r=route::recipe(q->draws[i],route::recipe_stage(q->draws[i],i),nullptr);require(sf::dependency_geometry(r),"native packed vertex contract");}
 return q;
};
auto qualified=accept_dependency();unsigned dependency_negatives{};
#include "radial_native_contract_fixture.inl"
const auto reject_dependency=[&]{auto q=std::make_unique<route::Plan>();q->stamp=s;require(!route::build_plan(*q,motion),"unsafe dependency rejected");++dependency_negatives;};
put(queue+72,dependency_packets[1]);put(queue+88,dependency_packets[0]);reject_dependency();put(queue+72,dependency_packets[0]);put(queue+88,dependency_packets[1]);
put(mode_arrays[1]+3,static_cast<unsigned char>(1));reject_dependency();put(mode_arrays[1]+3,static_cast<unsigned char>(0));
put(graphics_objects[1]+0x50,extra_owners[0]);reject_dependency();put(graphics_objects[1]+0x50,extra_owners[1]);
put(extra_owners[0]+0x88,64U+(32U<<16));reject_dependency();put(extra_owners[0]+0x88,128U+(64U<<16));
put(vertex_starts[1]+16,.5F);reject_dependency();put(vertex_starts[1]+16,0.F);
put(vertex_starts[0]+192+8,.5F);reject_dependency();put(vertex_starts[0]+192+8,0.F);
require(route::dependency_bindings_stable(*qualified,5),"stable consumer owners");
put(graphics_objects[1]+0x48,owners[2]);require(!route::dependency_bindings_stable(*qualified,5),"changed owner rejected before draw");reject_dependency();put(graphics_objects[1]+0x48,extra_owners[0]);
accept_dependency();require(dependency_negatives==7,"dependency rejection coverage");
// Exercise the production reader/comparison without weakening its admission.
route::dependency_binding_trace={};
auto &radial=qualified->draws[5];radial.binding_entry=route::dependency_binding_check(*qualified,5);
require(radial.binding_entry.accepted(),"binding check entry baseline");
const auto expect_difference=[&](unsigned field,unsigned index){
 const auto c=route::dependency_binding_check(*qualified,5);
 require(c.readable&&!c.equal&&!c.accepted()&&c.differences==1&&c.stored==1&&c.changes[0][0]==field&&c.changes[0][1]==index,"exact production binding difference");
};
const auto mask_owner=extra_owners[2],mask_view=qualified->dependency_bindings[1].views[2],descriptor=qualified->dependency_bindings[1].descriptors[2];
put(mask_view+0x10,descriptor+32);expect_difference(9,2);
require(!route::dependency_bindings_stable(*qualified,5),"descriptor change still rejected");
require(route::dependency_binding_trace.samples[0].entry.accepted()&&!route::dependency_binding_trace.samples[0].draw.accepted(),"entry versus API boundary receipt");
put(mask_view+0x10,descriptor);
put(mask_owner+0x70,2U);expect_difference(7,28);put(mask_owner+0x70,0U);
const auto saved_object=qualified->dependency_bindings[1].objects[6];put(graphics_objects[1]+0x60,saved_object+8);expect_difference(3,6);put(graphics_objects[1]+0x60,saved_object);
put(mode_arrays[1]+3,static_cast<unsigned char>(1));expect_difference(2,3);
require(!route::dependency_binding_check(*qualified,5).contract,"mode contract rejection distinguished");put(mode_arrays[1]+3,static_cast<unsigned char>(0));
put(graphics_objects[1]+0x10,std::uintptr_t{0});const auto unreadable=route::dependency_binding_check(*qualified,5);
require(!unreadable.readable&&unreadable.read.phase==3&&unreadable.read.index==0&&!unreadable.accepted(),"unreadable mode stage preserved");put(graphics_objects[1]+0x10,mode_arrays[1]);
require(route::dependency_binding_check(*qualified,5).accepted(),"restored bindings accepted");route::dependency_binding_trace={};
std::cout<<"PASS exact descriptor, owner flags, constant object, mode and unreadable binding diagnostics\n";
#include "scaled_dependency_record_fixture.inl"
#include "dependency_refresh_fixture.inl"
std::cout<<"PASS dependency discovery, packed vertices, 7 negative cases, live owner recheck\n";
put(filters+0x3c,4U);put(context+0x2b80,4U);
