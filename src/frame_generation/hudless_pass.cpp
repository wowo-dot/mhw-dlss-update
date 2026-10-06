#include "hudless_pass.hpp"
#include "screen_transform.hpp"
#include <d3dcompiler.h>
#include <bit>
#include <stdexcept>
#include <string>
#include <utility>

namespace mhw::frame_generation {
namespace {
using Microsoft::WRL::ComPtr;
void require(bool v, const char *why) { if (!v) throw std::runtime_error(why); }
void hr(HRESULT v, const char *why) { require(SUCCEEDED(v), why); }
constexpr std::uint32_t probe_magic = 0x48554431;  // "HUD1"
constexpr unsigned probe_bytes = 16;

// Mirrors screen_transform.hpp. CBSystem layout from the game's reflection:
// iRegion@0, fSourceMipLevel@16, fGammaCorrect@20, fGammaCorrectEx@24.
// fGammaCorrect (1/2.2 in every capture) doubles as a layout check.
constexpr char shader[] = R"(
cbuffer CBSystem : register(b0) { uint4 iRegion; float fSourceMipLevel; float fGammaCorrect; float2 fGammaCorrectEx; };
Texture2D<float4> scene : register(t0);
RWTexture2D<unorm float4> hudless : register(u0);
RWByteAddressBuffer probe : register(u1);
float encode(float c) { return c <= 0.0031308 ? 12.92 * c : 1.055 * pow(c, 1.0 / 2.4) - 0.055; }
[numthreads(16,16,1)] void main(uint3 id : SV_DispatchThreadID) {
    float g = fGammaCorrectEx.x;
    bool ok = g >= GAMMA_MIN && g <= GAMMA_MAX && abs(fGammaCorrect - 0.45454545) < 0.001;
    if (!ok) g = 1.0;
    if (id.x == 0 && id.y == 0)
        probe.Store4(0, uint4(asuint(fGammaCorrectEx.x), asuint(fGammaCorrect), ok ? 1u : 0u, PROBE_MAGIC));
    uint w, h; hudless.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;
    float3 c = saturate(scene.Load(int3(id.xy, 0)).rgb);
    float k = min(0.05 * g + 0.95, 1.0);
    float3 b = 1.0 - pow(max(1.0 - k * c, 0.0), g);
    hudless[id.xy] = float4(encode(b.r), encode(b.g), encode(b.b), 1.0);
}
)";
}  // namespace

HudlessPass::HudlessPass(ID3D12Device *device, unsigned width, unsigned height)
    : device_(device), width_(width), height_(height) {
    require(device && width && height && width <= 16384 && height <= 16384, "hudless extent");
    D3D12_FEATURE_DATA_FORMAT_SUPPORT support{format};
    hr(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)), "hudless format query");
    require((support.Support1 & D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) &&
                (support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE), "hudless typed store");

    D3D12_DESCRIPTOR_RANGE ranges[]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0},
                                    {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 1}};
    D3D12_ROOT_PARAMETER params[3]{};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].Descriptor = {0, 0};
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable = {2, ranges};
    params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    params[2].Descriptor = {1, 0};
    D3D12_ROOT_SIGNATURE_DESC rd{3, params};
    ComPtr<ID3DBlob> signature, code, errors;
    hr(D3D12SerializeRootSignature(&rd, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors), "hudless root serialization");
    hr(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_)),
       "hudless root");
    const auto min = std::to_string(screen::gamma_min), max = std::to_string(screen::gamma_max),
               magic = std::to_string(probe_magic) + "u";
    const D3D_SHADER_MACRO macros[]{{"GAMMA_MIN", min.c_str()}, {"GAMMA_MAX", max.c_str()},
                                    {"PROBE_MAGIC", magic.c_str()}, {nullptr, nullptr}};
    const auto compiled = D3DCompile(shader, sizeof(shader) - 1, "mhw_fg_hudless", macros, nullptr, "main", "cs_5_0",
                                     D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS |
                                         D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
    if (FAILED(compiled))
        throw std::runtime_error(errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize())
                                        : "hudless shader compile");
    D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};
    pd.pRootSignature = root_.Get();
    pd.CS = {code->GetBufferPointer(), code->GetBufferSize()};
    hr(device->CreateComputePipelineState(&pd, IID_PPV_ARGS(&pipeline_)), "hudless pipeline");

    D3D12_DESCRIPTOR_HEAP_DESC hd{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2 * slots, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
    hr(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap_)), "hudless descriptors");
    heap_->SetName(L"mhw-fg-hudless-descriptors");
    step_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = probe_bytes * slots;
    buffer.Height = buffer.DepthOrArraySize = buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    buffer.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    const D3D12_HEAP_PROPERTIES default_heap{D3D12_HEAP_TYPE_DEFAULT}, readback_heap{D3D12_HEAP_TYPE_READBACK};
    hr(device->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr,
                                       IID_PPV_ARGS(&probe_)), "hudless probe");
    buffer.Flags = D3D12_RESOURCE_FLAG_NONE;
    hr(device->CreateCommittedResource(&readback_heap, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                       IID_PPV_ARGS(&readback_)), "hudless probe readback");
    probe_->SetName(L"mhw-fg-hudless-probe");
    readback_->SetName(L"mhw-fg-hudless-probe-readback");
    void *mapped{};
    hr(readback_->Map(0, nullptr, &mapped), "hudless probe map");
    mapped_ = static_cast<const std::uint32_t *>(mapped);
}

void HudlessPass::record(ID3D12GraphicsCommandList *list, unsigned slot, ID3D12Resource *scene, ID3D12Resource *target,
                         D3D12_RESOURCE_STATES target_state, D3D12_GPU_VIRTUAL_ADDRESS system_constants) {
    require(list && scene && target && slot < slots && system_constants, "hudless arguments");
    const auto s = scene->GetDesc(), t = target->GetDesc();
    require(s.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && s.Width == width_ && s.Height == height_ &&
                s.Format == DXGI_FORMAT_R11G11B10_FLOAT && !(s.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE),
            "hudless scene layout");
    require(t.Width == width_ && t.Height == height_ && t.Format == format && (t.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
            "hudless target layout");
    // The slot is free (fence-gated), so its descriptors are not in flight.
    D3D12_CPU_DESCRIPTOR_HANDLE cpu = heap_->GetCPUDescriptorHandleForHeapStart();
    cpu.ptr += SIZE_T(2) * slot * step_;
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = s.Format;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels = 1;
    device_->CreateShaderResourceView(scene, &srv, cpu);
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
    uav.Format = format;
    uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device_->CreateUnorderedAccessView(target, nullptr, &uav, {cpu.ptr + step_});
    D3D12_GPU_DESCRIPTOR_HANDLE gpu = heap_->GetGPUDescriptorHandleForHeapStart();
    gpu.ptr += UINT64(2) * slot * step_;

    D3D12_RESOURCE_BARRIER before[3]{};
    before[0].Transition = {scene, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_RENDER_TARGET,
                            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};
    before[1].Transition = {target, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, target_state, D3D12_RESOURCE_STATE_UNORDERED_ACCESS};
    before[2].Transition = {probe_.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COPY_SOURCE,
                            D3D12_RESOURCE_STATE_UNORDERED_ACCESS};
    list->ResourceBarrier(3, before);
    ID3D12DescriptorHeap *heaps[]{heap_.Get()};
    list->SetDescriptorHeaps(1, heaps);
    list->SetComputeRootSignature(root_.Get());
    list->SetPipelineState(pipeline_.Get());
    list->SetComputeRootConstantBufferView(0, system_constants);
    list->SetComputeRootDescriptorTable(1, gpu);
    list->SetComputeRootUnorderedAccessView(2, probe_->GetGPUVirtualAddress() + UINT64(probe_bytes) * slot);
    list->Dispatch((width_ + 15) / 16, (height_ + 15) / 16, 1);
    D3D12_RESOURCE_BARRIER after[3]{before[0], before[1], before[2]};
    for (auto &b : after) std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
    list->ResourceBarrier(3, after);
    list->CopyBufferRegion(readback_.Get(), UINT64(probe_bytes) * slot, probe_.Get(), UINT64(probe_bytes) * slot, probe_bytes);
    written_[slot] = true;
}

HudlessPass::Probe HudlessPass::probe(unsigned slot) const noexcept {
    if (slot >= slots || !written_[slot] || !mapped_) return {};
    const auto *v = mapped_ + slot * (probe_bytes / 4);
    if (v[3] != probe_magic) return {};
    return {std::bit_cast<float>(v[0]), std::bit_cast<float>(v[1]), v[2] == 1, true};
}

}  // namespace mhw::frame_generation
