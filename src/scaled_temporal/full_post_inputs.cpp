#include "full_post_inputs.hpp"
#include <d3dcompiler.h>
#include <cstring>

namespace mhw::scaled_temporal {
namespace {
using Microsoft::WRL::ComPtr;
// One thread per 32-bit word of an output row: one R32 texel or two R8G8.
constexpr char shader_source[] = R"(
cbuffer Extent : register(b0) { uint in_w, in_h, out_w, out_h, in_pitch, out_pitch, bpp, words; };
ByteAddressBuffer region : register(t0);
RWByteAddressBuffer expanded : register(u0);
uint source(uint x, uint render, uint output) { return min(((2 * x + 1) * render) / (2 * output), render - 1); }
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= words || id.y >= out_h) return;
    uint row = source(id.y, in_h, out_h) * in_pitch;
    uint per = 4 / bpp, mask = bpp == 4 ? 0xffffffffu : 0xffffu, value = 0;
    for (uint k = 0; k < per; ++k) {
        uint x = id.x * per + k;
        if (x >= out_w) break;
        uint at = row + source(x, in_w, out_w) * bpp;
        value |= ((region.Load(at & ~3u) >> ((at & 3u) * 8)) & mask) << (k * bpp * 8);
    }
    expanded.Store(id.y * out_pitch + id.x * 4, value);
}
)";
constexpr unsigned pitch(unsigned width, unsigned bpp) noexcept {
    return (width * bpp + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
}
constexpr unsigned bytes_per_texel(FullPostInputRole r) noexcept {
    return r == FullPostInputRole::gbuffer5 ? 2U : 4U;
}
D3D12_RESOURCE_BARRIER transition(ID3D12Resource *r, D3D12_RESOURCE_STATES before,
                                  D3D12_RESOURCE_STATES after) noexcept {
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition = {r, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after};
    return b;
}
// Barriers whose states differ, in one call.
struct Batch {
    std::array<D3D12_RESOURCE_BARRIER, 3> b{};
    unsigned n{};
    void add(ID3D12Resource *r, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) noexcept {
        if (before != after) b[n++] = transition(r, before, after);
    }
    void issue(ID3D12GraphicsCommandList *list) noexcept {
        if (n) list->ResourceBarrier(n, b.data());
        n = 0;
    }
};
bool same_device(ID3D12DeviceChild *child, ID3D12Device *device) noexcept {
    ComPtr<ID3D12Device> actual;
    ComPtr<IUnknown> a, b;
    return child && device && SUCCEEDED(child->GetDevice(IID_PPV_ARGS(&actual))) && SUCCEEDED(actual.As(&a)) &&
           SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&b))) && a == b;
}
// The snapshot's compute root signature with its arguments, and its pipeline.
// A recording that never set a compute signature gets ours unbound; one that
// never set a pipeline keeps ours (the game must set its own before any draw
// or dispatch, as after every Reset without a pipeline).
void restore(ID3D12GraphicsCommandList *list, const temporal_present::Bindings &b) noexcept {
    using temporal_present::ArgumentKind;
    list->SetComputeRootSignature(reinterpret_cast<ID3D12RootSignature *>(b.compute.signature));
    for (unsigned i = 0; i < b.compute.arguments.size(); ++i) {
        const auto &a = b.compute.arguments[i];
        switch (a.kind) {
        case ArgumentKind::table: list->SetComputeRootDescriptorTable(i, {a.address}); break;
        case ArgumentKind::cbv: list->SetComputeRootConstantBufferView(i, a.address); break;
        case ArgumentKind::srv: list->SetComputeRootShaderResourceView(i, a.address); break;
        case ArgumentKind::uav: list->SetComputeRootUnorderedAccessView(i, a.address); break;
        case ArgumentKind::constants:
            for (unsigned n = 0; n < 64; ++n)
                if (a.mask & (std::uint64_t(1) << n)) list->SetComputeRoot32BitConstant(i, a.values[n], n);
            break;
        case ArgumentKind::unset: break;
        }
    }
    if (b.pipeline) list->SetPipelineState(reinterpret_cast<ID3D12PipelineState *>(b.pipeline));
}
} // namespace
bool FullPostInputs::prepare(ID3D12Device *device, unsigned width, unsigned height, std::string &reason) noexcept {
    root_.Reset();
    pipeline_.Reset();
    region_.Reset();
    expanded_.Reset();
    kept_.Reset();
    width_ = height_ = 0;
    bytes_ = 0;
    try {
        if (!device || !width || !height || width > 16384 || height > 16384) {
            reason = "full_post inputs extent";
            return false;
        }
        std::array<D3D12_ROOT_PARAMETER, 3> params{};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[0].Constants = {0, 0, 8};
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        params[1].Descriptor = {0, 0};
        params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        params[2].Descriptor = {0, 0};
        D3D12_ROOT_SIGNATURE_DESC rd{};
        rd.NumParameters = 3;
        rd.pParameters = params.data();
        ComPtr<ID3DBlob> blob, code, errors;
        if (FAILED(D3D12SerializeRootSignature(&rd, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors)) ||
            FAILED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                               IID_PPV_ARGS(&root_)))) {
            reason = "full_post inputs root";
            return false;
        }
        if (FAILED(D3DCompile(
                shader_source, sizeof(shader_source) - 1, "full_post_inputs", nullptr, nullptr, "main", "cs_5_0",
                D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                &code, &errors))) {
            reason = "full_post inputs shader";
            return false;
        }
        D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};
        pd.pRootSignature = root_.Get();
        pd.CS = {code->GetBufferPointer(), code->GetBufferSize()};
        ComPtr<ID3D12PipelineState> pipeline;
        if (FAILED(device->CreateComputePipelineState(&pd, IID_PPV_ARGS(&pipeline)))) {
            reason = "full_post inputs pipeline";
            return false;
        }
        // Buffers are created in COMMON and return to it after every use.
        const std::uint64_t size = std::uint64_t(pitch(width, 4)) * height;
        D3D12_RESOURCE_DESC d{};
        d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        d.Width = size;
        d.Height = d.DepthOrArraySize = d.MipLevels = 1;
        d.SampleDesc.Count = 1;
        d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COMMON,
                                                   nullptr, IID_PPV_ARGS(&region_))) ||
            FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COMMON,
                                                   nullptr, IID_PPV_ARGS(&kept_)))) {
            region_.Reset();
            reason = "full_post inputs region buffer";
            return false;
        }
        d.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COMMON,
                                                   nullptr, IID_PPV_ARGS(&expanded_)))) {
            region_.Reset();
            kept_.Reset();
            reason = "full_post inputs output buffer";
            return false;
        }
        region_->SetName(L"mhw-full-post-region");
        expanded_->SetName(L"mhw-full-post-expanded");
        kept_->SetName(L"mhw-full-post-depth");
        width_ = width;
        height_ = height;
        bytes_ = 3 * size;
        pipeline_ = std::move(pipeline);
        return true;
    } catch (...) {
        reason = "full_post inputs preparation";
    }
    root_.Reset();
    pipeline_.Reset();
    region_.Reset();
    expanded_.Reset();
    kept_.Reset();
    return false;
}
bool FullPostInputs::depth_input(ID3D12GraphicsCommandList *list, const FullPostInput &depth, unsigned rw,
                                 unsigned rh) const noexcept {
    if (!pipeline_ || !list || list->GetType() != D3D12_COMMAND_LIST_TYPE_DIRECT || !depth.texture || !rw || !rh ||
        rw > width_ || rh > height_)
        return false;
    ComPtr<ID3D12Device> device;
    if (FAILED(list->GetDevice(IID_PPV_ARGS(&device))) || !same_device(kept_.Get(), device.Get()) ||
        !same_device(depth.texture, device.Get()))
        return false;
    const auto d = depth.texture->GetDesc();
    return d.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && d.Width == width_ && d.Height == height_ &&
           d.DepthOrArraySize == 1 && d.MipLevels == 1 && d.SampleDesc.Count == 1 && !d.SampleDesc.Quality &&
           d.Format == full_post_input_format(FullPostInputRole::depth);
}
bool FullPostInputs::keep_depth(ID3D12GraphicsCommandList *list, const FullPostInput &depth, unsigned rw,
                                unsigned rh) noexcept {
    if (!depth_input(list, depth, rw, rh)) return false;
    auto *t = depth.texture, *kept = kept_.Get();
    Batch batch;
    D3D12_TEXTURE_COPY_LOCATION texture{}, in{};
    texture.pResource = t;
    texture.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    in.pResource = kept;
    in.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    in.PlacedFootprint.Footprint = {DXGI_FORMAT_R32_FLOAT, rw, rh, 1, pitch(rw, 4)};
    batch.add(t, depth.state, D3D12_RESOURCE_STATE_COPY_SOURCE);
    batch.add(kept, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    batch.issue(list);
    const D3D12_BOX box{0, 0, 0, rw, rh, 1};
    list->CopyTextureRegion(&in, 0, 0, 0, &texture, &box);
    batch.add(t, D3D12_RESOURCE_STATE_COPY_SOURCE, depth.state);
    batch.add(kept, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
    batch.issue(list);
    return true;
}
bool FullPostInputs::restore_depth(ID3D12GraphicsCommandList *list, const FullPostInput &depth, unsigned rw,
                                   unsigned rh) noexcept {
    if (!depth_input(list, depth, rw, rh)) return false;
    auto *t = depth.texture, *kept = kept_.Get();
    Batch batch;
    D3D12_TEXTURE_COPY_LOCATION texture{}, out{};
    texture.pResource = t;
    texture.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    out.pResource = kept;
    out.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    out.PlacedFootprint.Footprint = {DXGI_FORMAT_R32_FLOAT, rw, rh, 1, pitch(rw, 4)};
    batch.add(t, depth.state, D3D12_RESOURCE_STATE_COPY_DEST);
    batch.add(kept, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE);
    batch.issue(list);
    list->CopyTextureRegion(&texture, 0, 0, 0, &out, nullptr);
    batch.add(t, D3D12_RESOURCE_STATE_COPY_DEST, depth.state);
    batch.add(kept, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
    batch.issue(list);
    return true;
}
bool FullPostInputs::record(ID3D12GraphicsCommandList *list, const std::array<FullPostInput, 3> &inputs, unsigned rw,
                            unsigned rh, const temporal_present::Bindings &roots) noexcept {
    if (!pipeline_ || !list || list->GetType() != D3D12_COMMAND_LIST_TYPE_DIRECT || !roots.known || !roots.valid ||
        !roots.generation || !rw || !rh || rw > width_ || rh > height_)
        return false;
    ComPtr<ID3D12Device> device;
    if (FAILED(list->GetDevice(IID_PPV_ARGS(&device))) || !same_device(region_.Get(), device.Get())) return false;
    for (unsigned i = 0; i < 3; ++i) {
        auto *t = inputs[i].texture;
        if (!t || !same_device(t, device.Get())) return false;
        const auto d = t->GetDesc();
        if (d.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || d.Width != width_ || d.Height != height_ ||
            d.DepthOrArraySize != 1 || d.MipLevels != 1 || d.SampleDesc.Count != 1 || d.SampleDesc.Quality ||
            d.Format != full_post_input_format(static_cast<FullPostInputRole>(i)))
            return false;
        for (unsigned j = 0; j < i; ++j)
            if (inputs[j].texture == t) return false;
    }
    auto *expanded = expanded_.Get();
    Batch batch;
    for (unsigned i = 0; i < 3; ++i) {
        // The depth's region goes to the kept buffer (restore_depth()).
        auto *region = i ? region_.Get() : kept_.Get();
        auto *t = inputs[i].texture;
        const auto s = inputs[i].state;
        const auto role = static_cast<FullPostInputRole>(i);
        const auto format = full_post_input_format(role);
        const auto bpp = bytes_per_texel(role);
        D3D12_TEXTURE_COPY_LOCATION texture{}, in{}, out{};
        texture.pResource = t;
        texture.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        in.pResource = region;
        in.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        in.PlacedFootprint.Footprint = {format, rw, rh, 1, pitch(rw, bpp)};
        out.pResource = expanded;
        out.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        out.PlacedFootprint.Footprint = {format, width_, height_, 1, pitch(width_, bpp)};
        // The render region, into the first buffer.
        batch.add(t, s, D3D12_RESOURCE_STATE_COPY_SOURCE);
        batch.add(region, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        batch.issue(list);
        const D3D12_BOX box{0, 0, 0, rw, rh, 1};
        list->CopyTextureRegion(&in, 0, 0, 0, &texture, &box);
        // Expanded over the output.
        batch.add(t, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
        batch.add(region, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        batch.add(expanded, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        batch.issue(list);
        if (!i) {
            list->SetComputeRootSignature(root_.Get());
            list->SetPipelineState(pipeline_.Get());
        }
        const unsigned words = (width_ * bpp + 3) / 4;
        const std::array<std::uint32_t, 8> extent{rw,  rh,   width_, height_, pitch(rw, bpp), pitch(width_, bpp),
                                                  bpp, words};
        list->SetComputeRoot32BitConstants(0, 8, extent.data(), 0);
        list->SetComputeRootShaderResourceView(1, region->GetGPUVirtualAddress());
        list->SetComputeRootUnorderedAccessView(2, expanded->GetGPUVirtualAddress());
        list->Dispatch((words + 7) / 8, (height_ + 7) / 8, 1);
        // Over the texture.
        batch.add(expanded, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        batch.issue(list);
        list->CopyTextureRegion(&texture, 0, 0, 0, &out, nullptr);
        batch.add(t, D3D12_RESOURCE_STATE_COPY_DEST, s);
        batch.add(region, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
        batch.add(expanded, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
        batch.issue(list);
    }
    restore(list, roots);
    return true;
}
} // namespace mhw::scaled_temporal
