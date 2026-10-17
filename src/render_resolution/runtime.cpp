#include "runtime.hpp"
#include "control.hpp"
#include "../astera/astera_hooks.hpp"
#include "../projection_binding/memory.hpp"
#include "../perf/toggles.hpp"
#include <windows.h>
#include <d3d12.h>
#include <MinHook.h>
#include <intrin.h>
#include <atomic>
#include <array>
#include <iomanip>
#include <algorithm>
#include <ostream>
#include <string>

// Shared worker-side COM discovery; no projection recorder hooks are enabled.
namespace mhw::projection_binding {bool discover(std::uintptr_t,Runtime &,std::string &);}
extern "C" void mhw_resolution_entry();
extern "C" void mhw_binding_forward(void *,mhw::projection_binding::Frame *);
namespace mhw::render_resolution {
namespace {
namespace pb=projection_binding;
using Setter=void (*)(std::uintptr_t,float);
using Viewport=void (STDMETHODCALLTYPE *)(ID3D12GraphicsCommandList *,UINT,const D3D12_VIEWPORT *);
pb::Runtime discovered;
std::uintptr_t image{},renderer{},expected_return{};
std::array<std::uintptr_t,2> targets{};
void *original{};
// v0.29.0: the renderer update on al.hooks under AL; its first 16 original
// bytes (clean image; gate 0x229ed30 in sizing_code()). The stub reads its
// caller, so the hook runs first in its chain (OwnedHook).
constexpr char update_head[]="40 53 48 83 EC 20 80 B9 F4 B3 07 00 00 48 8B D9";
static_assert(astera::valid_pattern<16>(update_head),"the renderer update's pattern is its first 16 bytes");
astera::OwnedHook update_hook{"mhw_dlss Render:Update",update_head};
Viewport original_viewport{};
Setter setter{};
// The planned extent. The worker replaces it (install, retarget) under the
// lock; the render thread and the input checks take one copy per use.
Plan target;
unsigned basis_width{};  // the plan's basis: the scene width, or the window's (aspect option)
Ownership ownership;
std::atomic<bool> requested{},restored{},hooks_disabled{true};
// The player's DLSS Off (yield()): the game's own scale, until resume() or a
// rebase. Unqualified visits are the game's own business then, except the
// aspect switch, which is noted for the rebase.
std::atomic<bool> yielded{};
// Set by resume(): the game's own scale was the player's while yielded, so it
// is adopted as the native value instead of the start-up check (visits <= 32).
bool adopted{};
std::atomic<unsigned> pending{},fault{};
unsigned render_thread{};
bool shared_viewport{};
std::uint64_t visits{},unqualified{},overrides{},pre_restores{},foreign_requests{},scaled_observed{},restored_observed{};
std::array<std::uint64_t,3> viewport_counts{}; // display / reduced / other
std::uint64_t viewport_unreadable{};
SRWLOCK mutex=SRWLOCK_INIT;
struct Lock {Lock(){AcquireSRWLockExclusive(&mutex);}~Lock(){ReleaseSRWLockExclusive(&mutex);}};
struct Snapshot {
    std::uint64_t tick{},frame{};
    std::uintptr_t view{};
    unsigned width{},height{},view_flags{},depth_width{},depth_height{},width_override{},basis_width{},basis_height{};
    float desired{},applied{},previous{},content_desired{},content{},combined{},view_scale{},view_content{},view_combined{};
    bool depth_metadata{};
    // Which snapshot check failed (0 = qualified; see snapshot()), with the
    // renderer's sizing word and whether a backend wrapper was present.
    unsigned check{},sizing{};bool wrapper{};
    // The live aspect change (docs/research/live-aspect.md, clean image):
    // sRender's window size (+0x1f448), the aspect mode requested and applied
    // (+0x7b440, +0x7b43c; 0 window, 1 16:9, 2 64:27; 0x229ef20 applies it and
    // writes the scene size +0x198), its pending-change flags (+0x1f548; 4 is
    // the aspect, consumed by 0x229b020 on the game thread) and the scene
    // rectangle in the window (+0x188, 0x229f090).
    unsigned window_width{},window_height{},aspect_requested{},aspect_applied{},flags{};
    std::array<int,4> rect{};
};
Snapshot baseline{},reduced{},last{},restoration{};
// The startup admission snapshot, and the first and last unqualified
// in-session snapshots (an aspect-ratio change applies live, game-15).
Snapshot startup{},first_unqualified{},last_unqualified{};
// Views whose own scale is the truncated render/display ratio (1706/3413 for a
// pillarboxed odd scene) rather than the requested 0.5. The title-screen view
// keeps it (game-22, game-23) while rendering the same 1706x720 viewport, so it
// counts as the planned scale; the input checks still verify the rectangle.
std::uint64_t truncated_views{};Snapshot first_truncated{};
float truncated_ratio(const Plan &t) noexcept {return static_cast<float>(t.render.width)/static_cast<float>(t.display.width);}
// The view scale of the planned extent: the quantized request, or its
// truncated ratio (identical, 0.5, for 2:1 scenes).
bool planned_view_scale(const Plan &t,float value) noexcept {return value==t.quantized||value==truncated_ratio(t);}
// Aspect-change capture (part B research). A visit is notable when the
// requested and applied aspect differ, the scene or window size differs from
// the previous visit's, the sizing bit is set, or it is the first refused
// snapshot. Each capture holds the 16 visits before its first notable visit
// and the 240 from it; two per session (a switch and its way back).
struct Visit {std::int64_t qpc{};Snapshot s;};
constexpr unsigned visits_before=16,visits_after=240,capture_slots=2;
struct Capture {std::array<Visit,visits_before+visits_after> visits{};unsigned count{};bool done{},written{};};
std::array<Visit,visits_before> recent_visits{};unsigned recent_count{};
std::array<Capture,capture_slots> captures{};unsigned capture_index{};bool capturing{};
Snapshot previous_visit{};bool previous_valid{};
// The game changed its scene size with its aspect option (game-84: one visit
// shows the request pending, the next the new scene, about 120 ms later after
// the game recreated its targets). The override yields to the game's own
// scale until the worker rebases it on the new scene (live-aspect.md).
bool scene_pending{};std::atomic<bool> scene_flag{};std::uint64_t scene_changes{};
// Whether a refused snapshot shows the game's own aspect switch: the aspect
// applied as requested, not mid-resize, and the scene exactly the window
// fitted to it the way 0x229ef20 does (16:9 or 64:27; 0 is the window).
bool aspect_fit(const Snapshot &s) noexcept {
    if(s.aspect_requested!=s.aspect_applied||s.aspect_applied>2||(s.sizing&1)||!s.window_width||!s.window_height)return false;
    int w=static_cast<int>(s.window_width),h=static_cast<int>(s.window_height);
    if(s.aspect_applied){
        const float ratio=s.aspect_applied==2?64.F/27.F:16.F/9.F,fw=static_cast<float>(w),fh=static_cast<float>(h);
        if(fw/fh>ratio)w=static_cast<int>(fh*ratio);else if(fw/fh<ratio)h=static_cast<int>(fw/ratio);
    }
    return static_cast<unsigned>(w)==s.width&&static_cast<unsigned>(h)==s.height&&
           s.window_width==discovered.window_width&&s.window_height==discovered.window_height;
}
// Under the lock, every visit.
void note_visit(const Snapshot &s) noexcept {
    LARGE_INTEGER now{};QueryPerformanceCounter(&now);const Visit v{now.QuadPart,s};
    const bool notable=s.aspect_requested!=s.aspect_applied||(s.sizing&1)||(s.check&&unqualified==1)||
        (previous_valid&&(s.width!=previous_visit.width||s.height!=previous_visit.height||
                          s.window_width!=previous_visit.window_width||s.window_height!=previous_visit.window_height));
    previous_visit=s;previous_valid=true;
    if(capturing){
        auto &c=captures[capture_index];c.visits[c.count++]=v;
        if(c.count==c.visits.size()){c.done=true;capturing=false;++capture_index;}
    }else if(notable&&capture_index<capture_slots){
        auto &c=captures[capture_index];c.count=0;
        const unsigned kept=std::min(recent_count,visits_before);
        for(unsigned i=0;i<kept;++i)c.visits[c.count++]=recent_visits[(recent_count-kept+i)%visits_before];
        c.visits[c.count++]=v;capturing=true;
    }
    recent_visits[recent_count++%visits_before]=v;
}
struct ViewportSample {std::uint64_t tick{};std::uintptr_t caller{};std::array<float,6> values{};};
ViewportSample first_reduced_viewport{},last_reduced_viewport{};
void fail(unsigned reason) noexcept {unsigned zero{};fault.compare_exchange_strong(zero,reason);requested=false;}
bool write_request(float value) noexcept {
    __try {*reinterpret_cast<volatile float *>(renderer+0x1f0)=value;return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool snapshot(Snapshot &s,const Plan &t) noexcept {
    // Every value is read before any is judged, so a refused snapshot still
    // records what the renderer reported. Checks: 1 renderer, 2 API, 3 backend,
    // 4 view, 5 sizing word, 6 backend wrapper, 7 config, 8 renderer/view
    // values, 9 scale basis; then 10 sizing in progress, 11 wrapper present,
    // 12 width override, 13 basis differs from display, 14 non-finite scale,
    // 15 display differs from the swap chain, 16 content scale not 1,
    // 17 scale out of range, 18 view flags, 19 renderer or view changed.
    const auto stop=[&](unsigned check){s.check=check;return false;};
    std::uintptr_t root{},backend{},view{},config{},wrapper{};unsigned char api{},override_width{};unsigned sizing{};
    if(!pb::read(image+0x51c4480,root)||root!=renderer)return stop(1);
    if(!pb::read(image+0x51cc080,api)||api!=1)return stop(2);
    if(!pb::read(renderer+0x78,backend)||backend!=discovered.backend)return stop(3);
    if(!pb::read(renderer+0x1f450,view)||!view||view>UINTPTR_MAX-0x6d8)return stop(4);
    if(!pb::read(renderer+0xb4,sizing))return stop(5);
    s.sizing=sizing;
    if(!pb::read(backend+0x42700,wrapper))return stop(6);
    s.wrapper=wrapper!=0;
    if(!pb::read(image+0x51c5320,config)||!config||config>UINTPTR_MAX-0x2cdb||!pb::read(config+0x2cda,override_width))return stop(7);
    s.tick=GetTickCount64();s.view=view;s.width_override=override_width;
    // Diagnostics only (the aspect capture); a failed read leaves zeros.
    pb::read(renderer+0x1f448,s.window_width);pb::read(renderer+0x1f44c,s.window_height);
    pb::read(renderer+0x7b440,s.aspect_requested);pb::read(renderer+0x7b43c,s.aspect_applied);
    pb::read(renderer+0x1f548,s.flags);for(unsigned i=0;i<4;++i)pb::read(renderer+0x188+4*i,s.rect[i]);
    if(!pb::read(renderer+0x1f210,s.frame)||!pb::read(renderer+0x198,s.width)||!pb::read(renderer+0x19c,s.height)||
       !pb::read(renderer+0x1f0,s.desired)||!pb::read(renderer+0x1f4,s.applied)||!pb::read(renderer+0x1f8,s.previous)||
       !pb::read(renderer+0x200,s.content_desired)||!pb::read(renderer+0x204,s.content)||!pb::read(renderer+0x208,s.combined)||
       !pb::read(view+0x58,s.view_flags)||!pb::read(view+0x6b0,s.view_scale)||
       !pb::read(view+0x6b4,s.view_content)||!pb::read(view+0x6b8,s.view_combined))return stop(8);
    s.basis_width=s.width;s.basis_height=s.height;
    if(override_width&&(!pb::read(config+0x2cd0,s.basis_width)||!pb::read(config+0x2cd4,s.basis_height)))return stop(9);
    if(sizing&1)return stop(10);
    if(wrapper)return stop(11);
    if(override_width>1)return stop(12);
    if(!matching_basis({s.width,s.height},s.width_override,{s.basis_width,s.basis_height},{discovered.window_width,discovered.window_height}))return stop(13);
    for(float value:{s.desired,s.applied,s.previous,s.content_desired,s.content,s.combined,s.view_scale,s.view_content,s.view_combined})if(!std::isfinite(value))return stop(14);
    if(s.width!=t.display.width||s.height!=t.display.height)return stop(15);
    if(s.content_desired!=1.F||s.content!=1.F||s.view_content!=1.F)return stop(16);
    // A view may hold the truncated ratio of the planned render extent, just
    // under 0.5 for an odd scene; 2:1 scenes keep the 0.5 floor exactly.
    const float view_floor=std::min(.5F,truncated_ratio(t));
    if(s.desired<.5F||s.desired>1.F||s.applied<.5F||s.applied>1.F||s.view_scale<view_floor||s.view_scale>1.F)return stop(17);
    if(s.view_flags&0x840000)return stop(18);
    std::uintptr_t container{},texture{};unsigned packed{};
    s.depth_metadata=pb::read(view+0x628,container)&&container&&container<=UINTPTR_MAX-0xa8&&
        pb::read(container+0xa0,texture)&&texture&&texture<=UINTPTR_MAX-0x90&&pb::read(texture+0x88,packed);
    if(s.depth_metadata){s.depth_width=packed&0xffff;s.depth_height=(packed>>16)&0x3fff;}
    std::uintptr_t again{};
    if(!pb::read(renderer+0x1f450,again)||again!=view||!pb::read(image+0x51c4480,again)||again!=renderer)return stop(19);
    s.check=0;return true;
}
bool current_renderer() noexcept {std::uintptr_t r{};return pb::read(image+0x51c4480,r)&&r==renderer;}
void update(pb::Frame *f) {
    const bool matching=f->caller==expected_return&&f->gpr[1]==renderer;
    if(!matching){mhw_binding_forward(original,f);return;}
    const auto thread=GetCurrentThreadId();
    Plan t;
    {Lock lock;if(!render_thread)render_thread=thread;t=target;}
    if(render_thread!=thread){fail(1);mhw_binding_forward(original,f);return;}
    // This is inside the original renderer's update invocation. No worker writes.
    bool read_ok=current_renderer();float current{},restore_value{};
    read_ok=read_ok&&pb::read(renderer+0x1f0,current);
    if(!read_ok){fail(2);mhw_binding_forward(original,f);return;}
    {
        Lock lock;
        const bool was_owned=ownership.owned;
        if(ownership.before(current,restore_value)){
            if(!write_request(restore_value)){ownership.owned=true;fail(3);}
            else ++pre_restores;
        }else if(was_owned)++foreign_requests;
    }
    mhw_binding_forward(original,f);
    Snapshot s{};
    if(!snapshot(s,t)){
        Lock lock;if(!unqualified++)first_unqualified=s;last_unqualified=s;note_visit(s);
        if(s.check==15&&(requested.load()||yielded.load())&&!fault.load()&&aspect_fit(s)){
            // This visit wrote nothing: the game's own scale stays from here.
            requested=false;scene_pending=true;scene_flag=true;++scene_changes;
        }else if(overrides&&!scene_pending&&!yielded.load())fail(4);
        return;
    }
    {
    Lock lock;
    ++visits;last=s;note_visit(s);
    if(s.view_scale<.5F){if(!truncated_views++)first_truncated=s;}
    // A retarget during this visit leaves its count for the next one.
    if(t.quantized==target.quantized&&s.applied==t.quantized&&planned_view_scale(t,s.view_scale)&&s.combined==t.quantized&&s.view_combined==t.quantized){
        ++scaled_observed;reduced=s;
    }
    if(!requested.load()){
        if(!ownership.owned&&s.applied==s.desired&&s.view_scale==s.desired&&s.combined==s.desired&&s.view_combined==s.desired){
            ++restored_observed;restoration=s;restored=true;
        }
        return;
    }
    if(visits<=32&&!adopted){
        // Existing reduced rendering is not adopted as a new experiment.
        if(s.desired!=1.F||s.applied!=1.F||s.view_scale!=1.F||s.combined!=1.F||s.view_combined!=1.F){fail(5);return;}
        baseline=s;return;
    }
    ownership.after(s.desired,t.quantized);
    }
    // Do not hold the telemetry lock across engine code: SEH must be able to
    // unwind without leaving a lock owned by the interrupted render callback.
    setter(renderer,t.requested);
    float written{};
    if(!pb::read(renderer+0x1f0,written)||written!=t.quantized){fail(6);return;}
    {Lock lock;++overrides;}
}
void viewport_observation(ID3D12GraphicsCommandList *list,UINT count,const D3D12_VIEWPORT *views,std::uintptr_t caller) {
    if(perf::on(perf::no_viewport_trace)||!requested.load()||count!=1||!views)return;
    std::uintptr_t method{};
    if(!pb::system_method(reinterpret_cast<std::uintptr_t>(list),21,method)||method!=targets[1])return;
    ViewportSample sample{GetTickCount64(),caller,{}};
    const auto address=reinterpret_cast<std::uintptr_t>(views);
    if(address>UINTPTR_MAX-sizeof(D3D12_VIEWPORT))return;
    for(unsigned i=0;i<6;++i)if(!pb::read(address+4*i,sample.values[i])||!std::isfinite(sample.values[i])){Lock lock;++viewport_unreadable;return;}
    const auto &v=sample.values;unsigned kind=2;
    Lock lock;const auto &t=target;
    if(v[0]==0&&v[1]==0&&v[2]==static_cast<float>(t.display.width)&&v[3]==static_cast<float>(t.display.height))kind=0;
    else if(v[0]==0&&v[1]==0&&v[2]==static_cast<float>(t.render.width)&&v[3]==static_cast<float>(t.render.height))kind=1;
    if(!overrides)return;++viewport_counts[kind];
    if(kind==1){if(!first_reduced_viewport.tick)first_reduced_viewport=sample;last_reduced_viewport=sample;}
}
void STDMETHODCALLTYPE viewport_call(ID3D12GraphicsCommandList *list,UINT count,const D3D12_VIEWPORT *views) {
    ++pending;__try{original_viewport(list,count,views);viewport_observation(list,count,views,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));}__finally{--pending;}
}
void write_snapshot(std::ostream &out,const Snapshot &s) {
    out<<"{\"tick\":"<<s.tick<<",\"frame\":"<<s.frame<<",\"view\":"<<s.view<<",\"display\":["<<s.width<<','<<s.height
       <<"],\"scales\":["<<s.desired<<','<<s.applied<<','<<s.previous<<','<<s.content_desired<<','<<s.content<<','<<s.combined<<','<<s.view_scale<<','<<s.view_content<<','<<s.view_combined
       <<"],\"native_width_override\":"<<s.width_override<<",\"native_scale_basis\":["<<s.basis_width<<','<<s.basis_height
       <<"],\"view_flags\":"<<s.view_flags<<",\"depth_metadata\":"<<s.depth_metadata<<",\"depth_extent\":["<<s.depth_width<<','<<s.depth_height
       <<"],\"check\":"<<s.check<<",\"sizing\":"<<s.sizing<<",\"wrapper\":"<<s.wrapper<<"}";
}
void write_viewport(std::ostream &out,const ViewportSample &s) {
    out<<"{\"tick\":"<<s.tick<<",\"caller\":"<<s.caller<<",\"values\":[";
    for(unsigned i=0;i<s.values.size();++i)out<<(i?",":"")<<s.values[i];out<<"]}";
}
}
void dispatch(pb::Frame *f) {
    ++pending;bool returned=false;
    __try {update(f);returned=true;}
    __finally {if(!returned)fail(7);--pending;}
}
void observe_viewports(ID3D12GraphicsCommandList *list,unsigned count,const D3D12_VIEWPORT *views,std::uintptr_t caller) noexcept {
    ++pending;__try{viewport_observation(list,count,views,caller);}__finally{--pending;}
}
bool sizing_code(std::uintptr_t base) noexcept {
    struct Gate{unsigned rva,bytes;const char *sha;};
    constexpr Gate gates[]{
        {0x229ed30,493,"ca4ed5a0d6d18b2cda0bf84a5a7e3a4b17544bd523272affaf155f6ad150e08c"},
        {0x229bd70,105,"b4a5bb881df5a0a5d5a1c68d6e6cfc75de64f655c18438420c38836cc0bc8309"},
        {0x22982df,188,"d562a29be90631f4645d13fbf481489271ce1647969dbefae4d0fa2944bbfece"},
        {0x255d700,95,"e8cda592ae9fb49456a1c26b933d9eb3bfc33c1206830d4e5647c8beb5030d23"},
        {0x2391f30,535,"6d466f7eca374bcc389e46d54e687fe6bde382e1d175c1c0476a81bc505ce1be"},
        {0x23c3893,125,"c584d50b799cc517b72d99b2a475a9950d15d3a8244d778cb5cf45acd8930896"}};
    for(const auto &gate:gates)if(!pb::verify_code(base,gate.rva,gate.bytes,gate.sha))return false;
    return true;
}
bool ready(std::uintptr_t base,float scale,std::string &reason) {
    if(image){reason="already prepared";return false;}
    pb::Runtime probe;if(!pb::discover(base,probe,reason))return false;
    const auto planned=plan({probe.width,probe.height},probe.scene_override?probe.window_width:probe.width,scale);
    // DLAA asks for 1.0: the override then holds the game's full scale, and the
    // per-frame input stamps still prove which extent each frame rendered.
    if(!planned.valid){reason="extent plan";return false;}
    if(!sizing_code(base)){reason="native sizing code gate";return false;}
    std::uintptr_t root{};if(!pb::read(base+0x51c4480,root)||!root||root>UINTPTR_MAX-0x7b3f5){reason="renderer";return false;}
    // snapshot() reads this module state. No hook exists before install(), so
    // nothing else reads it while it is set for the check.
    image=base;renderer=root;discovered=probe;Snapshot s{};const bool admitted=snapshot(s,planned);
    image=0;renderer=0;discovered={};
    if(!admitted){reason="native sizing admission (check "+std::to_string(s.check)+")";return false;}
    return true;
}
bool install(std::uintptr_t base,float scale,std::string &reason,bool shared,bool yielded_start) {
    if(image){reason="already prepared";return false;}
    shared_viewport=shared;
    if(!pb::discover(base,discovered,reason))return false;
    // A pillarboxed scene quantizes its scale against the window (config basis).
    basis_width=discovered.scene_override?discovered.window_width:discovered.width;
    target=plan({discovered.width,discovered.height},basis_width,scale);
    if(!target.valid){reason="extent plan";return false;}
    if(!sizing_code(base)){reason="native sizing code gate";return false;}
    if(!pb::read(base+0x51c4480,renderer)||!renderer||renderer>UINTPTR_MAX-0x7b3f5){reason="renderer";return false;}
    image=base;expected_return=base+0x22982e7;setter=reinterpret_cast<Setter>(base+0x229bd70);
    Snapshot s{};const bool admitted=snapshot(s,target);{Lock lock;startup=s;}
    if(!admitted){reason="native sizing admission (check "+std::to_string(s.check)+")";return false;}
    targets={base+0x229ed30,discovered.targets[pb::viewport]};
    // The viewport method hook is the research builds' (shared_viewport
    // false); D3D12 methods are not al.hooks targets.
    if(!shared_viewport&&MH_CreateHook(reinterpret_cast<void *>(targets[1]),reinterpret_cast<void *>(viewport_call),reinterpret_cast<void **>(&original_viewport))!=MH_OK){reason="hook creation";return false;}
    // Yielded from the start (the player's DLSS Off): no visit checks or writes.
    yielded=yielded_start;requested=!yielded_start;hooks_disabled=false;
    update_hook.target=reinterpret_cast<void *>(targets[0]);
    if(!astera::hook_owned(update_hook,reinterpret_cast<void *>(mhw_resolution_entry),&original)||
       (!shared_viewport&&MH_EnableHook(reinterpret_cast<void *>(targets[1]))!=MH_OK)){
        astera::unhook_owned(update_hook);requested=false;yielded=false;reason="hook activation";return false;}
    return true;
}
void stop() noexcept {requested=false;}
void yield() noexcept {Lock lock;if(!image||fault.load()||!requested.load())return;yielded=true;requested=false;}
bool yielded_now() noexcept {return yielded.load();}
bool released() noexcept {Lock lock;return !requested.load()&&!ownership.owned;}
bool full_scale(std::uintptr_t base) noexcept {
    if(image)return false;
    try{
        pb::Runtime probe;std::string reason;if(!pb::discover(base,probe,reason))return false;
        const auto planned=plan({probe.width,probe.height},probe.scene_override?probe.window_width:probe.width,1.F);
        std::uintptr_t root{};if(!planned.valid||!pb::read(base+0x51c4480,root)||!root||root>UINTPTR_MAX-0x7b3f5)return false;
        // As in ready(): no hook reads this state before install().
        image=base;renderer=root;discovered=probe;Snapshot s{};const bool admitted=snapshot(s,planned);
        image=0;renderer=0;discovered={};
        return admitted&&s.desired==1.F&&s.applied==1.F&&s.view_scale==1.F&&s.combined==1.F&&s.view_combined==1.F;
    }catch(...){image=0;renderer=0;discovered={};return false;}
}
bool resume(float scale,std::string &reason) {
    Lock lock;
    if(!image||!yielded.load()||fault.load()){reason="render scale not yielded";return false;}
    if(scene_pending){reason="the screen layout changed";return false;}
    const auto planned=plan(target.display,basis_width,scale);
    if(!planned.valid){reason="extent plan";return false;}
    target=planned;scaled_observed=0;yielded=false;adopted=true;requested=true;return true;
}
bool retarget(float scale,std::string &reason) {
    Lock lock;
    if(!image||!requested.load()||fault.load()){reason="render scale not active";return false;}
    const auto planned=plan(target.display,basis_width,scale);
    if(!planned.valid){reason="extent plan";return false;}
    target=planned;scaled_observed=0;return true;
}
std::uint64_t applied_visits() noexcept {Lock lock;return scaled_observed;}
bool scene_changed() noexcept {return scene_flag.load();}
bool rebase(Extent display,unsigned basis,float scale,std::string &reason) {
    Lock lock;
    if(!image||!scene_pending||fault.load()){reason="no scene change pending";return false;}
    const auto planned=plan(display,basis,scale);
    if(!planned.valid){reason="extent plan";return false;}
    if(yielded.load())adopted=true;
    target=planned;basis_width=basis;scaled_observed=0;scene_pending=false;scene_flag=false;yielded=false;requested=true;return true;
}
unsigned write_captures(std::ostream &out,bool partial) {
    Lock lock;unsigned written=0;
    for(unsigned i=0;i<capture_slots;++i){auto &c=captures[i];
        if(c.written||!c.count||(!c.done&&!(partial&&capturing&&i==capture_index)))continue;
        LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);const auto first=c.visits[0].qpc;
        out<<std::boolalpha<<"{\"event\":\"aspect_capture\",\"schema\":1,\"index\":"<<i<<",\"complete\":"<<c.done
           <<",\"before\":"<<visits_before<<",\"visits\":[";
        for(unsigned j=0;j<c.count;++j){const auto &v=c.visits[j];const auto &s=v.s;
            const auto us=frequency.QuadPart>0?(v.qpc-first)*1000000/frequency.QuadPart:0;
            out<<(j?",":"")<<"{\"us\":"<<us<<",\"frame\":"<<s.frame<<",\"check\":"<<s.check<<",\"sizing\":"<<s.sizing
               <<",\"display\":["<<s.width<<','<<s.height<<"],\"window\":["<<s.window_width<<','<<s.window_height
               <<"],\"aspect\":["<<s.aspect_requested<<','<<s.aspect_applied<<"],\"flags\":"<<s.flags
               <<",\"rect\":["<<s.rect[0]<<','<<s.rect[1]<<','<<s.rect[2]<<','<<s.rect[3]
               <<"],\"scales\":["<<s.desired<<','<<s.applied<<','<<s.content_desired<<','<<s.content<<','<<s.view_scale<<','<<s.view_content
               <<"],\"override\":"<<s.width_override<<",\"basis\":["<<s.basis_width<<','<<s.basis_height
               <<"],\"depth\":["<<s.depth_width<<','<<s.depth_height<<"],\"wrapper\":"<<s.wrapper<<'}';}
        out<<"]}\n"<<std::flush;c.written=true;++written;
    }
    return written;
}
bool active_ready() noexcept {Lock lock;return requested.load()&&!fault.load()&&scaled_observed>=32;}
bool input_stamp(std::uintptr_t view,InputStamp &out) noexcept {
    Lock lock;Snapshot s{};
    if(!requested.load()||fault.load()||!snapshot(s,target)||s.view!=view||
       s.applied!=target.quantized||!planned_view_scale(target,s.view_scale)||s.combined!=target.quantized||s.view_combined!=target.quantized)return false;
    out={s.frame,s.view,s.width,s.height,target.render.width,target.render.height,s.applied,s.content};return true;
}
bool healthy() noexcept {return fault.load()==0;}
bool finish() {
    stop();if(hooks_disabled.load())return true;
    const auto start=GetTickCount64();
    while((!restored.load()||pending.load())&&GetTickCount64()-start<5000)Sleep(1);
    if(!restored.load()||pending.load())return false; // Keep the pinned recovery hook available.
    if(!astera::unhook_owned(update_hook))return false;
    if(!shared_viewport&&MH_DisableHook(reinterpret_cast<void *>(targets[1]))!=MH_OK)return false;
    while(pending.load()&&GetTickCount64()-start<6000)Sleep(1);
    if(pending.load())return false;
    hooks_disabled=true;return true;
}
void write(std::ostream &out) {
    Lock lock;out<<std::boolalpha<<std::setprecision(9)<<"{\"event\":\"resolution\",\"schema\":1,\"policy\":\"native-active-scale-v1\",\"requested\":"<<requested.load()
       <<",\"yielded\":"<<yielded.load()<<",\"fault\":"<<fault.load()<<",\"pending\":"<<pending.load()<<",\"hooks_disabled\":"<<hooks_disabled.load()<<",\"restored\":"<<restored.load()
       <<",\"owned_request\":"<<ownership.owned<<",\"visits\":"<<visits<<",\"unqualified\":"<<unqualified<<",\"overrides\":"<<overrides
       <<",\"pre_restores\":"<<pre_restores<<",\"foreign_requests\":"<<foreign_requests<<",\"scaled_observed\":"<<scaled_observed<<",\"restored_observed\":"<<restored_observed<<",\"truncated_views\":"<<truncated_views
       <<",\"display\":["<<target.display.width<<','<<target.display.height<<"],\"render\":["<<target.render.width<<','<<target.render.height<<"],\"requested_scale\":"<<target.requested<<",\"quantized_scale\":"<<target.quantized
       <<",\"swapchain_format\":"<<discovered.format<<",\"adapter_luid\":["<<discovered.luid_high<<','<<discovered.luid_low<<"],\"render_thread\":"<<render_thread
       <<",\"shared_viewport\":"<<shared_viewport<<",\"owned_hooks\":"<<(shared_viewport?1:2)
       <<",\"scene_changes\":"<<scene_changes<<",\"scene_pending\":"<<scene_pending
       <<",\"viewport_counts\":["<<viewport_counts[0]<<','<<viewport_counts[1]<<','<<viewport_counts[2]<<"],\"viewport_unreadable\":"<<viewport_unreadable
       <<",\"baseline\":";write_snapshot(out,baseline);out<<",\"reduced\":";write_snapshot(out,reduced);out<<",\"last\":";write_snapshot(out,last);out<<",\"restoration\":";write_snapshot(out,restoration);
    out<<",\"startup\":";write_snapshot(out,startup);out<<",\"first_unqualified\":";write_snapshot(out,first_unqualified);
    out<<",\"last_unqualified\":";write_snapshot(out,last_unqualified);
    out<<",\"first_truncated\":";write_snapshot(out,first_truncated);
    out<<",\"first_reduced_viewport\":";write_viewport(out,first_reduced_viewport);out<<",\"last_reduced_viewport\":";write_viewport(out,last_reduced_viewport);
    out<<",\"gpu_resource_descriptions_verified\":false,\"viewport_scene_attribution_verified\":false,\"dlss_evaluations\":0,\"injected_gpu_commands\":0,\"settings_writes\":0}\n"<<std::flush;
}
}
extern "C" void mhw_resolution_dispatch(mhw::projection_binding::Frame *frame){mhw::render_resolution::dispatch(frame);}
