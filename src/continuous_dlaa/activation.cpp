#include "activation.hpp"
#ifdef MHW_CONTINUOUS_SCALED
#include "../scaled_temporal/stream_route.hpp"
#endif
#include "../projection_binding/memory.hpp"
#include <windows.h>
#include <MinHook.h>
#include <atomic>
#include <array>
#include <ostream>

extern "C" {
// Aligned interlocked scalars are also read by the two leaf ASM gate overrides.
alignas(8) volatile LONG64 mhw_aa_context{};
alignas(8) volatile LONG64 mhw_aa_thread{};
alignas(8) volatile LONG64 mhw_aa_table_forced{},mhw_aa_sharpen_suppressed{};
void *mhw_aa_table_original{},*mhw_aa_sharpen_original{};
void *mhw_aa_table_resume{},*mhw_aa_sharpen_resume{};
void mhw_aa_frame_entry();void mhw_aa_taa_entry();void mhw_aa_queue_entry();
void mhw_aa_jitter2_entry();void mhw_aa_jitter1_entry();void mhw_aa_jitter3_entry();
void mhw_aa_table_entry();void mhw_aa_sharpen_entry();
void mhw_binding_forward(void *,mhw::projection_binding::Frame *);
}

namespace mhw::continuous_dlaa::activation {
namespace {
namespace pb=projection_binding;
std::uintptr_t image{},renderer{};
std::array<std::uintptr_t,8> targets{};
std::array<void *,8> originals{};
std::array<bool,8> enabled{};
std::array<bool,8> disable_queued{};
std::atomic<bool> requested{};
std::atomic<bool> failure_latched{};
std::atomic<unsigned> pending{},fault{};
std::atomic<std::uint64_t> owned_frames{},units{},admitted{},rejected{},competing{},jitter_suppressed{};
std::array<std::atomic<std::uint64_t>,4> choices{};
std::atomic<std::uint64_t> queue_visits{},queue_inserted{},queue_existing{},queue_rejected{},execute_unowned{};
std::atomic<std::uint64_t> queue_unfiltered{};
std::array<std::atomic<std::uint64_t>,6> queue_reasons{};
SRWLOCK telemetry_mutex=SRWLOCK_INIT;
struct TelemetryLock {TelemetryLock(){AcquireSRWLockExclusive(&telemetry_mutex);}~TelemetryLock(){ReleaseSRWLockExclusive(&telemetry_mutex);}};
thread_local bool frame_owned{};
thread_local unsigned frame_thread{};
thread_local std::uintptr_t frame_view{};
thread_local std::uintptr_t queued_taa{};
LONG64 context() noexcept {return InterlockedCompareExchange64(&mhw_aa_context,0,0);}
void fail(unsigned n) noexcept {unsigned zero{};fault.compare_exchange_strong(zero,n);requested=false;}
bool primary(std::uintptr_t ctx,std::uintptr_t view) noexcept {
    std::uintptr_t r{},primary_view{},backend{},wrapper{};unsigned flags{},sizing{};unsigned char api{};
    return ctx==renderer+0x710&&pb::read(image+0x51cc080,api)&&api==1&&
        pb::read(image+0x51c4480,r)&&r==renderer&&
        pb::read(renderer+0x1f450,primary_view)&&primary_view&&
        view==primary_view&&pb::read(view+0x58,flags)&&!(flags&0x840000)&&
        pb::read(renderer+0xb4,sizing)&&!(sizing&1)&&pb::read(renderer+0x78,backend)&&backend&&
        pb::read(backend+0x42700,wrapper)&&!wrapper;
}
bool begin_frame(const pb::Frame &f) noexcept {
    // R8 is the incoming view. The native body binds context+1d0 later;
    // that field may still be null or contain a previous view at entry.
    if(!requested.load()||f.gpr[4]!=0||!primary(f.gpr[2],f.gpr[3]))return false;
    std::uintptr_t camera{},type{},table{};unsigned char taa{},fxaa{};
    if(!pb::read(image+0x51c4400,camera)||camera!=f.gpr[1]||!camera||!pb::read(camera,type)||
       (type!=image+0x350d188&&type!=image+0x33f82a8)||
       !pb::read(image+0x51c4368,table)||!table||
       !pb::read(table+0xe9e4,taa)||taa>1||!pb::read(table+0xe9e5,fxaa)||fxaa>1)return false;
    if(frame_owned||InterlockedCompareExchange64(&mhw_aa_context,static_cast<LONG64>(f.gpr[2]),0)!=0){fail(1);return false;}
    frame_owned=true;frame_thread=GetCurrentThreadId();frame_view=f.gpr[3];
    {TelemetryLock lock;++owned_frames;++choices[taa+2U*fxaa];}return true;
}
void end_frame(bool owned,bool returned) noexcept {
    if(!owned)return;
    if(!returned)fail(2);
    InterlockedExchange64(&mhw_aa_thread,0);
    if(!frame_owned||frame_thread!=GetCurrentThreadId()||
       InterlockedCompareExchange64(&mhw_aa_context,0,static_cast<LONG64>(renderer+0x710))!=static_cast<LONG64>(renderer+0x710))fail(3);
    frame_owned=false;frame_thread=0;frame_view=0;
}
bool owned_context(std::uintptr_t ctx) noexcept {
    std::uintptr_t view{};
    return frame_owned&&frame_thread==GetCurrentThreadId()&&ctx==static_cast<std::uintptr_t>(context())&&
        pb::read(ctx+0x1d0,view)&&view==frame_view&&primary(ctx,view);
}
bool native_gates(std::uintptr_t unit,std::uintptr_t ctx,std::uintptr_t registry) noexcept {
    unsigned state{},flags{},mask{},displays{};std::uint16_t ctx_mask{},display{};
    unsigned category{},excluded{};
    if(!pb::read(unit+0xc,state)||!pb::read(unit+0x14,flags)||!pb::read(unit+0x48,mask)||
       !pb::read(unit+0x1c,displays)||!pb::read(ctx+0x152,ctx_mask)||!pb::read(ctx+0x15a,display)||
       !pb::read(unit+0x40,category)||!pb::read(registry+0x4a50,excluded))return false;
    return state==8&&(flags&0x10)&&(mask&ctx_mask)&&(displays&(1U<<(display&15)))&&!(category&~excluded);
}
std::uintptr_t reject_queue(unsigned reason) noexcept {
    TelemetryLock lock;++queue_rejected;++queue_reasons[reason];return 0;
}
std::uintptr_t prepare_queue(const pb::Frame &f) {
    // Camera calls this after native job join/merge and before the queue sorts
    // and assigns ping-pong resources. No worker context is adopted or changed.
    if(!owned_context(f.gpr[2])||f.gpr[3]!=frame_view)return 0;
    ++queue_visits;
    const bool bypass=failure_latched.load();
    std::uintptr_t queue{},registry{},type{},node{},taa{};unsigned count{};unsigned char collecting{},enabled_queue{};
    if(!pb::read(image+0x51cc238,queue)||queue!=f.gpr[1]||!queue||
       !pb::read(queue+0x38,enabled_queue)||enabled_queue!=1||!pb::read(queue+0x39,collecting)||collecting!=1||
       !pb::read(queue+0x3c,count)||count>16)return reject_queue(0);
    if(!bypass){
    if(!pb::read(image+0x51238c8,registry)||!registry||!pb::read(registry,type)||type!=image+0x33fcde8||
       !pb::read(registry+0x15d0,node))return reject_queue(1);
    unsigned nodes=0;
    while(node&&nodes++<1024){
        if(!pb::read(node,type))return reject_queue(1);
        if(type==image+0x351bda8){if(taa)return reject_queue(1);taa=node;}
        if(!pb::read(node+0x30,node))return reject_queue(1);
    }
    if(node||!taa)return reject_queue(1);
    if(!native_gates(taa,f.gpr[2],registry))return reject_queue(2);
    }
    std::uintptr_t owner{};unsigned char skip{},passthrough{};
    if(!bypass){
    if(!pb::read(taa+0x148,owner)||owner!=taa||!pb::read(taa+0x160,skip)||skip||
       !pb::read(taa+0x161,passthrough)||passthrough)return reject_queue(3);
    }
    // The native queue has sixteen pointer slots, followed by its two scratch
    // targets at +c0. Validate every entry before touching this transient queue.
    std::array<std::uintptr_t,16> keep{};unsigned kept=0,removed=0,found=0;
    for(unsigned i=0;i<count;++i){
        std::uintptr_t entry{},unit{},vt{};
        if(!pb::read(queue+0x40+8*i,entry)||!entry||!pb::read(entry+0x20,skip))return reject_queue(3);
        if(!skip){
            if(!pb::read(entry+8,unit))return reject_queue(3);
            if(unit&&!pb::read(unit,vt))return reject_queue(3);
            if(vt==image+0x351c1a8||vt==image+0x354dd78||vt==image+0x354df48||(bypass&&vt==image+0x351bda8)){++removed;continue;}
            if(vt==image+0x351bda8){
                unsigned stage{},order{};
                if(unit!=taa||entry!=taa+0x140||++found!=1||!pb::read(entry,stage)||stage!=15||
                   !pb::read(entry+4,order)||order!=2)return reject_queue(3);
            }
        }
        keep[kept++]=entry;
    }
    if(!bypass&&!found&&kept==keep.size())return reject_queue(4);
    if(queued_taa)return reject_queue(5);
    // Equivalent to native registration/removal before sorting; no preference,
    // enable flag, cached job list, worker data or persistent resource is edited.
    for(unsigned i=0;i<kept;++i)*reinterpret_cast<std::uintptr_t *>(queue+0x40+8*i)=keep[i];
    *reinterpret_cast<unsigned *>(queue+0x3c)=kept;
    if(bypass){TelemetryLock lock;competing+=removed;++queue_unfiltered;return 0;}
    if(!found){
        using Register=void(*)(std::uintptr_t,unsigned,unsigned);
        reinterpret_cast<Register>(image+0x24c9240)(taa,15,2);
    }
    unsigned after{};std::uintptr_t last{};
    if(!pb::read(queue+0x3c,after)||after!=kept+(found?0U:1U)||
       (!found&&(!pb::read(queue+0x40+8*kept,last)||last!=taa+0x140))){fail(6);return reject_queue(3);}
    {TelemetryLock lock;competing+=removed;if(found)++queue_existing;else ++queue_inserted;}
    return taa;
}
bool suppress_jitter(unsigned kind,const pb::Frame &f) noexcept {
    // Update runs before camera rendering. These exact native callbacks only
    // assign AA jitter; their allocation/reset callbacks remain untouched.
    if(!requested.load()&&!context())return false;
    constexpr std::array<std::uintptr_t,3> types{0x351bda8,0x354dd78,0x354df48};
    std::uintptr_t type{},r{};
    if(kind<2||kind>4||!pb::read(f.gpr[1],type)||type!=image+types[kind-2]||
       !pb::read(image+0x51c4480,r)||r!=renderer)return false;
    ++jitter_suppressed;return true;
}
void forward_taa(pb::Frame *f) {
    // Comparison overrides belong only to an admitted TAA2 Execute, not every
    // native unit/view inside the outer camera call. Keep them through return,
    // including an explicit diagnostic stop, then clear even on an exception.
    const auto thread=reinterpret_cast<LONG64>(NtCurrentTeb());
    const auto previous=InterlockedCompareExchange64(&mhw_aa_thread,thread,0);
    if(previous){
        fail(5);InterlockedExchange64(&mhw_aa_thread,0);
        __try{mhw_binding_forward(originals[1],f);}
        __finally{InterlockedExchange64(&mhw_aa_thread,previous);}
        return;
    }
    bool returned=false;
    __try{mhw_binding_forward(originals[1],f);returned=true;}
    __finally{InterlockedExchange64(&mhw_aa_thread,0);if(!returned)fail(5);}
}
void record_admission(){TelemetryLock lock;++units;++admitted;}
void dispatch_inner(unsigned kind,pb::Frame *f) {
    if(kind==0){
        const bool owned=begin_frame(*f);bool returned=false;
        __try{mhw_binding_forward(originals[0],f);returned=true;}
        __finally{end_frame(owned,returned);}
    }else if(kind==1){
        if(queued_taa&&f->gpr[1]==queued_taa&&owned_context(f->gpr[2])){
            record_admission();
            forward_taa(f);
        }else{++execute_unowned;mhw_binding_forward(originals[1],f);}
    }else if(kind==7){
        const auto previous=queued_taa;
        const auto previous_thread=InterlockedExchange64(&mhw_aa_thread,0);
#ifdef MHW_CONTINUOUS_SCALED
        // Forward saves the native return registers over Frame. Preserve the
        // incoming identity before it, and notify only the admitted outer queue.
        const auto manager=f->gpr[1],ctx=f->gpr[2],view=f->gpr[3];
#endif
        // Nested queues cannot inherit the outer queue's comparison authority.
        queued_taa=0;bool returned=false;
        __try{
            if(previous){++queue_visits;queued_taa=reject_queue(5);}else queued_taa=prepare_queue(*f);
            mhw_binding_forward(originals[7],f);returned=true;
#ifdef MHW_CONTINUOUS_SCALED
            if(!previous&&queued_taa&&requested.load()&&!fault.load()&&owned_context(ctx)&&view==frame_view)
                scaled_temporal::stream_route::post_queue_complete(manager,ctx,view,queued_taa);
#endif
        }
        __finally{queued_taa=previous;InterlockedExchange64(&mhw_aa_thread,previous_thread);if(!returned)fail(7);}
    }else if(!suppress_jitter(kind,*f))mhw_binding_forward(originals[kind],f);
}
}
void dispatch(unsigned kind,pb::Frame *f){++pending;__try{dispatch_inner(kind,f);}__finally{--pending;}}
bool prepare(std::uintptr_t host,std::uintptr_t r,std::string &reason){
    if(image){reason="AA activation already prepared";return false;}
    struct Gate {unsigned rva,bytes;const char *sha;};
    constexpr Gate gates[]{
        {0x24c9240,43,"3907659e9c8215d943edde455a4b54fe655c5a52f6b1bfd9571ec43d71c7d58f"},
        {0x24f1fc0,835,"1ec618caf8d1d455cc12fc12c57889b73e1073074faf04e82364fba10d01e190"},
        {0x24f2320,176,"2965519c5b4feea0ed564a4bfce9c87a49def3e96433a7ce7e796791d270162e"},
        {0x228e770,528,"33ae7df95058ac2e1ce8d920ff89c62d664068a975931bb10afb95ea88f00b97"},
        {0x2392150,4221,"d2c6c0f6a5ed39c998bcf62afed19132e6dd0d7cfbb19e18b80c15531af2bf91"},
        {0x23931e0,124,"88aa2c037b51855a2e516f683ec68c1a00565edc5f78741e8db71783e0b7e488"},
        {0x2596090,108,"cfbffc3008baba8b595a7ce926d63f16ce609ceb6379c538cf3231df958d3752"},
        {0x25979b0,124,"d390c4d7965f6b3aa683e7324b570eb574f63fe5a0d419daa469aff73de7399a"}};
    for(const auto &g:gates)if(!pb::verify_code(host,g.rva,g.bytes,g.sha)){reason="AA activation code gate";return false;}
    image=host;renderer=r;
    constexpr std::array<unsigned,8> rvas{0x228e770,0x2392150,0x23931e0,0x2596090,0x25979b0,0x239217d,0x2392f8e,0x24f1fc0};
    const std::array<void *,8> detours{reinterpret_cast<void *>(mhw_aa_frame_entry),reinterpret_cast<void *>(mhw_aa_taa_entry),
        reinterpret_cast<void *>(mhw_aa_jitter2_entry),reinterpret_cast<void *>(mhw_aa_jitter1_entry),reinterpret_cast<void *>(mhw_aa_jitter3_entry),
        reinterpret_cast<void *>(mhw_aa_table_entry),reinterpret_cast<void *>(mhw_aa_sharpen_entry),reinterpret_cast<void *>(mhw_aa_queue_entry)};
    for(unsigned i=0;i<rvas.size();++i){targets[i]=host+rvas[i];
        if(MH_CreateHook(reinterpret_cast<void *>(targets[i]),detours[i],&originals[i])!=MH_OK||
           MH_QueueEnableHook(reinterpret_cast<void *>(targets[i]))!=MH_OK){reason="AA activation hook creation";return false;}
        enabled[i]=true;
    }
    mhw_aa_table_original=originals[5];mhw_aa_sharpen_original=originals[6];
    mhw_aa_table_resume=reinterpret_cast<void *>(host+0x2392184);
    mhw_aa_sharpen_resume=reinterpret_cast<void *>(host+0x2392f95);
    requested=true;return true;
}
void stop() noexcept {requested=false;}
bool owns_projection(std::uintptr_t ctx,std::uintptr_t view) noexcept {
    return requested.load()&&!fault.load()&&frame_owned&&frame_thread==GetCurrentThreadId()&&
        ctx==static_cast<std::uintptr_t>(context())&&view==frame_view&&primary(ctx,view);
}
void latch_failure() noexcept {failure_latched=true;}
bool drained() noexcept {return !pending.load()&&!context();}
bool healthy() noexcept {return !fault.load();}
bool queue_disable() noexcept {
    disable_queued.fill(false);
    // After stop, new callbacks only forward. They can re-enter between the
    // parent's drain check and this call, especially during scale restoration.
    // MinHook can detach their entry points while the pinned DLL/trampolines
    // remain alive. An owned camera scope must still finish first.
    if(requested.load()||context())return false;
    bool ok=true;for(unsigned i=0;i<targets.size();++i)if(enabled[i]){
        disable_queued[i]=MH_QueueDisableHook(reinterpret_cast<void *>(targets[i]))==MH_OK;
        if(!disable_queued[i])ok=false;
    }
    return ok;
}
void disabled(bool success) noexcept {if(success)for(unsigned i=0;i<enabled.size();++i)if(disable_queued[i]){enabled[i]=false;disable_queued[i]=false;}}
void write(std::ostream &out){
    TelemetryLock lock;
    bool hooks_off=true;for(bool on:enabled)hooks_off=hooks_off&&!on;
    out<<"{\"schema\":2,\"policy\":\"native-postqueue-preparation-v2\",\"settings_writes\":0,\"layer_flag_writes\":0,\"requested\":"<<requested.load()
       <<",\"fault\":"<<fault.load()<<",\"pending\":"<<pending.load()<<",\"active_context\":"<<context()<<",\"frames\":"<<owned_frames.load()
       <<",\"taa_visits\":"<<units.load()<<",\"taa_admitted\":"<<admitted.load()<<",\"taa_rejected\":"<<rejected.load()
       <<",\"competing_suppressed\":"<<competing.load()<<",\"jitter_suppressed\":"<<jitter_suppressed.load()
       <<",\"table_forced\":"<<InterlockedCompareExchange64(&mhw_aa_table_forced,0,0)
       <<",\"sharpen_suppressed\":"<<InterlockedCompareExchange64(&mhw_aa_sharpen_suppressed,0,0)
       <<",\"queue_visits\":"<<queue_visits.load()<<",\"queue_inserted\":"<<queue_inserted.load()
       <<",\"queue_existing\":"<<queue_existing.load()<<",\"queue_rejected\":"<<queue_rejected.load()
       <<",\"execute_unowned\":"<<execute_unowned.load()<<",\"queue_reasons\":[";
    for(unsigned i=0;i<queue_reasons.size();++i)out<<(i?",":"")<<queue_reasons[i].load();out<<"]"
       <<",\"failure_latched\":"<<failure_latched.load()<<",\"queue_unfiltered\":"<<queue_unfiltered.load()
       <<",\"hooks_disabled\":"<<hooks_off<<",\"native_choices\":[";
    for(unsigned i=0;i<choices.size();++i)out<<(i?",":"")<<choices[i].load();out<<"]}";
}
}
extern "C" void mhw_aa_dispatch(unsigned kind,mhw::projection_binding::Frame *f){mhw::continuous_dlaa::activation::dispatch(kind,f);}
