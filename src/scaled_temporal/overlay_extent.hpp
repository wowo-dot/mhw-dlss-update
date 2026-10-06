#pragma once
#include "effects.hpp"
namespace mhw::scaled_temporal {
// A native in-place overlay remains the same draw: UVs, shader, descriptors,
// constants and blending are retained. Only its raster coverage is expanded.
// Caller separately proves queue order, target identity/state and lifetimes.
bool overlay_pixel_shader(std::span<const std::byte>);
// Offsets come from the native PSO layout, never from an assumed packed quad.
// The pinned pass-through VS consumes only these two float XY attributes.
struct OverlayVertexLayout {
    unsigned stride{},position{},uv{};
    DXGI_FORMAT position_format{},uv_format{};
};
bool overlay_vertex_layout(std::span<const D3D12_INPUT_ELEMENT_DESC>,unsigned stride,OverlayVertexLayout &) noexcept;
bool overlay_vertices(std::span<const std::byte>,const OverlayVertexLayout &,std::span<float,16>) noexcept;
bool overlay_coverage(std::span<const float> vertices) noexcept;
bool overlay_raster(const EffectBindings &,FilterGeometry) noexcept;
void record_overlay(const EffectBindings &,FilterGeometry) noexcept;
}
