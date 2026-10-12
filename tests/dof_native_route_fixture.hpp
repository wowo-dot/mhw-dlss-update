#pragma once
// Synthetic native packets and actual D3D12 bindings. Private shader/constant
// inputs come from the caller. No game process, native assets or offsets are edited.
#include "../src/scaled_temporal/stream_route.cpp"
namespace dof_route_fixture {
namespace r=mhw::scaled_temporal::stream_route;
namespace obs=mhw::temporal_present::observer;
using namespace dof_fixture;
struct Arena {
 std::unique_ptr<std::byte[]> memory=std::make_unique<std::byte[]>(131072);std::size_t used{};
 std::uintptr_t take(std::size_t bytes){bytes=(bytes+15)&~std::size_t(15);require(used+bytes<=131072,"DOF arena");
  const auto result=reinterpret_cast<std::uintptr_t>(memory.get()+used);used+=bytes;return result;}
 template<class T>static void put(std::uintptr_t p,const T &v){std::memcpy(reinterpret_cast<void *>(p),&v,sizeof(v));}
};
struct Route {
 Arena a;std::array<std::vector<std::byte>,6> shaders;unsigned negatives{};sf::DofLayout layout{};
 std::uintptr_t state{},executor{},backend_context{};std::array<std::uintptr_t,9> owners{};
 ID3D12GraphicsCommandList *list{};std::uint64_t generation{1};
 Route(ID3D12Device *device,ID3D12GraphicsCommandList *command,std::span<const ComPtr<ID3D12Resource>> textures,
       const std::filesystem::path &shader_dir,std::span<const float,448> constants):list(command){
  require(MH_Initialize()==MH_OK,"route hooks init");std::string reason;
  const auto resolver=[](std::uintptr_t p,unsigned slot,std::uintptr_t &out)noexcept{out=(*reinterpret_cast<std::uintptr_t **>(p))[slot];return out!=0;};
  const bool installed=obs::install(device,resolver,reason,true,nullptr,true);require(installed,reason.c_str());obs::begin();obs::reset(list,generation,nullptr);
  r::cfg={};r::cfg.device=reinterpret_cast<std::uintptr_t>(device);r::cfg.image=0x10000000;r::cfg.backend=a.take(16);
  r::width=2560;r::height=1440;r::render_w=1280;r::render_h=720;r::accepting=true;layout=sf::dof_layout(r::width,r::height);
  auto &p=r::plans[0];p.dof=std::make_shared<sf::DofNativeSequence>();auto &d=*p.dof;d.count=6;d.key=0x78006595;d.stage=4;
  p.stamp.frame=32;p.stamp.epoch=1;p.stamp.context=a.take(0x3000);p.stamp.view=a.take(8);
  for(unsigned i=0;i<5;++i)p.resources[i]=textures[i];
  p.overlay_count=1;p.ordered_count=4;p.order={0,4,1,2};p.keys[4]=d.key;p.draws[4].dependency=3;
  auto queue=a.take(16*20);Arena::put(p.stamp.context+0x2b78,queue);
  for(unsigned i=0;i<9;++i){owners[i]=a.take(0x100);const auto back=a.take(0x40),srv=a.take(0x20),uav=a.take(0x20);
   Arena::put(owners[i]+0x60,back);Arena::put(back+0x20,reinterpret_cast<std::uintptr_t>(textures[i].Get()));
   Arena::put(owners[i]+0x88,layout.sizes[i][0]|(layout.sizes[i][1]<<16)|(dof_formats::pooled()&&i>=2?0x80000000U:0U));
   Arena::put(owners[i]+0x90,sf::DofNativeSequence::engine_formats[i]);Arena::put(owners[i]+0x94,UINT_MAX);
   Arena::put(owners[i]+0x78,srv);Arena::put(owners[i]+0x80,uav);
   // Opaque native view handles are only compared by the route, never decoded,
   // copied or replaced. GPU tables below use the actual fixture descriptors.
   Arena::put(srv+0x10,srv);Arena::put(uav+0x10,uav);
  }
  const auto cb=a.take(1792);std::memcpy(reinterpret_cast<void *>(cb),constants.data(),1792);
  constexpr const char *hashes[]{"98c622bab8c0e113eeb4062dcc78fec129441854bcf67f61db6ed6239ca68e3c","d2c5eca59598bd0ebdd67cd2d5e3858e79862a50a8e076423b7c6a3f498f6245","4f54cf139cc8222b235670375faa7a8f0970bf51a699c30b82357bfa8f6001d8","b7afd17a818940e9f2da3830ad2e208767930f4decd1572b4fb64b164c85a532","c4c5caf1ae7f1cdfc6e2762ef954f51ed65b64056cbb02778c963977f84bec7f","61b739ad7861bffa528e0f9a1b58958765549cd0aac1d8e6a2b5366cc11ac6db"};
  for(unsigned pass=0;pass<6;++pass){
   auto &c=d.passes[pass];shaders[pass]=read(shader_dir/(std::string(hashes[pass])+".cs.dxbc"));
   const auto packet=a.take(0x30),graphics=a.take(0x300),handle=a.take(8),set=a.take(0x100),words=a.take(512),ids=a.take(256),modes=a.take(64),entry=a.take(0x20),shader=a.take(0x10);
   c.packet=packet;c.index=10+pass;Arena::put(packet,6U);Arena::put(packet+0x10,graphics);
   Arena::put(packet+0x18,std::array<unsigned,3>{layout.groups[pass][0],layout.groups[pass][1],1});
   Arena::put(packet+0x24,0xa55a0100U);Arena::put(graphics+8,handle);Arena::put(handle,set);
   Arena::put(graphics+0x10,modes);Arena::put(graphics+0x18,sf::DofNativeSequence::techniques[pass]);
   Arena::put(set,words);Arena::put(set+0x80,ids);Arena::put(set+0x88,sf::DofNativeSequence::counts[pass]);
   Arena::put(set+0x44,std::array<unsigned short,2>{static_cast<unsigned short>(sf::DofNativeSequence::counts[pass]),0});
   Arena::put(set+0x78,entry);Arena::put(entry+0x10,shader);Arena::put(shader,static_cast<unsigned>(shaders[pass].size()));Arena::put(shader+8,reinterpret_cast<std::uintptr_t>(shaders[pass].data()));
   for(unsigned j=0;j<sf::DofNativeSequence::counts[pass];++j){const auto word=sf::DofNativeSequence::words[pass][j],kind=word&15,index=(word>>4)&255,reg=word>>24;
    Arena::put(words+4*j,word);Arena::put(ids+4*index,(word>>12)&4095);std::uintptr_t object{};
    if(kind==2||kind==3)object=owners[kind==2?sf::DofNativeSequence::reads[pass][reg]:sf::DofNativeSequence::writes[pass][reg]];
    else if(kind==0)object=cb+(reg==0?0:reg==1?1280:1536);else object=a.take(8);
    Arena::put(graphics+0x30+8*index,object);
   }
   require(r::dof_read(c),"native compute packet capture");
  }
  p.draws[4].dependency_source=owners[1];p.draws[4].draw.packet=a.take(0x50);
  constexpr unsigned roles[]{5,6,1,4,8};for(unsigned i=0;i<5;++i)p.dependency_bindings[0].textures[i].object=owners[roles[i]];
  Arena::put(queue+16*16,d.key);Arena::put(queue+16*16+8,p.draws[4].draw.packet);
  require(r::dof_qualify(p),"complete native DOF recipe/actual resources");
  require(d.flagged==(dof_formats::pooled()?0x1fcU:0U)&&!d.failed_at&&!d.miss_count,"engine extent flags and clean diagnostics");
  std::array<std::uint64_t,5> comp{};for(unsigned i=0;i<5;++i)comp[i]=owners[roles[i]];
  const auto reject=[&]{require(!d.qualify(r::width,r::height,r::render_w,r::render_h,owners[1],comp),"malformed six-pass contract rejected");++negatives;};
  ++d.passes[0].prefix[6];reject();--d.passes[0].prefix[6];
  ++d.passes[2].index;reject();--d.passes[2].index;
  d.passes[3].modes[2]=1;reject();d.passes[3].modes[2]=0;
  const auto view=d.passes[0].views[0];d.passes[0].views[0][1]=0;reject();d.passes[0].views[0]=view;
  d.passes[1].shader.bytes[100]^=1;reject();d.passes[1].shader.bytes[100]^=1;
  const auto old=comp[1];comp[1]=comp[0];reject();comp[1]=old;
  d.passes[4].constants[0].bytes[0]^=1;reject();d.passes[4].constants[0].bytes[0]^=1;
  require(d.qualify(r::width,r::height,r::render_w,r::render_h,owners[1],comp),"valid contract recovers");
  // The adapted passes average a 2x2 box: a Quality-like 2/3 render scale is refused (96).
  require(!d.qualify(r::width,r::height,r::width*2/3,r::height*2/3,owners[1],comp)&&d.failed_at==96,"non-2:1 render scale rejected");++negatives;
  require(d.qualify(r::width,r::height,r::render_w,r::render_h,owners[1],comp),"valid contract recovers again");
  state=a.take(0x200);executor=a.take(16);backend_context=a.take(16);
  Arena::put(state+0x20,reinterpret_cast<std::uintptr_t>(list));Arena::put(executor+8,state);Arena::put(backend_context+8,r::cfg.backend);
  p.built=true;r::watch(0,p);
 }
 void warm(unsigned pass){auto &p=r::plans[0];auto &d=*p.dof;Arena::put(state+0x158,d.passes[pass].packet);
  const auto token=r::compute_before(executor,backend_context,d.passes[pass].packet);
  require(token==12,"native compute scope token");const auto &g=layout.groups[pass];
  require(!r::compute_dispatch(list,g[0],g[1],g[2],r::cfg.image+0x259cfca),"warmup preserves native dispatch");r::compute_after(token,true);
 }
 void prepare(){auto &p=r::plans[0];p.dof->entered=p.dof->returned=p.dof->dispatched=0;
  require(r::dof_warm(p),"both adapted PSOs warmed");require(r::prepare_selector(p),"selector");
  std::array<std::uintptr_t,12> packets{};for(auto &v:packets)v=a.take(8);
  for(unsigned i=0;i<6;++i)packets[3+i]=p.dof->passes[i].packet;packets[9]=p.draws[4].draw.packet;
  require(p.handoff.prepare({32,1,p.stamp.context,p.stamp.view},packets)==sf::FrameHandoff::Result::ok,"six dispatch handoff plan");
  for(unsigned i=0;i<3;++i)require(p.handoff.record(i,packets[i],{reinterpret_cast<std::uintptr_t>(list),generation})==sf::FrameHandoff::Result::ok,"preceding native stages");
  p.prepared=true;tail=packets;r::watch(0,p);
 }
 std::array<std::uintptr_t,12> tail{};
 void dispatch(unsigned pass){auto &p=r::plans[0];Arena::put(state+0x158,p.dof->passes[pass].packet);
  // Resource-state planning mutates only packet+8 between CPU preparation and execution.
  Arena::put(p.dof->passes[pass].packet+8,std::uintptr_t{0x12345678});
  const auto token=r::compute_before(executor,backend_context,p.dof->passes[pass].packet);require(token==12,"prepared native compute entry");
  const auto &g=layout.groups[pass];const bool replaced=r::compute_dispatch(list,g[0],g[1],g[2],r::cfg.image+0x259cfca);
  require(r::healthy(),r::failure.c_str());require(replaced==(pass==0||pass==3),"only two adapted native dispatches");
  if(!replaced)list->Dispatch(g[0],g[1],g[2]);r::compute_after(token,true);require(r::healthy(),"compute return");
 }
 void choose(){auto &p=r::plans[0];for(unsigned i=9;i<12;++i)require(p.handoff.record(i,tail[i],{reinterpret_cast<std::uintptr_t>(list),generation})==sf::FrameHandoff::Result::ok,"downstream composition stages");
  require(r::choose(p,sf::FrameHandoff::Choice::reconstructed),"late reconstructed selector");
 }
 void submitted(ID3D12CommandQueue *queue,std::uint64_t signal){
  const std::array<std::array<std::uint64_t,2>,1> batch{{{reinterpret_cast<std::uintptr_t>(list),generation}}};
  require(r::submitted_batch(reinterpret_cast<std::uintptr_t>(queue),batch,signal),"production submission order");
 }
 void retire(ID3D12CommandAllocator *allocator,ID3D12Fence *fence,std::uint64_t signal){
  require(fence->GetCompletedValue()>=signal&&fence->GetCompletedValue()!=UINT64_MAX,"actual GPU fence");
  r::collect(signal);require(r::lifetime.live()!=0,"completion alone retains recording");
  hr(allocator->Reset(),"actual allocator Reset");hr(list->Reset(allocator,nullptr),"actual recording Reset");hr(list->Close(),"close retired list");
  require(r::reset(list,generation),"Reset retirement");r::collect(1);
  r::collect(signal);
  require(!r::plans[0].prepared&&r::lifetime.live()==0,"fence plus Reset releases all native resources");
  require(r::dof_stats.dispatches==6&&r::dof_stats.adapted==2&&r::dof_stats.restored==2&&r::dof_stats.completed==1,"six-pass counters");
  require(obs::finish(),"observer shutdown");require(MH_Uninitialize()==MH_OK,"route hooks finish");
 }
};
}
