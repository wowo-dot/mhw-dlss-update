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
void observe_viewports(ID3D12GraphicsCommandList *,unsigned,const D3D12_VIEWPORT *,std::uintptr_t caller) noexcept;
bool active_ready() noexcept;
bool input_stamp(std::uintptr_t view,InputStamp &) noexcept;
void stop() noexcept;
bool healthy() noexcept;
bool finish();
void write(std::ostream &);
}
