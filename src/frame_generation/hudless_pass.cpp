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
void require(bool v, const char *why) {
    if (!v) throw std::runtime_error(why);
}
void hr(HRESULT v, const char *why) {
    require(SUCCEEDED(v), why);
}
constexpr std::uint32_t probe_magic = 0x48554431;  // "HUD1"
constexpr std::uint32_t screen_magic = 0x53435231; // "SCR1"
constexpr std::uint32_t hdr_magic = 0x48445231;    // "HDR1"
constexpr unsigned probe_bytes = 96, state_bytes = 96;

// State buffer: [0] g, [4] fGammaCorrect, [8] 1, [12] SCREEN_MAGIC (last valid
// capture); [16] attempts, [20] matches, [24..31] raw values of the last attempt;
// [32..47] iRegion of the last valid capture (where the screen write places the
// scene; logged for the aspect-ratio option, not used by the transform).
// HDR (v0.24.8): [48..63] white, ratio, gamma, GUI flag of the last valid
// CBRenderFrame; [64] attempts, [68] matches, [72] HDR_MAGIC, [76] the root
// parameter it was bound at; [80..95] raw values of the last attempt.
//
// Probe per slot: [0..47] as the state's first 48 bytes ([8] 1 when the SDR
// transform used a valid capture); [48..63] the HDR parameters the transform
// used; [64] HDR attempts, [68] matches, [72] parameter, [76] flags (1 the HDR
// transform ran, 2 with a valid capture); [80..95] the raw last HDR attempt.
//
// Capture. CBSystem layout from the game's reflection: iRegion@0,
// fSourceMipLevel@16, fGammaCorrect@20, fGammaCorrectEx@24. fGammaCorrect
// (1/2.2 in every capture) doubles as the layout check.
constexpr char capture_shader[] = R"(
cbuffer CBSystem : register(b0) { uint4 iRegion; float fSourceMipLevel; float fGammaCorrect; float2 fGammaCorrectEx; };
RWByteAddressBuffer state : register(u0);
[numthreads(1,1,1)] void main() {
    float g = fGammaCorrectEx.x;
    bool ok = g >= GAMMA_MIN && g <= GAMMA_MAX && abs(fGammaCorrect - 0.45454545) < 0.001;
    state.InterlockedAdd(16, 1u);
    state.Store2(24, uint2(asuint(fGammaCorrect), asuint(g)));
    if (ok) {
        state.Store4(0, uint4(asuint(g), asuint(fGammaCorrect), 1u, SCREEN_MAGIC));
        state.Store4(32, iRegion);
        state.InterlockedAdd(20, 1u);
    }
}
)";

// HDR capture. CBRenderFrame layout from the PQ screen write's reflection; the
// HDR fields plus the frame's booleans and delta time are the layout check, so
// a candidate CBV that is not CBRenderFrame (or SDR output) is not kept.
constexpr char capture_hdr_shader[] = R"(
cbuffer CBRenderFrame : register(b0) {
    uint iRenderFrame; uint iTotalTime; uint iTotalTimeEx; float fFPS;
    float fDeltaTime; float fSSAOEffect; uint bSSSEnable; uint bIsRenderingWater;
    float fWaterDepthBias; uint iGpuMode; float2 fDitherSize;
    uint bHdrOutput; float fHdrOutputWhiteLevel; float fHdrOutputGamutMappingRatio; float fHdrOutputGamma;
    uint bIsGUIHdrGamma;
};
cbuffer Candidate : register(b1) { uint parameter; };
RWByteAddressBuffer state : register(u0);
[numthreads(1,1,1)] void main() {
    bool ok = bHdrOutput != 0u && bSSSEnable <= 1u && bIsRenderingWater <= 1u && bIsGUIHdrGamma <= 1u &&
              fHdrOutputWhiteLevel >= WHITE_MIN && fHdrOutputWhiteLevel <= WHITE_MAX &&
              fHdrOutputGamutMappingRatio >= 0.0 && fHdrOutputGamutMappingRatio <= 1.0 &&
              fHdrOutputGamma >= HDR_GAMMA_MIN && fHdrOutputGamma <= HDR_GAMMA_MAX &&
              fDeltaTime >= 0.0 && fDeltaTime <= 10.0;
    uint4 values = uint4(asuint(fHdrOutputWhiteLevel), asuint(fHdrOutputGamutMappingRatio), asuint(fHdrOutputGamma), bIsGUIHdrGamma);
    state.InterlockedAdd(64, 1u);
    state.Store4(80, values);
    if (ok) {
        state.Store4(48, values);
        state.Store2(72, uint2(HDR_MAGIC, parameter));
        state.InterlockedAdd(68, 1u);
    }
}
)";

// Transform; mirrors screen_transform.hpp. Without a valid capture: g = 1.
constexpr char transform_shader[] = R"(
Texture2D<float4> scene : register(t0);
RWTexture2D<unorm float4> hudless : register(u0);
RWByteAddressBuffer probe : register(u1);
RWByteAddressBuffer state : register(u2);
float encode(float c) { c = saturate(c); return c <= 0.0031308 ? 12.92 * c : 1.055 * pow(c, 1.0 / 2.4) - 0.055; }
[numthreads(16,16,1)] void main(uint3 id : SV_DispatchThreadID) {
    uint4 s = state.Load4(0);
    float g = asfloat(s.x);
    bool ok = s.w == SCREEN_MAGIC && s.z == 1u && g >= GAMMA_MIN && g <= GAMMA_MAX;
    if (!ok) g = 1.0;
    if (id.x == 0 && id.y == 0) {
        probe.Store4(0, uint4(s.x, s.y, ok ? 1u : 0u, PROBE_MAGIC));
        probe.Store4(16, state.Load4(16));
        probe.Store4(32, state.Load4(32));
        uint4 c = state.Load4(64);
        probe.Store4(48, uint4(0u, 0u, 0u, 0u));
        probe.Store4(64, uint4(c.x, c.y, c.w, 0u));
        probe.Store4(80, state.Load4(80));
    }
    uint w, h; hudless.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;
    float3 c = saturate(scene.Load(int3(id.xy, 0)).rgb);
    float k = min(0.05 * g + 0.95, 1.0);
    float3 b = 1.0 - pow(max(1.0 - k * c, 0.0), g);
    hudless[id.xy] = float4(encode(b.r), encode(b.g), encode(b.b), 1.0);
}
)";

// HDR transform; mirrors screen_transform.hpp's hdr::encode (the game's
// System_CopyToBackBufferPQSingle). Without a valid capture: the shader's
// CBRenderFrame defaults (100 nits, ratio 0, gamma 1, no GUI gamma).
constexpr char transform_hdr_shader[] = R"(
Texture2D<float4> scene : register(t0);
RWTexture2D<unorm float4> hudless : register(u0);
RWByteAddressBuffer probe : register(u1);
RWByteAddressBuffer state : register(u2);
float pq(float x) {
    float p = pow(saturate(x), 0.1593017578125);
    return min(pow((0.8359375 + 18.8515625 * p) / (1.0 + 18.6875 * p), 78.84375), 1.0);
}
[numthreads(16,16,1)] void main(uint3 id : SV_DispatchThreadID) {
    uint4 h = state.Load4(48), c = state.Load4(64);
    float white = asfloat(h.x), ratio = asfloat(h.y), gamma = asfloat(h.z);
    uint gui = h.w;
    bool ok = c.z == HDR_MAGIC && white >= WHITE_MIN && white <= WHITE_MAX && ratio >= 0.0 && ratio <= 1.0 &&
              gamma >= HDR_GAMMA_MIN && gamma <= HDR_GAMMA_MAX && gui <= 1u;
    if (!ok) { white = 100.0; ratio = 0.0; gamma = 1.0; gui = 0u; }
    if (id.x == 0 && id.y == 0) {
        uint4 s = state.Load4(0);
        probe.Store4(0, uint4(s.x, s.y, 0u, PROBE_MAGIC));
        probe.Store4(16, state.Load4(16));
        probe.Store4(32, state.Load4(32));
        probe.Store4(48, uint4(asuint(white), asuint(ratio), asuint(gamma), gui));
        probe.Store4(64, uint4(c.x, c.y, c.w, ok ? 3u : 1u));
        probe.Store4(80, state.Load4(80));
    }
    uint w, h2; hudless.GetDimensions(w, h2);
    if (id.x >= w || id.y >= h2) return;
    float3 c0 = max(scene.Load(int3(id.xy, 0)).rgb, 0.0);
    float3 b = float3(dot(float3(0.627404, 0.329282, 0.043314), c0), dot(float3(0.069097, 0.919540, 0.011361), c0),
                      dot(float3(0.016392, 0.088013, 0.895595), c0));
    float3 x = b + ratio * (c0 - b);
    if (gui != 0u) {
        float3 g = pow(max(x, 0.0), gamma);
        x = float3(x.r < 1.0 ? g.r : x.r, x.g < 1.0 ? g.g : x.g, x.b < 1.0 ? g.b : x.b);
    }
    float3 n = x / (10000.0 / white);
    hudless[id.xy] = float4(pq(n.r), pq(n.g), pq(n.b), 1.0);
}
)";

D3D12_RESOURCE_BARRIER transition(ID3D12Resource *r, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
    D3D12_RESOURCE_BARRIER b{};
    b.Transition = {r, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, from, to};
    return b;
}

ComPtr<ID3D12RootSignature> root_signature(ID3D12Device *device, const D3D12_ROOT_SIGNATURE_DESC &desc,
                                           const char *why) {
    ComPtr<ID3DBlob> signature, errors;
    hr(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors), why);
    ComPtr<ID3D12RootSignature> root;
    hr(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root)),
       why);
    return root;
}

ComPtr<ID3D12PipelineState> compute_pipeline(ID3D12Device *device, ID3D12RootSignature *root, ID3DBlob *code,
                                             const char *why) {
    D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};
    pd.pRootSignature = root;
    pd.CS = {code->GetBufferPointer(), code->GetBufferSize()};
    ComPtr<ID3D12PipelineState> pipeline;
    hr(device->CreateComputePipelineState(&pd, IID_PPV_ARGS(&pipeline)), why);
    return pipeline;
}
} // namespace

ComPtr<ID3DBlob> HudlessPass::compile(Shader which) {
    const auto min = std::to_string(screen::gamma_min), max = std::to_string(screen::gamma_max),
               magic = std::to_string(probe_magic) + "u", screen = std::to_string(screen_magic) + "u",
               hdr = std::to_string(hdr_magic) + "u", white_min = std::to_string(screen::hdr::white_min),
               white_max = std::to_string(screen::hdr::white_max),
               hdr_gamma_min = std::to_string(screen::hdr::gamma_min),
               hdr_gamma_max = std::to_string(screen::hdr::gamma_max);
    const D3D_SHADER_MACRO macros[]{{"GAMMA_MIN", min.c_str()},
                                    {"GAMMA_MAX", max.c_str()},
                                    {"PROBE_MAGIC", magic.c_str()},
                                    {"SCREEN_MAGIC", screen.c_str()},
                                    {"HDR_MAGIC", hdr.c_str()},
                                    {"WHITE_MIN", white_min.c_str()},
                                    {"WHITE_MAX", white_max.c_str()},
                                    {"HDR_GAMMA_MIN", hdr_gamma_min.c_str()},
                                    {"HDR_GAMMA_MAX", hdr_gamma_max.c_str()},
                                    {nullptr, nullptr}};
    const char *source{}, *name{};
    std::size_t bytes{};
    switch (which) {
    case Shader::transform:
        source = transform_shader;
        bytes = sizeof(transform_shader) - 1;
        name = "mhw_fg_hudless";
        break;
    case Shader::capture:
        source = capture_shader;
        bytes = sizeof(capture_shader) - 1;
        name = "mhw_fg_screen_capture";
        break;
    case Shader::transform_hdr:
        source = transform_hdr_shader;
        bytes = sizeof(transform_hdr_shader) - 1;
        name = "mhw_fg_hudless_hdr";
        break;
    case Shader::capture_hdr:
        source = capture_hdr_shader;
        bytes = sizeof(capture_hdr_shader) - 1;
        name = "mhw_fg_frame_capture";
        break;
    }
    ComPtr<ID3DBlob> code, errors;
    const auto compiled =
        D3DCompile(source, bytes, name, macros, nullptr, "main", "cs_5_0",
                   D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                   &code, &errors);
    if (FAILED(compiled))
        throw std::runtime_error(
            errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize())
                   : "hudless shader compile");
    return code;
}

HudlessPass::HudlessPass(ID3D12Device *device, unsigned width, unsigned height)
    : device_(device), width_(width), height_(height) {
    require(device && width && height && width <= 16384 && height <= 16384, "hudless extent");
    const auto typed_store = [&](DXGI_FORMAT f) {
        D3D12_FEATURE_DATA_FORMAT_SUPPORT support{f};
        hr(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)),
           "hudless format query");
        return (support.Support1 & D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) &&
               (support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
    };
    require(typed_store(format), "hudless typed store");
    hdr_supported_ = typed_store(hdr_format);

    D3D12_DESCRIPTOR_RANGE ranges[]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0},
                                    {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 1}};
    D3D12_ROOT_PARAMETER params[3]{};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[0].DescriptorTable = {2, ranges};
    params[1].ParameterType = params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    params[1].Descriptor = {1, 0};
    params[2].Descriptor = {2, 0};
    root_ = root_signature(device, {3, params}, "hudless root");
    D3D12_ROOT_PARAMETER capture_params[2]{};
    capture_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    capture_params[0].Descriptor = {0, 0};
    capture_params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    capture_params[1].Descriptor = {0, 0};
    capture_root_ = root_signature(device, {2, capture_params}, "screen capture root");
    D3D12_ROOT_PARAMETER capture_hdr_params[3]{capture_params[0], capture_params[1], {}};
    capture_hdr_params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    capture_hdr_params[2].Constants = {1, 0, 1};
    capture_hdr_root_ = root_signature(device, {3, capture_hdr_params}, "frame capture root");
    pipeline_ = compute_pipeline(device, root_.Get(), compile(Shader::transform).Get(), "hudless pipeline");
    hdr_pipeline_ = compute_pipeline(device, root_.Get(), compile(Shader::transform_hdr).Get(), "hudless HDR pipeline");
    capture_pipeline_ =
        compute_pipeline(device, capture_root_.Get(), compile(Shader::capture).Get(), "screen capture pipeline");
    capture_hdr_pipeline_ =
        compute_pipeline(device, capture_hdr_root_.Get(), compile(Shader::capture_hdr).Get(), "frame capture pipeline");

    D3D12_DESCRIPTOR_HEAP_DESC hd{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2 * slots,
                                  D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
    hr(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap_)), "hudless descriptors");
    heap_->SetName(L"mhw-fg-hudless-descriptors");
    step_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Buffers start in COMMON (zeroed: no valid capture); each record returns
    // the state and probe buffers to COMMON.
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Height = buffer.DepthOrArraySize = buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    buffer.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    const D3D12_HEAP_PROPERTIES default_heap{D3D12_HEAP_TYPE_DEFAULT}, readback_heap{D3D12_HEAP_TYPE_READBACK};
    buffer.Width = state_bytes;
    hr(device->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_COMMON,
                                       nullptr, IID_PPV_ARGS(&state_)),
       "screen state");
    buffer.Width = probe_bytes * slots;
    hr(device->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_COMMON,
                                       nullptr, IID_PPV_ARGS(&probe_)),
       "hudless probe");
    buffer.Flags = D3D12_RESOURCE_FLAG_NONE;
    hr(device->CreateCommittedResource(&readback_heap, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_COPY_DEST,
                                       nullptr, IID_PPV_ARGS(&readback_)),
       "hudless probe readback");
    state_->SetName(L"mhw-fg-screen-state");
    probe_->SetName(L"mhw-fg-hudless-probe");
    readback_->SetName(L"mhw-fg-hudless-probe-readback");
    void *mapped{};
    hr(readback_->Map(0, nullptr, &mapped), "hudless probe map");
    mapped_ = static_cast<const std::uint32_t *>(mapped);
}

void HudlessPass::record(ID3D12GraphicsCommandList *list, unsigned slot, ID3D12Resource *scene, ID3D12Resource *target,
                         D3D12_RESOURCE_STATES target_state, D3D12_RESOURCE_STATES scene_state) {
    require(list && scene && target && slot < slots, "hudless arguments");
    const auto s = scene->GetDesc(), t = target->GetDesc();
    require(s.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && s.Width == width_ && s.Height == height_ &&
                s.Format == DXGI_FORMAT_R11G11B10_FLOAT && !(s.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE),
            "hudless scene layout");
    const bool hdr = t.Format == hdr_format;
    require(t.Width == width_ && t.Height == height_ && (t.Format == format || (hdr && hdr_supported_)) &&
                (t.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
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
    uav.Format = t.Format;
    uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device_->CreateUnorderedAccessView(target, nullptr, &uav, {cpu.ptr + step_});
    D3D12_GPU_DESCRIPTOR_HANDLE gpu = heap_->GetGPUDescriptorHandleForHeapStart();
    gpu.ptr += UINT64(2) * slot * step_;

    D3D12_RESOURCE_BARRIER before[4]{
        transition(scene, scene_state, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
        transition(target, target_state, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        transition(probe_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        transition(state_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)};
    list->ResourceBarrier(4, before);
    ID3D12DescriptorHeap *heaps[]{heap_.Get()};
    list->SetDescriptorHeaps(1, heaps);
    list->SetComputeRootSignature(root_.Get());
    list->SetPipelineState(hdr ? hdr_pipeline_.Get() : pipeline_.Get());
    list->SetComputeRootDescriptorTable(0, gpu);
    list->SetComputeRootUnorderedAccessView(1, probe_->GetGPUVirtualAddress() + UINT64(probe_bytes) * slot);
    list->SetComputeRootUnorderedAccessView(2, state_->GetGPUVirtualAddress());
    list->Dispatch((width_ + 15) / 16, (height_ + 15) / 16, 1);
    D3D12_RESOURCE_BARRIER after[4]{before[0], before[1], before[2], before[3]};
    for (auto &b : after) std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
    after[2].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    list->ResourceBarrier(4, after);
    list->CopyBufferRegion(readback_.Get(), UINT64(probe_bytes) * slot, probe_.Get(), UINT64(probe_bytes) * slot,
                           probe_bytes);
    const auto rest = transition(probe_.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
    list->ResourceBarrier(1, &rest);
    written_[slot] = true;
}

void HudlessPass::capture(ID3D12GraphicsCommandList *list, D3D12_GPU_VIRTUAL_ADDRESS system_constants) {
    require(list && system_constants, "screen capture arguments");
    auto b = transition(state_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    list->ResourceBarrier(1, &b);
    list->SetComputeRootSignature(capture_root_.Get());
    list->SetPipelineState(capture_pipeline_.Get());
    list->SetComputeRootConstantBufferView(0, system_constants);
    list->SetComputeRootUnorderedAccessView(1, state_->GetGPUVirtualAddress());
    list->Dispatch(1, 1, 1);
    std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
    list->ResourceBarrier(1, &b);
}

void HudlessPass::capture_hdr(ID3D12GraphicsCommandList *list, D3D12_GPU_VIRTUAL_ADDRESS frame_constants,
                              unsigned parameter) {
    require(list && frame_constants, "frame capture arguments");
    auto b = transition(state_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    list->ResourceBarrier(1, &b);
    list->SetComputeRootSignature(capture_hdr_root_.Get());
    list->SetPipelineState(capture_hdr_pipeline_.Get());
    list->SetComputeRootConstantBufferView(0, frame_constants);
    list->SetComputeRootUnorderedAccessView(1, state_->GetGPUVirtualAddress());
    list->SetComputeRoot32BitConstant(2, parameter, 0);
    list->Dispatch(1, 1, 1);
    std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
    list->ResourceBarrier(1, &b);
}

HudlessPass::Probe HudlessPass::probe(unsigned slot) const noexcept {
    if (slot >= slots || !written_[slot] || !mapped_) return {};
    const auto *v = mapped_ + slot * (probe_bytes / 4);
    if (v[3] != probe_magic) return {};
    Probe p{
        std::bit_cast<float>(v[0]), std::bit_cast<float>(v[1]), v[2] == 1, true, v[4], v[5], std::bit_cast<float>(v[6]),
        std::bit_cast<float>(v[7]), {v[8], v[9], v[10], v[11]}};
    p.hdr = (v[19] & 1u) != 0;
    p.hdr_ok = (v[19] & 2u) != 0;
    p.white = std::bit_cast<float>(v[12]);
    p.ratio = std::bit_cast<float>(v[13]);
    p.hdr_gamma = std::bit_cast<float>(v[14]);
    p.gui_gamma = v[15] != 0;
    p.hdr_attempts = v[16];
    p.hdr_matches = v[17];
    p.hdr_parameter = v[18];
    p.last_white = std::bit_cast<float>(v[20]);
    p.last_ratio = std::bit_cast<float>(v[21]);
    p.last_hdr_gamma = std::bit_cast<float>(v[22]);
    p.last_gui = v[23];
    return p;
}

} // namespace mhw::frame_generation
