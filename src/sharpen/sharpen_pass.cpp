#include "sharpen_pass.hpp"
#include "../common/require.hpp"
#include <d3dcompiler.h>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

namespace mhw::sharpen {
namespace {
using Microsoft::WRL::ComPtr;

// NIS_Scaler.h, embedded at build time (cmake/EmbedText.cmake).
constexpr unsigned char nis_scaler[] = {
#include "nis_scaler.inc"
};

// NIS's NVSharpen entry (its NIS_Main.hlsl) with the colour encoding around
// it. NVSharpen loads its tile with NVTEX_SAMPLE at texel centres only, so the
// encode after the sample is the texel's; NIS_CLAMP_OUTPUT saturates the
// encoded result.
// - SDR: IEC 61966-2-1 encode of saturate(T) (LDR mode) and its decode.
// - HDR10 (NIS_HDR_MODE 2): the game's BT.709 -> BT.2020 rotation (its PQ
//   screen write), PQ of the result x white / 10000, and the inverses.
// The store adds NVSharpen's change, decoded, to the texel itself: where it
// changes nothing the texel is written back bit for bit (a decode of the
// encode is not exact on the GPU), and SDR values above 1 are kept.
constexpr char shader[] = R"(
#define NIS_HLSL 1
#define NIS_SCALER 0
#define NIS_CLAMP_OUTPUT 1
cbuffer cb : register(b0) {
    float kDetectRatio; float kDetectThres; float kMinContrastRatio; float kRatioNorm;
    float kContrastBoost; float kEps; float kSharpStartY; float kSharpScaleY;
    float kSharpStrengthMin; float kSharpStrengthScale; float kSharpLimitMin; float kSharpLimitScale;
    float kScaleX; float kScaleY; float kDstNormX; float kDstNormY;
    float kSrcNormX; float kSrcNormY;
    uint kInputViewportOriginX; uint kInputViewportOriginY; uint kInputViewportWidth; uint kInputViewportHeight;
    uint kOutputViewportOriginX; uint kOutputViewportOriginY; uint kOutputViewportWidth; uint kOutputViewportHeight;
    float kWhite; float kInvWhite; // NIS's reserved0/1: the HDR10 white level (nits) and its inverse
};
SamplerState samplerLinearClamp : register(s0);
Texture2D in_texture : register(t0);
RWTexture2D<float3> out_texture : register(u0);
#if NIS_HDR_MODE == 2
static const float m1 = 2610.0 / 16384.0, m2 = 2523.0 / 32.0, c1 = 3424.0 / 4096.0, c2 = 2413.0 / 128.0,
                   c3 = 2392.0 / 128.0;
static const float3x3 to_2020 = {0.627404, 0.329282, 0.043314, 0.069097, 0.919540, 0.011361,
                                 0.016392, 0.088013, 0.895595};
static const float3x3 from_2020 = {1.660490360, -0.587639061, -0.072852475, -0.124549969, 1.132899890, -0.008347655,
                                   -0.018151890, -0.100578206, 1.118729895};
float4 mhw_encode(float4 t) {
    float3 p = pow(saturate(mul(to_2020, max(t.rgb, 0.0)) * (kWhite * 0.0001)), m1);
    return float4(pow((c1 + c2 * p) / (1.0 + c3 * p), m2), 1.0);
}
float3 mhw_decode(float4 e) {
    float3 p = pow(saturate(e.rgb), 1.0 / m2);
    float3 n = pow(max(p - c1, 0.0) / (c2 - c3 * p), 1.0 / m1);
    return max(mul(from_2020, n * (10000.0 * kInvWhite)), 0.0);
}
#else
float4 mhw_encode(float4 t) {
    float3 c = saturate(t.rgb);
    return float4(c <= 0.0031308 ? 12.92 * c : 1.055 * pow(c, 1.0 / 2.4) - 0.055, 1.0);
}
float3 mhw_decode(float4 e) {
    float3 c = saturate(e.rgb);
    return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}
#endif
float3 mhw_store(uint2 pos, float4 v) {
    float4 t = in_texture[pos];
    return max(t.rgb + (mhw_decode(v) - mhw_decode(mhw_encode(t))), 0.0);
}
#define NVTEX_SAMPLE(x, sampler, pos) mhw_encode(x.SampleLevel(sampler, pos, 0))
#define NVTEX_STORE(x, pos, v) x[uint2(pos)] = mhw_store(uint2(pos), v)
#include "NIS_Scaler.h"
[numthreads(NIS_THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 blockIdx : SV_GroupID, uint3 threadIdx : SV_GroupThreadID) {
    NVSharpen(blockIdx.xy, threadIdx.x);
}
)";

// Serves NIS_Scaler.h from the DLL to D3DCompile.
struct Include final : ID3DInclude {
    HRESULT __stdcall Open(D3D_INCLUDE_TYPE, LPCSTR name, LPCVOID, LPCVOID *data, UINT *bytes) override {
        if (!name || std::string_view(name) != "NIS_Scaler.h") return E_FAIL;
        *data = nis_scaler;
        *bytes = static_cast<UINT>(sizeof nis_scaler);
        return S_OK;
    }
    HRESULT __stdcall Close(LPCVOID) override { return S_OK; }
};

D3D12_RESOURCE_BARRIER transition(ID3D12Resource *r, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
    D3D12_RESOURCE_BARRIER b{};
    b.Transition = {r, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, from, to};
    return b;
}
} // namespace

ComPtr<ID3DBlob> SharpenPass::compile(bool hdr) {
    const auto width = std::to_string(block_width), height = std::to_string(block_height),
               group = std::to_string(thread_group);
    const D3D_SHADER_MACRO macros[]{{"NIS_HDR_MODE", hdr ? "2" : "0"},
                                    {"NIS_BLOCK_WIDTH", width.c_str()},
                                    {"NIS_BLOCK_HEIGHT", height.c_str()},
                                    {"NIS_THREAD_GROUP_SIZE", group.c_str()},
                                    {nullptr, nullptr}};
    Include include;
    ComPtr<ID3DBlob> code, errors;
    const auto compiled = D3DCompile(shader, sizeof(shader) - 1, hdr ? "mhw_nvsharpen_hdr" : "mhw_nvsharpen", macros,
                                     &include, "main", "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
    if (FAILED(compiled))
        throw std::runtime_error(
            errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize())
                   : "sharpen shader compile");
    return code;
}

SharpenPass::SharpenPass(ID3D12Device *device) : device_(device) {
    require(device, "sharpen device");
    D3D12_FEATURE_DATA_FORMAT_SUPPORT support{format};
    hr(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)), "sharpen format query");
    require((support.Support1 & D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) &&
                (support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE),
            "sharpen typed store");

    D3D12_DESCRIPTOR_RANGE ranges[]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0},
                                    {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 1}};
    D3D12_ROOT_PARAMETER params[2]{};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[0].Constants = {0, 0, constant_count};
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable = {2, ranges};
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    const D3D12_ROOT_SIGNATURE_DESC desc{2, params, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ComPtr<ID3DBlob> signature, errors;
    hr(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors), "sharpen root");
    hr(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_)),
       "sharpen root");
    for (unsigned hdr = 0; hdr < 2; ++hdr) {
        const auto code = compile(hdr != 0);
        D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};
        pd.pRootSignature = root_.Get();
        pd.CS = {code->GetBufferPointer(), code->GetBufferSize()};
        hr(device->CreateComputePipelineState(&pd, IID_PPV_ARGS(&pipelines_[hdr])), "sharpen pipeline");
    }
    D3D12_DESCRIPTOR_HEAP_DESC hd{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2 * cached_scenes,
                                  D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
    hr(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap_)), "sharpen descriptors");
    heap_->SetName(L"mhw-sharpen-descriptors");
    step_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

SharpenPass::Result SharpenPass::record(ID3D12GraphicsCommandList *list, ID3D12Resource *scene,
                                        D3D12_RESOURCE_STATES scene_state, const Constants &constants, bool hdr) {
    require(list && scene, "sharpen arguments");
    const auto s = scene->GetDesc();
    if (s.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || s.Format != format || s.MipLevels != 1 ||
        s.DepthOrArraySize != 1 || s.SampleDesc.Count != 1 || !s.Width || !s.Height || s.Width > 16384 ||
        s.Height > 16384 || (s.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) ||
        constants.nis.kInputViewportWidth != s.Width || constants.nis.kInputViewportHeight != s.Height)
        return Result::layout;
    // The scratch image follows the first scene after a reset; a scene of
    // another size waits for the next reset (a rebuild makes every one).
    if (!scratch_) {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = s.Width;
        desc.Height = s.Height;
        desc.DepthOrArraySize = desc.MipLevels = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        const D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_DEFAULT};
        hr(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                            nullptr, IID_PPV_ARGS(&scratch_)),
           "sharpen scratch");
        scratch_->SetName(L"mhw-sharpen-scratch");
    }
    const auto c = scratch_->GetDesc();
    if (c.Width != s.Width || c.Height != s.Height) return Result::scratch;
    unsigned slot = cached_scenes;
    for (unsigned i = 0; i < cached_scenes && slot == cached_scenes; ++i)
        if (scenes_[i].Get() == scene) slot = i;
    if (slot == cached_scenes) {
        // A new scene: descriptors in a slot no recorded work has used since
        // the last reset (slots are never reused before it).
        for (unsigned i = 0; i < cached_scenes && slot == cached_scenes; ++i)
            if (!scenes_[i]) slot = i;
        if (slot == cached_scenes) return Result::cache_full;
        D3D12_CPU_DESCRIPTOR_HANDLE cpu = heap_->GetCPUDescriptorHandleForHeapStart();
        cpu.ptr += SIZE_T(2) * slot * step_;
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Format = format;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Texture2D.MipLevels = 1;
        device_->CreateShaderResourceView(scene, &srv, cpu);
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
        uav.Format = format;
        uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device_->CreateUnorderedAccessView(scratch_.Get(), nullptr, &uav, {cpu.ptr + step_});
        scenes_[slot] = scene;
    }
    D3D12_GPU_DESCRIPTOR_HANDLE gpu = heap_->GetGPUDescriptorHandleForHeapStart();
    gpu.ptr += UINT64(2) * slot * step_;

    constexpr auto read = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    if (scene_state != read) {
        const auto in = transition(scene, scene_state, read);
        list->ResourceBarrier(1, &in);
    }
    ID3D12DescriptorHeap *heaps[]{heap_.Get()};
    list->SetDescriptorHeaps(1, heaps);
    list->SetComputeRootSignature(root_.Get());
    list->SetPipelineState(pipelines_[hdr ? 1 : 0].Get());
    list->SetComputeRoot32BitConstants(0, constant_count, &constants.nis, 0);
    list->SetComputeRootDescriptorTable(1, gpu);
    list->Dispatch(static_cast<UINT>((s.Width + block_width - 1) / block_width),
                   (s.Height + block_height - 1) / block_height, 1);
    D3D12_RESOURCE_BARRIER copy[2]{
        transition(scene, read, D3D12_RESOURCE_STATE_COPY_DEST),
        transition(scratch_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE)};
    list->ResourceBarrier(2, copy);
    list->CopyResource(scene, scratch_.Get());
    D3D12_RESOURCE_BARRIER after[2]{
        transition(scratch_.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        transition(scene, D3D12_RESOURCE_STATE_COPY_DEST, scene_state)};
    list->ResourceBarrier(scene_state == D3D12_RESOURCE_STATE_COPY_DEST ? 1U : 2U, after);
    return Result::recorded;
}

void SharpenPass::reset() noexcept {
    scratch_.Reset();
    for (auto &s : scenes_) s.Reset();
}

} // namespace mhw::sharpen
