#include "postfilter.hpp"
#include "draw_execution.hpp"
#include "../projection_binding/memory.hpp"
#include <windows.h>
#include <MinHook.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <ostream>
extern "C" void scaled_postfilter_entry();
extern "C" void mhw_binding_forward(void *,mhw::projection_binding::Frame *);
namespace mhw::scaled_temporal::postfilter {
namespace {
namespace tb=temporal_bindings;namespace pb=projection_binding;
constexpr unsigned capacity=3,command_limit=32,shader_limit=262144;
constexpr unsigned target_rva=0x24ca1f0,caller_rva=0x24f228b;
struct Geometry {
  std::uintptr_t packet{},state{},arena{},vertices{},graphics{};
  unsigned count{},first{},offset{},stride{};
  std::array<float,16> values{};
  bool operator==(const Geometry &)const=default;
};
struct Command {std::uintptr_t packet{};unsigned key{},opcode{};};
struct Shader {
  std::array<std::byte,shader_limit> shader{};unsigned shader_size{},technique{};
  std::uintptr_t shader_set{},shader_data{};bool saved{};std::string file,sha;
};
struct ColorGrade:Shader {
  Geometry geometry{};tb::Owner input{},output{};std::uintptr_t unit{},endpoint{};
  unsigned command_index{},queue_count{};bool selected{},stable{};
};
struct Record:Shader {
  tb::Snapshot stamp{};std::uintptr_t unit{},caller{};unsigned thread{},before{},after{},command_count{};
  tb::Owner input{},output{},final_source{},final_destination{};Geometry final_geometry{},filter_geometry{};
  std::array<unsigned,4> properties{},rectangle{},returned_rectangle{};
  std::array<Command,command_limit> commands{};
  ColorGrade grade{};
  bool selected{},entered{},returned{},valid{};
};
tb::Config cfg{};std::array<Record,capacity> records{};
std::atomic<unsigned> state{},pending{},fault{},published{},completed{};
std::atomic_flag busy=ATOMIC_FLAG_INIT;
void *original{};std::uintptr_t target{},caller{};bool installed{},disabled{},drained{};
bool read(void *,std::uintptr_t at,std::span<std::byte> out){return bridge_probe::read_bounded_scalar(nullptr,at,out)==bridge_probe::ScalarReadResult::success;}
const tb::Reader reader{nullptr,read};
bool pointer(std::uintptr_t p,std::uintptr_t n){return p&&p<=UINTPTR_MAX-n;}
template<class T,std::size_t N>bool array(std::uintptr_t at,std::array<T,N> &v){
  if(!pointer(at,sizeof(v)))return false;for(unsigned i=0;i<N;++i)if(!reader.get(at+sizeof(T)*i,v[i]))return false;return true;
}
void fail(unsigned n){unsigned zero{};fault.compare_exchange_strong(zero,n);state.store(2,std::memory_order_release);}
bool stamp(const Record &r){
  std::uint64_t frame{},epoch{};unsigned bank{};unsigned char async{};std::uintptr_t view{},primary{},type{};
  const auto &s=r.stamp;
  return s.context==cfg.renderer+0x710&&reader.get(cfg.renderer+0x1f210,frame)&&frame==s.frame&&
    reader.get(cfg.renderer+0x1f208,epoch)&&epoch==s.epoch&&reader.get(cfg.renderer+0x1f200,bank)&&bank==s.bank&&
    reader.get(cfg.renderer+0x93,async)&&async==s.async&&reader.get(s.context+0x1d0,view)&&view==s.view&&
    reader.get(cfg.renderer+0x1f450,primary)&&primary==view&&reader.get(r.unit,type)&&type==cfg.image+0x3537a70;
}
bool queue(std::uintptr_t context,unsigned &count,std::uintptr_t &at){
  return reader.get(context+0x2b80,count)&&count<65535&&reader.get(context+0x2b78,at)&&pointer(at,16ULL*(count+1));
}
bool geometry(std::uintptr_t context,std::uintptr_t packet,Geometry &g){
  g={};g.packet=packet;unsigned opcode{};unsigned char stride{};
  if(!pointer(packet,0x48)||!reader.get(packet,opcode)||(opcode&31)!=0||
     !reader.get(packet+0x18,g.state)||!pointer(g.state,0x50)||!reader.get(packet+0x20,g.graphics)||!pointer(g.graphics,0x20)||
     !reader.get(packet+0x40,g.count)||(g.count!=3&&g.count!=4)||!reader.get(packet+0x44,g.first)||g.first>1048576||
     !reader.get(g.state+0x20,stride)||stride!=16||!reader.get(g.state+0x24,g.offset)||g.offset>16777216||
     !reader.get(context+0x2b50,g.arena)||!pointer(g.arena,33554432))return false;
  g.stride=stride;g.vertices=g.arena+g.offset+static_cast<std::uintptr_t>(g.first)*stride;
  // Read exactly the referenced triangle/quad, never the surrounding vertex arena.
  for(unsigned i=0;i<4*g.count;++i)if(!reader.get(g.vertices+4*i,g.values[i])||!std::isfinite(g.values[i]))return false;
  return true;
}
bool shader(Shader &r,const Geometry &g){
  const auto graphics=g.graphics;std::uintptr_t handle{},entry{},desc{};
  if(!reader.get(graphics+8,handle)||!pointer(handle,8)||!reader.get(handle,r.shader_set)||!pointer(r.shader_set,0x90)||
     !reader.get(graphics+0x18,r.technique)||!reader.get(r.shader_set+0x58,entry)||!pointer(entry,0x18)||
     !reader.get(entry+0x10,desc)||!pointer(desc,16)||!reader.get(desc,r.shader_size)||r.shader_size<52||r.shader_size>shader_limit||
     !reader.get(desc+8,r.shader_data)||!pointer(r.shader_data,r.shader_size)||
     !pb::copy_memory(r.shader_data,r.shader.data(),r.shader_size)||!pb::dxbc_valid(r.shader.data(),r.shader_size))return false;
  const auto word=[&](unsigned at){unsigned v{};std::memcpy(&v,r.shader.data()+at,4);return v;};unsigned programs{};
  for(unsigned i=0;i<word(28);++i){const auto at=word(32+4*i);
    if(std::memcmp(r.shader.data()+at,"SHDR",4)&&std::memcmp(r.shader.data()+at,"SHEX",4))continue;
    if(++programs!=1||word(at+4)<8||(word(at+4)&3)||word(at+8)!=0x50||word(at+12)!=word(at+4)/4)return false;
  }
  std::array<std::byte,256> chunk{};
  for(unsigned at=0;at<r.shader_size;at+=256){const auto n=(std::min)(256U,r.shader_size-at);
    if(!pb::copy_memory(r.shader_data+at,chunk.data(),n)||std::memcmp(chunk.data(),r.shader.data()+at,n))return false;}
  std::uintptr_t same{};unsigned size{};
  return programs==1&&reader.get(graphics+8,same)&&same==handle&&reader.get(handle,same)&&same==r.shader_set&&
    reader.get(r.shader_set+0x58,same)&&same==entry&&reader.get(entry+0x10,same)&&same==desc&&
    reader.get(desc,size)&&size==r.shader_size&&reader.get(desc+8,same)&&same==r.shader_data;
}
bool grade_stable(const Record &r){
  const auto &g=r.grade;std::uintptr_t in{},out{},type{};tb::Owner a{},b{};Geometry geometry_now{};
  return g.selected&&reader.get(g.unit,type)&&type==cfg.image+0x3538400&&
    reader.get(g.unit+0x248,in)&&in==g.input.object&&reader.get(g.unit+0x250,out)&&out==g.output.object&&
    tb::read_owner(reader,in,a)&&a==g.input&&tb::read_owner(reader,out,b)&&b==g.output&&
    geometry(r.stamp.context,g.geometry.packet,geometry_now)&&geometry_now==g.geometry;
}
bool color_grade(Record &r){
  auto &g=r.grade;std::uintptr_t filters{};unsigned count{},units{},endpoints{};
  if(!reader.get(cfg.image+0x51cc238,filters)||!pointer(filters,0xd0)||!reader.get(filters+0x3c,count)||!count||count>16)return false;
  for(unsigned i=0;i<count;++i){std::uintptr_t entry{},unit{},type{};unsigned stage{},order{};
    if(!reader.get(filters+0x40+8*i,entry)||!pointer(entry,0x23)||!reader.get(entry,stage)||!reader.get(entry+4,order)||!reader.get(entry+8,unit))return false;
    if(unit){if(!reader.get(unit,type))return false;if(type==cfg.image+0x3538400&&stage==15&&order==0){g.unit=unit;++units;}}
    if(stage==15&&order==0x7fffff1){std::array<unsigned char,3> flags{};
      if(unit||!array(entry+0x20,flags)||flags!=std::array<unsigned char,3>{1,0,0})return false;g.endpoint=entry;++endpoints;}
  }
  std::uintptr_t input{},output{},at{};
  if(units!=1||endpoints!=1||g.endpoint!=g.unit+0x238||!reader.get(g.unit+0x248,input)||!reader.get(g.unit+0x250,output)||
     !tb::read_owner(reader,input,g.input)||!tb::read_owner(reader,output,g.output)||g.output!=r.final_source||
     !queue(r.stamp.context,g.queue_count,at))return false;
  unsigned draws{};
  for(unsigned i=0;i<g.queue_count;++i){unsigned key{},opcode{};std::uintptr_t packet{};
    if(!reader.get(at+16ULL*i,key))return false;if(key!=0x7ffffff1)continue;
    if(!reader.get(at+16ULL*i+8,packet)||!pointer(packet,4)||!reader.get(packet,opcode))return false;
    if((opcode&31)!=0)continue;
    if(++draws!=1||!geometry(r.stamp.context,packet,g.geometry)||g.geometry.count!=4||!shader(g,g.geometry))return false;g.command_index=i;
  }
  g.selected=draws==1;return grade_stable(r);
}
unsigned before(const pb::Frame &f){
  if(state.load(std::memory_order_acquire)!=1||f.caller!=caller)return UINT_MAX;
  const auto i=completed.load();if(i>=published.load(std::memory_order_acquire)||i>=capacity)return UINT_MAX;
  auto &r=records[i];if(f.gpr[1]!=r.unit||f.gpr[2]!=r.stamp.context)return UINT_MAX;
  if(busy.test_and_set(std::memory_order_acquire)){fail(2);return UINT_MAX;}
  r.entered=true;r.caller=f.caller;std::uintptr_t input{},output{},at{};
  r.valid=r.selected&&r.thread==GetCurrentThreadId()&&stamp(r)&&
    reader.get(r.unit+0x150,input)&&reader.get(r.unit+0x158,output)&&
    tb::read_owner(reader,input,r.input)&&tb::read_owner(reader,output,r.output)&&
    array(r.unit+0x168,r.properties)&&array(r.stamp.context+0x5c,r.rectangle)&&queue(r.stamp.context,r.before,at);
  if(!r.valid)fail(3);return i;
}
void after(unsigned i,bool returned){
  if(i==UINT_MAX)return;auto &r=records[i];r.returned=returned;
  std::uintptr_t at{},input{},output{};tb::Owner in{},out{},final{};Geometry final_geometry{};
  r.valid=r.valid&&returned&&stamp(r)&&array(r.stamp.context+0x5c,r.returned_rectangle)&&r.returned_rectangle==r.rectangle&&
    reader.get(r.unit+0x150,input)&&input==r.input.object&&tb::read_owner(reader,input,in)&&in==r.input&&
    reader.get(r.unit+0x158,output)&&output==r.output.object&&tb::read_owner(reader,output,out)&&out==r.output&&
    tb::read_owner(reader,r.final_source.object,final)&&final==r.final_source&&
    geometry(r.stamp.context,r.final_geometry.packet,final_geometry)&&final_geometry==r.final_geometry&&
    queue(r.stamp.context,r.after,at)&&r.after>=r.before&&r.after-r.before<=command_limit;
  r.grade.stable=grade_stable(r)&&r.grade.input==r.output;r.valid=r.valid&&r.grade.stable;
  unsigned draws{};
  if(r.valid){r.command_count=r.after-r.before;
    for(unsigned j=0;j<r.command_count;++j){auto &c=r.commands[j];unsigned raw{};
      if(!reader.get(at+16ULL*(r.before+j),c.key)||!reader.get(at+16ULL*(r.before+j)+8,c.packet)||
         !pointer(c.packet,4)||!reader.get(c.packet,raw)){r.valid=false;break;}c.opcode=raw&31;
      if(c.opcode==0&&(c.key>>27)==15&&(c.key&0x7ffffff)==100){
        if(++draws!=1||!geometry(r.stamp.context,c.packet,r.filter_geometry)||!shader(r,r.filter_geometry)){r.valid=false;break;}
      }
    }
  }
  r.valid=r.valid&&draws==1;
  if(r.valid&&draw_execution::active()){
    tb::Owner destination{};
    r.valid=tb::read_owner(reader,r.final_destination.object,destination)&&destination==r.final_destination&&
      draw_execution::publish(i,0,r.stamp,r.filter_geometry.packet,r.output,std::span(r.filter_geometry.values).first(4*r.filter_geometry.count))&&
      draw_execution::publish(i,1,r.stamp,r.grade.geometry.packet,r.grade.output,std::span(r.grade.geometry.values).first(4*r.grade.geometry.count))&&
      draw_execution::publish(i,2,r.stamp,r.final_geometry.packet,destination,std::span(r.final_geometry.values).first(4*r.final_geometry.count));
  }
  if(!r.valid)fail(returned?4:5);
  completed.store(i+1,std::memory_order_release);busy.clear(std::memory_order_release);
}
template<class T,std::size_t N>void values(std::ostream &o,const std::array<T,N> &v){o<<'[';for(unsigned i=0;i<N;++i){if(i)o<<',';o<<+v[i];}o<<']';}
void owner_json(std::ostream &o,const tb::Owner &v){o<<"{\"owner\":"<<v.object<<",\"resource\":"<<v.resource<<",\"width\":"<<v.width<<",\"height\":"<<v.height<<",\"format\":"<<v.format<<'}';}
void geometry_json(std::ostream &o,const Geometry &g){o<<"{\"packet\":"<<g.packet<<",\"state\":"<<g.state<<",\"arena\":"<<g.arena<<",\"vertices\":"<<g.vertices<<",\"graphics\":"<<g.graphics<<",\"count\":"<<g.count<<",\"first\":"<<g.first<<",\"offset\":"<<g.offset<<",\"stride\":"<<g.stride<<",\"values\":";values(o,g.values);o<<'}';}
bool save_shader(Shader &r,const std::filesystem::path &path){
  if(r.saved)return false;
  const auto file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file==INVALID_HANDLE_VALUE)return false;DWORD bytes{};
  const bool written=WriteFile(file,r.shader.data(),r.shader_size,&bytes,nullptr)&&bytes==r.shader_size&&FlushFileBuffers(file);
  const bool closed=CloseHandle(file)!=0;if(!written||!closed)return false;
  r.file=path.filename().string();r.sha=pb::bytes_sha256(r.shader.data(),r.shader_size);r.saved=r.sha.size()==64;return r.saved;
}
void shader_json(std::ostream &o,const Shader &s){o<<"\"technique\":"<<s.technique<<",\"shader_set\":"<<s.shader_set<<",\"shader_data\":"<<s.shader_data<<",\"shader_size\":"<<s.shader_size<<",\"saved\":"<<s.saved<<",\"file\":"<<std::quoted(s.file)<<",\"sha256\":"<<std::quoted(s.sha);}
}
void dispatch(pb::Frame *f){pending.fetch_add(1,std::memory_order_acq_rel);const auto i=before(*f);bool returned=false;
  __try{mhw_binding_forward(original,f);returned=true;}__finally{after(i,returned);pending.fetch_sub(1,std::memory_order_release);}}
bool select(unsigned i,const tb::Snapshot &s,std::uintptr_t unit,std::uintptr_t final_source,std::uintptr_t final_packet,const tb::Owner &final_destination) noexcept {
  if(!state.load(std::memory_order_acquire))return true;
  if(state.load()!=1||i>=capacity||i!=published.load()||i!=completed.load()){fail(1);return false;}
  auto &r=records[i];r.stamp=s;r.unit=unit;r.thread=GetCurrentThreadId();r.final_destination=final_destination;
  r.selected=stamp(r)&&tb::read_owner(reader,final_source,r.final_source)&&geometry(s.context,final_packet,r.final_geometry)&&color_grade(r);
  if(!r.selected){fail(1);return false;}published.store(i+1,std::memory_order_release);return true;
}
bool healthy() noexcept{return !fault.load(std::memory_order_acquire)&&draw_execution::healthy();}
bool complete() noexcept{return !state.load(std::memory_order_acquire)||(healthy()&&completed.load(std::memory_order_acquire)==capacity&&draw_execution::complete());}
bool settled() noexcept{return pending.load(std::memory_order_acquire)==0&&draw_execution::settled();}
void stop() noexcept{if(state.load())state.store(2,std::memory_order_release);draw_execution::stop();}
bool install(const tb::Config &c,std::string &reason){
  if(state.load()){reason="postfilter-already-used";return false;}cfg=c;target=c.image+target_rva;caller=c.image+caller_rva;
  if(!pb::verify_code(c.image,0x24ca1f0,792,"ace3852211fb63c13cfca395d4992f922addc69db0841fa44e540819779e60aa")||
     !pb::verify_code(c.image,0x23ca4a0,445,"97d8e82e322796881438a9318a4f5170401bf031b28d21afd19c77256a82816e")||
     !pb::verify_code(c.image,0x24d0780,4636,"5853f4dd6561e66842edab799f7d87ea3042f7b873dd72ab848e612579ff3e7b")){
    reason="postfilter-code";return false;}
  const auto create=MH_CreateHook(reinterpret_cast<void *>(target),reinterpret_cast<void *>(scaled_postfilter_entry),&original);
  if(create!=MH_OK){reason="postfilter-create-"+std::to_string(create);return false;}
  state.store(1,std::memory_order_release);const auto enable=MH_EnableHook(reinterpret_cast<void *>(target));
  if(enable!=MH_OK){fail(6);reason="postfilter-enable-"+std::to_string(enable);return false;}installed=true;return draw_execution::install(c,reason);
}
bool finish() noexcept{
  if(!state.load())return true;stop();const auto tick=GetTickCount64();while(!settled()&&GetTickCount64()-tick<2000)Sleep(1);
  drained=settled();if(!drained){fail(7);return false;}
  const auto status=installed?MH_DisableHook(reinterpret_cast<void *>(target)):MH_ERROR_DISABLED;
  disabled=status==MH_OK||status==MH_ERROR_DISABLED;while(!settled()&&GetTickCount64()-tick<2000)Sleep(1);
  const bool execution=draw_execution::finish();return disabled&&settled()&&healthy()&&completed.load()==capacity&&execution;
}
bool save(const std::wstring &prefix){
  if(!state.load())return true;if(!settled()||!disabled||!healthy()||completed.load()!=capacity)return false;
  for(unsigned i=0;i<capacity;++i){auto &r=records[i];
    if(!save_shader(r,prefix+L".postfilter-"+std::to_wstring(i)+L".ps.dxbc")||
       !save_shader(r.grade,prefix+L".colorgrade-"+std::to_wstring(i)+L".ps.dxbc"))return false;
  }return draw_execution::save(prefix);
}
void write(std::ostream &o){
  o<<"{\"schema\":2,\"policy\":\"postfilter-construction-v2\",\"fault\":"<<fault.load()<<",\"pending\":"<<pending.load()<<",\"selected\":"<<published.load()<<",\"completed\":"<<completed.load()<<",\"hooks_disabled\":"<<disabled<<",\"drained\":"<<drained<<",\"target\":"<<target<<",\"mutations\":0,\"records\":[";
  for(unsigned i=0;i<published.load()&&i<capacity;++i){if(i)o<<',';const auto &r=records[i];
    o<<"{\"index\":"<<i<<",\"frame\":"<<r.stamp.frame<<",\"epoch\":"<<r.stamp.epoch<<",\"thread\":"<<r.thread<<",\"context\":"<<r.stamp.context<<",\"view\":"<<r.stamp.view<<",\"unit\":"<<r.unit<<",\"caller\":"<<r.caller<<",\"valid\":"<<r.valid<<",\"entered\":"<<r.entered<<",\"returned\":"<<r.returned<<",\"count_before\":"<<r.before<<",\"count_after\":"<<r.after<<",\"rectangle\":";values(o,r.rectangle);
    o<<",\"returned_rectangle\":";values(o,r.returned_rectangle);o<<",\"properties_168\":";values(o,r.properties);
    o<<",\"input\":";owner_json(o,r.input);o<<",\"output\":";owner_json(o,r.output);o<<",\"final_source\":";owner_json(o,r.final_source);
    o<<",\"final_geometry\":";geometry_json(o,r.final_geometry);o<<",\"filter_geometry\":";geometry_json(o,r.filter_geometry);
    o<<',';shader_json(o,r);const auto &g=r.grade;
    o<<",\"colorgrade\":{\"unit\":"<<g.unit<<",\"endpoint\":"<<g.endpoint<<",\"queue_count\":"<<g.queue_count<<",\"command_index\":"<<g.command_index<<",\"selected\":"<<g.selected<<",\"stable\":"<<g.stable<<",\"input\":";owner_json(o,g.input);
    o<<",\"output\":";owner_json(o,g.output);o<<",\"geometry\":";geometry_json(o,g.geometry);o<<',';shader_json(o,g);o<<"},\"commands\":[";
    for(unsigned j=0;j<r.command_count;++j){if(j)o<<',';const auto &c=r.commands[j];o<<"{\"packet\":"<<c.packet<<",\"key\":"<<c.key<<",\"opcode\":"<<c.opcode<<'}';}o<<"]}";
  }o<<"],\"draw_execution\":";draw_execution::write(o);o<<'}';
}
#ifdef MHW_TEMPORAL_FIXTURE
void fixture_start(const tb::Config &c,void *forward,std::uintptr_t expected){cfg=c;original=forward;target=c.image+target_rva;caller=expected;state.store(1);}
#endif
}
extern "C" void mhw_scaled_postfilter(mhw::projection_binding::Frame *f){mhw::scaled_temporal::postfilter::dispatch(f);}
