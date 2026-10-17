// full_post's input upscale (full_post_inputs.hpp) on WARP with the debug
// layer: the scene depth, GBuffer4 and GBuffer5 hold distinct texels over their
// whole output-size allocations; after the pass every texel is the render
// region's texel under its centre, bit for bit, each texture is back in its
// entry state, and the game's compute recording continues with its own root
// signature, arguments and pipeline (its next dispatch stores its root
// constant). A recording that never set a compute signature or pipeline gets
// the signature unbound, and the game's own then validate. restore_depth()
// puts the depth's render region back (the rest stays upscaled); after
// keep_depth() alone, it leaves the depth unchanged.
#include "../src/scaled_temporal/full_post_inputs.hpp"
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
using Microsoft::WRL::ComPtr;
namespace st = mhw::scaled_temporal;
namespace tp = mhw::temporal_present;
void check(bool value, const char *why) {
    if (!value) {
        std::cerr << "FAIL " << why << '\n';
        std::exit(1);
    }
}
std::uintptr_t id(IUnknown *p) {
    return reinterpret_cast<std::uintptr_t>(p);
}
static_assert(st::full_post_source_texel(0, 64, 96) == 0 && st::full_post_source_texel(1, 64, 96) == 1 &&
                  st::full_post_source_texel(2, 64, 96) == 1 && st::full_post_source_texel(95, 64, 96) == 63 &&
                  st::full_post_source_texel(31, 32, 32) == 31 && st::full_post_source_texel(1, 25, 37) == 1 &&
                  st::full_post_source_texel(36, 25, 37) == 24,
              "nearest texel under the output texel's centre");
struct Gpu {
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12InfoQueue> info;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12Fence> fence;
    std::uint64_t value{};
    Gpu() {
        ComPtr<ID3D12Debug> debug;
        check(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))), "debug interface");
        debug->EnableDebugLayer();
        ComPtr<IDXGIFactory4> factory;
        ComPtr<IDXGIAdapter> warp;
        check(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) &&
                  SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))) &&
                  SUCCEEDED(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))),
              "WARP device");
        check(SUCCEEDED(device.As(&info)), "info queue");
        D3D12_COMMAND_QUEUE_DESC qd{};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        check(SUCCEEDED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue))) &&
                  SUCCEEDED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))),
              "queue");
    }
    void run(ID3D12GraphicsCommandList *list) {
        check(SUCCEEDED(list->Close()), "close");
        ID3D12CommandList *lists[]{list};
        queue->ExecuteCommandLists(1, lists);
        check(SUCCEEDED(queue->Signal(fence.Get(), ++value)), "signal");
        const auto event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        check(event && SUCCEEDED(fence->SetEventOnCompletion(value, event)), "fence event");
        WaitForSingleObject(event, 30000);
        CloseHandle(event);
        check(fence->GetCompletedValue() == value && device->GetDeviceRemovedReason() == S_OK, "GPU completion");
    }
    void quiet() {
        const auto n = info->GetNumStoredMessages();
        unsigned problems = 0;
        for (UINT64 i = 0; i < n; ++i) {
            SIZE_T size = 0;
            info->GetMessage(i, nullptr, &size);
            auto *m = static_cast<D3D12_MESSAGE *>(std::malloc(size));
            if (m && SUCCEEDED(info->GetMessage(i, m, &size)) && m->Severity <= D3D12_MESSAGE_SEVERITY_WARNING) {
                ++problems;
                std::cerr << "debug layer: " << m->pDescription << '\n';
            }
            std::free(m);
        }
        info->ClearStoredMessages();
        check(problems == 0, "no debug-layer warnings or errors");
    }
};
ComPtr<ID3D12Resource> buffer(ID3D12Device *device, std::uint64_t size, D3D12_HEAP_TYPE type,
                              D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) {
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width = size;
    d.Height = d.DepthOrArraySize = d.MipLevels = 1;
    d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    d.Flags = flags;
    D3D12_HEAP_PROPERTIES h{};
    h.Type = type;
    ComPtr<ID3D12Resource> r;
    const auto state = type == D3D12_HEAP_TYPE_UPLOAD     ? D3D12_RESOURCE_STATE_GENERIC_READ
                       : type == D3D12_HEAP_TYPE_READBACK ? D3D12_RESOURCE_STATE_COPY_DEST
                                                          : D3D12_RESOURCE_STATE_COMMON;
    check(SUCCEEDED(device->CreateCommittedResource(&h, D3D12_HEAP_FLAG_NONE, &d, state, nullptr, IID_PPV_ARGS(&r))),
          "buffer");
    return r;
}
ComPtr<ID3DBlob> compile(const char *source, const char *target) {
    ComPtr<ID3DBlob> code, errors;
    if (FAILED(
            D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, "main", target, 0, 0, &code, &errors))) {
        if (errors) std::cerr << static_cast<const char *>(errors->GetBufferPointer());
        check(false, target);
    }
    return code;
}
// The game's compute state: a root constant and a root UAV, and a shader that
// stores the constant.
struct Game {
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> pipeline;
    ComPtr<ID3D12Resource> out;
    explicit Game(ID3D12Device *device) {
        std::array<D3D12_ROOT_PARAMETER, 2> p{};
        p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        p[0].Constants = {0, 0, 1};
        p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        p[1].Descriptor = {0, 0};
        D3D12_ROOT_SIGNATURE_DESC d{2, p.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ComPtr<ID3DBlob> blob, errors;
        check(SUCCEEDED(D3D12SerializeRootSignature(&d, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors)) &&
                  SUCCEEDED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                                        IID_PPV_ARGS(&root))),
              "game root");
        const auto cs = compile(
            "cbuffer C : register(b0) { uint v; }; RWByteAddressBuffer o : register(u0); [numthreads(1,1,1)] void main() { o.Store(0, v); }",
            "cs_5_0");
        D3D12_COMPUTE_PIPELINE_STATE_DESC c{};
        c.pRootSignature = root.Get();
        c.CS = {cs->GetBufferPointer(), cs->GetBufferSize()};
        check(SUCCEEDED(device->CreateComputePipelineState(&c, IID_PPV_ARGS(&pipeline))), "game pipeline");
        out = buffer(device, 256, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    }
};
std::uint32_t texel(unsigned role, unsigned x, unsigned y) {
    // R32_FLOAT: a finite float per pixel; R32_UINT: all 32 bits, including
    // the flag bit; R8G8_SNORM: any 16 bits.
    const std::uint32_t h = (x * 73856093U) ^ (y * 19349663U) ^ (role * 83492791U);
    if (role == 0) {
        const float f = static_cast<float>(x) + static_cast<float>(y) * 4096.F + .25F;
        std::uint32_t b;
        std::memcpy(&b, &f, 4);
        return b;
    }
    return role == 1 ? h * 2654435761U : (h * 40503U) & 0xffffU;
}
unsigned bpp(unsigned role) {
    return role == 2 ? 2U : 4U;
}
unsigned pitch(unsigned w, unsigned b) {
    return (w * b + 255) & ~255U;
}
struct Case {
    unsigned w, h, rw, rh;
    bool compute_bound;
    bool keep_only{};
};
void run_case(Gpu &g, st::FullPostInputs &inputs, const Case &c) {
    auto *device = g.device.Get();
    std::string reason;
    check(inputs.prepare(device, c.w, c.h, reason), "prepare");
    check(inputs.bytes() == 3ULL * pitch(c.w, 4) * c.h, "buffer size");
    const std::array<D3D12_RESOURCE_STATES, 3> states{D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                                                          D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                                                      D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST};
    std::array<ComPtr<ID3D12Resource>, 3> textures;
    std::array<ComPtr<ID3D12Resource>, 3> uploads, readbacks;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    check(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) &&
              SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
                                                  IID_PPV_ARGS(&list))),
          "list");
    for (unsigned r = 0; r < 3; ++r) {
        D3D12_RESOURCE_DESC d{};
        d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        d.Width = c.w;
        d.Height = c.h;
        d.DepthOrArraySize = d.MipLevels = 1;
        d.Format = st::full_post_input_format(static_cast<st::FullPostInputRole>(r));
        d.SampleDesc.Count = 1;
        d.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        D3D12_HEAP_PROPERTIES h{};
        h.Type = D3D12_HEAP_TYPE_DEFAULT;
        check(SUCCEEDED(device->CreateCommittedResource(&h, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COPY_DEST,
                                                        nullptr, IID_PPV_ARGS(&textures[r]))),
              "texture");
        const auto p = pitch(c.w, bpp(r));
        uploads[r] = buffer(device, std::uint64_t(p) * c.h, D3D12_HEAP_TYPE_UPLOAD);
        readbacks[r] = buffer(device, std::uint64_t(p) * c.h, D3D12_HEAP_TYPE_READBACK);
        std::byte *m{};
        check(SUCCEEDED(uploads[r]->Map(0, nullptr, reinterpret_cast<void **>(&m))), "map upload");
        for (unsigned y = 0; y < c.h; ++y)
            for (unsigned x = 0; x < c.w; ++x) {
                const auto v = texel(r, x, y);
                std::memcpy(m + std::uint64_t(y) * p + x * bpp(r), &v, bpp(r));
            }
        uploads[r]->Unmap(0, nullptr);
        D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
        dst.pResource = textures[r].Get();
        src.pResource = uploads[r].Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Footprint = {d.Format, c.w, c.h, 1, p};
        list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        if (states[r] != D3D12_RESOURCE_STATE_COPY_DEST) {
            D3D12_RESOURCE_BARRIER b{};
            b.Transition = {textures[r].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COPY_DEST,
                            states[r]};
            list->ResourceBarrier(1, &b);
        }
    }
    // The game's recording before the slot.
    Game game(device);
    tp::Bindings roots;
    roots.reset(1);
    if (c.compute_bound) {
        list->SetComputeRootSignature(game.root.Get());
        list->SetComputeRoot32BitConstant(0, 0x5eed, 0);
        list->SetComputeRootUnorderedAccessView(1, game.out->GetGPUVirtualAddress());
        list->SetPipelineState(game.pipeline.Get());
        roots.root(false, id(game.root.Get()));
        const std::uint32_t seed[]{0x5eed};
        roots.constants(false, 0, 0, seed);
        roots.descriptor(false, 1, tp::ArgumentKind::uav, game.out->GetGPUVirtualAddress());
        roots.pipeline = id(game.pipeline.Get());
    }
    const std::array<st::FullPostInput, 3> in{
        {{textures[0].Get(), states[0]}, {textures[1].Get(), states[1]}, {textures[2].Get(), states[2]}}};
    // Refusals record nothing: a wrong role order, a region larger than the
    // output, an unknown snapshot.
    check(!inputs.record(list.Get(), {in[1], in[0], in[2]}, c.rw, c.rh, roots), "role formats");
    check(!inputs.record(list.Get(), in, c.w + 1, c.rh, roots), "region bounds");
    tp::Bindings unknown;
    check(!inputs.record(list.Get(), in, c.rw, c.rh, unknown), "unknown recording");
    check(!inputs.restore_depth(list.Get(), in[1], c.rw, c.rh) && !inputs.keep_depth(list.Get(), in[0], c.w + 1, c.rh),
          "depth helpers refuse");
    if (c.keep_only)
        check(inputs.keep_depth(list.Get(), in[0], c.rw, c.rh), "keep the depth");
    else
        check(inputs.record(list.Get(), in, c.rw, c.rh, roots), "record");
    // The late draws' restore of the depth's render region.
    check(inputs.restore_depth(list.Get(), in[0], c.rw, c.rh), "restore the depth");
    // The game continues: its own dispatch with its restored arguments, or,
    // on a recording that had none, after setting its own.
    if (!c.compute_bound) {
        list->SetComputeRootSignature(game.root.Get());
        list->SetComputeRoot32BitConstant(0, 0x5eed, 0);
        list->SetComputeRootUnorderedAccessView(1, game.out->GetGPUVirtualAddress());
        list->SetPipelineState(game.pipeline.Get());
    }
    D3D12_RESOURCE_BARRIER b{};
    b.Transition = {game.out.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COMMON,
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS};
    list->ResourceBarrier(1, &b);
    list->Dispatch(1, 1, 1);
    const auto game_readback = buffer(device, 256, D3D12_HEAP_TYPE_READBACK);
    std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    list->ResourceBarrier(1, &b);
    list->CopyBufferRegion(game_readback.Get(), 0, game.out.Get(), 0, 4);
    // Each texture leaves in its entry state.
    for (unsigned r = 0; r < 3; ++r) {
        if (states[r] != D3D12_RESOURCE_STATE_COPY_SOURCE) {
            D3D12_RESOURCE_BARRIER t{};
            t.Transition = {textures[r].Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, states[r],
                            D3D12_RESOURCE_STATE_COPY_SOURCE};
            list->ResourceBarrier(1, &t);
        }
        D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
        src.pResource = textures[r].Get();
        dst.pResource = readbacks[r].Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.PlacedFootprint.Footprint = {st::full_post_input_format(static_cast<st::FullPostInputRole>(r)), c.w, c.h, 1,
                                         pitch(c.w, bpp(r))};
        list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }
    g.run(list.Get());
    g.quiet();
    {
        std::uint32_t *v{};
        check(SUCCEEDED(game_readback->Map(0, nullptr, reinterpret_cast<void **>(&v))), "map game");
        check(*v == 0x5eed, "the game's arguments restored");
        game_readback->Unmap(0, nullptr);
    }
    for (unsigned r = 0; r < 3; ++r) {
        const std::byte *m{};
        check(SUCCEEDED(readbacks[r]->Map(0, nullptr, reinterpret_cast<void **>(const_cast<std::byte **>(&m)))),
              "map readback");
        const auto p = pitch(c.w, bpp(r));
        unsigned wrong = 0;
        for (unsigned y = 0; y < c.h; ++y)
            for (unsigned x = 0; x < c.w; ++x) {
                std::uint32_t v = 0;
                std::memcpy(&v, m + std::uint64_t(y) * p + x * bpp(r), bpp(r));
                const bool original = c.keep_only || (r == 0 && x < c.rw && y < c.rh);
                const auto want = original ? texel(r, x, y)
                                           : texel(r, st::full_post_source_texel(x, c.rw, c.w),
                                                   st::full_post_source_texel(y, c.rh, c.h));
                if (v != want && wrong++ < 4)
                    std::cerr << "role " << r << " (" << x << ',' << y << ") " << std::hex << v << " want " << want
                              << std::dec << '\n';
            }
        readbacks[r]->Unmap(0, nullptr);
        check(!wrong, "nearest upscale, bit-exact");
    }
    std::cout
        << "PASS " << c.rw << 'x' << c.rh << " -> " << c.w << 'x' << c.h
        << (c.compute_bound ? "" : " (no compute state)")
        << (c.keep_only
                ? ": depth kept and restored unchanged, the game's compute state kept\n"
                : ": depth, GBuffer4 and GBuffer5 upscaled, the depth's region restored, states and the game's compute state kept\n");
}
int main() {
    Gpu g;
    st::FullPostInputs inputs;
    std::string reason;
    check(!inputs.prepare(g.device.Get(), 0, 8, reason) && !inputs.ready(), "empty extent refused");
    for (const auto &c : {Case{96, 54, 64, 36, true}, Case{37, 23, 25, 15, true}, Case{130, 70, 65, 35, true},
                          Case{64, 32, 64, 32, true}, Case{97, 61, 43, 27, false}, Case{96, 54, 64, 36, true, true}})
        run_case(g, inputs, c);
}
