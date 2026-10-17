// DLSS in the session: the player's live preset changes and Off from the menu,
// and the game's own aspect change, each with frame generation paused around
// it.
#include "plugin.hpp"
#include "../alpha/status.hpp"
#include "../render_resolution/runtime.hpp"
#include "../reflex/temporal_coexistence.hpp"
#include <cstdint>

namespace mhw::plugin {
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
    if(fg_ready&&fg_loaded&&!generation_fault&&running&&released&&configured)fg::arm(true);
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
// Worker: the game changed its scene size with its aspect option
// (render_resolution yielded; continuous_dlaa::change_scene). Frame
// generation pauses as for a preset change and takes the new scene rectangle;
// the game renders at its own scale meanwhile. A failure after the rebuild
// began stops DLSS (fault 48). Frame generation (fg_ready) stays paused across
// the tries (its scene rectangle is the old one). While the player has DLSS
// off the switch waits for the way back (change_dlss_mode).
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
        // Not armed while DLSS-G is unloaded, but still ready for the next
        // load (v0.28.6: since v0.27.4 an aspect change with frame generation
        // off left it unavailable for the session).
        if(!configured){fg_ready=false;mhw::alpha::status::generation(mhw::alpha::status::Generation::unavailable,"scene_rect");}
        else if(fg_loaded&&!generation_fault)fg::arm(true);
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
}  // namespace mhw::plugin
