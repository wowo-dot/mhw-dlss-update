#include "runtime.hpp"
#include "../render_resolution/runtime.hpp"
#include "../continuous_dlaa/activation.hpp"
#include "../temporal_bindings/probe.hpp"
#include "../projection_binding/memory.hpp"
#include <windows.h>
#include <MinHook.h>
#include <ostream>
namespace mhw::scaled_temporal {
namespace rr=render_resolution;namespace aa=continuous_dlaa::activation;namespace tb=temporal_bindings;
namespace {bool native_prepared{},activation_unhooked{},activation_disabled{},scale_finished{};}
bool install(std::uintptr_t image,std::string &reason){
    if(!rr::install(image,.5F,reason
#ifdef MHW_SCALED_OUTPUT
        ,true
#endif
        ))return false;
    const auto start=GetTickCount64();
    while(rr::healthy()&&!rr::active_ready()&&GetTickCount64()-start<5000)Sleep(1);
    if(!rr::active_ready()){reason="scaled renderer not ready";return false;}
    std::uintptr_t renderer{};
    if(!projection_binding::read(image+0x51c4480,renderer)||!renderer){reason="scaled renderer identity";return false;}
    // Queue activation without applying it. The existing recorder verifies its
    // original code first, installs its observers, then starts its short window.
    native_prepared=true;
    if(!aa::prepare(image,renderer,reason)||!tb::install(image,reason))return false;
    if(MH_ApplyQueued()!=MH_OK){reason="scaled temporal activation";return false;}
    return true;
}
bool healthy() noexcept {return rr::healthy()&&aa::healthy();}
bool finish() noexcept {
    // Caller has already canceled and drained the input recorder, including
    // its GPU copies. No new temporal queue work is then requested.
    aa::stop();const auto start=GetTickCount64();
    activation_unhooked=!native_prepared;
    while(!activation_unhooked&&GetTickCount64()-start<2000){
        if(aa::drained()&&aa::unhook()){activation_unhooked=true;break;}
        Sleep(1);
    }
    activation_disabled=!native_prepared;
    if(native_prepared&&activation_unhooked){
        // A thread redirected to the trampoline before patch removal can
        // still return through our callback. Observe its retirement separately.
        const auto disabled_at=GetTickCount64();
        while(!aa::drained()&&GetTickCount64()-disabled_at<2000)Sleep(1);
        activation_disabled=aa::drained();
    }
    scale_finished=rr::finish();return activation_disabled&&scale_finished&&healthy();
}
void write(std::ostream &out){
    rr::write(out);
    out<<"{\"event\":\"scaled_temporal\",\"schema\":1,\"activation_unhooked\":"<<activation_unhooked<<",\"activation_disabled\":"<<activation_disabled
       <<",\"scale_finished\":"<<scale_finished<<",\"activation\":";aa::write(out);
#ifdef MHW_SCALED_OUTPUT
    out<<",\"dlss_evaluation_limit\":1,\"motion_conversion_basis\":\"render-dimensions\"}\n";
#else
    out<<",\"dlss_evaluations\":0,\"motion_conversion_basis\":\"allocation-dimensions-diagnostic-only\"}\n";
#endif
}
}
