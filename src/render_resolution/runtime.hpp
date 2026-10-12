#pragma once
#include <cstdint>
#include <iosfwd>
#include <string>
#include <d3d12.h>
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
bool input_stamp(std::uintptr_t view,InputStamp &) noexcept;
void stop() noexcept;
bool healthy() noexcept;
bool finish();
void write(std::ostream &);
}
