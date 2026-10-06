#include "draw_execution.hpp"
#include "../projection_binding/memory.hpp"
#include <MinHook.h>
#include <d3d12.h>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <ostream>
#ifdef MHW_SCALED_OUTPUT
#include "output_route.hpp"
#include "../temporal_present/observer.hpp"
#endif
extern "C" void scaled_draw_execution_entry();
extern "C" void mhw_binding_forward(void *,mhw::projection_binding::Frame *);
namespace mhw::scaled_temporal::draw_execution {
namespace {
namespace tb=temporal_bindings;namespace pb=projection_binding;
constexpr unsigned count=9,shader_limit=262144;
struct Shader {std::uintptr_t entry{},descriptor{},data{};unsigned size{};std::array<std::byte,shader_limit> bytes{};std::string file,sha;bool saved{};};
struct Record {
  tb::Snapshot stamp;tb::Owner output;pb::Draw draw;
  pb::AttachmentSnapshot before,after;
  std::array<Shader,2> shaders;
  std::array<D3D12_INPUT_ELEMENT_DESC,32> elements{};
  std::array<std::array<char,64>,32> semantics{};
  std::array<unsigned,2> arguments{};std::array<int,4> rectangle{};
  std::uintptr_t state{},recorded_packet{},root{},pipeline{};
  std::array<std::uintptr_t,4> packet_bindings{};
  D3D12_RESOURCE_DESC description{};
  unsigned thread{};bool valid{},returned{};
  std::array<float,16> vertices{};unsigned vertex_floats{},api_calls{};bool replaced{};
};
tb::Config cfg;std::array<Record,count> records;
// 0 empty, 1 publishing, 2 selectable, 3 executing, 4 finished.
std::array<std::atomic<unsigned>,count> phases;
std::atomic<unsigned> pending{},fault{},completed{};
std::atomic<bool> accepting{};bool configured{},installed{},disabled{},drained{};
void *original{};std::uintptr_t target{};
#ifdef MHW_SCALED_OUTPUT
thread_local unsigned api_record=UINT_MAX;
thread_local bool injecting{};
#endif
template<class T>bool field(std::uintptr_t at,unsigned off,T &v){
  return at&&at<=UINTPTR_MAX-off-sizeof(T)&&pb::copy_memory(at+off,&v,sizeof(T));
}
void fail(unsigned n){unsigned zero{};fault.compare_exchange_strong(zero,n);accepting.store(false,std::memory_order_release);}
bool source(Record &r){
  unsigned opcode{};const auto packet=r.draw.packet;
  return field(packet,0,opcode)&&(opcode&31)==0&&field(packet,0x10,r.packet_bindings)&&
    field(packet,0x40,r.arguments)&&(r.arguments[0]==3||r.arguments[0]==4)&&r.arguments[1]<=1048576&&
    field(r.packet_bindings[0],0x4c,r.rectangle);
}
bool shader(Shader &s,std::uintptr_t set,unsigned offset,unsigned type){
  if(!field(set,offset,s.entry)||!s.entry||!field(s.entry,0x10,s.descriptor)||
    !field(s.descriptor,0,s.size)||s.size<52||s.size>shader_limit||!field(s.descriptor,8,s.data)||!s.data||
    !pb::copy_memory(s.data,s.bytes.data(),s.size)||!pb::dxbc_valid(s.bytes.data(),s.size))return false;
  auto word=[&](unsigned at){unsigned v{};std::memcpy(&v,s.bytes.data()+at,4);return v;};unsigned programs{};
  for(unsigned i=0;i<word(28);++i){const auto at=word(32+4*i);
    if(std::memcmp(s.bytes.data()+at,"SHEX",4)&&std::memcmp(s.bytes.data()+at,"SHDR",4))continue;
    if(++programs!=1||word(at+4)<8||word(at+8)!=(type<<16|0x50U))return false;
  }
  return programs==1;
}
bool shader_stable(const Shader &s,std::uintptr_t set,unsigned offset){
  std::uintptr_t entry{},desc{},data{};unsigned size{};
  if(!field(set,offset,entry)||entry!=s.entry||!field(entry,0x10,desc)||desc!=s.descriptor||
    !field(desc,0,size)||size!=s.size||!field(desc,8,data)||data!=s.data)return false;
  std::array<std::byte,256> bytes{};
  for(unsigned at=0;at<size;at+=256){const auto n=(std::min)(256U,size-at);
    if(!pb::copy_memory(data+at,bytes.data(),n)||std::memcmp(bytes.data(),s.bytes.data()+at,n))return false;}
  return true;
}
bool layout(Record &r,bool compare){
  const auto &p=r.before.pipeline;
  if(p.input_count>32||(!p.input_elements&&p.input_count))return false;
  for(unsigned i=0;i<p.input_count;++i){D3D12_INPUT_ELEMENT_DESC element{};std::array<char,64> semantic{};bool terminated=false;
    if(!field(p.input_elements,i*sizeof(element),element)||!element.SemanticName)return false;
    for(unsigned j=0;j<semantic.size();++j){
      if(!field(reinterpret_cast<std::uintptr_t>(element.SemanticName),j,semantic[j]))return false;
      if(!semantic[j]){terminated=true;break;}
      if(semantic[j]<32||semantic[j]>126)return false;
    }
    if(!terminated)return false;
    if(compare){if(std::memcmp(&element,&r.elements[i],sizeof(element))||semantic!=r.semantics[i])return false;}
    else{r.elements[i]=element;r.semantics[i]=semantic;}
  }return true;
}
bool resource(Record &r){
  const auto candidate=r.after.views[0].resource;std::uintptr_t method{};
  for(unsigned slot:{0U,2U,7U,10U})if(!pb::system_method(candidate,slot,method))return false;
  ID3D12Resource *value{};ID3D12Device *device{};bool ok=false;
  __try{
    auto *input=reinterpret_cast<ID3D12Resource *>(candidate);
    if(SUCCEEDED(input->QueryInterface(IID_PPV_ARGS(&value)))&&value==input&&
      SUCCEEDED(value->GetDevice(IID_PPV_ARGS(&device)))&&reinterpret_cast<std::uintptr_t>(device)==cfg.device){
      r.description=value->GetDesc();const auto &d=r.description;
      ok=d.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D&&d.Width==r.output.width&&d.Height==r.output.height&&
        d.DepthOrArraySize==1&&d.MipLevels==1&&d.SampleDesc.Count==1&&d.SampleDesc.Quality==0&&
        (d.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)!=0;
    }
  }__finally{if(device)device->Release();if(value)value->Release();}
  return ok;
}
unsigned before(pb::Frame &f){
  if(!accepting.load(std::memory_order_acquire))return UINT_MAX;
  const auto packet=f.gpr[3];unsigned found=UINT_MAX;
  for(unsigned i=0;i<count;++i){const auto phase=phases[i].load(std::memory_order_acquire);
    if((phase==2||phase==3)&&records[i].draw.packet==packet){
      if(phase==3||found!=UINT_MAX){fail(1);return UINT_MAX;}found=i;}}
  if(found==UINT_MAX)return found;
  unsigned expected=2;if(!phases[found].compare_exchange_strong(expected,3)){fail(2);return UINT_MAX;}
  auto &r=records[found];r.thread=GetCurrentThreadId();r.draw.executor=f.gpr[1];r.draw.context=f.gpr[2];
  const auto bindings=r.packet_bindings;const auto args=r.arguments;const auto rect=r.rectangle;
  r.valid=field(r.draw.executor,8,r.state)&&field(r.state,0x20,r.draw.command)&&r.draw.command&&
    source(r)&&r.packet_bindings==bindings&&r.arguments==args&&r.rectangle==rect&&
    pb::snapshot_attachments(cfg.image,r.draw,r.before)&&
    r.before.color_count==1&&!r.before.views[8].owner&&r.before.views[0].resource==r.output.resource&&
    field(r.before.pipeline.shader_handle,0,r.draw.shader_set)&&
    shader(r.shaders[0],r.draw.shader_set,0x50,1)&&shader(r.shaders[1],r.draw.shader_set,0x58,0)&&layout(r,false);
  // The implementation being qualified is the observed VS/PS-only path.
  for(unsigned offset:{0x60U,0x68U,0x70U}){std::uintptr_t stage{};
    r.valid=r.valid&&field(r.draw.shader_set,offset,stage)&&!stage;}
  if(!r.valid)fail(3);return found;
}
void after(unsigned i,bool returned){
  if(i==UINT_MAX)return;auto &r=records[i];r.returned=returned;
  pb::AttachmentSnapshot now;std::array<std::uintptr_t,4> bindings{};std::array<unsigned,2> args{};std::array<int,4> rect{};
  r.valid=r.valid&&returned&&r.thread==GetCurrentThreadId()&&
    pb::snapshot_attachments(cfg.image,r.draw,r.after)&&pb::attachment_inputs_equal(r.before,r.after)&&
    pb::attachment_cache_matches(r.after)&&field(r.state,0x158,r.recorded_packet)&&r.recorded_packet==r.draw.packet&&
    field(r.state,0x108,r.pipeline)&&r.pipeline&&field(r.state,0x110,r.root)&&r.root&&
    field(r.draw.packet,0x10,bindings)&&bindings==r.packet_bindings&&field(r.draw.packet,0x40,args)&&args==r.arguments&&
    field(r.packet_bindings[0],0x4c,rect)&&rect==r.rectangle&&
    shader_stable(r.shaders[0],r.draw.shader_set,0x50)&&shader_stable(r.shaders[1],r.draw.shader_set,0x58)&&layout(r,true)&&resource(r)&&
    pb::snapshot_attachments(cfg.image,r.draw,now)&&now==r.after;
  if(!r.valid)fail(returned?4:5);
  phases[i].store(4,std::memory_order_release);completed.fetch_add(1,std::memory_order_release);
}
template<class T,std::size_t N>void values(std::ostream &o,const std::array<T,N> &v){o<<'[';for(unsigned i=0;i<N;++i){if(i)o<<',';o<<v[i];}o<<']';}
}
void dispatch(pb::Frame *f){pending.fetch_add(1,std::memory_order_acq_rel);const auto i=before(*f);bool returned=false;
#ifdef MHW_SCALED_OUTPUT
  const auto previous=api_record;if(previous!=UINT_MAX&&i!=UINT_MAX)fail(9);api_record=i;
#endif
  __try{mhw_binding_forward(original,f);returned=true;}__finally{
#ifdef MHW_SCALED_OUTPUT
    api_record=previous;
#endif
    after(i,returned);pending.fetch_sub(1,std::memory_order_release);}}
bool publish(unsigned frame,unsigned stage,const tb::Snapshot &s,std::uintptr_t packet,const tb::Owner &output,std::span<const float> vertices) noexcept {
  if(!configured)return true;
  if(!accepting.load()||frame>=3||stage>=3||!packet||!output.resource){fail(6);return false;}
  const auto i=3*frame+stage;unsigned empty{};
  if(!phases[i].compare_exchange_strong(empty,1)){fail(6);return false;}
  auto &r=records[i];r.stamp=s;r.output=output;r.draw.packet=packet;
  if(vertices.size()>16){fail(6);return false;}r.vertex_floats=static_cast<unsigned>(vertices.size());std::copy(vertices.begin(),vertices.end(),r.vertices.begin());
  // Save the complete immutable draw-input identity before publication. Never
  // claim a later packet solely because its arena reused the same address.
  if(!source(r)){fail(6);return false;}
#ifdef MHW_SCALED_OUTPUT
  if(vertices.size()!=r.arguments[0]*4||!output_route::publish_draw(frame,stage,s,output)){fail(6);return false;}
#endif
  phases[i].store(2,std::memory_order_release);return true;
}
#ifdef MHW_SCALED_OUTPUT
namespace {
ID3D12Resource *vertex_resource(std::uintptr_t ia,const D3D12_VERTEX_BUFFER_VIEW &view) noexcept {
  std::uintptr_t owner{},backend{},candidate{},base{},check{};unsigned bytes{},offset{};unsigned char stride{};
  if(!field(ia,0,owner)||!field(owner,0x60,backend)||!field(backend,0x20,candidate)||!candidate||
     !field(backend,0x40,base)||!field(backend,0x10,bytes)||!field(ia,0x24,offset)||!field(ia,0x20,stride)||
     stride!=16||offset>bytes||base>UINTPTR_MAX-offset||view.BufferLocation!=base+offset||view.SizeInBytes!=bytes-offset||view.StrideInBytes!=stride)return nullptr;
  for(unsigned slot:{0U,2U,7U,10U,11U})if(!pb::system_method(candidate,slot,check))return nullptr;
  ID3D12Resource *held{};ID3D12Device *device{};bool ok=false;
  __try{auto *p=reinterpret_cast<ID3D12Resource *>(candidate);
    if(SUCCEEDED(p->QueryInterface(IID_PPV_ARGS(&held)))&&held==p&&SUCCEEDED(held->GetDevice(IID_PPV_ARGS(&device)))&&reinterpret_cast<std::uintptr_t>(device)==cfg.device){
      const auto desc=held->GetDesc();ok=desc.Dimension==D3D12_RESOURCE_DIMENSION_BUFFER&&desc.Width>=bytes&&held->GetGPUVirtualAddress()==base&&
        field(owner,0x60,check)&&check==backend&&field(backend,0x20,check)&&check==candidate;}
  }__except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
  if(device)device->Release();if(!ok&&held){held->Release();held=nullptr;}return held;
}
bool draw_impl(Record &r,unsigned index,ID3D12GraphicsCommandList *list,unsigned n,unsigned instances,unsigned first,unsigned instance,std::uintptr_t caller){
  ++r.api_calls;pb::AttachmentSnapshot ready;temporal_present::observer::RasterBindings raster;temporal_present::Bindings roots;
  if(r.api_calls!=1||!r.valid||r.thread!=GetCurrentThreadId()||caller!=cfg.image+0x259d72e||
     reinterpret_cast<std::uintptr_t>(list)!=r.draw.command||n!=r.arguments[0]||first!=r.arguments[1]||instances!=1||instance||
     !pb::snapshot_attachments(cfg.image,r.draw,ready)||!pb::attachment_inputs_equal(r.before,ready)||!pb::attachment_cache_matches(ready)||
     !temporal_present::observer::raster_current(list,raster)||!raster.output_known||raster.render_target!=ready.views[0].handle||
     !temporal_present::observer::inspect(list,roots)||!roots.known||!roots.valid||!roots.graphics.signature||!roots.pipeline){fail(10);return false;}
  std::uintptr_t root{},pipeline{};
  if(!field(r.state,0x110,root)||root!=roots.graphics.signature||!field(r.state,0x108,pipeline)||pipeline!=roots.pipeline){fail(10);return false;}
  auto *vb=vertex_resource(r.packet_bindings[1],raster.vertex_buffer);if(!vb){fail(11);return false;}
  EffectRecipe recipe;recipe.stage=static_cast<EffectStage>(index%3);recipe.geometry={r.stamp.rectangle[2],r.stamp.rectangle[3],r.output.width,r.output.height};
  recipe.root=reinterpret_cast<ID3D12RootSignature *>(root);recipe.vertex_shader=std::span(r.shaders[0].bytes).first(r.shaders[0].size);recipe.pixel_shader=std::span(r.shaders[1].bytes).first(r.shaders[1].size);
  static_assert(sizeof(recipe.depth)==sizeof(ready.pipeline.depth)&&sizeof(recipe.blend)==sizeof(ready.pipeline.blend)&&sizeof(recipe.raster)==sizeof(ready.pipeline.raster));
  std::memcpy(&recipe.depth,ready.pipeline.depth.data(),sizeof(recipe.depth));std::memcpy(&recipe.blend,ready.pipeline.blend.data(),sizeof(recipe.blend));std::memcpy(&recipe.raster,ready.pipeline.raster.data(),sizeof(recipe.raster));
  recipe.input_layout=std::span(r.elements).first(r.before.pipeline.input_count);recipe.native_vertices=std::span(r.vertices).first(r.vertex_floats);
  EffectBindings bindings{list,reinterpret_cast<ID3D12PipelineState *>(pipeline),recipe.root,reinterpret_cast<ID3D12Resource *>(r.output.resource),vb,
    raster.vertex_buffer,raster.viewport,raster.scissor,raster.topology,n,first};
  const bool ok=output_route::draw(index,recipe,bindings,ready.views[0].handle,r.replaced);vb->Release();if(!ok)fail(12);return r.replaced;
}
}
bool draw_override(ID3D12GraphicsCommandList *list,unsigned n,unsigned instances,unsigned first,unsigned instance,std::uintptr_t caller) noexcept {
  if(injecting||api_record==UINT_MAX||!healthy())return false;bool replaced=false;injecting=true;
  __try{replaced=draw_impl(records[api_record],api_record,list,n,instances,first,instance,caller);}
  __finally{injecting=false;}return replaced;
}
#endif
bool healthy() noexcept{return !fault.load(std::memory_order_acquire);}
bool active() noexcept{return configured;}
bool complete() noexcept{return !configured||(healthy()&&completed.load(std::memory_order_acquire)==count);}
bool settled() noexcept{return pending.load(std::memory_order_acquire)==0;}
void stop() noexcept{accepting.store(false,std::memory_order_release);}
bool install(const tb::Config &c,std::string &reason){
  if(configured){reason="draw-execution-already-used";return false;}cfg=c;configured=true;target=c.image+0x259d670;
  if(!pb::verify_code(c.image,0x259d670,206,"d6b0b80a37a8fadba69d215cfcebd960c74f8d3554e9375b03ac7785d4f6ee39")||
    !pb::verify_code(c.image,0x259f9b0,1531,"7abd32763fb614e868e425a607d67d717205988f34a520fed0884fd51cb720c8")||
    !pb::verify_code(c.image,0x259e670,2554,"088e67c77b7e96af64c83c8297f6fc9f6d355dbc170530dda125b41f4cb53727")){
    reason="draw-execution-code";return false;}
  const auto created=MH_CreateHook(reinterpret_cast<void *>(target),reinterpret_cast<void *>(scaled_draw_execution_entry),&original);
  if(created!=MH_OK){reason="draw-execution-create-"+std::to_string(created);return false;}
  accepting.store(true,std::memory_order_release);const auto enabled=MH_EnableHook(reinterpret_cast<void *>(target));
  if(enabled!=MH_OK){fail(7);reason="draw-execution-enable-"+std::to_string(enabled);return false;}installed=true;return true;
}
bool finish() noexcept{
  if(!configured)return true;stop();const auto tick=GetTickCount64();while(!settled()&&GetTickCount64()-tick<2000)Sleep(1);
  drained=settled();if(!drained){fail(8);return false;}
  const auto result=installed?MH_DisableHook(reinterpret_cast<void *>(target)):MH_ERROR_DISABLED;
  disabled=result==MH_OK||result==MH_ERROR_DISABLED;
  while(!settled()&&GetTickCount64()-tick<2000)Sleep(1);
  return disabled&&settled()&&complete();
}
bool save(const std::wstring &prefix){
  if(!configured)return true;if(!disabled||!settled()||!complete())return false;
  for(unsigned i=0;i<count;++i)for(unsigned j=0;j<2;++j){auto &s=records[i].shaders[j];if(s.saved)return false;
    const auto path=std::filesystem::path(prefix+L".draw-"+std::to_wstring(i)+(j?L".ps.dxbc":L".vs.dxbc"));
    auto file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;DWORD written{};
    const bool ok=WriteFile(file,s.bytes.data(),s.size,&written,nullptr)&&written==s.size&&FlushFileBuffers(file);
    const bool closed=CloseHandle(file)!=0;if(!ok||!closed)return false;
    s.file=path.filename().string();s.sha=pb::bytes_sha256(s.bytes.data(),s.size);s.saved=s.sha.size()==64;if(!s.saved)return false;
  }return true;
}
void write(std::ostream &o){
  o<<"{\"schema\":1,\"enabled\":"<<configured<<",\"fault\":"<<fault.load()<<",\"pending\":"<<pending.load()<<",\"completed\":"<<completed.load()
   <<",\"hooks_disabled\":"<<disabled<<",\"drained\":"<<drained<<",\"mutations\":0,\"gpu_submission_verified\":false,\"records\":[";
  bool comma=false;for(unsigned i=0;i<count;++i){if(phases[i].load()!=4)continue;if(comma)o<<',';comma=true;
    const auto &r=records[i];const auto &a=r.after;const auto &p=a.pipeline;const auto &d=r.description;
    o<<"{\"index\":"<<i<<",\"frame\":"<<r.stamp.frame<<",\"epoch\":"<<r.stamp.epoch<<",\"view\":"<<r.stamp.view<<",\"stage\":"<<i%3
     <<",\"valid\":"<<r.valid<<",\"returned\":"<<r.returned<<",\"api_calls\":"<<r.api_calls<<",\"replaced\":"<<r.replaced<<",\"thread\":"<<r.thread<<",\"packet\":"<<r.draw.packet<<",\"executor\":"<<r.draw.executor
     <<",\"context\":"<<r.draw.context<<",\"command\":"<<r.draw.command<<",\"state\":"<<r.state<<",\"recorded_packet\":"<<r.recorded_packet
     <<",\"pipeline\":"<<r.pipeline<<",\"root\":"<<r.root<<",\"attachment\":"<<a.attachment<<",\"active_attachment\":"<<a.active_attachment
     <<",\"attachment_flags\":"<<a.attachment_flags<<",\"color_count\":"<<a.color_count<<",\"scissor_enabled\":"<<a.scissor_enabled<<",\"rtv\":"<<a.views[0].handle
     <<",\"resource\":"<<a.views[0].resource<<",\"format\":"<<d.Format<<",\"rtv_format\":"<<a.views[0].format<<",\"width\":"<<d.Width<<",\"height\":"<<d.Height
     <<",\"samples\":"<<d.SampleDesc.Count<<",\"flags\":"<<d.Flags<<",\"arguments\":";values(o,r.arguments);
    o<<",\"rectangle\":";values(o,r.rectangle);o<<",\"viewport\":";values(o,a.viewport);o<<",\"scissor\":";values(o,a.scissor);
    o<<",\"graphics_flags\":"<<p.graphics_flags<<",\"input_count\":"<<p.input_count<<",\"input_layout\":"<<p.input_layout
     <<",\"raster_variant\":"<<p.raster_variant<<",\"depth\":";values(o,p.depth);o<<",\"blend\":";values(o,p.blend);o<<",\"raster\":";values(o,p.raster);
    o<<",\"input_elements\":[";for(unsigned j=0;j<p.input_count;++j){if(j)o<<',';const auto &e=r.elements[j];
      o<<"{\"semantic\":"<<std::quoted(r.semantics[j].data())<<",\"index\":"<<e.SemanticIndex<<",\"format\":"<<e.Format
       <<",\"slot\":"<<e.InputSlot<<",\"offset\":"<<e.AlignedByteOffset<<",\"class\":"<<e.InputSlotClass<<",\"step\":"<<e.InstanceDataStepRate<<'}';}
    o<<"],\"shaders\":[";for(unsigned j=0;j<2;++j){if(j)o<<',';const auto &s=r.shaders[j];
      o<<"{\"size\":"<<s.size<<",\"saved\":"<<s.saved<<",\"file\":"<<std::quoted(s.file)<<",\"sha256\":"<<std::quoted(s.sha)<<'}';}o<<"]}";
  }o<<"]}";
}
#ifdef MHW_TEMPORAL_FIXTURE
void fixture_start(const tb::Config &c,void *forward){cfg=c;configured=true;original=forward;accepting.store(true);}
unsigned fixture_publications() noexcept{unsigned n{};for(auto &phase:phases)if(phase.load()>=2)++n;return n;}
#endif
}
extern "C" void mhw_scaled_draw_execution(mhw::projection_binding::Frame *f){mhw::scaled_temporal::draw_execution::dispatch(f);}
