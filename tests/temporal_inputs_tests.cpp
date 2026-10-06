#include "../src/temporal_bindings/inputs.hpp"
#include "../src/temporal_bindings/shader.hpp"
#include "../src/temporal_bindings/depth_routes.hpp"
#include <map>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <vector>
namespace tb=mhw::temporal_bindings;
void check(bool ok,const char *name){if(!ok){std::cerr<<name<<'\n';std::exit(1);}}
struct Memory {
  std::map<std::uintptr_t,std::byte> bytes;
  template<class T>void put(std::uintptr_t p,T v){auto b=std::as_bytes(std::span(&v,1));for(auto x:b)bytes[p++]=x;}
  static bool read(void *user,std::uintptr_t p,std::span<std::byte> out){auto &m=*static_cast<Memory *>(user);for(auto &b:out){auto i=m.bytes.find(p++);if(i==m.bytes.end())return false;b=i->second;}return true;}
  tb::Reader reader(){return {this,read};}
};
const tb::Config cfg{0x10000000,0x20000000,0x30000000,0x40000000};
constexpr std::uintptr_t layer=0x50000000,view=0x60000000,gbuffer=0x70000000,context=0x20000710,packet_at=0x80000000;
Memory fixture(){
  Memory m;auto ptr=[&](std::uintptr_t at,std::uintptr_t p){m.put(at,p);};
  m.put(cfg.image+0x51cc080,static_cast<unsigned char>(1));ptr(cfg.image+0x51c4480,cfg.renderer);
  ptr(cfg.renderer+0x78,cfg.backend);ptr(cfg.backend+0x18,cfg.device);ptr(cfg.backend+0x42700,0);
  m.put(cfg.renderer+0xb4,0U);ptr(layer,cfg.image+0x351bda8);ptr(context+0x1d0,view);ptr(cfg.renderer+0x1f450,view);
  m.put(cfg.renderer+0x1f210,std::uint64_t{5});m.put(cfg.renderer+0x1f208,std::uint64_t{50});m.put(cfg.renderer+0x1f200,1U);
  m.put(cfg.renderer+0x93,static_cast<unsigned char>(0));m.put(context+0x158,static_cast<std::uint16_t>(9));
  ptr(context+0x1c8,0x90100000);m.put(context+0x1ea74,8U);m.put(layer+0x184,0x01010000U);ptr(view+0x620,gbuffer);
  const std::array<unsigned,4> rect={0,0,2560,1440};for(unsigned i=0;i<4;++i)m.put(context+0x5c+4*i,rect[i]);
  std::array<std::uintptr_t,tb::roles> owners{};
  for(unsigned i=0;i<tb::roles;++i){
    const auto owner=0xa0000000ULL+0x1000*i;owners[i]=owner;const auto backing=0xb0000000ULL+0x1000*i;
    m.put(cfg.image+tb::id_rvas[i],0x10000U+10+i);ptr(context+0x710+8*(10+i),owner);
    ptr(owner,cfg.image+0x351c870);m.put(owner+0x58,5U);ptr(owner+0x60,backing);ptr(owner+0xf0,0);
    m.put(owner+0x70,2U);m.put(owner+0x90,34U);m.put(owner+0x94,UINT_MAX);m.put(owner+0x88,2560U|(1440U<<16));ptr(backing+0x20,0xc0000000ULL+0x1000*i);
  }
  ptr(layer+0x150,owners[0]);ptr(layer+0x170,owners[1]);ptr(layer+0x178,owners[4]);
  ptr(gbuffer+0x60,owners[3]);ptr(gbuffer+0x78,owners[5]);ptr(gbuffer+0xc0,owners[6]);
  m.put(packet_at,0xa0000006U);ptr(packet_at+0x10,0x90100000);m.put(packet_at+0x18,160U);m.put(packet_at+0x1c,90U);
  m.put(packet_at+0x20,1U);m.put(packet_at+0x24,static_cast<unsigned char>(0));ptr(packet_at+0x28,0);
  return m;
}
void shader_tests(const tb::Snapshot &s) {
  Memory m;
  const std::uintptr_t handle=0xe0000000,set=handle+0x100,entry=handle+0x200,desc=handle+0x300,ids=handle+0x400,modes=handle+0x500,words=handle+0x600;
  m.put(s.state+8,handle);m.put(handle,set);m.put(set+0x78,entry);m.put(entry+0x10,desc);m.put(desc,52U);m.put(desc+8,handle+0x800);
  m.put(s.state+0x18,s.technique);m.put(set+0x88,7U);m.put(set+0x80,ids);m.put(s.state+0x10,modes);m.put(set,words);
  m.put(set+0x44,std::uint16_t{9});m.put(set+0x46,std::uint16_t{0});
  for(unsigned i=0;i<7;++i){const auto &v=s.inputs[i];const auto shader_view=handle+0x900+32*i;
    m.put(ids+4*i,v.id);m.put(modes+i,static_cast<unsigned char>(i));m.put(s.state+0x30+8*i,v.object);
    m.put(words+4*i,(i<<24)|((v.id&4095)<<12)|(i<<4)|(i==4?3U:2U));
    m.put(v.object+(i==4?0x80:0x78),shader_view);m.put(shader_view+0x10,handle+0xc00+32*i);
  }
  m.put(words+28,0x01234000U);m.put(words+32,0x01000001U); // CB and sampler require no SRV reads.
  tb::ShaderRecipe recipe;check(tb::shader_recipe(m.reader(),s,recipe),"shader recipe");
  unsigned missing{};
  for(const auto &[address,value]:m.bytes){auto bad=m;bad.bytes.erase(address);tb::ShaderRecipe out;
    check(!tb::shader_recipe(bad.reader(),s,out),"missing shader scalar");++missing;}
  for(const auto &[address,value]:std::vector<std::pair<std::uintptr_t,unsigned>>{
      {desc,tb::shader_limit+1},{desc,0},{s.state+0x18,s.technique+1},{set+0x88,65},{set+0x88,0},
      {set+0x44,129},{words,4},{words,(7U<<4)|2},{s.state+0x30,1},{ids,UINT_MAX}}) {
    auto bad=m;bad.put(address,value);tb::ShaderRecipe out;check(!tb::shader_recipe(bad.reader(),s,out),"bad shader metadata");
  }
  for(auto at:{handle,set+0x78,entry+0x10,desc+8,set+0x80,s.state+0x10,set}) {
    auto bad=m;bad.put(at,UINTPTR_MAX-1);tb::ShaderRecipe out;check(!tb::shader_recipe(bad.reader(),s,out),"overflow pointer");
  }
  std::array<std::byte,52> bytes{};
  const auto put=[&](unsigned at,unsigned value){std::memcpy(bytes.data()+at,&value,4);};
  std::memcpy(bytes.data(),"DXBC",4);put(20,1);put(24,52);put(28,1);put(32,36);
  std::memcpy(bytes.data()+36,"SHEX",4);put(40,8);put(44,0x50050);put(48,2);
  check(tb::compute_dxbc(bytes.data(),52),"compute container structure");
  for(unsigned size=0;size<52;++size)check(!tb::compute_dxbc(bytes.data(),size),"truncated container");
  for(const auto &[at,value]:std::vector<std::pair<unsigned,unsigned>>{{44,0x10050},{44,0x50051},{48,3},{40,4},{32,32},{28,0},{24,51}}){
    auto bad=bytes;std::memcpy(bad.data()+at,&value,4);check(!tb::compute_dxbc(bad.data(),52),"invalid compute container");}
  std::cout<<"Shader recipe and DXBC rejection checks passed; "<<missing<<" missing shader bytes rejected\n";
}
void depth_tests(const tb::Snapshot &s) {
  auto m=fixture();const std::uintptr_t container=0xf0000000,attachment=container+0x100,view_wrapper=container+0x200;
  const auto owner=s.inputs[3].object;
  m.put(s.view+0x628,container);m.put(container+0xa0,owner);m.put(owner+0x78,view_wrapper);
  m.put(view_wrapper+0x10,std::uintptr_t{0xabc0});m.put(s.context+0x50,attachment);m.put(attachment+0x58,owner);
  auto expected=s.inputs[3];expected.id=0;
  tb::SceneDepth d;check(tb::scene_depth(m.reader(),s,d) && d.texture==expected,"scene lookup");
  // IDs belong to TAA slots, not this independently resolved texture owner.
  check(d.srv_readable && d.cpu_descriptor==0xabc0 && d.attachment_readable && d.attachment_resource==d.texture.resource,"scene metadata");
  for(auto at:{s.view+0x628,container+0xa0,owner+0x60}){
    auto bad=m;bad.put(at,std::uintptr_t{UINTPTR_MAX-1});tb::SceneDepth out;check(!tb::scene_depth(bad.reader(),s,out),"depth pointer overflow");}
  auto absent=m;absent.put(s.context+0x50,std::uintptr_t{});tb::SceneDepth out;
  check(tb::scene_depth(absent.reader(),s,out) && !out.attachment_readable,"missing current attachment is explicit unknown");
  absent=m;absent.bytes.erase(view_wrapper+0x10);
  check(tb::scene_depth(absent.reader(),s,out) && !out.srv_readable,"opaque view unavailable");
  absent=m;absent.bytes.erase(container+0xa0);check(!tb::scene_depth(absent.reader(),s,out),"unreadable selected depth");
  auto changed=m;changed.put(container+0xa0,s.inputs[4].object);changed.put(s.inputs[4].object+0x78,view_wrapper);
  check(tb::scene_depth(changed.reader(),s,out) && out!=d,"depth owner replacement detected");
  std::cout<<"Scene-depth ownership, overflow, missing and replacement checks passed\n";
}
void route_contract_tests(){
  namespace dr=tb::depth_routes;
  std::array<D3D12_RESOURCE_BARRIER,2> barriers{};
  for(unsigned i=0;i<2;++i){auto &t=barriers[i].Transition;t.pResource=reinterpret_cast<ID3D12Resource *>(0x1000ULL+i*0x1000);t.Subresource=UINT_MAX;
    t.StateBefore=i?D3D12_RESOURCE_STATE_RENDER_TARGET:D3D12_RESOURCE_STATE_DEPTH_WRITE;t.StateAfter=i?D3D12_RESOURCE_STATE_COPY_DEST:D3D12_RESOURCE_STATE_COPY_SOURCE;}
  check(dr::copy_states(barriers,0x1000,0x2000),"exact depth-copy state interval");
  check(!dr::copy_states(std::span(barriers.data(),1),0x1000,0x2000),"missing destination state");
  check(!dr::copy_states(barriers,0x1000,0x1000),"copy state alias");
  for(unsigned variant=0;variant<5;++variant){auto bad=barriers;
    if(variant==0)bad[0].Transition.StateAfter=static_cast<D3D12_RESOURCE_STATES>(0x840);
    if(variant==1)bad[0].Flags=D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
    if(variant==2)bad[0].Transition.Subresource=0;
    if(variant==3){bad[0]={};bad[0].Type=D3D12_RESOURCE_BARRIER_TYPE_ALIASING;}
    if(variant==4)bad[1].Transition.StateAfter=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    check(!dr::copy_states(bad,0x1000,0x2000),"unqualified depth-copy state");}
  auto drawn=barriers;drawn[0].Transition.StateAfter=static_cast<D3D12_RESOURCE_STATES>(0xc0);
  drawn[1].Transition.StateBefore=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;drawn[1].Transition.StateAfter=D3D12_RESOURCE_STATE_RENDER_TARGET;
  check(dr::draw_states(drawn,0x1000,0x2000) && !dr::copy_states(drawn,0x1000,0x2000),"draw and copy states remain distinct");
  drawn[0].Transition.StateAfter=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  check(dr::draw_states(drawn,0x1000,0x2000) && !dr::copy_states(drawn,0x1000,0x2000),"pixel-only native depth draw");
  check(!dr::draw_states(std::span(drawn.data(),1),0x1000,0x2000),"unknown draw destination");
  check(!dr::draw_states(std::span(drawn.data()+1,1),0x1000,0x2000),"unknown draw source");
  check(!dr::draw_states(drawn,0x1000,0x1000),"draw resource alias");
  for(unsigned state:{0U,0x40U,0x10U,0x4U,0x90U,0x88U,0x180U,0xc4U,0x8c0U,0x80000080U}){
    auto bad=drawn;bad[0].Transition.StateAfter=static_cast<D3D12_RESOURCE_STATES>(state);
    check(!dr::draw_states(bad,0x1000,0x2000),"nonpixel/write/extra-bit draw source rejected");}
  std::array<D3D12_RESOURCE_BARRIER,3> conflict{drawn[0],drawn[1],drawn[0]};
  check(!dr::draw_states(conflict,0x1000,0x2000),"conflicting draw transition chain");
  for(unsigned variant=0;variant<5;++variant){auto bad=drawn;
    if(variant==0)bad[0].Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
    if(variant==1)bad[1].Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_DEST;
    if(variant==2)bad[0].Flags=D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
    if(variant==3)bad[1].Transition.Subresource=0;
    if(variant==4){bad[0]={};bad[0].Type=D3D12_RESOURCE_BARRIER_TYPE_ALIASING;}
    check(!dr::draw_states(bad,0x1000,0x2000),"unqualified depth-draw state");}
  auto m=fixture();m.put(view+0x628,std::uintptr_t{0xf0000000});dr::Stamp s;
  check(dr::stamp(m.reader(),cfg,s) && s.frame==5 && s.epoch==50 && s.context==context,"copy frame stamp");
  for(auto at:{cfg.image+0x51cc080,cfg.renderer+0x1f210,view+0x628,context+0x1d0,cfg.backend+0x18}){
    auto bad=m;bad.bytes.erase(at);check(!dr::stamp(bad.reader(),cfg,s),"missing copy stamp");}
  for(auto at:{view+0x628,context+0x1d0}){auto bad=m;bad.put(at,UINTPTR_MAX);check(!dr::stamp(bad.reader(),cfg,s),"copy stamp overflow");}
  for(auto at:{cfg.renderer+0x1f200,cfg.renderer+0x93}){auto bad=m;bad.put(at,2U);check(!dr::stamp(bad.reader(),cfg,s),"copy stamp invalid bank");}
  Memory p;p.put(packet_at,0x8000000aU);p.put(packet_at+0x10,8U);p.put(packet_at+0x18,std::uintptr_t{0x1100});
  p.put(packet_at+0x20,std::uintptr_t{0x2200});p.put(packet_at+0x28,std::array<unsigned,6>{0,0,2560,1440,0,0});p.put(packet_at+0x40,std::uintptr_t{});
  dr::CopyPacket out;check(dr::packet(p.reader(),packet_at,out) && out.opcode==10,"copy packet contract");
  for(const auto &[at,value]:p.bytes){auto bad=p;bad.bytes.erase(at);check(!dr::packet(bad.reader(),packet_at,out),"truncated copy packet");}
  for(const auto &[at,value]:std::vector<std::pair<std::uintptr_t,unsigned>>{{packet_at,6},{packet_at+0x10,7},{packet_at+0x30,0},{packet_at+0x34,0}}){
    auto bad=p;bad.put(at,value);check(!dr::packet(bad.reader(),packet_at,out),"invalid copy packet");}
  auto alias=p;alias.put(packet_at+0x20,std::uintptr_t{0x1100});check(!dr::packet(alias.reader(),packet_at,out),"self copy packet");
  check(!dr::packet(p.reader(),UINTPTR_MAX-2,out),"copy packet pointer overflow");
}
tb::NativeFrame received{};unsigned forwarded{};
extern "C" std::uint64_t temporal_fixture_call(std::uintptr_t);
extern "C" void mhw_binding_forward(void *,mhw::projection_binding::Frame *);
extern "C" void mhw_binding_dispatch(unsigned,mhw::projection_binding::Frame *){std::abort();}
std::uint64_t original(std::uintptr_t a,std::uintptr_t b,std::uintptr_t c,std::uintptr_t d,std::uintptr_t e,std::uintptr_t f){
  check(a==0x1122 && b==0x3344 && c==0x5566 && d==0x7788 && e==0x99aa && f==0xaabb,"forwarded ABI");++forwarded;return 0x123456789abcdef0ULL;
}
extern "C" void mhw_temporal_dispatch(tb::NativeFrame *f){received=*f;mhw_binding_forward(reinterpret_cast<void *>(original),&f->call);}
extern "C" void mhw_temporal_consume(tb::NativeFrame *){std::abort();}
int main(){
  unsigned checks{};
  auto m=fixture();tb::Snapshot s;check(tb::snapshot(m.reader(),cfg,layer,context,s),"valid snapshot");++checks;
  check(s.frame==5 && s.inputs[0].width==2560 && s.inputs[0].height==1440,"decoded source");++checks;
  for(unsigned flags=0;flags<4;++flags){
    auto flagged=m;flagged.put(s.inputs[0].object+0x88,2560U|(1440U<<16)|(flags<<30));
    tb::Snapshot decoded;
    check(tb::snapshot(flagged.reader(),cfg,layer,context,decoded) && decoded.inputs[0].width==2560 &&
      decoded.inputs[0].height==1440,"dimension flags excluded from extent");++checks;
    tb::Owner owner;
    flagged.put(s.inputs[0].object+0x88,0xffffU|(0x3fffU<<16)|(flags<<30));
    check(tb::read_owner(flagged.reader(),s.inputs[0].object,owner) && owner.width==0xffff &&
      owner.height==0x3fff,"full packed extent with flags");++checks;
    flagged.put(s.inputs[0].object+0x88,2560U|(flags<<30));
    check(!tb::read_owner(flagged.reader(),s.inputs[0].object,owner),"flags do not make zero height valid");++checks;
  }
  shader_tests(s);
  depth_tests(s);
  route_contract_tests();
  tb::Packet p;check(tb::packet(m.reader(),packet_at,p) && tb::packet_matches(s,p,160,90,1),"packet association");++checks;
  // Every scalar read matters; removing any byte must reject the snapshot.
  unsigned missing{};
  for(const auto &[address,value]:m.bytes){
    if(address>=packet_at && address<packet_at+0x30)continue;
    auto broken=m;broken.bytes.erase(address);tb::Snapshot discarded;
    check(!tb::snapshot(broken.reader(),cfg,layer,context,discarded),"missing scalar accepted");++missing;
  }
  const std::vector<std::pair<std::uintptr_t,unsigned>> mutations={
    {cfg.image+0x51cc080,0},{cfg.renderer+0xb4,1},{cfg.renderer+0x1f200,2},{cfg.renderer+0x93,2},
    {context+0x1ea74,0x10},{context+0x64,0},{context+0x68,17000},{cfg.image+tb::id_rvas[0],UINT_MAX},
    {cfg.image+tb::id_rvas[1],0x1000a},{layer+0x150,0},{layer+0x170,0},{layer+0x178,0},{gbuffer+0x60,0},
    {gbuffer+0x78,0},{gbuffer+0xc0,0},{cfg.backend+0x42700,1},{layer,0},{context+0x1d0,0},
    {0xa0000088,0},{0xb0000020,0}};
  for(const auto &[at,value]:mutations){auto bad=m;bad.put(at,value);tb::Snapshot out;check(!tb::snapshot(bad.reader(),cfg,layer,context,out),"bad source accepted");++checks;}
  for(unsigned frame=1;frame<=12;++frame){auto rotated=m;rotated.put(cfg.renderer+0x1f210,std::uint64_t{frame});
    rotated.put(layer+0x168+8*((frame-1)%3),std::uintptr_t{0xa0001000});rotated.put(layer+0x168+8*(frame%3),std::uintptr_t{0xa0004000});
    tb::Snapshot out;check(tb::snapshot(rotated.reader(),cfg,layer,context,out),"history rotation");++checks;}
  for(unsigned change=0;change<8;++change){auto bad=p;
    if(change==0)bad.state++;if(change==1)bad.x++;if(change==2)bad.y++;if(change==3)bad.z++;
    if(change==4)bad.mode=1;if(change==5)bad.indirect=1;if(change==6)bad.opcode=10;if(change==7)bad.object=0;
    check(!tb::packet_matches(s,bad,160,90,1),"bad packet accepted");++checks;
  }
  auto secondary=m;secondary.put(0xa00000f0,std::uintptr_t{0xb0010000});secondary.put(0xb0010020,std::uintptr_t{0xc0010000});
  tb::Snapshot alt;check(tb::snapshot(secondary.reader(),cfg,layer,context,alt) && alt.inputs[0].resource==0xc0010000 && alt!=s,"secondary backing");++checks;
  const auto result=temporal_fixture_call(0x1020304050607080ULL);
  check(result==0x123456789abcdef0ULL && forwarded==1 && received.layer==0x1020304050607080ULL && received.call.caller,"entry ABI and return");++checks;
  check(received.call.stack[0]==0x99aa && received.call.stack[1]==0xaabb,"stack ABI");++checks;
  std::cout<<checks<<" checks and "<<missing<<" unreadable-byte cases passed\n";
}
