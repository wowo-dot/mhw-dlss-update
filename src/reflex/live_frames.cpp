#include "live_frames.hpp"
#include "frame_session.hpp"
#include "frame_stamp.hpp"
#include "frame_boundaries.hpp"
#include "../projection_binding/recorder.hpp"
#include "../projection_binding/memory.hpp"
#include "../astera/astera_hooks.hpp"
#include "../frame_generation/game_session.hpp"
#include <MinHook.h>
#include <atomic>
#include <array>
#include <cstring>
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
// Under Astera Loader the three functions join AL's shared chains (al.hooks):
// Main:Update is also hooked by AL's own frame tick and by other plugins. The
// pattern is each function's first head_size original bytes (read from the
// decoded image; the full-function SHA-256 covers them). AL checks them, even
// after another plugin's patch, and the gate hashes them with the live rest.
// Main:Update's order -1000 puts the Reflex simulation markers outside the
// other per-frame detours.
struct Gate { unsigned rva, bytes; const char* sha; unsigned kind; void(*entry)(); const char* name; const char* pattern; int order; };
constexpr std::array gates{
    Gate{0x1af2b60,4864,"9f6f5b3ac1a521ddd59351e41cc66c17dfc453ccd1b969afd4d59787bfef795d",0,reflex_boundary_update,
        "mhw_dlss Main:Update","40 53 48 81 EC 80 00 00 00 48 8B D9 E8 5F C6 76",-1000},
    Gate{0x229a080,1428,"961cf9969c28abc6b5b63ba9fb476978555eb188fdc173940d7ed81fc566a4e0",2,reflex_boundary_publish,
        "mhw_dlss Render:Publish","48 89 5C 24 10 48 89 6C 24 18 56 57 41 56 48 83",0},
    Gate{0x23a7f50,69,"1183e1807b99ec2ada753fa147af8c59b1f99f29bbbf6176ed7479fd48e09a46",11,reflex_boundary_submit,
        "mhw_dlss Render:Submit","40 53 48 83 EC 20 48 8B D9 E8 D2 16 14 00 84 C0",0}
};
constexpr unsigned head_size=16;
using Head=std::array<std::uint8_t,head_size>;
// "40 53 ...": exactly head_size bytes and no wildcards; they are original code.
constexpr bool parse_head(const char* p, Head& out) noexcept { return mhw::astera::pattern_bytes<head_size>(p,out); }
constexpr bool heads_valid() noexcept { for(const auto& g:gates) { Head h{};if(!parse_head(g.pattern,h))return false; } return true; }
static_assert(heads_valid(),"each al.hooks pattern is the function's first 16 bytes");
// Under AL the chain owns the entry: verify the original head with the live rest.
bool verify_chained(std::uintptr_t image, const Gate& g) noexcept {
    Head head{};return parse_head(g.pattern,head) && pb::verify_code_with_head(image,g.rva,g.bytes,g.sha,head.data(),head_size);
}
// The game's exit (clean image a78ea69b...1d80): the window procedure's
// WM_DESTROY (sub_1422E3160) calls the setter with the destroyed window; when
// it is the swap chain's (cD3DRender+0x1460), cD3DRender+0x1482 becomes 1 and
// stays set (only initForD3D11/12 clear it). From then on present12 jumps past
// Present, so the engine frame running at that moment, and any after it, ends
// without one. The quit flag stops the game thread's loop shortly after.
// Read-only spans; when one differs the flag is not read (dlaa-identity-path.md).
struct Span { unsigned rva, bytes; const char* sha; };
constexpr Span exit_spans[]{
    {0x23acbf0,17,"70eea0a022c36a8fec38c3f39f1f9f5ebfb3cb0a32f4cc3eae718b7fcd4105bb"},  // the setter
    {0x23a9e20,13,"ba1f3b884c9fe6772b29e07ac3eb09e20206312ac7bcf63df64f00b5ce6860d8"},  // present12: cmp [rbx+1482h],0; jnz
    {0x23aa22c,91,"13bb557a8281add92426ff34f25079acb4181af042c2baad157ad1938817c09d"}   // the jump target: return without Present
};
constexpr std::uintptr_t window_destroyed_offset=0x1482;
bool verify_exit(std::uintptr_t image) noexcept {
    for(const auto& s:exit_spans)if(!pb::verify_code(image,s.rva,s.bytes,s.sha))return false;
    return true;
}
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
    // AL: the hooks are in AL's al.hooks chains (handles), not MinHook patches.
    bool chained{};
    std::array<std::uint32_t,3> handles{};
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
    // The failed identity links as a mask (0: all hold); see LiveFrameStats::Fault.
    unsigned identity_links() const noexcept {
        std::uintptr_t e{},r{},b{},d{},link{};unsigned failed{};
        if(!pb::read(image+0x51c4148,e) || e!=engine)failed|=1;
        if(!pb::read(image+0x51c4480,r) || r!=renderer)failed|=2;
        if(!pb::read(image+0x51c7050,b) || b!=backend)failed|=4;
        if(!pb::read(backend+0x18,d) || d!=device)failed|=8;
        if(!pb::read(engine+0xaf7b0,link) || link!=renderer)failed|=16;
        if(!pb::read(renderer+0x78,link) || link!=backend)failed|=32;
        return failed;
    }
    bool identities() const noexcept { return !identity_links(); }
    // On a qualified backend only (after the identity check).
    bool window_destroyed() const noexcept {
        std::uint8_t destroyed{};return stats.exit_gate && pb::read(backend+window_destroyed_offset,destroyed) && destroyed;
    }
    void close() noexcept { stats.running=false;stats.frame_active=false;stats.closed=true;frames.stop(); }
    unsigned call{}; // the hooked call being checked (LiveFrameStats::Fault::call)
    void fault(const char* why, unsigned code=0, unsigned stage=0) noexcept {
        if(!stats.faults) {
            ++stats.faults;reason=why;
            stats.fault={call,GetCurrentThreadId(),stats.owner,identity_links(),code,stage,frames.id(),stats.frame_active};
        }
        stats.running=false;stats.frame_active=false;frames.stop();
    }
    bool accepted(sl::Result result, unsigned stage, FrameStamp value) noexcept {
        LARGE_INTEGER time{}; QueryPerformanceCounter(&time);
        if(sample_count<samples.size() && frames.id()<=8)
            samples[sample_count++]={frames.id(),stage,GetCurrentThreadId(),static_cast<unsigned>(result),
                static_cast<std::uint64_t>(time.QuadPart),frames.tokens(),frames.sleeps(),frames.markers(),value};
        if(result!=sl::Result::eOk || frames.disabled()) {
            fault("frame-policy-or-sdk",static_cast<unsigned>(result)|(frames.disabled()?0x100u:0u),stage);return false;
        }
        return true;
    }
    bool enter(unsigned kind, const boundaries::Frame& f) noexcept {
        try {
            std::lock_guard lock(mutex);
            if(!stats.running)return false;
            call=kind;
            if(kind==0 && stop_requested.load()) { stats.running=false;frames.stop();return false; }
            const auto thread=GetCurrentThreadId();
            if(!identities() || (stats.owner && stats.owner!=thread)) { fault("frame-owner-or-identity");return false; }
            const auto object=f.gpr[1];
            if(object!=(kind==0?engine:kind==2?renderer:backend)) { fault("native-this-identity");return false; }
            const auto value=stamp();
            if(kind==0) {
                if(stats.frame_active) { fault("nested-engine-frame");return false; }
                if(window_destroyed()) { close();return false; } // No frame the game cannot present.
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
            call=100+kind;
            if(!completed) { fault("native-frame-exception");return; }
            if(!identities() || GetCurrentThreadId()!=stats.owner) { fault("frame-return-identity");return; }
            const auto value=stamp();
            if(!value.valid) { fault("frame-return-stamp");return; }
            if(kind==11)accepted(frames.render_end(stats.owner,value),2,value);
            if(kind==0) {
                // The game's exit: its window went away during this frame, so
                // present12 skipped Present. Close without closing markers.
                const bool destroyed=window_destroyed();
                if(destroyed && !frames.presented()) { close();return; }
                if(accepted(frames.end(stats.owner),5,value)) {
                    stats.frame_active=false;
                    if(destroyed)close();
                    else if(stop_requested.load()) { stats.running=false;frames.stop(); }
                }
            }
        } catch(...) { std::lock_guard lock(mutex);fault("frame-return-exception"); }
    }
    bool present_begin(std::uintptr_t swap, unsigned flags) noexcept {
        try {
            std::lock_guard lock(mutex);
            call=20;
            if(flags&DXGI_PRESENT_TEST) { ++stats.present_tests;return false; }
            if(!stats.running || !stats.frame_active) {
                ++stats.untracked_presents;
                frame_generation::game::present_begin(nullptr,0); // No token: FG Off for this Present.
                return false;
            }
            std::uintptr_t native{};
            if(!identities() || GetCurrentThreadId()!=stats.owner || !pb::read(backend+0x1470,native) || native!=swap) {
                fault("present-owner-or-identity");return false;
            }
            const auto value=stamp();
            // Owner thread, token still valid (before PresentStart): tag the
            // matching FG inputs, set constants and options for this Present.
            frame_generation::game::present_begin(frames.input_token(stats.owner),value.temporal);
            const bool ok=accepted(frames.present_begin(stats.owner,value),3,value);
            return ok;
        } catch(...) { std::lock_guard lock(mutex);fault("present-entry-exception");return false; }
    }
    void present_end(std::uintptr_t swap, unsigned flags, HRESULT result) noexcept {
        try {
            std::lock_guard lock(mutex);
            if(!stats.running)return;
            call=21;
            std::uintptr_t native{};
            // stage: the first failed condition, in order.
            const unsigned failed=flags&DXGI_PRESENT_TEST?1u:FAILED(result)?2u:!identities()?3u:GetCurrentThreadId()!=stats.owner?4u:
                !pb::read(backend+0x1470,native) || native!=swap?5u:0u;
            if(failed) { fault("present-return-invariant",static_cast<unsigned>(result),failed);return; }
            const auto value=stamp();accepted(frames.present_end(stats.owner,value),4,value);
            frame_generation::game::present_end(true);
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
    s.chained=mhw::astera::active();
    if(s.chained) {
        require(mhw::astera::game_hooks_available(),"live-frame-al-hooks-missing");
        for(unsigned i=0;i<gates.size();++i) {
            const auto& g=gates[i];require(verify_chained(image,g),"live-frame-full-gate");
            auto& h=s.heads[i];h.target=image+g.rva;
            // The original head: the pattern's 16 bytes, then live code (verified).
            Head head{};parse_head(g.pattern,head);std::memcpy(h.original.data(),head.data(),head_size);
            for(unsigned n=head_size/8;n<h.original.size();++n)require(pb::read(h.target+n*8ULL,h.original[n]),"live-frame-original-head");
        }
        s.stats.exit_gate=verify_exit(image);
        // Each hook is live as soon as AL accepts it. Entries wait on this
        // lock until all three are in; Main:Update joins last.
        s.stats.running=true;
        for(auto i=static_cast<unsigned>(gates.size());i-->0;) {
            const auto& g=gates[i];auto& h=s.heads[i];
            s.handles[i]=mhw::astera::game_hook({g.order,g.name,g.pattern,reinterpret_cast<void*>(h.target),
                reinterpret_cast<void*>(g.entry),&s.originals[g.kind]});
            require(s.handles[i]!=0,"live-frame-al-hook-refused");h.created=true;
            require(mhw::astera::game_hook_state(s.handles[i])==mhw::astera::GameHookState::active && s.originals[g.kind],"live-frame-al-hook-inactive");
            h.enabled=true;
        }
        return;
    }
    for(unsigned i=0;i<gates.size();++i) {
        const auto& g=gates[i];require(pb::verify_code(image,g.rva,g.bytes,g.sha),"live-frame-full-gate");
        auto& h=s.heads[i];h.target=image+g.rva;
        for(unsigned n=0;n<h.original.size();++n)require(pb::read(h.target+n*8ULL,h.original[n]),"live-frame-original-head");
        require(MH_CreateHook(reinterpret_cast<void*>(h.target),reinterpret_cast<void*>(g.entry),&s.originals[g.kind])==MH_OK,"live-frame-create-hook");
        h.created=true;
    }
    for(auto& h:s.heads) { require(MH_QueueEnableHook(reinterpret_cast<void*>(h.target))==MH_OK,"live-frame-queue-hook");h.enabled=true; }
    s.stats.exit_gate=verify_exit(image);
    s.stats.running=true;
    require(MH_ApplyQueued()==MH_OK,"live-frame-enable-hooks");
}
PresentationObserver LiveFrames::observer() noexcept { return {state_,begin_present,end_present}; }
LiveFrameStats LiveFrames::snapshot() const {
    auto& s=*state_;std::lock_guard lock(s.mutex);auto value=s.stats;
    value.completed=s.frames.completed();value.tokens=s.frames.tokens();value.sleeps=s.frames.sleeps();value.markers=s.frames.markers();
    value.frame_id=s.frames.id();value.pending_calls=s.calls.load();value.fault_reason=s.reason;return value;
}
void LiveFrames::request_stop() noexcept { state_->stop_requested.store(true); }
bool LiveFrames::stop() noexcept {
    auto& s=*state_;request_stop();const auto began=GetTickCount64();
    // Finish the current frame before closing, without manufacturing markers
    // or beginning another token. A stalled native frame disables on timeout.
    while(GetTickCount64()-began<5000) { if(!snapshot().running)break;Sleep(1); }
    { std::lock_guard lock(s.mutex);if(s.stats.running) { s.stats.running=false;s.frames.stop(); } }
    bool ok=true;
    if(s.chained) {
        // AL keeps the entry patched for its chain; taking the detours out
        // relinks it. Calls already inside a detour finish, then drain.
        for(unsigned i=0;i<s.heads.size();++i) { auto& h=s.heads[i];if(!h.created)continue;
            if(mhw::astera::set_game_hook_enabled(s.handles[i],false))h.enabled=false;else if(h.enabled)ok=false;
        }
        const auto drain=GetTickCount64();while(s.calls.load() && GetTickCount64()-drain<30000)Sleep(1);
        if(s.calls.load())return false;
        std::lock_guard lock(s.mutex);
        // "restored" under AL: out of the chain and the function's code intact.
        for(unsigned i=0;i<s.heads.size();++i) { auto& h=s.heads[i];if(!h.created)continue;
            const auto state=mhw::astera::game_hook_state(s.handles[i]);
            h.restored=!h.enabled && (state==mhw::astera::GameHookState::disabled || state==mhw::astera::GameHookState::failed) &&
                verify_chained(s.image,gates[i]);
            ok=h.restored && ok;
        }
        s.stats.restored=ok;return ok;
    }
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
           <<",\"sha256\":\""<<g.sha<<"\",\"original_head_sha256\":\""<<pb::bytes_sha256(h.original.data(),32)<<"\",\"chain\":\""
           <<(s.chained?"al.hooks":"minhook")<<"\",\"order\":"<<(s.chained?g.order:0)<<",\"handle\":"<<s.handles[i]
           <<",\"state\":"<<(s.chained?static_cast<unsigned>(mhw::astera::game_hook_state(s.handles[i])):1u)<<"}";
    }
    log<<"],\"exit_gate\":"<<s.stats.exit_gate<<"}\n"<<std::flush;
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
           <<",\"original_head_sha256\":\""<<pb::bytes_sha256(h.original.data(),32)<<"\",\"chain\":\""<<(s.chained?"al.hooks":"minhook")
           <<"\",\"state\":"<<(s.chained?static_cast<unsigned>(mhw::astera::game_hook_state(s.handles[i])):h.enabled?1u:2u)<<"}\n";
    }
    log<<"{\"event\":\"frame_finished\",\"completed\":"<<s.frames.completed()<<",\"tokens\":"<<s.frames.tokens()<<",\"sleeps\":"<<s.frames.sleeps()
       <<",\"markers\":"<<s.frames.markers()<<",\"faults\":"<<s.stats.faults<<",\"owner\":"<<s.stats.owner<<",\"pending_calls\":"<<s.calls.load()
       <<",\"frame_active\":"<<s.stats.frame_active<<",\"native_bytes_restored\":"<<s.stats.restored<<",\"present_tests\":"<<s.stats.present_tests
       <<",\"untracked_presents\":"<<s.stats.untracked_presents<<",\"closed\":"<<s.stats.closed<<",\"reason\":\""<<(s.reason?s.reason:"")<<"\"}\n"<<std::flush;
}
}
extern "C" void mhw_reflex_boundary_dispatch(unsigned kind,mhw::reflex::boundaries::Frame* frame) noexcept {
    auto* s=mhw::reflex::dispatch.load();s->calls.fetch_add(1);
    const auto prior=GetLastError();const bool ticket=s->enter(kind,*frame);SetLastError(prior);
    bool completed=false;
    __try { mhw_reflex_boundary_forward(s->originals[kind],frame);completed=true; }
    __finally { const auto error=GetLastError();s->leave(kind,ticket,completed);s->calls.fetch_sub(1);SetLastError(error); }
}
