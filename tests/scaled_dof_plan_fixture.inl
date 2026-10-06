// Included in the native-queue fixture. Real COM resources, synthetic owners,
// and caller-supplied captured shaders; no game process is accessed.
const auto dv=read(dof_dir/L"730b7d23bcdca40477ed973c29c8fff8de7ab7244b05d142d5f9458fbfec5fdd.vs.dxbc");
const auto dp=read(dof_dir/L"9f04832cb0a0ff800169964919f7ebecba353e7e3c147c2b314e76af7372e3aa.ps.dxbc");
std::array<ComPtr<ID3D12Resource>,5> extra;
std::array<std::uintptr_t,5> extra_owners{};
for(unsigned i=0;i<5;++i){
 auto d=texture(i==2?8:i==4?128:64,i==2?4:i==4?64:32,i==0?DXGI_FORMAT_R16G16B16A16_FLOAT:i==3?DXGI_FORMAT_R8_UNORM:DXGI_FORMAT_R11G11B10_FLOAT);
 if(i==4)d.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
 extra[i]=resource(f.device.Get(),d,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COMMON);
 const auto o=a.take(0x100),backend=a.take(0x40),srv=a.take(0x20);extra_owners[i]=o;
 put(o,std::uintptr_t{1});put(o+0x60,backend);put(backend+0x20,reinterpret_cast<std::uintptr_t>(extra[i].Get()));
 put(o+0x88,static_cast<unsigned>(d.Width)+(d.Height<<16));put(o+0x90,i==0?2U:i==3?21U:34U);put(o+0x94,UINT_MAX);
 put(o+0x78,srv);put(srv+0x10,srv);
}
const auto scene_srv=a.take(0x20);put(owners[2]+0x78,scene_srv);put(scene_srv+0x10,scene_srv);
const auto unit=a.take(0x200),entry=unit+0x140;put(unit,image.base+0x3526650);put(entry,15U);put(entry+4,26005U);put(entry+8,unit);put(entry+0x10,owners[2]);put(entry+0x18,extra_owners[4]);
put(grade+0x248,extra_owners[4]);put(filters+0x60,entry);put(filters+0x3c,5U);
const auto packet=a.take(0x50),graphics=a.take(0x90),handle=a.take(8),set=a.take(0x90),ia=a.take(0x30),layout=a.take(0x20),rect=a.take(0x60),vo=a.take(0x80),nv=a.take(0x30);
std::memcpy(reinterpret_cast<void *>(packet),reinterpret_cast<void *>(packets[0]),0x50);
put(packet+0x10,rect);put(packet+0x18,ia);put(packet+0x20,graphics);put(graphics+8,handle);put(handle,set);put(graphics+0x20,layout);
const std::array<D3D12_INPUT_ELEMENT_DESC,4> attributes{{{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,16,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,24,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}}};
const auto elements=a.take(sizeof(attributes));put(elements,attributes);put(layout+0x10,elements);put(layout+0x18,4U);
put(rect,vo);put(vo+0x60,nv);put(nv+0x18,reinterpret_cast<std::uintptr_t>(extra[4].Get()));put(nv+0x20,26U);put(rect+0x48,1U);put(rect+0x4c,std::array<int,4>{0,0,64,32});
for(unsigned j=0;j<2;++j){const auto se=a.take(0x20),d=a.take(0x10);const auto &bytes=j?dp:dv;
 put(set+0x50+8*j,se);put(se+0x10,d);put(d,static_cast<unsigned>(bytes.size()));put(d+8,reinterpret_cast<std::uintptr_t>(bytes.data()));}
const auto raw=a.take(112);const auto v=sf::dependency_vertices({64,32,128,64},false);put(raw,v);put(ia+0x20,static_cast<unsigned char>(28));put(ia+0x24,static_cast<unsigned>(raw-vertices));
const auto ids=a.take(36),modes=a.take(9),words=a.take(36);put(set,words);put(set+0x80,ids);put(set+0x88,9U);put(set+0x14,std::array<unsigned short,2>{9,0});put(graphics+0x10,modes);
put(words,std::array<unsigned,9>{274433,17055761,2035746,17063986,33845314,52371538,69791842,16826480,52383872});
for(unsigned i=0;i<9;++i){put(ids+4*i,i+1);put(graphics+0x30+8*i,set);}
const std::array<std::uintptr_t,5> inputs{extra_owners[0],extra_owners[1],owners[2],extra_owners[2],extra_owners[3]};
for(unsigned i=0;i<5;++i)put(graphics+0x40+8*i,inputs[i]);
put(queue+64,0x78006595U);put(queue+72,packet);put(context+0x2b80,5U);
const auto accept=[&]{auto q=std::make_unique<route::Plan>();q->stamp=s;
 if(!route::build_plan(*q,motion))throw std::runtime_error("DOF plan "+std::to_string(route::plan_rejection)+":"+std::to_string(route::plan_qualification)+":"+std::to_string(route::dependency_rejection));
 require(q->overlay_count==1&&q->draws[4].dependency==3&&q->extra_resources==5,"DOF graph and retained inputs");
 require(q->draws[4].reads[2]==2&&q->grade_input_resource==q->draws[4].output.resource&&q->order[1]==4&&q->order[2]==1,"DOF t2 provenance and grading dependency");
 require(q->draws[4].read_count==5&&route::dependency_binding_check(*q,4).accepted(),"all five bindings captured");return q;};
auto q=accept();unsigned negatives{};const auto reject=[&]{auto b=std::make_unique<route::Plan>();b->stamp=s;require(!route::build_plan(*b,motion),"invalid DOF plan rejected");++negatives;};
put(entry+0x10,owners[1]);reject();put(entry+0x10,owners[2]);
put(graphics+0x50,extra_owners[1]);reject();put(graphics+0x50,owners[2]);
put(graphics+0x60,extra_owners[4]);reject();put(graphics+0x60,extra_owners[3]);
put(modes+6,static_cast<unsigned char>(1));reject();put(modes+6,static_cast<unsigned char>(0));
put(extra_owners[0]+0x88,128U+(64U<<16));reject();put(extra_owners[0]+0x88,64U+(32U<<16));
put(raw+8,.5F);reject();put(raw+8,0.F);
put(entry+4,0x7fffff2U);reject();put(entry+4,26005U);
put(filters+0x68,entry);put(filters+0x3c,6U);reject();put(filters+0x3c,5U);
// A changed fifth texture must be diagnosed, not silently omitted by the old
// four-register binding comparison.
std::uintptr_t old_view{};require(route::field(extra_owners[3],0x78,old_view),"fifth SRV");put(old_view+0x10,old_view+32);
const auto changed=route::dependency_binding_check(*q,4);require(changed.readable&&!changed.accepted()&&changed.changes[0][0]==9&&changed.changes[0][1]==4,"fifth descriptor change receipt");put(old_view+0x10,old_view);
accept();require(negatives==8,"DOF negative cases");
std::cout<<"PASS DOF five-input plan, native ordering, eight negative cases and fifth-binding change\n";
// The live Gallery bundle includes six compute dispatches BEFORE composition.
// Keep this distinct from the single-draw adapter fixture above: admission must
// still reject the whole bundle until its producer contracts are implemented.
const auto state_packet=a.take(0x30);put(state_packet,11U);
put(queue+64,0x78006595U);put(queue+72,state_packet);
constexpr std::array<std::array<unsigned,3>,6> groups{{{8,1440,1},{128,5,1},{16,9,1},{80,45,1},{80,45,1},{80,45,1}}};
std::uintptr_t first_compute{};
for(unsigned i=0;i<6;++i){const auto dispatch=a.take(0x30);if(!i)first_compute=dispatch;
 put(dispatch,6U);put(dispatch+0x18,groups[i]);put(dispatch+0x24,0xa55a0100U);
 put(queue+16*(5+i),0x78006595U);put(queue+16*(5+i)+8,dispatch);
}
put(queue+16*11,0x78006595U);put(queue+16*11+8,packet);put(context+0x2b80,12U);
auto complete=std::make_unique<route::Plan>();complete->stamp=s;
mhw::scaled_temporal::OutputDiagnostics::Graph observed;
require(!route::build_plan_observed(*complete,motion,observed)&&route::plan_rejection==17&&
 observed.command_gate[0]==7&&observed.command_gate[1]==0x78006595U&&observed.command_gate[2]==first_compute&&
 observed.command_gate[3]==5&&observed.command_gate[4]==6,"complete DOF bundle remains unqualified at first compute");
std::cout<<"PASS complete Gallery sequence [11,6,6,6,6,6,6,0] refused before composition\n";
put(filters+0x3c,4U);put(context+0x2b80,4U);put(grade+0x248,owners[2]);
