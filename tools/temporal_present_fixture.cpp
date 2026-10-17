// GPU proof of output packing and binding restoration. Never opens the game.
#include "../src/temporal_present/output.hpp"
#include "../src/temporal_present/bindings.hpp"
#include "../src/temporal_present/observer.hpp"
#include "../tests/fake_astera.hpp"
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>

namespace {
using Microsoft::WRL::ComPtr;
namespace tp = mhw::temporal_present;
void require(bool v, const char *why) {
    if (!v) throw std::runtime_error(why);
}
void hr(HRESULT v, const char *why) {
    require(SUCCEEDED(v), why);
}
bool fixture_method(std::uintptr_t object, unsigned slot, std::uintptr_t &target) noexcept {
    target = (*reinterpret_cast<std::uintptr_t **>(object))[slot];
    return target != 0;
}
template <class F> void rejects(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "expected rejection");
}
ComPtr<ID3D12Resource> resource(ID3D12Device *d, const D3D12_RESOURCE_DESC &desc, D3D12_HEAP_TYPE type,
                                D3D12_RESOURCE_STATES state) {
    ComPtr<ID3D12Resource> r;
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = type;
    hr(d->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&r)), "resource");
    return r;
}
D3D12_RESOURCE_DESC texture(unsigned w, unsigned h, DXGI_FORMAT f) {
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width = w;
    d.Height = h;
    d.DepthOrArraySize = d.MipLevels = 1;
    d.SampleDesc.Count = 1;
    d.Format = f;
    d.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    return d;
}
ComPtr<ID3D12Resource> buffer(ID3D12Device *d, UINT64 bytes, D3D12_HEAP_TYPE heap) {
    D3D12_RESOURCE_DESC b{};
    b.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    b.Width = bytes;
    b.Height = 1;
    b.DepthOrArraySize = b.MipLevels = 1;
    b.SampleDesc.Count = 1;
    b.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return resource(d, b, heap,
                    heap == D3D12_HEAP_TYPE_UPLOAD ? D3D12_RESOURCE_STATE_GENERIC_READ
                                                   : D3D12_RESOURCE_STATE_COPY_DEST);
}
void transition(ID3D12GraphicsCommandList *l, ID3D12Resource *r, D3D12_RESOURCE_STATES a, D3D12_RESOURCE_STATES b) {
    D3D12_RESOURCE_BARRIER v{};
    v.Transition = {r, UINT_MAX, a, b};
    l->ResourceBarrier(1, &v);
}
ComPtr<ID3DBlob> compile(const char *s, const char *entry, const char *profile) {
    ComPtr<ID3DBlob> c, e;
    auto result = D3DCompile(
        s, std::strlen(s), "fixture", nullptr, nullptr, entry, profile,
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &c, &e);
    if (FAILED(result) && e)
        std::cerr.write(static_cast<const char *>(e->GetBufferPointer()),
                        static_cast<std::streamsize>(e->GetBufferSize()));
    hr(result, "shader");
    return c;
}
// Independent arithmetic quantizer, including nearest-even halfway cases.
unsigned round_even(double x) {
    const auto low = static_cast<unsigned>(std::floor(x));
    const auto fraction = x - low;
    return low + (fraction > .5 || (fraction == .5 && (low & 1)));
}
unsigned channel(float v, unsigned bits) {
    const double maximum = std::ldexp(2.0 - std::ldexp(1.0, -static_cast<int>(bits)), 15);
    const auto x = std::clamp(double(v), 0.0, maximum);
    if (x < std::ldexp(1., -14)) return round_even(std::ldexp(x, 14 + static_cast<int>(bits)));
    int exponent{};
    const auto fraction = std::frexp(x, &exponent);
    const auto mantissa = round_even(std::ldexp(fraction, bits + 1));
    return (static_cast<unsigned>(exponent + 14) << bits) + mantissa - (1U << bits);
}
unsigned packed(const float *v) {
    return channel(v[0], 6) | (channel(v[1], 6) << 11) | (channel(v[2], 5) << 22);
}
float unpack(unsigned v, unsigned bits) {
    const auto e = v >> bits, m = v & ((1U << bits) - 1);
    return e ? std::ldexp(float((1U << bits) + m), static_cast<int>(e) - 15 - static_cast<int>(bits))
             : std::ldexp(float(m), -14 - static_cast<int>(bits));
}
struct Readback {
    ComPtr<ID3D12Resource> value;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT64 bytes{}, row{};
    unsigned height{};
    Readback(ID3D12Device *d, const D3D12_RESOURCE_DESC &desc) : height(desc.Height) {
        d->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, &row, &bytes);
        value = buffer(d, bytes, D3D12_HEAP_TYPE_READBACK);
    }
    void copy(ID3D12GraphicsCommandList *l, ID3D12Resource *source) {
        D3D12_TEXTURE_COPY_LOCATION a{}, b{};
        a.pResource = source;
        a.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        b.pResource = value.Get();
        b.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        b.PlacedFootprint = footprint;
        l->CopyTextureRegion(&b, 0, 0, 0, &a, nullptr);
    }
    std::vector<std::byte> read() {
        void *p{};
        D3D12_RANGE range{0, static_cast<SIZE_T>(bytes)};
        hr(value->Map(0, &range, &p), "readback map");
        std::vector<std::byte> out(static_cast<std::size_t>(row) * height);
        for (unsigned y = 0; y < height; ++y)
            std::memcpy(out.data() + row * y, static_cast<std::byte *>(p) + SIZE_T(footprint.Footprint.RowPitch) * y,
                        static_cast<SIZE_T>(row));
        D3D12_RANGE none{};
        value->Unmap(0, &none);
        return out;
    }
};
void run(int argc, wchar_t **argv) {
    const std::wstring_view mode = argc == 2 ? argv[1] : L"";
    const bool spatial = mode == L"--spatial" || mode == L"--spatial-fractional" || mode == L"--spatial-identity";
    const bool warp = argc == 2 && (mode == L"--warp" || spatial);
    require(warp || argc == 8, "usage: --warp OR --hardware luid_low luid_high width height input.rgba32f output.json");
    require(warp || std::wstring_view(argv[1]) == L"--hardware", "fixture mode");
    const unsigned factor = mode == L"--spatial" ? 2U : mode == L"--spatial-fractional" ? 3U : 1U;
    const unsigned w = warp ? 19U * factor : static_cast<unsigned>(std::stoul(argv[4])),
                   h = warp ? 17U * factor : static_cast<unsigned>(std::stoul(argv[5]));
    const unsigned aw = mode == L"--spatial-fractional" ? 38U : 19U, ah = mode == L"--spatial-fractional" ? 34U : 17U;
    const unsigned sw = spatial ? w + 3 : w, sh = spatial ? h + 5 : h;
    require(w && h && w <= 8192 && h <= 8192 && UINT64(w) * h <= 4194304, "fixture extent");
    std::vector<float> input(std::size_t(sw) * sh * 4);
    if (warp) {
        const float probes[]{-1,    0,     1.e-8F,     0.000030517578125F, .125F, .25F,
                             .5F,   1,     1.0078125F, 1.0234375F,         2.5F,  44.5F,
                             64512, 65024, 1.e10F};
        for (std::size_t i = 0; i < input.size(); ++i) input[i] = probes[i % std::size(probes)];
    } else {
        require(!std::filesystem::exists(argv[7]), "preserve fixture result");
        require(std::filesystem::file_size(argv[6]) == input.size() * 4, "input size");
        std::ifstream f(argv[6], std::ios::binary);
        require(bool(f.read(reinterpret_cast<char *>(input.data()), static_cast<std::streamsize>(input.size() * 4))),
                "input read");
    }
    for (std::size_t i = 0; i < input.size(); i += 4)
        for (unsigned j = 0; j < 3; ++j) require(std::isfinite(input[i + j]), "finite source");
    std::vector<std::byte> source_data(std::size_t(sw) * sh * (spatial ? 4 : 16));
    if (spatial) {
        for (unsigned y = 0; y < sh; ++y)
            for (unsigned x = 0; x < sw; ++x) {
                const auto i = std::size_t(y) * sw + x;
                // NaNs in every padding channel expose a single out-of-region load.
                const unsigned pixel = x < aw && y < ah ? packed(input.data() + i * 4) : 0xffffffffU;
                std::memcpy(source_data.data() + i * 4, &pixel, 4);
                input[i * 4] = unpack(pixel & 2047, 6);
                input[i * 4 + 1] = unpack((pixel >> 11) & 2047, 6);
                input[i * 4 + 2] = unpack(pixel >> 22, 5);
            }
    } else
        std::memcpy(source_data.data(), input.data(), source_data.size());
    ComPtr<ID3D12Debug> debug;
    hr(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)), "debug");
    debug->EnableDebugLayer();
    ComPtr<ID3D12Debug1> validation;
    hr(debug.As(&validation), "GPU validation");
    validation->SetEnableGPUBasedValidation(TRUE);
    ComPtr<IDXGIFactory4> factory;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "factory");
    if (warp)
        hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)), "warp");
    else
        hr(factory->EnumAdapterByLuid({static_cast<DWORD>(std::stoul(argv[2])), static_cast<LONG>(std::stol(argv[3]))},
                                      IID_PPV_ARGS(&adapter)),
           "adapter");
    hr(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "device");
    ComPtr<ID3D12InfoQueue> messages;
    hr(device.As(&messages), "messages");
    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Fence> fence;
    hr(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue)), "queue");
    hr(device->CreateCommandAllocator(qd.Type, IID_PPV_ARGS(&allocator)), "allocator");
    hr(device->CreateCommandList(0, qd.Type, allocator.Get(), nullptr, IID_PPV_ARGS(&list)), "list");
    hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "fence");
    auto sd = texture(sw, sh, spatial ? DXGI_FORMAT_R11G11B10_FLOAT : DXGI_FORMAT_R32G32B32A32_FLOAT),
         nd = texture(w, h, DXGI_FORMAT_R11G11B10_FLOAT);
    auto source = resource(device.Get(), sd, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);
    auto native = resource(device.Get(), nd, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    const auto fd = texture(w, h, DXGI_FORMAT_R32G32B32A32_FLOAT);
    auto follow = resource(device.Get(), fd, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    auto gd = fd;
    gd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    auto graphics = resource(device.Get(), gd, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    Readback native_pixels(device.Get(), nd), follow_pixels(device.Get(), fd), graphics_pixels(device.Get(), gd),
        source_pixels(device.Get(), sd);
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT upload_layout{};
    UINT64 upload_bytes{};
    device->GetCopyableFootprints(&sd, 0, 1, 0, &upload_layout, nullptr, nullptr, &upload_bytes);
    auto upload = buffer(device.Get(), upload_bytes, D3D12_HEAP_TYPE_UPLOAD);
    void *mapped{};
    D3D12_RANGE none{};
    hr(upload->Map(0, &none, &mapped), "upload map");
    const auto row_bytes = std::size_t(sw) * (spatial ? 4 : 16);
    for (unsigned y = 0; y < sh; ++y)
        std::memcpy(static_cast<std::byte *>(mapped) + SIZE_T(upload_layout.Footprint.RowPitch) * y,
                    source_data.data() + row_bytes * y, row_bytes);
    upload->Unmap(0, nullptr);
    D3D12_TEXTURE_COPY_LOCATION from{}, to{};
    from.pResource = upload.Get();
    from.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    from.PlacedFootprint = upload_layout;
    to.pResource = source.Get();
    to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    list->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
    transition(list.Get(), source.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);

    // Simulated engine bindings: one SRV+UAV heap, one sampler heap, and partial
    // root-constant updates. Neither downstream pass rebuilds its root bindings.
    D3D12_DESCRIPTOR_HEAP_DESC hd{};
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    hd.NumDescriptors = 2;
    hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ComPtr<ID3D12DescriptorHeap> heap, sampler, rtv;
    hr(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap)), "native heap");
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    hd.NumDescriptors = 1;
    hr(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&sampler)), "sampler heap");
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtv)), "RTV heap");
    device->CreateRenderTargetView(graphics.Get(), nullptr, rtv->GetCPUDescriptorHandleForHeapStart());
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = nd.Format;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(native.Get(), &srv, heap->GetCPUDescriptorHandleForHeapStart());
    const auto stride = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
    uav.Format = fd.Format;
    uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device->CreateUnorderedAccessView(follow.Get(), nullptr, &uav,
                                      {heap->GetCPUDescriptorHandleForHeapStart().ptr + stride});
    D3D12_SAMPLER_DESC samp{};
    samp.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samp.AddressU = samp.AddressV = samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samp.MaxLOD = D3D12_FLOAT32_MAX;
    samp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    device->CreateSampler(&samp, sampler->GetCPUDescriptorHandleForHeapStart());
    std::array<D3D12_DESCRIPTOR_RANGE, 3> ranges{{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0},
                                                  {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 1},
                                                  {D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 0, 0}}};
    std::array<D3D12_ROOT_PARAMETER, 3> params{};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[0].Constants = {0, 0, 4};
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable = {2, ranges.data()};
    params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[2].DescriptorTable = {1, &ranges[2]};
    D3D12_ROOT_SIGNATURE_DESC rd{};
    rd.NumParameters = 3;
    rd.pParameters = params.data();
    ComPtr<ID3DBlob> serialized, errors;
    ComPtr<ID3D12RootSignature> root;
    hr(D3D12SerializeRootSignature(&rd, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors), "native root serialize");
    hr(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&root)),
       "native root");
    constexpr char code[] = R"(
cbuffer C : register(b0) { uint width; uint height; uint red; uint alpha; };
Texture2D<float3> src : register(t0); RWTexture2D<float4> outp : register(u0); SamplerState samp : register(s0);
[numthreads(16,16,1)] void cs(uint3 i:SV_DispatchThreadID) { if(i.x<width && i.y<height) outp[i.xy]=float4(src.Load(int3(i.xy,0))+float3(red,0,0),alpha); }
float4 vs(uint i:SV_VertexID):SV_Position { return float4(i==2?3:-1,i==1?-3:1,0,1); }
float4 ps(float4 p:SV_Position):SV_Target { return float4(src.SampleLevel(samp,p.xy/float2(width,height),0)+float3(red,0,0),alpha); }
)";
    auto cs = compile(code, "cs", "cs_5_0"), vs = compile(code, "vs", "vs_5_0"), ps = compile(code, "ps", "ps_5_0");
    D3D12_COMPUTE_PIPELINE_STATE_DESC cd{};
    cd.pRootSignature = root.Get();
    cd.CS = {cs->GetBufferPointer(), cs->GetBufferSize()};
    ComPtr<ID3D12PipelineState> pipeline, graphic_pipeline;
    hr(device->CreateComputePipelineState(&cd, IID_PPV_ARGS(&pipeline)), "native compute pipeline");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};
    pd.pRootSignature = root.Get();
    pd.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    pd.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    pd.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pd.DepthStencilState.DepthEnable = FALSE;
    pd.DepthStencilState.StencilEnable = FALSE;
    pd.SampleMask = UINT_MAX;
    pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.NumRenderTargets = 1;
    pd.RTVFormats[0] = gd.Format;
    pd.SampleDesc.Count = 1;
    hr(device->CreateGraphicsPipelineState(&pd, IID_PPV_ARGS(&graphic_pipeline)), "native graphics pipeline");
    // The loader installs the observer's registered detours; the observer
    // checks them, with every hook the DLL installs.
    mhw::test::fake_astera::start();
    std::vector<mhw::astera::Hook> hooks;
    tp::observer::astera_hooks(hooks);
    require(mhw::test::fake_astera::install_methods(hooks, {.command_list = list.Get()}),
            "loader installs the observer's detours");
    std::string hook_reason;
    require(tp::observer::install(device.Get(), fixture_method, hook_reason), hook_reason.c_str());
    tp::observer::begin();
    tp::observer::reset(list.Get(), 1, nullptr);
    ID3D12DescriptorHeap *heaps[]{heap.Get(), sampler.Get()};
    list->SetDescriptorHeaps(2, heaps);
    list->SetComputeRootSignature(root.Get());
    list->SetGraphicsRootSignature(root.Get());
    list->SetPipelineState(pipeline.Get());
    const unsigned constants[]{w, h, 7, 11}, replacement = 3;
    list->SetComputeRoot32BitConstants(0, 4, constants, 0);
    list->SetComputeRoot32BitConstant(0, replacement, 2);
    list->SetGraphicsRoot32BitConstants(0, 4, constants, 0);
    list->SetComputeRootDescriptorTable(1, heap->GetGPUDescriptorHandleForHeapStart());
    list->SetComputeRootDescriptorTable(2, sampler->GetGPUDescriptorHandleForHeapStart());
    list->SetGraphicsRootDescriptorTable(1, heap->GetGPUDescriptorHandleForHeapStart());
    list->SetGraphicsRootDescriptorTable(2, sampler->GetGPUDescriptorHandleForHeapStart());
    const D3D12_VIEWPORT viewport{0, 0, float(w), float(h), 0, 1};
    const D3D12_RECT scissor{0, 0, static_cast<LONG>(w), static_cast<LONG>(h)};
    list->RSSetViewports(1, &viewport);
    list->RSSetScissorRects(1, &scissor);
    auto rt = rtv->GetCPUDescriptorHandleForHeapStart();
    list->OMSetRenderTargets(1, &rt, FALSE, nullptr);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    tp::Bindings state;
    state.reset(1, reinterpret_cast<std::uintptr_t>(pipeline.Get()));
    state.root(false, reinterpret_cast<std::uintptr_t>(root.Get()));
    state.root(true, reinterpret_cast<std::uintptr_t>(root.Get()));
    const std::uintptr_t ids[]{reinterpret_cast<std::uintptr_t>(heap.Get()),
                               reinterpret_cast<std::uintptr_t>(sampler.Get())};
    state.descriptor_heaps(ids);
    for (bool g : {false, true}) {
        state.constants(g, 0, 0, constants);
        state.descriptor(g, 1, tp::ArgumentKind::table, heap->GetGPUDescriptorHandleForHeapStart().ptr);
        state.descriptor(g, 2, tp::ArgumentKind::table, sampler->GetGPUDescriptorHandleForHeapStart().ptr);
    }
    state.constants(false, 0, 2, std::span(&replacement, 1));
    tp::Bindings observed;
    require(tp::observer::current(list.Get(), observed) &&
                observed.compute.signature == reinterpret_cast<std::uintptr_t>(root.Get()) &&
                observed.pipeline == reinterpret_cast<std::uintptr_t>(pipeline.Get()),
            "observed native state");
    require(observed.compute.arguments[0].mask == state.compute.arguments[0].mask &&
                observed.compute.arguments[0].values == state.compute.arguments[0].values &&
                observed.graphics.signature == state.graphics.signature,
            "actual API hook values");
    require(tp::observer::finish(), "state observer shutdown");
    tp::RestoreBindings restore(list.Get(), observed);
    auto prepared = spatial ? std::make_unique<tp::Output>(device.Get(), tp::SpatialSource{sw, sh, aw, ah}, w, h)
                            : std::make_unique<tp::Output>(device.Get(), w, h);
    auto &output = *prepared;
    if (spatial) {
        rejects([&] { tp::Output invalid(device.Get(), tp::SpatialSource{sw, sh, sw + 1, ah}, w, h); });
        rejects([&] { tp::Output invalid(device.Get(), tp::SpatialSource{sw, sh, 0, ah}, w, h); });
        // Any single render scale in [0.5, 1] per axis is accepted (the DLSS
        // presets); axes that no one scale can produce are not.
        rejects([&] { tp::Output invalid(device.Get(), tp::SpatialSource{sw, sh, aw + 2, ah}, w, h); });
        // The truncated half of an odd output (a pillarboxed 3413-wide scene).
        if (w & 1) {
            tp::Output half(device.Get(), tp::SpatialSource{sw, sh, w / 2, h / 2}, w, h);
        } else
            rejects([&] { tp::Output invalid(device.Get(), tp::SpatialSource{sw, sh, w / 2 + 2, h / 2}, w, h); });
    }
    rejects([&] { output.record(list.Get()); });
    rejects([&] { output.bind(source.Get(), source.Get()); });
    rejects([&] { output.bind(source.Get(), follow.Get()); });
    output.bind(source.Get(), native.Get());
    rejects([&] { output.bind(source.Get(), native.Get()); });
    rejects([&] { output.record(list.Get(), D3D12_RESOURCE_STATE_COMMON); });
    output.prepare_record(list.Get());
    output.record(list.Get());
    restore.apply();
    rejects([&] { output.record(list.Get()); });
    rejects([&] { output.retire(fence.Get(), 1, true); });
    transition(list.Get(), native.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    native_pixels.copy(list.Get(), native.Get());
    transition(list.Get(), native.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    list->Dispatch((w + 15) / 16, (h + 15) / 16, 1); // No root/heap/PSO rebind: restored compute state must work.
    transition(list.Get(), follow.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    follow_pixels.copy(list.Get(), follow.Get());
    transition(list.Get(), native.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
               D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    list->SetPipelineState(graphic_pipeline.Get());
    list->DrawInstanced(3, 1, 0, 0); // Graphics tables and constants must survive too.
    transition(list.Get(), graphics.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
    graphics_pixels.copy(list.Get(), graphics.Get());
    source_pixels.copy(list.Get(), source.Get());
    hr(list->Close(), "close");
    ID3D12CommandList *lists[]{list.Get()};
    queue->ExecuteCommandLists(1, lists);
    hr(queue->Signal(fence.Get(), 1), "signal");
    const auto event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    require(event != nullptr, "event");
    hr(fence->SetEventOnCompletion(1, event), "completion");
    require(WaitForSingleObject(event, 15000) == WAIT_OBJECT_0 && fence->GetCompletedValue() == 1, "complete");
    CloseHandle(event);
    hr(device->GetDeviceRemovedReason(), "device removed");
    rejects([&] { output.retire(fence.Get(), 1, false); });
    hr(allocator->Reset(), "retire allocator");
    hr(list->Reset(allocator.Get(), nullptr), "retire list");
    hr(list->Close(), "retired close");
    output.retire(fence.Get(), 1, true);
    require(output.recorded() && output.retired(), "retirement");
    auto a = native_pixels.read(), b = follow_pixels.read(), c = graphics_pixels.read(), s = source_pixels.read();
    require(s == source_data, "source unchanged");
    for (std::size_t i = 0; i < std::size_t(w) * h; ++i) {
        unsigned actual{};
        std::memcpy(&actual, a.data() + i * 4, 4);
        std::array<float, 4> spatial_reference{};
        if (spatial) {
            const auto x = std::clamp((double(i % w) + .5) * aw / w - .5, 0., double(aw - 1));
            const auto y = std::clamp((double(i / w) + .5) * ah / h - .5, 0., double(ah - 1));
            const auto x0 = static_cast<unsigned>(x), y0 = static_cast<unsigned>(y);
            const auto x1 = std::min(x0 + 1, aw - 1), y1 = std::min(y0 + 1, ah - 1);
            for (unsigned channel_index = 0; channel_index < 3; ++channel_index) {
                const auto at = [&](unsigned px, unsigned py) {
                    return double(input[(std::size_t(py) * sw + px) * 4 + channel_index]);
                };
                const auto top = at(x0, y0) + (at(x1, y0) - at(x0, y0)) * (x - x0);
                const auto bottom = at(x0, y1) + (at(x1, y1) - at(x0, y1)) * (x - x0);
                spatial_reference[channel_index] = static_cast<float>(top + (bottom - top) * (y - y0));
            }
        }
        const auto expected = packed(spatial ? spatial_reference.data() : input.data() + i * 4);
        // The fractional-ratio oracle uses double precision, whereas the GPU
        // uses float division/interpolation. At a quantization midpoint their
        // nearest representable values may be adjacent. Exact checks remain
        // mandatory for identity and the initial 2x Performance route.
        bool equal = actual == expected;
        if (mode == L"--spatial-fractional")
            equal = std::abs(int(actual & 2047) - int(expected & 2047)) <= 1 &&
                    std::abs(int((actual >> 11) & 2047) - int((expected >> 11) & 2047)) <= 1 &&
                    std::abs(int(actual >> 22) - int(expected >> 22)) <= 1;
        if (!equal)
            throw std::runtime_error("packed reference mismatch pixel " + std::to_string(i) + " got " +
                                     std::to_string(actual) + " expected " + std::to_string(expected));
        const float native_rgb[]{unpack(actual & 2047, 6), unpack((actual >> 11) & 2047, 6), unpack(actual >> 22, 5)};
        std::array<float, 4> compute{}, draw{};
        std::memcpy(compute.data(), b.data() + i * 16, 16);
        std::memcpy(draw.data(), c.data() + i * 16, 16);
        require(compute == std::array<float, 4>{native_rgb[0] + 3, native_rgb[1], native_rgb[2], 11},
                "downstream compute bindings/pixels");
        require(draw == std::array<float, 4>{native_rgb[0] + 7, native_rgb[1], native_rgb[2], 11},
                "downstream graphics bindings/pixels");
    }
    unsigned warnings{}, errors_count{};
    for (UINT64 i = 0; i < messages->GetNumStoredMessagesAllowedByRetrievalFilter(); ++i) {
        SIZE_T size{};
        hr(messages->GetMessage(i, nullptr, &size), "message size");
        std::vector<std::byte> bytes(size);
        auto *m = reinterpret_cast<D3D12_MESSAGE *>(bytes.data());
        hr(messages->GetMessage(i, m, &size), "message");
        if (m->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) ++errors_count;
        if (m->Severity == D3D12_MESSAGE_SEVERITY_WARNING) ++warnings;
        if (m->Severity <= D3D12_MESSAGE_SEVERITY_WARNING && warnings + errors_count <= 16)
            std::cerr << m->pDescription << '\n';
    }
    require(!warnings && !errors_count, "GPU validation diagnostics");
    if (!warp) {
        std::ofstream out(argv[7]);
        out << "{\"accepted\":true,\"width\":" << w << ",\"height\":" << h << ",\"pixels\":" << UINT64(w) * h
            << ",\"exact_packed_reference\":true,\"source_unchanged\":true,\"compute_restored\":true,\"graphics_tables_restored\":true,\"api_hooks_installed\":18,\"observed_state_used\":true,\"gpu_validation\":true,\"debug_errors\":0,\"debug_warnings\":0,\"game_process_access\":false}\n";
        require(bool(out), "result write");
    }
    std::cout << "Verified " << UINT64(w) * h
              << " packed pixels, restored compute/graphics output, unchanged source and clean GPU validation\n";
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    try {
        run(argc, argv);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
