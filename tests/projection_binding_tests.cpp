#include "recorder.hpp"
#include "provenance.hpp"
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <MinHook.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sstream>
#include <thread>
#include <vector>
namespace pb=mhw::projection_binding;
using U=std::uintptr_t;
using F=U(*)(U,U,U,U,U,U,U);
extern "C" {
U binding_fixture0(U,U,U,U,U,U,U); U binding_fixture1(U,U,U,U,U,U,U); U binding_fixture2(U,U,U,U,U,U,U);
U binding_fixture3(U,U,U,U,U,U,U); U binding_fixture4(U,U,U,U,U,U,U); U binding_fixture5(U,U,U,U,U,U,U);
U binding_fixture6(U,U,U,U,U,U,U); U binding_fixture7(U,U,U,U,U,U,U); U binding_fixture8(U,U,U,U,U,U,U);
U binding_fixture9(U,U,U,U,U,U,U); U binding_fixture10(U,U,U,U,U,U,U); U binding_fixture11(U,U,U,U,U,U,U);
U binding_fixture12(U,U,U,U,U,U,U); U binding_fixture13(U,U,U,U,U,U,U); U binding_fixture14(U,U,U,U,U,U,U);
U binding_fixture15(U,U,U,U,U,U,U); U binding_fixture16(U,U,U,U,U,U,U); U binding_fixture17(U,U,U,U,U,U,U);
U binding_fixture18(U,U,U,U,U,U,U);
U binding_fixture19(U,U,U,U,U,U,U);
U binding_fixture20(U,U,U,U,U,U,U);U binding_fixture21(U,U,U,U,U,U,U);U binding_fixture22(U,U,U,U,U,U,U);U binding_fixture23(U,U,U,U,U,U,U);
void binding_fixture_returns(F,std::array<U,2> *);
U binding_fixture_unsafe(U,U,U,U,U,U,U);
}
namespace {
constexpr std::array<F,pb::kind_count> functions={binding_fixture0,binding_fixture1,binding_fixture2,binding_fixture3,binding_fixture4,
 binding_fixture5,binding_fixture6,binding_fixture7,binding_fixture8,binding_fixture9,binding_fixture10,binding_fixture11,binding_fixture12,
 binding_fixture13,binding_fixture14,binding_fixture15,binding_fixture16,binding_fixture17,binding_fixture18,binding_fixture19,
 binding_fixture20,binding_fixture21,binding_fixture22,binding_fixture23};
std::array<U,pb::kind_count> targets{};
std::string mode;
std::atomic<unsigned> calls{},bad{};
unsigned original_jitter_calls{};
std::atomic<bool> release_original{};
struct Memory {void *p{};~Memory(){if(p)VirtualFree(p,0,MEM_RELEASE);}} image_memory,camera_memory,arena_memory;
std::vector<std::byte> renderer(0x1f570),table(0xe740),view(0x80),backend(0x42720),backend_context(0x2000),state(0x400);
std::array<std::byte,0x118> packet{};
std::array<std::byte,0x80> shaders{};
std::array<std::byte,0x18> metadata{},stage{};
std::array<std::byte,16> shader_desc{},graphics{};
std::array<std::byte,16> root_recipe{};
std::array<std::byte,44> blob{};
std::array<U,64> command_vt{},device_vt{};
std::array<U,2> executor{};
U image{},camera{},context{},metadata_pointer{},shader_set{},command_object{},device_object{},constants{},gpu{};
pb::Runtime rt{};
template<class T> U address(T &v){return reinterpret_cast<U>(v.data());}
template<class T> void put(U p,unsigned off,T value){std::memcpy(reinterpret_cast<void *>(p+off),&value,sizeof(value));}
void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
struct AddressPressure {
  std::array<void *,1024> reservations{};unsigned count{};
  ~AddressPressure(){for(unsigned i=0;i<count;++i)VirtualFree(reservations[i],0,MEM_RELEASE);}
  void fill() {
    // Only reserve MEM_FREE regions in this isolated owned-fixture process.
    // Include a granularity margin for MinHook's final forward search step.
    constexpr U gran=65536,radius=0x40000000;
    U cursor=(*std::min_element(targets.begin(),targets.end())-radius-gran)&~(gran-1);
    const U end=(*std::max_element(targets.begin(),targets.end())+radius+2*gran)&~(gran-1);
    unsigned queries{};
    while(cursor<end) {
      check(++queries<=8192,"pressure query bound");MEMORY_BASIC_INFORMATION m{};
      check(VirtualQuery(reinterpret_cast<void *>(cursor),&m,sizeof(m))==sizeof(m),"pressure query");
      const auto next=reinterpret_cast<U>(m.BaseAddress)+m.RegionSize;check(next>cursor,"pressure progress");
      if(m.State==MEM_FREE) {
        const auto start=(cursor+gran-1)&~(gran-1);
        const auto limit=std::min(next,end);
        if(start<limit) {
          check(count<reservations.size(),"pressure allocation bound");
          const auto bytes=std::min<U>(limit-start,0x10000000);
          auto p=VirtualAlloc(reinterpret_cast<void *>(start),bytes,MEM_RESERVE,PAGE_NOACCESS);
          check(p==reinterpret_cast<void *>(start),"pressure reserve");reservations[count++]=p;
          cursor=start+bytes;continue;
        }
      }
      cursor=next;
    }
    check(count!=0,"pressure did not reserve anything");
  }
};
unsigned bits(float f){return std::bit_cast<unsigned>(f);}
void setup(){
  image_memory.p=VirtualAlloc(nullptr,0x55d3000,MEM_RESERVE,PAGE_NOACCESS);check(image_memory.p,"reserve");image=reinterpret_cast<U>(image_memory.p);
  for(auto off:{0x51c4000U,0x4be7000U})check(VirtualAlloc(reinterpret_cast<void *>(image+off),4096,MEM_COMMIT,PAGE_READWRITE)!=nullptr,"globals");
  camera_memory.p=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);camera=reinterpret_cast<U>(camera_memory.p);check(camera!=0,"camera");
  arena_memory.p=VirtualAlloc(nullptr,12288,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);check(arena_memory.p,"arena");constants=reinterpret_cast<U>(arena_memory.p)+256;gpu=constants+0x100000000ULL;
  const auto r=address(renderer),b=address(backend);context=r+0x710;
  put(image,0x51c4480,r);put(image,0x51c4368,address(table));put(image,0x51c4400,camera);
  put(r,0x78,b);put<unsigned char>(r,0xa5,1);put(r,0x1f450,address(view));put(r,0x1f210,std::uint64_t{10});put(r,0x1f208,std::uint64_t{10});
  put(camera,0,image+0x350d188);put(camera,0x40,10U);put<unsigned char>(camera,0x70,1);
  put(context,0x230,bits(2560.F));put(context,0x234,bits(1440.F));metadata_pointer=address(metadata);put(metadata_pointer,0x10,268U);
  put(context,0x2b18,reinterpret_cast<U>(&metadata_pointer));put(context,0x710,constants);
  put(image,0x4be75f4,0U);put(image,0x4be7604,0x20f00U);put(image,0x4be763c,0xc8f00U);
  put(image,0x4be75f8,0xf00U);put(image,0x4be75fc,0x10f00U);put(image,0x4be7640,0xd8f00U);
  for(const auto word:{0U,16U,32U,200U,216U})for(unsigned i=0;i<4;++i)put(constants,4*(word+i*5),bits(1.F));
  put(b,0xff0,2U);put(b,0x1000,4096U);put(b,0x1058,U{0x1234});put(b,0x1098,U{0x100000000ULL});
  for(unsigned i=0;i<3;++i){put(b,0x1018+8*i,constants-256+4096*i);put(b,0x1080+8*i,gpu-256+4096*i);put(b,0x1030+4*i,4096U);}
  put(address(backend_context),8,b);executor[1]=address(state);
  for(unsigned i=0;i<pb::kind_count;++i)targets[i]=reinterpret_cast<U>(functions[i]);
  for(unsigned i=pb::root_cbv;i<pb::kind_count;++i)if(i!=pb::rollover)(i==pb::create_cbv || i==pb::create_pipeline?device_vt:command_vt)[pb::method_slots[i]]=targets[i];
  command_object=address(command_vt);device_object=address(device_vt);
  put(b,0x18,reinterpret_cast<U>(&device_object));
  put(b,0x270,address(root_recipe));put(address(root_recipe),0,reinterpret_cast<U>(&device_object));put<unsigned char>(address(root_recipe),9,1);
  for(unsigned i=0;i<5;++i)put(b,0x278+8*i,U{0x22220000}+i*0x10000);
  put(b,0x288,U{0x22220000});
  if(mode=="root_wrong")put(b,0x288,U{0x33330000});
  put(address(state),0x20,reinterpret_cast<U>(&command_object));put(address(state),0x108,U{0x11110000});put(address(state),0x110,U{0x22220000});
  put(address(state),0x40,32U);put(address(state),8,std::uint64_t{0x10000});put(address(state),0x10,std::uint64_t{0x20000});
  shader_set=address(shaders);put(address(graphics),8,reinterpret_cast<U>(&shader_set));put(shader_set,0x50,address(stage));
  put(address(stage),0x10,address(shader_desc));put(address(shader_desc),0,44U);put(address(shader_desc),8,address(blob));
  std::memcpy(blob.data(),"DXBC",4);put(address(blob),20,1U);put(address(blob),24,44U);put(address(blob),28,1U);put(address(blob),32,36U);std::memcpy(blob.data()+36,"SHDR",4);
  rt.backend=b;rt.device=reinterpret_cast<U>(&device_object);rt.mode=2;rt.cpu_resource=rt.gpu_resource=0x1234;
  rt.cpu_bytes=rt.gpu_bytes=12288;rt.gpu_base=gpu-256;
}
U invoke(unsigned kind,U a=0,U b=0,U c=0,U d=0,U e=0,U f=0,U g=0){return functions[kind](a,b,c,d,e,f,g);}
#include "projection_attachment_fixture.inl"
void next_jitter_frame(unsigned index) {
  const auto frame=10ULL+index;put(address(renderer),0x1f210,frame);put(address(renderer),0x1f208,frame);put(camera,0x40,static_cast<unsigned>(frame));
  constants=reinterpret_cast<U>(arena_memory.p)+256+1280*index;gpu=constants+0x100000000ULL;put(context,0x710,constants);
  std::memset(reinterpret_cast<void *>(constants),0,1072);
  for(const auto word:{0U,16U,32U,200U,216U})for(unsigned i=0;i<4;++i)put(constants,4*(word+i*5),bits(1.F));
  put(address(table),0xe6a0+8*static_cast<unsigned>(frame%20),std::uint64_t{bits(.25F)}|(std::uint64_t{bits(-.375F)}<<32));
}
void fixture_log(const char *name,const pb::Capture &capture,bool jitter=false) {
  const std::filesystem::path path(name);check(!std::filesystem::exists(path),"preserve fixture artifact");
  std::ofstream log(path,std::ios::binary);log<<std::boolalpha;
  const auto &pool=pb::pool_state();
  log<<"{\"event\":\"start\",\"owned_fixture\":true,\"schema\":1,\"version\":\""<<(capture.attachments_enabled?"0.3.1":jitter?"0.2.1":"0.1.3")<<"\",\"pid\":1,\"creation\":1,\"host_sha256\":\""<<pb::host_sha;
  if(capture.attachments_enabled)log<<"\",\"experiment\":\"projection-attachments-readonly\",\"jitter_writes\":0,\"window_ms\":2000,\"max_barriers\":64,\"max_resources\":9";
  else if(jitter)log<<"\",\"experiment\":\"projection-binding-stimulus\",\"injection_budget\":1,\"cpu_samples\":3,\"window_ms\":2000,\"max_recovery_deferrals\":64";
  else log<<"\",\"experiment\":\"projection-binding-readonly\",\"jitter_writes\":0";
  log<<",\"gpu_commands_added\":0}\n"
     <<"{\"event\":\"trampoline_reservation\",\"ready\":true,\"origin\":"<<pool.origin<<",\"slot\":"<<pool.slot<<",\"page\":"<<pool.page
     <<",\"policy_available\":true,\"policy_flags\":"<<pool.policy_flags<<",\"status\":0,\"native_targets\":10,\"block_bytes\":4096,\"free_slots\":62,\"hooks_installed\":false}\n"
     <<"{\"event\":\"ready\",\"hooks_installed\":false,\"pool_ready\":true}\n"
     <<"{\"event\":\"armed\",\"graphics_api\":\"D3D12\",\"live_spans\":"<<(capture.attachments_enabled?39:36)<<",\"backend\":"<<rt.backend<<",\"device\":"<<rt.device
     <<",\"cpu_resource\":"<<rt.cpu_resource<<",\"gpu_resource\":"<<rt.gpu_resource<<"}\n";
  auto prefix=path;prefix.replace_extension();pb::write_capture(log,capture,prefix.wstring());
  log<<"{\"event\":\"capture_complete\",\"stop\":2,\"phase\":4,\"fault\":0,\"pending\":0,\"drained\":true,\"drops\":0,\"gpu_contents_verified\":false,\"submission_verified\":false,\"raster_jitter_verified\":false}\n"
     <<"{\"event\":\"stopped\",\"fatal\":false,\"hooks_disabled\":true,\"pending\":0}\n";
  check(static_cast<bool>(log),"fixture log write");
}
void policy(){
  pb::Arena a{};a.mode=2;a.stride=4096;a.index=1;
  for(unsigned i=0;i<3;++i){a.cpu[i]=0x10000+4096*i;a.gpu[i]=0x10000000+4096*i;a.used[i]=4096;}
  auto p=pb::locate(a,0x10100,1072);check(p.valid && p.arena==0 && p.aligned_bytes==1280 && p.gpu==0x10000100,"consumed != current index");
  check(!pb::locate(a,0x10101,1072).valid,"unaligned");check(!pb::locate(a,0x10f00,1072).valid,"cross arena");
  a.used[0]=512;check(!pb::locate(a,0x10100,1072).valid,"unallocated tail");a.used[0]=4096;
  a.cpu[0]=UINTPTR_MAX-255;check(!pb::locate(a,a.cpu[0],256).valid,"overflow");
  check(pb::dxbc_valid(blob.data(),44),"valid structural blob");put(address(blob),40,UINT32_MAX);check(!pb::dxbc_valid(blob.data(),44),"chunk overflow");
  check(!pb::copy_memory(UINTPTR_MAX-2,blob.data(),16),"read overflow");check(!pb::copy_memory(image,blob.data(),16),"noaccess");
}
bool exception(){__try {invoke(pb::producer,camera+0x50,context);return false;}
  __except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}}
bool draw_exception(){__try {invoke(pb::draw0,address(executor),address(backend_context),0x2222);return false;}
  __except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}}
}
extern "C" U binding_fixture_body(U a,U b,U c,U d,U e,U f,U g,U kind){
  ++calls;
  if(mode.starts_with("attach_") && kind>=pb::state_setup)return attachment_body(a,b,c,d,e,f,g,kind);
  if(mode=="forwarding") {if(a!=0x11 || b!=0x22 || c!=0x33 || d!=0x44 || e!=0x55 || f!=0x66 || g!=0x77 || GetLastError()!=0x1111)++bad;
    SetLastError(0x2222);return 0x1122334455667788ULL;}
  if(kind==pb::producer){
    if(mode.starts_with("jitter_")) {
      std::uint64_t jitter{};std::memcpy(&jitter,reinterpret_cast<void *>(camera+0x38),8);
      if(jitter) {
        ++original_jitter_calls;
        if(mode=="jitter_exception" || (mode=="jitter_sequence_exception" && original_jitter_calls==2)){SetLastError(0x3333);RaiseException(0xe0424242,0,0,nullptr);}
        if(mode=="jitter_sequence_stop" && original_jitter_calls==2)pb::stop();
        if(mode=="jitter_conflict" || (mode=="jitter_sequence_conflict" && original_jitter_calls==2))put(camera,0x38,std::uint64_t{1234});
        if(mode=="jitter_provider" || (mode=="jitter_sequence_provider" && original_jitter_calls==2))put(address(backend),0x42700,U{1234});
        if(mode=="jitter_exit_lock")check(pb::hold_owned(pb::consume,false),"hold exit contention");
        if(mode=="jitter_pending")while(!release_original.load())SwitchToThread();
        const auto x=std::bit_cast<float>(static_cast<unsigned>(jitter))/2560.F;
        const auto y=std::bit_cast<float>(static_cast<unsigned>(jitter>>32))/1440.F;
        for(const auto word:{0U,32U}) {put(constants,4*(word+12),bits(x));put(constants,4*(word+13),bits(y));}
      }
      return 1;
    }
    if(mode=="stop_pending")while(!release_original.load())SwitchToThread();
    if(mode=="exception"){SetLastError(0x3333);RaiseException(0xe0424242,0,0,nullptr);}return 1;
  }
  if(kind==pb::api_draw && mode=="draw_exception") {SetLastError(0x3333);RaiseException(0xe0424242,0,0,nullptr);}
  if(kind==pb::publish){put(context,0x1a8,address(packet));put(address(packet),0,11U);put(address(packet),0x10,1U);put(address(packet),0x18,constants);put(address(packet),0x20,0U);}
  if(kind==pb::consume) {U published{};std::memcpy(&published,packet.data()+0x18,8);put(address(backend_context),0x1860,published);}
  if(kind==pb::rollover) {
    unsigned index{};std::memcpy(&index,backend.data()+0x103c,4);put(address(backend),0x103c,(index+1)%3);
  }
  if(kind==pb::draw0){
    if(mode.starts_with("jitter_")) {
      const std::array<float,6> viewport={0,0,2560,1440,0,1};
      invoke(pb::viewport,reinterpret_cast<U>(&command_object),1,reinterpret_cast<U>(viewport.data()));
    }
    invoke(pb::state_setup,a,b,address(graphics),c);invoke(pb::api_draw,reinterpret_cast<U>(&command_object),3,1,0,0);
  }
  if(kind==pb::state_setup && mode!="cached") {
    U bound_gpu=gpu;
    if(mode.starts_with("jitter_")) {std::memcpy(&bound_gpu,packet.data()+0x18,8);bound_gpu+=0x100000000ULL;invoke(pb::root_signature,reinterpret_cast<U>(&command_object),0x22220000);}
    invoke(pb::root_cbv,reinterpret_cast<U>(&command_object),2,bound_gpu);
  }
  if(kind==pb::state_setup && mode=="root_changed")put(shader_set,0x10,0x10000U);
  return 0x1122334455667788ULL;
}
#include "projection_sequence_fixture.inl"
int main(int argc,char **argv){
  try {
    mode=argc>1?argv[1]:"chain";
    if(mode=="resource_sdk"){resource_sdk(argc>2?argv[2]:nullptr);return 0;}
    setup();if(mode=="policy"){policy();std::cout<<"pass: policy\n";return 0;}
    if(mode=="contract_policy") {
      check(pb::root_variant(0x10000,0x10000,1,false,false)==1,"compact root");
      check(pb::root_variant(0,0x10000,1,false,false)==0,"both flags required");
      check(pb::root_variant(0,0,0,false,false)==2,"VS-only root");
      check(pb::root_variant(0x10000,0x10000,0,true,false)==3,"GS precedence");
      check(pb::root_variant(0x10000,0x10000,0,true,true)==4,"HS precedence");
      const std::array<unsigned,5> ids={0xf00,0x10f00,0x20f00,0xc8f00,0xd8f00};
      check(pb::matrix_layout(ids,268),"matrix layout");check(!pb::matrix_layout(ids,231),"matrix extent");
      auto broken=ids;broken[1]=0x8f00;check(!pb::matrix_layout(broken,268),"matrix overlap");
      broken=ids;broken[1]=UINT32_MAX;check(!pb::matrix_layout(broken,268),"missing matrix id");
      std::cout<<"pass: contract_policy\n";return 0;
    }
    AddressPressure pressure;
    if(mode=="pool_starved") {
      check(MH_Initialize()==MH_OK,"control initialize");pressure.fill();void *original{};
      check(MH_CreateHook(reinterpret_cast<void *>(targets[0]),reinterpret_cast<void *>(targets[1]),&original)==MH_ERROR_MEMORY_ALLOC,
        "unreserved negative control unexpectedly allocated");
      check(original==nullptr && MH_Uninitialize()==MH_OK,"negative control cleanup");
      std::cout<<"pass: pool_starved\n";return 0;
    }
    if(mode=="pool_unhooked") {
      std::array<std::array<std::byte,64>,pb::kind_count> before{};
      for(unsigned i=0;i<pb::kind_count;++i)std::memcpy(before[i].data(),reinterpret_cast<void *>(targets[i]),64);
      check(pb::install_owned(image,rt,targets),"reserve without arm");
      check(pb::pool_state().ready && pb::status().stop!=pb::Stop::recording,"reservation readiness");
      for(unsigned i=0;i<pb::kind_count;++i)check(!std::memcmp(before[i].data(),reinterpret_cast<void *>(targets[i]),64),"reservation patched code");
      check(!pb::install_owned(image,rt,targets),"second reservation accepted");
      invoke(pb::producer,camera+0x50,context);check(calls==1 && pb::status().phase==pb::Phase::empty,"reservation installed hook");
      check(pb::remove_owned(),"unarmed cleanup");std::cout<<"pass: pool_unhooked\n";return 0;
    }
    if(mode=="unsafe_prefix") {
      targets[0]=reinterpret_cast<U>(&binding_fixture_unsafe);check(pb::install_owned(image,rt,targets),"unsafe prepare");std::string why;
      check(!pb::arm(why) && pb::install_status().stage==4 && pb::status().disabled,"unsafe prefix accepted");
      std::cout<<"pass: unsafe_prefix\n";return 0;
    }
    if(mode.starts_with("attach_"))setup_attachments();
    check(pb::install_owned(image,rt,targets,mode.starts_with("jitter_"),mode.starts_with("attach_"),mode.starts_with("jitter_sequence_")),"prepare fixture");
    if(mode=="pool_exhausted")pressure.fill();
    std::string reason;check(pb::arm(reason),reason.c_str());check(!pb::arm(reason),"rearm");
    if(mode.starts_with("jitter_sequence_")){sequence_case(argc,argv);std::cout<<"pass: "<<mode<<'\n';return 0;}
    if(mode.starts_with("attach_")) {
      invoke(pb::producer,camera+0x50,context);invoke(pb::publish,context,0);
      invoke(pb::consume,address(executor),address(backend_context),address(packet));
      check(pb::status().phase==pb::Phase::consumed,"attachment source chain");
      if(mode=="attach_exception")check(attachment_exception(),"attachment exception forwarding");
      else invoke(pb::draw0,address(executor),address(backend_context),address(attachment_packet));
      check(pb::settled(),"attachment did not stop");
      const bool accept=mode=="attach_depth" || mode=="attach_color_depth" || mode=="attach_no_depth" || mode=="attach_null_color" ||
        mode=="attach_cached" || mode=="attach_same_handles" || mode=="attach_created" || mode=="attach_identity_adjusted";
      check(accept?(pb::status().stop==pb::Stop::complete && pb::status().fault==pb::Fault::none):pb::status().fault!=pb::Fault::none,"attachment verdict");
      check(pb::disable(),"attachment disable");const auto capture=pb::finished_capture();
      check(capture && !capture->stimulus.enabled && capture->draw_count==1,"attachment bounds");
      std::uint64_t jitter{};std::memcpy(&jitter,reinterpret_cast<void *>(camera+0x38),8);check(!jitter,"readonly mode wrote camera");
      check(resource0.refs==1 && resource1.refs==1 && device_refs==0,"resource reference leak");
      check(query_lock_ok,"COM call held bookkeeping lock");
      if(accept) {
        const auto &x=capture->draws[0].attachments;check(x.described && x.at_draw && x.stable && x.resources_released,"attachment evidence");
        check(x.resource_count==(mode=="attach_color_depth"?2U:1U),"resource deduplication");
        if(mode=="attach_no_depth")check(!x.ready.views[8].resource && x.ready.cached_depth_resource==0xdeadbeef,"stale depth reused");
        check(capture->draws[0].creation.succeeded==(mode=="attach_created"),"cache promoted to creation");
        if(mode=="attach_identity_adjusted")check(x.resources[0].identity!=x.resources[0].resource,"adjusted fixture is not adjusted");
        for(unsigned phase=0;phase<3;++phase) {
          check(x.resource_check_counts[phase]==x.resource_count,"missing query phase");
          for(unsigned i=0;i<x.resource_count;++i)check(x.resource_checks[phase][i].checkpoint==15 && x.resource_checks[phase][i].temporaries_released,"query incomplete");
        }
      } else {
        const auto &d=capture->draws[0];
        const bool late_identity=mode=="attach_identity_draw_change" || mode=="attach_identity_return_change";
        const auto expected=mode=="attach_rotation" || mode=="attach_count" || mode.starts_with("attach_create_")?pb::Fault::layout:
          mode=="attach_interface" || mode=="attach_device" || (mode.starts_with("attach_identity_") && !late_identity)?pb::Fault::identity:
          mode=="attach_exception"?pb::Fault::exception:
          mode=="attach_split" || mode=="attach_overflow" || mode=="attach_total_overflow"?pb::Fault::overflow:pb::Fault::changed;
        check(pb::status().fault==(mode=="attach_create_output"?pb::Fault::changed:expected),"wrong attachment rejection reason");
        if(mode!="attach_rotation")check(d.attachments.entry.valid,"rejection did not reach attachment gate");
        if(mode!="attach_rotation" && mode!="attach_count")check(d.state_seen,"rejection did not reach state setup");
        if(mode!="attach_rotation" && mode!="attach_count" && mode!="attach_interface" && mode!="attach_device" && (!mode.starts_with("attach_identity_") || late_identity))
          check(d.attachments.described,"rejection did not reach resource description");
        if(mode.starts_with("attach_identity_")) {
          const auto phase=mode=="attach_identity_draw_change"?1U:mode=="attach_identity_return_change"?2U:0U;
          const auto index=mode=="attach_identity_partial_fail"?1U:0U;
          check(d.attachments.resource_check_counts[phase]>index,"failed query not retained");
          const auto &q=d.attachments.resource_checks[phase][index];
          const unsigned checkpoint=late_identity?15:mode=="attach_identity_mismatch"?8:mode=="attach_identity_roundtrip"?10:
            mode=="attach_identity_roundtrip_mismatch"?12:mode=="attach_identity_self_hr"?7:mode=="attach_identity_roundtrip_hr"?9:6;
          check(q.checkpoint==checkpoint && q.temporaries_released,"wrong query diagnostic");
          if(mode=="attach_identity_partial_fail")check(d.attachments.resource_count==1,"partial ownership fixture not reached");
        }
      }
      check(pb::remove_owned(),"attachment hook cleanup");
      if(accept && argc>2)fixture_log(argv[2],*capture);
      std::cout<<"pass: "<<mode<<'\n';return 0;
    }
    if(mode.starts_with("jitter_")) {
      next_jitter_frame(0);invoke(pb::producer,camera+0x50,context);
      check(pb::status().phase==pb::Phase::empty && pb::status().fault==pb::Fault::none,"jitter baseline");
      invoke(pb::producer,camera+0x50,context);check(pb::status().fault==pb::Fault::none,"same frame repeat");
      next_jitter_frame(1);
      if(mode=="jitter_changed")put(image,0x51c4368,address(table)+8);
      if(mode=="jitter_zero")put(address(table),0xe6a0+8*11,std::uint64_t{});
      if(mode=="jitter_readonly") {DWORD old{};check(VirtualProtect(camera_memory.p,4096,PAGE_READONLY,&old)!=FALSE,"camera readonly");}
      if(mode=="jitter_exception") {check(exception(),"jitter exception propagation");check(GetLastError()==0x3333,"jitter exception LastError");}
      else if(mode=="jitter_pending") {
        // Stop while the original stimulus is running on the same producer
        // thread used for baseline: a helper only signals stop/release.
        std::thread helper([]{while(calls.load()<3)SwitchToThread();pb::stop();check(!pb::settled(),"stimulus pending drain");release_original=true;});
        invoke(pb::producer,camera+0x50,context);helper.join();
      } else invoke(pb::producer,camera+0x50,context);
      if(mode=="jitter_exit_lock")pb::release_owned();
      std::uint64_t restored{};std::memcpy(&restored,reinterpret_cast<void *>(camera+0x38),8);
      check(restored==(mode=="jitter_conflict"?1234U:0U),"camera restoration/competing value");
      if(mode!="jitter_chain" && mode!="jitter_recovery_first" && mode!="jitter_recovery_changed" &&
         mode!="jitter_deferred" && mode!="jitter_deferral_cap" && mode!="jitter_consume_collision") {
        check(pb::settled(),"rejected stimulus not settled");
        check(mode=="jitter_pending"?pb::status().stop==pb::Stop::requested:pb::status().fault!=pb::Fault::none,"invalid stimulus accepted");
        const auto c=pb::finished_capture();check(c && c->stimulus.injections<=1,"stimulus budget");
        if(c->stimulus.injections && mode!="jitter_conflict")check(c->stimulus.restorations==1,"missed restoration");
        check(pb::remove_owned(),"stimulus failure cleanup");std::cout<<"pass: "<<mode<<'\n';return 0;
      }
      check(pb::status().phase==pb::Phase::source,"stimulus source");
      invoke(pb::publish,context,0);
      if(mode=="jitter_consume_collision")check(pb::hold_owned(pb::producer,true),"hold stimulus consumer lock");
      invoke(pb::consume,address(executor),address(backend_context),address(packet));
      if(mode=="jitter_consume_collision") {
        pb::release_owned();check(pb::status().fault==pb::Fault::contention && pb::status().drops==1,"consumer contention incorrectly deferred");
        check(pb::remove_owned(),"consumer collision cleanup");std::cout<<"pass: "<<mode<<'\n';return 0;
      }
      if(mode=="jitter_deferred" || mode=="jitter_deferral_cap") {
        next_jitter_frame(2);check(pb::hold_owned(pb::consume,false),"hold recovery collision");
        for(unsigned i=0;i<(mode=="jitter_deferral_cap"?65U:1U);++i)invoke(pb::producer,camera+0x50,context);
        pb::release_owned();check(pb::status().fault==pb::Fault::none && pb::status().drops==0,"recovery observation collision rejected binding");
        if(mode=="jitter_deferral_cap") {
          check(pb::status().stop==pb::Stop::capacity && pb::settled(),"unbounded recovery deferrals");
          const auto c=pb::finished_capture();check(c && c->stimulus.recovery_deferrals==65 && c->stimulus.count==2 &&
            c->stimulus.injections==1 && c->stimulus.restorations==1,"deferral cap altered mutation budget");
          check(pb::remove_owned(),"deferral cap cleanup");std::cout<<"pass: "<<mode<<'\n';return 0;
        }
      }
      check(pb::provenance::owned_draw_return_calls()==0,"draw return published before draw");
      if(mode=="jitter_recovery_first") {next_jitter_frame(2);invoke(pb::producer,camera+0x50,context);check(pb::status().stop==pb::Stop::recording,"premature CPU-only completion");}
      check(pb::provenance::owned_draw_return_calls()==0,"CPU recovery fabricated draw return");
      invoke(pb::draw0,address(executor),address(backend_context),0x2222);
      // A copy-list Execute may run now, before the later recovery call. Its
      // selected-draw check needs the actual returned draw, not CPU completion.
      check(pb::provenance::owned_draw_return_calls()==1,"draw return delayed until CPU recovery");
      if(mode!="jitter_recovery_first") {
        check(pb::status().stop==pb::Stop::recording,"premature draw-only completion");next_jitter_frame(mode=="jitter_deferred"?3:2);
        if(mode=="jitter_recovery_changed")put(context,0x230,bits(1920.F));
        invoke(pb::producer,camera+0x50,context);
      }
      if(mode=="jitter_recovery_changed") {check(pb::status().fault==pb::Fault::changed && pb::remove_owned(),"changed recovery accepted");std::cout<<"pass: "<<mode<<'\n';return 0;}
      check(pb::status().stop==pb::Stop::complete && pb::status().fault==pb::Fault::none && pb::settled(),"stimulus chain completion");
      check(pb::provenance::owned_draw_return_calls()==1,"CPU recovery republished draw return");
      const auto c=pb::finished_capture();check(c && c->stimulus.count==3 && c->stimulus.injections==1 && c->stimulus.restorations==1,"stimulus count");
      check(c->source.anchor.frame==11 && c->stimulus.calls[0].source.anchor.frame==10 &&
        c->stimulus.calls[2].source.anchor.frame==(mode=="jitter_deferred"?13U:12U),"stimulus frames");
      check(c->stimulus.recovery_deferrals==(mode=="jitter_deferred"?1U:0U),"recovery deferral accounting");
      check(pb::remove_owned(),"stimulus cleanup");if(argc>2)fixture_log(argv[2],*c,true);
      std::cout<<"pass: "<<mode<<'\n';return 0;
    }
    if(mode=="matrix_layout" || mode=="matrix_nonfinite") {
      if(mode=="matrix_layout")put(image,0x4be75f8,0x20f00U);else put(constants,0,0x7fc00000U);
      invoke(pb::producer,camera+0x50,context);
      check(pb::status().fault==pb::Fault::layout && pb::settled() && pb::remove_owned(),"invalid matrix admitted");
      std::cout<<"pass: "<<mode<<'\n';return 0;
    }
    if(mode=="forwarding"){
      std::array<U,2> out{};SetLastError(0x1111);binding_fixture_returns(functions[pb::api_indirect],&out);
      check(out[0]==0x1122334455667788ULL && out[1]==out[0] && GetLastError()==0x2222 && bad==0 && calls==1,"ABI preservation");pb::stop();
    }else if(mode=="exception"){
      check(exception(),"exception propagation");check(GetLastError()==0x3333,"exception LastError");check(pb::status().fault==pb::Fault::exception && pb::settled(),"exception cleanup");
    }else if(mode=="deadline"){
      pb::expire_owned();invoke(pb::producer,camera+0x50,context);check(pb::status().phase==pb::Phase::empty && calls==1,"post deadline forwarding");
    }else if(mode=="stop_pending"){
      std::thread t([]{invoke(pb::producer,camera+0x50,context);});while(!calls.load())SwitchToThread();pb::stop();check(!pb::settled(),"premature drain");
      release_original=true;t.join();check(pb::settled() && pb::status().pending==0,"drain");
    }else{
      if(mode=="unrelated_lock") {
        check(pb::hold_owned(pb::producer,true),"hold source test lock");
        std::thread t([]{invoke(pb::producer,camera+0x60,context);});t.join();pb::release_owned();
        check(pb::status().fault==pb::Fault::none && pb::status().phase==pb::Phase::empty,"unrelated producer entered lock");
      }
      invoke(pb::producer,camera+0x50,context);check(pb::status().phase==pb::Phase::source,"source");
      if(mode=="matrix_id_changed") {
        put(image,0x4be75f8,0xf01U);invoke(pb::publish,context,0);
        check(pb::status().fault==pb::Fault::changed && pb::settled() && pb::remove_owned(),"changed matrix ID admitted");
        std::cout<<"pass: matrix_id_changed\n";return 0;
      }
      if(mode=="reuse") {
        put(address(backend),0x103c,2U);invoke(pb::rollover,address(backend));
        check(pb::status().fault==pb::Fault::changed && pb::settled(),"arena reuse accepted");
        check(pb::remove_owned(),"reuse remove");std::cout<<"pass: reuse\n";return 0;
      }
      if(mode=="changed")put(constants,200,0xdeadbeefU);
      if(mode=="unrelated_lock") {
        check(pb::hold_owned(pb::producer,true),"hold publication test lock");
        std::thread t([]{invoke(pb::publish,context,1234);});t.join();pb::release_owned();
        check(pb::status().fault==pb::Fault::none && pb::status().phase==pb::Phase::source,"unrelated publication entered lock");
      }
      invoke(pb::publish,context,0);
      if(mode=="changed"){check(pb::status().fault==pb::Fault::changed && calls==2,"changed forwarded");}
      else {
        check(pb::status().phase==pb::Phase::published,"publication");
        if(mode=="unrelated_lock" || mode=="relevant_lock" || mode=="rollover_parallel") {
          check(pb::hold_owned(pb::producer,true),"hold consume test lock");
          std::thread t([]{
            if(mode=="rollover_parallel")invoke(pb::rollover,address(backend));
            else invoke(pb::consume,address(executor),address(backend_context),address(packet)+(mode=="unrelated_lock"?16:0));
          });t.join();pb::release_owned();
          const auto s=pb::status();
          if(mode=="relevant_lock") {
            check(s.fault==pb::Fault::contention && s.drops==1 && s.contention_site==2*(pb::consume+1) &&
              s.owner_site==2*(pb::producer+1)+1 && s.owner_thread==GetCurrentThreadId() && s.contention_thread!=s.owner_thread,
              "relevant contention not diagnosed/rejected");
            check(pb::settled() && pb::remove_owned(),"contention cleanup");std::cout<<"pass: relevant_lock\n";return 0;
          }
          check(s.fault==pb::Fault::none && s.drops==0 && s.phase==pb::Phase::published,"unrelated/lifetime guard collided with capture lock");
        }
        if(mode=="foreign") {
          invoke(pb::consume,address(executor),address(backend_context),address(packet)+16);check(pb::status().phase==pb::Phase::published,"foreign packet accepted");pb::stop();
        }else {
          invoke(pb::consume,address(executor),address(backend_context),address(packet));check(pb::status().phase==pb::Phase::consumed,"consumption");
          if(mode=="unrelated_lock") {
            check(pb::hold_owned(pb::producer,true),"hold draw test lock");
            std::thread t([]{invoke(pb::draw0,address(executor),address(backend_context)+8,0x2222);});t.join();pb::release_owned();
            check(pb::status().fault==pb::Fault::none && pb::status().phase==pb::Phase::consumed,"unrelated draw entered lock");
          }
          if(mode=="draw_exception") {
            check(draw_exception(),"nested exception propagated");check(GetLastError()==0x3333 && pb::settled() && !pb::status().pending,"nested cleanup");
            check(pb::status().fault==pb::Fault::exception && pb::remove_owned(),"nested fault/removal");
            check(pb::provenance::owned_draw_return_calls()==0,"exception published draw return");std::cout<<"pass: draw_exception\n";return 0;
          }
          invoke(pb::draw0,address(executor),address(backend_context),0x2222);
          if(mode=="root_changed" || mode=="root_wrong") {
            check(pb::status().fault==pb::Fault::changed && pb::settled() && pb::remove_owned(),"invalid root accepted");
            check(pb::provenance::owned_draw_return_calls()==0,"invalid root published draw return");
            std::cout<<"pass: "<<mode<<'\n';return 0;
          }
          check(pb::status().stop==pb::Stop::complete && pb::status().fault==pb::Fault::none && pb::settled(),"complete draw");
          const auto capture=pb::finished_capture();check(capture && capture->draw_count==1 && capture->blob_count==1,"capture bounded");
          const auto &draw=capture->draws[0];check(draw.returned && draw.state_seen && draw.source_unchanged && draw.draw_api_observed,"draw evidence");
          check(draw.root_binding_observed==(mode!="cached"),"cache incorrectly promoted");
          if(argc>2) {
            const std::filesystem::path path(argv[2]);check(!std::filesystem::exists(path),"preserve fixture artifact");
            std::ofstream log(path,std::ios::binary);log<<std::boolalpha;
            const auto &pool=pb::pool_state();
            log<<"{\"event\":\"start\",\"owned_fixture\":true,\"schema\":1,\"version\":\"0.1.3\",\"pid\":1,\"creation\":1,\"host_sha256\":\""<<pb::host_sha
               <<"\",\"experiment\":\"projection-binding-readonly\",\"jitter_writes\":0,\"gpu_commands_added\":0}\n"
               <<"{\"event\":\"trampoline_reservation\",\"ready\":true,\"origin\":"<<pool.origin<<",\"slot\":"<<pool.slot<<",\"page\":"<<pool.page
               <<",\"policy_available\":true,\"policy_flags\":"<<pool.policy_flags<<",\"status\":0,\"native_targets\":10,\"block_bytes\":4096,\"free_slots\":62,\"hooks_installed\":false}\n"
               <<"{\"event\":\"ready\",\"hooks_installed\":false,\"pool_ready\":true}\n"
               <<"{\"event\":\"armed\",\"graphics_api\":\"D3D12\",\"live_spans\":36,\"backend\":"<<rt.backend
               <<",\"cpu_resource\":"<<rt.cpu_resource<<",\"gpu_resource\":"<<rt.gpu_resource<<"}\n";
            auto prefix=path;prefix.replace_extension();pb::write_capture(log,*capture,prefix.wstring());
            log<<"{\"event\":\"capture_complete\",\"stop\":2,\"phase\":4,\"fault\":0,\"pending\":0,\"drained\":true,\"drops\":0,\"gpu_contents_verified\":false,\"submission_verified\":false,\"raster_jitter_verified\":false}\n"
               <<"{\"event\":\"stopped\",\"fatal\":false,\"hooks_disabled\":true,\"pending\":0}\n";
            check(static_cast<bool>(log),"fixture log write");
          }
        }
      }
    }
    check(pb::remove_owned(),"restore hooks");std::cout<<"pass: "<<mode<<'\n';return 0;
  }catch(const std::exception &e){std::cerr<<e.what()<<" phase="<<static_cast<unsigned>(pb::status().phase)<<" fault="<<static_cast<unsigned>(pb::status().fault)<<'\n';return 1;}
}
