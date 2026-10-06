#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <ostream>
#include <span>
#include <type_traits>

namespace mhw::scaled_temporal {
// Diagnostic CPU copies only. No COM calls, mutation, admission or GPU ownership.
// Keep this object off the stack: the bounded shader snapshots are deliberately large.
class OutputDependencies {
public:
 static constexpr unsigned capacity=2,draw_capacity=4,shader_capacity=16384,attempt_limit=16;
 struct Texture {
  // object, primary, secondary, resource, packed extent, engine format, GPU format
  std::array<std::uint64_t,7> fields{};
  bool operator==(const Texture &)const=default;
 };
 template<unsigned N>struct ShaderBytes {
  std::array<std::uint64_t,3> identity{};unsigned size{};
  std::array<unsigned char,N> bytes{};
  bool operator==(const ShaderBytes &)const=default;
 };
 using Shader=ShaderBytes<shader_capacity>;
 struct Range {
  std::array<unsigned short,2> range{};std::array<unsigned,128> words{};
  bool operator==(const Range &)const=default;
 };
 struct Constant {
  unsigned word{},size{};std::array<unsigned char,1072> bytes{};
  bool operator==(const Constant &)const=default;
 };
 struct Draw {
  std::uint64_t packet{},index{};
  std::array<unsigned,18> prefix{};
  // attachment, flags, depth, view, backend, resource, format, rotating
  std::array<std::uint64_t,8> target{};std::array<int,4> rectangle{};
  std::uint64_t shader_set{};unsigned owner_count{};
  std::array<std::uint64_t,64> objects{};std::array<unsigned,64> ids{};
  std::array<unsigned char,64> modes{};std::array<Texture,64> textures{};
  std::array<bool,64> texture_used{};std::array<Range,2> ranges{};
  std::array<Shader,2> shaders{};
  std::array<std::uint64_t,4> vertex{}; // IA, stride, offset, CPU arena
  unsigned vertex_bytes{},layout_count{};
  std::array<unsigned char,1020> vertices{};
  std::array<std::array<char,64>,32> semantics{};
  std::array<std::array<unsigned,6>,32> layout{};
  unsigned constant_count{};std::array<Constant,3> constants{};
  bool operator==(const Draw &)const=default;
 };
 struct Compute {
  std::uint64_t packet{},index{},shader_set{};unsigned technique{},owner_count{};
  std::array<unsigned,12> prefix{}; // native dispatch is 48 bytes, not a draw
  std::array<std::uint64_t,64> objects{};std::array<unsigned,64> ids{};
  std::array<unsigned char,64> modes{};std::array<Texture,64> textures{};
  std::array<bool,64> texture_used{};Range bindings{};
  // One entry per binding: native SRV/UAV view and CPU descriptor handle.
  std::array<std::array<std::uint64_t,2>,128> views{};
  ShaderBytes<65536> shader{};
  unsigned constant_count{};std::array<Constant,3> constants{};
  bool operator==(const Compute &)const=default;
 };
 struct Sample {
  std::array<std::uint64_t,8> at{};
  std::array<std::uint64_t,3> connection{}; // native descriptor, source owner, destination owner
  std::uint64_t queue{},destination{};unsigned total{},key{},count{};
  std::array<Draw,draw_capacity> draws{};
  unsigned command_count{},compute_count{};
  // Queue index, native packet, raw opcode, typed capture index (or UINT32_MAX).
  std::array<std::array<std::uint64_t,4>,16> commands{};
  std::array<Compute,8> computes{};
  bool operator==(const Sample &)const=default;
 };
private:
 bool bundle_{};
 std::array<Sample,capacity> samples_{};Sample first_{},second_{};
 unsigned count_{},attempts_{};std::array<unsigned,6> errors_{};
 std::uint64_t last_frame_{};
 template<class R,class T>static bool field(R &read,std::uint64_t p,unsigned off,T &out){
  return p&&p<=UINT64_MAX-off-sizeof(T)&&read(p+off,&out,sizeof(out));
 }
 template<class R>static bool texture(R &read,std::uint64_t object,Texture &out){
  auto &v=out.fields;v[0]=object;unsigned extent{},format{},gpu{};
  if(!field(read,object,0x60,v[1])||!field(read,object,0xf0,v[2])||
     !field(read,v[2]?v[2]:v[1],0x20,v[3])||!v[3]||
     !field(read,object,0x88,extent)||!field(read,object,0x90,format)||!field(read,object,0x94,gpu))return false;
  v[4]=extent;v[5]=format;v[6]=gpu;return true;
 }
 // The reflected native DOF buffers only: CBViewProjection b0 (11),
 // CBScreen b1 (12), CBNewDOFFilter b3 (501). Owner is the raw CPU
 // constant allocation for mode 0, as for the projection-binding recorder.
 // These are synchronous paired copies; no owner is dereferenced later.
 template<class R,class D>static bool constants(R &read,D &d,const Range &range){
  for(unsigned i=0;i<range.range[0];++i){const auto w=range.words[i];if(w&15)continue;
   const auto index=(w>>4)&255,id=(w>>12)&4095,reg=w>>24;
   unsigned bytes{};
   if(id==11&&reg==0)bytes=1072;
   else if(id==12&&reg==1)bytes=112;
   else if(id==501&&reg==3)bytes=96;
   else return false;
   if(index>=d.owner_count||d.ids[index]!=id||d.modes[index]!=0)return false;
   const auto owner=d.objects[index];
   if(!owner||(owner&15)||owner>UINT64_MAX-bytes)return false;
   bool duplicate=false;
   for(unsigned j=0;j<d.constant_count;++j){
    if(d.constants[j].word==w)duplicate=true;
    else if((d.constants[j].word>>24)==reg)return false;
   }
   if(duplicate)continue;
   if(d.constant_count==d.constants.size())return false;
   auto &c=d.constants[d.constant_count++];c.word=w;c.size=bytes;
   if(!read(owner,c.bytes.data(),bytes))return false;
  }
  return true;
 }
 template<class R>static bool draw(R &read,Draw &d,std::uint64_t context){
  if(!field(read,d.packet,0,d.prefix)||(d.prefix[0]&31)!=0)return false;
  const auto ptr=[&](unsigned i){return std::uint64_t(d.prefix[i])|(std::uint64_t(d.prefix[i+1])<<32);};
  auto &t=d.target;t[0]=ptr(4);unsigned flags{},format{};unsigned char rotating{};
  if(!field(read,t[0],0x48,flags)||!field(read,t[0],0x40,t[2])||!field(read,t[0],0,t[3])||
     !field(read,t[3],0x60,t[4])||!field(read,t[4],0x18,t[5])||!t[5]||
     !field(read,t[4],0x20,format)||!field(read,t[4],0x28,rotating)||!field(read,t[0],0x4c,d.rectangle))return false;
  t[1]=flags;t[6]=format;t[7]=rotating;
  const auto graphics=ptr(8);std::uint64_t handle{},ids{},modes{},words{},layout{},elements{};
  unsigned char stride{};unsigned offset{};auto &v=d.vertex;v[0]=ptr(6);
  if(d.prefix[16]>4||!d.prefix[16]||d.prefix[17]>1048576||!field(read,v[0],0x20,stride)||!stride||
     !field(read,v[0],0x24,offset)||offset>16777216||!field(read,context,0x2b50,v[3])||!v[3]||v[3]>UINT64_MAX-33554432)return false;
  v[1]=stride;v[2]=offset;d.vertex_bytes=stride*d.prefix[16];
  const auto start=std::uint64_t(offset)+std::uint64_t(stride)*d.prefix[17];
  if(start+d.vertex_bytes>33554432||!read(v[3]+start,d.vertices.data(),d.vertex_bytes)||
     !field(read,graphics,0x20,layout)||!field(read,layout,0x10,elements)||!field(read,layout,0x18,d.layout_count)||d.layout_count>32)return false;
  for(unsigned i=0;i<d.layout_count;++i){std::uint64_t name{};bool ended=false;
   if(!field(read,elements,32*i,name)||!field(read,elements,32*i+8,d.layout[i]))return false;
   for(unsigned j=0;j<64;++j){auto &c=d.semantics[i][j];if(!field(read,name,j,c))return false;
    if(!c){ended=true;break;}if(c<32||c>126)return false;}
   if(!ended)return false;
  }
  if(!field(read,graphics,8,handle)||!field(read,handle,0,d.shader_set)||
     !field(read,d.shader_set,0x88,d.owner_count)||d.owner_count>64||
     !field(read,d.shader_set,0x80,ids)||!field(read,graphics,0x10,modes)||!field(read,d.shader_set,0,words))return false;
  for(unsigned i=0;i<d.owner_count;++i)
   if(!field(read,ids,4*i,d.ids[i])||!field(read,modes,i,d.modes[i])||!field(read,graphics,0x30+8*i,d.objects[i]))return false;
  for(unsigned stage=0;stage<2;++stage){auto &b=d.ranges[stage];
   if(!field(read,d.shader_set,stage?0x14:8,b.range)||b.range[0]>128||unsigned(b.range[0])+b.range[1]>4096)return false;
   for(unsigned i=0;i<b.range[0];++i){auto &w=b.words[i];if(!field(read,words,4U*(b.range[1]+i),w))return false;
    const auto kind=w&15,index=(w>>4)&255;if(kind>3||index>=d.owner_count)return false;
    if(kind==2||kind==3){d.texture_used[index]=true;if(!texture(read,d.objects[index],d.textures[index]))return false;}
   }
   auto &s=d.shaders[stage];auto &identity=s.identity;
   if(!field(read,d.shader_set,stage?0x58:0x50,identity[0])||!field(read,identity[0],0x10,identity[1])||
      !field(read,identity[1],0,s.size)||s.size<52||s.size>shader_capacity||!field(read,identity[1],8,identity[2])||
      !identity[2]||identity[2]>UINT64_MAX-s.size||!read(identity[2],s.bytes.data(),s.size))return false;
   if(s.bytes[0]!='D'||s.bytes[1]!='X'||s.bytes[2]!='B'||s.bytes[3]!='C')return false;
  }
  return true;
 }
 template<class R>static bool compute(R &read,Compute &d){
  if(!field(read,d.packet,0,d.prefix)||(d.prefix[0]&31)!=6)return false;
  const auto ptr=[&](unsigned i){return std::uint64_t(d.prefix[i])|(std::uint64_t(d.prefix[i+1])<<32);};
  // Mode is one byte; its adjacent bytes are unrelated native data.
  if((d.prefix[9]&255)!=0||ptr(10)||!d.prefix[6]||!d.prefix[7]||!d.prefix[8]||
     d.prefix[6]>65535||d.prefix[7]>65535||d.prefix[8]>65535)return false;
  const auto state=ptr(4);std::uint64_t handle{},ids{},modes{},words{};
  if(!field(read,state,8,handle)||!field(read,handle,0,d.shader_set)||!field(read,state,0x18,d.technique)||
     !field(read,d.shader_set,0x88,d.owner_count)||!d.owner_count||d.owner_count>64||
     !field(read,d.shader_set,0x80,ids)||!field(read,state,0x10,modes)||!field(read,d.shader_set,0,words))return false;
  for(unsigned i=0;i<d.owner_count;++i)
   if(!field(read,ids,4*i,d.ids[i])||d.ids[i]==UINT32_MAX||!field(read,modes,i,d.modes[i])||
      !field(read,state,0x30+8*i,d.objects[i])||!d.objects[i])return false;
  auto &b=d.bindings;
  if(!field(read,d.shader_set,0x44,b.range)||!b.range[0]||b.range[0]>128||unsigned(b.range[0])+b.range[1]>4096)return false;
  for(unsigned i=0;i<b.range[0];++i){auto &w=b.words[i];if(!field(read,words,4U*(b.range[1]+i),w))return false;
   const auto kind=w&15,index=(w>>4)&255;if(kind>3||index>=d.owner_count)return false;
   if(kind==2||kind==3){d.texture_used[index]=true;auto &v=d.views[i];
    if(!texture(read,d.objects[index],d.textures[index])||!field(read,d.objects[index],kind==2?0x78:0x80,v[0])||
       !field(read,v[0],0x10,v[1])||!v[1])return false;
   }
  }
  auto &h=d.shader;auto &id=h.identity;
  if(!field(read,d.shader_set,0x78,id[0])||!field(read,id[0],0x10,id[1])||!field(read,id[1],0,h.size)||
     h.size<52||h.size>h.bytes.size()||!field(read,id[1],8,id[2])||!id[2]||id[2]>UINT64_MAX-h.size||
     !read(id[2],h.bytes.data(),h.size))return false;
  return h.bytes[0]=='D'&&h.bytes[1]=='X'&&h.bytes[2]=='B'&&h.bytes[3]=='C'&&constants(read,d,d.bindings);
 }
 template<class G,class R>static unsigned capture(const G &g,unsigned key,R &read,Sample &s,bool bundle){
  static_assert(std::is_trivially_copyable_v<Sample>);
  // Avoid a large aggregate temporary on the game's render-thread stack.
  std::fill_n(reinterpret_cast<unsigned char *>(&s),sizeof(s),static_cast<unsigned char>(0));s.at=g.at;s.key=key;
  unsigned filters{};
  for(unsigned i=0;i<g.count;++i){const auto &f=g.filters[i];if(((f[1]<<27)|f[2])!=s.key)continue;
   ++filters;std::array<std::uint64_t,4> current{};
   if(!field(read,f[0],0,current)||current!=std::array<std::uint64_t,4>{f[1]|(f[2]<<32),f[3],f[5],f[6]})return 1;
   s.destination=f[6];s.connection={f[0],f[5],f[6]};
  }
  if(filters!=1||!s.destination||!field(read,g.at[3],0x2b78,s.queue)||!s.queue||
     !field(read,g.at[3],0x2b80,s.total)||!s.total||s.total>=65535||s.queue>UINT64_MAX-16ULL*s.total)return 1;
  struct Packet {unsigned key,padding;std::uint64_t object;};static_assert(sizeof(Packet)==16);
  std::array<Packet,256> block{};
  for(unsigned base=0;base<s.total;base+=256){const auto n=(std::min)(256U,s.total-base);
   if(!read(s.queue+16ULL*base,block.data(),n*sizeof(Packet)))return 2;
   for(unsigned j=0;j<n;++j){const auto &p=block[j];if(p.key!=s.key)continue;
    unsigned opcode{};if(!field(read,p.object,0,opcode))return 2;
    if(bundle){
     if(s.command_count==s.commands.size())return 3;
     auto &command=s.commands[s.command_count++];command={base+j,p.object,opcode,UINT32_MAX};
     if((opcode&31)==6){
      if(s.compute_count==s.computes.size())return 3;
      command[3]=s.compute_count;auto &d=s.computes[s.compute_count++];d.packet=p.object;d.index=base+j;
      if(!compute(read,d))return 4;continue;
     }
     if((opcode&31)==0)command[3]=s.count;
     else if((opcode&31)!=11)return 4;
    }
    if((opcode&31)!=0)continue;
    if(s.count==draw_capacity)return 3;
    auto &d=s.draws[s.count++];d.packet=p.object;d.index=base+j;if(!draw(read,d,g.at[3]))return 4;
    if(bundle)for(const auto &range:d.ranges)if(!constants(read,d,range))return 4;
   }
  }
  return s.count&&(!bundle||s.compute_count)?0:3;
 }
 template<class G,class R,class S>void observe_key(const G &g,unsigned key,R &read,S &stamp){
  if(count_==capacity||attempts_==attempt_limit||g.count>16||
     !g.at[1]||g.at[1]<=last_frame_||(last_frame_&&g.at[1]-last_frame_<60))return;
  last_frame_=g.at[1];++attempts_;
  auto error=capture(g,key,read,first_,bundle_);if(!error)error=capture(g,key,read,second_,bundle_);
  if(!error&&(first_!=second_||!stamp()))error=5;
  if(error){++errors_[error];return;}
  samples_[count_++]=first_;
 }
public:
 explicit OutputDependencies(bool compute_bundle=false):bundle_(compute_bundle){}
 unsigned size()const{return count_;}unsigned attempts()const{return attempts_;}
 const Sample &sample(unsigned i)const{return samples_[i];}
 // Diagnostic groups only. A nonzero mask never authorizes replay.
 static unsigned compute_difference(const Compute &a,const Compute &b){
  unsigned mask{};
  if(a.packet!=b.packet||a.index!=b.index||a.shader_set!=b.shader_set||a.technique!=b.technique||a.owner_count!=b.owner_count)mask|=1;
  if(a.prefix!=b.prefix)mask|=2;
  if(a.objects!=b.objects||a.ids!=b.ids||a.modes!=b.modes)mask|=4;
  if(a.textures!=b.textures||a.texture_used!=b.texture_used)mask|=8;
  if(a.bindings!=b.bindings)mask|=16;
  if(a.views!=b.views)mask|=32;
  if(a.shader!=b.shader)mask|=64;
  if(a.constant_count!=b.constant_count||a.constants!=b.constants)mask|=128;
  return mask;
 }
 // Queue preparation precedes the native resource-state planner. It writes
 // packet +8 (prefix words 2/3), then the compute consumer reads that list to
 // emit barriers. Compare the stable recipe across those stages, retaining
 // the raw prefix separately. This never proves a resource's GPU state.
 // Same-stage capture/matches deliberately keep exact equality.
 static unsigned compute_recording_difference(const Compute &a,const Compute &b){
  auto mask=compute_difference(a,b);
  for(unsigned i=0;i<a.prefix.size();++i)
   if(i!=2&&i!=3&&a.prefix[i]!=b.prefix[i])return mask;
  return mask&~2U;
 }
 template<class R>static bool compute_reread(const Compute &expected,Compute &scratch,R read){
  std::fill_n(reinterpret_cast<unsigned char *>(&scratch),sizeof(scratch),static_cast<unsigned char>(0));
  scratch.packet=expected.packet;scratch.index=expected.index;
  return compute(read,scratch);
 }
 template<class R>static bool compute_matches(const Compute &expected,Compute &scratch,R read){
  return compute_reread(expected,scratch,read)&&scratch==expected;
 }
 template<class G,class R,class S>void observe(const G &g,R read,S stamp){
  if(g.reason!=17||(g.command_gate[0]!=9&&g.command_gate[0]!=10)||!g.command_gate[1])return;
  observe_key(g,static_cast<unsigned>(g.command_gate[1]),read,stamp);
 }
 template<class G,class R,class S>void observe_intermediate(const G &g,R read,S stamp,std::uint64_t target_type){
  if(g.reason!=6||g.detail!=4||g.count>16||(g.link_reads&7)!=7||!g.links[4]||
     !g.links[5]||g.links[4]==g.links[5]||!target_type)return;
  unsigned matches{},key{};
  for(unsigned i=0;i<g.count;++i){const auto &f=g.filters[i];
   if(f[1]==15&&f[2]>100&&f[2]<0x7fffff1&&f[3]&&f[4]==target_type&&
      f[5]==g.links[4]&&f[6]==g.links[5]){++matches;key=static_cast<unsigned>((f[1]<<27)|f[2]);}
  }
  if(matches==1)observe_key(g,key,read,stamp);
 }
 template<class G,class R,class S>void observe_compute_bundle(const G &g,R read,S stamp,std::uint64_t target_type){
  if(!bundle_||g.reason!=17||g.detail||g.command_gate[0]!=7||!g.command_gate[1]||
     !g.command_gate[5]||(g.command_gate[4]&31)!=6||g.count>16||!target_type)return;
  unsigned matches{};const auto key=static_cast<unsigned>(g.command_gate[1]);
  for(unsigned i=0;i<g.count;++i){const auto &f=g.filters[i];
   if(((f[1]<<27)|f[2])==key&&f[1]==15&&f[2]>100&&f[2]<0x7fffff1&&f[3]&&f[4]==target_type&&
      f[5]&&f[6]&&f[5]!=f[6])++matches;
  }
  if(matches==1)observe_key(g,key,read,stamp);
 }
 void write(std::ostream &o)const{
  const auto a=[&](const auto &v){o<<'[';bool comma=false;for(auto x:v){if(comma)o<<',';o<<+x;comma=true;}o<<']';};
  const auto hex=[&](const auto &s){constexpr char chars[]="0123456789abcdef";o<<'"';
   for(unsigned i=0;i<s.size;++i)o<<chars[s.bytes[i]>>4]<<chars[s.bytes[i]&15];o<<'"';};
  const auto constant_json=[&](const auto &d){
   o<<",\"constants\":[";
   for(unsigned j=0;j<d.constant_count;++j){if(j)o<<',';const auto &c=d.constants[j];
    o<<"{\"word\":"<<c.word<<",\"size\":"<<c.size<<",\"hex\":";hex(c);o<<'}';}o<<']';
  };
  o<<"{\"schema\":"<<(bundle_?3:1)<<",\"policy\":\""<<(bundle_?"prepared-compute-constants-v1":"prepared-draw-dependencies-v1")<<"\",\"diagnostic_only\":true,\"capacity\":2,\"draw_capacity\":4,\"attempt_limit\":16,\"attempts\":"<<attempts_
   <<",\"errors\":";a(errors_);o<<",\"samples\":[";
  for(unsigned i=0;i<count_;++i){if(i)o<<',';const auto &s=samples_[i];o<<"{\"at\":";a(s.at);
   o<<",\"connection\":";a(s.connection);
   o<<",\"key\":"<<s.key<<",\"queue\":"<<s.queue<<",\"total\":"<<s.total<<",\"destination\":"<<s.destination<<",\"draws\":[";
   for(unsigned j=0;j<s.count;++j){if(j)o<<',';const auto &d=s.draws[j];o<<"{\"packet\":"<<d.packet<<",\"index\":"<<d.index<<",\"prefix\":";a(d.prefix);
    o<<",\"target\":";a(d.target);o<<",\"rectangle\":";a(d.rectangle);o<<",\"shader_set\":"<<d.shader_set;
    o<<",\"vertex\":";a(d.vertex);o<<",\"vertices\":";a(std::span(d.vertices).first(d.vertex_bytes));
    o<<",\"layout\":[";for(unsigned k=0;k<d.layout_count;++k){if(k)o<<',';o<<"{\"semantic\":"<<std::quoted(d.semantics[k].data())<<",\"fields\":";a(d.layout[k]);o<<'}';}o<<']';
    o<<",\"owners\":[";for(unsigned k=0;k<d.owner_count;++k){if(k)o<<',';a(std::array<std::uint64_t,3>{d.ids[k],d.modes[k],d.objects[k]});}o<<']';
    o<<",\"textures\":[";bool comma=false;for(unsigned k=0;k<d.owner_count;++k)if(d.texture_used[k]){if(comma)o<<',';comma=true;
     o<<"{\"index\":"<<k<<",\"fields\":";a(d.textures[k].fields);o<<'}';}o<<']';
    o<<",\"ranges\":[";for(unsigned k=0;k<2;++k){if(k)o<<',';const auto &r=d.ranges[k];o<<"{\"range\":";a(r.range);o<<",\"words\":";a(std::span(r.words).first(r.range[0]));o<<'}';}o<<']';
    if(bundle_)constant_json(d);
    o<<",\"shaders\":[";for(unsigned k=0;k<2;++k){if(k)o<<',';const auto &h=d.shaders[k];o<<"{\"identity\":";a(h.identity);o<<",\"size\":"<<h.size<<",\"hex\":";hex(h);o<<'}';}o<<"]}";
   }o<<']';
   if(bundle_){o<<",\"commands\":[";for(unsigned j=0;j<s.command_count;++j){if(j)o<<',';a(s.commands[j]);}o<<"],\"computes\":[";
    for(unsigned j=0;j<s.compute_count;++j){if(j)o<<',';const auto &d=s.computes[j];
     o<<"{\"packet\":"<<d.packet<<",\"index\":"<<d.index<<",\"prefix\":";a(d.prefix);
     o<<",\"shader_set\":"<<d.shader_set<<",\"technique\":"<<d.technique<<",\"owners\":[";
     for(unsigned k=0;k<d.owner_count;++k){if(k)o<<',';a(std::array<std::uint64_t,3>{d.ids[k],d.modes[k],d.objects[k]});}o<<']';
     o<<",\"textures\":[";bool comma=false;for(unsigned k=0;k<d.owner_count;++k)if(d.texture_used[k]){if(comma)o<<',';comma=true;
      o<<"{\"index\":"<<k<<",\"fields\":";a(d.textures[k].fields);o<<'}';}o<<']';
     o<<",\"range\":";a(d.bindings.range);o<<",\"words\":";a(std::span(d.bindings.words).first(d.bindings.range[0]));
     o<<",\"views\":[";for(unsigned k=0;k<d.bindings.range[0];++k){if(k)o<<',';a(d.views[k]);}o<<']';
     constant_json(d);
     o<<",\"shader\":{\"identity\":";a(d.shader.identity);o<<",\"size\":"<<d.shader.size<<",\"hex\":";hex(d.shader);o<<"}}";
    }o<<']';
   }
   o<<",\"double_snapshot_equal\":true,\"gpu_verified\":false}";
  }o<<"]}";
 }
};
}
