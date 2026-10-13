#include "runtime.hpp"
#ifdef MHW_CONTINUOUS_SCALED
#include "../scaled_temporal/stream_route.hpp"
#include "../scaled_temporal/resolve.hpp"
#include "../render_resolution/runtime.hpp"
#include "../render_resolution/control.hpp"
#include "scaled_depth_trace.hpp"
#include "depth_schedule_trace.hpp"
#include "depth_consumer_admission.hpp"
#include "depth_consumer_copy.hpp"
#endif
#include "activation.hpp"
#include "history.hpp"
#include "requests.hpp"
#include "retention.hpp"
#include "native_taa.hpp"
#include "shader_rejection.hpp"
#include "resolve.hpp"
#include "unfiltered_lifetime.hpp"
#include "reconstruction.hpp"
#include "state_trace.hpp"
#include "native_state_admission.hpp"
#include "depth_eligibility.hpp"
#include "depth_destination.hpp"
#include "depth_lease.hpp"
#include "identity_depth_survey.hpp"
#include "../temporal_present/unfiltered.hpp"
#include "../temporal_bindings/depth_routes.hpp"
#include "../temporal_bindings/shader.hpp"
#include "../projection_binding/memory.hpp"
#include "../bridge_probe/readable_region.hpp"
#include "../perf/hook_timing.hpp"
#include "../perf/in_flight.hpp"
#include "../dlss_sdk/inline.hpp"
#include "../gpu_memory/budget.hpp"
#include "../temporal_present/observer.hpp"
#include "../ngx_startup/startup.hpp"
#ifdef MHW_FG_GAME
#include "../frame_generation/game_session.hpp"
#endif
#include "../astera/astera_hooks.hpp"
#include <MinHook.h>
#include <wrl/client.h>
#include <atomic>
#include <bit>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <intrin.h>
#include <stdexcept>
extern "C" void temporal_dispatch_entry();
extern "C" void temporal_consumer_entry();
extern "C" void temporal_depth_producer_entry();
extern "C" void temporal_depth_consumer_entry();
extern "C" void temporal_jitter_entry();
extern "C" void mhw_binding_forward(void *,mhw::projection_binding::Frame *);
namespace mhw::projection_binding {bool discover(std::uintptr_t,Runtime &,std::string &);
bool refresh_scene(std::uintptr_t,const Runtime &,Runtime &,std::string &);}
namespace mhw::continuous_dlaa {
namespace {
namespace pb=projection_binding;namespace tb=temporal_bindings;namespace dr=tb::depth_routes;namespace obs=temporal_present::observer;
using Microsoft::WRL::ComPtr;using tb::NativeFrame;
tb::Config cfg;pb::Runtime runtime;
#ifdef MHW_CONTINUOUS_SCALED
namespace sr=scaled_temporal::stream_route;
constexpr unsigned hook_count=13;
constexpr auto runtime_history_policy=HistoryPolicy::distance_only_comparison;
std::unique_ptr<temporal_present::Output> spatial;
std::uint64_t spatial_recordings{},route_only_signals{};
dlss_sdk::Geometry scaled_geometry;
// The game's render extent at scale 0.5 (render_resolution::plan): 2:1 for
// even scenes, truncated per axis for a pillarboxed odd scene (3413 -> 1706).
dlss_sdk::Extent scaled_render{};
unsigned render_width(){return scaled_render.width;}unsigned render_height(){return scaled_render.height;}
dlss_sdk::ReconstructionMode configured_mode{dlss_sdk::ReconstructionMode::performance};
#else
constexpr unsigned hook_count=11;
constexpr auto runtime_history_policy=HistoryPolicy::camera_displacement;
unsigned render_width(){return runtime.width;}unsigned render_height(){return runtime.height;}
#endif
// pending: callbacks in flight, without a locked instruction a call
// (perf/in_flight.hpp; the indexed-draw hook alone runs about 2,600 times a
// frame on several recording threads). Drain checks use zero().
std::atomic<bool> accepting{};perf::InFlightCount pending;std::atomic<unsigned> fault{};
std::atomic<std::uint64_t> frames{},skips{},jitter_count{},restorations{},reset_count{},depth_count{};
std::atomic<unsigned> last_skip{};
SRWLOCK mutex=SRWLOCK_INIT;
struct Lock {Lock(){AcquireSRWLockExclusive(&mutex);}~Lock(){ReleaseSRWLockExclusive(&mutex);}};
struct Guard {Guard(){pending.add();}~Guard(){pending.sub();}};
void fail(unsigned n) noexcept {unsigned zero{};fault.compare_exchange_strong(zero,n);accepting.store(false);}
bool read(void *,std::uintptr_t p,std::span<std::byte> out){return pb::copy_memory(p,out.data(),static_cast<unsigned>(out.size()));}
const tb::Reader reader{nullptr,read};
std::array<std::uintptr_t,hook_count> targets{};std::array<void *,hook_count> originals{};std::array<bool,hook_count> enabled{};
// AL: hooks 5.. (D3D12) are AL chains; their detours run from install to
// finish, as the MinHook patches did, and otherwise continue straight on.
constexpr unsigned first_api_hook=5;
std::atomic<bool> chain_live{};
// native: producer, consumer, depth producer, depth consumer, jitter;
// D3D: Reset, Close, ExecuteCommandLists, ResourceBarrier, Dispatch, CopyResource.
using BarrierFn=void(WINAPI *)(ID3D12GraphicsCommandList *,UINT,const D3D12_RESOURCE_BARRIER *);
using DispatchFn=void(WINAPI *)(ID3D12GraphicsCommandList *,UINT,UINT,UINT);
using CopyFn=void(WINAPI *)(ID3D12GraphicsCommandList *,ID3D12Resource *,ID3D12Resource *);
RecordingPins recording_pins;
UnfilteredLifetime unfiltered_lifetime;
ReconstructionState reconstruction;
std::atomic<std::uint64_t> zero_jitter_projections{};
std::array<std::uint64_t,12> dispatch_reasons{};
StateTrace state_trace;
NativeStateAdmission state_admission;
DepthEligibilityTrace depth_eligibility;
DepthDestinationTrace depth_destination;
struct List {ComPtr<ID3D12GraphicsCommandList> value;std::uint64_t generation{},injected_generation{},signal{};bool closed{},submitted{};};
// Keeps native Execute and our fence signals in one CPU order (execute_hook),
// and orders the live rebuild's queue barrier with them (change_mode).
SRWLOCK submission_lock=SRWLOCK_INIT;
struct SubmissionGuard {SubmissionGuard(){AcquireSRWLockExclusive(&submission_lock);}~SubmissionGuard(){ReleaseSRWLockExclusive(&submission_lock);}};
// list_count: the slots used so far; a slot whose list was pruned is empty
// (value null) until another list takes it.
std::array<List,128> lists;unsigned list_count{};std::uint64_t generation{},sequence{},signal_value{};
std::uint64_t lists_pruned{};unsigned lists_peak{};
ComPtr<ID3D12CommandQueue> last_queue;ComPtr<ID3D12Fence> fence;
std::array<ComPtr<ID3D12Resource>,3> raw;
std::unique_ptr<dlss_sdk::InlineFrame> pass;
std::filesystem::path sdk_runtime;  // install()'s NGX runtime folder
struct Lease {ID3D12GraphicsCommandList *list{};std::uint64_t generation{},frame{};bool depth{},evaluated{};} lease;
HistoryStamp previous;
// Keep the previous depth identity alive across comparisons, so freeing old
// recordings cannot let a newly allocated resource reuse its pointer identity.
ComPtr<ID3D12Resource> history_depth;
HistoryTelemetry history_telemetry{runtime_history_policy};
struct Projection {pb::Source source;HistoryStamp history;std::uint64_t candidate{};bool valid{};};
std::array<Projection,32> projections;
struct Taa {tb::Snapshot source;tb::SceneDepth depth;tb::Packet packet;tb::ShaderRecipe shader;Projection projection;std::uint64_t sequence{};bool live{},forwarding{};
    ResourcePins resources;
#ifdef MHW_CONTINUOUS_SCALED
    std::uint64_t depth_sequence{};
    std::uintptr_t depth_object{};dr::CopyPacket depth_packet;
#endif
    std::uint64_t bypasses{},last_bypass_tick{},last_bypass_caller{};unsigned last_bypass_publishers{};};
struct Depth {dr::Stamp stamp;dr::CopyPacket packet;tb::Owner source,destination;std::uintptr_t object{};std::uint64_t sequence{};bool live{};};
#ifdef MHW_CONTINUOUS_SCALED
ScaledDepthTrace scaled_depth_trace;
DepthScheduleTrace depth_schedule_trace;
DepthConsumerAdmission depth_consumer_admission;
IdentityDepthSurvey identity_depth_survey;
SRWLOCK depth_trace_mutex=SRWLOCK_INIT;
struct DepthTraceLock {DepthTraceLock(){AcquireSRWLockExclusive(&depth_trace_mutex);}~DepthTraceLock(){ReleaseSRWLockExclusive(&depth_trace_mutex);}};
void note_depth(unsigned stage,unsigned reason,const Depth &d,ScaledDepthTrace::Values values={}) noexcept {
    auto frame=d.stamp.frame,epoch=d.stamp.epoch;
    // An early producer rejection may lack a validated stamp. These two
    // diagnostic reads never grant frame/resource ownership.
    if(!frame)reader.get(cfg.renderer+0x1f210,frame);
    if(!epoch)reader.get(cfg.renderer+0x1f208,epoch);
    DepthTraceLock lock;scaled_depth_trace.observe({GetTickCount64(),frame,epoch,d.sequence,stage,reason,values});
}
#define DEPTH_NOTE(...) note_depth(__VA_ARGS__)
#else
#define DEPTH_NOTE(...) ((void)0)
#endif
std::array<Taa,32> requests;std::array<Depth,64> depths;
std::atomic<unsigned> publishing{};
RequestTelemetry request_telemetry;
ForwardedRequestTelemetry forwarded_telemetry;
// POD token lives on the native hook's stack, including through SEH cleanup.
// The matching slot retains its resource pins until this callback returns.
struct ForwardedRequest {std::uint64_t sequence{};unsigned slot{};tb::Packet packet;};
thread_local bool native_forwarding{};
ResolveTelemetry resolve_telemetry;
ShaderRejection shader_rejection;
NativeShaderCache shader_cache;
#ifdef MHW_CONTINUOUS_SCALED
NativeShaderCache preparation_shader_cache;
#endif
#ifdef MHW_CONTINUOUS_SCALED
constexpr bool include_menu_shader=true;
#else
constexpr bool include_menu_shader=false;
#endif
struct PreparedResolve {temporal_present::Bindings bindings;HistoryDecision history;bool evaluate{},full_output{};};
thread_local PreparedResolve prepared_resolve;
thread_local bool injected{};
thread_local Taa producer,consumer;
thread_local Depth depth_producer,depth_consumer;
thread_local Projection projection;
thread_local unsigned producer_count{},depth_producer_count{};
thread_local std::uintptr_t producer_array{};
thread_local ID3D12GraphicsCommandList *current_list{};
thread_local std::uintptr_t current_recording{};
thread_local unsigned current_kind{},barrier_count{},dispatch_count{},copy_count{};
thread_local std::array<D3D12_RESOURCE_BARRIER,128> barriers;
std::atomic_flag projection_busy=ATOMIC_FLAG_INIT;
List *find(ID3D12GraphicsCommandList *l){if(!l)return nullptr;for(unsigned i=0;i<list_count;++i)if(lists[i].value.Get()==l)return &lists[i];return nullptr;}
unsigned live_lists(){unsigned n{};for(unsigned i=0;i<list_count;++i)n+=lists[i].value!=nullptr;return n;}
bool child(ID3D12DeviceChild *p){if(!p)return false;std::uintptr_t target{};if(!pb::system_method(reinterpret_cast<std::uintptr_t>(p),7,target))return false;
    ComPtr<ID3D12Device> device;return SUCCEEDED(p->GetDevice(IID_PPV_ARGS(&device)))&&reinterpret_cast<std::uintptr_t>(device.Get())==cfg.device;}
bool pin(std::uintptr_t address,unsigned format,ComPtr<ID3D12Resource> &owned,bool uav=false){
    auto *r=reinterpret_cast<ID3D12Resource *>(address);if(!child(r))return false;
    const auto d=r->GetDesc();if(d.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||d.Width!=runtime.width||d.Height!=runtime.height||d.MipLevels!=1||d.DepthOrArraySize!=1||d.SampleDesc.Count!=1||d.SampleDesc.Quality||static_cast<unsigned>(d.Format)!=format||(uav&&!(d.Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)))return false;
    owned=r;return true;
}
void transition(ID3D12GraphicsCommandList *list,ID3D12Resource *r,D3D12_RESOURCE_STATES a,D3D12_RESOURCE_STATES b){D3D12_RESOURCE_BARRIER v{};v.Transition={r,UINT_MAX,a,b};reinterpret_cast<BarrierFn>(originals[8])(list,1,&v);}
void copy_input(ID3D12GraphicsCommandList *list,ID3D12Resource *source,unsigned role,D3D12_RESOURCE_STATES state){
    transition(list,source,state,D3D12_RESOURCE_STATE_COPY_SOURCE);reinterpret_cast<CopyFn>(originals[10])(list,raw[role].Get(),source);transition(list,source,D3D12_RESOURCE_STATE_COPY_SOURCE,state);
}
#ifdef MHW_CONTINUOUS_SCALED
// An odd scene's effects sample up to scene x scale (1706.5 of 3413 columns),
// half a column past the 1706 the game renders. That column keeps what the
// full-size frames before the reduced scale drew there, and the game's effects
// spread it along the right edge (title-screen colours, game-22/game-24).
// After copying a native input, replicate its last rendered column (and row,
// for a fractional height) into the one just outside, from the private copy.
bool edge_column{},edge_row{};std::uint64_t edge_fills{};
void fill_edge(ID3D12GraphicsCommandList *list,ID3D12Resource *native,unsigned role,D3D12_RESOURCE_STATES state){
    if(!edge_column&&!edge_row)return;
    auto *copy=raw[role].Get();const unsigned rw=render_width(),rh=render_height();
    transition(list,copy,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);transition(list,native,state,D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION d{},s{};d.pResource=native;s.pResource=copy;d.Type=s.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    bool copied=true;
    if(edge_column){const D3D12_BOX b{rw-1,0,0,rw,rh,1};copied=sr::copy_region(list,&d,rw,0,0,&s,&b)&&copied;}
    if(edge_row){const D3D12_BOX b{0,rh-1,0,rw,rh,1};copied=sr::copy_region(list,&d,0,rh,0,&s,&b)&&copied;}
    transition(list,native,D3D12_RESOURCE_STATE_COPY_DEST,state);transition(list,copy,D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COPY_DEST);
    if(copied)++edge_fills;
}
#endif
bool state_of(std::uintptr_t resource,D3D12_RESOURCE_STATES expected){bool known=false;unsigned state{};
    for(unsigned i=0;i<barrier_count;++i){const auto &b=barriers[i];
        if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING&&(!b.Aliasing.pResourceBefore||!b.Aliasing.pResourceAfter||reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceBefore)==resource||reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceAfter)==resource))return false;
        if(b.Type!=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION||reinterpret_cast<std::uintptr_t>(b.Transition.pResource)!=resource)continue;
        if(b.Flags||b.Transition.Subresource!=UINT_MAX||(known&&state!=unsigned(b.Transition.StateBefore)))return false;known=true;state=b.Transition.StateAfter;
    }return known&&state==unsigned(expected);
}
void trace_states(ID3D12GraphicsCommandList *list,bool temporal){
    const auto *l=find(list);state_trace.capture(reader,consumer.source,consumer.sequence,list,l?l->generation:0,
        consumer.packet.object,GetTickCount64(),temporal,std::span(barriers).first(barrier_count));
}
bool provider(pb::Provider &p) noexcept {
  unsigned char use{};
  p.valid=pb::read(cfg.renderer+0xa5,use) && use<=1 && pb::read(cfg.renderer+0xb4,p.sizing) && !(p.sizing&1) &&
    pb::read(cfg.renderer+0x78,p.backend) && p.backend==cfg.backend && pb::read(p.backend+0x18,p.device) && p.device==cfg.device &&
    pb::read(p.backend+0x42700,p.wrapper) && !p.wrapper;
  p.use=use;return p.valid;
}
bool anchor(pb::Source &s,const NativeFrame &f) noexcept {
  auto &a=s.anchor;unsigned char active{},display{},async{};unsigned stamp{},flags{};std::uint64_t zero{};
  if(!pb::read(cfg.image+0x51c4480,a.renderer) || a.renderer!=cfg.renderer || a.renderer>UINTPTR_MAX-0x1f570 ||
     !pb::read(cfg.image+0x51c4368,a.table) || !a.table || a.table>UINTPTR_MAX-0xe740 ||
     !pb::read(cfg.image+0x51c4400,s.camera) || !s.camera || s.camera>UINTPTR_MAX-0x100 ||
     f.call.gpr[1]!=s.camera+0x50 || f.call.gpr[2]!=a.renderer+0x710)return false;
  a.context=f.call.gpr[2];
  if(!pb::read(s.camera,s.camera_type) || (s.camera_type!=cfg.image+0x350d188 && s.camera_type!=cfg.image+0x33f82a8) ||
     !pb::read(s.camera+0x70,active) || !active || !pb::read(s.camera+0x75,display) || display ||
     !pb::read(s.camera+0x38,zero) || zero || !pb::read(s.camera+0x40,stamp) ||
     !pb::read(a.renderer+0x1f210,a.frame) || !pb::read(a.renderer+0x1f208,a.epoch) || stamp!=static_cast<unsigned>(a.epoch) ||
     !pb::read(a.renderer+0x1f450,a.view) || !a.view || a.view>UINTPTR_MAX-0x60 ||
     !pb::read(a.view+0x58,flags) || (flags&0x840000) ||
     !pb::read(a.renderer+0x1f200,a.bank) || a.bank>1 || !pb::read(a.renderer+0x93,async) || async>1)return false;
  a.async_mode=async;return true;
}
bool projection_before(const NativeFrame &f){
    if(!accepting.load()||reconstruction.failed()||f.call.caller!=cfg.image+0x228e834)return false;
#ifdef MHW_CONTINUOUS_SCALED
    if(!render_resolution::active_ready())return false;
#endif
    if(projection_busy.test_and_set()){fail(2);return false;}
    projection={};pb::Provider provider_state;
    auto &s=projection.source;
    if(!anchor(s,f)||!provider(provider_state)||!pb::read(s.anchor.table+0xe6a0+8*(s.anchor.frame%20),projection.candidate)||!pb::jitter_finite(projection.candidate)){
        projection_busy.clear();return false;}
    if(!reconstruction.jitter()){projection.candidate=0;++zero_jitter_projections;}
    if(!pb::jitter_exchange(s.camera+0x38,0,projection.candidate)){fail(3);projection_busy.clear();return false;}
    ++jitter_count;return true;
}
void projection_after(bool admitted,bool returned,NativeFrame &f){
    if(!admitted)return;
    auto &s=projection.source;
    // Restore regardless of stop, failed validation, or an exception in native code.
    s.projection=bridge_probe::capture_projection_snapshot(cfg.image,s.anchor,bridge_probe::read_bounded_scalar,nullptr);
    const auto &p=s.projection;
    const bool restored=pb::jitter_exchange(s.camera+0x38,projection.candidate,0);
    if(restored)++restorations;else fail(4);
    const auto bits=std::uint64_t(p.jitter_bits[0])|(std::uint64_t(p.jitter_bits[1])<<32);
    bool valid=returned&&(f.call.gpr[0]&255)&&restored&&p.valid&&p.matrix_relation&&p.manager==s.camera&&p.vtable==s.camera_type&&bits==projection.candidate&&
        p.tagged_constants==p.constants&&p.words&&p.words<=pb::max_constants/4;
    if(valid){s.bytes=p.words*4;std::array<std::byte,pb::max_constants> again{};pb::Provider after;
        valid=pb::copy_memory(p.constants,s.data.data(),s.bytes)&&pb::copy_memory(p.constants,again.data(),s.bytes)&&
            std::memcmp(s.data.data(),again.data(),s.bytes)==0&&pb::capture_matrices(cfg.image,s)&&provider(after);}
    if(valid){LARGE_INTEGER tick{},frequency{};QueryPerformanceCounter(&tick);QueryPerformanceFrequency(&frequency);
        auto &h=projection.history;h.frame=s.anchor.frame;h.epoch=s.anchor.epoch;h.camera=s.camera;h.view=s.anchor.view;
        h.microseconds=static_cast<std::uint64_t>((tick.QuadPart/frequency.QuadPart)*1000000+(tick.QuadPart%frequency.QuadPart)*1000000/frequency.QuadPart);
        h.viewport=p.viewport_bits;h.projection=p.unjittered_bits;for(unsigned i=0;i<16;++i)h.view_matrix[i]=std::bit_cast<float>(s.matrices.bits[1][i]);
        projection.valid=true;Lock lock;projections[h.frame%projections.size()]=projection;
    }else fail(5);
    projection_busy.clear();
}
bool taa_before(const NativeFrame &f){
    if(!accepting.load()||f.call.caller!=cfg.image+tb::caller_rva)return false;
    publishing.fetch_add(1);producer={};auto &s=producer.source;
    bool ok=!f.call.stack[0]&&!static_cast<unsigned char>(f.call.stack[1])&&tb::snapshot(reader,cfg,f.layer,f.call.gpr[1],s)&&
        tb::scene_depth(reader,s,producer.depth)&&tb::shader_recipe(reader,s,producer.shader)&&
        reader.get(s.context+0x2b80,producer_count)&&producer_count<65535&&reader.get(s.context+0x2b78,producer_array)&&producer_array;
    if(ok){Lock lock;producer.projection=projections[s.frame%projections.size()];const auto &a=producer.projection.source.anchor;
        ok=producer.projection.valid&&a.frame==s.frame&&a.epoch==s.epoch&&a.view==s.view&&a.context==s.context&&a.bank==s.bank&&a.async_mode==s.async;}
    #ifdef MHW_CONTINUOUS_SCALED
    render_resolution::InputStamp scale;
    if(ok)ok=render_resolution::input_stamp(s.view,scale)&&scale.frame==s.frame&&scale.render_width==render_width()&&scale.render_height==render_height();
    if(ok){Lock lock;for(const auto &d:depths)if(d.live&&d.stamp.frame==s.frame&&d.stamp.epoch==s.epoch&&d.stamp.view==s.view&&d.stamp.bank==s.bank&&d.stamp.async==s.async&&d.destination==producer.depth.texture&&d.sequence>producer.depth_sequence){producer.depth_sequence=d.sequence;producer.depth_object=d.object;producer.depth_packet=d.packet;}}
    dlss_sdk::PixelJitter mapped;const auto &projection_bits=producer.projection.source.projection;
    if(ok)ok=dlss_sdk::projection_jitter(scaled_geometry,{std::bit_cast<float>(projection_bits.jitter_bits[0]),std::bit_cast<float>(projection_bits.jitter_bits[1])},
        std::bit_cast<float>(projection_bits.viewport_bits[0]),std::bit_cast<float>(projection_bits.viewport_bits[1]),mapped);
#endif
    if(ok)ok=s.rectangle==std::array<unsigned,4>{0,0,render_width(),render_height()}&&
        pin(s.inputs[0].resource,26,producer.resources[0])&&pin(s.inputs[5].resource,42,producer.resources[1])&&
        pin(s.inputs[4].resource,26,producer.resources[2],true)&&pin(producer.depth.texture.resource,41,producer.resources[3]);
    if(!ok){producer.resources={};++skips;publishing.fetch_sub(1);return false;}
    producer.projection.history.depth=producer.depth.texture.resource;return true;
}
RequestIdentity identity(const Taa &t){const auto &s=t.source;
    return {s.frame,s.epoch,t.sequence,t.projection.history.microseconds,t.packet.object,t.packet.state,s.view,s.context,s.layer,
        t.depth.texture.resource,s.bank,s.async,t.bypasses,t.last_bypass_tick,t.last_bypass_caller,t.last_bypass_publishers};}
// Called under mutex after the original producer and all existing validations.
// Preserve fail(6), the original slot and publication sequence on collision.
void publish_taa(){
    auto &m=request_telemetry;++m.publication_attempts;
    const auto index=producer.source.frame%requests.size();auto &slot=requests[index];
    if(slot.live){
        if(!m.collisions){auto &c=m.first_collision;c.tick=GetTickCount64();c.publication_attempt=m.publication_attempts;
            c.slot=static_cast<unsigned>(index);for(const auto &t:requests)if(t.live)++c.live;
            c.publishers=publishing.load();c.pending_callbacks=pending.load();c.old_request=identity(slot);c.new_request=identity(producer);
            c.lease_list=reinterpret_cast<std::uintptr_t>(lease.list);c.lease_generation=lease.generation;c.lease_frame=lease.frame;
            c.lease_depth=lease.depth;c.lease_evaluated=lease.evaluated;}
        ++m.collisions;fail(6);
    }else {producer.sequence=++sequence;producer.live=true;slot=producer;++m.published;}
}
void taa_after(bool admitted,bool returned){
    if(!admitted)return;
    tb::Snapshot again;tb::SceneDepth depth;tb::ShaderRecipe shader;unsigned count{};std::uintptr_t array{},object{};
    const auto &s=producer.source;
    const bool ok=returned&&tb::snapshot(reader,cfg,s.layer,s.context,again)&&s==again&&tb::scene_depth(reader,s,depth)&&depth==producer.depth&&
        tb::shader_recipe(reader,s,shader)&&shader==producer.shader&&reader.get(s.context+0x2b80,count)&&count==producer_count+1&&
        reader.get(s.context+0x2b78,array)&&array&&reader.get(array+16ULL*(count-1)+8,object)&&tb::packet(reader,object,producer.packet)&&
        tb::packet_matches(s,producer.packet,(render_width()+15)/16,(render_height()+15)/16,1);
    if(ok){Lock lock;publish_taa();
#ifdef MHW_CONTINUOUS_SCALED
        Depth selected;selected.stamp.frame=s.frame;selected.stamp.epoch=s.epoch;selected.sequence=producer.depth_sequence;
        note_depth(2,producer.depth_sequence?0U:1U,selected,{producer.sequence,producer.packet.object,s.context,s.view,producer.depth.texture.resource});
        // A prepared full-output frame must have a qualified color resolve
        // before downstream CPU workers are allowed to commit their geometry.
        std::array<std::byte,native_taa_shader_bytes> native_bytes{};
        const auto variant=native_shader_index(producer.shader.technique,producer.shader.size,include_menu_shader);
        const bool color_contract=single_native_output(s,producer.shader,include_menu_shader)&&
            pb::copy_memory(producer.shader.bytecode,native_bytes.data(),producer.shader.size)&&
            preparation_shader_cache.matches(variant,std::span<const std::byte>(native_bytes).first(producer.shader.size),
                [](std::span<const std::byte> data){return pb::bytes_sha256(data.data(),static_cast<unsigned>(data.size()));});
        if(accepting.load()&&color_contract)sr::publish(producer.source,producer.packet);
#endif
    }
    else fail(7);producer.resources={};publishing.fetch_sub(1);
}
#ifdef MHW_CONTINUOUS_SCALED
// Render scale 1.0 (DLAA): the render extent is the scene. The depth writer
// then takes the game's plain path (a native CopyResource, not the nested
// draw the scaled admission qualifies), so depth is leased at that copy, as in
// the unscaled build (dlaa-identity-path.md).
bool identity_extent(){return pass&&render_width()==runtime.width&&render_height()==runtime.height;}
#endif
bool depth_before(const NativeFrame &f){
    if(!accepting.load()||f.call.gpr[1]!=cfg.renderer+0x710||static_cast<unsigned>(f.call.stack[0])!=8)return false;
    depth_producer={};auto &d=depth_producer;std::uintptr_t destination{},attachment{},source{};
    unsigned reason{};const auto gate=[&](unsigned code,bool ok){if(!ok)reason=code;return ok;};
    const bool ok=gate(1,dr::stamp(reader,cfg,d.stamp))&&gate(2,reader.get(d.stamp.container+0xa0,destination)&&destination==f.call.gpr[4])&&
        gate(3,reader.get(d.stamp.context+0x50,attachment)&&attachment&&reader.get(attachment+0x58,source))&&
        gate(4,tb::read_owner(reader,source,d.source))&&gate(5,tb::read_owner(reader,destination,d.destination))&&
        gate(6,reader.get(d.stamp.context+0x2b80,depth_producer_count)&&depth_producer_count<65535);
    #ifdef MHW_CONTINUOUS_SCALED
    unsigned flags{};const bool flags_read=reader.get(cfg.renderer+0x710+0x1ea74,flags);
    note_depth(0,reason,d,{f.call.gpr[1],f.call.gpr[4],destination,attachment,source,d.stamp.container,flags_read,flags,depth_producer_count});
    #endif
    return ok;
}
void depth_after(bool admitted,bool returned){
    if(!admitted)return;auto &d=depth_producer;unsigned count{};std::uintptr_t array{};dr::Stamp again;
    if(!returned||!dr::stamp(reader,cfg,again)||again!=d.stamp||!reader.get(d.stamp.context+0x2b80,count)||count<=depth_producer_count||count-depth_producer_count>64||
        !reader.get(d.stamp.context+0x2b78,array)||!array||!reader.get(array+16ULL*(count-1)+8,d.object)||!dr::packet(reader,d.object,d.packet)||
        d.packet.source!=d.source.object||d.packet.destination!=d.destination.object){fail(8);return;}
    Lock lock;d.sequence=++sequence;d.live=true;depths[d.sequence%depths.size()]=d;
    DEPTH_NOTE(1,0,d,{d.object,d.packet.nested,d.source.resource,d.destination.resource,d.packet.rectangle[0],d.packet.rectangle[1],d.packet.rectangle[2],d.packet.rectangle[3],d.packet.rectangle[4],d.packet.rectangle[5]});
}
void begin_forwarded(const ConsumerSample &sample,ForwardedRequest &token){
    auto &m=forwarded_telemetry;++m.candidates;
    if(!sample.matches){++m.unmatched;return;}
    if(sample.matches!=1){++m.ambiguous;return;}
    const auto index=sample.newest.frame%requests.size();auto &t=requests[index];tb::Packet a,b;
    if(!t.live||t.forwarding||!t.sequence||t.sequence!=sample.newest.sequence||
        !tb::packet(reader,sample.packet,a)||!tb::packet(reader,sample.packet,b)||a!=b||a!=t.packet||
        !tb::packet_matches(t.source,a,(render_width()+15)/16,(render_height()+15)/16,1)){
        ++m.invalid;return;
    }
    t.forwarding=true;token={t.sequence,static_cast<unsigned>(index),a};native_forwarding=true;
    ++m.started;++m.pending;
}
void finish_forwarded(ForwardedRequest &token,bool returned){
    if(!token.sequence)return;
    Lock lock;auto &m=forwarded_telemetry;auto &t=requests[token.slot];tb::Packet a,b;
    const bool same=t.live&&t.forwarding&&t.sequence==token.sequence&&t.packet==token.packet;
    const bool stable=returned&&same&&tb::packet(reader,token.packet.object,a)&&
        tb::packet(reader,token.packet.object,b)&&a==b&&a==token.packet;
    --m.pending;native_forwarding=false;token.sequence=0;
    if(!stable){++m.failed;if(same)t.forwarding=false;fail(returned?32:10);return;}
    const auto receipt=identity(t);if(!m.retired)m.first_retired=receipt;m.last_retired=receipt;++m.retired;
    t.live=false;t.forwarding=false;t.resources={};
}
bool consumer_before(const NativeFrame &f,bool depth,ForwardedRequest *forwarded=nullptr){
    const bool prepared_output=
#ifdef MHW_CONTINUOUS_SCALED
        !depth&&sr::owns_packet(f.call.gpr[3]);
#else
        false;
#endif
    if(!accepting.load()&&!prepared_output)return false;
    if(current_kind||native_forwarding){Lock lock;auto &m=request_telemetry.consumers[depth?1:0];++m.calls;++m.reentrant;fail(9);return false;}
    const auto publishers=publishing.load();
    if(publishers&&!prepared_output){Lock lock;auto &m=request_telemetry.consumers[depth?1:0];++m.calls;
        ConsumerSample sample;sample.ordinal=m.calls;sample.tick=GetTickCount64();sample.packet=f.call.gpr[3];sample.caller=f.call.caller;sample.publishers=publishers;
        if(depth){for(const auto &d:depths)if(d.live){++sample.live;if(d.object==sample.packet){++sample.matches;
            if(d.sequence>sample.newest.sequence){sample.newest.frame=d.stamp.frame;sample.newest.epoch=d.stamp.epoch;sample.newest.sequence=d.sequence;sample.newest.packet=d.object;sample.newest.view=d.stamp.view;sample.newest.context=d.stamp.context;}}}}
        else for(auto &t:requests)if(t.live){++sample.live;if(t.packet.object==sample.packet){++sample.matches;
            ++t.bypasses;t.last_bypass_tick=sample.tick;t.last_bypass_caller=sample.caller;t.last_bypass_publishers=publishers;
            if(t.sequence>sample.newest.sequence)sample.newest=identity(t);}}
        if(!m.blocked)m.first_blocked=sample;m.last_blocked=sample;++m.blocked;
        if(depth&&sample.newest.frame){Depth d;d.stamp.frame=sample.newest.frame;d.stamp.epoch=sample.newest.epoch;d.sequence=sample.newest.sequence;
            DEPTH_NOTE(3,4,d,{sample.packet,sample.caller,publishers,sample.matches});}
        if(!depth&&forwarded)begin_forwarded(sample,*forwarded);
        return false;}
    bool found=false;
    {Lock lock;
        auto &m=request_telemetry.consumers[depth?1:0];++m.calls;ConsumerSample sample;
        sample.ordinal=m.calls;sample.packet=f.call.gpr[3];sample.caller=f.call.caller;
        if(depth){Depth *latest{};for(auto &d:depths)if(d.live){++sample.live;if(d.object==f.call.gpr[3]){++sample.matches;if(!latest||d.sequence>latest->sequence)latest=&d;}}
            if(latest){sample.newest.frame=latest->stamp.frame;sample.newest.epoch=latest->stamp.epoch;sample.newest.sequence=latest->sequence;sample.newest.packet=latest->object;sample.newest.view=latest->stamp.view;sample.newest.context=latest->stamp.context;}
            if(latest){depth_consumer=*latest;latest->live=false;dr::CopyPacket p;found=dr::packet(reader,f.call.gpr[3],p)&&p==depth_consumer.packet;
                if(!found)DEPTH_NOTE(3,1,depth_consumer,{f.call.gpr[3],f.call.caller});}}
        else {Taa *latest{};for(auto &t:requests)if(t.live){++sample.live;if(t.packet.object==f.call.gpr[3]){++sample.matches;if(!latest||t.sequence>latest->sequence)latest=&t;}}
            if(latest)sample.newest=identity(*latest);
            if(latest&&latest->forwarding){++m.reentrant;fail(9);return false;}
            if(latest){consumer=*latest;latest->resources={};latest->live=false;tb::Packet p;found=tb::packet(reader,f.call.gpr[3],p)&&p==consumer.packet;}}
        if(sample.matches){++m.matched;if(!found)++m.changed;
            if(sample.matches>1){sample.tick=GetTickCount64();if(!m.multiple)m.first_multiple=sample;m.last_multiple=sample;++m.multiple;}}
        else {sample.tick=GetTickCount64();if(!m.unmatched)m.first_unmatched=sample;m.last_unmatched=sample;++m.unmatched;}
    }
    if(!found)return false;
    std::uintptr_t backend{},command{};
    if(!f.call.gpr[1]||!f.call.gpr[2]||!reader.get(f.call.gpr[2]+8,backend)||backend!=cfg.backend||
        !reader.get(f.call.gpr[1]+8,current_recording)||!current_recording||!reader.get(current_recording+0x20,command)||!command){
            if(depth)DEPTH_NOTE(3,2,depth_consumer,{f.call.gpr[1],f.call.gpr[2],backend,current_recording,command});return false;}
    current_list=reinterpret_cast<ID3D12GraphicsCommandList *>(command);
    {Lock lock;if(!find(current_list)){if(depth)DEPTH_NOTE(3,3,depth_consumer,{command,current_recording});return false;}}
    if(depth)DEPTH_NOTE(3,0,depth_consumer,{command,current_recording});
    current_kind=depth?2:1;barrier_count=dispatch_count=copy_count=0;return true;
}
void consumer_after(bool admitted,bool returned){if(admitted){if(!returned)fail(10);current_kind=0;current_list=nullptr;}consumer.resources={};}
void WINAPI barrier_hook(ID3D12GraphicsCommandList *list,UINT n,const D3D12_RESOURCE_BARRIER *data){
    perf::hook_timing::Scope timing(perf::hook_timing::barrier);Guard guard;
    {perf::hook_timing::ForwardScope forward(timing.sample);reinterpret_cast<BarrierFn>(originals[8])(list,n,data);}
    #ifdef MHW_CONTINUOUS_SCALED
    if(!injected)sr::barriers(list,n,data);
#endif
    if(injected||!current_kind||!accepting.load())return;
    if(list!=current_list||n>barriers.size()-barrier_count||!pb::copy_memory(reinterpret_cast<std::uintptr_t>(data),barriers.data()+barrier_count,n*sizeof(*data))){fail(11);return;}
    barrier_count+=n;
}

#ifdef MHW_CONTINUOUS_SCALED
using DrawFn=void(WINAPI *)(ID3D12GraphicsCommandList *,UINT,UINT,UINT,UINT);
using IndexedFn=void(WINAPI *)(ID3D12GraphicsCommandList *,UINT,UINT,UINT,INT,UINT);
void WINAPI indexed_hook(ID3D12GraphicsCommandList *list,UINT n,UINT instances,UINT first,INT base,UINT instance){
 perf::hook_timing::Scope timing(perf::hook_timing::indexed);Guard guard;
 if(!sr::indexed_draw(list,n,instances,first,base,instance,reinterpret_cast<std::uintptr_t>(_ReturnAddress()))){
  perf::hook_timing::ForwardScope forward(timing.sample);reinterpret_cast<IndexedFn>(originals[12])(list,n,instances,first,base,instance);}
}
struct DepthDraw {
 std::array<std::uintptr_t,4> bindings{};std::array<unsigned,2> arguments{};std::array<unsigned,4> rectangle{};
 std::uintptr_t packet{},pipeline{};unsigned rejection{};
 bool operator==(const DepthDraw &)const=default;
};
bool depth_draw_snapshot(DepthDraw &out){
 const auto &d=depth_consumer;const auto nested=d.packet.nested;
 const auto gate=[&](unsigned reason,bool ok){if(!ok)out.rejection=reason;return ok;};
 return gate(1,nested&&reader.get(nested+0x10,out.bindings)&&reader.get(nested+0x40,out.arguments))&&
    gate(2,reader.get(current_recording+0x158,out.packet)&&out.packet==nested)&&
    gate(3,reader.get(current_recording+0x108,out.pipeline)&&out.pipeline)&&
    gate(4,reader.get(out.bindings[0]+0x4c,out.rectangle)&&out.rectangle==std::array<unsigned,4>{0,0,render_width(),render_height()})&&
    gate(5,d.packet.rectangle==std::array<unsigned,6>{0,0,render_width(),render_height(),0,0})&&
    gate(6,out.arguments[0]>0&&out.arguments[0]<=6);
}
void WINAPI draw_hook(ID3D12GraphicsCommandList *list,UINT n,UINT instances,UINT first,UINT instance){
 perf::hook_timing::Scope timing(perf::hook_timing::draw);Guard guard;const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());DepthDraw before;
 if(!injected&&current_kind==2&&accepting.load()){
    unsigned reason{};
    if(list!=current_list||caller!=cfg.image+0x259d72e)reason=1;
    else if(++copy_count!=1)reason=2;
    else if(!depth_draw_snapshot(before))reason=3;
    else if(before.arguments!=std::array<unsigned,2>{n,first}||instances!=1||instance)reason=4;
    else if(!dr::draw_states(std::span(barriers).first(barrier_count),depth_consumer.source.resource,depth_consumer.destination.resource))reason=5;
    note_depth(4,reason,depth_consumer,{reinterpret_cast<std::uintptr_t>(list),caller,copy_count,n,instances,first,instance,before.rejection,
        before.packet,before.pipeline,before.rectangle[0],before.rectangle[1],before.rectangle[2],before.rectangle[3],before.arguments[0],before.arguments[1]});
    if(reason==5){ScaledDepthTrace::Values values{};values[0]=barrier_count;
        for(const auto &b:std::span(barriers).first(barrier_count)){
            if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING){++values[11];continue;}
            if(b.Type!=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)continue;
            const auto resource=reinterpret_cast<std::uintptr_t>(b.Transition.pResource);
            const unsigned at=resource==depth_consumer.source.resource?1U:resource==depth_consumer.destination.resource?6U:0U;
            if(at){++values[at];values[at+1]=b.Flags;values[at+2]=b.Transition.Subresource;values[at+3]=b.Transition.StateBefore;values[at+4]=b.Transition.StateAfter;}}
        note_depth(6,0,depth_consumer,values);}
 }
 if(!sr::draw(list,n,instances,first,instance,caller)){
    perf::hook_timing::ForwardScope forward(timing.sample);reinterpret_cast<DrawFn>(originals[11])(list,n,instances,first,instance);}
}
#endif
void WINAPI copy_hook(ID3D12GraphicsCommandList *list,ID3D12Resource *destination,ID3D12Resource *source){
    perf::hook_timing::Scope timing(perf::hook_timing::copy);Guard guard;const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    {perf::hook_timing::ForwardScope forward(timing.sample);reinterpret_cast<CopyFn>(originals[10])(list,destination,source);}
#ifdef MHW_CONTINUOUS_SCALED
    // The scaled build copies depth at the TAA dispatch, on its own recording,
    // at every render scale (DepthConsumerAdmission; render scale 1.0 is its
    // whole-copy mode since v0.19.3). A lease at the native copy needed that
    // copy on the TAA's command list, which the game does not keep at 1.0
    // (dlaa-identity-path.md, game-70/74/75).
    (void)caller;
#else
    if(injected||current_kind!=2||!accepting.load())return;
    const auto &d=depth_consumer;
    if(++copy_count!=1||caller!=cfg.image+0x259e100||list!=current_list||reinterpret_cast<std::uintptr_t>(destination)!=d.destination.resource||reinterpret_cast<std::uintptr_t>(source)!=d.source.resource||
        !dr::copy_states(std::span(barriers).first(barrier_count),d.source.resource,d.destination.resource)
        ){
        DEPTH_NOTE(5,8,d,{2,copy_count,caller,reinterpret_cast<std::uintptr_t>(list),reinterpret_cast<std::uintptr_t>(source),reinterpret_cast<std::uintptr_t>(destination)});return;}
    Lock lock;if(!accepting.load())return;if(lease.list){DEPTH_NOTE(5,2,d,{2,reinterpret_cast<std::uintptr_t>(lease.list),lease.generation,lease.frame});++skips;return;}
    auto *l=find(list);if(!l||l->closed){DEPTH_NOTE(5,3,d,{2,reinterpret_cast<std::uintptr_t>(list)});return;}
    const Taa *selected{};for(const auto &t:requests)if(depth_copy_belongs(t,d))selected=&t;
    if(!selected){DEPTH_NOTE(5,4,d,{2,reinterpret_cast<std::uintptr_t>(list),l->generation});return;}
    recording_pins.collect(fence->GetCompletedValue());
    if(!recording_pins.retain(list,l->generation,selected->resources)){fail(1);return;}
    lease={list,l->generation,d.stamp.frame,true,false};l->injected_generation=l->generation;
    copy_input(list,destination,1,D3D12_RESOURCE_STATE_COPY_DEST);++depth_count;
    DEPTH_NOTE(5,0,d,{2,reinterpret_cast<std::uintptr_t>(list),l->generation,selected->sequence});
#endif
}
bool dispatch_before(ID3D12GraphicsCommandList *list,UINT x,UINT y,UINT z,std::uintptr_t caller){
    const bool prepared_output=
#ifdef MHW_CONTINUOUS_SCALED
        current_kind==1&&sr::full_contract(consumer.source);
#else
        false;
#endif
    if(injected||current_kind!=1||(!accepting.load()&&!prepared_output))return false;
    const auto skip=[](unsigned why){last_skip=why;++skips;Lock lock;++dispatch_reasons[why];return false;};
    if(++dispatch_count!=1||list!=current_list||caller!=cfg.image+0x259cfca)return skip(1);
    const auto &s=consumer.source;tb::Packet packet;tb::ShaderRecipe shader;std::uintptr_t current_packet{},root{},cached{},pipeline{};
    for(unsigned role:{0U,4U}){const auto &owner=s.inputs[role];tb::Owner actual;if(!tb::read_owner(reader,owner.object,actual))return skip(2);actual.id=owner.id;if(actual!=owner)return skip(2);}
    if(!reader.get(current_recording+0x158,current_packet)||current_packet!=consumer.packet.object||!tb::packet(reader,current_packet,packet)||packet!=consumer.packet||!tb::packet_matches(s,packet,x,y,z)||
        !tb::shader_recipe(reader,s,shader)||shader!=consumer.shader||!reader.get(cfg.backend+0x2a0,root)||!reader.get(current_recording+0x118,cached)||root!=cached||!reader.get(current_recording+0x108,pipeline))return skip(4);
    temporal_present::Bindings state;if(!obs::current(list,state)||state.compute.signature!=root||state.pipeline!=pipeline)return skip(5);
    if(!state_of(s.inputs[0].resource,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)||!state_of(s.inputs[4].resource,D3D12_RESOURCE_STATE_UNORDERED_ACCESS)){
        Lock lock;const auto *record=find(list);
        const bool admitted=(accepting.load()||prepared_output)&&record&&!record->closed&&record->generation==state.generation&&
            state_admission.attempt(reader,s,consumer.sequence,list,record->generation,consumer.packet.object,GetTickCount64(),std::span(barriers).first(barrier_count));
        if(!admitted){trace_states(list,false);last_skip=6;++skips;++dispatch_reasons[6];return false;}
        tb::Packet again;std::uintptr_t active_packet{};
        if(!tb::packet(reader,consumer.packet.object,again)||again!=consumer.packet||!reader.get(current_recording+0x158,active_packet)||active_packet!=again.object){last_skip=4;++skips;++dispatch_reasons[4];return false;}
    }
    std::array<std::byte,native_taa_shader_bytes> bytecode{};
    const unsigned variant=native_shader_index(shader.technique,shader.size,include_menu_shader);
    const bool shape=single_native_output(s,shader,include_menu_shader);
    const bool copied=shape&&pb::copy_memory(shader.bytecode,bytecode.data(),shader.size);
    const auto tested=std::span<const std::byte>(bytecode).first(copied?shader.size:0U);
    Lock lock;const auto *l=find(list);
    const auto locked_skip=[](unsigned why){last_skip=why;++skips;++dispatch_reasons[why];return false;};
    if(!accepting.load()&&!prepared_output)return false;
    ++resolve_telemetry.shader_checks;
    const bool previously_verified=variant<unknown_native_shader&&shader_cache.verified[variant];
    const bool matches=copied&&shader_cache.matches(variant,tested,[](std::span<const std::byte> data){return pb::bytes_sha256(data.data(),static_cast<unsigned>(data.size()));});
    if(!matches){
        if(!shader_rejection.present){
            shader_rejection.capture(s,shader,shape,copied,matches,previously_verified,tested,reader);
            shader_rejection.packet=consumer.packet;shader_rejection.sequence=consumer.sequence;
            shader_rejection.generation=state.generation;shader_rejection.list=reinterpret_cast<std::uintptr_t>(list);}
        ++resolve_telemetry.shader_rejected;fail(29);return locked_skip(9);}
    if(!l||l->closed||l->generation!=state.generation)return locked_skip(5);
#ifdef MHW_CONTINUOUS_SCALED
    obs::Predicate native_predicate;
    if(!obs::predicate_current(list,native_predicate)||native_predicate.buffer)return locked_skip(5);
#endif
    const auto unfiltered=[&](unsigned why,bool allow_full=true){
#ifdef MHW_CONTINUOUS_SCALED
        const bool owns_private=!lease.list||(lease.list==list&&lease.generation==l->generation&&lease.frame==s.frame&&!lease.evaluated);
        if(prepared_output&&allow_full&&owns_private){
            // The native post chain already owns a full-output contract. A
            // missing temporal input changes reconstruction quality, not the
            // dimensions consumed by recording workers that may run first.
            if(!lease.list){
                recording_pins.collect(fence->GetCompletedValue());
                if(!recording_pins.retain(list,l->generation,consumer.resources)){fail(1);return locked_skip(8);}
                lease={list,l->generation,s.frame,false,false};find(list)->injected_generation=l->generation;
                ++spatial_recordings;
            }
            if(!sr::ready(list,l->generation,s,current_recording)){fail(46);return locked_skip(11);}
            copy_input(list,reinterpret_cast<ID3D12Resource *>(s.inputs[0].resource),0,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            fill_edge(list,reinterpret_cast<ID3D12Resource *>(s.inputs[0].resource),0,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            last_skip=why;++skips;++dispatch_reasons[why];reconstruction.suspend();prepared_resolve={state,{},false,true};return true;
        }
#else
        (void)allow_full;
#endif
        const ResourcePins colors{consumer.resources[0],consumer.resources[2],consumer.resources[0],consumer.resources[2]};
        if(!temporal_present::can_copy_unfiltered(list,colors[0].Get(),colors[1].Get(),runtime.width,runtime.height,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)||
           !unfiltered_lifetime.retain(list,l->generation,colors)){fail(33);return locked_skip(11);}
        last_skip=why;++skips;++dispatch_reasons[why];reconstruction.suspend();prepared_resolve={state,{},false};return true;
    };
    if(reconstruction.failed()||!accepting.load())return unfiltered(0);
    for(unsigned role:{1U,2U,3U,5U,6U}){const auto &owner=s.inputs[role];tb::Owner actual;
        if(!tb::read_owner(reader,owner.object,actual))return unfiltered(2);actual.id=owner.id;if(actual!=owner)return unfiltered(2);}
    const DepthEligibilityTrace::Identity depth_at{GetTickCount64(),s.frame,s.epoch,consumer.sequence,consumer.packet.object,
        reinterpret_cast<std::uintptr_t>(list),l->generation,s.view,s.context,s.bank,s.async};
    DepthReadObservation depth_reads{reader};tb::SceneDepth depth;
    const bool depth_readable=tb::scene_depth(depth_reads.reader(),s,depth);
    const bool depth_equal=depth_readable&&depth==consumer.depth;
    depth_eligibility.scene(depth_at,consumer.depth,depth,depth_readable,depth_equal,depth_reads);
    if(!depth_destination.check(depth_at,consumer.depth,depth,depth_readable))return unfiltered(3);
    if(!state_of(s.inputs[5].resource,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)){trace_states(list,true);return unfiltered(6);}
    #ifdef MHW_CONTINUOUS_SCALED
    auto *depth_resource=consumer.resources[3].Get();
    const bool depth_description=depth_resource&&reinterpret_cast<std::uintptr_t>(depth_resource)==consumer.depth.texture.resource&&
        depth_copy_description(depth_resource->GetDesc(),runtime.width,runtime.height,true)&&
        depth_copy_description(raw[1]->GetDesc(),runtime.width,runtime.height,false);
    const auto depth_state=depth_consumer_admission.attempt(reader,s,consumer.depth.texture,consumer.depth_object,consumer.depth_packet,
        consumer.packet.object,std::span(barriers).first(barrier_count),consumer.sequence,reinterpret_cast<std::uintptr_t>(list),
        l->generation,GetTickCount64(),!lease.list&&consumer.depth_sequence!=0,depth_description,identity_extent());
    if(depth_state){
        // This recording owns all private inputs and their completion signal.
        // The old draw/copy callbacks no longer inject scaled depth work.
        recording_pins.collect(fence->GetCompletedValue());
        if(!recording_pins.retain(list,l->generation,consumer.resources)){fail(1);return locked_skip(8);}
        lease={list,l->generation,s.frame,true,false};find(list)->injected_generation=l->generation;
        if(!record_consumer_depth(list,depth_resource,raw[1].Get(),runtime.width,runtime.height,
            static_cast<D3D12_RESOURCE_STATES>(depth_state),reinterpret_cast<DepthBarrierFn>(originals[8]),reinterpret_cast<DepthCopyFn>(originals[10]))){
            fail(44);return locked_skip(11);}
        ++depth_count;depth_consumer_admission.copied();
        // GBuffer4's state was proven above (state_of, TAA's own barriers).
        sr::full_post_states(s,depth_state,reinterpret_cast<std::uintptr_t>(depth_resource),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,s.inputs[5].resource);
    }
    #endif
    const bool qualified_depth=lease.depth
#ifdef MHW_CONTINUOUS_SCALED
        &&depth_state!=0
#endif
        ;
    depth_eligibility.lease(depth_at,reinterpret_cast<std::uintptr_t>(lease.list),lease.generation,lease.frame,qualified_depth,lease.evaluated);
    #ifdef MHW_CONTINUOUS_SCALED
    depth_schedule_trace.observe(reader,s,consumer.depth,consumer.depth_object,consumer.depth_packet,consumer.depth_sequence,
        consumer.sequence,reinterpret_cast<std::uintptr_t>(list),l->generation,consumer.packet.object,GetTickCount64(),
        lease.list!=list||lease.generation!=l->generation||lease.frame!=s.frame||!qualified_depth||lease.evaluated,std::span(barriers).first(barrier_count));
    #endif
    #ifdef MHW_CONTINUOUS_SCALED
    // Render scale 1.0: record the native depth schedule (observation only).
    if(identity_extent())identity_depth_survey.observe(reader,s,consumer.depth.texture,consumer.depth_object,consumer.depth_packet,
        consumer.packet.object,consumer.sequence,consumer.depth_sequence,reinterpret_cast<std::uintptr_t>(list),l->generation,GetTickCount64(),
        lease.list!=list||lease.generation!=l->generation||lease.frame!=s.frame||!qualified_depth||lease.evaluated);
    #endif
    if(lease.list!=list||lease.generation!=l->generation||lease.frame!=s.frame||!qualified_depth||lease.evaluated){
        #ifdef MHW_CONTINUOUS_SCALED
        {DepthTraceLock trace_lock;scaled_depth_trace.missing({GetTickCount64(),s.frame,s.epoch,consumer.sequence,consumer.depth_sequence,
            consumer.packet.object,reinterpret_cast<std::uintptr_t>(list),l->generation},
            {reinterpret_cast<std::uintptr_t>(lease.list),lease.generation,lease.frame,qualified_depth,lease.evaluated});}
        #endif
        return unfiltered(7);}
    if(!recording_pins.matches(list,l->generation,consumer.resources))return unfiltered(8);
    const auto decision=history_decision(previous,consumer.projection.history,runtime_history_policy);
    // A skipped frame creates a real frame gap. Never resume with retained
    // history if a suspension did not yet create such a reset boundary.
    if(!decision.valid||(reconstruction.waiting()&&!decision.reset))return unfiltered(10);
    if(!temporal_present::can_copy_unfiltered(list,raw[0].Get(),reinterpret_cast<ID3D12Resource *>(s.inputs[4].resource),runtime.width,runtime.height,D3D12_RESOURCE_STATE_COPY_DEST)){
        fail(30);return locked_skip(11);}
    #ifdef MHW_CONTINUOUS_SCALED
    if(!sr::ready(list,l->generation,s,current_recording)){
        {DepthTraceLock trace_lock;++scaled_depth_trace.readiness_rejected;}return unfiltered(7,false);}
#endif
    prepared_resolve={state,decision,true,prepared_output};
    copy_input(list,reinterpret_cast<ID3D12Resource *>(s.inputs[0].resource),0,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    copy_input(list,reinterpret_cast<ID3D12Resource *>(s.inputs[5].resource),2,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
#ifdef MHW_CONTINUOUS_SCALED
    fill_edge(list,reinterpret_cast<ID3D12Resource *>(s.inputs[0].resource),0,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    fill_edge(list,reinterpret_cast<ID3D12Resource *>(s.inputs[5].resource),2,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
#endif
    return true;
}
ResolveResult dispatch_replace(ID3D12GraphicsCommandList *list){
    Lock lock;const auto &state=prepared_resolve.bindings;const auto &decision=prepared_resolve.history;
    if(!prepared_resolve.evaluate){
        if(!obs::matches_current(list,state)){fail(12);return {};}
#ifdef MHW_CONTINUOUS_SCALED
        if(prepared_resolve.full_output){
            auto result=scaled_temporal::record_resolve(*pass,*spatial,list,reinterpret_cast<ID3D12Resource *>(consumer.source.inputs[4].resource),
                state,{},true,0,false);
            sr::resolved(result.output_written(),false);
            if(!result.output_written()||!obs::matches_current(list,state)){fail(46);return {ResolveOutcome::unrecoverable};}
            lease.evaluated=true;return result;
        }
#endif
        const bool copied=temporal_present::copy_unfiltered(list,consumer.resources[0].Get(),consumer.resources[2].Get(),
            runtime.width,runtime.height,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        if(!copied||!obs::matches_current(list,state)){fail(34);return {ResolveOutcome::unrecoverable};}
        return {ResolveOutcome::unfiltered};
    }
    if(!obs::matches_current(list,state)||lease.list!=list||lease.generation!=state.generation){fail(12);return {};}
    const auto bits=consumer.projection.source.projection.jitter_bits;
    #ifdef MHW_CONTINUOUS_SCALED
    dlss_sdk::PixelJitter mapped;const auto viewport=consumer.projection.source.projection.viewport_bits;
    if(!dlss_sdk::projection_jitter(scaled_geometry,{std::bit_cast<float>(bits[0]),std::bit_cast<float>(bits[1])},
        std::bit_cast<float>(viewport[0]),std::bit_cast<float>(viewport[1]),mapped)){sr::resolved(false);fail(41);return {};}
    const dlss_sdk::HistoryFrame frame{consumer.source.frame,consumer.source.epoch,mapped.x,mapped.y};
#else
    const dlss_sdk::HistoryFrame frame{consumer.source.frame,consumer.source.epoch,std::bit_cast<float>(bits[0])*.5F,std::bit_cast<float>(bits[1])*-.5F};
#endif
    // Once claimed, even an explicit stop between preparation and replacement
    // must write the output. It must never resume the native TAA calculation.
    #ifdef MHW_CONTINUOUS_SCALED
    auto result=scaled_temporal::record_resolve(*pass,*spatial,list,reinterpret_cast<ID3D12Resource *>(consumer.source.inputs[4].resource),
        state,frame,decision.reset,decision.milliseconds,accepting.load()&&!reconstruction.failed());
    sr::resolved(result.output_written(),result.outcome==ResolveOutcome::reconstructed);
#else
    auto result=record_resolve(*pass,list,raw[0].Get(),reinterpret_cast<ID3D12Resource *>(consumer.source.inputs[4].resource),
        runtime.width,runtime.height,state,frame,decision.reset,decision.milliseconds,accepting.load()&&!reconstruction.failed());
#endif
    if(!obs::matches_current(list,state)){fail(31);result.outcome=ResolveOutcome::unrecoverable;return result;}
    if(result.output_written())lease.evaluated=true;
    if(result.outcome!=ResolveOutcome::reconstructed){
        if(result.outcome==ResolveOutcome::recovered){reconstruction.latch(14);activation::latch_failure();}
        else if(result.outcome!=ResolveOutcome::unfiltered)fail(30);
        return result;
    }
#ifdef MHW_FG_GAME
    // Stage this frame's converted depth/motion and camera; the replaced final
    // screen draw of the same frame records them with the HUD-less colour.
    {frame_generation::game::SlotCamera camera;const auto &h=consumer.projection.history;camera.view=h.view_matrix;
     for(unsigned i=0;i<16;++i)camera.projection[i]=std::bit_cast<float>(h.projection[i]);
     camera.jitter_x=frame.jitter_x;camera.jitter_y=frame.jitter_y;camera.reset=decision.reset;
     const auto images=pass->private_images();
     const bool outer=injected;injected=true;  // our staging copy is not native work
     frame_generation::game::stage(list,consumer.source.frame,consumer.source.epoch,images[1],images[2],camera);
     injected=outer;}
#endif
    history_telemetry.record(previous,consumer.projection.history,decision);
    history_depth=consumer.resources[3];previous=consumer.projection.history;lease.evaluated=true;++frames;last_skip=0;if(decision.reset)++reset_count;
    reconstruction.complete();
    return result;
}
#ifdef MHW_FG_GAME
// stream_route final-scene observer: our barriers, copies and dispatch must
// bypass this runtime's own hooks exactly like the DLSS replacement's work.
void fg_final_scene(ID3D12GraphicsCommandList *list,const temporal_present::Bindings &roots,ID3D12Resource *scene,
    std::uint64_t frame,std::uint64_t epoch,D3D12_RESOURCE_STATES scene_state) noexcept {
    const bool outer=injected;injected=true;
    frame_generation::game::record(list,roots,scene,frame,epoch,scene_state);
    injected=outer;
}
#endif
ResolveResult replace_scoped(ID3D12GraphicsCommandList *list){
    ResolveResult result;injected=true;__try{result=dispatch_replace(list);}__finally{injected=false;}return result;
}
void dispatch_call(ID3D12GraphicsCommandList *list,UINT x,UINT y,UINT z,std::uintptr_t caller){
#ifdef MHW_CONTINUOUS_SCALED
    if(!injected){injected=true;const bool replaced=sr::compute_dispatch(list,x,y,z,caller);injected=false;if(replaced)return;}
#endif
    const bool candidate=!injected&&current_kind==1&&list==current_list&&caller==cfg.image+0x259cfca;
    if(candidate){Lock lock;resolve_telemetry.begin();}
    const bool admitted=dispatch_before(list,x,y,z,caller);
    const auto result=dispatch_resolve(admitted,[&]{return replace_scoped(list);},
        [&]{reinterpret_cast<DispatchFn>(originals[9])(list,x,y,z);});
    if(candidate){Lock lock;resolve_telemetry.end(result);}
}
void WINAPI dispatch_hook(ID3D12GraphicsCommandList *list,UINT x,UINT y,UINT z){perf::hook_timing::Scope timing(perf::hook_timing::dispatch);Guard guard;dispatch_call(list,x,y,z,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));}
// The game creates and releases command lists while it runs (the
// nDraw::ExternalRegion proxies each own one; game-86 filled the 128 slots
// over a few minutes), so the registry cannot pin every list it has seen.
// A list whose last reference is the registry's own was released by its
// creator: nothing can Reset, record or submit it again. Its generation is
// retired in every per-list table as its next Reset would retire it, the
// observer forgets it, and the slot is freed. A list with a recording of ours
// not yet submitted, or pinned elsewhere (the stream route's lifetime, the
// spatial output), has further references and stays. Under Lock.
unsigned prune_released(){
    unsigned pruned{};
    for(unsigned i=0;i<list_count&&!fault.load();++i){
        auto &l=lists[i];auto *p=l.value.Get();
        if(!p||lease.list==p)continue;
        p->AddRef();if(p->Release()!=1)continue;
        if(recording_pins.pending(p,l.generation)||unfiltered_lifetime.pending(p,l.generation))continue;
#ifdef MHW_CONTINUOUS_SCALED
        if(!sr::reset(p,l.generation)){fail(42);break;}
#endif
        if(!recording_pins.reset(p,l.generation)){fail(16);break;}
        if(!unfiltered_lifetime.reset(p,l.generation)){fail(35);break;}
#ifdef MHW_FG_GAME
        frame_generation::game::reset(reinterpret_cast<std::uintptr_t>(p),l.generation);
#endif
        obs::forget(p);
        l=List{};++pruned;
    }
    while(list_count&&!lists[list_count-1].value)--list_count;
    lists_pruned+=pruned;return pruned;
}
// A free slot for a newly seen list: an empty one, a new one, or one freed by
// pruning the released lists. Under Lock.
List *list_slot(){
    const auto empty=[]()->List *{
        for(unsigned i=0;i<list_count;++i)if(!lists[i].value)return &lists[i];
        return list_count<lists.size()?&lists[list_count++]:nullptr;};
    auto *l=empty();
    if(!l&&prune_released())l=empty();
    return l;
}
HRESULT WINAPI reset_hook(ID3D12GraphicsCommandList *list,ID3D12CommandAllocator *allocator,ID3D12PipelineState *pipeline){
    perf::hook_timing::Scope timing(perf::hook_timing::reset_close);Guard guard;HRESULT result;
    {perf::hook_timing::ForwardScope forward(timing.sample);result=reinterpret_cast<HRESULT(WINAPI *)(ID3D12GraphicsCommandList *,ID3D12CommandAllocator *,ID3D12PipelineState *)>(originals[5])(list,allocator,pipeline);}
    if(FAILED(result)||!child(list)||list->GetType()!=D3D12_COMMAND_LIST_TYPE_DIRECT)return result;
    Lock lock;auto *l=find(list);
    if(!l){if(!accepting.load())return result;l=list_slot();if(!l){fail(15);return result;}*l=List{};l->value=list;
        if(const auto n=live_lists();n>lists_peak)lists_peak=n;}
    if(lease.list==list){if(!l->submitted)fail(16);lease={};}
    #ifdef MHW_CONTINUOUS_SCALED
    if(!sr::reset(list,l->generation))fail(42);
    if(fence)sr::collect(fence->GetCompletedValue());
#endif
    if(!recording_pins.reset(list,l->generation))fail(16);
    if(!unfiltered_lifetime.reset(list,l->generation))fail(35);
    if(fence&&!fault.load()){recording_pins.collect(fence->GetCompletedValue());unfiltered_lifetime.collect();}
#ifdef MHW_FG_GAME
    frame_generation::game::reset(reinterpret_cast<std::uintptr_t>(list),l->generation);
#endif
    l->generation=++generation;l->closed=false;l->submitted=false;
    obs::reset(list,l->generation,pipeline);return result;
}
HRESULT WINAPI close_hook(ID3D12GraphicsCommandList *list){
    perf::hook_timing::Scope close_timing(perf::hook_timing::reset_close);Guard guard;
#ifdef MHW_FG_GAME
    // FG brightness capture: the list's last commands, before the native Close.
    if(!injected&&frame_generation::game::capture_wanted()){
        bool open=false;{Lock lock;const auto *l=find(list);open=l&&!l->closed;}
        temporal_present::Bindings roots;
        if(open&&obs::inspect(list,roots)){injected=true;frame_generation::game::closing(list,roots);injected=false;}
    }
#endif
    HRESULT result;
    {perf::hook_timing::ForwardScope forward(close_timing.sample);result=reinterpret_cast<HRESULT(WINAPI *)(ID3D12GraphicsCommandList *)>(originals[6])(list);}
    Lock lock;if(auto *l=find(list)){if(SUCCEEDED(result))l->closed=true;else if(lease.list==list||unfiltered_lifetime.tracked(list,l->generation))fail(17);}return result;
}
void WINAPI execute_hook(ID3D12CommandQueue *queue,UINT count,ID3D12CommandList *const *submitted){
    // Keep native Execute and both completion signals in the same CPU order.
    // Reserving a fence value under Lock alone cannot prevent two submitters
    // from publishing the larger value first after leaving that lock.
    perf::hook_timing::Scope execute_timing(perf::hook_timing::execute);
    SubmissionGuard submission_guard;
    Guard guard;std::array<ID3D12CommandList *,128> copy{};
    bool selected=false,signal_requested=false;ID3D12GraphicsCommandList *selected_list{};std::uint64_t value{};
    UnfilteredLifetime::Submission unfiltered_submission;bool unfiltered_valid=false;
    #ifdef MHW_CONTINUOUS_SCALED
    std::array<std::array<std::uint64_t,2>,128> observed_lists{};bool observed_batch=false,route_selected=false;
    #endif
    if(count<=copy.size()&&(!count||pb::copy_memory(reinterpret_cast<std::uintptr_t>(submitted),copy.data(),count*sizeof(copy[0])))){
        Lock lock;unsigned matches{};
        #ifdef MHW_CONTINUOUS_SCALED
        observed_batch=true;
        #endif
        for(unsigned i=0;i<count;++i){auto *l=find(reinterpret_cast<ID3D12GraphicsCommandList *>(copy[i]));
            #ifdef MHW_CONTINUOUS_SCALED
            observed_lists[i]={reinterpret_cast<std::uintptr_t>(copy[i]),l?l->generation:0};
            #endif
            if(!l||l->injected_generation!=l->generation)continue;
            if(l->submitted||!l->closed||lease.list!=l->value.Get()||lease.generation!=l->generation){fail(18);continue;}
            ++matches;selected_list=l->value.Get();
        }
        #ifdef MHW_CONTINUOUS_SCALED
        route_selected=sr::submission_needed(std::span(observed_lists).first(count));
        bool route_closed=true;
        if(route_selected)for(unsigned i=0;i<count;++i)
            if(sr::submission_needed(std::span(observed_lists).subspan(i,1))){
                const auto *l=find(reinterpret_cast<ID3D12GraphicsCommandList *>(copy[i]));
                route_closed=route_closed&&l&&l->closed;
            }
        const bool owned_batch=matches||route_selected;
        #else
        const bool owned_batch=matches!=0;
        #endif
        if(owned_batch){
            const bool queue_valid=matches<=1&&child(queue)&&queue->GetDesc().Type==D3D12_COMMAND_LIST_TYPE_DIRECT
#ifdef MHW_CONTINUOUS_SCALED
                &&route_closed
#endif
                ;
            selected=queue_valid&&matches==1;
            if(!queue_valid){
#ifdef MHW_CONTINUOUS_SCALED
                if(route_selected)sr::reject_submission(std::span(observed_lists).first(count));
#endif
                fail(19);
            }
            else {
                // Only one private recording may await submission. If the game
                // changes DIRECT queues, explicitly chain our private work.
                bool ordered=true;
                if(last_queue&&last_queue.Get()!=queue&&FAILED(queue->Wait(fence.Get(),signal_value))){selected=false;ordered=false;fail(20);}
                #ifdef MHW_CONTINUOUS_SCALED
                if(ordered&&route_selected&&!sr::validate_submission(reinterpret_cast<std::uintptr_t>(queue),std::span(observed_lists).first(count),signal_value+1)){
                    sr::reject_submission(std::span(observed_lists).first(count));selected=false;ordered=false;fail(43);}
                #endif
                if(ordered){value=++signal_value;last_queue=queue;signal_requested=true;if(selected)find(selected_list)->submitted=true;
#ifdef MHW_CONTINUOUS_SCALED
                    if(route_selected&&!selected)++route_only_signals;
#endif
                }
            }
        }
        const auto batch=std::span<ID3D12CommandList *const>(copy.data(),count);
        unfiltered_valid=unfiltered_lifetime.before(queue,batch,unfiltered_submission);
        if(!unfiltered_valid)fail(36);
    }else if(accepting.load())fail(21);
    {perf::hook_timing::ForwardScope forward(execute_timing.sample);
    reinterpret_cast<void(WINAPI *)(ID3D12CommandQueue *,UINT,ID3D12CommandList *const *)>(originals[7])(queue,count,submitted);}
    #ifdef MHW_CONTINUOUS_SCALED
    if(observed_batch)sr::observe_submission(reinterpret_cast<std::uintptr_t>(queue),GetTickCount64(),std::span(observed_lists).first(count));
    #endif
    #ifdef MHW_FG_GAME
    if(observed_batch){static std::atomic<std::uint64_t> fg_batches{};const auto fg_batch=++fg_batches;
        for(unsigned i=0;i<count;++i)if(observed_lists[i][1])frame_generation::game::submitted(observed_lists[i][0],observed_lists[i][1],fg_batch);}
    #endif
    if(signal_requested){const auto result=queue->Signal(fence.Get(),value);Lock lock;
        if(FAILED(result)){fail(22);return;}
        #ifdef MHW_CONTINUOUS_SCALED
        if(route_selected&&!sr::submitted_batch(reinterpret_cast<std::uintptr_t>(queue),std::span(observed_lists).first(count),value)){fail(43);return;}
        #endif
        if(selected){
        auto *l=find(selected_list);if(!l||l->generation!=lease.generation){fail(23);return;}
        if(!recording_pins.submitted(selected_list,l->generation,value)){fail(28);return;}
        l->signal=value;lease={};
        }
    }
    if(unfiltered_valid){Lock lock;if(!unfiltered_lifetime.after(queue,unfiltered_submission))fail(37);}
}
#ifdef MHW_CONTINUOUS_SCALED
bool observe_prepared_resolve(const NativeFrame &f){
    if(!sr::owns_packet(f.call.gpr[3]))return false;
    std::uintptr_t backend{},recording{},command{};
    if(!reader.get(f.call.gpr[2]+8,backend)||backend!=cfg.backend||!reader.get(f.call.gpr[1]+8,recording)||
       !recording||!reader.get(recording+0x20,command)||!command)return false;
    Lock lock;auto *list=reinterpret_cast<ID3D12GraphicsCommandList *>(command);const auto *entry=find(list);
    return entry&&!entry->closed&&sr::observe_resolve(f.call.gpr[3],list,entry->generation);
}
#endif
void native_seh(unsigned kind,NativeFrame *f){
    auto timing=perf::hook_timing::begin(perf::hook_timing::native_frame);
    bool admitted=false,returned=false;
#ifdef MHW_CONTINUOUS_SCALED
    // Forwarding saves native RETURN registers into f->call. R8 is volatile;
    // preserve the entry packet independently for after-call scope cleanup.
    const auto prepared_packet=f->call.gpr[3];
    const bool prepared=kind==1&&observe_prepared_resolve(*f);
    const auto compute_token=kind==1?sr::compute_before(f->call.gpr[1],f->call.gpr[2],prepared_packet):UINT_MAX;
#endif
    ForwardedRequest forwarded{};
    if(kind==0)admitted=taa_before(*f);
    else if(kind==1)admitted=consumer_before(*f,false,&forwarded);
    else if(kind==2)admitted=depth_before(*f);
    else if(kind==3)admitted=consumer_before(*f,true);
    else admitted=projection_before(*f);
    __try{perf::hook_timing::forward_begin(timing);mhw_binding_forward(originals[kind],&f->call);perf::hook_timing::forward_end(timing);returned=true;}
    __finally{
#ifdef MHW_CONTINUOUS_SCALED
        if(prepared)sr::finish_resolve(prepared_packet,returned);
        if(compute_token!=UINT_MAX)sr::compute_after(compute_token,returned);
#endif
        if(kind==0)taa_after(admitted,returned);
        else if(kind==1||kind==3){finish_forwarded(forwarded,returned);consumer_after(admitted,returned);}
        else if(kind==2)depth_after(admitted,returned);
        else projection_after(admitted,returned,*f);
        perf::hook_timing::end(timing);
    }
}
}
void native(unsigned kind,NativeFrame *f){pending.add();__try{native_seh(kind,f);}__finally{pending.sub();}}
namespace {
// A pending live preset request: the mode's value, or -1.
std::atomic<int> mode_request{-1};
// Set once install() completed (any thread reads it; the worker owns pass).
std::atomic<bool> live_installed{};
// The last video memory check (install or rebuild), for the summary. Under Lock.
gpu_memory::Check memory_check;
// The game's adapter for the video memory budget: its swap chain's factory
// enumerates it by the device's LUID (no second factory). Once a process.
bool pin_adapter(ID3D12Device *device,std::uintptr_t swapchain) noexcept{
    if(gpu_memory::pinned.load())return true;
    ComPtr<IDXGISwapChain> chain;ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter3> adapter;
    return device&&swapchain&&SUCCEEDED(reinterpret_cast<IUnknown *>(swapchain)->QueryInterface(IID_PPV_ARGS(&chain)))&&
        SUCCEEDED(chain->GetParent(IID_PPV_ARGS(&factory)))&&
        SUCCEEDED(factory->EnumAdapterByLuid(device->GetAdapterLuid(),IID_PPV_ARGS(&adapter)))&&gpu_memory::pin(adapter.Get());
}
// Whether `needed` more bytes fit the video memory budget; keeps the check for
// the summary and, on a refusal, the reason.
bool memory_fits(std::uint64_t needed,gpu_memory::Check &c,std::string &why){
    c=gpu_memory::check(needed);{Lock lock;memory_check=c;}
    if(c.passed)return true;
    why=c.read?"video memory: "+std::to_string(needed>>20)+" MB needed, "+
        std::to_string(gpu_memory::room({c.budget,c.usage})>>20)+" MB left in the budget":"video memory budget unavailable";
    return false;
}
}
void request_mode(dlss_sdk::ReconstructionMode m) noexcept{mode_request.store(static_cast<int>(m));}
bool requested_mode(dlss_sdk::ReconstructionMode &m) noexcept{
    const int v=mode_request.exchange(-1);if(v<0)return false;m=static_cast<dlss_sdk::ReconstructionMode>(v);return true;}
#ifdef MHW_CONTINUOUS_SCALED
namespace {
// Polls `done` until true or `ms` pass; Sleep(1) between polls.
template<class F> bool wait_for(F &&done,std::uint64_t ms){
    const auto end=GetTickCount64()+ms;
    while(!done()){if(GetTickCount64()>=end)return false;Sleep(1);}
    return true;
}
// Observes `count` at zero once within `ms`. Every hook counts itself before
// it reads `accepting`, so after a stop one zero proves that no callback that
// could still have seen the runtime accepting is running.
bool observed_zero(const perf::InFlightCount &count,std::uint64_t ms){
    // One flush (zero()), then polling the slots.
    if(count.zero())return true;
    const auto end=GetTickCount64()+ms;
    while(!count.idle()){if(GetTickCount64()>=end)return false;SwitchToThread();}
    return true;
}
bool observed_zero(const std::atomic<unsigned> &count,std::uint64_t ms){
    const auto end=GetTickCount64()+ms;
    while(count.load()){if(GetTickCount64()>=end)return false;SwitchToThread();}
    return true;
}
template<class F> bool observed_true(F &&idle,std::uint64_t ms){
    const auto end=GetTickCount64()+ms;
    while(!idle()){if(GetTickCount64()>=end)return false;SwitchToThread();}
    return true;
}
}
bool live_modes() noexcept{return live_installed.load()&&healthy();}
namespace {
// change_mode() and change_scene(): the same rebuild. A scene change also
// rediscovers the scene, reallocates the scene-size copies and resumes the
// render-scale override with rebase() instead of retarget().
ModeChange rebuild(dlss_sdk::ReconstructionMode requested,bool scene) noexcept{
    ModeChange r;r.scene=scene;r.mode=requested;
    LARGE_INTEGER began{},frequency{};QueryPerformanceCounter(&began);QueryPerformanceFrequency(&frequency);
    const auto mark=[&](unsigned phase){LARGE_INTEGER now{};QueryPerformanceCounter(&now);r.phase=phase;
        r.us[phase]=frequency.QuadPart>0?static_cast<std::uint64_t>((now.QuadPart-began.QuadPart)*1000000/frequency.QuadPart):0;};
    // Nothing changed: admit frames again, unless a fault arrived meanwhile.
    const auto resume=[&](const char *why){
        r.reason=why;
        if(!healthy()){stop();return r;}
        sr::begin();accepting.store(true);r.resumed=true;return r;
    };
    bool changed=false;
    try{
        if(!live_modes()||!pass||!spatial||reconstruction.failed()){r.reason="DLSS is not running";return r;}
        // The scene: the current one, or the game's new one (read again; the
        // device, swap chain, arena and window must be the installed ones).
        pb::Runtime found=runtime;
        if(scene){
            if(!render_resolution::scene_changed()){r.reason="no scene change pending";return r;}
            std::string why;
            if(!pb::refresh_scene(cfg.image,runtime,found,why)){r.reason="scene: "+why;r.retry=true;return r;}
            if(found.window_width!=runtime.window_width||found.window_height!=runtime.window_height||found.format!=runtime.format)
                throw std::runtime_error("the window or swap chain changed");
        }else if(render_resolution::scene_changed()){r.reason="the screen layout is changing";r.retry=true;return r;}
        r.scene_width=found.width;r.scene_height=found.height;
        const unsigned basis=found.scene_override?found.window_width:found.width;
        const auto planned=render_resolution::plan({found.width,found.height},basis,dlss_sdk::requested_scale(requested));
        if(!planned.valid){
            if(scene)throw std::runtime_error("extent plan");
            r.reason="extent plan";return r;}
        // As install(): a 100% plan just below the scene runs as Quality.
        auto mode=requested;
        if(mode==dlss_sdk::ReconstructionMode::dlaa&&(planned.render.width!=found.width||planned.render.height!=found.height))
            mode=dlss_sdk::ReconstructionMode::quality;
        r.mode=mode;r.render_width=planned.render.width;r.render_height=planned.render.height;
        if(!scene&&mode==scaled_geometry.mode&&planned.render.width==render_width()&&planned.render.height==render_height()){
            configured_mode=requested;r.ok=r.unchanged=true;return r;}
        // The new images must fit the video memory budget while the old ones
        // still exist (they are released after the swap); nothing changed yet.
        // A preset the budget refuses leaves the current one running; a scene
        // change stops DLSS (the game's new scene needs them).
        const bool new_copies=found.width!=runtime.width||found.height!=runtime.height;
        gpu_memory::Check memory;
        if(std::string why;!memory_fits(gpu_memory::dlss_bytes(found.width,found.height,planned.render.width,planned.render.height,new_copies),memory,why)){
            if(scene)throw std::runtime_error(why);
            r.reason=why;r.memory=r.resumed=true;return r;}
        // 1 pause.
        accepting.store(false);sr::stop();mark(1);
        // 2 drain.
        const auto drained=[&]{
            Lock lock;
            const auto completed=fence?fence->GetCompletedValue():0;
            if(completed==UINT64_MAX)return false;
            sr::collect(completed);
            return !publishing.load()&&!projection_busy.test()&&!lease.list&&completed>=signal_value&&sr::idle();
        };
        if(!wait_for(drained,3000)){r.retry=true;return resume("drain timeout (is the game rendering?)");}
        if(!observed_zero(pending,2000)||!observed_true(sr::callbacks_idle,2000)){r.retry=true;return resume("callbacks still running");}
        if(!drained()){r.retry=true;return resume("drain changed");}
        mark(2);
        // 3 queue barrier.
        {
            SubmissionGuard guard;ComPtr<ID3D12CommandQueue> queue;
            {Lock lock;queue=last_queue;if(queue)r.barrier=++signal_value;}
            if(queue&&FAILED(queue->Signal(fence.Get(),r.barrier)))return resume("queue barrier signal");
        }
        if(r.barrier&&!wait_for([&]{const auto v=fence->GetCompletedValue();return v!=UINT64_MAX&&v>=r.barrier;},2000)){
            r.retry=true;return resume("queue barrier timeout");}
        mark(3);
        // 4 the render scale: retarget, or rebase on the new scene. From here
        // a failure stops the runtime.
        if(scene){
            if(std::string why;!render_resolution::rebase({found.width,found.height},basis,dlss_sdk::requested_scale(requested),why))
                throw std::runtime_error("render scale: "+why);
        }else if(std::string why;!render_resolution::retarget(dlss_sdk::requested_scale(requested),why))
            return resume(("render scale: "+why).c_str());
        changed=true;
        if(!wait_for([]{return render_resolution::applied_visits()>=4;},3000))throw std::runtime_error("the game did not apply the render scale");
        mark(4);
        // 5 rebuild. Nothing records with the old feature, output, copies or
        // route effects any more, and the barrier proved their GPU work done.
        auto *device=reinterpret_cast<ID3D12Device *>(cfg.device);
        const auto *initialization=ngx_startup::borrowed_initialization();
        if(!initialization)throw std::runtime_error("NGX startup ownership");
        {
            Lock lock;
            if(spatial->recorded()&&!spatial->retired())spatial->retire(fence.Get(),r.barrier,true);
            pass->retire(fence.Get(),r.barrier);
        }
        // The private copies of the game's inputs are scene-sized.
        std::array<ComPtr<ID3D12Resource>,3> copies{raw[0],raw[1],raw[2]};
        if(new_copies){
            constexpr DXGI_FORMAT formats[]{DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R32_UINT};
            for(unsigned i=0;i<3;++i){D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=found.width;desc.Height=found.height;
                desc.DepthOrArraySize=desc.MipLevels=1;desc.Format=formats[i];desc.SampleDesc.Count=1;D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
                copies[i].Reset();
                if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&copies[i]))))
                    throw std::runtime_error("continuous raw allocation");}
        }
        const dlss_sdk::Geometry geometry{{found.width,found.height},{planned.render.width,planned.render.height},{found.width,found.height},mode};
        auto fresh=std::make_unique<dlss_sdk::InlineFrame>(device,geometry,std::array<ID3D12Resource *,3>{copies[0].Get(),copies[1].Get(),copies[2].Get()},
            sdk_runtime,*initialization,true,true,false,gpu_memory::room({memory.budget,memory.usage}));
        auto fresh_spatial=std::make_unique<temporal_present::Output>(device,temporal_present::SpatialSource{found.width,found.height,
            planned.render.width,planned.render.height},found.width,found.height,true,true);
        if(!sr::resize(planned.render.width,planned.render.height,found.width,found.height,r.reason))throw std::runtime_error(r.reason);
        {
            Lock lock;
            pass=std::move(fresh);spatial=std::move(fresh_spatial);raw=copies;runtime=found;
            scaled_render={planned.render.width,planned.render.height};
            edge_column=static_cast<float>(runtime.width)*planned.quantized>static_cast<float>(planned.render.width);
            edge_row=static_cast<float>(runtime.height)*planned.quantized>static_cast<float>(planned.render.height);
            scaled_geometry=geometry;configured_mode=requested;
            // Published requests, depths and projections belong to frames at
            // the old extent; none is consumed now. The next frame resets.
            for(auto &x:requests)x={};
            for(auto &x:depths)x={};
            for(auto &x:projections)x={};
            previous={};history_depth.Reset();
        }
        mark(5);
        // 6 resume.
        sr::begin();accepting.store(true);mark(6);r.ok=true;return r;
    }catch(const std::exception &e){r.reason=e.what();}
    catch(...){r.reason="rebuild";}
    if(!changed&&!scene&&!fault.load()){const std::string why=r.reason.empty()?"mode change":r.reason;return resume(why.c_str());}
    r.retry=false;fail(scene?48:47);stop();return r;
}
}
ModeChange change_mode(dlss_sdk::ReconstructionMode requested) noexcept{return rebuild(requested,false);}
ModeChange change_scene() noexcept{return rebuild(configured_mode,true);}
bool diagnostic_pause(bool pause) noexcept{
    if(!live_modes())return false;
    if(pause){accepting.store(false);sr::stop();return true;}
    if(!healthy())return false;
    sr::begin();accepting.store(true);return true;
}
void reconstruction_mode(dlss_sdk::ReconstructionMode m) noexcept{if(!pass)configured_mode=m;}
// Worker: the mode DLSS runs (after install; it may differ from the configured
// one, see install) or will run.
dlss_sdk::ReconstructionMode reconstruction_mode() noexcept{return pass?scaled_geometry.mode:configured_mode;}
#else
bool live_modes() noexcept{return false;}
ModeChange change_mode(dlss_sdk::ReconstructionMode) noexcept{ModeChange r;r.reason="unscaled build";return r;}
ModeChange change_scene() noexcept{ModeChange r;r.reason="unscaled build";return r;}
bool diagnostic_pause(bool) noexcept{return false;}
// The unscaled build always runs DLAA.
void reconstruction_mode(dlss_sdk::ReconstructionMode) noexcept{}
dlss_sdk::ReconstructionMode reconstruction_mode() noexcept{return dlss_sdk::ReconstructionMode::dlaa;}
#endif
bool scene_extent(unsigned &scene_width,unsigned &scene_height,unsigned &window_width,unsigned &window_height) noexcept{
    if(!pass)return false;
    scene_width=runtime.width;scene_height=runtime.height;window_width=runtime.window_width;window_height=runtime.window_height;return true;
}
bool render_extent(unsigned &width,unsigned &height) noexcept{
    if(!pass)return false;
    width=render_width();height=render_height();return true;
}
bool qualified(std::uintptr_t image,pb::Runtime &found,std::string &reason){
        if(!pb::discover(image,found,reason)||
            !pb::verify_code(image,0x2392150,4221,"d2c6c0f6a5ed39c998bcf62afed19132e6dd0d7cfbb19e18b80c15531af2bf91")||
            !pb::verify_code(image,tb::dispatch_rva,321,"d073b70ae437d281596861720893c3b3cc9f82155fa0c83f2c3ae6918fd2f3a2")||
            !pb::verify_code(image,0x259cc20,1283,"6921c7cbfc7b277f4b076a68de8165bd54b3f6571ca574fdb16d5c35cbe154da")||
            !pb::verify_code(image,0x23cd170,1711,"8103c87804f325a15bbeb3529f3b575cc14e3bae986156d53e1b796fbbec6d31")||
            !pb::verify_code(image,0x259ba50,495,"f560506e570fff4dd21df27e5d68db204420e82e3eed1bb5aae2cc3ad1441ee5")||
            !pb::verify_code(image,0x259df30,681,"e84ae8742c4a0ef334692595bb89c04c8be80306d4416c20304566cc1e5dd85a")||
            !pb::verify_code(image,0x228f3b0,2000,"df1bf81584401fe529bc5fbbc3e19010e8959c2d13745eca6f87897183148373")||
            !pb::verify_code(image,0x2292610,304,"4adcc8e99a9ff16ba9af27c8db165b998958561d82da0d3c9e7bcdcb82cd37f9")||
            !pb::verify_code(image,0x22927f0,736,"bef3f834e34a78dce1392d047062123d7c2d5633de7390aa0216cb654519e2ed")||
            !pb::verify_code(image,0x229cee0,7756,"11f7cc4d48598967f439659c87142839c4ad3fb900cfafb7fab3631655691af3")||
            !pb::verify_code(image,0x24e7a10,2620,"7ea61c6f4bbb21c5d8a25d5801a92476b825595c96937ac57d1e44870caea6fb")){
            if(reason.empty())reason="continuous-code-gate";return false;}
        return true;
}
bool ready(std::uintptr_t image,std::string &reason){
    try{
        if(pass){reason="continuous-already-prepared";return false;}
        pb::Runtime found;if(!qualified(image,found,reason))return false;
#ifdef MHW_CONTINUOUS_SCALED
        if(!render_resolution::ready(image,dlss_sdk::requested_scale(configured_mode),reason))return false;
#endif
        return true;
    }catch(const std::exception &e){reason=e.what();return false;}
}
bool install(std::uintptr_t image,const std::filesystem::path &sdk,std::string &reason){
    try {
        if(pass){reason="continuous-already-prepared";return false;}
        if(!qualified(image,runtime,reason))return false;
        cfg={image,0,runtime.backend,runtime.device};if(!pb::read(image+0x51c4480,cfg.renderer)||!cfg.renderer)throw std::runtime_error("continuous renderer");
        auto *device=reinterpret_cast<ID3D12Device *>(cfg.device);
#ifdef MHW_CONTINUOUS_SCALED
        {const auto planned=render_resolution::plan({runtime.width,runtime.height},runtime.scene_override?runtime.window_width:runtime.width,
             dlss_sdk::requested_scale(configured_mode));
         if(!planned.valid)throw std::runtime_error("scaled render plan");scaled_render={planned.render.width,planned.render.height};
         edge_column=static_cast<float>(runtime.width)*planned.quantized>static_cast<float>(planned.render.width);
         edge_row=static_cast<float>(runtime.height)*planned.quantized>static_cast<float>(planned.render.height);}
        const unsigned planned_width=render_width(),planned_height=render_height();
#else
        const unsigned planned_width=runtime.width,planned_height=runtime.height;
#endif
        // The images must fit the video memory budget (gpu_memory/budget.hpp).
        if(!pin_adapter(device,runtime.swapchain))throw std::runtime_error("video memory: the game's adapter");
        gpu_memory::Check memory;
        if(std::string why;!memory_fits(gpu_memory::dlss_bytes(runtime.width,runtime.height,planned_width,planned_height),memory,why))
            throw std::runtime_error(why);
        const auto byte_limit=gpu_memory::room({memory.budget,memory.usage});
        constexpr DXGI_FORMAT formats[]{DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R32_UINT};
        for(unsigned i=0;i<3;++i){D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=runtime.width;desc.Height=runtime.height;desc.DepthOrArraySize=desc.MipLevels=1;desc.Format=formats[i];desc.SampleDesc.Count=1;
            D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
            if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&raw[i]))))throw std::runtime_error("continuous raw allocation");}
        const auto *initialization=ngx_startup::borrowed_initialization();if(!initialization)throw std::runtime_error("continuous startup ownership");
        sdk_runtime=sdk;
        #ifdef MHW_CONTINUOUS_SCALED
        // DLAA renders at the scene size. A basis width that is not a multiple
        // of 16 pixels quantizes 100% just below it; DLSS then upscales that
        // extent as Quality.
        auto mode=configured_mode;
        if(mode==dlss_sdk::ReconstructionMode::dlaa&&(render_width()!=runtime.width||render_height()!=runtime.height))mode=dlss_sdk::ReconstructionMode::quality;
        scaled_geometry={{runtime.width,runtime.height},{render_width(),render_height()},{runtime.width,runtime.height},mode};
        pass=std::make_unique<dlss_sdk::InlineFrame>(device,scaled_geometry,std::array<ID3D12Resource *,3>{raw[0].Get(),raw[1].Get(),raw[2].Get()},sdk,*initialization,true,true,false,byte_limit);
        spatial=std::make_unique<temporal_present::Output>(device,temporal_present::SpatialSource{runtime.width,runtime.height,render_width(),render_height()},runtime.width,runtime.height,true,true);
#else
        pass=std::make_unique<dlss_sdk::InlineFrame>(device,dlss_sdk::Geometry::dlaa(runtime.width,runtime.height),std::array<ID3D12Resource *,3>{raw[0].Get(),raw[1].Get(),raw[2].Get()},sdk,*initialization,true,true,true,byte_limit);
#endif
        if(FAILED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence))))throw std::runtime_error("continuous fence");
        if(!unfiltered_lifetime.initialize(device))throw std::runtime_error("unfiltered lifetime fence");
        ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12CommandQueue> queue;
        D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
        if(FAILED(device->CreateCommandAllocator(q.Type,IID_PPV_ARGS(&allocator)))||FAILED(device->CreateCommandList(0,q.Type,allocator.Get(),nullptr,IID_PPV_ARGS(&list)))||FAILED(list->Close())||FAILED(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))))throw std::runtime_error("continuous method discovery");
        targets[0]=image+tb::dispatch_rva;targets[1]=image+0x259cc20;targets[2]=image+0x23cd170;targets[3]=image+0x259df30;targets[4]=image+0x228f3b0;
        constexpr unsigned slots[]{10,9,10,26,14,17
#ifdef MHW_CONTINUOUS_SCALED
            ,12,13
#endif
        };
        for(unsigned i=5;i<hook_count;++i)if(!pb::system_method(reinterpret_cast<std::uintptr_t>(i==7?static_cast<IUnknown *>(queue.Get()):static_cast<IUnknown *>(list.Get())),slots[i-5],targets[i]))throw std::runtime_error("continuous API gate");
        const std::array<void *,hook_count> detours{reinterpret_cast<void *>(temporal_dispatch_entry),reinterpret_cast<void *>(temporal_consumer_entry),reinterpret_cast<void *>(temporal_depth_producer_entry),reinterpret_cast<void *>(temporal_depth_consumer_entry),reinterpret_cast<void *>(temporal_jitter_entry),
            reinterpret_cast<void *>(reset_hook),reinterpret_cast<void *>(close_hook),reinterpret_cast<void *>(execute_hook),reinterpret_cast<void *>(barrier_hook),reinterpret_cast<void *>(dispatch_hook),reinterpret_cast<void *>(copy_hook)
#ifdef MHW_CONTINUOUS_SCALED
            ,reinterpret_cast<void *>(draw_hook),reinterpret_cast<void *>(indexed_hook)
#endif
        };
        #ifdef MHW_CONTINUOUS_SCALED
        // The sizing gates must be checked before activation modifies their callees.
        if(!render_resolution::install(image,dlss_sdk::requested_scale(configured_mode),reason,true))throw std::runtime_error(reason);
        if(!obs::install(device,pb::system_method,reason,true,render_resolution::observe_viewports,true,true))throw std::runtime_error(reason);
        if(!sr::prepare(cfg,runtime.width,runtime.height,render_width(),render_height(),reason))throw std::runtime_error(reason);
#ifdef MHW_FG_GAME
        sr::observe_final_scene(&fg_final_scene);
#endif
#else
        if(!obs::install(device,pb::system_method,reason))return false;
#endif
        for(unsigned i=0;i<targets.size();++i){
            if(astera::active()&&i>=first_api_hook){if(!originals[i])throw std::runtime_error("continuous AL chain");enabled[i]=true;continue;}
            if(MH_CreateHook(reinterpret_cast<void *>(targets[i]),detours[i],&originals[i])!=MH_OK)throw std::runtime_error("continuous hook creation");
            if(MH_QueueEnableHook(reinterpret_cast<void *>(targets[i]))!=MH_OK)throw std::runtime_error("continuous enable queue");enabled[i]=true;
        }
        if(!activation::prepare(image,cfg.renderer,reason))throw std::runtime_error(reason);
        obs::begin();
#ifdef MHW_CONTINUOUS_SCALED
        sr::begin();
#endif
        accepting.store(true);if(MH_ApplyQueued()!=MH_OK)throw std::runtime_error("continuous enable apply");
#ifdef MHW_CONTINUOUS_SCALED
        live_installed.store(true);
#endif
        if(astera::active()){chain_live.store(true,std::memory_order_release);
#ifdef MHW_CONTINUOUS_SCALED
            sr::chains_live(true);
#endif
        }
        return true;
    }catch(const std::exception &e){reason=e.what();stop();return false;}
}
void stop() noexcept {accepting.store(false);live_installed.store(false);activation::stop();
#ifdef MHW_CONTINUOUS_SCALED
    sr::stop();render_resolution::stop();
#endif
}
bool healthy() noexcept {return fault.load()==0&&obs::healthy()&&activation::healthy()
#ifdef MHW_CONTINUOUS_SCALED
    &&sr::healthy()&&render_resolution::healthy()
#endif
    ;}
void finish() noexcept {
    stop();const auto deadline=GetTickCount64()+2000;
    while((!pending.zero()||projection_busy.test()||!activation::drained())&&GetTickCount64()<deadline)Sleep(1);
    if(!pending.zero()||projection_busy.test()||!activation::drained()){fail(24);return;}
    #ifdef MHW_CONTINUOUS_SCALED
    // Restore native sizing while the render-thread callback is still installed.
    if(!render_resolution::finish()){fail(44);return;}
    while(sr::healthy()&&!sr::drained()&&GetTickCount64()<deadline+6000)Sleep(1);
    if(!sr::drained()){
        fail(45);
        // A rejected frame cannot finish its logical chain. Retain uncertain
        // pins, but still remove interception once route callbacks are idle.
        bool abandoned=sr::abandon();
        while(!abandoned&&GetTickCount64()<deadline+6000){Sleep(1);abandoned=sr::abandon();}
        if(!abandoned)return;
    }
    const bool route_disable_queued=sr::queue_disable();
    if(!route_disable_queued)fail(45);
#endif
    if(!activation::queue_disable())fail(25);
    std::array<bool,targets.size()> disable_queued{};
    for(unsigned i=0;i<targets.size();++i)if(enabled[i]){
        disable_queued[i]=(astera::active()&&i>=first_api_hook)||MH_QueueDisableHook(reinterpret_cast<void *>(targets[i]))==MH_OK;
        if(!disable_queued[i])fail(25);
    }
    if(MH_ApplyQueued()==MH_OK){
        if(astera::active())chain_live.store(false,std::memory_order_release);
        for(unsigned i=0;i<targets.size();++i)if(disable_queued[i])enabled[i]=false;
        activation::disabled(true);
#ifdef MHW_CONTINUOUS_SCALED
        if(route_disable_queued)sr::disabled(true);
#endif
    }else fail(26);
    // A forwarded callback may have entered before the patch was removed.
    // Keep the observer and all trampolines available until it returns.
    const auto return_deadline=GetTickCount64()+2000;
    const auto callbacks_busy=[](){return !pending.zero()||projection_busy.test()||!activation::drained()
#ifdef MHW_CONTINUOUS_SCALED
        ||!sr::callbacks_idle()
#endif
        ;};
    while(callbacks_busy()&&GetTickCount64()<return_deadline)Sleep(1);
    if(callbacks_busy()){fail(24);return;}
    if(!obs::finish())fail(27);
    // Completed AND reset recordings can release their resource pins. Remaining
    // recordings, private SDK state and list registry stay pinned at stop.
    // No inference of retirement, game-thread wait, unload or SDK Shutdown.
}
void save_rejected_shader(const std::filesystem::path &path) noexcept {
    try {
        Lock lock;
        if(!shader_rejection.present||!shader_rejection.bytes||accepting.load()||!pending.zero()||
           !activation::drained())return;
        const HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE)return;
        DWORD written{};
        shader_rejection.file_saved=WriteFile(file,shader_rejection.bytecode.data(),shader_rejection.bytes,&written,nullptr)&&
            written==shader_rejection.bytes&&FlushFileBuffers(file);
        CloseHandle(file);
    }catch(...){/* Artifact errors never change renderer recovery. */}
}
bool quiescent() noexcept {
    Lock lock;
    if(accepting.load()||!pending.zero()||projection_busy.test()||!activation::drained()||
       !obs::settled()||!obs::hooks_disabled()||lease.list||
       (fence&&(fence->GetCompletedValue()==UINT64_MAX||fence->GetCompletedValue()<signal_value)))return false;
    for(bool e:enabled)if(e)return false;
#ifdef MHW_CONTINUOUS_SCALED
    if(!sr::callbacks_idle()||!sr::drained())return false;
#endif
    return healthy();
}
void save_compute_depth(const std::filesystem::path &prefix) noexcept {
#ifdef MHW_CONTINUOUS_SCALED
    Lock lock;if(!fault.load()){if(fence)sr::collect(fence->GetCompletedValue());sr::save_compute_depth(prefix);}
#else
    (void)prefix;
#endif
}
void save_inserted_effect(const std::filesystem::path &prefix) noexcept {
#ifdef MHW_CONTINUOUS_SCALED
    Lock lock;if(!fault.load())sr::save_inserted_effect(prefix);
#else
    (void)prefix;
#endif
}
void write_summary(std::ostream &out){Lock lock;const auto &h=history_telemetry;const auto reads=bridge_probe::read_cache_stats();
    out<<"{\"active\":"<<accepting.load()<<",\"fault\":"<<fault.load()<<",\"frames\":"<<frames.load()<<",\"skips\":"<<skips.load()
       <<",\"lists\":{\"live\":"<<live_lists()<<",\"peak\":"<<lists_peak<<",\"pruned\":"<<lists_pruned<<'}'
       <<",\"reads\":{\"level1_misses\":"<<reads.level1_misses<<",\"region_hits\":"<<reads.region_hits<<",\"refreshes\":"<<reads.refreshes<<",\"queries\":"<<reads.queries<<",\"background_queries\":"<<reads.background_queries<<'}'
       <<",\"video_memory\":{\"checked\":"<<memory_check.done<<",\"read\":"<<memory_check.read<<",\"passed\":"<<memory_check.passed
       <<",\"budget_mb\":"<<(memory_check.budget>>20)<<",\"usage_mb\":"<<(memory_check.usage>>20)<<",\"needed_mb\":"<<(memory_check.needed>>20)
       <<",\"images_mb\":"<<((pass?pass->allocation_bytes():0)>>20)<<'}'
       <<",\"evaluations\":"<<h.evaluations<<",\"resets\":"<<h.resets<<",\"reasons\":{";
    for(unsigned i=0;i<reset_reason_count;++i){if(i)out<<',';out<<'"'<<reset_reason_names[i]<<"\":"<<h.counts[i];}
    out<<"},\"projection_changes_retained\":"<<h.projection_changes_retained<<",\"skip_reasons\":[";
    for(unsigned i=0;i<dispatch_reasons.size();++i){if(i)out<<',';out<<dispatch_reasons[i];}out<<']';
#ifdef MHW_CONTINUOUS_SCALED
    // Skip reason 7 is a missing depth lease or a route readiness rejection.
    {DepthTraceLock trace_lock;out<<",\"depth_missing\":"<<scaled_depth_trace.missing_count<<",\"route_not_ready\":"<<scaled_depth_trace.readiness_rejected;}
    // Why the private depth copy was not admitted (DLAA, game-65: every frame),
    // and the depth producer/consumer trace by stage and reason.
    out<<",\"consumer_depth\":";depth_consumer_admission.write(out);
    out<<",\"depth_capture\":";{DepthTraceLock trace_lock;scaled_depth_trace.write(out);}
    out<<",\"identity_depth\":";identity_depth_survey.write(out);
    out<<",\"route\":";sr::write_summary(out);
#endif
    out<<'}';
}
void write(std::ostream &out){Lock lock;bool disabled=true;for(bool e:enabled)disabled=disabled&&!e;
    const auto completed=fence?fence->GetCompletedValue():0;
    // A failed/duplicate native submission is still forwarded. Its old fence
    // may not cover that unexpected use, so freeze all remaining pins on fault.
    if(!fault.load())recording_pins.collect(completed);
#ifdef MHW_CONTINUOUS_SCALED
    if(!fault.load())sr::collect(completed);
#endif
    out<<std::boolalpha<<"{\"event\":\"continuous\",\"schema\":1,\"active\":"<<accepting.load()<<",\"fault\":"<<fault.load()<<",\"pending\":"<<pending.load()<<",\"frames\":"<<frames.load()<<",\"skips\":"<<skips.load()<<",\"history_resets\":"<<reset_count.load()
       <<",\"jitter_injections\":"<<jitter_count.load()<<",\"jitter_restorations\":"<<restorations.load()<<",\"depth_copies\":"<<depth_count.load()<<",\"signal_value\":"<<signal_value<<",\"completed_value\":"<<completed
       <<",\"last_skip\":"<<last_skip.load()<<",\"list_count\":"<<list_count<<",\"lists\":{\"live\":"<<live_lists()<<",\"peak\":"<<lists_peak<<",\"pruned\":"<<lists_pruned<<"},\"resource_count\":"<<4*recording_pins.live()<<",\"pending_private_recording\":"<<(lease.list!=nullptr)<<",\"hooks_disabled\":"<<disabled<<",\"retention\":\"reset-and-fence-v1\",\"width\":"<<runtime.width<<",\"height\":"<<runtime.height<<",\"format\":"<<runtime.format<<",\"luid_low\":"<<runtime.luid_low<<",\"luid_high\":"<<runtime.luid_high<<",\"sdk\":";
    if(pass)pass->write(out);else out<<"null";out<<",\"history\":";history_telemetry.write(out);
    out<<",\"requests\":";request_telemetry.write(out);out<<",\"recordings\":";recording_pins.write(out,completed);
    out<<",\"resolve\":";resolve_telemetry.write(out);
    out<<",\"shader_rejection\":";shader_rejection.write(out,shader_rejection.bytes?pb::bytes_sha256(shader_rejection.bytecode.data(),shader_rejection.bytes):std::string{});
#ifdef MHW_CONTINUOUS_SCALED
    out<<",\"shader_admission\":{\"schema\":1,\"variants\":[";
    for(unsigned i=0;i<unknown_native_shader;++i){if(i)out<<',';const auto &v=native_shader_variants[i];
        out<<"{\"technique\":"<<v.technique<<",\"bytes\":"<<v.bytes<<",\"verified\":"<<shader_cache.verified[i]<<",\"accepted\":"<<shader_cache.accepted[i]<<'}';}
    out<<"]}";
    out<<",\"preparation_shader_admission\":{\"schema\":1,\"variants\":[";
    for(unsigned i=0;i<unknown_native_shader;++i){if(i)out<<',';const auto &v=native_shader_variants[i];
        out<<"{\"technique\":"<<v.technique<<",\"bytes\":"<<v.bytes<<",\"verified\":"<<preparation_shader_cache.verified[i]<<",\"accepted\":"<<preparation_shader_cache.accepted[i]<<'}';}
    out<<"]}";
#endif
    out<<",\"reconstruction\":";reconstruction.write(out);
    out<<",\"state_trace\":";state_trace.write(out);
    out<<",\"state_admission\":";state_admission.write(out);
    out<<",\"depth_eligibility\":";depth_eligibility.write(out);
    out<<",\"depth_destination\":";depth_destination.write(out);
    out<<",\"unfiltered_lifetime\":";unfiltered_lifetime.write(out,!fault.load());
    out<<",\"zero_jitter_projections\":"<<zero_jitter_projections.load()<<",\"dispatch_reasons\":[";
    for(unsigned i=0;i<dispatch_reasons.size();++i)out<<(i?",":"")<<dispatch_reasons[i];out<<']';
    out<<",\"forwarded_requests\":";forwarded_telemetry.write(out);
    out<<",\"activation\":";activation::write(out);
#ifdef MHW_CONTINUOUS_SCALED
    out<<",\"scaled_depth_capture\":";{DepthTraceLock trace_lock;scaled_depth_trace.write(out);}
    out<<",\"depth_schedule\":";depth_schedule_trace.write(out);
    out<<",\"consumer_depth\":";depth_consumer_admission.write(out);
    out<<",\"spatial_recordings\":"<<spatial_recordings<<",\"route_only_signals\":"<<route_only_signals;
    out<<",\"edge_fill\":{\"column\":"<<edge_column<<",\"row\":"<<edge_row<<",\"fills\":"<<edge_fills<<'}';
    out<<",\"scaled_route\":";sr::write(out);
    out<<",\"render_width\":"<<render_width()<<",\"render_height\":"<<render_height();
    std::ostringstream scale;render_resolution::write(scale);auto json=scale.str();while(!json.empty()&&(json.back()=='\n'||json.back()=='\r'))json.pop_back();out<<",\"resolution\":"<<json;
#endif
    out<<"}\n"<<std::flush;

}
}
extern "C" void mhw_temporal_dispatch(mhw::temporal_bindings::NativeFrame *f){mhw::continuous_dlaa::native(0,f);}
extern "C" void mhw_temporal_consume(mhw::temporal_bindings::NativeFrame *f){mhw::continuous_dlaa::native(1,f);}
extern "C" void mhw_temporal_depth_produce(mhw::temporal_bindings::NativeFrame *f){mhw::continuous_dlaa::native(2,f);}
extern "C" void mhw_temporal_depth_consume(mhw::temporal_bindings::NativeFrame *f){mhw::continuous_dlaa::native(3,f);}
extern "C" void mhw_temporal_jitter(mhw::temporal_bindings::NativeFrame *f){mhw::continuous_dlaa::native(4,f);}
namespace mhw::continuous_dlaa {
namespace {
template<auto Detour,unsigned I> struct Chained;
template<class R,class... A,R(WINAPI *Detour)(A...),unsigned I> struct Chained<Detour,I> {
    static R WINAPI call(A... a){
        if(chain_live.load(std::memory_order_acquire))return Detour(a...);
        return reinterpret_cast<R(WINAPI *)(A...)>(originals[I])(a...);
    }
};
struct ChainedMethod {unsigned index;astera::Family family;unsigned slot;void *detour;};
const ChainedMethod chained_methods[]{
    {5,astera::command_list,10,reinterpret_cast<void *>(&Chained<&reset_hook,5>::call)},
    {6,astera::command_list,9,reinterpret_cast<void *>(&Chained<&close_hook,6>::call)},
    {7,astera::queue,10,reinterpret_cast<void *>(&Chained<&execute_hook,7>::call)},
    {8,astera::command_list,26,reinterpret_cast<void *>(&Chained<&barrier_hook,8>::call)},
    {9,astera::command_list,14,reinterpret_cast<void *>(&Chained<&dispatch_hook,9>::call)},
    {10,astera::command_list,17,reinterpret_cast<void *>(&Chained<&copy_hook,10>::call)},
#ifdef MHW_CONTINUOUS_SCALED
    {11,astera::command_list,12,reinterpret_cast<void *>(&Chained<&draw_hook,11>::call)},
    {12,astera::command_list,13,reinterpret_cast<void *>(&Chained<&indexed_hook,12>::call)},
#endif
};
static_assert(std::size(chained_methods)==hook_count-first_api_hook,"every D3D12 hook is chained");
}
void astera_hooks(std::vector<astera::Hook> &out){
    for(const auto &m:chained_methods)out.push_back({astera::method(m.family,m.slot),m.detour,&originals[m.index]});
}
}
