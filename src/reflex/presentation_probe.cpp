// Native presentation transaction; starts Off, live frame probe arms Reflex On.
// Public API detours use declared Windows x64
// signatures. No vendor call executes under DllMain/loader lock. Hash-qualified
// callers identify the retail game's factory observations and main DX12 device.
// Manual SL initialization occurs after the native main device is created and
// before returning to the initializer's queue/swap-chain creation instructions.
// Public creation calls preserve arguments/results/Win32 last-error.
#include "streamline.hpp"
#include "presentation.hpp"
#include "../astera/astera_hooks.hpp"
#include "native_factory.hpp"
#ifdef MHW_REFLEX_TEMPORAL
#include "temporal_coexistence.hpp"
#ifdef MHW_FG_SCENE_PROBE
#include "../scaled_temporal/stream_route.hpp"
#endif
#endif
#ifdef MHW_FG_GAME
#include "../frame_generation/game_session.hpp"
#include "../frame_generation/display_refresh.hpp"
#include "../frame_generation/crash_log.hpp"
#include "frame_generation_policy.hpp"
#include "../perf/frame_stats.hpp"
#include "../perf/toggles.hpp"
#include "../perf/sampler.hpp"
#include <algorithm>
#ifdef MHW_ALPHA
#include "../alpha/config.hpp"
#include "../alpha/menu.hpp"
#include "../alpha/status.hpp"
#include "../alpha/version.hpp"
#include "reflex_mode.hpp"
#include "../../third_party/al/al_plugin.h"
#include "../temporal_present/observer.hpp"
#include "../continuous_dlaa/runtime.hpp"
#include "../render_resolution/runtime.hpp"
#include "../scaled_temporal/stream_route.hpp"
#include "../bridge_probe/readable_region.hpp"
#include "../perf/hook_timing.hpp"
#include <cstddef>
#include <iterator>
#include <vector>
// Alpha build: mhw_dlss.ini replaces the research sidecars, the session arms
// itself once the game is ready and runs until exit, with light logging.
namespace {
mhw::alpha::Config alpha_config;
// The plugin's own folder (empty when the DLL sits in the game folder itself):
// the only place inside the game folder the alpha reads its runtime from and logs to.
// Lexical, as the configuration's paths are: under MO2 the module reports its
// virtual game-folder path while canonical() resolves to the mod's real folder.
std::wstring alpha_home;
// Folder holding mhw_dlss.ini: a failure before the log opens is recorded there.
std::filesystem::path alpha_base;
// The plugin log (defined with the Astera Loader entry below).
void loader_log(al_log_level_t level, const std::string& text) noexcept;
}
#endif
#include <atomic>
#include <cstdio>
#include <memory>
// "<dll>.fg-mode": "off" runs without DLSS-G (A/B diagnostic); "2x".."8x" sets
// the starting frame multiplier (default 2x). Live changes: "<log>.mfg-<n>".
// fg_plugin: DLSS-G requested at start, then usable (requested and supported;
// the device's attach decides). fg_loaded: loaded now (Streamline paces the
// swap chain, Reflex stays on); v0.27.4 loads and unloads it with the swap
// chain (fg-live-toggle.md), so the worker updates it after each switch.
namespace { bool fg_plugin = true, fg_loaded = false; unsigned fg_multiplier = 2;
            std::atomic<unsigned> fg_support_state{};std::atomic<bool> fg_loaded_state{}; }
#ifdef MHW_ALPHA
// The ini's frame generation mode and dynamic target (game_session.hpp).
namespace { mhw::frame_generation::game::Mode fg_mode{}; unsigned fg_target{}; }
#endif
#endif
#ifdef MHW_REFLEX_FRAME_PROBE
#include "live_frames.hpp"
#include "driver_status.hpp"
#endif
#include "../projection_binding/memory.hpp"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <MinHook.h>
#include <intrin.h>
#include <atomic>
#include <array>
#include <fstream>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <string_view>

namespace {
namespace pb = mhw::projection_binding;
using FactoryFn = HRESULT(WINAPI*)(UINT, REFIID, void**);
using DeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
HMODULE module{};
FactoryFn factory_original{}; DeviceFn device_original{};
std::array<void*, 2> targets{};
// Own one process-lived loader reference per native API module. Hooks and
// forwarding trampolines may outlive the worker; do not unload their targets.
std::array<HMODULE, 2> creation_modules{};
std::array<std::array<std::uint64_t, 4>, 2> original_heads{};
std::array<bool, 2> created{}, enabled{};
std::atomic<unsigned> active{};
std::atomic<bool> accepting{}, completed{};
std::uintptr_t image{};
std::filesystem::path runtime, output;
std::ofstream log;
std::mutex sdk_mutex;
// These are process-lived. A failed shutdown retains the native device pin;
// no borrowed object is destroyed while the SDK could still refer to it.
// Explicit worker cleanup only. Never call vendor shutdown from CRT/DllMain
// teardown if the process exits before the bounded test has stopped.
auto& device_pin = *new Microsoft::WRL::ComPtr<ID3D12Device>;
auto& sdk = *new mhw::reflex::Streamline;
mhw::reflex::Presentation* presentation{};
#ifdef MHW_REFLEX_OWNERSHIP_PROBE
bool ownership_written{};
#endif
std::array<mhw::reflex::PresentationHook, mhw::reflex::Presentation::hook_capacity> presentation_heads{};
static_assert(sizeof(mhw::reflex::PresentationHook) == 48, "versioned retained-head metadata");
std::filesystem::path stop_file;
#ifdef MHW_REFLEX_FRAME_PROBE
std::filesystem::path arm_file;
mhw::reflex::LiveFrames* live_frames{};
bool frames_written{};
#ifdef MHW_REFLEX_TEMPORAL
bool temporal_cleanup_written{};
bool finish_temporal() noexcept {
    const bool clean=mhw::reflex::temporal::finish();
    try {
        if(!temporal_cleanup_written){
            mhw::reflex::temporal::receipt(log,"drained",device_pin.Get());
            temporal_cleanup_written=true;
        }
    }catch(...){return false;}
    return clean;
}
#endif
bool mode_change_attempted{}, mode_restored{};
constexpr auto capture_mode = sl::ReflexMode::eLowLatency;
// The player's Reflex mode (reflex_mode.hpp): requested by the ini and the
// menu, applied by the worker. Research builds always use capture_mode.
std::atomic<unsigned> reflex_requested{static_cast<unsigned>(capture_mode)}, reflex_applied{3};
constexpr unsigned capture_seconds = 600;
#endif
bool initialized{}, device_checked{}, success{};
unsigned owner{}, qualified_factories{}, qualified_devices{};
std::string failure;
// AL (alpha): al_plugin_init starts the worker and waits for this event,
// set once the worker accepts the game's device or failed before that. Init
// never waits for the NGX stage, which follows device creation.
HANDLE startup_ready{};
std::string startup_failure;
void signal_startup() noexcept { if (startup_ready) SetEvent(startup_ready); }
// AL's al.game reported this exe's SHA-256: the worker skips re-hashing it.
bool exe_hash_verified{};
// AL: the swap-chain chains were missing when the game created its swap chain.
std::atomic<bool> swapchain_chains_missing{};

void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
void api(sl::Result result, const char* reason) {
    if (result != sl::Result::eOk)
        throw std::runtime_error(std::string(reason) + ":" + std::to_string(static_cast<unsigned>(result)));
}
std::string quoted(const std::string& value) {
    std::ostringstream out; out << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c);
        else out << c;
    }
    return out.str() + '"';
}
struct Call { Call() { active.fetch_add(1); } ~Call() { active.fetch_sub(1); } };
bool gate(unsigned rva) {
    if (rva == 0x23a15fd)
        return pb::verify_code(image, 0x23a0950, 4158, "138af20800e364c70b5a9228f6d294fbcccad1cf106e0013e84d008b2481e4bc");
    return pb::verify_code(image, 0x23a48e0, 11101, "8757a054cc1c203128fdfb73ac88975dda7dc73ef5ef1a53a80f22cd0eb8e500");
}
#ifdef MHW_REFLEX_FRAME_PROBE
bool driver_mode(const char* phase, bool expected, bool enforce=true) {
    const auto state=live_frames?live_frames->driver_status(device_pin.Get()):mhw::reflex::driver_sleep_status(device_pin.Get());
    log << "{\"event\":\"driver_sleep_status\",\"phase\":" << quoted(phase)
        << ",\"device\":" << reinterpret_cast<std::uintptr_t>(device_pin.Get())
        << ",\"result\":" << state.result << ",\"low_latency\":" << state.low_latency
        << ",\"query_id\":" << 0xaef96ca1U << ",\"structure_version\":" << (136U | (1U<<16)) << "}\n" << std::flush;
    return state.result==0 && (!enforce || state.low_latency==expected);
}
void mode_options(const char* phase, sl::ReflexMode mode, sl::Result result) {
    log << "{\"event\":\"mode_options\",\"phase\":" << quoted(phase) << ",\"mode\":" << static_cast<unsigned>(mode)
        << ",\"result\":" << static_cast<unsigned>(result)
        << ",\"frame_limit_us\":0,\"use_markers_to_optimize\":false,\"boost_requested\":" << (mode==sl::ReflexMode::eLowLatencyWithBoost) << "}\n" << std::flush;
}
#ifdef MHW_ALPHA
const char* reflex_mode_name(unsigned mode) noexcept {
    return mode==0?"Off":mode==2?"Boost":"On";
}
// The worker, under sdk_mutex: applies the requested mode if it changed. Off
// is refused while DLSS-G is loaded (it needs Reflex); the menu already keeps
// players from asking for that. True when a new mode was applied.
bool apply_reflex_mode(bool loaded) {
    const auto requested=reflex_requested.load();
    if(requested==reflex_applied.load()||requested>2)return false;
    if(requested==0&&loaded){
        log << "{\"event\":\"reflex_mode_refused\",\"mode\":0,\"reason\":\"frame generation needs Reflex\"}\n" << std::flush;
        reflex_requested.store(reflex_applied.load());return false;
    }
    const auto mode=static_cast<sl::ReflexMode>(requested);
    const auto result=sdk.options(mode);mode_options("live",mode,result);
    if(result==sl::Result::eOk){reflex_applied.store(requested);mhw::alpha::status::reflex_mode(reflex_mode_name(requested));return true;}
    reflex_requested.store(reflex_applied.load());return false;
}
// A live preset change the game did not render through (paused, minimized):
// retried every 2 s, up to 15 times, unless the menu asks for another mode.
struct DlssRetry {std::uint64_t at{};mhw::continuous_dlaa::ModeRequest request{};unsigned count{};};
// Frame generation was configured at arming and has not stopped since: it is
// armed whenever DLSS runs with DLSS-G loaded (its inputs are DLSS's).
bool fg_ready{};
// v0.27.4 (fg-live-toggle.md): the game's own "make the buffers again"
// request and its latency waitable, checked once at arming.
// - sub_14229C320 (the HDR switch) sets 0x4000 in sRender+0x1F548 when the
//   chain's buffers must be made again; sub_14229B020 (gated by
//   frame_boundaries.cpp) consumes it at the next frame and resizes;
// - sub_1423ADE30 waits on cD3DRender+0x10F0 (sRender+0x78), which
//   cD3DRender::initForD3D12 (gated) fills from GetFrameLatencyWaitableObject.
bool game_chain_ready{};
bool game_chain_qualified() noexcept {
    std::uintptr_t renderer{}, backend{};
    return pb::verify_code(image,0x229c320,0x44,"4d688f8b830ca460e4ec196111c5f0cd73dbc95089b2a7ebcb6c2bbc4a22a0c0")&&
           pb::verify_code(image,0x23ade30,0x47,"6c9808bc6ea7ff818d46b1e8d91ff3abfd6e0db181b1f9b503370707746b1900")&&
           pb::verify_code(image,0x229b020,2420,"a4a09901e108fe38dabfb6d9df3a52f9d5ccd68a12b8b8e6f7a9ea671cbf5e90")&&
           pb::verify_code(image,0x23a48e0,11101,"8757a054cc1c203128fdfb73ac88975dda7dc73ef5ef1a53a80f22cd0eb8e500")&&
           pb::read(image+0x51c4480,renderer)&&renderer&&pb::read(renderer+0x78,backend)&&backend;
}
std::uintptr_t game_waitable_slot() noexcept {
    std::uintptr_t renderer{}, backend{};
    return pb::read(image+0x51c4480,renderer)&&renderer&&pb::read(renderer+0x78,backend)&&backend?backend+0x10F0:0;
}
bool game_resize_request() noexcept {
    std::uintptr_t renderer{};
    if(!pb::read(image+0x51c4480,renderer)||!renderer)return false;
    __try{_InterlockedOr(reinterpret_cast<volatile long*>(renderer+0x1F548),0x4000);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
// A switch in progress: asked at, game requests made, the presentation's
// outcome count before it, and what was asked.
struct GenerationSwitch {std::uint64_t asked_at{};unsigned tries{};std::uint64_t outcomes{};bool want{},active{};};
std::uint64_t switch_outcomes(const mhw::reflex::PresentationStats& v) noexcept {return v.switches+v.switch_failures+v.switch_refusals;}
// Worker, every loop: DLSS-G is loaded exactly while frame generation is on
// and DLSS runs. A change asks the presentation for a switch and the game for
// its buffer remake, in which the switch happens; frame generation is Off
// with its inputs settled before an unload, armed again after a load.
void sync_generation(GenerationSwitch& g){
    namespace fg=mhw::frame_generation::game;namespace cd=mhw::continuous_dlaa;
    if(!fg_plugin||!game_chain_ready||!fg_ready)return;
    const auto v=presentation->snapshot();
    if(!v.identity)return;
    const auto now=GetTickCount64();
    if(g.active){
        if(switch_outcomes(v)==g.outcomes){
            if(now-g.asked_at<1500)return;
            // The game did not resize (paused, minimized): ask again, up to 20 times.
            if(++g.tries<20){game_resize_request();g.asked_at=now;return;}
            presentation->request_switch(fg_loaded);  // nothing to do at a later resize
            log<<"{\"event\":\"fg_switch\",\"want\":"<<g.want<<",\"done\":false,\"reason\":\"no resize\"}\n"<<std::flush;
            g={};return;
        }
        fg_loaded=v.frame_generation_loaded;fg_loaded_state.store(fg_loaded);
        log<<"{\"event\":\"fg_switch\",\"want\":"<<g.want<<",\"done\":true,\"loaded\":"<<fg_loaded<<",\"reason\":"<<v.switch_reason
           <<",\"switches\":"<<v.switches<<",\"failures\":"<<v.switch_failures<<",\"refusals\":"<<v.switch_refusals
           <<",\"hr\":"<<v.switch_last_hr<<",\"waitable_failures\":"<<v.waitable_failures<<",\"tries\":"<<g.tries
           <<",\"ms\":"<<now-g.asked_at<<",\"buffers\":"<<v.buffers<<"}\n"<<std::flush;
        mhw::alpha::status::reflex_mode(reflex_mode_name(reflex_applied.load()));
        if(!fg_loaded&&reflex_applied.load()<=2){
            // DLSS-G paces 6x with a frame limit that outlives its unload
            // (perf13c: a flat 208 fps = 1250 / 6 after an unload at 6x, DLSS on
            // or off, until the player set Reflex Off: 253). Setting the same
            // options again does not reach the driver (v0.28.1, game-137: still
            // 208), a change does: Off, then the player's mode (v0.28.2).
            std::lock_guard lock(sdk_mutex);
            const auto mode=static_cast<sl::ReflexMode>(reflex_applied.load());
            mode_options("after_unload_off",sl::ReflexMode::eOff,sdk.options(sl::ReflexMode::eOff));
            mode_options("after_unload",mode,sdk.options(mode));
        }
        if(fg_loaded&&cd::live_modes()&&!cd::dlss_off())fg::arm(true);
        g={};return;
    }
    const bool want=fg::status().enabled&&cd::live_modes()&&!cd::dlss_off();
    if(want==fg_loaded)return;
    if(!want){
        // DLSS-G Off at the next Present and no input slot in use.
        if(fg::status().armed)fg::arm(false);
        if(fg::status().on||!fg::inputs_settled())return;
    }
    if(!presentation->request_switch(want))return;
    g={now,0,switch_outcomes(v),want,true};
    game_resize_request();
}
// Worker: after a scene change, frame generation takes the new scene
// rectangle (centred in the back buffer); false if it does not fit.
bool configure_generation_scene(){
    unsigned sw{},sh{},ww{},wh{};mhw::reflex::temporal::scene_extent(sw,sh,ww,wh);
    const auto chain=presentation->snapshot();
    if(!sw||!sh||sw>chain.width||sh>chain.height||ww!=chain.width||wh!=chain.height)return false;
    mhw::frame_generation::game::back_buffer(chain.format);
    mhw::frame_generation::game::configure(sdk,chain.width,chain.height,{(chain.width-sw)/2,(chain.height-sh)/2,sw,sh});
    return true;
}
// The DLSS status after a change: the player's Off, or the mode and sizes.
void dlss_status(){
    namespace cd=mhw::continuous_dlaa;
    if(cd::dlss_off()){mhw::alpha::status::dlss(mhw::alpha::status::Stage::player_off);return;}
    unsigned rw{},rh{},sw{},sh{},ww{},wh{};
    mhw::reflex::temporal::render_extent(rw,rh);mhw::reflex::temporal::scene_extent(sw,sh,ww,wh);
    mhw::alpha::status::dlss_sizes(rw,rh,sw,sh);
    mhw::alpha::status::dlss_mode(mhw::alpha::dlss_mode_name(cd::reconstruction_mode()));
    mhw::alpha::status::dlss(mhw::alpha::status::Stage::running);
}
// Worker: a live preset change (continuous_dlaa::change_mode), the player's
// Off (turn_off) or the way back from it. Frame generation pauses for it:
// Off with its inputs settled before, its render-size images released after
// a rebuild (and its scene rectangle taken again after an aspect switch made
// while off), then armed again if DLSS runs.
void change_dlss_mode(const mhw::continuous_dlaa::ModeRequest &wanted,DlssRetry &retry){
    namespace fg=mhw::frame_generation::game;namespace cd=mhw::continuous_dlaa;
    const auto began=GetTickCount64();const auto from=cd::configured_mode_value();const bool from_off=cd::dlss_off();
    const bool scene=from_off&&!wanted.off&&mhw::render_resolution::scene_changed();
    const bool generation=fg::status().armed;
    bool settled=true;
    if(generation){
        fg::arm(false);
        while(!(settled=fg::inputs_settled())&&GetTickCount64()-began<3000)Sleep(1);
    }
    const auto fg_ms=GetTickCount64()-began;
    cd::ModeChange r;
    if(settled)r=wanted.off?cd::turn_off():cd::change_mode(wanted.mode);
    else{r.reason="frame generation inputs did not settle";r.resumed=true;}
    bool released=true,configured=true;
    if(fg_ready&&r.ok&&!r.unchanged&&!wanted.off){
        released=fg::release_inputs(scene);
        if(released&&scene)configured=configure_generation_scene();
    }
    const bool running=cd::live_modes()&&!cd::dlss_off();
    if(fg_ready&&fg_loaded&&running&&released&&configured)fg::arm(true);
    log<<"{\"event\":\"dlss_mode_change\",\"from\":"<<quoted(from_off?"off":mhw::alpha::dlss_mode_value(from))
       <<",\"requested\":"<<quoted(wanted.off?"off":mhw::alpha::dlss_mode_value(wanted.mode))
       <<",\"mode\":"<<quoted(r.off?"off":mhw::alpha::dlss_mode_value(r.mode))<<",\"scene\":"<<r.scene
       <<",\"ok\":"<<r.ok<<",\"unchanged\":"<<r.unchanged<<",\"resumed\":"<<r.resumed<<",\"phase\":"<<r.phase
       <<",\"reason\":"<<quoted(r.reason)<<",\"render\":["<<r.render_width<<','<<r.render_height<<"],\"barrier\":"<<r.barrier<<",\"phase_us\":[";
    for(unsigned i=1;i<r.us.size();++i)log<<(i>1?",":"")<<r.us[i];
    log<<"],\"frame_generation\":{\"paused\":"<<generation<<",\"settle_ms\":"<<fg_ms<<",\"settled\":"<<settled<<",\"released\":"<<released
       <<",\"configured\":"<<configured<<",\"armed\":"<<fg::status().armed
       <<"},\"total_ms\":"<<GetTickCount64()-began<<",\"retry\":"<<retry.count<<"}\n"<<std::flush;
    if(r.ok){dlss_status();retry={};}
    else if((r.resumed||r.retry)&&!r.memory&&retry.count<15){retry.at=GetTickCount64()+2000;retry.request=wanted;++retry.count;}
    else retry={};
    if(fg_ready&&!released){fg_ready=false;mhw::alpha::status::generation(mhw::alpha::status::Generation::stopped,"mode-change-inputs");}
    else if(fg_ready&&!configured){fg_ready=false;mhw::alpha::status::generation(mhw::alpha::status::Generation::unavailable,"scene_rect");}
}
// The live aspect change: tried every 500 ms while not ready (the game
// mid-resize, a drain), up to 40 times; the game renders at its own scale
// meanwhile. A failure after the rebuild began stops DLSS (fault 48).
// Frame generation (fg_ready) stays paused across the tries (its scene
// rectangle is the old one). While the player has DLSS off the switch waits
// for the way back (change_dlss_mode).
struct SceneRetry {std::uint64_t at{};unsigned count{};std::string reason;};
// Worker: the game changed its scene size with its aspect option
// (render_resolution yielded; continuous_dlaa::change_scene). Frame
// generation pauses as for a preset change and takes the new scene rectangle.
void change_dlss_scene(SceneRetry &retry){
    namespace fg=mhw::frame_generation::game;namespace cd=mhw::continuous_dlaa;
    const auto began=GetTickCount64();
    const bool generation=fg_ready;
    bool settled=true;
    if(fg::status().armed){
        fg::arm(false);
        while(!(settled=fg::inputs_settled())&&GetTickCount64()-began<3000)Sleep(1);
    }
    cd::ModeChange r;
    if(settled)r=cd::change_scene();
    else{r.reason="frame generation inputs did not settle";r.retry=true;}
    bool released=true,configured=false;
    if(generation&&r.ok){
        released=fg::release_inputs(true);
        configured=released&&configure_generation_scene();
        if(configured&&fg_loaded)fg::arm(true);
        else{fg_ready=false;mhw::alpha::status::generation(mhw::alpha::status::Generation::unavailable,"scene_rect");}
    }
    const bool last=!r.ok&&r.retry&&retry.count+1>=40;
    if(r.ok||!r.retry||last||!retry.count||r.reason!=retry.reason){
        log<<"{\"event\":\"dlss_scene_change\",\"ok\":"<<r.ok<<",\"retry\":"<<r.retry<<",\"attempt\":"<<retry.count+1
           <<",\"phase\":"<<r.phase<<",\"reason\":"<<quoted(r.reason)<<",\"scene\":["<<r.scene_width<<','<<r.scene_height
           <<"],\"render\":["<<r.render_width<<','<<r.render_height<<"],\"mode\":"<<quoted(mhw::alpha::dlss_mode_value(r.mode))
           <<",\"barrier\":"<<r.barrier<<",\"phase_us\":[";
        for(unsigned i=1;i<r.us.size();++i)log<<(i>1?",":"")<<r.us[i];
        log<<"],\"frame_generation\":{\"paused\":"<<generation<<",\"settled\":"<<settled<<",\"released\":"<<released
           <<",\"configured\":"<<configured<<"},\"total_ms\":"<<GetTickCount64()-began<<"}\n"<<std::flush;
    }
    if(r.ok){dlss_status();retry={};}
    else if(r.retry&&!last){retry.at=GetTickCount64()+500;++retry.count;retry.reason=r.reason;}
    else{
        // Not ready after 40 tries, or failed: DLSS stays off; the game renders
        // at its own scale (a failure also stopped the runtime). Frame
        // generation stays off with it.
        retry.at=UINT64_MAX;
        if(r.retry)cd::stop();
        mhw::alpha::status::dlss(mhw::alpha::status::Stage::off,"scene-change");
        if(fg_ready){fg_ready=false;mhw::alpha::status::generation(mhw::alpha::status::Generation::stopped,"scene-change");}
    }
}
#endif
#endif
bool close_sdk() noexcept {
#ifdef MHW_REFLEX_TEMPORAL
    const bool temporal_clean=finish_temporal();
#endif
#ifdef MHW_REFLEX_FRAME_PROBE
    if(live_frames) {
        const bool restored=live_frames->stop();
        if(!frames_written) { try { live_frames->write(log);frames_written=true; } catch(...) { return false; } }
        if(!restored)return false;
    }
#endif
    if (presentation) {
        const bool restored = presentation->stop();
#ifdef MHW_REFLEX_OWNERSHIP_PROBE
        try { if(!ownership_written){presentation->write_ownership(log);ownership_written=true;} }catch(...){return false;}
#endif
        presentation_heads = presentation->hooks();
        try {
            for (unsigned i = 0; i < presentation_heads.size(); ++i) {
                const auto& head = presentation_heads[i]; if (!head.created) continue;
                log << "{\"event\":\"presentation_hook_restoration\",\"kind\":" << i << ",\"target\":" << head.target
                    << ",\"restored\":" << head.restored << ",\"original_head_sha256\":"
                    << quoted(pb::bytes_sha256(head.original.data(), 32)) << "}\n";
            }
            log << "{\"event\":\"presentation_cleanup\",\"restored\":" << restored << ",\"heads_rva\":"
                << reinterpret_cast<std::uintptr_t>(&presentation_heads) - reinterpret_cast<std::uintptr_t>(module)
                << ",\"head_record_bytes\":" << sizeof(mhw::reflex::PresentationHook) << "}\n" << std::flush;
        } catch (...) { return false; }
        if (!restored) return false;
    }
#ifdef MHW_REFLEX_FRAME_PROBE
    if(mode_change_attempted && !mode_restored) {
        // Every callback is stopped and drained before changing mode. Apply Off
        // even when the On call or its readback failed partway through setup.
        const auto result=sdk.options(sl::ReflexMode::eOff);
        try {
            mode_options("restore_off",sl::ReflexMode::eOff,result);
            // Frame and presentation callbacks have drained. Keep the device
            // pinned while the accepted Off request becomes observable.
            const auto wait=mhw::reflex::wait_driver_off(device_pin.Get());
            log << "{\"event\":\"driver_sleep_status\",\"phase\":\"restored_off\",\"device\":"
                << reinterpret_cast<std::uintptr_t>(device_pin.Get()) << ",\"result\":" << wait.status.result
                << ",\"low_latency\":" << wait.status.low_latency << ",\"query_id\":" << 0xaef96ca1U
                << ",\"structure_version\":" << (136U | (1U<<16)) << ",\"queries\":" << wait.queries
                << ",\"elapsed_ms\":" << wait.elapsed_ms << ",\"timeout_ms\":" << mhw::reflex::driver_off_timeout_ms
                << ",\"initial_low_latency\":" << wait.initial_low_latency << "}\n" << std::flush;
            const bool off=wait.status.result==0 && !wait.status.low_latency;
            mode_restored=result==sl::Result::eOk && off;
        } catch(...) { return false; }
        if(!mode_restored)return false;
    }
#endif
#ifdef MHW_REFLEX_TEMPORAL
    // Remove Reflex callbacks even on temporal failure, but retain SDK/device
    // ownership if temporal GPU work or hook removal is not proven complete.
    if(!temporal_clean)return false;
#endif
    const auto result = sdk.shutdown();
    if (result != sl::Result::eOk) return false;
    if (presentation && !presentation->release_after_shutdown()) return false;
    initialized = false; device_pin.Reset(); return true;
}
void failed(const std::exception& error) noexcept {
    try {
        failure = error.what(); accepting.store(false);
        // Worker drains creation callbacks before stopping routes or SDK.
        log << "{\"event\":\"failed\",\"reason\":" << quoted(failure) << "}\n" << std::flush;
    } catch (...) { accepting.store(false); }
    completed.store(true);
}
HRESULT WINAPI factory_hook(UINT flags, REFIID iid, void** result) {
    Call call; const auto error = GetLastError();
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    if (accepting.load() && (caller == image + 0x23a15fd || caller == image + 0x23a49c6)) {
        std::lock_guard lock(sdk_mutex);
        if (accepting.load()) {
            try {
                const auto rva = static_cast<unsigned>(caller - image);
                require(gate(rva), "native-factory-code-gate");
                ++qualified_factories;
                // Observation only. Missing an earlier factory callback must
                // not prevent attaching the qualified main native device.
            } catch (const std::exception& e) { failed(e); }
        }
    }
    SetLastError(error);
    return factory_original(flags, iid, result);
}
// The game's main device exists: Streamline, Reflex and presentation. `out`
// is the game's output pointer from the D3D12CreateDevice detour (it locates
// the renderer that receives the device); null under AL, whose
// DEVICE_CREATED phase comes from the game's own import call.
void attach_device(IUnknown* adapter, D3D_FEATURE_LEVEL level, IUnknown* created_device, void** out, unsigned caller_rva) {
    require(!initialized && !device_checked && !qualified_devices, "duplicate-startup-device");
    ++qualified_devices;
    require(created_device != nullptr, "native-device-creation");
    require(SUCCEEDED(created_device->QueryInterface(IID_PPV_ARGS(&device_pin))), "native-device-interface");
    require(device_pin->GetNodeCount() == 1 && SUCCEEDED(device_pin->GetDeviceRemovedReason()), "native-device-health");
    owner = GetCurrentThreadId();
#ifdef MHW_ALPHA
    // v0.27.5: the card first (capabilities.hpp), so a failure below still shows it.
    namespace caps_ns = mhw::alpha::capabilities;
    caps_ns::Capabilities caps; caps.known = true;
    if (Microsoft::WRL::ComPtr<IDXGIAdapter1> described; adapter && SUCCEEDED(adapter->QueryInterface(IID_PPV_ARGS(&described)))) {
        DXGI_ADAPTER_DESC1 d{};
        if (SUCCEEDED(described->GetDesc1(&d))) {
            char name[256]{};
            WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, name, static_cast<int>(sizeof(name)), nullptr, nullptr);
            caps.card.name = name; caps.card.vendor = d.VendorId; caps.card.device = d.DeviceId;
            caps.card.memory_mb = static_cast<std::uint64_t>(d.DedicatedVideoMemory) >> 20;
            // DLSS is NVIDIA's (the menu locks it with that reason).
            if (caps.card.vendor != caps_ns::nvidia) caps.dlss = caps_ns::Support::no;
        }
    }
    mhw::alpha::status::card_capabilities(caps);
#endif
#ifdef MHW_FG_GAME
    // FG target only: load DLSS-G with Reflex/PCL; the default stays Reflex-only.
    sdk.initialize(runtime, output / L"sdk-logs", fg_plugin ? mhw::reflex::StreamlineFeatures::reflex_and_frame_generation
                                                             : mhw::reflex::StreamlineFeatures::reflex_only);
#else
    sdk.initialize(runtime, output / L"sdk-logs");
#endif
    initialized = true;
    log << "{\"event\":\"sdk_initialized\",\"before_factory\":false,\"after_native_device_creation\":true,"
           "\"before_native_queues_and_swapchain\":true,\"caller_rva\":" << caller_rva
        << ",\"thread\":" << owner << ",\"device\":" << reinterpret_cast<std::uintptr_t>(device_pin.Get())
        << ",\"sleep\":false,\"markers\":false}\n" << std::flush;
    auto luid = device_pin->GetAdapterLuid();
    sl::AdapterInfo info{}; info.deviceLUID = reinterpret_cast<std::uint8_t*>(&luid);
    info.deviceLUIDSizeInBytes = sizeof(luid);
#ifdef MHW_ALPHA
    {   // The driver as Streamline detects it.
        sl::FeatureRequirements reflex_requirements{};
        if (sdk.requirements(sl::kFeatureReflex, reflex_requirements) == sl::Result::eOk) {
            caps.card.driver_major = reflex_requirements.driverVersionDetected.major;
            caps.card.driver_minor = reflex_requirements.driverVersionDetected.minor;
        }
        caps.reflex = sdk.supported(sl::kFeatureReflex, info) == sl::Result::eOk ? caps_ns::Support::yes : caps_ns::Support::no;
        if (!fg_plugin) { caps.frame_generation = caps_ns::Support::no; caps.missing = caps_ns::Missing::plugin_off; }
        mhw::alpha::status::card_capabilities(caps);
    }
#endif
    api(sdk.supported(sl::kFeatureReflex, info), "Reflex-support");
    api(sdk.supported(sl::kFeaturePCL, info), "PCL-support");
#ifdef MHW_FG_GAME
    if (fg_plugin) {
        const auto fg_support = sdk.supported(sl::kFeatureDLSS_G, info);
        sl::FeatureRequirements fg_requirements{};
        const auto fg_requirements_result = sdk.requirements(sl::kFeatureDLSS_G, fg_requirements);
        log << "{\"event\":\"fg_support\",\"supported\":" << static_cast<int>(fg_support)
            << ",\"requirements\":" << static_cast<int>(fg_requirements_result)
            << ",\"flags\":" << static_cast<unsigned>(fg_requirements.flags)
            << ",\"max_viewports\":" << fg_requirements.maxNumViewports
            << ",\"required_tags\":" << fg_requirements.numRequiredTags << "}\n" << std::flush;
#ifdef MHW_ALPHA
        {   // Why not, in the SDK's own terms (sl_result.h).
            const auto result = fg_support != sl::Result::eOk ? fg_support : fg_requirements_result;
            caps.frame_generation_result = static_cast<unsigned>(result);
            caps.frame_generation = result == sl::Result::eOk ? caps_ns::Support::yes : caps_ns::Support::no;
            caps.missing = result == sl::Result::eOk ? caps_ns::Missing::none
                         : result == sl::Result::eErrorDriverOutOfDate ? caps_ns::Missing::driver
                         : result == sl::Result::eErrorOSOutOfDate ? caps_ns::Missing::os
                         : result == sl::Result::eErrorOSDisabledHWS ? caps_ns::Missing::scheduling
                         : result == sl::Result::eErrorAdapterNotSupported || result == sl::Result::eErrorNoSupportedAdapterFound ||
                                   result == sl::Result::eErrorFeatureNotSupported ? caps_ns::Missing::card
                                                                                  : caps_ns::Missing::other;
            if (fg_requirements_result == sl::Result::eOk) {
                caps.driver_required_major = fg_requirements.driverVersionRequired.major;
                caps.driver_required_minor = fg_requirements.driverVersionRequired.minor;
                if (fg_requirements.driverVersionDetected.major) {
                    caps.card.driver_major = fg_requirements.driverVersionDetected.major;
                    caps.card.driver_minor = fg_requirements.driverVersionDetected.minor;
                }
            }
            mhw::alpha::status::card_capabilities(caps);
        }
#endif
        // v0.27.0: DLSS-G loads at every start, so frame generation turns on
        // and off live. A card or system without it runs DLSS and Reflex alone.
        if (fg_support != sl::Result::eOk || fg_requirements_result != sl::Result::eOk) {
            const auto unloaded = sdk.unload_frame_generation();
            log << "{\"event\":\"fg_unloaded\",\"supported\":" << static_cast<int>(fg_support)
                << ",\"requirements\":" << static_cast<int>(fg_requirements_result) << ",\"result\":" << static_cast<int>(unloaded) << "}\n" << std::flush;
            fg_plugin = false;
#ifdef MHW_ALPHA
            mhw::alpha::status::generation(mhw::alpha::status::Generation::unavailable, "DLSS-G-support");
#endif
        }
    }
#endif
    api(sdk.attach(device_pin.Get()), "attach-native-device");
#if defined(MHW_FG_GAME) && defined(MHW_ALPHA)
    if (fg_plugin && sdk.frame_generation()) {
        // DLSS-G's own limits (guide 14.1, 14.2), while it is loaded; if it
        // cannot answer before a swap chain exists the session's first state
        // read completes them.
        sl::DLSSGState limits{};
        const auto read = sdk.frame_generation_state(sl::ViewportHandle{0}, limits);
        if (read == sl::Result::eOk && limits.numFramesToGenerateMax) {
            caps.max_multiplier = limits.numFramesToGenerateMax + 1;
            caps.dynamic = limits.bIsDynamicMFGSupported == sl::Boolean::eTrue ? caps_ns::Support::yes : caps_ns::Support::no;
        }
        log << "{\"event\":\"fg_limits\",\"result\":" << static_cast<int>(read) << ",\"max_generated\":" << limits.numFramesToGenerateMax
            << ",\"dynamic\":" << (limits.bIsDynamicMFGSupported == sl::Boolean::eTrue) << "}\n" << std::flush;
        // v0.27.4: frame generation off (or DLSS off) at start: the game's
        // chain is made without DLSS-G (full speed); a switch loads it later.
        if (!alpha_config.frame_generation || !alpha_config.dlss) {
            const auto unloaded = sdk.unload_frame_generation();
            log << "{\"event\":\"fg_unloaded\",\"reason\":\"off_at_start\",\"result\":" << static_cast<int>(unloaded) << "}\n" << std::flush;
        }
    }
#endif
#ifdef MHW_FG_GAME
    // Loaded (also a plugin that could not be unloaded): Streamline owns the
    // swap chain. Usable: requested and supported (loaded now or later).
    fg_loaded = sdk.frame_generation();
    fg_plugin = fg_plugin && sdk.frame_generation_requested();
    fg_loaded_state.store(fg_loaded); fg_support_state.store(fg_plugin ? 2U : 1U);
    log << "{\"event\":\"fg_loaded\",\"loaded\":" << fg_loaded << ",\"usable\":" << fg_plugin << "}\n" << std::flush;
    if (fg_loaded) api(sdk.frame_generation_off(sl::ViewportHandle{0}, true), "DLSS-G-initially-Off");
#endif
    api(sdk.options(sl::ReflexMode::eOff), "Reflex-Off");
    sl::ReflexState state{}; api(sdk.state(state), "Reflex-state");
#ifdef MHW_ALPHA
    if (!state.lowLatencyAvailable) caps.reflex = caps_ns::Support::no;
    mhw::alpha::status::card_capabilities(caps);
    log << "{\"event\":\"card_capabilities\",\"name\":" << quoted(caps.card.name) << ",\"vendor\":" << caps.card.vendor
        << ",\"device\":" << caps.card.device << ",\"memory_mb\":" << caps.card.memory_mb
        << ",\"driver\":" << quoted(caps_ns::version_text(caps.card.driver_major, caps.card.driver_minor))
        << ",\"reflex\":" << static_cast<unsigned>(caps.reflex) << ",\"frame_generation\":" << static_cast<unsigned>(caps.frame_generation)
        << ",\"missing\":" << static_cast<unsigned>(caps.missing) << ",\"result\":" << caps.frame_generation_result
        << ",\"driver_required\":" << quoted(caps_ns::version_text(caps.driver_required_major, caps.driver_required_minor))
        << ",\"max_multiplier\":" << caps.max_multiplier << ",\"dynamic\":" << static_cast<unsigned>(caps.dynamic) << "}\n" << std::flush;
    // v0.28.0: the menu's choices follow the card.
    mhw::alpha::menu::card_ready();
#endif
    device_checked = true;
    log << "{\"event\":\"device_checked\",\"caller_rva\":" << caller_rva << ",\"loader\":" << (out == nullptr)
        << ",\"thread\":" << owner << ",\"device\":" << reinterpret_cast<std::uintptr_t>(device_pin.Get())
        << ",\"luid_low\":" << luid.LowPart << ",\"luid_high\":" << luid.HighPart
        << ",\"feature_level\":" << static_cast<unsigned>(level)
        << ",\"low_latency_available\":" << state.lowLatencyAvailable
        << ",\"mode\":0,\"rendering_modified\":false,\"sleep_calls\":0,\"markers\":0}\n" << std::flush;
    require(state.lowLatencyAvailable, "Reflex-unavailable");
    auto factory_pin = mhw::reflex::adapter_factory(adapter);
    std::uintptr_t backend{}, published{};
    if (out) {
    // This hash-qualified call writes its device to [rdi+0x18].
    // The constructor's renderer global is not published until
    // RVA 0x23a72d3, after queues and the swap chain already exist.
    const auto device_output = reinterpret_cast<std::uintptr_t>(out);
    require(device_output >= 0x18, "startup-native-device-output");
    backend = device_output - 0x18;
    std::uintptr_t factory{}, selected_adapter{}, stored_device{};
    require(pb::read(backend, factory) && factory && pb::read(backend + 0x10, selected_adapter) &&
        selected_adapter == reinterpret_cast<std::uintptr_t>(adapter) &&
        pb::read(backend + 0x18, stored_device) && stored_device == reinterpret_cast<std::uintptr_t>(*out),
        "startup-native-output-owner");
    Microsoft::WRL::ComPtr<IDXGIFactory2> owner_factory;
    require(SUCCEEDED(reinterpret_cast<IUnknown*>(factory)->QueryInterface(IID_PPV_ARGS(&owner_factory))) &&
        owner_factory.Get() == factory_pin.Get(), "startup-native-factory-identity");
    require(pb::read(image + 0x51c7050, published), "startup-native-publication-slot");
    }
    log << "{\"event\":\"factory_context\",\"source\":\"" << (out ? "native-adapter-parent" : "loader-adapter-parent")
        << "\",\"output_owner_matches\":" << (out != nullptr) << ","
           "\"factory\":" << reinterpret_cast<std::uintptr_t>(factory_pin.Get()) << ",\"backend\":" << backend
        << ",\"global_backend_published\":" << (out && published == backend) << "}\n" << std::flush;
    presentation = new mhw::reflex::Presentation(sdk);
#ifdef MHW_REFLEX_OWNERSHIP_PROBE
    presentation->ownership(image,0x55d3000);
#endif
#ifdef MHW_FG_GAME
    // Streamline owns pacing, generated presentation and the host-visible buffers.
    presentation->install(device_pin.Get(), factory_pin.Get(), fg_loaded ? mhw::reflex::PresentationContract::sdk_paced
                                                                          : mhw::reflex::PresentationContract::synchronous_native);
    // DLSS-G Off before any host window manipulation (guide 12.0).
    presentation->lifecycle({presentation, [](void*, unsigned method) noexcept {
        return mhw::frame_generation::game::lifecycle_guard(method); },
        // The HUD-less image follows the back buffer: SDR or HDR10 (v0.24.8).
        [](void*, std::uint32_t format) noexcept { mhw::frame_generation::game::back_buffer(format); }});
#else
    presentation->install(device_pin.Get(), factory_pin.Get());
#endif
    success = true; accepting.store(false); completed.store(true);
    log << "{\"event\":\"presentation_installed\",\"factory\":" << reinterpret_cast<std::uintptr_t>(factory_pin.Get())
        << ",\"device_native\":true,\"returned_interfaces_native\":true,\"creation_routes\":4,\"mode\":0}\n" << std::flush;
}
HRESULT WINAPI device_hook(IUnknown* adapter, D3D_FEATURE_LEVEL level, REFIID iid, void** out) {
    Call call;
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const auto result = device_original(adapter, level, iid, out);
    const auto error = GetLastError();
    if (accepting.load() && caller == image + 0x23a4e2e) {
        std::lock_guard lock(sdk_mutex);
        if (accepting.load()) {
            try {
                require(gate(0x23a4e2e), "native-device-code-gate");
                require(SUCCEEDED(result) && out && *out, "native-device-creation");
                attach_device(adapter, level, static_cast<IUnknown*>(*out), out, 0x23a4e2e);
            } catch (const std::exception& e) { failed(e); }
        }
    }
    SetLastError(error); return result;
}
std::filesystem::path path_setting(const std::filesystem::path& file) {
    require(std::filesystem::file_size(file) > 0 && std::filesystem::file_size(file) <= 4096, "configuration-size");
    std::ifstream in(file, std::ios::binary); std::string value; std::getline(in, value);
    if (!value.empty() && value.back() == '\r') value.pop_back();
    const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    require(count > 0, "configuration-utf8"); std::wstring wide(static_cast<std::size_t>(count), L'\0');
    require(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), wide.data(), count) == count, "configuration-utf8");
    const std::filesystem::path path(wide); require(path.is_absolute() && std::filesystem::is_directory(path), "configuration-directory");
    return std::filesystem::canonical(path);
}
bool clean_hooks() noexcept {
    accepting.store(false); bool ok = true;
    for (unsigned i = 0; i < targets.size(); ++i)
        if (enabled[i]) {
            const auto status = MH_DisableHook(targets[i]);
            if (status == MH_OK || status == MH_ERROR_DISABLED) enabled[i] = false; else ok = false;
        }
    const auto began = GetTickCount64();
    while (active.load() && GetTickCount64() - began < 30000) Sleep(10);
    if (active.load()) return false;
    for (unsigned i = 0; i < targets.size(); ++i) if (created[i]) {
        std::array<std::uint64_t, 4> current{}; bool read = true;
        for (unsigned n = 0; n < current.size(); ++n)
            read = pb::read(reinterpret_cast<std::uintptr_t>(targets[i]) + n*8ULL, current[n]) && read;
        const bool restored = read && current == original_heads[i];
        ok = ok && restored;
        try { log << "{\"event\":\"hook_restoration\",\"kind\":" << i << ",\"target\":" << reinterpret_cast<std::uintptr_t>(targets[i])
            << ",\"restored\":" << restored << ",\"original_head_sha256\":" << quoted(pb::bytes_sha256(original_heads[i].data(), 32))
            << "}\n" << std::flush; } catch (...) { ok = false; }
    }
    // Keep the DLL/trampolines pinned for originals already fetched by callers.
    // Do not remove hooks, uninitialize MinHook or unload the interposer here.
    return ok;
}
bool query_swap(std::uintptr_t address, IDXGISwapChain** owned, DXGI_SWAP_CHAIN_DESC* desc) noexcept {
    __try {
        return SUCCEEDED(reinterpret_cast<IUnknown*>(address)->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(owned))) &&
               SUCCEEDED((*owned)->GetDesc(desc));
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool describe_swap() {
    const auto began = GetTickCount64();
    while (GetTickCount64() - began < 60000) {
        std::uintptr_t backend{}, swap{}, table{}, qi{}, release{}, desc_method{}, again{};
        if (pb::read(image + 0x51c7050, backend) && backend && pb::read(backend + 0x1470, swap) && swap &&
            pb::read(swap, table) && table && pb::read(table, qi) && pb::read(table + 16, release) && pb::read(table + 96, desc_method)) {
            bool native = true;
            for (const auto method : {qi, release, desc_method}) {
                MEMORY_BASIC_INFORMATION memory{};
                native = VirtualQuery(reinterpret_cast<void*>(method), &memory, sizeof(memory)) == sizeof(memory) &&
                    memory.Type == MEM_IMAGE && memory.AllocationBase == GetModuleHandleW(L"dxgi.dll") && native;
            }
            Microsoft::WRL::ComPtr<IDXGISwapChain> pin; DXGI_SWAP_CHAIN_DESC desc{};
            if (native && query_swap(swap, &pin, &desc) && pb::read(backend + 0x1470, again) && again == swap) {
                log << "{\"event\":\"swapchain_snapshot\",\"native_interface\":true,\"swapchain\":" << swap
                    << ",\"width\":" << desc.BufferDesc.Width << ",\"height\":" << desc.BufferDesc.Height
                    << ",\"format\":" << static_cast<unsigned>(desc.BufferDesc.Format) << ",\"buffers\":" << desc.BufferCount
                    << ",\"windowed\":" << (desc.Windowed != FALSE) << "}\n" << std::flush;
                return desc.BufferDesc.Width && desc.BufferDesc.Height && desc.BufferDesc.Format != DXGI_FORMAT_UNKNOWN;
            }
        }
        Sleep(50);
    }
    return false;
}
void load_creation_api(unsigned kind, const wchar_t* name, const char* symbol) {
    const bool was_loaded = GetModuleHandleW(name) != nullptr;
    wchar_t system[32768]{}, actual[32768]{};
    const auto length = GetSystemDirectoryW(system, static_cast<UINT>(std::size(system)));
    require(length && length < std::size(system), "system-directory");
    const auto path = std::filesystem::path(system) / name;
    creation_modules[kind] = LoadLibraryExW(path.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    require(creation_modules[kind] != nullptr, "load-native-creation-module");
    const auto count = GetModuleFileNameW(creation_modules[kind], actual, static_cast<DWORD>(std::size(actual)));
    require(count && count < std::size(actual) && std::filesystem::equivalent(path, actual), "native-module-path");
    targets[kind] = reinterpret_cast<void*>(GetProcAddress(creation_modules[kind], symbol));
    require(targets[kind] && pb::executable(reinterpret_cast<std::uintptr_t>(targets[kind]),
        reinterpret_cast<std::uintptr_t>(creation_modules[kind])), "native-creation-export");
    log << "{\"event\":\"api_module\",\"kind\":" << kind << ",\"was_loaded\":" << was_loaded
        << ",\"loaded_from_system32\":true,\"module\":" << reinterpret_cast<std::uintptr_t>(creation_modules[kind])
        << ",\"target\":" << reinterpret_cast<std::uintptr_t>(targets[kind]) << "}\n" << std::flush;
}
#ifdef MHW_FG_GAME
std::atomic<bool> boot_monitor_stop{};
// Diagnostic boot timeline: text table 0 (message table array at host
// +0x5012DC0), the UI object's state at [[engine+0xAF800]+0x51ACC], and the
// swap-chain view, sampled every 20 ms and written when anything changes.
void boot_monitor(std::filesystem::path path) noexcept {
    try {
        std::ofstream out(path, std::ios::binary); if (!out) return;
        const auto began = GetTickCount64();
        std::uint64_t last_key = ~0ULL; std::uintptr_t last_table{}, last_ui{};
        while (!boot_monitor_stop.load() && GetTickCount64() - began < 180000) {
            std::uintptr_t table{}, engine{}, ui{}; unsigned state = ~0U, state_b = ~0U;
            pb::read(image + 0x5012DC0, table);
            if (pb::read(image + 0x51c4148, engine) && engine && pb::read(engine + 0xAF800, ui) && ui) {
                pb::read(ui + 0x51ACC, state); pb::read(ui + 0x51AB0, state_b);
            }
            mhw::frame_generation::PresentationBuffers b{}; mhw::reflex::scene_presentation_buffers(b);
            const std::uint64_t key = (std::uint64_t(state) << 32) ^ (std::uint64_t(state_b) << 16) ^ (table != 0) ^
                (b.generation << 8) ^ (b.present_other << 20) ^ (std::uint64_t(ui != 0) << 1);
            if (key != last_key || (table != 0) != (last_table != 0) || ui != last_ui) {
                out << "{\"t_ms\":" << GetTickCount64() - began << ",\"table0\":" << (table != 0) << ",\"ui\":" << (ui != 0)
                    << ",\"state\":" << static_cast<int>(state) << ",\"state_b\":" << static_cast<int>(state_b)
                    << ",\"generation\":" << b.generation << ",\"present_calls\":" << b.present_calls
                    << ",\"present_other\":" << b.present_other << "}\n" << std::flush;
                last_key = key; last_table = table; last_ui = ui;
            }
            Sleep(20);
        }
    } catch (...) {}
}
// Swap-chain view the host received through the routed calls (diagnostic).
int fg_buffers_json(char* out, int size) noexcept {
    mhw::frame_generation::PresentationBuffers b{};
    if (!mhw::reflex::scene_presentation_buffers(b)) return 0;
    return std::snprintf(out, static_cast<size_t>(size),
        "{\"generation\":%llu,\"known_mask\":%u,\"game_index\":%u,\"game_index_max\":%u,\"game_index_calls\":%llu,"
        "\"out_of_range\":%llu,\"conflicts\":%llu,\"foreign\":%llu,\"present_calls\":%llu,\"present_ok\":%llu,"
        "\"present_other\":%llu,\"first_results\":[%u,%u,%u,%u],\"first_result_at\":[%llu,%llu,%llu,%llu]}",
        static_cast<unsigned long long>(b.generation), b.known_mask, b.game_index, b.game_index_max,
        static_cast<unsigned long long>(b.game_index_calls), static_cast<unsigned long long>(b.out_of_range),
        static_cast<unsigned long long>(b.conflicts), static_cast<unsigned long long>(b.foreign),
        static_cast<unsigned long long>(b.present_calls), static_cast<unsigned long long>(b.present_ok),
        static_cast<unsigned long long>(b.present_other), b.present_results[0], b.present_results[1], b.present_results[2],
        b.present_results[3], static_cast<unsigned long long>(b.present_result_at[0]),
        static_cast<unsigned long long>(b.present_result_at[1]), static_cast<unsigned long long>(b.present_result_at[2]),
        static_cast<unsigned long long>(b.present_result_at[3]));
}
void log_fg_buffers(std::ostream& out, const char* phase) {
    char text[512]{}; const int n = fg_buffers_json(text, sizeof text);
    out << "{\"event\":\"fg_buffers\",\"phase\":\"" << phase << "\",\"buffers\":" << (n > 0 ? text : "null") << "}\n" << std::flush;
}
// Host swap-chain lifecycle calls recorded by the presentation hooks.
std::uint64_t lifecycle_dropped_logged{};
const char* stop_reason="not_armed";  // Why the FG worker loop ended.
void log_lifecycle(std::ostream& out) {
    if (!presentation) return;
    std::array<mhw::reflex::LifecycleEvent, mhw::reflex::Presentation::lifecycle_capacity> events{};
    std::uint64_t dropped{};
    const auto count = presentation->drain_lifecycle(events, dropped);
    if (!count && dropped == lifecycle_dropped_logged) return;
    LARGE_INTEGER frequency{}; QueryPerformanceFrequency(&frequency);
    constexpr const char* names[]{"", "create_swap_chain", "create_for_hwnd", "create_for_core_window", "", "", "set_fullscreen_state",
                                  "", "resize_buffers", "", "", "resize_buffers1", "resize_target"};
    for (unsigned i = 0; i < count; ++i) {
        const auto& e = events[i];
        out << "{\"event\":\"swap_lifecycle\",\"sequence\":" << e.sequence << ",\"method\":" << e.method << ",\"name\":\""
            << (e.method < std::size(names) ? names[e.method] : "") << "\",\"thread\":" << e.thread << ",\"host_thread\":" << e.host_thread
            << ",\"current_chain\":" << e.current_chain << ",\"routed\":" << e.routed << ",\"guard_off\":" << e.guard_off
            << ",\"result\":" << e.result << ",\"qpc\":" << e.began_qpc << ",\"ms\":"
            << (frequency.QuadPart ? double(e.ended_qpc - e.began_qpc) * 1000.0 / double(frequency.QuadPart) : 0.0) << ",\"args\":[";
        for (unsigned n = 0; n < e.args.size(); ++n) out << (n ? "," : "") << e.args[n];
        out << "],\"after\":";
        if (e.described)
            out << "{\"width\":" << e.width << ",\"height\":" << e.height << ",\"format\":" << e.format << ",\"buffers\":" << e.buffers
                << ",\"flags\":" << e.flags << ",\"windowed\":" << e.windowed << "}";
        else out << "null";
        out << "}\n";
    }
    if (dropped != lifecycle_dropped_logged) {
        out << "{\"event\":\"swap_lifecycle_dropped\",\"dropped\":" << dropped << "}\n";
        lifecycle_dropped_logged = dropped;
    }
    out << std::flush;
}
// Overhead measurements (v0.12): host frame intervals for the last window,
// labelled with the active perf switches. Armed runs add the driver's own
// per-frame report (Reflex markers), which separates CPU and GPU time.
void log_frames(std::ostream& out, const char* phase) {
    const auto w = mhw::perf::host_frames.take();
    out << "{\"event\":\"frame_stats\",\"phase\":\"" << phase << "\",\"perf\":" << mhw::perf::toggles.load() << ",\"frames\":" << w.frames
        << ",\"total_us\":" << w.total_us << ",\"fps\":" << (w.total_us ? double(w.frames) * 1e6 / double(w.total_us) : 0.0)
        << ",\"p50_us\":" << w.p50_us << ",\"p95_us\":" << w.p95_us << ",\"p99_us\":" << w.p99_us << ",\"max_us\":" << w.max_us
        << ",\"over_50ms\":" << w.over_50ms << "}\n" << std::flush;
}
// The first Reflex frame fault: its reason and context (LiveFrameStats::Fault).
void write_reflex_fault(std::ostream& out, const mhw::reflex::LiveFrameStats& frames) {
    const auto& f = frames.fault;
    out << "\"reflex_fault\":\"" << (frames.fault_reason ? frames.fault_reason : "") << "\",\"reflex_fault_detail\":{\"call\":" << f.call
        << ",\"thread\":" << f.thread << ",\"owner\":" << f.owner << ",\"links\":" << f.links << ",\"code\":" << f.code
        << ",\"stage\":" << f.stage << ",\"frame\":" << f.frame << ",\"in_frame\":" << f.in_frame << '}';
}
template<std::size_t N> std::uint64_t median_of(std::array<std::uint64_t, N>& values, unsigned count) {
    if (!count) return 0;
    std::nth_element(values.begin(), values.begin() + count / 2, values.begin() + count);
    return values[count / 2];
}
template<std::size_t N> std::uint64_t max_of(const std::array<std::uint64_t, N>& values, unsigned count) {
    return count ? *std::max_element(values.begin(), values.begin() + count) : 0;
}
void log_latency(std::ostream& out) {
    auto state = std::make_unique<sl::ReflexState>();
    sl::Result result{};
    { std::lock_guard lock(sdk_mutex); result = sdk.state(*state); }
    out << "{\"event\":\"latency\",\"result\":" << static_cast<unsigned>(result) << ",\"available\":" << state->latencyReportAvailable;
    if (result == sl::Result::eOk && state->latencyReportAvailable) {
        std::array<const sl::ReflexReport*, sl::kReflexFrameReportCount> frames{};
        unsigned n{};
        for (const auto& f : state->frameReport)
            if (f.frameID && f.simEndTime >= f.simStartTime && f.renderSubmitEndTime >= f.renderSubmitStartTime && f.presentEndTime >= f.presentStartTime &&
                f.driverEndTime >= f.driverStartTime && f.osRenderQueueEndTime >= f.osRenderQueueStartTime && f.gpuRenderEndTime >= f.gpuRenderStartTime)
                frames[n++] = &f;
        std::sort(frames.begin(), frames.begin() + n, [](auto a, auto b) { return a->frameID < b->frameID; });
        // Microseconds as the driver reports them; interval = consecutive frames' present starts.
        std::array<std::array<std::uint64_t, sl::kReflexFrameReportCount>, 9> v{};
        std::array<unsigned, 9> c{};
        for (unsigned i = 0; i < n; ++i) {
            const auto& f = *frames[i];
            if (i && f.frameID == frames[i - 1]->frameID + 1 && f.presentStartTime >= frames[i - 1]->presentStartTime)
                v[0][c[0]++] = f.presentStartTime - frames[i - 1]->presentStartTime;
            v[1][c[1]++] = f.simEndTime - f.simStartTime;
            v[2][c[2]++] = f.renderSubmitEndTime - f.renderSubmitStartTime;
            v[3][c[3]++] = f.presentEndTime - f.presentStartTime;
            v[4][c[4]++] = f.driverEndTime - f.driverStartTime;
            v[5][c[5]++] = f.osRenderQueueEndTime - f.osRenderQueueStartTime;
            v[6][c[6]++] = f.gpuRenderEndTime - f.gpuRenderStartTime;
            v[7][c[7]++] = f.gpuActiveRenderTimeUs;
            v[8][c[8]++] = f.gpuFrameTimeUs;
        }
        constexpr std::array<const char*, 9> names{"interval", "simulation", "submit", "present", "driver", "queue", "gpu", "gpu_active", "gpu_frame"};
        out << ",\"frames\":" << n;
        for (unsigned k = 0; k < names.size(); ++k) {
            const auto top = max_of(v[k], c[k]);
            out << ",\"" << names[k] << "\":[" << median_of(v[k], c[k]) << ',' << top << ']';
        }
    }
    out << "}\n" << std::flush;
}
// Live perf switches: "<log>.perf-N" holds "<creation> <mask>", sequenced and
// generation-checked like the multiplier markers. The window before a switch
// is logged first, so every frame_stats line covers one mask.
void poll_perf(std::ostream& out, const std::filesystem::path& folder, const std::string& stem, std::uint64_t creation, unsigned& requests) {
    const auto marker = folder / (stem + ".perf-" + std::to_string(requests + 1));
    if (!std::filesystem::exists(marker) || std::filesystem::file_size(marker) > 64) return;
    std::ifstream input(marker); std::uint64_t generation{}; unsigned mask{}; input >> generation >> mask;
    const bool accepted = bool(input) && generation == creation && mask <= mhw::perf::toggle_mask;
    const auto previous = mhw::perf::toggles.load();
    ++requests;
    if (accepted) { log_frames(out, "switch"); mhw::perf::toggles.store(mask); }
    out << "{\"event\":\"perf_toggles\",\"request\":" << requests << ",\"mask\":" << mask << ",\"previous\":" << previous
        << ",\"accepted\":" << accepted << "}\n" << std::flush;
}
#endif
DWORD WINAPI worker(void*) noexcept {
    HMODULE pin{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&worker), &pin)) { startup_failure = "module-pin"; signal_startup(); return 1; }
    {   // One active copy per process: a second one (e.g. loaded by AL and also
        // force-loaded by MO2) stays inert. The handle lives as long as the process.
        const auto name = L"Local\\mhw_dlss.instance." + std::to_wstring(GetCurrentProcessId());
        const HANDLE instance = CreateMutexW(nullptr, FALSE, name.c_str());
        if (!instance || GetLastError() == ERROR_ALREADY_EXISTS) {
            startup_failure = "another mhw_dlss copy is already active in this process"; signal_startup(); return 0;
        }
    }
    try {
        wchar_t dll_name[32768]{}, host_name[32768]{};
        const auto dll_length = GetModuleFileNameW(module, dll_name, static_cast<DWORD>(std::size(dll_name)));
        const auto host_length = GetModuleFileNameW(nullptr, host_name, static_cast<DWORD>(std::size(host_name)));
        require(dll_length && dll_length < std::size(dll_name), "module-path");
        require(host_length && host_length < std::size(host_name), "host-path");
        const std::filesystem::path dll(dll_name);
#ifdef MHW_ALPHA
        {
            // Plugin layout: mhw_dlss.dll, mhw_dlss.ini, the NVIDIA runtime and
            // logs in one folder, injected by AL (MO2 force-load in development).
            // With the ini beside the DLL in a folder of its own (not the game
            // folder itself), that folder is the one place inside the game
            // folder the alpha may read its runtime from and log to.
            const auto base = dll.parent_path().lexically_normal(); std::error_code missing;
            const auto game_folder = std::filesystem::path(host_name).parent_path().lexically_normal();
            if (std::filesystem::is_regular_file(base / L"mhw_dlss.ini", missing) && _wcsicmp(base.c_str(), game_folder.c_str()))
                alpha_home = base.wstring();
            alpha_base = base;
            std::ifstream in(base / L"mhw_dlss.ini", std::ios::binary);
            const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            alpha_config = mhw::alpha::parse_config(text, base);
            output = alpha_config.log_dir; std::filesystem::create_directories(output);
            runtime = alpha_config.runtime_dir;
            // DLSS-G loads unless frame_generation_plugin = off; the frame
            // generation setting only turns it on or off (live, in the menu).
            fg_plugin = alpha_config.generation_plugin; fg_multiplier = alpha_config.multiplier;
            mhw::frame_generation::game::enable(alpha_config.frame_generation);
            fg_mode = alpha_config.dynamic ? mhw::frame_generation::game::Mode::dynamic
                    : alpha_config.generation_auto ? mhw::frame_generation::game::Mode::automatic
                                                   : mhw::frame_generation::game::Mode::fixed;
            fg_target = alpha_config.dynamic_target;
            mhw::alpha::status::generation(fg_plugin ? mhw::alpha::status::Generation::starting : mhw::alpha::status::Generation::off);
            mhw::continuous_dlaa::reconstruction_mode(alpha_config.dlss_mode);
            mhw::continuous_dlaa::start_off(!alpha_config.dlss);
            mhw::alpha::status::dlss_mode(mhw::alpha::dlss_mode_name(alpha_config.dlss_mode));
            // profile = on: the stack sampler too (perf::sample_profile; started
            // with the frame windows, as in the research build).
            mhw::perf::toggles.store(mhw::perf::all_toggles|(alpha_config.profile?mhw::perf::sample_profile:0U));
            reflex_requested.store(alpha_config.reflex==mhw::alpha::ReflexMode::off?0U:alpha_config.reflex==mhw::alpha::ReflexMode::boost?2U:1U);
            mhw::alpha::status::reflex_mode(mhw::alpha::reflex_name(alpha_config.reflex));
            mhw::reflex::sdk_verbose_log = false;
        }
#else
        auto data = dll; data += L".output-dir"; output = path_setting(data);
        auto provider = dll; provider += L".runtime-dir"; runtime = path_setting(provider);
#endif
#if defined(MHW_FG_GAME) && !defined(MHW_ALPHA)
        {   // Diagnostic A/B switch: "<dll>.fg-mode" containing "off" runs this
            // binary without the DLSS-G plugin (Reflex-only, synchronous routing).
            auto mode_file = dll; mode_file += L".fg-mode";
            std::ifstream mode_in(mode_file); std::string mode_text; mode_in >> mode_text;
            fg_plugin = mode_text != "off";
            if (const auto m = mhw::reflex::parse_multiplier(mode_text)) fg_multiplier = m;
            // "<dll>.perf": initial perf-switch mask (v0.12; 0 = original paths).
            auto perf_file = dll; perf_file += L".perf";
            std::ifstream perf_in(perf_file); unsigned mask{};
            if (perf_in >> mask && mask <= mhw::perf::toggle_mask) mhw::perf::toggles.store(mask);
        }
#endif
        const auto game_root = std::filesystem::canonical(std::filesystem::path(host_name).parent_path()).wstring();
        const auto under = [](const std::wstring& text, const std::wstring& root) {
            return text.size() >= root.size() && CompareStringOrdinal(text.data(), static_cast<int>(root.size()),
                root.data(), static_cast<int>(root.size()), TRUE) == CSTR_EQUAL &&
                (text.size() == root.size() || text[root.size()] == L'\\' || text[root.size()] == L'/');
        };
        for (const auto& path : {output, runtime}) {
            const auto text = path.wstring();
#ifdef MHW_ALPHA
            // The plugin folder may sit inside the game folder (AL, MO2 virtual
            // paths); nothing else there is used.
            if (!alpha_home.empty() && under(path.lexically_normal().wstring(), alpha_home)) continue;
#endif
            require(!under(text, game_root), "configuration-in-game-directory");
        }
        image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)); std::string reason;
        require(pb::host_identity(image, reason, !exe_hash_verified), "retail-host-identity");
        FILETIME ct{}, et{}, kt{}, ut{}; require(GetProcessTimes(GetCurrentProcess(), &ct, &et, &kt, &ut) != FALSE, "process-identity");
        const auto creation = (static_cast<std::uint64_t>(ct.dwHighDateTime) << 32) | ct.dwLowDateTime;
#ifdef MHW_FG_GAME
        unsigned perf_requests{};
#endif
#ifdef MHW_REFLEX_FRAME_PROBE
        const auto name = "reflex-frames-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(creation) + ".jsonl";
        arm_file=output/(name+".arm");require(!std::filesystem::exists(arm_file),"preserve-arm-file");
#else
        const auto name = "reflex-presentation-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(creation) + ".jsonl";
#endif
        stop_file = output / (name + ".stop");
        require(!std::filesystem::exists(stop_file), "preserve-stop-file");
        HANDLE file = CreateFileW((output/name).c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        require(file != INVALID_HANDLE_VALUE, "preserve-existing-log"); CloseHandle(file);
        log.open(output/name, std::ios::binary | std::ios::app); require(bool(log), "log"); log << std::boolalpha;
#ifdef MHW_FG_GAME
        const bool crash_log_installed = mhw::frame_generation::crash_log::install(output/(name+".crash.jsonl"), fg_buffers_json);
#ifndef MHW_ALPHA
        if (const auto monitor = CreateThread(nullptr, 0, [](void* p) -> DWORD {
                std::unique_ptr<std::filesystem::path> path(static_cast<std::filesystem::path*>(p)); boot_monitor(*path); return 0; },
                new std::filesystem::path(output/(name+".boot.jsonl")), 0, nullptr)) CloseHandle(monitor);
#endif
#endif
#ifdef MHW_REFLEX_TEMPORAL
#if defined(MHW_FG_GAME)
#ifdef MHW_ALPHA
        log << "{\"event\":\"start\",\"schema\":1,\"version\":\"" MHW_ALPHA_VERSION "\",\"build\":\"alpha\",\"experiment\":\"frame-generation-game\",\"target_mode\":1,\"fg_plugin_loaded\":" << fg_plugin
#else
        log << "{\"event\":\"start\",\"schema\":1,\"version\":\"0.12.14\",\"experiment\":\"frame-generation-game\",\"target_mode\":1,\"fg_plugin_loaded\":" << fg_plugin
#endif
            << ",\"fg_multiplier\":" << fg_multiplier << ",\"fg_enabled\":false,\"pid\":" << GetCurrentProcessId()
#elif defined(MHW_FG_SCENE_PROBE)
        log << "{\"event\":\"start\",\"schema\":7,\"version\":\"0.6.5\",\"experiment\":\"frame-generation-scene-boundary\",\"target_mode\":1,\"fg_plugin_loaded\":false,\"fg_enabled\":false,\"pid\":" << GetCurrentProcessId()
#elif defined(MHW_REFLEX_OWNERSHIP_PROBE)
        log << "{\"event\":\"start\",\"schema\":4,\"version\":\"0.6.0\",\"experiment\":\"frame-generation-game-ownership\",\"target_mode\":1,\"fg_plugin_loaded\":false,\"fg_enabled\":false,\"pid\":" << GetCurrentProcessId()
#else
        log << "{\"event\":\"start\",\"schema\":3,\"version\":\"0.5.1\",\"experiment\":\"reflex-temporal-coexistence\",\"target_mode\":1,\"pid\":" << GetCurrentProcessId()
#endif
#elif defined(MHW_REFLEX_FRAME_PROBE)
        log << "{\"event\":\"start\",\"schema\":2,\"version\":\"0.4.2\",\"experiment\":\"reflex-live-frames\",\"target_mode\":1,\"pid\":" << GetCurrentProcessId()
#else
        log << "{\"event\":\"start\",\"schema\":1,\"version\":\"0.2.1\",\"experiment\":\"reflex-persistent-presentation\",\"pid\":" << GetCurrentProcessId()
#endif
            << ",\"creation\":" << creation << ",\"host_sha256\":" << quoted(pb::host_sha)
            << ",\"host_version\":\"0.0.0.0\",\"dll_sha256\":" << quoted(pb::file_sha256(dll.wstring()))
            << ",\"sdk\":\"2.12.0\",\"graphics_api\":\"D3D12\",\"mode\":0,\"sleep\":false,\"markers\":false"
#ifdef MHW_REFLEX_TEMPORAL
            << ",\"upscaling\":true,\"temporal_version\":" << quoted(mhw::reflex::temporal::version())
#else
            << ",\"upscaling\":false"
#endif
            << "}\n" << std::flush;
#ifdef MHW_ALPHA
        log << "{\"event\":\"alpha_config\",\"dlss_mode\":\"" << mhw::alpha::dlss_setting_value(alpha_config.dlss,alpha_config.dlss_mode)
            << "\",\"frame_generation\":" << alpha_config.frame_generation << ",\"multiplier\":" << alpha_config.multiplier
            << ",\"dynamic\":" << alpha_config.dynamic << ",\"dynamic_target\":" << quoted(mhw::alpha::dynamic_target_value(alpha_config.dynamic_target))
            << ",\"frame_generation_auto\":" << alpha_config.generation_auto << ",\"frame_generation_plugin\":" << quoted(alpha_config.generation_plugin ? "auto" : "off") << ",\"profile\":" << alpha_config.profile
            << ",\"perf_ab\":" << alpha_config.perf_ab << ",\"hook_timing\":" << alpha_config.hook_timing
            << ",\"reflex\":" << quoted(mhw::alpha::reflex_value(alpha_config.reflex)) << ",\"runtime_dir\":" << quoted(alpha_config.runtime_dir.string())
            << ",\"log_dir\":" << quoted(alpha_config.log_dir.string()) << ",\"warnings\":[";
        for (unsigned i = 0; i < alpha_config.warnings.size(); ++i) log << (i ? "," : "") << quoted(alpha_config.warnings[i]);
        log << "]}\n" << std::flush;
#endif
        // Refuse a late load; public API hooks are installed before first use.
        std::uintptr_t backend{}; require(pb::read(image + 0x51c7050, backend), "backend-slot");
        if (backend) { std::uintptr_t device{}; require(pb::read(backend + 0x18, device) && !device, "main-device-already-created"); }
        // The native constructor loads both modules dynamically. They are not
        // static imports of this probe, so GetModuleHandle alone races startup.
        // Resolve exact system modules before installing either creation hook.
        // Under AL the device arrives through DEVICE_CREATED instead.
        if (!mhw::astera::active()) {
            load_creation_api(0, L"dxgi.dll", "CreateDXGIFactory2");
            load_creation_api(1, L"d3d12.dll", "D3D12CreateDevice");
        }
#ifdef MHW_REFLEX_FRAME_PROBE
        const bool pool_ready=mhw::reflex::LiveFrames::reserve(image,reason);
        const auto& pool=pb::pool_state();
        log << "{\"event\":\"frame_pool\",\"ready\":" << pool_ready << ",\"attempted\":" << pool.attempted
            << ",\"policy_available\":" << pool.policy_available << ",\"policy_flags\":" << pool.policy_flags
            << ",\"status\":" << pool.status << ",\"win32\":" << pool.win32 << ",\"origin\":" << pool.origin
            << ",\"slot\":" << pool.slot << ",\"page\":" << pool.page << ",\"reason\":" << quoted(reason) << "}\n" << std::flush;
        require(pool_ready,reason.c_str());
        require(MH_Initialize() == MH_ERROR_ALREADY_INITIALIZED, "hook-initialize-after-reservation");
#else
        require(MH_Initialize() == MH_OK, "hook-initialize");
#endif
        for (unsigned i = 0; i < targets.size() && !mhw::astera::active(); ++i) {
            for (unsigned n = 0; n < original_heads[i].size(); ++n)
                require(pb::read(reinterpret_cast<std::uintptr_t>(targets[i]) + n*8ULL, original_heads[i][n]), "original-api-head");
            void* original{};
            require(MH_CreateHook(targets[i], i == 0 ? reinterpret_cast<void*>(&factory_hook) : reinterpret_cast<void*>(&device_hook), &original) == MH_OK, "creation-hook");
            created[i] = true;
            if (i == 0) factory_original = reinterpret_cast<FactoryFn>(original); else device_original = reinterpret_cast<DeviceFn>(original);
        }
        for (unsigned i = 0; i < targets.size() && !mhw::astera::active(); ++i) require(MH_QueueEnableHook(targets[i]) == MH_OK, "queue-creation-hook");
        log << "{\"event\":\"prepared\",\"loader\":" << mhw::astera::active() << ",\"queued_api_hooks\":" << (mhw::astera::active() ? 0 : 2)
            << ",\"sdk_initialized\":false,\"original_heads_rva\":"
            << reinterpret_cast<std::uintptr_t>(&original_heads) - reinterpret_cast<std::uintptr_t>(module)
            << ",\"targets\":[" << reinterpret_cast<std::uintptr_t>(targets[0]) << ','
            << reinterpret_cast<std::uintptr_t>(targets[1]) << "]}\n" << std::flush;
        accepting.store(true);
        if (mhw::astera::active()) signal_startup();
        else {
            enabled.fill(true); // cleanup also covers a partially applied batch
            require(MH_ApplyQueued() == MH_OK, "enable-creation-hooks");
        }
#ifdef MHW_REFLEX_TEMPORAL
        // Native API callbacks run on the game thread while this worker admits
        // the later NGX initializer. One early pool, one worker, two SDK owners.
#ifdef MHW_ALPHA
        mhw::reflex::temporal::prepare(module,output,image,runtime);
#else
        mhw::reflex::temporal::prepare(module,output,image);
#endif
        { std::lock_guard lock(sdk_mutex);
          mhw::reflex::temporal::receipt(log,"prepared",device_pin.Get()); }
#endif
        { std::lock_guard lock(sdk_mutex);
          log << "{\"event\":\"ready\",\"early_api_hooks\":2,\"startup_completed\":" << completed.load() << "}\n" << std::flush; }
        const auto began = GetTickCount64();
        while (!completed.load() && GetTickCount64() - began < 180000) Sleep(20);
        const bool hooks_clean = clean_hooks();
        const bool swap_observed = success && hooks_clean && describe_swap() && !swapchain_chains_missing.load();
#ifdef MHW_ALPHA
        if (!swap_observed) mhw::alpha::status::worker_failed(failure.empty() ? std::string_view("presentation") : std::string_view(failure));
#endif
        if (swap_observed) {
#ifdef MHW_REFLEX_FRAME_PROBE
            require(driver_mode("startup_off",false),"driver-startup-Off");
            log << "{\"event\":\"frames_ready\",\"mode\":0,\"target_mode\":1,\"sleep_calls\":0,\"markers\":0,\"armed\":false,\"duration_seconds\":" << capture_seconds << "}\n" << std::flush;
#ifdef MHW_FG_GAME
            log << "{\"event\":\"fg_crash_log\",\"installed\":" << crash_log_installed << "}\n";
            log_fg_buffers(log, "frames_ready");
#endif
            const auto wait_start=GetTickCount64();
#ifdef MHW_FG_GAME
            log << "{\"event\":\"perf_toggles\",\"request\":0,\"mask\":" << mhw::perf::toggles.load() << ",\"previous\":0,\"accepted\":true}\n" << std::flush;
            mhw::perf::host_frames.take();
#ifndef MHW_ALPHA
            auto window_start=wait_start;
            mhw::perf::sampler::start();
#else
            if(alpha_config.profile)mhw::perf::sampler::start();
            // Hot game-memory regions stay validated off the render thread.
            mhw::bridge_probe::start_region_refresher();
            if(alpha_config.hook_timing)mhw::perf::hook_timing::enable();
#endif
#endif
#ifdef MHW_ALPHA
            // Arm once the swap chain has been unchanged for 3 s (startup creates
            // it and resizes it several times) and the game qualifies; until
            // then the game renders natively. The first and the latest reason
            // the game was not ready yet are logged when it arms.
            bool arm_now=false;
            {
                auto stats=presentation->snapshot();auto stable_since=GetTickCount64();unsigned attempts{};
                std::string not_ready,first_reason;bool waiting_logged=false;
                while(stats.routing && !stats.faults) {
                    Sleep(250);
                    const auto now_stats=presentation->snapshot();
                    if(now_stats.swapchains!=stats.swapchains||now_stats.resizes!=stats.resizes)stable_since=GetTickCount64();
                    stats=now_stats;
                    log_lifecycle(log);
                    if(GetTickCount64()-stable_since<3000)continue;
                    not_ready.clear();
                    if(mhw::reflex::temporal::ready(not_ready)){arm_now=true;break;}
                    if(!attempts++)first_reason=not_ready;
                    // A game that never qualifies would otherwise leave no reason
                    // in the log until the session ends (game-63).
                    if(!waiting_logged&&GetTickCount64()-wait_start>=15000){waiting_logged=true;
                        log << "{\"event\":\"alpha_waiting\",\"attempts\":" << attempts << ",\"waited_ms\":" << GetTickCount64()-wait_start
                            << ",\"reason\":" << quoted(not_ready) << "}\n" << std::flush;}
                }
                log << "{\"event\":\"alpha_arm\",\"ready\":" << arm_now << ",\"attempts\":" << attempts << ",\"waited_ms\":"
                    << GetTickCount64()-wait_start << ",\"first_reason\":" << quoted(first_reason) << ",\"last_reason\":" << quoted(not_ready)
                    << ",\"routing\":" << stats.routing << ",\"faults\":" << stats.faults << ",\"fault_site\":" << stats.fault_site
                    << ",\"fault_hr\":" << stats.fault_hr << "}\n" << std::flush;
                if(!arm_now)mhw::alpha::status::worker_failed(not_ready.empty()?std::string_view("presentation"):std::string_view(not_ready));
            }
            if(arm_now) {
#else
            while(!std::filesystem::exists(arm_file) && !std::filesystem::exists(stop_file) && GetTickCount64()-wait_start<600000) {
                const auto stats=presentation->snapshot();if(!stats.routing || stats.faults)break;
#ifdef MHW_FG_GAME
                log_lifecycle(log);
                poll_perf(log,output,name,creation,perf_requests);
                if(GetTickCount64()-window_start>=5000){log_frames(log,"unarmed");mhw::perf::sampler::write(log);window_start=GetTickCount64();}
#endif
                Sleep(100);
            }
            if(std::filesystem::exists(arm_file) && !std::filesystem::exists(stop_file)) {
#endif
#ifdef MHW_REFLEX_OWNERSHIP_PROBE
                presentation->ownership_running();
#endif
#ifdef MHW_REFLEX_TEMPORAL
                // Qualify temporal gates before the three Reflex frame gates
                // are patched. Its own observer continues to own submission.
                mhw::reflex::temporal::arm(device_pin.Get());
                mhw::reflex::temporal::receipt(log,"armed",device_pin.Get());
#ifdef MHW_ALPHA
                {
                    unsigned render_w{},render_h{},scene_w{},scene_h{},window_w{},window_h{};
                    mhw::reflex::temporal::render_extent(render_w,render_h);
                    mhw::reflex::temporal::scene_extent(scene_w,scene_h,window_w,window_h);
                    mhw::alpha::status::dlss_sizes(render_w,render_h,scene_w,scene_h);
                    mhw::alpha::status::dlss_mode(mhw::alpha::dlss_mode_name(mhw::continuous_dlaa::reconstruction_mode()));
                    mhw::alpha::status::dlss(mhw::continuous_dlaa::dlss_off()?mhw::alpha::status::Stage::player_off:mhw::alpha::status::Stage::running);
                    // NGX startup and the DLSS install succeeded (capabilities.hpp).
                    mhw::alpha::status::dlss_available(true);
                }
#endif
#endif
                // The alpha arms the player's mode (Off only without DLSS-G).
                auto arm_mode=capture_mode;
#ifdef MHW_ALPHA
                arm_mode=static_cast<sl::ReflexMode>(reflex_requested.load());
                if(arm_mode==sl::ReflexMode::eOff&&fg_loaded)arm_mode=capture_mode;
#endif
                {
                    std::lock_guard lock(sdk_mutex);
                    mode_change_attempted=true;
                    const auto result=sdk.options(arm_mode);
                    mode_options("activate_on",arm_mode,result);api(result,"Reflex-On");
                    reflex_applied.store(static_cast<unsigned>(arm_mode));reflex_requested.store(static_cast<unsigned>(arm_mode));
                    // Owned runs report Off immediately and after frame 8, On
                    // by frame 32. Keep this observation, require the mode at
                    // the first five-second checkpoint and again after drain.
                    require(driver_mode("requested_on",arm_mode!=sl::ReflexMode::eOff,false),"driver-On-query");
                }
                live_frames=new mhw::reflex::LiveFrames(sdk);
                presentation->observe(live_frames->observer());
                live_frames->install(image,reinterpret_cast<std::uintptr_t>(device_pin.Get()),log);live_frames->describe(log,static_cast<unsigned>(arm_mode));
                log << "{\"event\":\"frames_armed\",\"mode\":" << static_cast<unsigned>(arm_mode) << ",\"duration_seconds\":" << capture_seconds << ",\"sleep_calls_enabled\":true,\"markers_enabled\":true}\n" << std::flush;
#ifdef MHW_ALPHA
                mhw::alpha::status::reflex(mhw::alpha::status::Stage::running);
#endif
#ifdef MHW_FG_GAME
                // With MHW's aspect-ratio option the scene is letterboxed or
                // pillarboxed in the back buffer, centred (checked against the
                // screen write's iRegion and viewport in the FG status).
                unsigned scene_width{}, scene_height{}, window_width{}, window_height{};
                const bool scene_known = mhw::reflex::temporal::scene_extent(scene_width, scene_height, window_width, window_height);
                if (!fg_plugin) {
                    // "<dll>.fg-mode" off: DLSS and Reflex only; the DLSS-G plugin is not loaded.
                    log << "{\"event\":\"fg_unsupported\",\"reason\":\"plugin_off\"}\n" << std::flush;
                } else if (const auto chain = presentation->snapshot(); !scene_known || !scene_width || !scene_height ||
                    scene_width > chain.width || scene_height > chain.height || window_width != chain.width || window_height != chain.height) {
                    log << "{\"event\":\"fg_unsupported\",\"reason\":\"scene_rect\",\"scene_known\":" << scene_known
                        << ",\"scene\":[" << scene_width << ',' << scene_height << "],\"window\":[" << window_width << ',' << window_height
                        << "],\"chain\":[" << chain.width << ',' << chain.height << "]}\n" << std::flush;
#ifdef MHW_ALPHA
                    mhw::alpha::status::generation(mhw::alpha::status::Generation::unavailable, "scene_rect");
#endif
                } else {
                    const mhw::frame_generation::game::SceneRect scene{(chain.width - scene_width) / 2, (chain.height - scene_height) / 2,
                                                                       scene_width, scene_height};
                    mhw::frame_generation::game::back_buffer(chain.format);
                    mhw::frame_generation::game::configure(sdk, chain.width, chain.height, scene);
                    mhw::frame_generation::game::multiplier(fg_multiplier);
#ifdef MHW_ALPHA
                    mhw::frame_generation::game::mode(fg_mode, fg_target);
#endif
#ifdef MHW_ALPHA
                    // Armed whenever DLSS runs with DLSS-G loaded (fg_ready); v0.27.4
                    // loads and unloads DLSS-G with the swap chain (sync_generation).
                    fg_ready=true;
                    game_chain_ready=game_chain_qualified();
                    if(game_chain_ready)presentation->waitable_slot(game_waitable_slot());
                    log<<"{\"event\":\"fg_switch_ready\",\"game_chain\":"<<game_chain_ready<<",\"identity\":"<<presentation->snapshot().identity<<"}\n"<<std::flush;
                    if(!mhw::continuous_dlaa::dlss_off()&&fg_loaded)mhw::frame_generation::game::arm(true);
#else
                    mhw::frame_generation::game::arm(true);
#endif
                    // DLSS-G's state is read on the present thread (Streamline
                    // requires it there) and logged in fg_status ("state").
                    log << "{\"event\":\"fg_armed\",\"width\":" << chain.width << ",\"height\":" << chain.height
                        << ",\"scene\":[" << scene.x << ',' << scene.y << ',' << scene.width << ',' << scene.height << ']'
                        << ",\"display_hz\":" << mhw::frame_generation::display_refresh_hz() << "}\n" << std::flush;
#ifdef MHW_ALPHA
                    mhw::alpha::status::generation(mhw::alpha::status::Generation::armed);
#endif
                }
#endif
                const auto started=GetTickCount64();auto last=started;
                // v0.24.7: Reflex is confirmed running at the first heartbeat
                // with frames. The mod can arm before the game's first frame,
                // and a first launch after a driver install can compile
                // shaders for longer than a heartbeat (game-105: no frame in
                // the first 20 s, the check failed and DLSS stayed off).
                constexpr std::uint64_t reflex_check_frames=60;bool reflex_confirmed=false;
#ifdef MHW_ALPHA
                // After a live mode change: the sleep count at which the driver is
                // asked whether it took the change (it applies at the next sleep).
                std::uint64_t live_check_sleeps{};
                DlssRetry dlss_retry;SceneRetry scene_retry;GenerationSwitch generation_switch;unsigned learned_multiplier{};
                // perf_ab (diagnostic, FG off): DLSS paused and running in
                // turn, 30 s each, logged with the time to the plugin log.
                std::uint64_t perf_ab_next=alpha_config.perf_ab&&!fg_loaded?GetTickCount64()+30000:0;bool perf_ab_paused=false;
#endif
#ifdef MHW_FG_SCENE_PROBE
                unsigned scene_requested{};
#endif
#ifdef MHW_FG_GAME
#ifndef MHW_ALPHA
                unsigned multiplier_requests{};
#endif
                stop_reason="duration";
#endif
#ifdef MHW_ALPHA
                // 20 s, so a session closed soon after an event still logs it.
                constexpr std::uint64_t heartbeat_ms=20000;
                for(;;) {
#else
                constexpr std::uint64_t heartbeat_ms=5000;
                while(!std::filesystem::exists(stop_file) && GetTickCount64()-started<capture_seconds*1000ULL) {
#endif
                    const auto frames=live_frames->snapshot();const auto stats=presentation->snapshot();
#ifdef MHW_FG_GAME
                    log_lifecycle(log);
                    if(frames.closed && !frames.faults){
                        // The game destroyed its window (its exit); logged at once, the process ends soon.
                        log << "{\"event\":\"game_exit\",\"completed\":" << frames.completed << "}\n" << std::flush;
#if defined(MHW_ALPHA)&&defined(MHW_REFLEX_TEMPORAL)
                        // v0.24.7: a last summary, so a session shorter than
                        // one heartbeat still records its counters. Built in
                        // memory and written at once: the process may end.
                        {std::ostringstream summary;
                         mhw::reflex::temporal::sample();
                         summary << "{\"event\":\"fg_status\",\"display_hz\":" << mhw::frame_generation::display_refresh_hz() << ",\"session\":";
                         mhw::frame_generation::game::write(summary);summary << "}\n";
                         summary << "{\"event\":\"dlss_summary\",\"runtime\":";mhw::reflex::temporal::summary(summary);summary << "}\n";
                         log << summary.str() << std::flush;}
#endif
                        stop_reason="game_exit";break;
                    }
                    if(!frames.running || frames.faults){
                        // Logged at once: the process may end before fg_stopped.
                        log << "{\"event\":\"reflex_fault\",\"running\":" << frames.running << ",\"completed\":" << frames.completed << ',';
                        write_reflex_fault(log, frames);log << "}\n" << std::flush;
                        stop_reason="reflex_frames";break;
                    }
                    if(!stats.routing || stats.faults){stop_reason="presentation";break;}
#endif
                    if(!frames.running || frames.faults || !stats.routing || stats.faults)break;
#ifdef MHW_ALPHA
                    if(reflex_requested.load()!=reflex_applied.load()){
                        std::lock_guard lock(sdk_mutex);
                        if(apply_reflex_mode(fg_loaded))live_check_sleeps=live_frames->snapshot().sleeps+2;
                    }else if(live_check_sleeps&&frames.sleeps>=live_check_sleeps){
                        // As at the first heartbeat: low latency on for On and Boost, off for Off.
                        live_check_sleeps=0;
                        require(driver_mode("live",reflex_applied.load()!=0),"driver-live-mode");
                    }
                    mhw::render_resolution::write_captures(log);
                    if(perf_ab_next&&GetTickCount64()>=perf_ab_next){
                        perf_ab_paused=!perf_ab_paused;perf_ab_next+=30000;
                        const bool applied=mhw::continuous_dlaa::diagnostic_pause(perf_ab_paused);
                        loader_log(AL_LOG_INFO,std::string("perf: dlss = ")+(perf_ab_paused?"paused":"running")+(applied?" (applied now)":" (refused)"));
                        log<<"{\"event\":\"perf_ab\",\"paused\":"<<perf_ab_paused<<",\"applied\":"<<applied<<"}\n"<<std::flush;
                        if(!applied)perf_ab_next=0;
                    }
                    // The game's own aspect switch (render_resolution yielded).
                    sync_generation(generation_switch);
                    {   // DLSS-G's limits once its state was read (capabilities.hpp).
                        const auto session=mhw::frame_generation::game::status();
                        if(session.maximum&&session.maximum+1!=learned_multiplier){
                            learned_multiplier=session.maximum+1;
                            const auto dynamic=session.dynamic_support==2?mhw::alpha::capabilities::Support::yes
                                              :session.dynamic_support==1?mhw::alpha::capabilities::Support::no:mhw::alpha::capabilities::Support::unknown;
                            mhw::alpha::status::generation_limits(learned_multiplier,dynamic);
                            log<<"{\"event\":\"fg_limits\",\"source\":\"session\",\"max_multiplier\":"<<learned_multiplier<<",\"dynamic\":"<<session.dynamic_support<<"}\n"<<std::flush;
                        }
                    }
                    // While the player has DLSS off it waits for the way back (change_dlss_mode).
                    if(mhw::render_resolution::scene_changed()&&GetTickCount64()>=scene_retry.at&&!mhw::continuous_dlaa::dlss_off())
                        change_dlss_scene(scene_retry);
                    {   // A live preset change or Off from the menu, or its retry.
                        mhw::continuous_dlaa::ModeRequest wanted{};
                        if(mhw::continuous_dlaa::requested_mode(wanted)){dlss_retry={};change_dlss_mode(wanted,dlss_retry);}
                        else if(dlss_retry.at&&GetTickCount64()>=dlss_retry.at){dlss_retry.at=0;change_dlss_mode(dlss_retry.request,dlss_retry);}
                    }
#endif
#ifdef MHW_FG_SCENE_PROBE
                    if(frames.completed&&scene_requested<4) {
                        const unsigned request=scene_requested+1;
                        const auto marker=output/(name+".scene-"+std::to_string(request));
                        bool valid=request==1;
                        if(!valid&&std::filesystem::exists(marker)){
                            require(std::filesystem::file_size(marker)<=64,"bounded-scene-request");
                            std::ifstream input(marker);std::uint64_t generation{};input>>generation;
                            valid=bool(input)&&generation==creation;
                            require(valid,"scene-request-generation");
                        }
                        // Request-time host GetBuffer labels seed presentation-only
                        // diagnostic matching; Present rechecks their generation.
                        mhw::frame_generation::PresentationBuffers buffers{};
                        const bool buffers_read=valid&&mhw::reflex::scene_presentation_buffers(buffers);
                        if(valid&&mhw::scaled_temporal::stream_route::scene_request(request,buffers)){
                            scene_requested=request;
                            log<<"{\"event\":\"scene_capture_requested\",\"request\":"<<request<<",\"generation_checked\":true"
                               <<",\"presentation_buffers_read\":"<<buffers_read<<",\"presentation_generation\":"<<buffers.generation
                               <<",\"presentation_known_mask\":"<<buffers.known_mask<<"}\n"<<std::flush;
                        }
                    }
#endif
#ifdef MHW_FG_GAME
#ifndef MHW_ALPHA
                    poll_perf(log,output,name,creation,perf_requests);
                    {   // Sequenced, generation-checked multiplier requests: "<creation> <total>".
                        const auto marker=output/(name+".mfg-"+std::to_string(multiplier_requests+1));
                        if(std::filesystem::exists(marker)&&std::filesystem::file_size(marker)<=64){
                            std::ifstream input(marker);std::uint64_t generation{};unsigned total{};input>>generation>>total;
                            const bool accepted=bool(input)&&generation==creation&&mhw::reflex::generated_frames_for(total,0);
                            if(accepted)mhw::frame_generation::game::multiplier(total);
                            ++multiplier_requests;
                            log<<"{\"event\":\"fg_multiplier\",\"request\":"<<multiplier_requests<<",\"multiplier\":"<<total
                               <<",\"accepted\":"<<accepted<<"}\n"<<std::flush;
                        }
                    }
#endif
#endif
#ifdef MHW_REFLEX_TEMPORAL
                    if(!mhw::reflex::temporal::healthy()){
#ifdef MHW_FG_GAME
                        stop_reason="temporal_unhealthy";mhw::reflex::temporal::fault_snapshot();
#endif
                        break;
                    }
#endif
                    const auto now=GetTickCount64();
                    if(now-last>=heartbeat_ms) {
#ifdef MHW_REFLEX_TEMPORAL
                        mhw::reflex::temporal::sample();
#ifdef MHW_FG_GAME
                        log << "{\"event\":\"fg_status\",\"display_hz\":" << mhw::frame_generation::display_refresh_hz()
                            << ",\"session\":";mhw::frame_generation::game::write(log);log << "}\n" << std::flush;
#ifdef MHW_ALPHA
                        log << "{\"event\":\"dlss_summary\",\"runtime\":";mhw::reflex::temporal::summary(log);log << "}\n" << std::flush;
                        mhw::perf::hook_timing::write(log,frames.completed);
#endif
                        log_fg_buffers(log, "status");
                        log_frames(log, "armed");log_latency(log);mhw::perf::sampler::write(log);
#endif
#endif
                        if(!reflex_confirmed&&frames.completed>=reflex_check_frames){
                            require(driver_mode("running_on",reflex_applied.load()!=0),"driver-On-not-enabled");reflex_confirmed=true;}
                        log << "{\"event\":\"frame_heartbeat\",\"completed\":" << frames.completed << ",\"tokens\":" << frames.tokens
                            << ",\"sleeps\":" << frames.sleeps << ",\"markers\":" << frames.markers << ",\"faults\":" << frames.faults
                            << ",\"owner\":" << frames.owner << ",\"presents\":" << stats.presents << ",\"native_presents\":" << stats.native_presents << "}\n" << std::flush;
                        last=now;
                    }
                    Sleep(100);
                }
            }
#else
            log << "{\"event\":\"persistent_ready\",\"mode\":0,\"sleep_calls\":0,\"markers\":0,\"ceiling_seconds\":600}\n" << std::flush;
            const auto started = GetTickCount64(); auto last = started;
            while (!std::filesystem::exists(stop_file) && GetTickCount64() - started < 600000) {
                const auto stats = presentation->snapshot();
                if (!stats.routing || stats.faults) break;
                std::uintptr_t current_backend{}, current_device{};
                if (!pb::read(image + 0x51c7050, current_backend) || !current_backend ||
                    !pb::read(current_backend + 0x18, current_device) || current_device != stats.native_device) {
                    log << "{\"event\":\"runtime_rejection\",\"reason\":\"native-device-changed\"}\n" << std::flush;
                    success = false; break;
                }
                const auto now = GetTickCount64();
                if (now - last >= 5000) {
                    log << "{\"event\":\"heartbeat\",\"presents\":" << stats.presents << ",\"native_presents\":" << stats.native_presents
                        << ",\"queues\":" << stats.queues << ",\"swapchains\":" << stats.swapchains << ",\"resizes\":" << stats.resizes
                        << ",\"faults\":" << stats.faults << ",\"native_swapchain\":" << stats.native_swapchain
                        << ",\"proxy_swapchain\":" << stats.proxy_swapchain << ",\"width\":" << stats.width << ",\"height\":" << stats.height
                        << ",\"format\":" << stats.format << ",\"buffers\":" << stats.buffers << ",\"installed_hooks\":" << stats.installed_hooks << "}\n" << std::flush;
                    last = now;
                }
                Sleep(100);
            }
#endif
        }
#ifdef MHW_FG_GAME
        // Under SDK pacing the game renders into Streamline's buffers and the
        // physical chain belongs to its pacer; neither can be retired under MHW.
        // Stop therefore means FG Off; DLSS, Reflex and routing stay live until exit.
        if(live_frames) {
#ifdef MHW_ALPHA
            {   // The loop ended on a fault: frame generation is off from here on.
                // The game's exit is not one; the status keeps its last state.
                const std::string_view why=stop_reason;
                if(why=="reflex_frames"||why=="presentation")mhw::alpha::status::reflex(mhw::alpha::status::Stage::off,why);
                if(why!="game_exit")mhw::alpha::status::worker_failed(why);
            }
#endif
            mhw::frame_generation::game::arm(false);
            const auto fg_off_began=GetTickCount64();
            while(mhw::frame_generation::game::frame_generation_on() && GetTickCount64()-fg_off_began<3000)Sleep(10);
#ifdef MHW_ALPHA
            // A DLSS fault: the game's own rendering returns, at its own render
            // scale and anti-aliasing, instead of the reduced scale DLSS asked for.
            if(stop_reason==std::string_view("temporal_unhealthy")){
                mhw::continuous_dlaa::stop();
                const auto until=GetTickCount64()+3000;
                while(GetTickCount64()<until&&!mhw::render_resolution::write_captures(log))Sleep(50);
            }
#endif
            const auto frames=live_frames->snapshot();
            if(stop_reason==std::string_view("duration")&&std::filesystem::exists(stop_file))stop_reason="stop_request";
            log_lifecycle(log);
            log << "{\"event\":\"fg_stopped\",\"reason\":\"" << stop_reason << "\",\"off\":" << !mhw::frame_generation::game::frame_generation_on()
                << ",\"reflex_frames\":" << frames.completed << ",\"reflex_faults\":" << frames.faults << ',';
            write_reflex_fault(log, frames);log << ",\"session\":";
            mhw::frame_generation::game::write(log);log << "}\n";
        }
        log << "{\"event\":\"fg_runtime_retained\",\"sdk_alive\":true,\"presentation_routed\":true,\"temporal_active\":true}\n" << std::flush;
        // Keep recording host lifecycle calls (resizes after FG ended) with a
        // presentation heartbeat on change, until exit or 30 minutes.
        {
            const auto observed_from=GetTickCount64();std::uint64_t last_resizes=~0ULL,last_swapchains=~0ULL;
            auto window_start=observed_from;log_frames(log,"stop");
            while(presentation&&GetTickCount64()-observed_from<1800000ULL){
                log_lifecycle(log);
                poll_perf(log,output,name,creation,perf_requests);
                if(GetTickCount64()-window_start>=5000){log_frames(log,"fg_off");log_latency(log);mhw::perf::sampler::write(log);window_start=GetTickCount64();}
#ifdef MHW_ALPHA
                mhw::render_resolution::write_captures(log);
#endif
                const auto stats=presentation->snapshot();
                if(stats.resizes!=last_resizes||stats.swapchains!=last_swapchains){
                    log<<"{\"event\":\"presentation_state\",\"swapchains\":"<<stats.swapchains<<",\"resizes\":"<<stats.resizes
                       <<",\"width\":"<<stats.width<<",\"height\":"<<stats.height<<",\"buffers\":"<<stats.buffers<<",\"faults\":"<<stats.faults
                       <<",\"routing\":"<<stats.routing<<",\"presents\":"<<stats.presents<<",\"queues\":"<<stats.queues<<",\"fault_site\":"<<stats.fault_site<<",\"fault_hr\":"<<stats.fault_hr
                       <<",\"identity\":"<<stats.identity<<",\"identity_refused\":"<<stats.identity_refused<<",\"frame_generation_loaded\":"<<stats.frame_generation_loaded
                       <<",\"switches\":"<<stats.switches<<",\"switch_failures\":"<<stats.switch_failures<<",\"switch_refusals\":"<<stats.switch_refusals
                       <<",\"switch_reason\":"<<stats.switch_reason<<",\"switch_waits\":"<<stats.switch_waits<<"}\n"<<std::flush;
                    last_resizes=stats.resizes;last_swapchains=stats.swapchains;
                }
                Sleep(100);
            }
        }
        return 0;
#endif
#ifdef MHW_REFLEX_TEMPORAL
        const bool temporal_clean=finish_temporal();
#endif
        std::lock_guard lock(sdk_mutex);
#ifdef MHW_REFLEX_FRAME_PROBE
        const bool frames_clean=!live_frames || live_frames->stop();
        const auto frame_stats=live_frames?live_frames->snapshot():mhw::reflex::LiveFrameStats{};
        if(live_frames) { live_frames->write(log);frames_written=true; }
        const bool latency_routed=!presentation || presentation->stop();
        if(initialized && frames_clean && latency_routed) {
            if(mode_change_attempted)require(driver_mode("stopped_on",true),"driver-On-not-retained");
            sl::ReflexState state{};api(sdk.state(state),"final-Reflex-state");
            unsigned reports{};
            for(const auto& f:state.frameReport) {
                if(!f.simStartTime || !f.simEndTime || !f.renderSubmitStartTime || !f.renderSubmitEndTime || !f.presentStartTime || !f.presentEndTime)continue;
                ++reports;
                log << "{\"event\":\"driver_frame\",\"frame\":" << f.frameID << ",\"simulation_start_us\":" << f.simStartTime
                    << ",\"simulation_end_us\":" << f.simEndTime << ",\"submit_start_us\":" << f.renderSubmitStartTime
                    << ",\"submit_end_us\":" << f.renderSubmitEndTime << ",\"present_start_us\":" << f.presentStartTime << ",\"present_end_us\":" << f.presentEndTime << "}\n";
            }
            log << "{\"event\":\"driver_state\",\"mode\":" << (mode_change_attempted?1:0) << ",\"mode_source\":\"requested\",\"low_latency_available\":" << state.lowLatencyAvailable
                << ",\"reports_available\":" << state.latencyReportAvailable << ",\"complete_reports\":" << reports << "}\n" << std::flush;
        }
#endif
        // Quiesce routes before the final state sample or vendor shutdown.
        const bool routes_clean = !presentation || presentation->stop();
        const auto stats = presentation ? presentation->snapshot() : mhw::reflex::PresentationStats{};
        const bool sdk_clean = close_sdk();
        log << "{\"event\":\"sdk_shutdown\",\"passed\":" << sdk_clean << ",\"after_presentation_stop\":true}\n";
        log << "{\"event\":\"finished\",\"startup_passed\":" << success << ",\"hooks_restored\":" << (hooks_clean && routes_clean)
            << ",\"sdk_shutdown_passed\":" << sdk_clean << ",\"qualified_factories\":" << qualified_factories
            << ",\"qualified_devices\":" << qualified_devices << ",\"pending_calls\":" << active.load() << ",\"swapchain_observed\":" << swap_observed
            << ",\"swapchain_proxy_installed\":false,\"game_reflex_active\":"
#ifdef MHW_REFLEX_FRAME_PROBE
            << (mode_change_attempted && !mode_restored)
#else
            << false
#endif
            << ",\"presents\":" << stats.presents
            << ",\"native_presents\":" << stats.native_presents << ",\"queues\":" << stats.queues << ",\"swapchains\":" << stats.swapchains
            << ",\"resizes\":" << stats.resizes << ",\"faults\":" << stats.faults << ",\"presentation_pending\":" << stats.pending_calls
            << ",\"pins_released\":" << sdk_clean
#ifdef MHW_REFLEX_TEMPORAL
            << ",\"temporal_drained\":" << temporal_clean
#endif
#ifdef MHW_REFLEX_FRAME_PROBE
            << ",\"frame_hooks_restored\":" << frames_clean << ",\"frame_calls\":" << frame_stats.completed
            << ",\"sleep_calls\":" << frame_stats.sleeps << ",\"markers\":" << frame_stats.markers
            << ",\"frame_faults\":" << frame_stats.faults
            << ",\"mode_restored_off\":" << mode_restored
#endif
            << "}\n" << std::flush;
#ifdef MHW_REFLEX_FRAME_PROBE
        return success && hooks_clean && routes_clean && frames_clean && sdk_clean && swap_observed && frame_stats.completed &&
            !frame_stats.faults && !frame_stats.frame_active && frame_stats.tokens==frame_stats.completed &&
            frame_stats.sleeps==frame_stats.completed && frame_stats.markers==frame_stats.completed*6 && !stats.faults && log ? 0 : 2;
#else
        return success && hooks_clean && routes_clean && sdk_clean && swap_observed && stats.presents && !stats.faults && log ? 0 : 2;
#endif
    } catch (const std::exception& e) {
        try { startup_failure = e.what(); } catch (...) {}
#ifdef MHW_ALPHA
        mhw::alpha::status::worker_failed(e.what());
#endif
        signal_startup();
#ifdef MHW_FG_GAME
        // Once Streamline has created the host chain under SDK pacing, the game
        // renders into Streamline's buffers: unhooking or shutting it down here
        // destroys them under the game. Keep presentation routed; FG Off only.
        if (presentation && presentation->snapshot().swapchains) {
            mhw::frame_generation::game::arm(false);
            try { log << "{\"event\":\"worker_failed\",\"reason\":" << quoted(e.what())
                << ",\"sdk_retained\":true,\"presentation_routed\":true,\"fg_off_requested\":true}\n" << std::flush; } catch (...) {}
            return 3;
        }
#endif
        const bool clean = clean_hooks();
        std::lock_guard lock(sdk_mutex);
        const bool sdk_clean=close_sdk(); // cleanup emits records before this JSON object
        try { log << "{\"event\":\"worker_failed\",\"reason\":" << quoted(e.what()) << ",\"hooks_restored\":" << clean
            << ",\"sdk_shutdown_passed\":" << sdk_clean << "}\n" << std::flush; } catch (...) {}
#ifdef MHW_ALPHA
        // Failed before the log opened (configuration, paths, identity): leave
        // one line next to mhw_dlss.ini so the silent fail-closed is explainable.
        if (!log.is_open() && !alpha_base.empty()) try {
            std::ofstream early(alpha_base / L"startup-error.jsonl", std::ios::binary | std::ios::app);
            early << "{\"event\":\"worker_failed\",\"stage\":\"startup\",\"version\":\"" << mhw::alpha::version << "\",\"pid\":" << GetCurrentProcessId()
                << ",\"reason\":" << quoted(e.what()) << "}\n" << std::flush;
        } catch (...) {}
#endif
        return 3;
    }
}
}
#ifdef MHW_ALPHA
// Astera Loader plugin entry (plugin API 1.4: "al.d3d12" 1.0, "al.hooks" 1.0).
// AL loads AsteraLoader\plugins\mhw_dlss\mhw_dlss.dll, calls al_plugin_info,
// then al_plugin_init on its PRE_MAIN thread outside the loader lock. Init
// claims the presentation role, registers every DXGI/D3D12 detour (AL owns the
// patches), hands al.hooks to the live frame hooks, starts the worker and
// waits until it accepts the device.
namespace {
const al_api_t* loader_api{};
al_plugin_t* loader_self{};
const al_d3d12_t* loader_d3d12{};
void loader_begin(void* self) { loader_d3d12->begin_internal(static_cast<al_plugin_t*>(self)); }
void loader_end(void* self) { loader_d3d12->end_internal(static_cast<al_plugin_t*>(self)); }
void loader_log(al_log_level_t level, const std::string& text) noexcept { if (loader_api) loader_api->log(loader_self, level, text.c_str()); }
void loader_status(al_status_t status, const std::string& reason) noexcept {
    if (loader_api) loader_api->set_status(loader_self, status, reason.empty() ? nullptr : reason.c_str());
    namespace st = mhw::alpha::status;
    st::loader(status == AL_STATUS_DISABLED ? st::Loader::disabled : status == AL_STATUS_DEGRADED ? st::Loader::degraded : st::Loader::active, reason);
}
// UNAVAILABLE: AL will not call the detours (DirectX 11, a family failed).
bool loader_graphics_running(std::string& why) noexcept {
    al_d3d12_status_t s{}; s.size = sizeof(s); loader_d3d12->status(loader_self, &s);
    if (s.state != AL_D3D12_UNAVAILABLE) return true;
    why = s.reason ? s.reason : "al.d3d12 unavailable"; return false;
}
void AL_CALL on_phase(al_plugin_t*, al_phase_t phase, const void* data) {
    try {
        if (phase == AL_PHASE_DEVICE_CREATED) {
            const auto* d = static_cast<const al_phase_device_t*>(data);
            std::string why;
            if (!d || d->size < sizeof(*d) || !d->device) { loader_status(AL_STATUS_DEGRADED, "device phase without a device"); return; }
            if (!loader_graphics_running(why)) { loader_status(AL_STATUS_DEGRADED, "graphics hooks unavailable: " + why); return; }
            {
                Call call;
                if (accepting.load()) {
                    std::lock_guard lock(sdk_mutex);
                    if (accepting.load()) {
                        try {
                            attach_device(static_cast<IUnknown*>(d->adapter), static_cast<D3D_FEATURE_LEVEL>(d->minimum_feature_level),
                                static_cast<IUnknown*>(d->device), nullptr, 0);
                        } catch (const std::exception& e) { failed(e); }
                    }
                }
            }
            if (!success) loader_status(AL_STATUS_DEGRADED, failure.empty() ? "the game's device was not attached" : failure);
        } else if (phase == AL_PHASE_SWAPCHAIN_CREATED) {
            std::string why;
            if (!mhw::reflex::presentation_astera_swapchain_chains() || !loader_graphics_running(why)) {
                swapchain_chains_missing.store(true);
                loader_status(AL_STATUS_DEGRADED, "swap-chain hooks unavailable; DLSS and frame generation stay off");
            }
        }
    } catch (...) {}
}
const char* const supported_exes[]{mhw::projection_binding::host_sha, nullptr};
const al_plugin_info_t plugin_info{
    sizeof(al_plugin_info_t), AL_API_VERSION, "mhw_dlss", "MHW DLSS / Frame Generation", mhw::alpha::version,
    AL_PHASE_BIT(AL_PHASE_DEVICE_CREATED) | AL_PHASE_BIT(AL_PHASE_SWAPCHAIN_CREATED),
    0, nullptr, nullptr, supported_exes, on_phase, nullptr};
al_result_t start(const al_api_t* api, const al_init_t* init) {
    const auto refuse = [&](const std::string& why) { loader_status(AL_STATUS_DISABLED, why); loader_log(AL_LOG_ERROR, why); return AL_ERROR; };
    loader_d3d12 = static_cast<const al_d3d12_t*>(api->get_interface(init->self, AL_D3D12_INTERFACE, AL_D3D12_VERSION));
    if (!loader_d3d12 || loader_d3d12->size < sizeof(al_d3d12_t)) return refuse("this Astera Loader has no al.d3d12 1.0");
    const auto* game = static_cast<const al_game_t*>(api->get_interface(init->self, AL_GAME_INTERFACE, AL_GAME_VERSION));
    if (game && game->size >= sizeof(al_game_t)) {
        if (game->graphics == AL_GRAPHICS_DX11) return refuse("the game is set to DirectX 11; this mod needs DirectX 12");
        exe_hash_verified = game->exe_sha256 && std::string_view(game->exe_sha256) == mhw::projection_binding::host_sha;
    }
    if (loader_d3d12->claim_presentation(init->self) != AL_OK) return refuse("another plugin holds the presentation role (see the log)");
    mhw::astera::activate(loader_begin, loader_end, init->self);
    // Game functions other plugins share (the live frame hooks). Without
    // al.hooks the worker's frame stage fails closed: no Reflex or frame
    // generation, as with any other missing frame gate.
    const auto* game_hooks = static_cast<const al_hooks_t*>(api->get_interface(init->self, AL_HOOKS_INTERFACE, AL_HOOKS_VERSION));
    if (game_hooks && game_hooks->size >= sizeof(al_hooks_t)) mhw::astera::provide_game_hooks(game_hooks, init->self);
    else loader_log(AL_LOG_WARNING, "this Astera Loader has no al.hooks 1.0; Reflex and frame generation stay off");
    std::vector<mhw::astera::Hook> hooks;
    mhw::reflex::presentation_astera_hooks(hooks);
    mhw::temporal_present::observer::astera_hooks(hooks);
    mhw::continuous_dlaa::astera_hooks(hooks);
    mhw::scaled_temporal::stream_route::astera_hooks(hooks);
    std::vector<al_d3d12_hook_t> table;
    for (const auto& h : hooks) table.push_back({h.method, 0, h.detour, h.next});
    if (loader_d3d12->hook(init->self, table.data(), static_cast<std::uint32_t>(table.size())) != AL_OK)
        return refuse("graphics hook registration refused (see the log)");
    loader_log(AL_LOG_INFO, std::to_string(table.size()) + " graphics detours registered, presentation role held");
    startup_ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!startup_ready) return refuse("startup event");
    const auto thread = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
    if (!thread) return refuse("worker thread");
    CloseHandle(thread);
    // The worker reads its configuration, opens its log and reserves its hook
    // pool; device creation cannot start before this returns (PRE_MAIN).
    if (WaitForSingleObject(startup_ready, 30000) != WAIT_OBJECT_0) return refuse("startup timed out");
    if (!accepting.load()) return refuse(startup_failure.empty() ? "startup failed (see the mod's log)" : startup_failure);
    loader_status(AL_STATUS_ACTIVE, {});
    // CMFM's Mods menu, when installed (optional; the ini is read by now).
    mhw::alpha::menu::add_pages(api, init->self, alpha_config, alpha_base, alpha_config.generation_plugin);
    return AL_OK;
}
}
extern "C" __declspec(dllexport) const al_plugin_info_t* AL_CALL al_plugin_info(void) { return &plugin_info; }
extern "C" __declspec(dllexport) al_result_t AL_CALL al_plugin_init(const al_api_t* api, const al_init_t* init) {
    if (!api || !init || !init->self || api->size < offsetof(al_api_t, publish_interface)) return AL_ERROR;
    loader_api = api; loader_self = init->self;
    try { return start(api, init); } catch (...) { loader_status(AL_STATUS_DISABLED, "start-up exception"); return AL_ERROR; }
}
#endif
#ifdef MHW_ALPHA
namespace mhw::reflex {
void request_mode(unsigned mode) noexcept { if(mode<=2)reflex_requested.store(mode); }
unsigned applied_mode() noexcept { return reflex_applied.load(); }
unsigned frame_generation_support() noexcept { return fg_support_state.load(); }
bool frame_generation_loaded() noexcept { return fg_loaded_state.load(); }
}
#endif
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        module = instance; DisableThreadLibraryCalls(instance);
#ifndef MHW_ALPHA
        const auto thread = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
#endif
        // The alpha is an Astera Loader plugin: al_plugin_init starts the worker.
    }
    return TRUE;
}
