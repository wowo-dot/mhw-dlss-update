#pragma once
#include "../temporal_present/bindings.hpp"
#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>
#include <string>

namespace mhw::scaled_temporal {
// full_post (full_post.hpp): after DLSS the post chain runs at the output size,
// but three of its inputs are written by the scene at the render size, into
// the top-left render region of their output-size allocations
// (docs/research/full-size-post-chain.md):
// - the scene depth (R32_FLOAT; view +0x628 -> +0xA0): depth of field,
//   distance fog, depth colour, haze, god rays, motion blur, colour cubes;
// - GBuffer4 (R32_UINT; motion as two halves in screen-relative units, the low
//   bit a flag): motion blur;
// - GBuffer5 (R8G8_SNORM; extra motion in screen-relative units, read where
//   GBuffer4's flag is set): motion blur.
// Each becomes a nearest-texel upscale of its render region over the whole
// texture. Texels are copied bit-exact; no value is rescaled, since none of
// the three is in pixel units.
//
// Per input: copy the render region into a buffer, expand it into a second
// buffer at the output size (compute, raw buffers), copy that over the
// texture. The depth's render region stays in its own buffer until the next
// upscale, so that restore_depth() can put it back for the late draws (the
// hunter silhouettes) that read the depth at the render size after the post
// chain. Only root constants and root buffer views are bound: descriptor
// heaps, graphics bindings, render targets and the raster state are never
// touched. The compute root signature, its arguments and the pipeline are
// restored from the caller's snapshot of the list (the observer's mirror).
struct FullPostInput {
    ID3D12Resource *texture{};
    D3D12_RESOURCE_STATES state{}; // its state on entry; left in it
};
enum class FullPostInputRole { depth, gbuffer4, gbuffer5 };
constexpr DXGI_FORMAT full_post_input_format(FullPostInputRole r) noexcept {
    return r == FullPostInputRole::depth      ? DXGI_FORMAT_R32_FLOAT
           : r == FullPostInputRole::gbuffer4 ? DXGI_FORMAT_R32_UINT
                                              : DXGI_FORMAT_R8G8_SNORM;
}
// The texel the upscale reads for output coordinate x: the render texel under
// the output texel's centre, floor((x + 0.5) * render / output).
constexpr unsigned full_post_source_texel(unsigned x, unsigned render, unsigned output) noexcept {
    const auto t = static_cast<unsigned>((std::uint64_t(2) * x + 1) * render / (std::uint64_t(2) * output));
    return t < render ? t : render - 1;
}
class FullPostInputs {
public:
    // Output size; the buffers hold one 32-bit texel per output pixel.
    bool prepare(ID3D12Device *, unsigned width, unsigned height, std::string &reason) noexcept;
    bool ready() const noexcept { return pipeline_ != nullptr; }
    std::uint64_t bytes() const noexcept { return bytes_; }
    unsigned width() const noexcept { return width_; }
    unsigned height() const noexcept { return height_; }
    // Each texture's description must match its role at the output size, the
    // render region must fit, and the snapshot must be a known, valid recording
    // of `list`. The caller proves every state and serializes GPU use of the
    // buffers (one recording at a time, in GPU order). Returns false before the
    // first command if anything does not hold.
    bool record(ID3D12GraphicsCommandList *, const std::array<FullPostInput, 3> &, unsigned render_width,
                unsigned render_height, const temporal_present::Bindings &) noexcept;
    // The depth's render region alone, into the kept buffer: when the
    // upscale cannot be recorded, a later restore() then writes back the same
    // texels. Copies and barriers only, no binding changes.
    bool keep_depth(ID3D12GraphicsCommandList *, const FullPostInput &depth, unsigned render_width,
                    unsigned render_height) noexcept;
    // The kept render region back over the depth's top-left region (copies and
    // barriers only). The caller proves that record() or keep_depth() of the
    // same frame precedes it on the GPU, and the depth's current state.
    bool restore_depth(ID3D12GraphicsCommandList *, const FullPostInput &depth, unsigned render_width,
                       unsigned render_height) noexcept;

private:
    bool depth_input(ID3D12GraphicsCommandList *, const FullPostInput &, unsigned, unsigned) const noexcept;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12Resource> region_, expanded_, kept_;
    unsigned width_{}, height_{};
    std::uint64_t bytes_{};
};
} // namespace mhw::scaled_temporal
