#pragma once
#include <cstdint>
#include <iosfwd>
#include <string>
#include <d3d12.h>
#include "control.hpp"
namespace mhw::render_resolution {
struct InputStamp {
    std::uint64_t frame{};std::uintptr_t view{};
    unsigned display_width{},display_height{},render_width{},render_height{};
    float active_scale{},content_scale{};
    bool operator==(const InputStamp &)const=default;
};
// shared_viewport means another owner forwards successful RSSetViewports calls
// to observe_viewports; this controller must not create or disable that hook.
bool install(std::uintptr_t image,float scale,std::string &reason,bool shared_viewport=false);
// install()'s qualification without hooks or retained state: the renderer is
// discovered, the plan is valid, the sizing code matches and the native sizing
// snapshot is admitted. Early in startup this fails and can be retried.
bool ready(std::uintptr_t image,float scale,std::string &reason);
void observe_viewports(ID3D12GraphicsCommandList *,unsigned,const D3D12_VIEWPORT *,std::uintptr_t caller) noexcept;
bool active_ready() noexcept;
// Worker, live preset change: plan `scale` against install()'s display and
// basis and make it the target from the render thread's next visit (each
// visit takes one copy of the target under the lock). active_ready() then
// waits for 32 visits at the new scale again. False (target unchanged) for an
// invalid plan, before install, after a fault or stop.
bool retarget(float scale,std::string &reason);
// Visits since install or the last retarget that showed the target applied.
std::uint64_t applied_visits() noexcept;
// Worker: writes each aspect-change capture once it is complete (with
// `partial`, also one still filling), as "aspect_capture" events. Returns how
// many it wrote. Research for the live aspect change (part B).
unsigned write_captures(std::ostream &,bool partial=false);
// The live aspect change (live-aspect.md). The game changed its scene size
// with its aspect option: the override yielded (the game renders at its own
// scale; input stamps refuse) instead of faulting. Any thread.
bool scene_changed() noexcept;
// Worker, after rediscovering the scene: plan `scale` on the new scene and
// basis and resume the override. False (still yielded) if no change is
// pending, after a fault, or for an invalid plan.
bool rebase(Extent display,unsigned basis,float scale,std::string &reason);
bool input_stamp(std::uintptr_t view,InputStamp &) noexcept;
void stop() noexcept;
bool healthy() noexcept;
bool finish();
void write(std::ostream &);
}
