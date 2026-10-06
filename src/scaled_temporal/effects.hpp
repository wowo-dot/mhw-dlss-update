#pragma once
#include "filter_shader.hpp"
#include <d3d12.h>
#include <array>
#include <cstdint>
#include <memory>
#include <span>

namespace mhw::scaled_temporal {
// Values are stable diagnostic identities, not execution order. Haze, when
// present, executes between motion_filter and color_grade.
enum class EffectStage { motion_filter, color_grade, final_screen, haze, scene_copy, radial, dof_composite };
struct EffectVertices {
    std::array<float,16> values{};
    unsigned count{};
    D3D_PRIMITIVE_TOPOLOGY topology{};
};
// These describe the observed top-left native draws, including final-screen
// half-texel coordinates. No game data or native shader is embedded here.
EffectVertices effect_vertices(EffectStage,FilterGeometry,bool full_output);
struct EffectRecipe {
    EffectStage stage{};
    FilterGeometry geometry{};
    ID3D12RootSignature *root{};
    std::span<const std::byte> vertex_shader,pixel_shader;
    D3D12_BLEND_DESC blend{};
    D3D12_RASTERIZER_DESC raster{};
    D3D12_DEPTH_STENCIL_DESC depth{};
    // Base effects use a two-element layout and float CPU vertices. Dependency
    // effects additionally qualify their packed layout and bytes below.
    std::span<const D3D12_INPUT_ELEMENT_DESC> input_layout;
    std::span<const float> native_vertices;
    // Packed four-vertex dependency recipes: 16-byte copy or 28-byte two-UV
    // radial layout. Kept as bytes because packed color is not a float.
    std::span<const std::byte> dependency_vertices;
};
struct EffectBindings {
    ID3D12GraphicsCommandList *list{};
    ID3D12PipelineState *pipeline{};
    ID3D12RootSignature *root{};
    ID3D12Resource *output{},*vertex_resource{};
    D3D12_VERTEX_BUFFER_VIEW vertex_buffer{};
    D3D12_VIEWPORT viewport{};
    D3D12_RECT scissor{};
    D3D_PRIMITIVE_TOPOLOGY topology{};
    unsigned count{},first{};
};
// Qualifies native coverage and both UV domains without creating GPU objects.
bool dependency_geometry(const EffectRecipe &) noexcept;
// Construct before claiming the native draw. Private PSO/VB, immutable root
// signature and shader bytes. record() replaces one qualified DrawInstanced,
// expands its viewport/UVs, then restores exactly the four changed bindings.
// It never changes descriptors, root arguments, render targets or resources'
// states. Caller proves those bindings and the output's RT state at API entry;
// native wrapper/cache alone is NOT that proof. Unqualified calls must forward.
// Calls and GPU execution must be serialized by the owning renderer.
class Effect {
public:
    // Streaming callers may own the native references themselves. In that mode
    // every list, pipeline, output and vertex resource must remain pinned until
    // BOTH successful Reset and completion of that recording's submission.
    Effect(ID3D12Device *,const EffectRecipe &,bool caller_retained=false);
    ~Effect();
    Effect(const Effect &)=delete;
    Effect &operator=(const Effect &)=delete;
    bool record(const EffectBindings &) noexcept;
    // Caller supplies a fence covering every use and proves every containing
    // recording retired. Unproved uses intentionally retain their references.
    void retire(ID3D12Fence *,std::uint64_t,bool recordings_retired);
    bool recorded() const noexcept;
    bool retired() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
