#include "probe.hpp"
#ifdef MHW_SCALED_OUTPUT
#include "../scaled_temporal/output_route.hpp"
#endif
#include "owned_inputs.hpp"
#ifdef MHW_TEMPORAL_INLINE
#include "inline_pass.hpp"
#endif
#include "shader.hpp"
#include "consumer.hpp"
#include "submission.hpp"
#include "jitter.hpp"
#include "depth_routes.hpp"
#include "../projection_binding/memory.hpp"
#ifdef MHW_TEMPORAL_SCALED
#include "../render_resolution/runtime.hpp"
#include "../continuous_dlaa/activation.hpp"
#include "../scaled_temporal/handoff.hpp"
#endif
#include <windows.h>
#include <d3d12.h>
#include <MinHook.h>
#include <atomic>
#include <ostream>
#include <algorithm>
#include <cstring>
#ifdef MHW_TEMPORAL_PRESENT_STATE
#include "../temporal_present/observer.hpp"
#endif
extern "C" void temporal_dispatch_entry();
extern "C" void mhw_binding_forward(void *,mhw::projection_binding::Frame *);
namespace mhw::projection_binding {bool discover(std::uintptr_t,Runtime &,std::string &);}
namespace mhw::temporal_bindings {
namespace {
namespace pb=projection_binding;
struct Resource {std::uintptr_t identity{};D3D12_RESOURCE_DESC desc{};ID3D12Resource *held{};};
struct Record {
#ifdef MHW_TEMPORAL_SCALED
  render_resolution::InputStamp scale_before,scale_after;
#endif
  Snapshot before,after;Packet queued;std::array<Resource,roles> resources;
  SceneDepth depth,depth_after;Resource depth_resource;bool depth_stable{};
  ShaderRecipe shader,shader_after;bool shader_stable{};
  unsigned thread{},count_before{},count_after{},x{},y{},z{};
  std::uint64_t enter{},exit{},producer_order{};std::uintptr_t caller{},array_before{},array_after{};
  bool returned{},valid{};
};
Config cfg;pb::Runtime runtime;
std::array<Record,capacity> records;
std::atomic<unsigned> state{},pending{},fault{},count{};
std::atomic_flag busy=ATOMIC_FLAG_INIT;
void *original{};std::uintptr_t target{};std::uint64_t started{};
bool installed{},disabled{};unsigned acquired{},released{};
bool consumer_enabled{},jitter_enabled{},scene_enabled{},routes_enabled{};
std::uint64_t drain_started_tick{},drain_finished_tick{},journal_stopped_tick{},unhook_started_tick{},finish_tick{};
bool callbacks_drained{},journal_stopped_before_unhook{};
#ifdef MHW_TEMPORAL_FIXTURE
void (*before_unhook)(){};
void (*before_drain)(){};
#endif
std::array<std::byte,shader_limit> shader_bytes;
unsigned shader_size{};bool shader_saved{};
bool read(void *,std::uintptr_t at,std::span<std::byte> out) {
  return bridge_probe::read_bounded_scalar(nullptr,at,out)==bridge_probe::ScalarReadResult::success;
}
const Reader reader{nullptr,read};
bool same_bytes(const ShaderRecipe &s) {
  if(s.size!=shader_size)return false;
  std::array<std::byte,512> scratch;
  for(unsigned offset=0;offset<s.size;) {
    const auto n=(std::min)(512U,s.size-offset);
    if(!pb::copy_memory(s.bytecode+offset,scratch.data(),n) || std::memcmp(scratch.data(),shader_bytes.data()+offset,n))return false;
    offset+=n;
  }
  return true;
}
bool capture_shader(Record &r) {
  if(!shader_recipe(reader,r.before,r.shader))return false;
  if(!shader_size) {
    if(!pb::copy_memory(r.shader.bytecode,shader_bytes.data(),r.shader.size) || !compute_dxbc(shader_bytes.data(),r.shader.size))return false;
    shader_size=r.shader.size;
  }
  ShaderRecipe stable;
  return same_bytes(r.shader) && shader_recipe(reader,r.before,stable) && stable==r.shader;
}
void fail(unsigned f) {unsigned zero{};fault.compare_exchange_strong(zero,f);state.store(2,std::memory_order_release);}
bool methods(std::uintptr_t p,std::initializer_list<unsigned> slots) {
  for(const auto slot:slots){std::uintptr_t address{};if(!pb::system_method(p,slot,address))return false;}return true;
}
bool release(IUnknown *p) noexcept {
  if(!p)return true;
  if(!methods(reinterpret_cast<std::uintptr_t>(p),{2}))return false;
  __try{p->Release();return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool pin(std::uintptr_t object,Resource &out) noexcept {
  ID3D12Resource *r{};ID3D12Device *device{};IUnknown *identity{};bool ok=false;
  if(!methods(object,{0,2,7,10}))return false;
  __try {
    auto p=reinterpret_cast<ID3D12Resource *>(object);
    if(SUCCEEDED(p->QueryInterface(IID_PPV_ARGS(&r))) && reinterpret_cast<std::uintptr_t>(r)==object &&
       SUCCEEDED(r->GetDevice(IID_PPV_ARGS(&device))) && reinterpret_cast<std::uintptr_t>(device)==cfg.device &&
       SUCCEEDED(r->QueryInterface(IID_PPV_ARGS(&identity))) && identity) {
      out.identity=reinterpret_cast<std::uintptr_t>(identity);out.desc=r->GetDesc();
      ok=out.desc.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D && out.desc.Width && out.desc.Width<=16384 &&
        out.desc.Height && out.desc.Height<=16384 && out.desc.DepthOrArraySize && out.desc.MipLevels && out.desc.SampleDesc.Count;
    }
  }__except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
  const bool a=release(identity),b=release(device);ok=ok&&a&&b;
  if(ok){out.held=r;++acquired;}else if(!release(r))fail(9);
  return ok;
}
unsigned before(NativeFrame &f) noexcept {
  if(state.load(std::memory_order_acquire)!=1 || f.call.caller!=cfg.image+caller_rva)return UINT_MAX;
  if(!submission::warmed())return UINT_MAX;
  if(routes_enabled && !depth_routes::wants())return UINT_MAX;
  if(count.load(std::memory_order_acquire)>=capacity)return UINT_MAX;
  if(jitter_enabled && !jitter::wanted(count.load(std::memory_order_acquire)))return UINT_MAX;
  pending.fetch_add(1,std::memory_order_acq_rel);
  if(state.load(std::memory_order_acquire)!=1){pending.fetch_sub(1,std::memory_order_release);return UINT_MAX;}
  if(busy.test_and_set(std::memory_order_acquire)){fail(1);pending.fetch_sub(1,std::memory_order_release);return UINT_MAX;}
  const auto i=count.load();
  if(i>=capacity || GetTickCount64()-started>=2000){fail(2);busy.clear(std::memory_order_release);pending.fetch_sub(1,std::memory_order_release);return UINT_MAX;}
  auto &r=records[i];r.enter=GetTickCount64();r.thread=GetCurrentThreadId();r.caller=f.call.caller;
  r.x=static_cast<unsigned>(f.call.gpr[2]);r.y=static_cast<unsigned>(f.call.gpr[3]);r.z=static_cast<unsigned>(f.call.gpr[4]);
  r.valid=!f.call.stack[0] && !static_cast<unsigned char>(f.call.stack[1]) && snapshot(reader,cfg,f.layer,f.call.gpr[1],r.before) &&
    reader.get(r.before.context+0x2b80,r.count_before) && r.count_before<65535 &&
    reader.get(r.before.context+0x2b78,r.array_before) && r.array_before;
  if(r.valid && i)r.valid=r.before.frame>records[i-1].before.frame;
#ifdef MHW_TEMPORAL_SCALED
  if(r.valid)r.valid=render_resolution::input_stamp(r.before.view,r.scale_before)&&r.scale_before.frame==r.before.frame;
#endif
  if(r.valid && jitter_enabled)r.valid=jitter::matches(i,r.before);
  if(r.valid)r.valid=capture_shader(r);
  for(unsigned n=0;r.valid && n<roles;++n)r.valid=pin(r.before.inputs[n].resource,r.resources[n]);
  if(r.valid && scene_enabled)r.valid=scene_depth(reader,r.before,r.depth) && pin(r.depth.texture.resource,r.depth_resource);
  if(!r.valid)fail(3);
  if(r.valid && consumer_enabled)r.producer_order=gpu::publication_begin();
  return i;
}
void after(unsigned i,bool returned) noexcept {
  if(i==UINT_MAX)return;
  auto &r=records[i];r.returned=returned;
  std::uintptr_t object{},stable_array{},stable_object{};unsigned stable_count{};Packet again;
  r.valid=r.valid && returned && snapshot(reader,cfg,r.before.layer,r.before.context,r.after) && r.before==r.after &&
    reader.get(r.before.context+0x2b80,r.count_after) && r.count_after==r.count_before+1 &&
    reader.get(r.before.context+0x2b78,r.array_after) && r.array_after && r.array_after<=UINTPTR_MAX-16ULL*r.count_after &&
    reader.get(r.array_after+16ULL*(r.count_after-1)+8,object) && packet(reader,object,r.queued) &&
    packet_matches(r.before,r.queued,r.x,r.y,r.z) &&
    reader.get(r.before.context+0x2b80,stable_count) && stable_count==r.count_after &&
    reader.get(r.before.context+0x2b78,stable_array) && stable_array==r.array_after &&
    reader.get(stable_array+16ULL*(stable_count-1)+8,stable_object) && stable_object==object &&
    packet(reader,object,again) && again==r.queued;
  r.shader_stable=r.valid && shader_recipe(reader,r.after,r.shader_after) && r.shader==r.shader_after && same_bytes(r.shader_after);
#ifdef MHW_TEMPORAL_SCALED
  if(r.valid)r.valid=render_resolution::input_stamp(r.after.view,r.scale_after)&&r.scale_before==r.scale_after;
#endif
  r.valid=r.valid && r.shader_stable;
  if(scene_enabled){r.depth_stable=r.valid && scene_depth(reader,r.after,r.depth_after) && r.depth==r.depth_after;r.valid=r.valid && r.depth_stable;}
  r.exit=GetTickCount64();r.valid=r.valid && r.exit-started<2000;
  if(!r.valid)fail(returned?4:5);
  if(r.valid && consumer_enabled && !gpu::publish(i,r.before,r.queued,r.shader,r.resources[5].identity,r.resources[0].identity,r.producer_order,scene_enabled?&r.depth:nullptr)){r.valid=false;fail(12);}
  if(r.valid && jitter_enabled && !jitter::acknowledge(i,r.before)){r.valid=false;fail(14);}
  if(r.valid && routes_enabled && !depth_routes::select(i,r.before,r.depth,r.queued)){r.valid=false;fail(15);}
#ifdef MHW_TEMPORAL_SCALED
  if(r.valid && scaled_temporal::handoff::active() && !scaled_temporal::handoff::publish(i,r.before)){r.valid=false;fail(19);}
#endif
  count.store(i+1,std::memory_order_release);
  if(i+1==capacity && !consumer_enabled)state.store(2,std::memory_order_release);
  busy.clear(std::memory_order_release);pending.fetch_sub(1,std::memory_order_release);
}
}
void dispatch(NativeFrame *f) {
  const auto i=before(*f);bool returned=false;
  __try {mhw_binding_forward(original,&f->call);returned=true;}
  __finally {after(i,returned);}
}
#ifdef MHW_TEMPORAL_FIXTURE
void fixture_start(const Config &c,void *forward,bool consumer,bool with_jitter,bool scene,bool routes) {
  cfg=c;original=forward;consumer_enabled=consumer;jitter_enabled=with_jitter;scene_enabled=scene;routes_enabled=routes;started=GetTickCount64();
#ifdef MHW_TEMPORAL_PRESENT_STATE
  temporal_present::observer::begin();
#endif
  if(submission::active())submission::begin(started);if(consumer_enabled)gpu::begin(started);if(routes_enabled)depth_routes::begin(started);state.store(1,std::memory_order_release);
}
#endif
bool install(std::uintptr_t image,std::string &reason) {
  if(installed || state.load()!=0){reason="already-used";return false;}
  if(!pb::discover(image,runtime,reason) ||
     !pb::verify_code(image,0x2392150,4221,"d2c6c0f6a5ed39c998bcf62afed19132e6dd0d7cfbb19e18b80c15531af2bf91") ||
     !pb::verify_code(image,dispatch_rva,321,"d073b70ae437d281596861720893c3b3cc9f82155fa0c83f2c3ae6918fd2f3a2") ||
     !pb::verify_code(image,0x257ca70,8,"5f55ddcc3069359da7b5df1443cb44504c3c71a47126a3362a58dc9f0a469771") ||
     !pb::verify_code(image,0x255ce10,15,"1aeeca798d67b2352fa2c1dcfd68361fd34aff602c35ba18877acf6aaf0bc809") ||
     !pb::verify_code(image,0x23d0830,518,"652891ad67f47d632a5cfbc9dc5d0a4356075d22f875bf4ebf872f16c8bd1c97") ||
     !pb::verify_code(image,0x23acc10,228,"a44070c5139f8781f6142f1ae2b4710efe8f284fd3b52656c6543e1bf9f6c734") ||
     !pb::verify_code(image,0x23a3bb0,107,"ea53238830e5665c24a082f0a70e543147c44698daffb1bff576d8c3c13fe041") ||
     !pb::verify_code(image,0x259cc20,1283,"6921c7cbfc7b277f4b076a68de8165bd54b3f6571ca574fdb16d5c35cbe154da") ||
     !pb::verify_code(image,0x259f610,924,"936ac61d6575029672cd657bc8956e63f00d8ac50df2ede414d8c93dcf886bb5") ||
     !pb::verify_code(image,0x23d0130,126,"467232d83007c7713430d1ae05c38ed6013f86afe8185f5577cd4a9936394053")) {
    if(reason.empty())reason="temporal-live-code";return false;
  }
  cfg.image=image;cfg.backend=runtime.backend;cfg.device=runtime.device;
  if(!reader.get(image+0x51c4480,cfg.renderer) || !cfg.renderer){reason="renderer";return false;}
  if(!owned::prepare(reinterpret_cast<ID3D12Device *>(cfg.device),runtime.width,runtime.height,reason))return false;
#ifdef MHW_TEMPORAL_PRESENT_STATE
  if(!temporal_present::observer::install(reinterpret_cast<ID3D12Device *>(cfg.device),pb::system_method,reason
#ifdef MHW_SCALED_OUTPUT
    ,true,render_resolution::observe_viewports
#endif
    ))return false;
#ifdef MHW_SCALED_OUTPUT
  if(!scaled_temporal::output_route::install(cfg,reason))return false;
#endif
#endif
  consumer_enabled=true;if(!submission::install(cfg,reason) || !gpu::install(cfg,reason))return false;
  if(!gpu::prepare_motion(runtime.width,runtime.height,reason,true))return false;
  routes_enabled=true;
#ifdef MHW_TEMPORAL_SCALED
  if(!depth_routes::install(cfg,reason,true))return false;
#else
  if(!depth_routes::install(cfg,reason))return false;
#endif
  if(!depth_routes::prepare_pixels(runtime.width,runtime.height,reason))return false;
  scene_enabled=true;jitter_enabled=false;
#ifdef MHW_TEMPORAL_SCALED
  jitter_enabled=true;if(!jitter::prepare(cfg,reason,continuous_dlaa::activation::owns_projection))return false;
  if(!scaled_temporal::handoff::install(cfg,reason))return false;
#elif defined(MHW_TEMPORAL_DLSS)
  jitter_enabled=true;if(!jitter::prepare(cfg,reason))return false;
#endif
  target=image+dispatch_rva;
  const auto created=MH_CreateHook(reinterpret_cast<void *>(target),reinterpret_cast<void *>(temporal_dispatch_entry),&original);
  if(created!=MH_OK){reason="create-hook-"+std::to_string(created);return false;}
  started=GetTickCount64();
#ifdef MHW_TEMPORAL_PRESENT_STATE
  temporal_present::observer::begin();
#endif
  submission::begin(started);gpu::begin(started);depth_routes::begin(started);state.store(1,std::memory_order_release);
  const auto enabled=MH_EnableHook(reinterpret_cast<void *>(target));
  if(enabled!=MH_OK){state.store(2);reason="enable-hook-"+std::to_string(enabled);return false;}
  installed=true;
  if(jitter_enabled && !jitter::begin(started,reason))return false;
  return true;
}
bool done() noexcept {
  if(state.load()==1 && GetTickCount64()-started>=2000)fail(6);
  submission::poll();if(!submission::healthy())fail(13);
  if(consumer_enabled && !gpu::healthy())fail(12);
  if(jitter_enabled && !jitter::healthy())fail(14);
  if(routes_enabled && !depth_routes::healthy())fail(15);
#ifdef MHW_TEMPORAL_SCALED
  if(!scaled_temporal::handoff::healthy())fail(19);
#ifdef MHW_SCALED_OUTPUT
  if(!scaled_temporal::output_route::healthy())fail(20);
  if(!scaled_temporal::output_route::complete())return state.load()==2;
#endif
  if(scaled_temporal::handoff::active() && !scaled_temporal::handoff::complete())return state.load()==2;
#endif
#ifdef MHW_TEMPORAL_PRESENT_STATE
  if(!temporal_present::observer::healthy())fail(18);
#endif
  if(consumer_enabled && count.load(std::memory_order_acquire)==capacity && gpu::complete() && submission::complete() && (!jitter_enabled || jitter::complete()))state.store(2,std::memory_order_release);
  return state.load()==2;
}
#ifdef MHW_TEMPORAL_FIXTURE
void fixture_before_unhook(void (*callback)()) noexcept {before_unhook=callback;}
void fixture_before_drain(void (*callback)()) noexcept {before_drain=callback;}
#endif
// Stop producer/consumer admission first. Admitted callbacks still need the
// shared barrier hooks and submission journal until they have drained.
void cancel() noexcept {state.store(2,std::memory_order_release);jitter::stop();depth_routes::stop();gpu::stop();
#ifdef MHW_TEMPORAL_SCALED
  scaled_temporal::handoff::stop();
#endif
#ifdef MHW_TEMPORAL_PRESENT_STATE
  temporal_present::observer::stop();
#endif
}
bool settled() noexcept {return pending.load(std::memory_order_acquire)==0 && (!consumer_enabled || gpu::settled()) && submission::settled() && (!jitter_enabled || jitter::settled()) && (!routes_enabled || depth_routes::settled())
#ifdef MHW_TEMPORAL_SCALED
  && scaled_temporal::handoff::settled()
#endif
#ifdef MHW_TEMPORAL_PRESENT_STATE
  && temporal_present::observer::settled()
#endif
  ;}
bool finish() noexcept {
  cancel();
  // No hook removal until admitted producers and dependent consumers have
  // drained. Direct D3D callbacks have their own pending count and may finish
  // after admission closes; submission::finish retains/drains those separately.
  const auto dependencies_settled=[](){return pending.load(std::memory_order_acquire)==0 &&
    (!consumer_enabled || gpu::settled()) && (!jitter_enabled || jitter::settled()) &&
    (!routes_enabled || depth_routes::settled())
#ifdef MHW_TEMPORAL_SCALED
    && scaled_temporal::handoff::settled()
#endif
#ifdef MHW_TEMPORAL_PRESENT_STATE
    && temporal_present::observer::settled()
#endif
    ;};
  drain_started_tick=GetTickCount64();
#ifdef MHW_TEMPORAL_FIXTURE
  if(before_drain)before_drain();
#endif
  while(!dependencies_settled() && GetTickCount64()-drain_started_tick<2000)Sleep(1);
  drain_finished_tick=GetTickCount64();
  callbacks_drained=dependencies_settled() && drain_finished_tick-drain_started_tick<=2000;
  if(!callbacks_drained)fail(17);
  submission::stop();journal_stopped_tick=submission::cutoff_tick();
  journal_stopped_before_unhook=!submission::admission_open();
  unhook_started_tick=GetTickCount64();
#ifdef MHW_TEMPORAL_FIXTURE
  if(before_unhook)before_unhook();
#endif
  const auto status=installed?MH_DisableHook(reinterpret_cast<void *>(target)):MH_ERROR_DISABLED;
  disabled=status==MH_OK || status==MH_ERROR_DISABLED;
  const bool jitter_ok=!jitter_enabled || jitter::finish(false);
  const bool routes_ok=!routes_enabled || depth_routes::finish(false);
  const bool consumer_ok=!consumer_enabled || gpu::finish(false);
#ifdef MHW_TEMPORAL_SCALED
  const bool handoff_ok=scaled_temporal::handoff::finish();
  if(!handoff_ok)fail(19);
#endif
#ifdef MHW_TEMPORAL_INLINE
  ID3D12Fence *completed_fence{};
  const bool submission_ok=submission::finish(&completed_fence,inline_pass::selected_receipt);
  inline_pass::completion(completed_fence);
#else
  const bool submission_ok=submission::finish();
#endif
#ifdef MHW_TEMPORAL_PRESENT_STATE
  const bool bindings_ok=temporal_present::observer::finish();
  if(!bindings_ok)fail(18);
#endif
  finish_tick=GetTickCount64();
  if(pending.load(std::memory_order_acquire) || (jitter_enabled && !jitter::settled()))return false;
  if(consumer_enabled && !gpu::settled())return false;
  if(routes_enabled && !depth_routes::settled())return false;
#ifdef MHW_TEMPORAL_SCALED
  if(!scaled_temporal::handoff::settled())return false;
#endif
#ifdef MHW_TEMPORAL_PRESENT_STATE
  if(!temporal_present::observer::settled())return false;
#endif
  for(auto &r:records)for(auto &v:r.resources)if(v.held) {
    if(release(v.held)){v.held=nullptr;++released;}else fail(9);
  }
  for(auto &r:records)if(r.depth_resource.held){if(release(r.depth_resource.held)){r.depth_resource.held=nullptr;++released;}else fail(9);}
  const bool ok=callbacks_drained && journal_stopped_before_unhook && disabled && !fault.load() && count.load()==capacity && acquired==released && consumer_ok && submission_ok && jitter_ok && routes_ok;
  const bool pair_ok=gpu::finish_motion(ok);const bool depth_ok=depth_routes::finish_pixels(ok);return pair_ok && depth_ok && ok;
}
bool save_shader(const std::wstring &path) {
  if(!settled() || !disabled || fault.load() || count.load()!=capacity || !shader_size || shader_saved ||
     (consumer_enabled && !gpu::complete()))return false;
  const auto file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file==INVALID_HANDLE_VALUE)return false;
  DWORD written{};const bool ok=WriteFile(file,shader_bytes.data(),shader_size,&written,nullptr) && written==shader_size && FlushFileBuffers(file);
  const bool closed=CloseHandle(file)!=0;shader_saved=ok && closed;return shader_saved;
}
void write(std::ostream &o) {
  o<<"{\"event\":\"capture\",\"schema\":1,\"started_tick\":"<<started<<",\"graphics_api\":\"D3D12\",\"adapter_luid_low\":"<<runtime.luid_low
   <<",\"adapter_luid_high\":"<<runtime.luid_high<<",\"swapchain_format\":"<<runtime.format
   <<",\"width\":"<<runtime.width<<",\"height\":"<<runtime.height<<",\"fault\":"<<fault.load()
   <<",\"renderer\":"<<cfg.renderer<<",\"backend\":"<<cfg.backend<<",\"device\":"<<cfg.device
   <<",\"pending\":"<<pending.load()<<",\"hooks_disabled\":"<<disabled<<",\"pins_acquired\":"<<acquired<<",\"pins_released\":"<<released
   <<",\"shader_saved\":"<<shader_saved<<",\"shader_bytes\":"<<shader_size<<",\"shader_sha256\":\""<<(shader_size?pb::bytes_sha256(shader_bytes.data(),shader_size):"")
   <<"\",\"typed_views_verified\":false,\"gpu_consumption_verified\":false,\"motion_contents_verified\":false,\"records\":[";
  for(unsigned i=0;i<(std::min)(count.load(),capacity);++i) {
    if(i)o<<',';const auto &r=records[i];const auto &s=r.before;
    o<<"{\"valid\":"<<r.valid<<",\"returned\":"<<r.returned<<",\"snapshots_equal\":"<<(r.before==r.after)<<",\"thread\":"<<r.thread<<",\"caller\":"<<r.caller
     <<",\"enter_tick\":"<<r.enter<<",\"exit_tick\":"<<r.exit<<",\"producer_order\":"<<r.producer_order<<",\"frame\":"<<s.frame<<",\"epoch\":"<<s.epoch
     <<",\"bank\":"<<s.bank<<",\"async\":"<<s.async<<",\"context\":"<<s.context<<",\"view\":"<<s.view
     <<",\"layer\":"<<s.layer<<",\"gbuffer\":"<<s.gbuffer<<",\"technique\":"<<s.technique<<",\"taa_bytes\":"<<s.taa_bytes
     <<",\"count_before\":"<<r.count_before<<",\"count_after\":"<<r.count_after<<",\"array_before\":"<<r.array_before<<",\"array_after\":"<<r.array_after
     <<",\"packet\":"<<r.queued.object<<",\"state\":"<<s.state<<",\"packet_state\":"<<r.queued.state
     <<",\"opcode\":"<<r.queued.opcode<<",\"indirect\":"<<r.queued.indirect<<",\"mode\":"<<r.queued.mode
     <<",\"groups\":["<<r.x<<','<<r.y<<','<<r.z<<"],\"packet_groups\":["<<r.queued.x<<','<<r.queued.y<<','<<r.queued.z<<"],\"rectangle\":[";
    for(unsigned j=0;j<4;++j){if(j)o<<',';o<<s.rectangle[j];}o<<"]";
#ifdef MHW_TEMPORAL_SCALED
    const auto &scale=r.scale_before;
    o<<",\"scale_state\":{\"stable\":"<<(r.scale_before==r.scale_after)<<",\"frame\":"<<scale.frame<<",\"view\":"<<scale.view
     <<",\"display\":["<<scale.display_width<<','<<scale.display_height<<"],\"render\":["<<scale.render_width<<','<<scale.render_height
     <<"],\"active\":"<<scale.active_scale<<",\"content\":"<<scale.content_scale<<"}";
#endif
    o<<",\"inputs\":[";
    for(unsigned j=0;j<roles;++j) {
      if(j)o<<',';const auto &v=s.inputs[j];const auto &a=r.resources[j];const auto &d=a.desc;
      o<<"{\"name\":\""<<names[j]<<"\",\"id\":"<<v.id<<",\"owner\":"<<v.object<<",\"type\":"<<v.type
       <<",\"kind\":"<<v.kind<<",\"engine_flags\":"<<v.flags<<",\"engine_width\":"<<v.width<<",\"engine_height\":"<<v.height
       <<",\"engine_format\":"<<v.format<<",\"gpu_format\":"<<v.gpu_format<<",\"primary\":"<<v.primary<<",\"secondary\":"<<v.secondary
       <<",\"backing\":"<<v.backing<<",\"resource\":"<<v.resource<<",\"identity\":"<<a.identity
       <<",\"dimension\":"<<d.Dimension<<",\"width\":"<<d.Width<<",\"height\":"<<d.Height<<",\"array_size\":"<<d.DepthOrArraySize
       <<",\"mips\":"<<d.MipLevels<<",\"format\":"<<d.Format<<",\"sample_count\":"<<d.SampleDesc.Count<<",\"sample_quality\":"<<d.SampleDesc.Quality
       <<",\"layout\":"<<d.Layout<<",\"alignment\":"<<d.Alignment<<",\"flags\":"<<d.Flags<<'}';
    }
    const auto &sh=r.shader;
    o<<"],\"shader\":{\"stable\":"<<r.shader_stable<<",\"handle\":"<<sh.handle<<",\"set\":"<<sh.set<<",\"entry\":"<<sh.entry
     <<",\"descriptor\":"<<sh.descriptor<<",\"bytecode\":"<<sh.bytecode<<",\"bytes\":"<<sh.size<<",\"technique\":"<<sh.technique
     <<",\"ids\":"<<sh.ids<<",\"modes\":"<<sh.modes<<",\"words\":"<<sh.words<<",\"binding_start\":"<<sh.binding_start<<",\"owners\":[";
    for(unsigned j=0;j<sh.owner_count && j<owner_limit;++j){if(j)o<<',';const auto &v=sh.owners[j];
      o<<"{\"id\":"<<v.id<<",\"owner\":"<<v.object<<",\"mode\":"<<v.mode<<'}';}
    o<<"],\"bindings\":[";
    for(unsigned j=0;j<sh.binding_count && j<binding_limit;++j){if(j)o<<',';const auto &v=sh.bindings[j];
      o<<"{\"word\":"<<v.word<<",\"view\":"<<v.view<<",\"cpu_descriptor\":"<<v.cpu_descriptor<<'}';}
    o<<"]}";
    if(scene_enabled){const auto &sd=r.depth;const auto &v=sd.texture;const auto &a=r.depth_resource;const auto &d=a.desc;
      o<<",\"scene_depth\":{\"stable\":"<<r.depth_stable<<",\"container\":"<<sd.container
       <<",\"srv_readable\":"<<sd.srv_readable<<",\"srv\":"<<sd.srv<<",\"cpu_descriptor\":"<<sd.cpu_descriptor
       <<",\"attachment_readable\":"<<sd.attachment_readable<<",\"attachment\":"<<sd.attachment
       <<",\"attachment_owner\":"<<sd.attachment_owner<<",\"attachment_resource\":"<<sd.attachment_resource
       <<",\"owner\":"<<v.object<<",\"type\":"<<v.type<<",\"kind\":"<<v.kind<<",\"engine_flags\":"<<v.flags
       <<",\"engine_width\":"<<v.width<<",\"engine_height\":"<<v.height<<",\"engine_format\":"<<v.format<<",\"gpu_format\":"<<v.gpu_format
       <<",\"primary\":"<<v.primary<<",\"secondary\":"<<v.secondary<<",\"backing\":"<<v.backing
       <<",\"resource\":"<<v.resource<<",\"identity\":"<<a.identity<<",\"dimension\":"<<d.Dimension
       <<",\"width\":"<<d.Width<<",\"height\":"<<d.Height<<",\"array_size\":"<<d.DepthOrArraySize<<",\"mips\":"<<d.MipLevels
       <<",\"format\":"<<d.Format<<",\"sample_count\":"<<d.SampleDesc.Count<<",\"sample_quality\":"<<d.SampleDesc.Quality
       <<",\"layout\":"<<d.Layout<<",\"alignment\":"<<d.Alignment<<",\"flags\":"<<d.Flags<<'}';
    }
    o<<'}';
  }
  o<<"],\"shutdown\":{\"callbacks_drained\":"<<callbacks_drained<<",\"journal_stopped_before_unhook\":"<<journal_stopped_before_unhook
   <<",\"drain_started_tick\":"<<drain_started_tick<<",\"drain_finished_tick\":"<<drain_finished_tick<<",\"journal_stopped_tick\":"<<journal_stopped_tick
   <<",\"unhook_started_tick\":"<<unhook_started_tick<<",\"finish_tick\":"<<finish_tick<<'}';
  if(consumer_enabled){o<<",\"native_compute\":";gpu::write(o);}
  if(submission::active()){o<<",\"submission\":";submission::write(o);}
  if(routes_enabled){o<<",\"depth_routes\":";depth_routes::write(o);}
  o<<",\"motion_readback\":";gpu::write_motion(o);o<<",\"color_readback\":";gpu::write_color(o);
  o<<",\"depth_readback\":";depth_routes::write_pixels(o);
  o<<",\"owned_gpu_inputs\":";owned::write(o);
#ifdef MHW_TEMPORAL_SCALED
  if(scaled_temporal::handoff::active()){o<<",\"final_handoff\":";scaled_temporal::handoff::write(o);}
#endif
#ifdef MHW_TEMPORAL_INLINE
  o<<",\"inline_dlaa\":";inline_pass::write(o);
#endif
  if(jitter_enabled){o<<",\"projection_stimulus\":";jitter::write(o);}
#ifdef MHW_TEMPORAL_PRESENT_STATE
  o<<",\"native_bindings\":";temporal_present::observer::write(o);
#endif
  o<<"}\n";
}
}
extern "C" void mhw_temporal_dispatch(mhw::temporal_bindings::NativeFrame *f) {mhw::temporal_bindings::dispatch(f);}
