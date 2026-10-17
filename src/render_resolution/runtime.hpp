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
// The observer forwards successful RSSetViewports calls to observe_viewports.
// yielded_start: installed yielded (the player's DLSS Off at start, yield()).
bool install(std::uintptr_t image,float scale,std::string &reason,bool yielded_start=false);
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
// The player's DLSS Off: from the render thread's next visit the game's own
// scale applies (the owned request is restored, nothing is written) and the
// game may change its scale or aspect freely. An aspect switch meanwhile is
// noted as for scene_changed(); rebase() then resumes on the new scene.
// Unlike stop(), reversible. No effect before install, after a fault or stop.
void yield() noexcept;
// No override is requested and the render thread's last visit restored any
// owned value (after yield() or stop(); before install). The game's own scale
// setters then cannot race the restore. Any thread.
bool released() noexcept;
// Before install() only, as ready(): the game's own scale is 1 in every field
// the start-up check (the first 32 visits) compares. Worker.
bool full_scale(std::uintptr_t image) noexcept;
// Worker: the override again at `scale` (planned as retarget() does) from the
// next visit. The game's own scale meanwhile is adopted as its native value
// (restored at the next yield). False (still yielded) if not yielded, after a
// fault, for an invalid plan, or with a scene change pending (use rebase()).
bool resume(float scale,std::string &reason);
void stop() noexcept;
bool healthy() noexcept;
bool finish();
void write(std::ostream &);
}
