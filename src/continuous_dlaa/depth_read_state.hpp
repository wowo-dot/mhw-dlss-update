#pragma once
#include <d3d12.h>
namespace mhw::continuous_dlaa {
// Pinned native converter 0x259ba50: only the shader-read bits are admitted.
// A tracker can merge pixel and compute reads; its final state is not a proof
// that any transition has executed at the current reconstruction packet.
constexpr unsigned depth_shader_read_state(unsigned native) noexcept {
    if(!native||(native&~12U))return 0;
    return ((native&4U)?D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE:0U)|
           ((native&8U)?D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:0U);
}
constexpr bool depth_copy_state(unsigned state) noexcept {
    constexpr unsigned reads=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    return state==D3D12_RESOURCE_STATE_RENDER_TARGET||(state&&!(state&~reads));
}
}
