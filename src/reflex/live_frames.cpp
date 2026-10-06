#include "live_frames.hpp"
#include "frame_session.hpp"
#include "frame_stamp.hpp"
#include "frame_boundaries.hpp"
#include "../projection_binding/recorder.hpp"
#include "../projection_binding/memory.hpp"
#ifdef MHW_FG_SCENE_PROBE
#include "../scaled_temporal/stream_route.hpp"
#endif
#include <MinHook.h>
#include <atomic>
#include <array>
#include <mutex>
#include <ostream>
#include <stdexcept>

extern "C" {
void mhw_reflex_boundary_forward(void*, mhw::reflex::boundaries::Frame*);
void reflex_boundary_update(); void reflex_boundary_publish(); void reflex_boundary_submit();
}
namespace mhw::reflex {
namespace {
namespace pb = mhw::projection_binding;
struct Gate { unsigned rva, bytes; const char* sha; unsigned kind; void(*entry)(); };
constexpr std::array gates{
    Gate{0x1af2b60,4864,"9f6f5b3ac1a521ddd59351e41cc66c17dfc453ccd1b969afd4d59787bfef795d",0,reflex_boundary_update},
    Gate{0x229a080,1428,"961cf9969c28abc6b5b63ba9fb476978555eb188fdc173940d7ed81fc566a4e0",2,reflex_boundary_publish},
    Gate{0x23a7f50,69,"1183e1807b99ec2ada753fa147af8c59b1f99f29bbbf6176ed7479fd48e09a46",11,reflex_boundary_submit}
};
std::atomic<LiveFrames::State*> dispatch{};
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
}
struct LiveFrames::State {
    explicit State(Streamline& sdk) : frames(sdk) {}
    mutable std::mutex mutex;
    FrameSession frames;
    std::atomic<unsigned> calls{};
    std::atomic<bool> stop_requested{};
    std::uintptr_t image{}, engine{}, renderer{}, backend{}, device{};
    std::array<void*,13> originals{};
    std::array<PresentationHook,3> heads{};
    LiveFrameStats stats;
    struct Sample {
        unsigned id{}, stage{}, thread{}, result{};
        std::uint64_t qpc{}, tokens{}, sleeps{}, markers{};
        FrameStamp stamp;
    };
    std::array<Sample,64> samples{}; unsigned sample_count{};
    const char* reason{};

    FrameStamp stamp() const noexcept {
        return read_frame_stamp([](std::uintptr_t at, auto& out) noexcept { return pb::read(at,out); },renderer).value;
    }
    bool identities() const noexcept {
        std::uintptr_t e{},r{},b{},d{},link{};
        return pb::read(image+0x51c4148,e) && e==engine && pb::read(image+0x51c4480,r) && r==renderer &&
            pb::read(image+0x51c7050,b) && b==backend && pb::read(backend+0x18,d) && d==device &&
            pb::read(engine+0xaf7b0,link) && link==renderer && pb::read(renderer+0x78,link) && link==backend;
    }
    void fault(const char* why) noexcept {
        if(!stats.faults) { ++stats.faults;reason=why; }
        stats.running=false;stats.frame_active=false;frames.stop();
    }
    bool accepted(sl::Result result, unsigned stage, FrameStamp value) noexcept {
        LARGE_INTEGER time{}; QueryPerformanceCounter(&time);
        if(sample_count<samples.size() && frames.id()<=8)
            samples[sample_count++]={frames.id(),stage,GetCurrentThreadId(),static_cast<unsigned>(result),
                static_cast<std::uint64_t>(time.QuadPart),frames.tokens(),frames.sleeps(),frames.markers(),value};
        if(result!=sl::Result::eOk || frames.disabled()) { fault("frame-policy-or-sdk");return false; }
        return true;
    }
    bool enter(unsigned kind, const boundaries::Frame& f) noexcept {
        try {
            std::lock_guard lock(mutex);
            if(!stats.running)return false;
            if(kind==0 && stop_requested.load()) { stats.running=false;frames.stop();return false; }
            const auto thread=GetCurrentThreadId();
            if(!identities() || (stats.owner && stats.owner!=thread)) { fault("frame-owner-or-identity");return false; }
            const auto object=f.gpr[1];
            if(object!=(kind==0?engine:kind==2?renderer:backend)) { fault("native-this-identity");return false; }
            const auto value=stamp();
            if(kind==0) {
                if(stats.frame_active) { fault("nested-engine-frame");return false; }
                stats.owner=thread;
                if(!accepted(frames.begin(thread,value),0,value))return false;
                stats.frame_active=true;return true;
            }
            // Hooks can be enabled halfway through a native frame. Only the
            // following qualified engine entry creates the first token.
            if(!stats.frame_active)return false;
            if(kind==2)return accepted(frames.render_begin(thread,value),1,value);
            return kind==11;
        } catch(...) { std::lock_guard lock(mutex);fault("frame-entry-exception");return false; }
    }
    void leave(unsigned kind, bool ticket, bool completed) noexcept {
        try {
            std::lock_guard lock(mutex);
            if(!ticket || !stats.running)return;
            if(!completed) { fault("native-frame-exception");return; }
            if(!identities() || GetCurrentThreadId()!=stats.owner) { fault("frame-return-identity");return; }
            const auto value=stamp();
            if(!value.valid) { fault("frame-return-stamp");return; }
            if(kind==11)accepted(frames.render_end(stats.owner,value),2,value);
            if(kind==0) {
                if(accepted(frames.end(stats.owner),5,value)) {
                    stats.frame_active=false;
                    if(stop_requested.load()) { stats.running=false;frames.stop(); }
                }
            }
        } catch(...) { std::lock_guard lock(mutex);fault("frame-return-exception"); }
    }
    bool present_begin(std::uintptr_t swap, unsigned flags) noexcept {
        try {
            std::lock_guard lock(mutex);
            if(flags&DXGI_PRESENT_TEST) { ++stats.present_tests;return false; }
            if(!stats.running || !stats.frame_active) { ++stats.untracked_presents;return false; }
            std::uintptr_t native{};
            if(!identities() || GetCurrentThreadId()!=stats.owner || !pb::read(backend+0x1470,native) || native!=swap) {
                fault("present-owner-or-identity");return false;
            }
            const auto value=stamp();const bool ok=accepted(frames.present_begin(stats.owner,value),3,value);
#ifdef MHW_FG_SCENE_PROBE
            if(ok)mhw::scaled_temporal::stream_route::scene_present(value.temporal,value.epoch,frames.id(),stats.owner,swap);
#endif
            return ok;
        } catch(...) { std::lock_guard lock(mutex);fault("present-entry-exception");return false; }
    }
    void present_end(std::uintptr_t swap, unsigned flags, HRESULT result) noexcept {
        try {
            std::lock_guard lock(mutex);
            if(!stats.running)return;
            std::uintptr_t native{};
            if(flags&DXGI_PRESENT_TEST || FAILED(result) || !identities() || GetCurrentThreadId()!=stats.owner ||
                !pb::read(backend+0x1470,native) || native!=swap) { fault("present-return-invariant");return; }
            const auto value=stamp();accepted(frames.present_end(stats.owner,value),4,value);
        } catch(...) { std::lock_guard lock(mutex);fault("present-return-exception"); }
    }
};
namespace {
bool begin_present(void* p,std::uintptr_t swap,unsigned flags) noexcept { return static_cast<LiveFrames::State*>(p)->present_begin(swap,flags); }
void end_present(void* p,std::uintptr_t swap,unsigned flags,HRESULT result) noexcept { static_cast<LiveFrames::State*>(p)->present_end(swap,flags,result); }
}
LiveFrames::LiveFrames(Streamline& sdk) : state_(new State(sdk)) {
    State* expected{};require(dispatch.compare_exchange_strong(expected,state_),"one-live-frame-dispatch");
}
bool LiveFrames::reserve(std::uintptr_t image,std::string& reason) {
    // The disk/PE-qualified image is known during early loading, but these
    // native bodies may not yet have their final runtime bytes. Reservation
    // only allocates reachable private space; install still gates every full
    // native function before creating or enabling its hook.
    if(image>UINTPTR_MAX-0x55d3000) { reason="live-frame-image-range";return false; }
    std::array<std::uintptr_t,10> targets{};
    for(unsigned i=0;i<targets.size();++i)targets[i]=image+gates[i%gates.size()].rva;
    return pb::reserve_pool(targets,reason);
}
void LiveFrames::install(std::uintptr_t image,std::uintptr_t sdk_device,std::ostream& log) {
    auto& s=*state_;std::lock_guard lock(s.mutex);require(!s.image,"live-frame-already-installed");s.image=image;
    const auto started=GetTickCount64();unsigned attempts{};StampRead first{},last{};
    const char* rejection="";
    if(!pb::read(image+0x51c4148,s.engine) || !s.engine || !pb::read(image+0x51c4480,s.renderer) || !s.renderer ||
        !pb::read(image+0x51c7050,s.backend) || !s.backend)rejection="live-frame-global-anchors";
    else if(!pb::read(s.backend+0x18,s.device) || !s.device || s.device!=sdk_device)rejection="live-frame-SDK-device";
    else for(;;) {
        if(!s.identities()){rejection="live-frame-anchor-links";break;}
        last=read_frame_stamp([](std::uintptr_t at,auto& out) noexcept {return pb::read(at,out);},s.renderer);
        if(++attempts==1)first=last;
        if(last.status==StampReadStatus::stable)break;
        if(!retry_anchor_stamp(last.status,attempts,GetTickCount64()-started)) {
            rejection=last.status==StampReadStatus::changing?"live-frame-stamp-timeout":
                last.status==StampReadStatus::invalid_bank?"live-frame-stamp-bank":"live-frame-stamp-unreadable";
            break;
        }
        Sleep(1);
    }
    log<<"{\"event\":\"frame_anchor_check\",\"policy\":\"bounded-worker-stamp-v1\",\"passed\":"<<(!*rejection)
       <<",\"engine\":"<<s.engine<<",\"renderer\":"<<s.renderer<<",\"backend\":"<<s.backend<<",\"device\":"<<s.device
       <<",\"attempts\":"<<attempts<<",\"attempt_limit\":"<<anchor_stamp_attempt_limit<<",\"timeout_ms\":"<<anchor_stamp_timeout_ms
       <<",\"elapsed_ms\":"<<GetTickCount64()-started<<",\"first_status\":"<<static_cast<unsigned>(first.status)
       <<",\"final_status\":"<<static_cast<unsigned>(last.status)<<",\"first\":["<<first.value.epoch<<','<<first.value.temporal<<','
       <<first.value.bank<<','<<first.epoch_after<<','<<first.bank_after<<"],\"last\":["<<last.value.epoch<<','<<last.value.temporal<<','
       <<last.value.bank<<','<<last.epoch_after<<','<<last.bank_after<<"],\"hooks_created\":false,\"reason\":\""<<rejection<<"\"}\n"<<std::flush;
    require(!*rejection,rejection);require(bool(log),"live-frame-anchor-log");
    for(unsigned i=0;i<gates.size();++i) {
        const auto& g=gates[i];require(pb::verify_code(image,g.rva,g.bytes,g.sha),"live-frame-full-gate");
        auto& h=s.heads[i];h.target=image+g.rva;
        for(unsigned n=0;n<h.original.size();++n)require(pb::read(h.target+n*8ULL,h.original[n]),"live-frame-original-head");
        require(MH_CreateHook(reinterpret_cast<void*>(h.target),reinterpret_cast<void*>(g.entry),&s.originals[g.kind])==MH_OK,"live-frame-create-hook");
        h.created=true;
    }
    for(auto& h:s.heads) { require(MH_QueueEnableHook(reinterpret_cast<void*>(h.target))==MH_OK,"live-frame-queue-hook");h.enabled=true; }
    s.stats.running=true;
    require(MH_ApplyQueued()==MH_OK,"live-frame-enable-hooks");
}
PresentationObserver LiveFrames::observer() noexcept { return {state_,begin_present,end_present}; }
LiveFrameStats LiveFrames::snapshot() const {
    auto& s=*state_;std::lock_guard lock(s.mutex);auto value=s.stats;
    value.completed=s.frames.completed();value.tokens=s.frames.tokens();value.sleeps=s.frames.sleeps();value.markers=s.frames.markers();
    value.frame_id=s.frames.id();value.pending_calls=s.calls.load();return value;
}
void LiveFrames::request_stop() noexcept { state_->stop_requested.store(true); }
bool LiveFrames::stop() noexcept {
    auto& s=*state_;request_stop();const auto began=GetTickCount64();
    // Finish the current frame before closing, without manufacturing markers
    // or beginning another token. A stalled native frame disables on timeout.
    while(GetTickCount64()-began<5000) { if(!snapshot().running)break;Sleep(1); }
    { std::lock_guard lock(s.mutex);if(s.stats.running) { s.stats.running=false;s.frames.stop(); } }
    bool ok=true;
    // DisableHook leaves the queued state unchanged when a target is already
    // disabled. Cancel every created target first, including a setup that
    // queued only some entries, so another route's ApplyQueued cannot revive it.
    for(auto& h:s.heads)if(h.created)
        ok=(MH_QueueDisableHook(reinterpret_cast<void*>(h.target))==MH_OK) && ok;
    for(auto& h:s.heads)if(h.enabled) {
        const auto status=MH_DisableHook(reinterpret_cast<void*>(h.target));
        if(status==MH_OK || status==MH_ERROR_DISABLED)h.enabled=false;else ok=false;
    }
    const auto drain=GetTickCount64();while(s.calls.load() && GetTickCount64()-drain<30000)Sleep(1);
    if(s.calls.load())return false;
    std::lock_guard lock(s.mutex);
    for(auto& h:s.heads)if(h.created) {
        std::array<std::uint64_t,4> actual{};bool read=true;
        for(unsigned n=0;n<actual.size();++n)read=pb::read(h.target+n*8ULL,actual[n]) && read;
        h.restored=read && actual==h.original;ok=h.restored && ok;
    }
    for(const auto& g:gates)if(s.image)ok=pb::verify_code(s.image,g.rva,g.bytes,g.sha) && ok;
    s.stats.restored=ok;return ok;
}
DriverSleepStatus LiveFrames::driver_status(IUnknown* native_device) const {
    // Serialize the worker's bounded driver readback with native frame calls;
    // options/shutdown occur only before install or after callback drain.
    std::lock_guard lock(state_->mutex);
    return driver_sleep_status(native_device);
}
void LiveFrames::describe(std::ostream& log, unsigned requested_mode) const {
    auto& s=*state_;std::lock_guard lock(s.mutex);LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
    log<<"{\"event\":\"frame_hooks_installed\",\"engine\":"<<s.engine<<",\"renderer\":"<<s.renderer<<",\"backend\":"<<s.backend
       <<",\"qpc_frequency\":"<<frequency.QuadPart<<",\"sdk_mode\":"<<requested_mode<<",\"sample_frames\":8,\"native_gates\":[";
    for(unsigned i=0;i<gates.size();++i) {
        const auto& g=gates[i];const auto& h=s.heads[i];if(i)log<<',';
        log<<"{\"kind\":"<<g.kind<<",\"target\":"<<h.target<<",\"rva\":"<<g.rva<<",\"bytes\":"<<g.bytes
           <<",\"sha256\":\""<<g.sha<<"\",\"original_head_sha256\":\""<<pb::bytes_sha256(h.original.data(),32)<<"\"}";
    }
    log<<"]}\n"<<std::flush;
}
void LiveFrames::write(std::ostream& log) const {
    auto& s=*state_;std::lock_guard lock(s.mutex);
    for(unsigned i=0;i<s.sample_count;++i) {
        const auto& v=s.samples[i];log<<"{\"event\":\"frame_stage\",\"id\":"<<v.id<<",\"stage\":"<<v.stage<<",\"thread\":"<<v.thread
            <<",\"result\":"<<v.result<<",\"qpc\":"<<v.qpc<<",\"tokens\":"<<v.tokens<<",\"sleeps\":"<<v.sleeps<<",\"markers\":"<<v.markers
            <<",\"stamp\":["<<v.stamp.epoch<<','<<v.stamp.temporal<<','<<v.stamp.bank<<','<<v.stamp.valid<<"]}\n";
    }
    for(unsigned i=0;i<s.heads.size();++i) { const auto& h=s.heads[i];if(!h.created)continue;
        log<<"{\"event\":\"frame_hook_restoration\",\"kind\":"<<gates[i].kind<<",\"target\":"<<h.target<<",\"restored\":"<<h.restored
           <<",\"original_head_sha256\":\""<<pb::bytes_sha256(h.original.data(),32)<<"\"}\n";
    }
    log<<"{\"event\":\"frame_finished\",\"completed\":"<<s.frames.completed()<<",\"tokens\":"<<s.frames.tokens()<<",\"sleeps\":"<<s.frames.sleeps()
       <<",\"markers\":"<<s.frames.markers()<<",\"faults\":"<<s.stats.faults<<",\"owner\":"<<s.stats.owner<<",\"pending_calls\":"<<s.calls.load()
       <<",\"frame_active\":"<<s.stats.frame_active<<",\"native_bytes_restored\":"<<s.stats.restored<<",\"present_tests\":"<<s.stats.present_tests
       <<",\"untracked_presents\":"<<s.stats.untracked_presents<<",\"reason\":\""<<(s.reason?s.reason:"")<<"\"}\n"<<std::flush;
}
}
extern "C" void mhw_reflex_boundary_dispatch(unsigned kind,mhw::reflex::boundaries::Frame* frame) noexcept {
    auto* s=mhw::reflex::dispatch.load();s->calls.fetch_add(1);
    const auto prior=GetLastError();const bool ticket=s->enter(kind,*frame);SetLastError(prior);
    bool completed=false;
    __try { mhw_reflex_boundary_forward(s->originals[kind],frame);completed=true; }
    __finally { const auto error=GetLastError();s->leave(kind,ticket,completed);s->calls.fetch_sub(1);SetLastError(error); }
}
