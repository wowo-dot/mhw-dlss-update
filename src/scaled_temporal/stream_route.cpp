#include "stream_route.hpp"
#include "effects.hpp"
#include "color_dependencies.hpp"
#include "color_chain.hpp"
#include "dependency_binding_trace.hpp"
#include "stream_lifetime.hpp"
#include "readiness_trace.hpp"
#include "effect_order.hpp"
#include "screen_filters.hpp"
#include "output_diagnostics.hpp"
#include "output_dependencies.hpp"
#include "prepared_queue.hpp"
#include "frame_handoff.hpp"
#include "draw_state_trace.hpp"
#include "recording_states.hpp"
#include "compute_resource_trace.hpp"
#include "compute_depth_pixels.hpp"
#include "dof_native_sequence.hpp"
#include "overlay_extent.hpp"
#include "silhouette_shader.hpp"
#include "silhouette_draw.hpp"
#include "watch_filter.hpp"
#ifdef MHW_FG_SCENE_PROBE
#include "../frame_generation/scene_trace.hpp"
#endif
#include "../projection_binding/memory.hpp"
#include "../temporal_present/observer.hpp"
#include "../perf/toggles.hpp"
#include "../perf/hook_timing.hpp"
#include <MinHook.h>
#include <wrl/client.h>
#include <atomic>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <intrin.h>
#include <ostream>
#include <stdexcept>
extern "C" void scaled_postfilter_entry();
extern "C" void scaled_draw_execution_entry();
extern "C" void scaled_stream_final_entry();
extern "C" void scaled_copy_execution_entry();
extern "C" void scaled_indexed_execution_entry();
extern "C" void mhw_binding_forward(void *,mhw::projection_binding::Frame *);
namespace mhw::scaled_temporal::stream_route {
namespace {
namespace tb=temporal_bindings;namespace pb=projection_binding;namespace obs=temporal_present::observer;
using Microsoft::WRL::ComPtr;
using CopyFn=void(WINAPI *)(ID3D12GraphicsCommandList *,const D3D12_TEXTURE_COPY_LOCATION *,UINT,UINT,UINT,const D3D12_TEXTURE_COPY_LOCATION *,const D3D12_BOX *);
constexpr unsigned shader_limit=16384;
constexpr unsigned resource_capacity=32;
struct Shader {std::uintptr_t entry{},descriptor{},data{};unsigned size{};std::array<std::byte,shader_limit> bytes{};};
struct Record {
 tb::Owner output;pb::Draw draw;pb::AttachmentSnapshot before;
 std::array<Shader,2> shaders;
 std::array<D3D12_INPUT_ELEMENT_DESC,32> elements{},recipe_elements{};std::array<std::array<char,64>,32> semantics{};
 std::array<unsigned,2> arguments{};std::array<int,4> rectangle{};
 std::array<std::uintptr_t,4> packet_bindings{};
 std::array<float,16> vertices{};unsigned vertex_floats{};
 unsigned vertex_stride{},vertex_offset{};OverlayVertexLayout overlay_layout{};
 unsigned dependency{}; // 0: native auxiliary overlay, 1: scene copy, 2: radial, 3: DOF composite, 4: screen filter
 const ScreenFilter *screen{}; // dependency 4: the approved entry
 BindingCheck binding_entry;bool binding_ready{};
 std::array<std::byte,112> dependency_vertices{};
 std::uintptr_t dependency_source{};
 std::array<unsigned,5> reads{};unsigned read_count{};
 std::string signature;
};
struct HazeBindings {
 unsigned count{};
 std::array<unsigned,64> ids{};
 std::array<unsigned char,64> modes{};
 std::array<std::uintptr_t,64> objects{};
 // Only the first count/start pair is traversed by native 0x259f330.
 // The remaining eight bytes are opaque metadata, not additional ranges.
 std::array<std::array<unsigned short,2>,2> ranges{};
 std::array<std::array<unsigned char,8>,2> metadata{};
 std::array<std::array<unsigned,128>,2> words{};
 std::array<tb::Owner,5> textures{}; // PS t0..t4, engine metadata only
 std::array<std::uintptr_t,5> views{},descriptors{};
 std::array<unsigned,5> indices{};
 bool operator==(const HazeBindings &)const=default;
};
struct SilhouetteCpu {
 pb::NativePipeline native;
 Record layout;
 struct Code {std::uintptr_t entry{},descriptor{},data{};unsigned size{};};
 std::array<Code,2> identity;
 std::array<std::vector<std::byte>,2> bytes;
 std::uintptr_t set{};
 ShaderHash vs_hash{},ps_hash{};  // for diagnostics: which programs a rejected draw used
};
struct LatePacket {
 std::array<std::uintptr_t,4> bindings{};std::array<unsigned,3> arguments{};
 std::shared_ptr<SilhouetteCpu> recipe;pb::NativePipeline native;
 bool silent{};  // writes no colour, depth or stencil (silhouette_silent): stays native
};
struct Plan {
 tb::Snapshot stamp;tb::Packet temporal;std::uintptr_t copy{};
 std::array<std::byte,64> copy_fields{};std::array<Record,12> draws;
 std::array<ComPtr<ID3D12Resource>,resource_capacity> resources;
 unsigned extra_resources{};
 std::array<HazeBindings,8> dependency_bindings{};
 std::array<HazeBindings,8> dependency_current{};
 HazeBindings haze;bool has_haze{};
 std::uintptr_t grade_input_resource{};
 std::array<OutputDiagnostics::Draw,12> observations{};
 unsigned overlay_count{},ordered_count{};
 // The draw of a filter between the TAA copy and the motion filter (game-40:
 // uDepthColorFilter in the Fatalis smoke): an overlay slot with dependency 4
 // (an approved screen filter) that runs first.
 unsigned pre_stage{UINT_MAX};
 std::array<unsigned,12> order{},keys{},targets{};
 unsigned executed{};bool published{},built{},claimed{};
 std::uintptr_t motion_unit{};unsigned motion_thread{},motion_visits{};
 bool motion_returned{},queue_finished{};
 // Full-output ownership is fixed before native recording workers start.
 // GPU order is proved separately from the order in which workers visit us.
 FrameHandoff handoff;
 std::shared_ptr<DofNativeSequence> dof;
 ComPtr<ID3D12Resource> selector; // one immutable GPU choice per prepared frame
 struct Recording {
  ID3D12GraphicsCommandList *list{};std::uint64_t generation{};
  std::array<bool,resource_capacity> known{};std::array<unsigned,resource_capacity> states{};
 };
 std::array<Recording,8> recordings{};
 bool prepared{},resolved{},copied{},completed{};unsigned recorded_draws{};
 // Render scale 1.0 (DLAA, dlaa-identity-path.md): the frame ends with a native
 // copy from the grade output to the final attachment, not the System_Copy
 // quad. No effect is replayed; the plan holds the resolve, the TAA hand-off
 // copy and that final copy (handoff use 2; resources 2 and 3 its source and
 // destination).
 bool identity{},final_copied{};std::uintptr_t final_copy{};std::array<std::byte,64> final_copy_fields{};
 // Late indexed packets are frozen before recording workers start, sorted
 // in native key/insertion order and included in GPU ownership. About 17 per
 // frame in combat; Alatreon's arrival exceeded the former 32 (game-45).
 static constexpr unsigned late_capacity=64;
 std::array<std::uintptr_t,late_capacity> late_packets{};
 std::array<unsigned,late_capacity> late_keys{};std::array<LatePacket,late_capacity> late{};
 unsigned late_count{},late_recorded{};std::uint64_t late_executed{};
};
unsigned effect_count(const Plan &p){return p.identity?0U:(p.has_haze?4U:3U)+p.overlay_count;}
bool shared_overlay_key(const Plan &p){
 for(unsigned i=0;i<p.overlay_count;++i)for(unsigned j=0;j<i;++j)if(p.keys[4+i]==p.keys[4+j])return true;
 return false;
}
unsigned effect_stage(const Plan &p,unsigned ordinal){return p.ordered_count?p.order[ordinal]:unordered_effect_stage(p.has_haze,ordinal);}
unsigned effect_ordinal(const Plan &p,unsigned stage){for(unsigned i=0;i<effect_count(p);++i)if(effect_stage(p,i)==stage)return i;return UINT_MAX;}
unsigned handoff_draw(const Plan &p,unsigned stage){
 const auto ordinal=effect_ordinal(p,stage);
 return 2+ordinal+(p.dof&&ordinal>=effect_ordinal(p,p.dof->stage)?6:0)+(stage==2?p.late_count:0);
}
unsigned handoff_indexed(const Plan &p,unsigned position){return 1+effect_count(p)+(p.dof?6:0)+position;}
unsigned handoff_compute(const Plan &p,unsigned pass){return 2+effect_ordinal(p,p.dof->stage)+pass;}
unsigned handoff_count(const Plan &p){return 2+effect_count(p)+(p.dof?6:0)+p.late_count+(p.identity?1U:0U);}
unsigned output_index(const Plan &p,unsigned stage){return stage<4?2+stage:p.targets[stage];}
unsigned resource_count(const Plan &p){return p.identity?4U:(p.has_haze?9U:5U)+p.extra_resources;}
struct Pipeline {std::unique_ptr<Effect> effect;std::string signature;ComPtr<ID3D12RootSignature> root;};
tb::Config cfg;unsigned width{},height{};
// The game's render extent inside the output (render_resolution::plan): 2:1
// for even scenes, truncated per axis for a pillarboxed odd scene (3413 -> 1706).
unsigned render_w{},render_h{};
// The first render-extent mismatch of each kind, for scenes whose render extent
// is not an exact half: a draw vertex {stage, index, observed bits, expected
// bits}; a draw rectangle {stage, left, top, right, bottom}; the final quad's
// source rectangle and the scene copy box {left, top, right, bottom}.
std::array<unsigned,4> vertex_miss{},final_miss{},copy_miss{};std::array<int,5> rect_miss{};
bool vertex_missed{},rect_missed{},final_missed{},copy_missed{};
std::array<Plan,32> plans;
// perf::packet_filter (v0.12): the packets and resources each ring slot's plan
// can match, written under the route lock when the plan is built and cleared
// when the slot is published again. A hook takes the route lock only for a
// listed packet, or a barrier on a listed resource; any other call cannot
// match a plan and is skipped without it. A stale entry only costs a locked
// check. The slot being rewritten holds a plan whose packets have not run yet.
constexpr unsigned watch_packets=96;
struct Watch {
 std::array<std::atomic<std::uintptr_t>,watch_packets> packets{};std::array<std::atomic<std::uintptr_t>,resource_capacity> resources{};
 std::atomic<unsigned> packet_count{},resource_count{};std::atomic<bool> all{};
 PlanFilter packet_filter,resource_filter;  // v0.24.3: this slot's lists (watch_filter.hpp)
};
std::array<Watch,32> watches;
std::atomic<std::uint32_t> watch_mask{};
// The union of every watched slot's filters: most hooked calls (every engine
// draw, every barrier) match no plan and are rejected here.
UnionFilter any_packet,any_resource;
static_assert(watches.size()==plans.size()&&plans.size()<=32,"one watch bit per plan slot");
bool filtering() noexcept {return perf::on(perf::packet_filter);}
// Under the route lock.
void unwatch(unsigned slot) noexcept {
 const auto mask=watch_mask.fetch_and(~(1U<<slot),std::memory_order_acq_rel)&~(1U<<slot);auto &w=watches[slot];
 w.packet_count.store(0,std::memory_order_release);w.resource_count.store(0,std::memory_order_release);w.all.store(false,std::memory_order_release);
 w.packet_filter.clear();w.resource_filter.clear();
 // The unions rebuilt from the slots still watched.
 std::array<std::uint64_t,UnionFilter::words> packets{},resources{};bool overflow=false;
 for(auto m=mask;m;m&=m-1){const auto &x=watches[static_cast<unsigned>(std::countr_zero(m))];
  overflow=overflow||x.all.load(std::memory_order_relaxed);
  for(unsigned i=0,n=x.packet_count.load(std::memory_order_relaxed);i<n;++i)UnionFilter::set(packets,x.packets[i].load(std::memory_order_relaxed));
  for(unsigned i=0,n=x.resource_count.load(std::memory_order_relaxed);i<n;++i)UnionFilter::set(resources,x.resources[i].load(std::memory_order_relaxed));}
 if(overflow)packets.fill(~0ULL);
 any_packet.assign(packets);any_resource.assign(resources);
}
template<class Fn> bool watched(Fn &&listed) noexcept {
 for(auto mask=watch_mask.load(std::memory_order_acquire);mask;mask&=mask-1){
  const auto &w=watches[static_cast<unsigned>(std::countr_zero(mask))];if(w.all.load(std::memory_order_acquire)||listed(w))return true;}
 return false;
}
bool watched_packet(std::uintptr_t packet) noexcept {
 if(!any_packet.may_contain(packet))return false;
 return watched([&](const Watch &w){if(!w.packet_filter.may_contain(packet))return false;const auto n=w.packet_count.load(std::memory_order_acquire);
  for(unsigned i=0;i<n;++i)if(w.packets[i].load(std::memory_order_relaxed)==packet)return true;return false;});
}
bool watched_resource(std::uintptr_t resource) noexcept {
 if(!any_resource.may_contain(resource))return false;
 return watched([&](const Watch &w){if(!w.resource_filter.may_contain(resource))return false;const auto n=w.resource_count.load(std::memory_order_acquire);
  for(unsigned i=0;i<n;++i)if(w.resources[i].load(std::memory_order_relaxed)==resource)return true;return false;});
}
// Under the route lock: when a plan is built, and again when a dependency
// refresh pins another resource for it. Rewritten entries keep their values.
void watch(unsigned slot,const Plan &p) noexcept {
 auto &w=watches[slot];unsigned n{},r{};bool overflow=false;
 // Filter bits first (only added while the slot is watched), then the lists.
 const auto add=[&](std::uintptr_t packet){if(!packet)return;if(n==watch_packets){overflow=true;return;}
  w.packet_filter.add(packet);any_packet.add(packet);w.packets[n++].store(packet,std::memory_order_relaxed);};
 for(const auto &d:p.draws)add(d.draw.packet);
 for(unsigned i=0;i<p.late_count&&i<p.late_packets.size();++i)add(p.late_packets[i]);
 if(p.dof)for(const auto &d:p.dof->passes)add(d.packet);
 add(p.copy);add(p.final_copy);add(p.temporal.object);
 for(unsigned i=0;i<resource_count(p)&&i<p.resources.size();++i)
  if(auto *x=p.resources[i].Get()){const auto address=reinterpret_cast<std::uintptr_t>(x);
   w.resource_filter.add(address);any_resource.add(address);w.resources[r++].store(address,std::memory_order_relaxed);}
 if(overflow){w.packet_filter.fill();any_packet.fill();}
 w.all.store(overflow,std::memory_order_release);
 w.packet_count.store(n,std::memory_order_release);w.resource_count.store(r,std::memory_order_release);
 watch_mask.fetch_or(1U<<slot,std::memory_order_release);
}
// tBaseMap is a mutable shader binding. Capture it while the final quad is
// enqueued, before later filter preparation replaces that context slot.
struct FinalQuad {
 tb::Snapshot stamp;tb::Owner source,destination;std::uintptr_t attachment{},packet{},queue{};
 unsigned before{},after{},base_id{};bool entered{},valid{};
};
std::array<FinalQuad,32> final_quads;
std::array<Pipeline,4> pipelines;
// Immutable variants are never evicted while native recordings may use them.
// Exhaustion leaves a new recipe on the native path instead of replacing PSOs.
std::array<Pipeline,16> dependency_pipelines;
Pipeline *cached_dependency(const Record &r){for(auto &p:dependency_pipelines)if(p.effect&&p.signature==r.signature)return &p;return nullptr;}
Pipeline *draw_pipeline(const Plan &p,unsigned stage){return stage<4?&pipelines[stage]:p.draws[stage].dependency?cached_dependency(p.draws[stage]):nullptr;}
bool needs_pipeline(const Plan &p,unsigned stage){return stage<4||p.draws[stage].dependency;}
unsigned recipe_stage(const Record &r,unsigned stage){return r.dependency?3+r.dependency:stage>=4?0:stage;}
StreamLifetime lifetime;
std::array<ComputeDepthPixels,2> compute_depth_pixels;
ReadinessTrace readiness;
OutputDiagnostics diagnostics;
OutputDependencies dependencies,intermediate_dependencies,inserted_dependencies;
bool inserted_saved{};
OutputDependencies compute_dependencies{true};
DrawStateTrace draw_state_trace;
using StateJournal=RecordingStates<128,32,ComPtr<ID3D12Resource>>;
StateJournal recording_states;
std::uint64_t state_observations{},state_unscoped{},state_imports{};
std::uint64_t overlay_planned{},overlay_draws{},overlay_completed{},overlay_rejected{};
std::uint64_t overlay_shared_prepared{},overlay_shared_completed{};
std::uint64_t overlay_extended_draws{};std::array<unsigned,6> overlay_extended_layout{};
prepared_queue::Recorder prepared_queues;
struct Active {unsigned slot{};ID3D12GraphicsCommandList *list{};std::uint64_t generation{};std::uintptr_t recording{};
 bool resolved{},copied{};unsigned draws{};std::array<bool,9> known{};std::array<unsigned,9> states{};} active;
std::array<std::uintptr_t,6> targets{};std::array<void *,6> originals{};std::array<bool,6> enabled{};
// AL: hook 2 (CopyTextureRegion) is an AL chain, live while the runtime
// has the stream hooks applied (chains_live), otherwise continuing straight on.
constexpr unsigned api_hook=2;
std::atomic<bool> chain_live{};
std::atomic<bool> accepting{};std::atomic<unsigned> pending{},fault{};
std::uint64_t published{},built{},unqualified{},claimed{},copies{},draws{},warmups{},complete{};
std::uint64_t haze_claimed{},haze_complete{},haze_draws{};
std::uint64_t final_captured{},final_rejected{};
std::uint64_t identity_plans{},identity_final_copies{},identity_observed{};  // render scale 1.0 (DLAA)
std::uint64_t spatial_frames{},native_frames{},prepared_frames{},early_draws{},multilist_frames{},submitted_frames{};
std::array<std::uint64_t,19> rejection_counts{};  // rejected plan builds by graph reason (18: other)
std::uint64_t summarized_rejections{};
bool abandoned{};
std::string failure;unsigned plan_rejection{},plan_detail{},plan_qualification{};
std::array<std::uint64_t,10> plan_vertex_gate{};
SRWLOCK mutex=SRWLOCK_INIT;
struct Lock {Lock(){AcquireSRWLockExclusive(&mutex);}~Lock(){ReleaseSRWLockExclusive(&mutex);}};
struct Guard {Guard(){++pending;}~Guard(){--pending;}};
thread_local bool injecting{};
thread_local unsigned api_slot=UINT_MAX,api_stage=UINT_MAX,api_calls{};
thread_local std::uintptr_t api_state{};
thread_local pb::Draw api_draw;
thread_local pb::AttachmentSnapshot api_before;
thread_local unsigned copy_slot=UINT_MAX,copy_calls{},resolve_slot=UINT_MAX;
thread_local bool copy_final{};  // the executing copy is an identity plan's final copy
thread_local std::uintptr_t copy_recording{};
void fail(unsigned n){unsigned zero{};fault.compare_exchange_strong(zero,n);accepting=false;}
template<class T>bool field(std::uintptr_t at,unsigned off,T &v){return at&&at<=UINTPTR_MAX-off-sizeof(T)&&pb::copy_memory(at+off,&v,sizeof(T));}
bool read(void *,std::uintptr_t at,std::span<std::byte> b){return pb::copy_memory(at,b.data(),static_cast<unsigned>(b.size()));}
const tb::Reader reader{nullptr,read};
#ifdef MHW_FG_SCENE_PROBE
#include "scene_probe.inl"
#endif
bool same_stamp(const tb::Snapshot &);
std::shared_ptr<SilhouetteCpu> silhouette_cpu(std::uintptr_t packet,bool &silent);
std::uint64_t silhouette_planned{};
#include "silhouette_probe.inl"
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
ID3D12Resource *vertex_resource(const Record &r,const D3D12_VERTEX_BUFFER_VIEW &view) noexcept {
  const auto ia=r.packet_bindings[1];
  std::uintptr_t owner{},backend{},candidate{},base{},check{};unsigned bytes{},offset{};unsigned char stride{};
  if(!field(ia,0,owner)||!field(owner,0x60,backend)||!field(backend,0x20,candidate)||!candidate||
     !field(backend,0x40,base)||!field(backend,0x10,bytes)||!field(ia,0x24,offset)||!field(ia,0x20,stride)||
     !stride||stride!=r.vertex_stride||offset!=r.vertex_offset||offset>bytes||base>UINTPTR_MAX-offset||
     view.BufferLocation!=base+offset||view.SizeInBytes!=bytes-offset||view.StrideInBytes!=stride||
     (UINT64(r.arguments[1])+r.arguments[0])*stride>view.SizeInBytes)return nullptr;
  for(unsigned slot:{0U,2U,7U,10U,11U})if(!pb::system_method(candidate,slot,check))return nullptr;
  ID3D12Resource *held{};ID3D12Device *device{};bool ok=false;
  __try{auto *p=reinterpret_cast<ID3D12Resource *>(candidate);
    if(SUCCEEDED(p->QueryInterface(IID_PPV_ARGS(&held)))&&held==p&&SUCCEEDED(held->GetDevice(IID_PPV_ARGS(&device)))&&reinterpret_cast<std::uintptr_t>(device)==cfg.device){
      const auto desc=held->GetDesc();ok=desc.Dimension==D3D12_RESOURCE_DIMENSION_BUFFER&&desc.Width>=bytes&&held->GetGPUVirtualAddress()==base&&
        field(owner,0x60,check)&&check==backend&&field(backend,0x20,check)&&check==candidate;}
  }__except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
  if(device)device->Release();if(!ok&&held){held->Release();held=nullptr;}return held;
}
std::string signature(const EffectRecipe &r){
    if(r.vertex_shader.size()>262144||r.pixel_shader.size()>262144||r.native_vertices.size()>16||r.dependency_vertices.size()>112||r.input_layout.size()>32)return {};
    std::string s=pb::bytes_sha256(r.vertex_shader.data(),static_cast<unsigned>(r.vertex_shader.size()))+pb::bytes_sha256(r.pixel_shader.data(),static_cast<unsigned>(r.pixel_shader.size()));
    s+=pb::bytes_sha256(&r.blend,sizeof(r.blend))+pb::bytes_sha256(&r.raster,sizeof(r.raster))+pb::bytes_sha256(&r.depth,sizeof(r.depth));
    s+=pb::bytes_sha256(r.native_vertices.data(),static_cast<unsigned>(r.native_vertices.size_bytes()));
    if(!r.dependency_vertices.empty())s+=pb::bytes_sha256(r.dependency_vertices.data(),static_cast<unsigned>(r.dependency_vertices.size()));
    for(const auto &e:r.input_layout){if(!e.SemanticName)return {};s+=e.SemanticName;
        const std::array<unsigned,6> fields{e.SemanticIndex,unsigned(e.Format),e.InputSlot,e.AlignedByteOffset,unsigned(e.InputSlotClass),e.InstanceDataStepRate};s+=pb::bytes_sha256(fields.data(),sizeof(fields));}
    const std::array<unsigned,5> fields{unsigned(r.stage),r.geometry.render_width,r.geometry.render_height,r.geometry.output_width,r.geometry.output_height};
    return s+pb::bytes_sha256(fields.data(),sizeof(fields));
}

EffectRecipe recipe(Record &r,unsigned stage,ID3D12RootSignature *root){
 EffectRecipe result;result.stage=static_cast<EffectStage>(stage);result.geometry={render_w,render_h,width,height};result.root=root;
 result.vertex_shader=std::span(r.shaders[0].bytes).first(r.shaders[0].size);result.pixel_shader=std::span(r.shaders[1].bytes).first(r.shaders[1].size);
 static_assert(sizeof(result.depth)==sizeof(r.before.pipeline.depth)&&sizeof(result.blend)==sizeof(r.before.pipeline.blend)&&sizeof(result.raster)==sizeof(r.before.pipeline.raster));
 std::memcpy(&result.depth,r.before.pipeline.depth.data(),sizeof(result.depth));std::memcpy(&result.blend,r.before.pipeline.blend.data(),sizeof(result.blend));std::memcpy(&result.raster,r.before.pipeline.raster.data(),sizeof(result.raster));
 // Stored descriptor pointers refer to native strings. Use our bounded copies.
 r.recipe_elements=r.elements;for(unsigned i=0;i<r.before.pipeline.input_count;++i)r.recipe_elements[i].SemanticName=r.semantics[i].data();
 result.input_layout=std::span(r.recipe_elements).first(r.before.pipeline.input_count);result.native_vertices=std::span(r.vertices).first(r.vertex_floats);
 if(r.dependency&&r.dependency!=4)result.dependency_vertices=std::span(r.dependency_vertices).first(r.dependency==1?64:112);
 return result;
}
bool geometry(Record &r,std::uintptr_t context,unsigned stage){
 // A screen-filter draw holds an overlay slot but is a base-style screen
 // triangle (two-element layout, native vertices), like haze.
 const bool screen=stage>=4&&r.dependency==4,overlay=stage>=4&&!screen;
 const auto kind=screen?EffectStage::screen_filter:static_cast<EffectStage>(stage);
 plan_vertex_gate={};
 plan_qualification=(stage<<16)|1U;
 if(!source(r)||r.arguments[0]!=(overlay?4U:stage>=2?3U:4U))return false;
 plan_qualification=(stage<<16)|2U;
 unsigned flags{},format{};unsigned char rotating{};std::uintptr_t view{},backend{},resource{},depth{};
 if(!field(r.packet_bindings[0],0x48,flags)||(flags&7)!=1||!field(r.packet_bindings[0],0x40,depth)||depth||
    !field(r.packet_bindings[0],0,view)||!field(view,0x60,backend)||!field(backend,0x18,resource)||resource!=r.output.resource||
    !field(backend,0x20,format)||format!=static_cast<unsigned>(r.dependency==1?DXGI_FORMAT_R10G10B10A2_UNORM:DXGI_FORMAT_R11G11B10_FLOAT)||!field(backend,0x28,rotating)||rotating)return false;
 unsigned char stride{};unsigned offset{};std::uintptr_t arena{};
 plan_qualification=(stage<<16)|3U;
 // Keep short-circuit receipts; overlays additionally decode the actual layout.
 auto &vg=plan_vertex_gate;vg={r.draw.packet,r.packet_bindings[1],context,r.arguments[0],r.arguments[1],0,0,0,0,0};
 if(!field(r.packet_bindings[1],0x20,stride)){vg[9]=1;return false;}vg[5]|=1;vg[6]=stride;
 if(overlay?stride<16:stride!=16){vg[9]=2;return false;}
 if(!field(r.packet_bindings[1],0x24,offset)){vg[9]=3;return false;}vg[5]|=2;vg[7]=offset;
 if(offset>16777216){vg[9]=4;return false;}
 if(!field(context,0x2b50,arena)){vg[9]=5;return false;}vg[5]|=4;vg[8]=arena;
 if(!arena){vg[9]=6;return false;}
 if(arena>UINTPTR_MAX-33554432){vg[9]=7;return false;}
 plan_vertex_gate={};
 r.vertex_stride=stride;r.vertex_offset=offset;
 r.vertex_floats=r.arguments[0]*4;
 plan_qualification=(stage<<16)|4U;
 // Both the larger stride and first-vertex displacement must fit the same
 // bounded arena window. Do not rely on the old packed-vertex arithmetic.
 const auto vertex_start=UINT64(offset)+UINT64(r.arguments[1])*stride;
 const auto vertex_bytes=UINT64(r.arguments[0])*stride;
 if(vertex_start+vertex_bytes>33554432)return false;
 if(!overlay){
  if(!pb::copy_memory(arena+vertex_start,r.vertices.data(),r.vertex_floats*4))return false;
  plan_qualification=(stage<<16)|5U;
  const auto expected=effect_vertices(kind,{render_w,render_h,width,height},false);
  for(unsigned i=0;i<r.vertex_floats;++i)if(!std::isfinite(r.vertices[i])||std::abs(r.vertices[i]-expected.values[i])>0.000001F){
   if(!vertex_missed){vertex_missed=true;vertex_miss={stage,i,std::bit_cast<unsigned>(r.vertices[i]),std::bit_cast<unsigned>(expected.values[i])};}
   return false;}}
 const auto w=stage==2?width:render_w,h=stage==2?height:render_h;
 plan_qualification=(stage<<16)|6U;
 if(r.rectangle!=std::array<int,4>{0,0,static_cast<int>(w),static_cast<int>(h)}&&!rect_missed){
  rect_missed=true;rect_miss={static_cast<int>(stage),r.rectangle[0],r.rectangle[1],r.rectangle[2],r.rectangle[3]};}
 if(r.rectangle!=std::array<int,4>{0,0,static_cast<int>(w),static_cast<int>(h)}||
    !pb::snapshot_native_pipeline(cfg.image,r.draw.packet,r.before.pipeline)||
    (overlay?(r.before.pipeline.input_count<2||r.before.pipeline.input_count>32):r.before.pipeline.input_count!=2)||
    !field(r.before.pipeline.shader_handle,0,r.draw.shader_set)||
    !shader(r.shaders[0],r.draw.shader_set,0x50,1)||!shader(r.shaders[1],r.draw.shader_set,0x58,0)||!layout(r,false))return false;
 plan_qualification=(stage<<16)|7U;
 for(unsigned off:{0x60U,0x68U,0x70U}){std::uintptr_t other{};if(!field(r.draw.shader_set,off,other)||other)return false;}
 plan_qualification=(stage<<16)|8U;
 if(overlay&&!r.dependency&&(r.shaders[0].size!=644||pb::bytes_sha256(r.shaders[0].bytes.data(),644)!="730b7d23bcdca40477ed973c29c8fff8de7ab7244b05d142d5f9458fbfec5fdd"||
     !overlay_pixel_shader(std::span(r.shaders[1].bytes).first(r.shaders[1].size))))return false;
 if(r.dependency&&!screen){
  plan_qualification=(stage<<16)|5U;
  if(stride!=(r.dependency==1?16U:28U)||vertex_bytes>r.dependency_vertices.size()||
     !pb::copy_memory(arena+vertex_start,r.dependency_vertices.data(),static_cast<unsigned>(vertex_bytes))||
     !dependency_geometry(recipe(r,recipe_stage(r,stage),nullptr)))return false;
 }else if(overlay){
  plan_qualification=(stage<<16)|5U;
  // recipe() substitutes bounded semantic-name copies; no native strings are
  // dereferenced by the decoder. Preserve the original GPU vertices and PSO.
  if(!overlay_vertex_layout(recipe(r,0,nullptr).input_layout,stride,r.overlay_layout))return false;
  std::array<std::byte,4*255> raw{};
  plan_qualification=(stage<<16)|4U;
  if(!pb::copy_memory(arena+vertex_start,raw.data(),static_cast<unsigned>(vertex_bytes)))return false;
  plan_qualification=(stage<<16)|5U;
  if(!overlay_vertices(std::span(raw).first(static_cast<unsigned>(vertex_bytes)),r.overlay_layout,r.vertices))return false;
 }
 plan_qualification=(stage<<16)|9U;
 r.signature=signature(recipe(r,recipe_stage(r,stage),nullptr));if(r.signature.empty())return false;
 // A new recipe's pipeline state is checked here, so an unexpected one leaves
 // the frame native instead of failing the route when its effect is built.
 plan_qualification=(stage<<16)|15U;
 if(screen&&!effect_admitted(recipe(r,recipe_stage(r,stage),nullptr)))return false;
 plan_qualification=0;return true;
}
bool overlay_inputs(const Plan &p,const Record &r){
 // Keep native UVs only for native auxiliary inputs. Sampling a reconstructed
 // color surface would require a different UV-domain contract, so reject it.
 unsigned n{};std::uintptr_t words{},objects{};std::array<unsigned short,2> range{};
 if(!field(r.draw.shader_set,0x88,n)||n>64||!field(r.draw.shader_set,0,words)||!field(r.draw.shader_set,0x14,range)||
    range[0]>128||unsigned(range[0])+range[1]>4096)return false;
 objects=r.packet_bindings[2]+0x30;
 for(unsigned i=0;i<range[0];++i){unsigned word{};
  if(!field(words,4U*(range[1]+i),word))return false;
  const unsigned kind=word&15U,index=(word>>4)&255U;if(kind>2||(kind&&index>=n))return false;
  if(kind!=2)continue;
  std::uintptr_t object{};tb::Owner owner_value;
  if(!field(objects,8*index,object)||!tb::read_owner(reader,object,owner_value))return false;
  for(unsigned j=0;j<(p.has_haze?6U:5U);++j)if(owner_value.resource==reinterpret_cast<std::uintptr_t>(p.resources[j].Get()))return false;
  for(unsigned j=0;j<p.overlay_count;++j)if(p.draws[4+j].dependency&&owner_value.resource==p.draws[4+j].output.resource)return false;
 }
 return true;
}
bool same_stamp(const tb::Snapshot &s){
 std::uint64_t frame{},epoch{};std::uintptr_t view{},primary{};unsigned bank{};unsigned char async{};
 return field(cfg.renderer,0x1f210,frame)&&frame==s.frame&&field(cfg.renderer,0x1f208,epoch)&&epoch==s.epoch&&
  field(cfg.renderer,0x1f200,bank)&&bank==s.bank&&field(cfg.renderer,0x93,async)&&async==s.async&&
  field(s.context,0x1d0,view)&&view==s.view&&field(cfg.renderer,0x1f450,primary)&&primary==view;
}
bool owner(std::uintptr_t at,tb::Owner &out){return tb::read_owner(reader,at,out)&&out.width==width&&out.height==height&&out.resource;}
bool final_identity(const FinalQuad &q,const tb::Snapshot &s){
 std::uintptr_t attachment{},destination{};tb::Owner a{},b{};
 return q.valid&&q.stamp.frame==s.frame&&q.stamp.epoch==s.epoch&&q.stamp.context==s.context&&q.stamp.view==s.view&&
  q.stamp.bank==s.bank&&q.stamp.async==s.async&&same_stamp(s)&&
  field(s.view,0x460,attachment)&&attachment==q.attachment&&field(attachment,0x58,destination)&&destination==q.destination.object&&
  tb::read_owner(reader,q.source.object,a)&&a==q.source&&tb::read_owner(reader,q.destination.object,b)&&b==q.destination;
}
unsigned final_before(const pb::Frame &f){
 if(!accepting.load()||f.caller!=cfg.image+0x23c7b08||f.gpr[2]!=cfg.renderer+0x710)return UINT_MAX;
 Lock lock;std::uint64_t frame{};if(!field(cfg.renderer,0x1f210,frame)||!frame)return UINT_MAX;
 const auto slot=static_cast<unsigned>(frame%final_quads.size());auto &q=final_quads[slot];
 if((q.entered||q.valid)&&q.stamp.frame==frame){q.valid=false;++final_rejected;return UINT_MAX;}
 q={};q.stamp.frame=frame;q.stamp.context=f.gpr[2];auto &s=q.stamp;
 std::uintptr_t source{},destination{};std::array<unsigned,4> rectangle{},viewport{};std::array<unsigned,2> dimensions{};std::array<float,2> offset{};
 // The stored identity is widened, but the native flag occupies one byte.
 unsigned char async{};
 if(!field(cfg.renderer,0x93,async)){++final_rejected;return UINT_MAX;}s.async=async;
 const bool ok=field(cfg.renderer,0x1f208,s.epoch)&&field(cfg.renderer,0x1f200,s.bank)&&
  field(s.context,0x1d0,s.view)&&same_stamp(s)&&
  field(f.gpr[3],0,rectangle)&&rectangle==std::array<unsigned,4>{0,0,render_w,render_h}&&
  field(f.gpr[4],0,dimensions)&&dimensions==std::array<unsigned,2>{width,height}&&
  field(f.stack[0],0,offset)&&offset==std::array<float,2>{-.5F/width,-.5F/height}&&(f.stack[1]&255)==1&&
  field(s.context,0x5c,viewport)&&viewport==std::array<unsigned,4>{0,0,width,height}&&
  field(cfg.image,0x4be7450,q.base_id)&&q.base_id!=UINT_MAX&&field(s.context,0x710+8*(q.base_id&0xfff),source)&&owner(source,q.source)&&
  field(s.view,0x460,q.attachment)&&field(q.attachment,0x58,destination)&&owner(destination,q.destination)&&q.source.resource!=q.destination.resource&&
  field(s.context,0x2b80,q.before)&&q.before<65534&&field(s.context,0x2b78,q.queue)&&q.queue&&q.queue<=UINTPTR_MAX-16ULL*(q.before+1);
 if(!ok){++final_rejected;
  if(rectangle[2]&&rectangle!=std::array<unsigned,4>{0,0,render_w,render_h}&&!final_missed){final_missed=true;final_miss=rectangle;}
  return UINT_MAX;}q.entered=true;return slot;
}
void final_after(unsigned slot,bool returned){
 if(slot==UINT_MAX)return;Lock lock;auto &q=final_quads[slot];
 std::uintptr_t source{},queue{},packet{};unsigned key{},opcode{};
 q.valid=returned&&q.entered&&same_stamp(q.stamp)&&field(q.stamp.context,0x710+8*(q.base_id&0xfff),source)&&source==q.source.object&&
  field(q.stamp.context,0x2b80,q.after)&&q.after==q.before+1&&field(q.stamp.context,0x2b78,queue)&&queue==q.queue&&
  field(queue+16ULL*q.before,0,key)&&key==0x80000000&&field(queue+16ULL*q.before,8,packet)&&field(packet,0,opcode)&&(opcode&31)==0;
 q.entered=false;q.packet=packet;q.valid=q.valid&&final_identity(q,q.stamp);
 if(q.valid)++final_captured;else ++final_rejected;
}
#include "haze_capture.inl"
#include "haze_route.inl"
bool pin(const tb::Owner &o,ComPtr<ID3D12Resource> &out){
 std::uintptr_t method{};for(unsigned slot:{0U,2U,7U,10U})if(!pb::system_method(o.resource,slot,method))return false;
 auto *r=reinterpret_cast<ID3D12Resource *>(o.resource);ComPtr<ID3D12Device> d;
 if(FAILED(r->GetDevice(IID_PPV_ARGS(&d)))||reinterpret_cast<std::uintptr_t>(d.Get())!=cfg.device)return false;
 const auto desc=r->GetDesc();if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||desc.Width!=width||desc.Height!=height||
   desc.DepthOrArraySize!=1||desc.MipLevels!=1||desc.SampleDesc.Count!=1||desc.SampleDesc.Quality||desc.Format!=DXGI_FORMAT_R11G11B10_FLOAT)return false;
 out=r;return true;
}
#include "dependency_route.inl"
#include "compute_resources.inl"
#include "dof_native_plan.inl"
Plan::Recording *track_recording(Plan &p,ID3D12GraphicsCommandList *list,std::uint64_t gen,bool create){
 if(!list||!gen)return nullptr;Plan::Recording *empty{};
 for(auto &r:p.recordings){if(r.list==list)return r.generation==gen?&r:nullptr;if(!r.list&&!empty)empty=&r;}
 if(!create||!empty)return nullptr;empty->list=list;empty->generation=gen;
 for(unsigned i=0;i<resource_count(p);++i){const auto state=recording_states.get(reinterpret_cast<std::uintptr_t>(list),gen,reinterpret_cast<std::uintptr_t>(p.resources[i].Get()));
  empty->known[i]=state.known;empty->states[i]=state.value;if(state.known)++state_imports;}
 return empty;
}
void completed(Plan &p){
 if(!p.completed&&p.resolved&&p.copied&&(!p.identity||p.final_copied)&&p.recorded_draws==effect_count(p)&&(!p.dof||p.dof->dispatched==63)&&
    p.handoff.recorded()==p.handoff.count()){
  p.completed=true;++complete;overlay_completed+=p.overlay_count;if(shared_overlay_key(p))++overlay_shared_completed;if(p.has_haze)++haze_complete;
  unsigned count{};for(const auto &r:p.recordings)if(r.list)++count;if(count>1)++multilist_frames;
 }
}
bool prepare_selector(Plan &p){
 // perf::pooled_selectors: a buffer kept from the slot's retired plan (publish) is rewritten.
 if(!p.selector||!perf::on(perf::pooled_selectors)){
  D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_UPLOAD;
  D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;desc.Width=8;
  desc.Height=desc.DepthOrArraySize=desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  auto *device=reinterpret_cast<ID3D12Device *>(cfg.device);
  if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&p.selector))))return false;
 }
 void *mapped{};D3D12_RANGE none{};
 if(FAILED(p.selector->Map(0,&none,&mapped)))return false;
 const std::uint64_t native=0;std::memcpy(mapped,&native,8);const D3D12_RANGE written{0,8};p.selector->Unmap(0,&written);return true;
}
bool choose(Plan &p,FrameHandoff::Choice choice){
 if(!p.selector||p.handoff.choice()!=FrameHandoff::Choice::undecided||p.handoff.submitted_count())return false;
 void *mapped{};D3D12_RANGE none{};
 if(FAILED(p.selector->Map(0,&none,&mapped)))return false;
 const std::uint64_t value=choice==FrameHandoff::Choice::native?0:1;
 std::memcpy(mapped,&value,8);const D3D12_RANGE written{0,8};p.selector->Unmap(0,&written);
 const bool ok=p.handoff.choose(choice)==FrameHandoff::Result::ok;
#ifdef MHW_FG_SCENE_PROBE
 if(ok)scene_trace.choice(p.stamp.frame,p.stamp.epoch,static_cast<unsigned>(choice));
#endif
 return ok;
}
// Predication gates only the draw/copy commands. Native resource barriers and
// binding restoration always execute; only one branch does GPU image work.
struct PredicateScope {
 ID3D12GraphicsCommandList *list;
 explicit PredicateScope(ID3D12GraphicsCommandList *command):list(command){
  obs::Predicate current;if(!obs::predicate_current(list,current)||current.buffer)throw std::runtime_error("stream native predicate is not disabled");
 }
 void native(ID3D12Resource *selector){list->SetPredication(selector,0,D3D12_PREDICATION_OP_NOT_EQUAL_ZERO);}
 void full(ID3D12Resource *selector){list->SetPredication(selector,0,D3D12_PREDICATION_OP_EQUAL_ZERO);}
 ~PredicateScope(){list->SetPredication(nullptr,0,D3D12_PREDICATION_OP_EQUAL_ZERO);}
};
bool prepare_frame(Plan &p){
 if(handoff_count(p)>FrameHandoff::capacity||(p.dof&&!dof_warm(p)))return false;
 for(unsigned ordinal=0;ordinal<effect_count(p);++ordinal){const auto stage=effect_stage(p,ordinal);if(!needs_pipeline(p,stage))continue;
  const auto *cache=draw_pipeline(p,stage);if(!cache||!cache->effect||cache->signature!=p.draws[stage].signature)return false;}
 tb::Packet actual;
 if(!tb::packet(reader,p.temporal.object,actual)||actual!=p.temporal)return false;
 ComPtr<ID3D12Resource> color;
 if(!pin(p.stamp.inputs[0],color)||color==p.resources[0]||
    !(p.resources[0]->GetDesc().Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))return false;
 std::array<std::uintptr_t,FrameHandoff::capacity> packets{p.temporal.object,p.copy};
 for(unsigned ordinal=0;ordinal<effect_count(p);++ordinal){const auto stage=effect_stage(p,ordinal);packets[handoff_draw(p,stage)]=p.draws[stage].draw.packet;}
 if(p.dof)for(unsigned pass=0;pass<6;++pass)packets[handoff_compute(p,pass)]=p.dof->passes[pass].packet;
 for(unsigned i=0;i<p.late_count;++i)packets[handoff_indexed(p,i)]=p.late_packets[i];
 if(p.identity)packets[2]=p.final_copy;
 if(!prepare_selector(p)||p.handoff.prepare({p.stamp.frame,p.stamp.epoch,p.stamp.context,p.stamp.view},
     std::span(packets).first(handoff_count(p)))!=FrameHandoff::Result::ok)return false;
 p.prepared=true;p.claimed=true;overlay_planned+=p.overlay_count;if(shared_overlay_key(p))++overlay_shared_prepared;
 silhouette_planned+=p.late_count;
 ++prepared_frames;++claimed;if(p.has_haze)++haze_claimed;
 return true;
}
bool retain_recording(Plan &p,ID3D12GraphicsCommandList *list,std::uint64_t gen,std::span<IUnknown *const> extra={}){
 std::array<IUnknown *,resource_capacity+16> pins{};unsigned count=resource_count(p);
 for(unsigned i=0;i<count;++i)pins[i]=p.resources[i].Get();
 if(p.selector)pins[count++]=p.selector.Get();else return false;
 if(extra.size()>pins.size()-count)return false;
 for(auto *value:extra)pins[count++]=value;
 return lifetime.retain(list,gen,std::span(pins).first(count));
}
// DLAA (dlaa-identity-path.md): at render scale 1.0 the game ends the frame with
// a native copy (opcode 13, sub_1423C9250, from 0x23c7ad6) from the colour-
// grading output to the view's final attachment instead of the System_Copy quad.
// Every effect then renders at full size and stays native. The plan proves the
// TAA hand-off copy (where the reconstruction enters, as in the scaled presets)
// and that final copy (frame generation's HUD-less anchor): one each, with the
// same owner, packet and extent identities as the scaled route's.
bool build_identity_plan(Plan &p,OutputDiagnostics::Graph &observation){
 const auto &s=p.stamp;std::uintptr_t filters{},motion_in{},grade{},grade_out{},attachment{},final_dest{};
 unsigned count{},taas{},grades{};
 if(!same_stamp(s)||!field(cfg.image,0x51cc238,filters)||!field(filters,0x3c,count)||!count||count>16){plan_rejection=1;return false;}
 for(unsigned i=0;i<count;++i){std::uintptr_t e{},u{},type{},input{},output{};unsigned stage{},order{};
  if(!field(filters,0x40+8*i,e)||!field(e,0,stage)||!field(e,4,order)||!field(e,8,u)||!field(e,0x10,input)||!field(e,0x18,output)||(u&&!field(u,0,type))){plan_rejection=2;return false;}
  observation.filters[observation.count++]={e,stage,order,u,type,input,output};
  if(e==s.layer+0x140){if(stage!=15||order!=2||u!=s.layer||input!=s.inputs[0].object){plan_rejection=3;return false;}motion_in=output;++taas;}
  if(type==cfg.image+0x3538400&&stage==15&&order==0){grade=u;++grades;}
 }
 observation.links={0,grade,0,motion_in,0,0,0,0};
 if(taas!=1||grades!=1){plan_rejection=6;plan_detail=1;return false;}
 if(!field(grade,0x250,grade_out)||!grade_out){plan_rejection=6;plan_detail=4;return false;}
 observation.links[6]=grade_out;
 if(!field(s.view,0x460,attachment)||!attachment||!field(attachment,0x58,final_dest)||!final_dest){plan_rejection=6;plan_detail=6;return false;}
 std::uintptr_t array{};unsigned commands{};
 if(!field(s.context,0x2b80,commands)||!commands||commands>=65535||!field(s.context,0x2b78,array)||!array||array>UINTPTR_MAX-16ULL*commands){plan_rejection=8;return false;}
 struct Command {unsigned key{},padding{};std::uintptr_t packet{};};static_assert(sizeof(Command)==16);
 // Copy packets (sub_1423C9250): +0x20 mode, +0x24/+0x28 subresources,
 // +0x30 destination x/y, +0x38 source rectangle, +0x48 an optional pointer.
 const auto copy_shape=[&](std::uintptr_t packet,const std::array<unsigned,4> &expected){
  unsigned mode{},ds{},ss{};std::array<unsigned,2> xy{};std::array<unsigned,4> rect{};std::uintptr_t optional{};
  return field(packet,0x20,mode)&&field(packet,0x24,ds)&&field(packet,0x28,ss)&&field(packet,0x30,xy)&&field(packet,0x38,rect)&&
   field(packet,0x48,optional)&&!mode&&!ds&&!ss&&!xy[0]&&!xy[1]&&!optional&&rect==expected;};
 std::array<Command,256> block;unsigned handoffs{},finals{};
 for(unsigned base=0;base<commands;base+=256){const auto n=(std::min)(256U,commands-base);
  if(!pb::copy_memory(array+16ULL*base,block.data(),n*sizeof(Command))){plan_rejection=9;return false;}
  for(unsigned j=0;j<n;++j){const auto &c=block[j];unsigned raw{};
   if(c.key!=0x78000002&&(c.key>>27)!=16)continue;
   if(!field(c.packet,0,raw)){plan_rejection=10;return false;}
   if((raw&31)!=13)continue;
   std::uintptr_t src{},dst{};if(!field(c.packet,0x18,src)||!field(c.packet,0x10,dst)){plan_rejection=11;return false;}
   if(c.key==0x78000002){
    if(src!=s.inputs[4].object||dst!=motion_in)continue;
    if(++handoffs!=1||!pb::copy_memory(c.packet+0x10,p.copy_fields.data(),64)){plan_rejection=12;return false;}p.copy=c.packet;
    if(!copy_shape(c.packet,s.rectangle)){plan_rejection=13;return false;}
   }else if(src==grade_out&&dst==final_dest){
    if(++finals!=1||!pb::copy_memory(c.packet+0x10,p.final_copy_fields.data(),64)){plan_rejection=12;return false;}p.final_copy=c.packet;
    if(!copy_shape(c.packet,{0,0,width,height})){plan_rejection=13;return false;}
   }
  }
 }
 if(handoffs!=1||finals!=1){plan_rejection=15;return false;}
 unsigned again{};std::uintptr_t again_array{};
 if(!field(s.context,0x2b80,again)||again!=commands||!field(s.context,0x2b78,again_array)||again_array!=array||!same_stamp(s)){plan_rejection=16;return false;}
 tb::Owner transfer,source,destination;
 if(!owner(motion_in,transfer)||!owner(grade_out,source)||!owner(final_dest,destination)){plan_rejection=7;return false;}
 if(!pin(s.inputs[4],p.resources[0])||!pin(transfer,p.resources[1])||!pin(source,p.resources[2])||!pin(destination,p.resources[3])){plan_rejection=17;return false;}
 for(unsigned i=0;i<4;++i)for(unsigned j=0;j<i;++j)if(p.resources[i]==p.resources[j]){plan_rejection=17;return false;}
 return true;
}
// All admitted effects and the exact copy are selected from the completed native
// post-filter queue. Keys narrow the search; owners, packet fields, geometry and
// shader recipes prove each identity. No packet is selected by an address alone.
bool build_plan_observed(Plan &p,std::uintptr_t unit,OutputDiagnostics::Graph &observation){
 p.overlay_count=p.ordered_count=p.extra_resources=0;p.dof.reset();p.pre_stage=UINT_MAX;
 const auto overlay_failure=[&](OutputCommands::Branch branch,unsigned key,std::uintptr_t packet=0,
     unsigned index=UINT32_MAX,unsigned raw=0,bool raw_read=false,unsigned seen=0){
  observation.command_gate={branch,key,packet,index,raw,raw_read,seen,p.overlay_count};
  plan_rejection=17;++overlay_rejected;return false;};
 plan_rejection=plan_detail=plan_qualification=0;plan_vertex_gate={};
 p.identity=render_w==width&&render_h==height;
 if(p.identity){const bool built_identity=build_identity_plan(p,observation);if(built_identity)++identity_plans;return built_identity;}
 const auto &s=p.stamp;std::uintptr_t filters{},motion_in{},motion_out{},grade{},grade_in{},grade_out{},final_dest{},attachment{},endpoint{};
 std::array<ColorLink,16> middle_links{};std::array<unsigned,8> middle_path{};unsigned middle_size{},middle_count{};
 unsigned count{},taas{},motions{},grades{},ends{},hazes{};std::uintptr_t haze_unit{},haze_in{},haze_out{};
 unsigned pre_filters{},pre_order{};std::uintptr_t pre_unit{},pre_in{},pre_out{};
 if(!same_stamp(s)||!field(cfg.image,0x51cc238,filters)||!field(filters,0x3c,count)||!count||count>16){plan_rejection=1;return /* rejected */ false;}
 for(unsigned i=0;i<count;++i){std::uintptr_t e{},u{},type{},input{},output{};unsigned stage{},order{};
  if(!field(filters,0x40+8*i,e)||!field(e,0,stage)||!field(e,4,order)||!field(e,8,u)||!field(e,0x10,input)||!field(e,0x18,output)||(u&&!field(u,0,type))){plan_rejection=2;return /* rejected */ false;}
  observation.filters[observation.count++]={e,stage,order,u,type,input,output};
  if(e==s.layer+0x140){if(stage!=15||order!=2||u!=s.layer||input!=s.inputs[0].object){plan_rejection=3;return /* rejected */ false;}motion_in=output;++taas;}
  if(type==cfg.image+0x3537a70&&stage==15&&order==100){if(u!=unit){plan_rejection=4;return /* rejected */ false;}++motions;}
  if(type==cfg.image+0x35372b0&&stage==15&&order==20000){
   if(e!=u+0x140||!input||!output||input==output){plan_rejection=6;plan_detail=7;return false;}
   ++hazes;haze_unit=u;haze_in=input;haze_out=output;}
  // A filter between TAA (order 2) and the motion unit (order 100); its draw
  // must be an approved screen filter (discover_dependencies).
  if(stage==15&&order>2&&order<100&&e!=s.layer+0x140){
   if(e!=u+0x140||!input||!output||input==output){plan_rejection=6;plan_detail=9;return false;}
   ++pre_filters;pre_unit=u;pre_in=input;pre_out=output;pre_order=order;}
  if(type==cfg.image+0x3538400&&stage==15&&order==0){grade=u;++grades;}
  if(stage==15&&order==0x7fffff1){std::array<unsigned char,3> flags{};
   if(u||!field(e,0x20,flags)||flags!=std::array<unsigned char,3>{1,0,0}){plan_rejection=5;return /* rejected */ false;}endpoint=e;++ends;}
 }
 std::uintptr_t check{};
 const auto &final=final_quads[s.frame%final_quads.size()];
 observation.links={unit,grade,endpoint,motion_in,0,0,0,final.source.object};
 const auto link=[&](std::uintptr_t at,unsigned offset,std::uintptr_t &value,unsigned bit,unsigned slot){
  const bool ok=field(at,offset,value);if(ok)observation.link_reads|=1U<<bit;
  if(slot<observation.links.size())observation.links[slot]=value;return ok;};
 if(taas!=1||motions!=1||grades!=1||ends!=1||hazes>1||pre_filters>1)plan_detail=1;
 else if(endpoint!=grade+0x238)plan_detail=2;
 // With a pre-motion filter the motion unit reads its output, not the TAA copy.
 else if(!link(unit,0x150,check,0,8)||check!=(pre_filters?pre_out:motion_in)||!link(unit,0x158,motion_out,1,4))plan_detail=3;
 else if(!link(grade,0x248,grade_in,2,5)||!grade_in||!link(grade,0x250,grade_out,3,6))plan_detail=4;
 else if(!final_identity(final,s)||final.source.object!=grade_out)plan_detail=5;
 else if(!field(s.view,0x460,attachment)||!field(attachment,0x58,final_dest)||final_dest!=final.destination.object)plan_detail=6;
 if(!plan_detail&&hazes&&(haze_in!=motion_out||!haze_unit))plan_detail=7;
 if(!plan_detail&&pre_filters&&(pre_in!=motion_in||!pre_unit))plan_detail=9;
 if(!plan_detail){
  for(unsigned i=0;i<observation.count;++i){const auto &v=observation.filters[i];
   if(v[1]==15&&v[2]>100&&v[2]<0x7fffff1&&v[5]&&v[6])middle_links[middle_size++]={static_cast<unsigned>((v[1]<<27)|v[2]),v[5],v[6]};}
  if(!color_chain(std::span(middle_links).first(middle_size),hazes?haze_out:motion_out,grade_in,middle_path,middle_count))plan_detail=4;
  if(!plan_detail&&middle_count&&hazes&&middle_links[middle_path[0]].key<=0x78004e20)plan_detail=4;
 }
 if(plan_detail){plan_rejection=6;return false;}
 p.has_haze=hazes==1;
 tb::Owner pre_output;if(pre_filters&&!owner(pre_out,pre_output)){plan_rejection=7;return false;}
 const unsigned pre_key=(15U<<27)|pre_order;
 tb::Owner transfer,grade_input;
 if(!owner(grade_in,grade_input)){plan_rejection=7;return false;}p.grade_input_resource=grade_input.resource;
 if(!owner(motion_in,transfer)||!owner(motion_out,p.draws[0].output)||!owner(grade_out,p.draws[1].output)||!owner(final_dest,p.draws[2].output)){plan_rejection=7;return /* rejected */ false;}
 if(p.has_haze&&!owner(haze_out,p.draws[3].output)){plan_rejection=7;return false;}
 struct OverlayCandidate {unsigned key{},seen{};tb::Owner output;std::uintptr_t source{};};
 std::array<OverlayCandidate,8> overlays{};unsigned overlay_candidates{};
 for(unsigned i=0;i<middle_count;++i){const auto &edge=middle_links[middle_path[i]];tb::Owner output;
  if(!owner(edge.destination,output)){plan_rejection=7;return false;}
  overlays[overlay_candidates++]={edge.key,0,output,static_cast<std::uintptr_t>(edge.source)};
 }
 for(unsigned i=0;i<observation.count;++i){const auto &f=observation.filters[i];
  if(f[1]!=15||f[2]<=100||f[2]>=0x7fffff1||f[5]||!f[6])continue;
  unsigned target=UINT_MAX;for(unsigned j:{0U,1U,3U})if((j!=3||p.has_haze)&&f[6]==p.draws[j].output.object)target=j;
  if(target==UINT_MAX){
   for(unsigned j=0;j<middle_count;++j)if(f[6]==middle_links[middle_path[j]].destination)target=4+j;
  }
  if(target==UINT_MAX)continue;std::array<unsigned char,3> flags{};
  if(!field(f[0],0x20,flags))return overlay_failure(OutputCommands::flags_read,static_cast<unsigned>((f[1]<<27)|f[2]));
  if(flags!=std::array<unsigned char,3>{0,1,0})continue;
  if(overlay_candidates==overlays.size())return overlay_failure(OutputCommands::candidate_capacity,static_cast<unsigned>((f[1]<<27)|f[2]));
  tb::Owner target_owner;
  if(target<4)target_owner=p.draws[target].output;else if(!owner(f[6],target_owner)){plan_rejection=7;return false;}
  overlays[overlay_candidates++]={static_cast<unsigned>((f[1]<<27)|f[2]),0,target_owner,0};
 }
 p.keys={0x78000064,0x7ffffff1,0x80000000,0x78004e20};
 std::uintptr_t array{};unsigned commands{};
 if(!field(s.context,0x2b80,commands)||!commands||commands>=65535||!field(s.context,0x2b78,array)||!array||array>UINTPTR_MAX-16ULL*commands){plan_rejection=8;return /* rejected */ false;}
 // Bounded block reads avoid thousands of ReadProcessMemory-style probes.
 struct Command {unsigned key{},padding{};std::uintptr_t packet{};};static_assert(sizeof(Command)==16);
 std::array<Command,256> block;std::array<unsigned,5> found{};
 for(unsigned base=0;base<commands;base+=256){const auto n=(std::min)(256U,commands-base);
  if(!pb::copy_memory(array+16ULL*base,block.data(),n*sizeof(Command))){plan_rejection=9;return /* rejected */ false;}
  for(unsigned j=0;j<n;++j){const auto &c=block[j];unsigned raw{};
   OverlayCandidate *overlay{};for(unsigned k=0;k<overlay_candidates;++k)if(overlays[k].key==c.key){
    // A sort key is not a descriptor identity. Multiple source-free effects
    // can advertise the very same output. Consume their native commands once,
    // in queue order, then independently qualify every draw and dependency.
    // Conflicting outputs or color-chain producers remain ambiguous.
    if(overlay){if(overlay->source||overlays[k].source||overlay->output!=overlays[k].output)
      return overlay_failure(OutputCommands::ambiguous_key,c.key,c.packet,base+j);}
    else overlay=&overlays[k];
   }
   if(pre_filters&&c.key==pre_key){
    if(!field(c.packet,0,raw)){plan_rejection=10;return false;}
    if((raw&31)==11)continue;  // state-list packets remain native
    // v0.24.5: the game's depth of field (uNewDOFFilter) sits here, between
    // TAA and the motion filter, in the hub and at the title (order 16; after
    // the motion filter in the field). Its six compute passes precede the
    // composition draw, as in the chain (dof_native_sequence.hpp), and read
    // the TAA output: DLSS's full-size result.
    if((raw&31)==6){
     // The gate's `seen` word: 0x100 * why + the passes read so far (why: 1 a
     // dispatch after the composition draw, 2 another sequence's key, 3 a
     // seventh dispatch, 4 an unreadable dispatch).
     const unsigned read=p.dof?p.dof->count:0U;
     const unsigned why=p.pre_stage!=UINT_MAX?1U:p.dof&&p.dof->key!=c.key?2U:p.dof&&p.dof->count==6?3U:0U;
     if(why)return overlay_failure(OutputCommands::unsupported_opcode,c.key,c.packet,base+j,raw,true,0x100U*why+read);
     if(!p.dof){p.dof=std::make_shared<DofNativeSequence>();p.dof->key=c.key;}
     auto &dispatch=p.dof->passes[p.dof->count];dispatch.packet=c.packet;dispatch.index=base+j;
     if(!dof_read(dispatch))return overlay_failure(OutputCommands::unsupported_opcode,c.key,c.packet,base+j,raw,true,0x400U+read);
     ++p.dof->count;continue;
    }
    if((raw&31)!=0||p.pre_stage!=UINT_MAX)return overlay_failure(OutputCommands::unsupported_opcode,c.key,c.packet,base+j,raw,true);
    if(p.overlay_count==8)return overlay_failure(OutputCommands::draw_capacity,c.key,c.packet,base+j,raw,true);
    const auto stage=4+p.overlay_count++;p.keys[stage]=c.key;p.draws[stage].output=pre_output;
    // A depth-of-field composition reads the TAA output as its scene (t2).
    p.draws[stage].dependency_source=p.dof&&p.dof->key==c.key?pre_in:0;p.draws[stage].draw.packet=c.packet;p.pre_stage=stage;continue;
   }
   if(!overlay&&c.key!=0x78000002&&c.key!=0x78000064&&c.key!=0x7ffffff1&&c.key!=0x80000000&&c.key!=0x78004e20)continue;
   if(!field(c.packet,0,raw)){if(overlay){observation.command_gate={OutputCommands::packet_read,c.key,c.packet,base+j,0,0,overlay->seen,p.overlay_count};}plan_rejection=10;return /* rejected */ false;}
   if(overlay){
    if((raw&31)==0){const auto seen=overlay->seen++;
     for(unsigned k=0;k<p.overlay_count;++k)if(p.draws[4+k].draw.packet==c.packet)
      return overlay_failure(OutputCommands::duplicate_packet,c.key,c.packet,base+j,raw,true,seen);
     if(p.overlay_count==8)return overlay_failure(OutputCommands::draw_capacity,c.key,c.packet,base+j,raw,true,seen);
     const auto stage=4+p.overlay_count++;p.keys[stage]=c.key;p.draws[stage].output=overlay->output;p.draws[stage].dependency_source=overlay->source;p.draws[stage].draw.packet=c.packet;}
    // Multiple distinct draws may share one descriptor. State-list packets
    // remain native; every draw below retains its own complete qualification.
    else if((raw&31)==6){
     if(!overlay->source||overlay->seen||
        (p.dof&&(p.dof->key!=c.key||p.dof->count==6)))
      return overlay_failure(OutputCommands::unsupported_opcode,c.key,c.packet,base+j,raw,true,overlay->seen);
     if(!p.dof){p.dof=std::make_shared<DofNativeSequence>();p.dof->key=c.key;}
     auto &dispatch=p.dof->passes[p.dof->count];dispatch.packet=c.packet;dispatch.index=base+j;
     if(!dof_read(dispatch))return overlay_failure(OutputCommands::unsupported_opcode,c.key,c.packet,base+j,raw,true,overlay->seen);
     ++p.dof->count;
    }
    else if((raw&31)!=11)return overlay_failure(OutputCommands::unsupported_opcode,c.key,c.packet,base+j,raw,true,overlay->seen);
   }else if((raw&31)==13&&c.key==0x78000002){
    std::uintptr_t src{},dst{};if(!field(c.packet,0x18,src)||!field(c.packet,0x10,dst)){plan_rejection=11;return /* rejected */ false;}
    if(src!=s.inputs[4].object||dst!=motion_in)continue;
    if(++found[4]!=1||!pb::copy_memory(c.packet+0x10,p.copy_fields.data(),64)){plan_rejection=12;return /* rejected */ false;}p.copy=c.packet;
    unsigned mode{},ds{},ss{};std::array<unsigned,2> xy{};std::array<unsigned,4> rect{};std::uintptr_t optional{};
    if(!field(c.packet,0x20,mode)||!field(c.packet,0x24,ds)||!field(c.packet,0x28,ss)||!field(c.packet,0x30,xy)||
       !field(c.packet,0x38,rect)||rect!=s.rectangle||!field(c.packet,0x48,optional)||mode||ds||ss||xy[0]||xy[1]||optional){plan_rejection=13;return /* rejected */ false;}
   }else if((raw&31)==0&&c.key!=0x78000002){
    const auto stage=c.key==0x78000064?0U:c.key==0x7ffffff1?1U:c.key==0x78004e20?3U:2U;
    if(stage==3&&!p.has_haze){plan_rejection=14;return false;}
    if(++found[stage]!=1||(stage==2&&c.packet!=final.packet)){plan_rejection=14;return /* rejected */ false;}p.draws[stage].draw.packet=c.packet;
   }
  }
 }
 for(unsigned i=0;i<found.size();++i)if(found[i]!=(i==3&&!p.has_haze?0U:1U)){plan_rejection=15;return /* rejected */ false;}
 if(pre_filters&&p.pre_stage==UINT_MAX){plan_rejection=15;return false;}
 unsigned again{};std::uintptr_t again_array{};
 if(!field(s.context,0x2b80,again)||again!=commands||!field(s.context,0x2b78,again_array)||again_array!=array||!same_stamp(s)){plan_rejection=16;return /* rejected */ false;}
 for(unsigned stage=0;stage<4;++stage){if(stage==3&&!p.has_haze)continue;
  if(!geometry(p.draws[stage],s.context,stage)||!pin(p.draws[stage].output,p.resources[stage+2])){plan_rejection=17;return false;}}
 if(p.has_haze&&!qualify_haze(p)){plan_rejection=6;plan_detail=8;return false;}
 if(!pin(s.inputs[4],p.resources[0])||!pin(transfer,p.resources[1])||p.resources[0]==p.resources[1])return false;
 for(unsigned stage=0;stage<4+p.overlay_count;++stage)if(stage!=3||p.has_haze)p.order[p.ordered_count++]=stage;
 // Native 0x23d2370 is an unsigned-key stable merge sort. Overlay stages are
 // assigned in completed-queue insertion order, so their stage number is the
 // tie-breaker (never their packet address). GPU order is proved separately by
 // FrameHandoff. See multi-overlay-draws.md for the pinned native contract.
 std::sort(p.order.begin(),p.order.begin()+p.ordered_count,[&](unsigned a,unsigned b){
  return p.keys[a]!=p.keys[b]?p.keys[a]<p.keys[b]:a<b;});
 if(!discover_dependencies(p)){plan_qualification=12;
  const auto stage=dependency_stage;
  {const auto &d=dependency_description;
   dependency_latest_failure={dependency_rejection,stage,dependency_texture_check,d[0],d[1],d[2],d[4],d[5]};
   if(!dependency_failures++)dependency_first_failure=dependency_latest_failure;}
  return overlay_failure(OutputCommands::input_bindings,stage<4+p.overlay_count?p.keys[stage]:0,
                         stage<4+p.overlay_count?p.draws[stage].draw.packet:0);
 }
 unsigned preceding=UINT_MAX;
 for(unsigned i=0;i<p.ordered_count;++i){const auto stage=p.order[i];
  if(stage<4){preceding=stage;continue;}
  // The pre-motion draw is first in native order (its key precedes every
  // other effect); the motion filter, a base stage, must follow it.
  if(stage==p.pre_stage){
   if(i){plan_qualification=(stage<<16)|10U;return overlay_failure(OutputCommands::output_order,p.keys[stage],p.draws[stage].draw.packet);}
   if(!geometry(p.draws[stage],s.context,stage))return overlay_failure(OutputCommands::geometry,p.keys[stage],p.draws[stage].draw.packet);
   continue;
  }
  if(preceding==UINT_MAX||preceding==2||(!p.draws[stage].dependency&&p.draws[stage].output!=p.draws[preceding].output)){plan_qualification=(stage<<16)|10U;return overlay_failure(OutputCommands::output_order,p.keys[stage],p.draws[stage].draw.packet);}
  if(!geometry(p.draws[stage],s.context,stage))return overlay_failure(OutputCommands::geometry,p.keys[stage],p.draws[stage].draw.packet);
  if(!p.draws[stage].dependency){
   if(!overlay_inputs(p,p.draws[stage])){plan_qualification=(stage<<16)|11U;return overlay_failure(OutputCommands::input_bindings,p.keys[stage],p.draws[stage].draw.packet);}
   p.targets[stage]=UINT_MAX;
   for(unsigned k=0;k<resource_count(p);++k)if(reinterpret_cast<std::uintptr_t>(p.resources[k].Get())==p.draws[stage].output.resource)p.targets[stage]=k;
   if(p.targets[stage]==UINT_MAX)return overlay_failure(OutputCommands::output_order,p.keys[stage]);
  }
  preceding=stage;
 }
 if(p.dof&&!dof_qualify(p)){plan_qualification=14;return overlay_failure(OutputCommands::input_bindings,p.dof->key);}
 if(!dependency_order(p)){plan_qualification=13;return overlay_failure(OutputCommands::output_order,0);}
 return true;
}
bool build_plan(Plan &p,std::uintptr_t unit){
 OutputDiagnostics::Graph observation;const auto &s=p.stamp;
 observation.at={GetTickCount64(),s.frame,s.epoch,s.context,s.view,s.layer,s.bank,s.async};
 bool accepted=false;
 try{accepted=build_plan_observed(p,unit,observation);}
 catch(...){observation.reason=18;observation.detail=plan_detail;diagnostics.graph(observation,false);throw;}
 observation.reason=accepted?0:plan_rejection?plan_rejection:18;observation.detail=plan_detail;observation.qualification=plan_qualification;observation.vertex_gate=plan_vertex_gate;
 if(!accepted)diagnostics.commands.observe(observation,[](std::uint64_t at,void *out,std::size_t bytes){
  return pb::copy_memory(static_cast<std::uintptr_t>(at),out,static_cast<unsigned>(bytes));},[&]{return same_stamp(s);});
 if(!accepted)dependencies.observe(observation,[](std::uint64_t at,void *out,std::size_t bytes){
  return pb::copy_memory(static_cast<std::uintptr_t>(at),out,static_cast<unsigned>(bytes));},[&]{return same_stamp(s);});
 // Diagnostic selector from the captured color-chain break. It never admits a pass.
 if(!accepted)intermediate_dependencies.observe_intermediate(observation,[](std::uint64_t at,void *out,std::size_t bytes){
  return pb::copy_memory(static_cast<std::uintptr_t>(at),out,static_cast<unsigned>(bytes));},[&]{return same_stamp(s);},cfg.image+0x3526650);
 if(!accepted)inserted_dependencies.observe_inserted(observation,[](std::uint64_t at,void *out,std::size_t bytes){
  return pb::copy_memory(static_cast<std::uintptr_t>(at),out,static_cast<unsigned>(bytes));},[&]{return same_stamp(s);});
 if(!accepted)compute_dependencies.observe_compute_bundle(observation,[](std::uint64_t at,void *out,std::size_t bytes){
  return pb::copy_memory(static_cast<std::uintptr_t>(at),out,static_cast<unsigned>(bytes));},[&]{return same_stamp(s);},cfg.image+0x3526650);
 compute_gpu_capture();
 if(!accepted)++rejection_counts[(std::min)(observation.reason,18U)];
 diagnostics.graph(observation,accepted);return accepted;
}
struct PostReceipt {unsigned slot{UINT_MAX};std::uint64_t frame{},epoch{};std::uintptr_t unit{};unsigned thread{};};
PostReceipt post_before(const pb::Frame &f){
 if(!accepting.load()||f.caller!=cfg.image+0x24f228b||f.gpr[2]!=cfg.renderer+0x710)return {};
 std::uint64_t frame{};if(!field(cfg.renderer,0x1f210,frame))return {};
 Lock lock;const unsigned slot=static_cast<unsigned>(frame%plans.size());auto &p=plans[slot];
 if(!p.published||p.queue_finished||p.stamp.frame!=frame||!same_stamp(p.stamp))return {};
 ++p.motion_visits;p.motion_returned=false;
 p.motion_unit=f.gpr[1];p.motion_thread=GetCurrentThreadId();
 return {slot,p.stamp.frame,p.stamp.epoch,p.motion_unit,p.motion_thread};
}
void post_after(const PostReceipt &receipt,bool returned){
 if(receipt.slot==UINT_MAX)return;Lock lock;auto &p=plans[receipt.slot];
 if(!accepting.load()||p.queue_finished||p.stamp.frame!=receipt.frame||p.stamp.epoch!=receipt.epoch||
    p.motion_unit!=receipt.unit||p.motion_thread!=receipt.thread||receipt.thread!=GetCurrentThreadId())return;
 // Haze executes later in the same native loop. Only remember this return;
 // scanning the draw queue here sees a valid but still incomplete frame.
 p.motion_returned=returned&&p.motion_visits==1&&same_stamp(p.stamp);
}
unsigned draw_before(pb::Frame &f,unsigned &stage){
 Lock lock;if(fault.load()||abandoned)return UINT_MAX;unsigned selected=UINT_MAX;
 for(unsigned i=0;i<plans.size();++i)if(plans[i].built)for(unsigned ordinal=0;ordinal<effect_count(plans[i]);++ordinal){const auto &p=plans[i];const auto j=effect_stage(p,ordinal);
  if(!(p.executed&(1U<<j))&&p.draws[j].draw.packet==f.gpr[3]){
   if(selected!=UINT_MAX){fail(2);return UINT_MAX;}selected=i;stage=j;}}
 if(selected==UINT_MAX)return selected;
 auto &p=plans[selected];auto &r=p.draws[stage];
 api_draw=r.draw;api_draw.executor=f.gpr[1];api_draw.context=f.gpr[2];
 if(r.dependency){r.binding_entry=dependency_binding_check(p,stage);dependency_binding_trace.entry(r.binding_entry);dependency_enter(p,stage);}
 auto &observation=p.observations[stage];observation={};observation[0]=GetTickCount64();
 observation[4]=api_draw.executor;observation[5]=api_draw.context;observation[6]=f.gpr[3];
 field(cfg.renderer,0x1f210,observation[9]);field(cfg.renderer,0x1f208,observation[10]);
 std::array<std::uintptr_t,4> bindings{};std::array<unsigned,2> args{};
 bool ok=field(f.gpr[1],8,api_state)&&field(api_state,0x20,api_draw.command)&&api_draw.command&&
   field(r.draw.packet,0x10,bindings)&&bindings==r.packet_bindings&&field(r.draw.packet,0x40,args)&&args==r.arguments&&
   pb::snapshot_attachments(cfg.image,api_draw,api_before)&&api_before.color_count==1&&!api_before.views[8].owner&&
   api_before.views[0].resource==r.output.resource&&api_before.pipeline==r.before.pipeline;
 observation[1]=api_draw.command;observation[3]=api_state;
 if(!ok){diagnostics.draw(p.stamp.frame,p.stamp.epoch,stage,observation);if(p.claimed)fail(3);else{p.executed|=1U<<stage;++unqualified;}return UINT_MAX;}
 api_calls=0;return selected;
}
void draw_after(unsigned slot,unsigned stage,bool returned){
 if(slot==UINT_MAX)return;Lock lock;auto &p=plans[slot];pb::AttachmentSnapshot now;
 const bool ok=returned&&api_calls==1&&pb::snapshot_attachments(cfg.image,api_draw,now)&&pb::attachment_inputs_equal(api_before,now)&&pb::attachment_cache_matches(now);
 if(!ok){if(p.claimed)fail(4);else ++unqualified;}
 p.executed|=1U<<stage;
 p.observations[stage][7]=returned;
 diagnostics.draw(p.stamp.frame,p.stamp.epoch,stage,p.observations[stage]);
}
std::atomic<FinalSceneObserver> final_scene_observer{};
bool record_effect(unsigned slot,unsigned stage,const EffectRecipe &prepared,const EffectBindings &b,std::uintptr_t rtv,bool &replaced){
 auto &p=plans[slot];auto &r=p.draws[stage];const bool overlay=stage>=4&&!r.dependency;const auto target=output_index(p,stage);
 Pipeline *effect_cache=draw_pipeline(p,stage);
 if(r.dependency&&!effect_cache){for(auto &candidate:dependency_pipelines)if(!candidate.effect){effect_cache=&candidate;break;}
  if(!effect_cache){++warmups;return false;}}
 if(!overlay){auto &cache=*effect_cache;
  if(!cache.effect){cache.effect=std::make_unique<Effect>(reinterpret_cast<ID3D12Device *>(cfg.device),prepared,true);cache.signature=r.signature;cache.root=prepared.root;}
  if(cache.signature!=r.signature||cache.root.Get()!=prepared.root)throw std::runtime_error("stream effect recipe changed");}
 if(!p.prepared){++warmups;return false;}
 const FilterGeometry overlay_extent{render_w,render_h,width,height};
 if(overlay&&(!overlay_raster(b,overlay_extent)||!overlay_inputs(p,r)))throw std::runtime_error("stream overlay contract changed");
 obs::RasterBindings raster;temporal_present::Bindings roots;
 if(!obs::inspect(b.list,roots)||!roots.known||!roots.valid)throw std::runtime_error("stream draw generation");
 auto *track=track_recording(p,b.list,roots.generation,true);
 const bool raster_readable=obs::raster_current(b.list,raster);
 unsigned mask{};
 if(!track)mask|=1U;
 // Private replacement effects require a proved local output transition.
 // An in-place overlay keeps the exact native draw, RTV, shaders, resources
 // and resource barriers; it changes only viewport/scissor coverage within
 // that same target. It can therefore preserve native state inheritance
 // across command lists without inventing a local state observation. A
 // contradictory observed state still rejects both kinds of draw below.
 if(track&&!track->known[target]&&!overlay)mask|=2U;
 if(track&&track->known[target]&&track->states[target]!=D3D12_RESOURCE_STATE_RENDER_TARGET)mask|=4U;
 if(b.output!=p.resources[target].Get())mask|=8U;
 if(!raster_readable)mask|=16U;
 if(raster_readable&&!raster.output_known)mask|=32U;
 if(raster_readable&&raster.output_known&&raster.render_target!=rtv)mask|=64U;
 if(roots.pipeline!=reinterpret_cast<std::uintptr_t>(b.pipeline))mask|=128U;
 if(roots.graphics.signature!=reinterpret_cast<std::uintptr_t>(b.root))mask|=256U;
 if(mask){
  draw_state_trace.reject({{GetTickCount64(),p.stamp.frame,p.stamp.epoch,stage,r.draw.packet,reinterpret_cast<std::uintptr_t>(b.list),roots.generation},
    {track!=nullptr,track&&track->known[target],track?track->states[target]:0,
     reinterpret_cast<std::uintptr_t>(p.resources[target].Get()),reinterpret_cast<std::uintptr_t>(b.output),
     raster_readable,raster.output_known,rtv,raster.render_target,
     reinterpret_cast<std::uintptr_t>(b.pipeline),roots.pipeline,reinterpret_cast<std::uintptr_t>(b.root),roots.graphics.signature},mask});
  throw std::runtime_error("stream draw order or state");
 }
 // The unchanged native SRV can inherit its read state from an earlier GPU
 // recording. A local contradictory transition still rejects the adapter.
 if(stage==3&&(!haze_bindings_stable(p)||(track->known[2]&&
    !(track->states[2]&D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE))))throw std::runtime_error("stream haze bindings or color state");
 if(r.dependency){
  if(!dependency_current_stable(p,stage))throw std::runtime_error("stream dependency bindings changed");
  for(unsigned i=0;i<r.read_count;++i)if(track->known[r.reads[i]]&&!(track->states[r.reads[i]]&D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE))
   throw std::runtime_error("stream dependency read state");
 }
 std::array<IUnknown *,6> pins{b.pipeline,b.root,b.output,b.vertex_resource};unsigned pin_count=4;
 for(auto heap:roots.heaps)if(heap)pins[pin_count++]=reinterpret_cast<IUnknown *>(heap);
 if(!retain_recording(p,b.list,roots.generation,std::span(pins).first(pin_count)))throw std::runtime_error("stream draw retention");
 if(p.handoff.record(handoff_draw(p,stage),r.draw.packet,{reinterpret_cast<std::uintptr_t>(b.list),roots.generation})!=FrameHandoff::Result::ok)
  throw std::runtime_error("stream prepared draw recording");
 if(!p.resolved)++early_draws;
 injecting=true;
 {PredicateScope predicate(b.list);predicate.native(p.selector.Get());b.list->DrawInstanced(b.count,1,b.first,0);
  predicate.full(p.selector.Get());if(overlay){record_overlay(b,overlay_extent);replaced=true;}else replaced=effect_cache->effect->record(b);}
 injecting=false;
 obs::RasterBindings after;
 const bool restored=obs::matches_current(b.list,roots)&&obs::raster_current(b.list,after)&&after.output_known&&
  after.render_target==raster.render_target&&after.topology==raster.topology&&
  !std::memcmp(&raster.vertex_buffer,&after.vertex_buffer,sizeof(raster.vertex_buffer))&&
  !std::memcmp(&raster.viewport,&after.viewport,sizeof(raster.viewport))&&!std::memcmp(&raster.scissor,&after.scissor,sizeof(raster.scissor));
 if(!replaced||!restored)throw std::runtime_error("stream effect recording or restore");
 if(const auto observer=final_scene_observer.load();stage==2&&observer){
  injecting=true;observer(b.list,roots,b.output,p.stamp.frame,p.stamp.epoch,D3D12_RESOURCE_STATE_RENDER_TARGET);injecting=false;
  if(!obs::matches_current(b.list,roots))throw std::runtime_error("stream final-scene observer restore");}
 ++p.recorded_draws;++draws;if(stage==3)++haze_draws;if(r.dependency)++dependency_draws[r.dependency-1];if(stage>=4){++overlay_draws;
  if(!r.dependency&&r.vertex_stride!=16&&r.vertex_stride){++overlay_extended_draws;const auto &v=r.overlay_layout;
   overlay_extended_layout={v.stride,r.before.pipeline.input_count,unsigned(v.position_format),v.position,unsigned(v.uv_format),v.uv};}}
 completed(p);return true;
}

#include "dof_native_record.inl"
#include "silhouette_route.inl"

bool copy_override(ID3D12GraphicsCommandList *list,const D3D12_TEXTURE_COPY_LOCATION *destination,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION *source,const D3D12_BOX *box,std::uintptr_t caller){
 if(injecting||(filtering()&&copy_slot==UINT_MAX))return false;
 Lock lock;if(copy_slot==UINT_MAX||fault.load())return false;
 auto &p=plans[copy_slot];if(!p.prepared)return false;
 D3D12_TEXTURE_COPY_LOCATION d{},s{};D3D12_BOX b{};
 if(!pb::copy_memory(reinterpret_cast<std::uintptr_t>(destination),&d,sizeof(d))||!pb::copy_memory(reinterpret_cast<std::uintptr_t>(source),&s,sizeof(s)))return false;
 if(copy_final){
  // The native final copy at render scale 1.0 (DLAA): forwarded unchanged on
  // both branches (it is already full size), then frame generation's HUD-less
  // capture reads the final scene target, now in COPY_DEST.
  if(d.pResource!=p.resources[3].Get()||s.pResource!=p.resources[2].Get())return false;
  temporal_present::Bindings bindings;if(!obs::inspect(list,bindings)||!bindings.known||!bindings.valid){fail(5);return false;}
  auto *track=track_recording(p,list,bindings.generation,true);
  std::uintptr_t packet{},command{};std::array<std::byte,64> fields{};
  if(caller!=cfg.image+0x259caf0||p.final_copied||++copy_calls!=1||!track||x||y||z||
     // A native copy needs its destination in COPY_DEST, and the copy leaves
     // it there: the observer's barrier relies on that. An unknown state (its
     // barrier in an earlier list) is accepted, a contradicting one is not.
     (track->known[2]&&(track->states[2]&D3D12_RESOURCE_STATE_COPY_SOURCE)==0)||(track->known[3]&&track->states[3]!=D3D12_RESOURCE_STATE_COPY_DEST)||
     !field(copy_recording,0x158,packet)||packet!=p.final_copy||!field(copy_recording,0x20,command)||command!=reinterpret_cast<std::uintptr_t>(list)||
     !pb::copy_memory(p.final_copy+0x10,fields.data(),64)||fields!=p.final_copy_fields||
     d.Type!=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX||s.Type!=d.Type||d.SubresourceIndex||s.SubresourceIndex||
     (box&&(!pb::copy_memory(reinterpret_cast<std::uintptr_t>(box),&b,sizeof(b))||b.left||b.top||b.front||b.back!=1||b.right!=width||b.bottom!=height))){
   fail(5);return false;}
  if(!retain_recording(p,list,bindings.generation)||p.handoff.record(2,p.final_copy,{reinterpret_cast<std::uintptr_t>(list),bindings.generation})!=FrameHandoff::Result::ok){fail(5);return false;}
  reinterpret_cast<CopyFn>(originals[2])(list,destination,0,0,0,source,box);
  ++identity_final_copies;
  if(const auto observer=final_scene_observer.load()){
   injecting=true;observer(list,bindings,p.resources[3].Get(),p.stamp.frame,p.stamp.epoch,D3D12_RESOURCE_STATE_COPY_DEST);injecting=false;
   if(!obs::matches_current(list,bindings)){fail(5);return true;}++identity_observed;}
  p.final_copied=true;++copies;completed(p);return true;
 }
 if(d.pResource!=p.resources[1].Get()||s.pResource!=p.resources[0].Get())return false;
 temporal_present::Bindings bindings;if(!obs::inspect(list,bindings)||!bindings.known||!bindings.valid){fail(5);return false;}
 // A worker may record the copy on a different list from the resolve. Import
 // that list's observed native states, as draws do; no state crosses lists.
 auto *track=track_recording(p,list,bindings.generation,true);
 std::uintptr_t packet{},command{};std::array<std::byte,64> fields{};
 if(caller!=cfg.image+0x259caf0||p.copied||++copy_calls!=1||!track||x||y||z||
    !track->known[0]||!track->known[1]||(track->states[0]&D3D12_RESOURCE_STATE_COPY_SOURCE)==0||track->states[1]!=D3D12_RESOURCE_STATE_COPY_DEST||
    !field(copy_recording,0x158,packet)||packet!=p.copy||!field(copy_recording,0x20,command)||command!=reinterpret_cast<std::uintptr_t>(list)||
    !pb::copy_memory(p.copy+0x10,fields.data(),64)||fields!=p.copy_fields||
    d.Type!=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX||s.Type!=d.Type||d.SubresourceIndex||s.SubresourceIndex||
    !box||!pb::copy_memory(reinterpret_cast<std::uintptr_t>(box),&b,sizeof(b))||b.left||b.top||b.front||b.back!=1||b.right!=render_w||b.bottom!=render_h){
  if(box&&b.right&&(b.right!=render_w||b.bottom!=render_h)&&!copy_missed){copy_missed=true;copy_miss={b.left,b.top,b.right,b.bottom};}
  fail(5);return false;}
 if(!retain_recording(p,list,bindings.generation)||p.handoff.record(1,p.copy,{reinterpret_cast<std::uintptr_t>(list),bindings.generation})!=FrameHandoff::Result::ok){fail(5);return false;}
 {PredicateScope predicate(list);predicate.native(p.selector.Get());reinterpret_cast<CopyFn>(originals[2])(list,destination,0,0,0,source,box);
  predicate.full(p.selector.Get());const D3D12_BOX full{0,0,0,width,height,1};reinterpret_cast<CopyFn>(originals[2])(list,destination,0,0,0,source,&full);}
 p.copied=true;++copies;completed(p);return true;
}
void WINAPI copy_hook(ID3D12GraphicsCommandList *list,const D3D12_TEXTURE_COPY_LOCATION *d,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION *s,const D3D12_BOX *box){
 Guard guard;bool replaced=false;
#ifdef MHW_FG_SCENE_PROBE
 const auto scene_receipt=scene_copy(list,d,x,y,z,s,box,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
#endif
 try{replaced=copy_override(list,d,x,y,z,s,box,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));}
 catch(const std::exception &e){Lock lock;failure=e.what();fail(5);}
 if(!replaced)reinterpret_cast<CopyFn>(originals[2])(list,d,x,y,z,s,box);
#ifdef MHW_FG_SCENE_PROBE
 scene_copy_returned(scene_receipt,replaced);
#endif
}
}
namespace ht=perf::hook_timing;
void post_dispatch(pb::Frame *f){auto timing=ht::begin(ht::route_post);++pending;const auto receipt=post_before(*f);bool returned=false;
 __try{ht::forward_begin(timing);mhw_binding_forward(originals[0],f);ht::forward_end(timing);returned=true;}
 __finally{post_after(receipt,returned);--pending;ht::end(timing);}}
void post_queue_complete(std::uintptr_t manager,std::uintptr_t context,std::uintptr_t view,std::uintptr_t taa) noexcept {
 const ht::Scope timing(ht::route_queue);Guard guard;Lock lock;if(!accepting.load())return;
 std::uint64_t frame{};std::uintptr_t current_manager{};
 if(!field(cfg.renderer,0x1f210,frame)||!field(cfg.image,0x51cc238,current_manager)||manager!=current_manager)return;
 // Observe the shared preparation contract before applying the prototype's
 // motion/grade/haze whitelist. An unfamiliar filter remains visible here.
 prepared_queues.observe(reader,cfg,manager,context,view,taa);
 auto &p=plans[frame%plans.size()];
 if(!p.published||p.queue_finished||p.stamp.frame!=frame||p.stamp.context!=context||p.stamp.view!=view||
    p.stamp.layer!=taa||!same_stamp(p.stamp)||p.motion_thread!=GetCurrentThreadId())return;
 p.queue_finished=true;
 try{
  if(p.motion_returned&&p.motion_visits==1&&build_plan(p,p.motion_unit)){p.built=true;++built;if(p.identity||silhouette_candidates(p))(void)prepare_frame(p);
   watch(static_cast<unsigned>(frame%plans.size()),p);}
  else{++unqualified;p.resources={};}
 }catch(const std::exception &e){failure=e.what();fail(1);}
}
bool haze_before_safe(const pb::Frame &f) noexcept{try{return haze_before(f);}catch(...){Lock lock;haze_reject(11);return false;}}
void haze_after_safe(bool entered,bool returned) noexcept{try{haze_after(entered,returned);}catch(...){Lock lock;haze_reject(11);}}
void final_dispatch(pb::Frame *f){auto timing=ht::begin(ht::route_final);++pending;const auto slot=final_before(*f);const bool capture=haze_before_safe(*f);bool returned=false;
 __try{ht::forward_begin(timing);mhw_binding_forward(originals[3],f);ht::forward_end(timing);returned=true;}
 __finally{final_after(slot,returned);haze_after_safe(capture,returned);--pending;ht::end(timing);}}
void draw_dispatch(pb::Frame *f){auto timing=ht::begin(ht::route_draw);++pending;unsigned stage=UINT_MAX;
 const auto slot=filtering()&&!watched_packet(f->gpr[3])?UINT_MAX:draw_before(*f,stage);bool returned=false;
#ifdef MHW_FG_SCENE_PROBE
 const auto previous_scene=scene_scope;scene_scope=scene_enter(*f);
#endif
 const auto old_slot=api_slot,old_stage=api_stage;api_slot=slot;api_stage=stage;
 __try{ht::forward_begin(timing);mhw_binding_forward(originals[1],f);ht::forward_end(timing);returned=true;}__finally{draw_after(slot,stage,returned);
#ifdef MHW_FG_SCENE_PROBE
  scene_exit(returned);scene_scope=previous_scene;
#endif
  api_slot=old_slot;api_stage=old_stage;--pending;ht::end(timing);}}
void indexed_dispatch(pb::Frame *f){
 auto timing=ht::begin(ht::route_indexed);++pending;const auto previous=indexed_scope;indexed_scope=filtering()&&!watched_packet(f->gpr[3])?IndexedScope{}:indexed_before(*f);bool returned=false;
#ifdef MHW_FG_SCENE_PROBE
 const auto previous_scene=scene_scope;scene_scope=scene_enter(*f);
#endif
 __try{ht::forward_begin(timing);mhw_binding_forward(originals[5],f);ht::forward_end(timing);returned=true;}
 __finally{indexed_after(returned);silhouette_after(returned);
#ifdef MHW_FG_SCENE_PROBE
  scene_exit(returned);scene_scope=previous_scene;
#endif
  indexed_scope=previous;--pending;ht::end(timing);}
}
bool indexed_draw(ID3D12GraphicsCommandList *list,unsigned n,unsigned instances,unsigned first,int base,unsigned instance,std::uintptr_t caller) noexcept {
#ifdef MHW_FG_SCENE_PROBE
 scene_api(list,true,{n,instances,first,static_cast<std::uint64_t>(static_cast<std::int64_t>(base)),instance},caller);
#endif
 if(injecting||(indexed_scope.sample==UINT_MAX&&indexed_scope.slot==UINT_MAX))return false;Lock lock;
 try{
  if(indexed_scope.sample!=UINT_MAX)indexed_sample(list,n,instances,first,base,instance,caller);
  if(indexed_scope.slot!=UINT_MAX&&!fault.load()&&!abandoned){const bool result=record_indexed(list,n,instances,first,base,instance,caller);
#ifdef MHW_FG_SCENE_PROBE
   scene_scope.replaced=result;
#endif
   return result;}
 }catch(const std::exception &e){injecting=false;failure=e.what();fail(14);}
 catch(...){injecting=false;fail(14);}
 return indexed_scope.replaced;
}
unsigned copy_before(const pb::Frame &f){
 Lock lock;if(fault.load()||abandoned)return UINT_MAX;unsigned slot=UINT_MAX;
 bool final=false;
 for(unsigned i=0;i<plans.size();++i){const auto &q=plans[i];if(!q.prepared)continue;
  const bool handoff=q.copy==f.gpr[3]&&!q.copied,closing=q.identity&&q.final_copy==f.gpr[3]&&!q.final_copied;
  if(!handoff&&!closing)continue;
  if(slot!=UINT_MAX){fail(5);return UINT_MAX;}slot=i;final=closing;}
 if(slot==UINT_MAX)return slot;
 std::uintptr_t backend{};
 if(!field(f.gpr[2],8,backend)||backend!=cfg.backend||!field(f.gpr[1],8,copy_recording)||!copy_recording){fail(5);return UINT_MAX;}
 copy_calls=0;copy_final=final;return slot;
}
void copy_after(bool returned){if(copy_slot!=UINT_MAX){Lock lock;
 if(!returned||copy_calls!=1||!(copy_final?plans[copy_slot].final_copied:plans[copy_slot].copied))fail(5);}}
void copy_dispatch(pb::Frame *f){
 auto timing=ht::begin(ht::route_copy);++pending;const auto old_slot=copy_slot;const auto old_recording=copy_recording;const auto old_calls=copy_calls;const auto old_final=copy_final;
 copy_final=false;copy_slot=filtering()&&!watched_packet(f->gpr[3])?UINT_MAX:copy_before(*f);bool returned=false;
 __try{ht::forward_begin(timing);mhw_binding_forward(originals[4],f);ht::forward_end(timing);returned=true;}
 __finally{
  copy_after(returned);
  copy_slot=old_slot;copy_recording=old_recording;copy_calls=old_calls;copy_final=old_final;--pending;ht::end(timing);
 }
}
void observe_final_scene(FinalSceneObserver observer) noexcept {final_scene_observer.store(observer);}
bool draw(ID3D12GraphicsCommandList *list,unsigned n,unsigned instances,unsigned first,unsigned instance,std::uintptr_t caller) noexcept {
#ifdef MHW_FG_SCENE_PROBE
 scene_api(list,false,{n,instances,first,0,instance},caller);
#endif
 if(injecting||api_slot==UINT_MAX)return false;Lock lock;if(fault.load()||abandoned)return false;
 auto &p=plans[api_slot];auto &r=p.draws[api_stage];bool replaced=false;
 try{
  ++api_calls;pb::AttachmentSnapshot now;obs::RasterBindings raster;temporal_present::Bindings roots;std::uintptr_t root{},pipeline{},packet{};
  if(api_calls!=1||caller!=cfg.image+0x259d72e||reinterpret_cast<std::uintptr_t>(list)!=api_draw.command||n!=r.arguments[0]||first!=r.arguments[1]||instances!=1||instance||
     !pb::snapshot_attachments(cfg.image,api_draw,now)||!pb::attachment_inputs_equal(api_before,now)||!pb::attachment_cache_matches(now)||
     !obs::raster_current(list,raster)||!raster.output_known||raster.render_target!=now.views[0].handle||
     !obs::inspect(list,roots)||!roots.known||!roots.valid||!roots.graphics.signature||!roots.pipeline||
     !field(api_state,0x158,packet)||packet!=r.draw.packet||!field(api_state,0x110,root)||root!=roots.graphics.signature||
     !field(api_state,0x108,pipeline)||pipeline!=roots.pipeline||
     !shader_stable(r.shaders[0],r.draw.shader_set,0x50)||!shader_stable(r.shaders[1],r.draw.shader_set,0x58)||!layout(r,true))throw std::runtime_error("stream native draw identity");
  p.observations[api_stage][2]=roots.generation;p.observations[api_stage][8]=1;
  diagnostics.draw(p.stamp.frame,p.stamp.epoch,api_stage,p.observations[api_stage]);
  auto prepared=recipe(r,recipe_stage(r,api_stage),reinterpret_cast<ID3D12RootSignature *>(root));
  ComPtr<ID3D12Resource> vb;vb.Attach(vertex_resource(r,raster.vertex_buffer));if(!vb)throw std::runtime_error("stream vertex buffer");
  EffectBindings b{list,reinterpret_cast<ID3D12PipelineState *>(pipeline),prepared.root,p.resources[output_index(p,api_stage)].Get(),vb.Get(),raster.vertex_buffer,raster.viewport,raster.scissor,raster.topology,n,first};
  replaced=record_effect(api_slot,api_stage,prepared,b,now.views[0].handle,replaced);
#ifdef MHW_FG_SCENE_PROBE
  scene_scope.replaced=replaced;
#endif
 }catch(const std::exception &e){injecting=false;failure=e.what();fail(6);}
 return replaced;
}
bool prepare(const tb::Config &c,unsigned w,unsigned h,unsigned rw,unsigned rh,std::string &reason){
 cfg=c;width=w;height=h;render_w=rw;render_h=rh;
 // DLAA renders at the scene size; the route then runs at identity.
 if(!valid_filter_geometry({rw,rh,w,h})){reason="stream extent";return false;}
 struct Gate{unsigned rva,bytes;const char *sha;};
 constexpr Gate gates[]{
  {0x23d2370,1047,"0cf82d2b8b6259fbb5cad0c7468a780bb7026d9a20f720e9bbd8b5cd501fffc3"},
  {0x23ca8b0,206,"b27d637cadf610c779092c55dc94cda7812d4525fbb864fc26acad0b91620edc"},
  {0x24c7760,2256,"d1e1c91d7cd6ff65cf5fee677a998692fc4e9ead2bedcb7c0a79f88fbe752701"},
  {0x259f330,1664,"ec12eca9df5bc5598eed515d9e882fad9d0afcf54c0488f9eaf98127ab046c32"},
  {0x24ca1f0,792,"ace3852211fb63c13cfca395d4992f922addc69db0841fa44e540819779e60aa"},
  {0x23ca4a0,445,"97d8e82e322796881438a9318a4f5170401bf031b28d21afd19c77256a82816e"},
  {0x24d0780,4636,"5853f4dd6561e66842edab799f7d87ea3042f7b873dd72ab848e612579ff3e7b"},
  {0x259d670,206,"d6b0b80a37a8fadba69d215cfcebd960c74f8d3554e9375b03ac7785d4f6ee39"},
  {0x259d740,214,"82e2ac66bd7cfa4ce7a61244efaaa6f7e3db0bbb766606962bb23fcb91daafba"},
  {0x259f9b0,1531,"7abd32763fb614e868e425a607d67d717205988f34a520fed0884fd51cb720c8"},
  {0x259e670,2554,"088e67c77b7e96af64c83c8297f6fc9f6d355dbc170530dda125b41f4cb53727"},
  {0x259c650,1214,"299ede85b4e2bbfb25f0ae2c20e003317efc2809c591eb46677ec154495daf88"},
  {0x259e470,512,"7db0a032642249004ebda64a7e8442407851cec8f25208e8966a63990ddade49"},
  {0x24e6e50,910,"d961028c0e34625cd23343327e3ba3ad401c4d439a5c653a9ca0e6893836bc04"},
  {0x2297e20,656,"58fe604c883de6d505ce2211797c2efaddc360046d10bb14c2e50fafbae4ab6d"},
  {0x23c7831,740,"0da63ff39cc114c78483728e19221e1dd22806398522ec0095ec6c6349801247"}};
 for(const auto &g:gates)if(!pb::verify_code(c.image,g.rva,g.bytes,g.sha)){reason="stream code gate";return false;}
 targets[0]=c.image+0x24ca1f0;targets[1]=c.image+0x259d670;targets[3]=c.image+0x2297e20;targets[4]=c.image+0x259c650;targets[5]=c.image+0x259d740;
 auto *device=reinterpret_cast<ID3D12Device *>(c.device);ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;
#ifndef MHW_ALPHA
 // Research builds only: two scene-sized readbacks of the native depth (a
 // diagnostic; about 66 MB at 4K). Unprepared, they record nothing.
 for(auto &capture:compute_depth_pixels)if(!capture.prepare(device,w,h)){reason="compute depth readback allocation";return false;}
#endif
 if(FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)))||
    FAILED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)))||FAILED(list->Close())||
    !pb::system_method(reinterpret_cast<std::uintptr_t>(list.Get()),16,targets[2])){reason="stream method discovery";return false;}
 const std::array<void *,6> detours{reinterpret_cast<void *>(scaled_postfilter_entry),reinterpret_cast<void *>(scaled_draw_execution_entry),reinterpret_cast<void *>(copy_hook),reinterpret_cast<void *>(scaled_stream_final_entry),reinterpret_cast<void *>(scaled_copy_execution_entry),reinterpret_cast<void *>(scaled_indexed_execution_entry)};
 for(unsigned i=0;i<targets.size();++i){
  if(astera::active()&&i==api_hook){if(!originals[i]){reason="stream AL chain";return false;}enabled[i]=true;continue;}
  if(MH_CreateHook(reinterpret_cast<void *>(targets[i]),detours[i],&originals[i])!=MH_OK||
    MH_QueueEnableHook(reinterpret_cast<void *>(targets[i]))!=MH_OK){reason="stream hook preparation";return false;}enabled[i]=true;}
 return true;
}
void begin() noexcept{accepting=true;}
void publish(const tb::Snapshot &s,const tb::Packet &packet) noexcept{
 Lock lock;if(!accepting.load())return;auto &p=plans[s.frame%plans.size()];
 if(p.prepared){fail(7);return;}
 unwatch(static_cast<unsigned>(s.frame%plans.size()));
 // Expanded overlay recipes must not create a half-megabyte stack temporary
 // on the game's render worker when a retired ring slot is reused.
 // perf::pooled_selectors: the GPU no longer reads the previous plan's selector
 // (a prepared plan must be retired before its slot is published again, and an
 // unprepared one was never submitted), so the slot keeps the buffer.
 auto selector=perf::on(perf::pooled_selectors)?std::move(p.selector):ComPtr<ID3D12Resource>{};
 std::destroy_at(&p);std::construct_at(&p);p.stamp=s;p.temporal=packet;p.published=true;++published;
 p.selector=std::move(selector);
}
bool copy_region(ID3D12GraphicsCommandList *list,const D3D12_TEXTURE_COPY_LOCATION *d,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION *s,const D3D12_BOX *box) noexcept{
 if(!originals[2])return false;reinterpret_cast<CopyFn>(originals[2])(list,d,x,y,z,s,box);return true;
}
bool ready(ID3D12GraphicsCommandList *list,std::uint64_t gen,const tb::Snapshot &s,std::uintptr_t recording) noexcept{
 Lock lock;auto &p=plans[s.frame%plans.size()];
 const auto note=[&](ReadinessTrace::Reason reason,unsigned stage=12){
  readiness.note(reason,[&]{ReadinessTrace::Snapshot v;
   v.at={GetTickCount64(),s.frame,s.epoch,reinterpret_cast<std::uintptr_t>(list),gen,recording};
   v.request={s.context,s.view,s.bank,s.async};
   v.plan={s.frame%plans.size(),p.stamp.frame,p.stamp.epoch,p.published,p.built,p.claimed,p.executed,p.temporal.object,
       p.stamp.context,p.stamp.view,p.stamp.bank,p.stamp.async};
   v.active={active.slot,reinterpret_cast<std::uintptr_t>(active.list),active.generation,active.recording,
       active.resolved,active.copied,active.draws,active.list?plans[active.slot].stamp.frame:0,active.list?plans[active.slot].stamp.epoch:0};
   v.accepting=accepting.load();v.fault=fault.load();v.stamp_equal=p.stamp==s;v.stage=stage;
   // Only a built plan's draws are complete (a rejected build keeps partial ones).
   if(p.built)for(unsigned ordinal=0;ordinal<effect_count(p);++ordinal){const auto i=effect_stage(p,ordinal);if(i>=p.draws.size()||!needs_pipeline(p,i))continue;const auto *cache=draw_pipeline(p,i);
       if(!cache||!cache->effect)v.pipeline_missing_mask|=1U<<i;
       if(!cache||p.draws[i].signature!=cache->signature)v.pipeline_signature_mask|=1U<<i;}
   return v;});return reason==ReadinessTrace::accepted;};
 // Keep the original short-circuit order. Recording a reason never permits a
 // rejected route, changes a packet, or acquires additional native resources.
 if(!accepting.load()&&!p.prepared)return note(ReadinessTrace::inactive);
 if(fault.load())return note(ReadinessTrace::faulted);
 if(active.list)return note(ReadinessTrace::active_route);
 if(!gen||!recording)return note(ReadinessTrace::arguments);
 if(!p.built)return note(ReadinessTrace::unbuilt);
 if(p.resolved)return note(ReadinessTrace::claimed);
 if(p.stamp!=s)return note(ReadinessTrace::stamp);
 for(unsigned ordinal=0;ordinal<effect_count(p);++ordinal){const auto i=effect_stage(p,ordinal);if(!needs_pipeline(p,i))continue;
  const auto *cache=draw_pipeline(p,i);if(!cache||!cache->effect)return note(ReadinessTrace::pipeline_missing,i);
  if(p.draws[i].signature!=cache->signature)return note(ReadinessTrace::pipeline_signature,i);}
 if(!p.prepared)return note(ReadinessTrace::unbuilt);
 tb::Packet packet;if(!tb::packet(reader,p.temporal.object,packet)||packet!=p.temporal)return note(ReadinessTrace::packet);
 if(p.has_haze&&!haze_bindings_stable(p))return note(ReadinessTrace::packet);
 const FrameHandoff::Recording recording_id{reinterpret_cast<std::uintptr_t>(list),gen};
 const auto existing=p.handoff.uses()[0].recording;
 if(!track_recording(p,list,gen,true)||!retain_recording(p,list,gen)||
    (existing?existing!=recording_id:p.handoff.record(0,p.temporal.object,recording_id)!=FrameHandoff::Result::ok)){note(ReadinessTrace::retention);fail(8);return false;}
 note(ReadinessTrace::accepted);
 active={};active.slot=static_cast<unsigned>(s.frame%plans.size());active.list=list;active.generation=gen;active.recording=recording;
 return true;
}
bool full_contract(const tb::Snapshot &s) noexcept{Lock lock;const auto &p=plans[s.frame%plans.size()];return p.prepared&&p.stamp==s;}
bool owns_packet(std::uintptr_t packet) noexcept{if(filtering()&&!watched_packet(packet))return false;Lock lock;if(fault.load()||abandoned)return false;for(const auto &p:plans)if(p.prepared&&p.temporal.object==packet&&!p.resolved)return true;return false;}
void resolved(bool ok,bool reconstructed) noexcept{
 Lock lock;if(!active.list||!ok){fail(9);return;}auto &p=plans[active.slot];
 if(!choose(p,reconstructed?FrameHandoff::Choice::reconstructed:FrameHandoff::Choice::spatial)){fail(9);return;}
 p.resolved=true;if(!reconstructed)++spatial_frames;active={};completed(p);
}
bool observe_resolve(std::uintptr_t packet,ID3D12GraphicsCommandList *list,std::uint64_t gen) noexcept{
 Lock lock;if(fault.load()||abandoned)return false;for(auto &p:plans)if(p.prepared&&!p.resolved&&p.temporal.object==packet){
  if(resolve_slot!=UINT_MAX){fail(8);return false;}
  if(!track_recording(p,list,gen,true)||!retain_recording(p,list,gen)||
     p.handoff.record(0,packet,{reinterpret_cast<std::uintptr_t>(list),gen})!=FrameHandoff::Result::ok){fail(8);return false;}
  resolve_slot=static_cast<unsigned>(p.stamp.frame%plans.size());return true;
 }return false;
}
void finish_resolve(std::uintptr_t packet,bool returned) noexcept{
 Lock lock;
 if(resolve_slot!=UINT_MAX&&plans[resolve_slot].temporal.object==packet)resolve_slot=UINT_MAX;
 for(auto &p:plans)if(p.prepared&&p.temporal.object==packet&&!p.resolved){
  if(!returned||!p.handoff.uses()[0].recording||!choose(p,FrameHandoff::Choice::native)){fail(9);return;}
  p.resolved=true;++native_frames;if(active.list&&active.slot==p.stamp.frame%plans.size())active={};completed(p);
 }
}
void observe_submission(std::uint64_t queue,std::uint64_t tick,std::span<const std::array<std::uint64_t,2>> lists) noexcept{
 Lock lock;diagnostics.submit(queue,tick,lists);
#ifdef MHW_FG_SCENE_PROBE
 scene_trace.submit(queue,scene_qpc(),lists);
#endif
}
unsigned compute_before(std::uintptr_t executor,std::uintptr_t backend_context,std::uintptr_t packet) noexcept{
 // Only the DOF passes and the bounded GPU samples act here.
 if(filtering()&&!compute_gpu_pending.load()&&!watched_packet(packet))return UINT_MAX;
 Lock lock;if(!accepting.load()&&!dof_active())return UINT_MAX;
 dof_before(executor,backend_context,packet);
 const unsigned dof_token=dof_scope.slot!=UINT_MAX?12:UINT_MAX;
 if(!compute_gpu_pending.load())return dof_token;
 for(unsigned s=0;s<compute_gpu_count;++s){auto &q=compute_gpu_samples[s];if(q.error)continue;const auto &cpu=compute_dependencies.sample(s);
  for(unsigned p=0;p<6;++p){auto &pass=q.passes[p];if(pass.entered||pass.error||cpu.computes[p].packet!=packet)continue;
   if(compute_gpu_scope!=UINT_MAX){pass.error=5;return dof_token;}
   auto &e=pass.entry;
   const auto reject=[&](unsigned step,unsigned error=1U){e[0]=step;pass.error=error;compute_gpu_pending.store(compute_gpu_any_pending());return dof_token;};
   if(!field(cfg.renderer,0x1f210,e[8]))return reject(12,6);
   if(e[8]<cpu.at[1]||e[8]-cpu.at[1]>2)return reject(13,6);
   temporal_present::Bindings bindings;
   if(!field(backend_context,8,e[1]))return reject(1);
   if(e[1]!=cfg.backend)return reject(2);
   if(!field(executor,8,e[2]))return reject(3);
   if(!field(e[2],0x20,e[3]))return reject(4);
   if(!e[3])return reject(5);
   e[4]=obs::inspect(reinterpret_cast<ID3D12GraphicsCommandList *>(e[3]),bindings)?1U:0U;
   e[5]=bindings.known?1U:0U;e[6]=bindings.valid?1U:0U;e[7]=bindings.generation;
   if(!e[4])return reject(6);
   if(!e[5])return reject(7);
   if(!e[6])return reject(8);
   if(!e[7])return reject(9);
   if(!compute_recipe_observed(cpu.computes[p],pass))return reject(pass.recipe[0]?11:10);
   pass.entered=true;pass.at={GetTickCount64(),e[3],bindings.generation,e[2],0};
   compute_gpu_scope=s*6+p;return compute_gpu_scope;
  }
 }compute_gpu_pending.store(compute_gpu_any_pending());return dof_token;
}
void compute_diagnostic_dispatch(ID3D12GraphicsCommandList *list,unsigned x,unsigned y,unsigned z,std::uintptr_t caller) noexcept{
 if(compute_gpu_scope==UINT_MAX)return;Lock lock;
 const auto s=compute_gpu_scope/6,p=compute_gpu_scope%6;auto &q=compute_gpu_samples[s];auto &pass=q.passes[p];
 const auto &cpu=compute_dependencies.sample(s).computes[p];++pass.calls;pass.at[4]=GetTickCount64();
 temporal_present::Bindings bindings;std::uintptr_t packet{};
 if(pass.calls!=1||caller!=cfg.image+0x259cfca||reinterpret_cast<std::uintptr_t>(list)!=pass.at[1]||
    !field(pass.at[3],0x158,packet)||packet!=cpu.packet||!obs::inspect(list,bindings)||!bindings.known||!bindings.valid||
    bindings.generation!=pass.at[2]||!bindings.compute.signature||!bindings.pipeline||
    std::array<unsigned,3>{x,y,z}!=std::array<unsigned,3>{cpu.prefix[6],cpu.prefix[7],cpu.prefix[8]}){pass.error=2;return;}
 if(!compute_recipe_observed(cpu,pass)){pass.error=3;return;}
 pass.pipeline={bindings.compute.signature,bindings.pipeline};
 for(unsigned u=0;u<pass.use_count;++u){const auto resource=q.resources[pass.uses[u][1]].owner.fields[3];
  const auto state=compute_gpu_states.get(pass.at[1],pass.at[2],resource);pass.states[u]={state.known?1U:0U,state.known?state.value:0U};
 }
 if(p==0&&compute_depth_pixels[s].available()){
  auto &capture=compute_depth_pixels[s];unsigned depth_use=UINT_MAX,count{};
  for(unsigned u=0;u<pass.use_count;++u)if((pass.uses[u][0]&15)==2&&(pass.uses[u][0]>>24)==0){depth_use=u;++count;}
  obs::Predicate predicate;
  if(count!=1||std::array<unsigned,3>{x,y,z}!=std::array<unsigned,3>{(width+319)/320,height,1}||
     pb::bytes_sha256(cpu.shader.bytes.data(),cpu.shader.size)!="98c622bab8c0e113eeb4062dcc78fec129441854bcf67f61db6ed6239ca68e3c")capture.reject(1);
  else if(!pass.states[depth_use][0]||(pass.states[depth_use][1]!=0x40&&pass.states[depth_use][1]!=0xc0))capture.reject(2);
  else if(!obs::predicate_current(list,predicate)||!predicate.known||predicate.buffer)capture.reject(3);
  else {
   const auto &resource=q.resources[pass.uses[depth_use][1]];
   // Bypass our own copy/barrier observation; restore the exact native state.
   injecting=true;
   const bool copied=capture.record(list,resource.hold.Get(),pass.states[depth_use][1],pass.at[2],pass.entry[8],cpu.packet);
   injecting=false;if(!copied)capture.reject(4);
  }
 }
}
bool compute_dispatch(ID3D12GraphicsCommandList *list,unsigned x,unsigned y,unsigned z,std::uintptr_t caller) noexcept{
 if(injecting)return false;
 compute_diagnostic_dispatch(list,x,y,z,caller);
 return dof_dispatch(list,x,y,z,caller);
}
void compute_after(unsigned token,bool returned) noexcept{
 Lock lock;dof_after(returned);if(token>=12)return;auto &p=compute_gpu_samples[token/6].passes[token%6];
 p.returned=returned;if(compute_gpu_scope!=token||!returned||p.calls!=1)p.error=4;
 compute_gpu_pending.store(compute_gpu_any_pending());
 compute_gpu_scope=UINT_MAX;
}
void barriers(ID3D12GraphicsCommandList *list,unsigned n,const D3D12_RESOURCE_BARRIER *data) noexcept{
 if(injecting)return;
#ifndef MHW_FG_SCENE_PROBE
 // Only aliasing barriers and transitions of listed plan resources reach the
 // journal; an unreadable or oversized batch takes the locked path and fails there.
 if(filtering()&&!compute_gpu_pending.load()&&n<=4096){bool listed=false;
  for(unsigned j=0;j<n&&!listed;++j){D3D12_RESOURCE_BARRIER b{};
   listed=!pb::copy_memory(reinterpret_cast<std::uintptr_t>(data+j),&b,sizeof(b))||b.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING||
    (b.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION&&watched_resource(reinterpret_cast<std::uintptr_t>(b.Transition.pResource)));}
  if(!listed)return;}
#endif
 Lock lock;if(fault.load())return;
 const auto slot=copy_slot!=UINT_MAX?copy_slot:api_slot!=UINT_MAX?api_slot:resolve_slot;
 temporal_present::Bindings bindings;if(!obs::inspect(list,bindings)||!bindings.known||!bindings.valid)return;
 if(n>4096){fail(10);return;}
 for(unsigned j=0;j<n;++j){D3D12_RESOURCE_BARRIER b{};if(!pb::copy_memory(reinterpret_cast<std::uintptr_t>(data+j),&b,sizeof(b))){fail(10);return;}
  compute_gpu_barrier(list,bindings.generation,b);
#ifdef MHW_FG_SCENE_PROBE
  scene_barrier(list,bindings.generation,b);
#endif
  // Observe the whole recording, including barriers emitted before an owned
  // native callback. Only already pinned plan resources may enter the journal.
  // A resource shared by several plans/roles is observed exactly once.
  std::array<ID3D12Resource *,32*resource_capacity> visited{};unsigned visited_count{};
  for(auto &p:plans)if(p.prepared&&p.handoff.submitted_count()!=p.handoff.count()){
  for(unsigned i=0;i<resource_count(p);++i){auto *resource=p.resources[i].Get();if(!resource)continue;
   const bool alias=b.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING&&(!b.Aliasing.pResourceBefore||!b.Aliasing.pResourceAfter||b.Aliasing.pResourceBefore==resource||b.Aliasing.pResourceAfter==resource);
   if(!alias&&(b.Type!=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION||b.Transition.pResource!=resource))continue;
   if(std::find(visited.begin(),visited.begin()+visited_count,resource)==visited.begin()+visited_count){
    visited[visited_count++]=resource;
    const bool full=!alias&&!b.Flags&&(b.Transition.Subresource==0||b.Transition.Subresource==UINT_MAX);
    const auto result=recording_states.observe(reinterpret_cast<std::uintptr_t>(list),bindings.generation,reinterpret_cast<std::uintptr_t>(resource),
      full,full?static_cast<unsigned>(b.Transition.StateBefore):0,full?static_cast<unsigned>(b.Transition.StateAfter):0,p.resources[i]);
    if(result!=StateJournal::Result::ok){fail(10);return;}++state_observations;if(slot==UINT_MAX)++state_unscoped;
   }
   // Existing plan records receive the update, without enrolling unrelated
   // recordings in a plan or carrying knowledge across Reset generations.
   if(auto *track=track_recording(p,list,bindings.generation,false)){
    const auto state=recording_states.get(reinterpret_cast<std::uintptr_t>(list),bindings.generation,reinterpret_cast<std::uintptr_t>(resource));
    track->known[i]=state.known;track->states[i]=state.value;
   }
  }
  }
 }
}
bool intersects(const Plan &p,std::span<const std::array<std::uint64_t,2>> batch){
 for(const auto &use:p.handoff.uses())for(const auto &entry:batch)
  if(use.recording&&use.recording==FrameHandoff::Recording{entry[0],entry[1]})return true;
 return false;
}
unsigned batch_recordings(std::span<const std::array<std::uint64_t,2>> batch,std::array<FrameHandoff::Recording,128> &out){
 unsigned count{};for(const auto &entry:batch)if(entry[0]&&entry[1])out[count++]={entry[0],entry[1]};return count;
}
bool submission_needed(std::span<const std::array<std::uint64_t,2>> batch) noexcept{
 Lock lock;for(const auto &entry:batch){if(lifetime.tracked(reinterpret_cast<IUnknown *>(entry[0]),entry[1]))return true;
  for(const auto &capture:compute_depth_pixels)if(capture.tracked(entry[0],entry[1]))return true;}return false;
}
bool validate_submission(std::uint64_t queue,std::span<const std::array<std::uint64_t,2>> batch,std::uint64_t signal) noexcept{
 Lock lock;if(batch.size()>128)return false;
 for(const auto &capture:compute_depth_pixels)if(!capture.validate(queue,batch,signal))return false;
 std::array<FrameHandoff::Recording,128> records{};const auto count=batch_recordings(batch,records);
 for(const auto &p:plans)if(p.prepared&&intersects(p,batch)){
  unsigned end{};
  if(p.handoff.validate_submission(queue,std::span(records).first(count),signal,end)!=FrameHandoff::Result::ok)return false;
 }return true;
}
bool submitted_batch(std::uint64_t queue,std::span<const std::array<std::uint64_t,2>> batch,std::uint64_t signal) noexcept{
 Lock lock;if(batch.size()>128){fail(11);return false;}
 for(const auto &capture:compute_depth_pixels)if(!capture.validate(queue,batch,signal)){fail(11);return false;}
 std::array<FrameHandoff::Recording,128> records{};const auto count=batch_recordings(batch,records);
 // Validate the entire batch before changing any transaction or pin receipt.
 for(const auto &p:plans)if(p.prepared&&intersects(p,batch)){
  unsigned end{};if(p.handoff.validate_submission(queue,std::span(records).first(count),signal,end)!=FrameHandoff::Result::ok){fail(11);return false;}}
 for(auto &p:plans)if(p.prepared&&intersects(p,batch)){
  if(p.handoff.submitted(queue,std::span(records).first(count),signal)!=FrameHandoff::Result::ok){fail(11);return false;}
  if(p.handoff.submitted_count()==p.handoff.count())++submitted_frames;
 }
 for(const auto &entry:batch){auto *list=reinterpret_cast<IUnknown *>(entry[0]);
  if(lifetime.tracked(list,entry[1])&&!lifetime.submitted(list,entry[1],signal)){fail(11);return false;}}
 for(auto &capture:compute_depth_pixels)capture.submitted(queue,batch,signal);
 return true;
}
void reject_submission(std::span<const std::array<std::uint64_t,2>> batch) noexcept{
 Lock lock;
 // Native Execute is always forwarded. Seal outstanding selectors before it,
 // so a late CPU callback cannot change a value the GPU may already be reading.
 for(auto &p:plans)if(p.prepared&&intersects(p,batch)&&p.handoff.choice()==FrameHandoff::Choice::undecided)
  (void)choose(p,FrameHandoff::Choice::native);
 fail(11);
}
bool reset(ID3D12GraphicsCommandList *list,std::uint64_t gen) noexcept{
 Lock lock;if(!lifetime.reset(list,gen)){fail(12);return false;}
#ifdef MHW_FG_SCENE_PROBE
 scene_trace.reset(reinterpret_cast<std::uintptr_t>(list),gen);
#endif
 for(auto &capture:compute_depth_pixels)if(!capture.reset(reinterpret_cast<std::uintptr_t>(list),gen)){fail(12);return false;}
 compute_gpu_states.reset(reinterpret_cast<std::uintptr_t>(list),gen);
 if(!recording_states.reset(reinterpret_cast<std::uintptr_t>(list),gen)){fail(12);return false;}
 for(auto &p:plans)if(p.prepared){
  const auto result=p.handoff.reset({reinterpret_cast<std::uintptr_t>(list),gen});
  if(result!=FrameHandoff::Result::ok&&result!=FrameHandoff::Result::missing){fail(12);return false;}
 }return true;
}
void collect(std::uint64_t value) noexcept{Lock lock;if(!fault.load()){
 for(auto &capture:compute_depth_pixels)capture.collect(value);
 lifetime.collect(value);for(auto &p:plans)if(p.prepared&&p.handoff.retired(value))p.prepared=false;}}
bool drained() noexcept{Lock lock;if(pending.load()||active.list)return false;
 for(const auto &capture:compute_depth_pixels)if(capture.pending_submission())return false;
 for(const auto &p:plans)if(p.prepared&&p.handoff.submitted_count()!=p.handoff.count())return false;return true;}
bool callbacks_idle() noexcept{return pending.load()==0;}
bool scene_request(unsigned request,const frame_generation::PresentationBuffers &buffers) noexcept {
#ifdef MHW_FG_SCENE_PROBE
 Lock lock;return accepting.load()&&!fault.load()&&scene_trace.request(request,buffers);
#else
 (void)request;(void)buffers;return false;
#endif
}
void scene_present(std::uint64_t frame,std::uint64_t epoch,unsigned reflex_id,unsigned thread,std::uintptr_t swap,
    const frame_generation::PresentationBuffers &buffers) noexcept {
#ifdef MHW_FG_SCENE_PROBE
 Lock lock;scene_trace.present(frame,epoch,reflex_id,thread,swap,scene_qpc(),buffers);
#else
 (void)frame;(void)epoch;(void)reflex_id;(void)thread;(void)swap;(void)buffers;
#endif
}
bool abandon() noexcept{
 Lock lock;if(accepting.load()||pending.load())return false;
 // Never report unresolved recordings as submitted/completed or release pins.
 // Already published selectors are immutable; undecided selectors remain
 // native and cannot be changed by a later callback after the fault cutoff.
 fail(13);
 for(auto &p:plans)if(p.prepared&&p.handoff.choice()==FrameHandoff::Choice::undecided)
  if(!choose(p,FrameHandoff::Choice::native))return false;
 active={};abandoned=true;return true;
}
void stop() noexcept{accepting=false;}
namespace {
bool idle_locked() noexcept{
 if(accepting.load()||fault.load()||abandoned||active.list||dof_active())return false;
 for(const auto &capture:compute_depth_pixels)if(capture.pending_submission())return false;
 for(const auto &p:plans)if(p.prepared)return false;
 return true;
}
}
bool idle() noexcept{Lock lock;return idle_locked();}
bool resize(unsigned rw,unsigned rh,unsigned w,unsigned h,std::string &reason) noexcept{
 try{
  Lock lock;
  if(!idle_locked()){reason="stream route not idle";return false;}
  if(!valid_filter_geometry({rw,rh,w,h})){reason="stream extent";return false;}
  // The diagnostic depth readbacks are sized for the output at prepare.
  if(w!=width||h!=height)for(auto &capture:compute_depth_pixels)capture.disable();
  render_w=rw;render_h=rh;width=w;height=h;
  for(auto &p:pipelines)p={};
  for(auto &p:dependency_pipelines)p={};
  for(auto &p:silhouette_pipelines)p={};
  // Built but never prepared plans hold the old geometry; publish() would
  // replace them anyway. A pooled selector stays with its slot.
  for(unsigned i=0;i<plans.size();++i){auto &p=plans[i];unwatch(i);
   auto selector=std::move(p.selector);std::destroy_at(&p);std::construct_at(&p);p.selector=std::move(selector);}
  vertex_missed=rect_missed=final_missed=copy_missed=false;
  return true;
 }catch(...){reason="stream resize";return false;}
}
bool queue_disable() noexcept{for(unsigned i=0;i<targets.size();++i)if(enabled[i]&&!(astera::active()&&i==api_hook)&&MH_QueueDisableHook(reinterpret_cast<void *>(targets[i]))!=MH_OK)return false;return true;}
void chains_live(bool yes) noexcept{chain_live.store(yes,std::memory_order_release);}
void disabled(bool yes) noexcept{if(yes){chain_live.store(false,std::memory_order_release);Lock lock;enabled.fill(false);recording_states.clear();compute_gpu_states.stop();for(auto &s:compute_gpu_samples)for(auto &r:s.resources)r.hold.Reset();for(auto &c:compute_depth_pixels)c.disable();}}
void save_compute_depth(const std::filesystem::path &prefix) noexcept{
 try{Lock lock;if(fault.load())return;for(unsigned i=0;i<compute_depth_pixels.size();++i)
  compute_depth_pixels[i].save(prefix.wstring()+L"-gallery-depth-"+std::to_wstring(i)+L".r32f");}catch(...){/* No renderer changes for artifact failure. */}
}
void save_inserted_effect(const std::filesystem::path &prefix) noexcept{
 // Once per session, a few hundred KB (shader bytes), when first captured.
 try{Lock lock;if(inserted_saved||!inserted_dependencies.size())return;
  std::ofstream out(prefix.wstring()+L".inserted-effect.json",std::ios::binary|std::ios::trunc);
  inserted_dependencies.write(out);out<<'\n';inserted_saved=bool(out);}catch(...){/* No renderer changes for artifact failure. */}
}
bool healthy() noexcept{return !fault.load();}
#ifdef MHW_FG_SCENE_PROBE
// Status writers copy the bounded CPU map under the route lock, then
// serialize it after release so render callbacks never wait on its text.
SceneTrace scene_snapshot;
SRWLOCK scene_write_lock=SRWLOCK_INIT;
struct SceneWriteLock {SceneWriteLock(){AcquireSRWLockExclusive(&scene_write_lock);}~SceneWriteLock(){ReleaseSRWLockExclusive(&scene_write_lock);}};
#endif
void write_summary(std::ostream &o){
 Lock lock;const auto a=[&](const auto &v){o<<'[';for(unsigned i=0;i<v.size();++i){if(i)o<<',';o<<v[i];}o<<']';};
 o<<"{\"fault\":"<<fault.load()<<",\"published\":"<<published<<",\"built\":"<<built<<",\"claimed\":"<<claimed
  <<",\"identity\":{\"plans\":"<<identity_plans<<",\"final_copies\":"<<identity_final_copies<<",\"observed\":"<<identity_observed<<'}'
  <<",\"spatial_frames\":"<<spatial_frames<<",\"native_frames\":"<<native_frames<<",\"failure\":"<<std::quoted(failure)
  <<",\"dependency_draws\":";a(dependency_draws);o<<",\"plan_rejections\":";a(rejection_counts);
 o<<",\"readiness\":";a(readiness.reasons());
 o<<",\"unknown_shaders\":"<<unknown_count<<",\"unknown_shader\":";
 if(unknown_count)o<<"{\"key\":"<<unknown_key<<",\"ps_sha256\":\""<<unknown_shader<<"\"}";else o<<"null";
 o<<",\"inserted_capture\":{\"attempts\":"<<inserted_dependencies.attempts()<<",\"samples\":"<<inserted_dependencies.size()<<",\"saved\":"<<inserted_saved<<",\"errors\":";
 a(inserted_dependencies.errors());o<<'}';
 // Late indexed draws (hunter silhouette pieces): a rejected scan leaves the frame native.
 o<<",\"late_draws\":{\"planned\":"<<silhouette_planned<<",\"rejected\":"<<indexed_candidates_rejected<<",\"reasons\":";
 a(indexed_candidate_reasons);o<<",\"max\":"<<indexed_candidates_max<<",\"capacity\":"<<Plan::late_capacity<<'}';
 // Which late draws drew the output-sized silhouette, and why others stayed native.
 o<<",\"silhouette\":";silhouette_summary(o);
 // Depth of field: a failed qualification rejects the whole plan (rejection 17,
 // qualification 14), so DLSS skips the frame. Which check failed, and how.
 o<<",\"dof\":";dof_write(o);
 // Dependency discovery failures: {rejection, stage, texture check, role, engine
 // format, DXGI format, width, height} of the first and the latest.
 o<<",\"dependency_failures\":"<<dependency_failures<<",\"dependency_failure\":{\"first\":";a(dependency_first_failure);
 o<<",\"latest\":";a(dependency_latest_failure);o<<"},\"latest_rejection\":";
 if(diagnostics.rejected_graphs()!=summarized_rejections)diagnostics.write_latest_rejection(o);else o<<"null";
 summarized_rejections=diagnostics.rejected_graphs();o<<'}';
}
void write(std::ostream &o){
#ifdef MHW_FG_SCENE_PROBE
 SceneWriteLock scene_guard;
 {
#endif
 Lock lock;bool off=true;for(bool b:enabled)off=off&&!b;
 unsigned unresolved{},unsubmitted{};for(const auto &p:plans)if(p.prepared){unresolved+=!p.resolved;unsubmitted+=p.handoff.submitted_count()!=p.handoff.count();}
 o<<"{\"silhouette_probe\":";indexed_write(o);o<<",\"silhouette_output\":";silhouette_write(o);
 o<<",\"schema\":3,\"fault\":"<<fault.load()<<",\"pending\":"<<pending.load()<<",\"hooks_disabled\":"<<off<<",\"published\":"<<published<<",\"built\":"<<built
 <<",\"unqualified\":"<<unqualified<<",\"claimed\":"<<claimed<<",\"copies\":"<<copies<<",\"draws\":"<<draws<<",\"complete\":"<<complete<<",\"warmup_draws\":"<<warmups
 <<",\"haze_claimed\":"<<haze_claimed<<",\"haze_complete\":"<<haze_complete<<",\"haze_draws\":"<<haze_draws
 <<",\"haze_admission_rejection\":"<<haze_admission_rejection
 <<",\"prepared_frames\":"<<prepared_frames<<",\"spatial_frames\":"<<spatial_frames<<",\"early_draws\":"<<early_draws<<",\"multilist_frames\":"<<multilist_frames<<",\"submitted_frames\":"<<submitted_frames
 <<",\"native_frames\":"<<native_frames<<",\"unresolved_frames\":"<<unresolved<<",\"unsubmitted_frames\":"<<unsubmitted<<",\"abandoned\":"<<abandoned<<",\"selection\":\"immutable-frame-predicate-v1\""
 <<",\"active\":"<<(active.list!=nullptr)<<",\"retained\":"<<lifetime.retained()<<",\"released\":"<<lifetime.released()<<",\"live\":"<<lifetime.live()<<",\"peak\":"<<lifetime.peak()
 <<",\"final_captured\":"<<final_captured<<",\"final_rejected\":"<<final_rejected<<",\"plan_rejection\":"<<plan_rejection<<",\"plan_detail\":"<<plan_detail<<",\"failure\":"<<std::quoted(failure)
 <<",\"haze_aux_descriptions\":[";
 for(unsigned i=0;i<haze_aux_descriptions.size();++i){const auto &d=haze_aux_descriptions[i];if(i)o<<',';
  o<<'['<<d.Dimension<<','<<d.Width<<','<<d.Height<<','<<d.DepthOrArraySize<<','<<d.MipLevels<<','<<d.Format<<','<<d.SampleDesc.Count<<','<<d.SampleDesc.Quality<<','<<d.Flags<<']';}
 o<<"],\"recording_states\":{\"schema\":1,\"policy\":\"native-recording-observation-v1\",\"observations\":"<<state_observations<<",\"outside_selected_callbacks\":"<<state_unscoped<<",\"imported\":"<<state_imports<<"},\"readiness\":";readiness.write(o,4);o<<",\"diagnostics\":";diagnostics.write(o);o<<",\"haze_capture\":";haze_write(o);
 o<<",\"overlays\":{\"schema\":1,\"policy\":\"native-extent-and-dependencies-v2\",\"prepared\":"<<overlay_planned<<",\"draws\":"<<overlay_draws<<",\"completed\":"<<overlay_completed<<",\"rejected\":"<<overlay_rejected;
 o<<",\"shared_key_prepared\":"<<overlay_shared_prepared<<",\"shared_key_completed\":"<<overlay_shared_completed;
 o<<",\"extended_draws\":"<<overlay_extended_draws<<",\"extended_layout\":";
 if(overlay_extended_draws){o<<'[';for(unsigned i=0;i<overlay_extended_layout.size();++i){if(i)o<<',';o<<overlay_extended_layout[i];}o<<']';}else o<<"null";o<<"}";
 o<<",\"prepared_queue\":";prepared_queues.write(o);o<<",\"draw_state_failures\":";draw_state_trace.write(o);
 o<<",\"dependency_route\":{\"schema\":2,\"copy_draws\":"<<dependency_draws[0]<<",\"radial_draws\":"<<dependency_draws[1]<<",\"dof_draws\":"<<dependency_draws[2]<<",\"screen_filter_draws\":"<<dependency_draws[3]<<",\"rejection\":"<<dependency_rejection<<",\"last_description\":[";
 for(unsigned i=0;i<dependency_description.size();++i){if(i)o<<',';o<<dependency_description[i];}
 o<<"],\"texture_check\":"<<dependency_texture_check<<",\"failures\":"<<dependency_failures<<",\"first_failure\":[";
 for(unsigned i=0;i<dependency_first_failure.size();++i){if(i)o<<',';o<<dependency_first_failure[i];}o<<"]}";
 o<<",\"extent\":{\"render\":["<<render_w<<','<<render_h<<"],\"output\":["<<width<<','<<height<<"],\"vertex_miss\":["<<vertex_miss[0]<<','<<vertex_miss[1]<<','<<vertex_miss[2]<<','<<vertex_miss[3]
  <<"],\"rect_miss\":["<<rect_miss[0]<<','<<rect_miss[1]<<','<<rect_miss[2]<<','<<rect_miss[3]<<','<<rect_miss[4]
  <<"],\"final_miss\":["<<final_miss[0]<<','<<final_miss[1]<<','<<final_miss[2]<<','<<final_miss[3]
  <<"],\"copy_miss\":["<<copy_miss[0]<<','<<copy_miss[1]<<','<<copy_miss[2]<<','<<copy_miss[3]<<"]}";
 o<<",\"dependency_binding_checks\":";dependency_binding_trace.write(o);
 o<<",\"dependency_refresh\":";dependency_refresh.write(o);
 o<<",\"dependencies\":";dependencies.write(o);
#ifdef MHW_FG_SCENE_PROBE
 scene_snapshot=scene_trace; // Serialized below, after the route lock.
#endif
 o<<",\"intermediate_dependencies\":";intermediate_dependencies.write(o);
 o<<",\"inserted_dependencies\":";inserted_dependencies.write(o);
 o<<",\"dof_native_sequence\":";dof_write(o);
 o<<",\"compute_dependencies\":";compute_dependencies.write(o);o<<",\"compute_resources\":";compute_gpu_write(o);
 o<<",\"compute_depth_pixels\":{\"schema\":1,\"policy\":\"native-depth-before-reduction-v1\",\"diagnostic_only\":true,\"captures\":[";
 for(unsigned i=0;i<compute_depth_pixels.size();++i){if(i)o<<',';compute_depth_pixels[i].write(o);}o<<"]}";
#ifdef MHW_FG_SCENE_PROBE
 }
 o<<",\"scene_boundary\":";scene_snapshot.write(o);
#endif
 o<<'}';
}
}
extern "C" void mhw_scaled_postfilter(mhw::projection_binding::Frame *f){mhw::scaled_temporal::stream_route::post_dispatch(f);}
extern "C" void mhw_scaled_draw_execution(mhw::projection_binding::Frame *f){mhw::scaled_temporal::stream_route::draw_dispatch(f);}
extern "C" void mhw_scaled_stream_final(mhw::projection_binding::Frame *f){mhw::scaled_temporal::stream_route::final_dispatch(f);}
extern "C" void mhw_scaled_copy_execution(mhw::projection_binding::Frame *f){mhw::scaled_temporal::stream_route::copy_dispatch(f);}
extern "C" void mhw_scaled_indexed_execution(mhw::projection_binding::Frame *f){mhw::scaled_temporal::stream_route::indexed_dispatch(f);}
namespace mhw::scaled_temporal::stream_route {
namespace {
template<auto Detour> struct Chained;
template<class... A,void(WINAPI *Detour)(A...)> struct Chained<Detour> {
    static void WINAPI call(A... a){
        if(chain_live.load(std::memory_order_acquire))Detour(a...);
        else reinterpret_cast<void(WINAPI *)(A...)>(originals[api_hook])(a...);
    }
};
}
void astera_hooks(std::vector<astera::Hook> &out){
    out.push_back({astera::method(astera::command_list,16),reinterpret_cast<void *>(&Chained<&copy_hook>::call),&originals[api_hook]});
}
}
