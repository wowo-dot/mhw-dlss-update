// SharpenPass on WARP with the debug layer and GPU-based validation: both
// shaders compile as the DLL builds them; the scene ends in COPY_DEST as the
// final copy leaves it; a flat region comes back bit for bit (SDR, and HDR10
// above 1.0); a fine texture gains contrast, in HDR10 without clipping at 1;
// scenes are cached, and a scene of another size waits for reset().
#include "../src/sharpen/sharpen_pass.hpp"
#include <windows.h>
#include <dxgi1_4.h>
#include <d3d12sdklayers.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
using mhw::sharpen::SharpenPass;
namespace pv = DirectX::PackedVector;

namespace {
void require(bool v, const std::string &why) {
    if (!v) throw std::runtime_error(why);
}
void hr(HRESULT v, const char *why) {
    require(SUCCEEDED(v), why);
}

// Not multiples of the 32 x 32 block. Columns below `flat_end` are flat; from
// `texture_begin` on, a sinusoid of period 6 pixels along x.
constexpr unsigned width = 72, height = 40, flat_end = 24, texture_begin = 30;
constexpr float pi = 3.14159265f;

struct Device {
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12InfoQueue> info;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Fence> fence;
    std::uint64_t signal{};

    Device() {
        ComPtr<ID3D12Debug> debug;
        hr(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)), "debug layer");
        debug->EnableDebugLayer();
        ComPtr<ID3D12Debug1> validation;
        hr(debug.As(&validation), "validation");
        validation->SetEnableGPUBasedValidation(TRUE);
        ComPtr<IDXGIFactory4> factory;
        ComPtr<IDXGIAdapter1> warp;
        hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "factory");
        hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)), "warp");
        hr(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "device");
        hr(device.As(&info), "info queue");
        hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "fence");
        D3D12_COMMAND_QUEUE_DESC q{};
        hr(device->CreateCommandQueue(&q, IID_PPV_ARGS(&queue)), "queue");
        hr(device->CreateCommandAllocator(q.Type, IID_PPV_ARGS(&allocator)), "allocator");
        hr(device->CreateCommandList(0, q.Type, allocator.Get(), nullptr, IID_PPV_ARGS(&list)), "list");
    }
    ComPtr<ID3D12Resource> resource(const D3D12_RESOURCE_DESC &desc, D3D12_HEAP_TYPE type,
                                    D3D12_RESOURCE_STATES state) {
        D3D12_HEAP_PROPERTIES heap{type};
        ComPtr<ID3D12Resource> r;
        hr(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&r)),
           "resource");
        return r;
    }
    void execute() {
        hr(list->Close(), "close");
        ID3D12CommandList *lists[]{list.Get()};
        queue->ExecuteCommandLists(1, lists);
        hr(queue->Signal(fence.Get(), ++signal), "signal");
        const HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        require(event != nullptr, "event");
        const auto armed = fence->SetEventOnCompletion(signal, event);
        const auto waited = SUCCEEDED(armed) ? WaitForSingleObject(event, 30000) : WAIT_FAILED;
        CloseHandle(event);
        require(waited == WAIT_OBJECT_0, "GPU wait");
        hr(device->GetDeviceRemovedReason(), "device health");
        hr(allocator->Reset(), "allocator reset");
        hr(list->Reset(allocator.Get(), nullptr), "list reset");
    }
    void messages() {
        unsigned problems{};
        for (UINT64 i = 0; i < info->GetNumStoredMessages(); ++i) {
            SIZE_T size{};
            hr(info->GetMessage(i, nullptr, &size), "message size");
            std::vector<std::byte> bytes(size);
            auto *m = reinterpret_cast<D3D12_MESSAGE *>(bytes.data());
            hr(info->GetMessage(i, m, &size), "message");
            if (m->Severity <= D3D12_MESSAGE_SEVERITY_WARNING) {
                std::cerr << m->pDescription << '\n';
                ++problems;
            }
        }
        require(!problems, "debug layer warnings or errors");
    }
};

D3D12_RESOURCE_BARRIER transition(ID3D12Resource *r, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
    D3D12_RESOURCE_BARRIER b{};
    b.Transition = {r, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, from, to};
    return b;
}

std::uint32_t pack(float v) {
    pv::XMFLOAT3PK p;
    pv::XMStoreFloat3PK(&p, DirectX::XMVectorSet(v, v, v, 0.0f));
    return p.v;
}
float unpack_red(std::uint32_t bits) {
    pv::XMFLOAT3PK p;
    p.v = bits;
    return DirectX::XMVectorGetX(pv::XMLoadFloat3PK(&p));
}

// A scene T as the game's final attachment: R11G11B10_FLOAT render target,
// here in COPY_DEST (after the final copy), with its upload and readback.
struct Scene {
    ComPtr<ID3D12Resource> texture, upload, readback;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    unsigned w{}, h{};

    Scene(Device &f, unsigned width_, unsigned height_) : w(width_), h(height_) {
        D3D12_RESOURCE_DESC d{};
        d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        d.Width = w;
        d.Height = h;
        d.DepthOrArraySize = d.MipLevels = 1;
        d.SampleDesc.Count = 1;
        d.Format = SharpenPass::format;
        d.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        texture = f.resource(d, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);
        UINT64 bytes{};
        f.device->GetCopyableFootprints(&d, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);
        D3D12_RESOURCE_DESC b{};
        b.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        b.Width = bytes;
        b.Height = b.DepthOrArraySize = b.MipLevels = 1;
        b.SampleDesc.Count = 1;
        b.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        upload = f.resource(b, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        readback = f.resource(b, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
    }
    // Every pixel of a column the same grey.
    std::vector<std::uint32_t> fill(Device &f, float (*column)(unsigned x, float base), float base) {
        std::vector<std::uint32_t> packed(w);
        for (unsigned x = 0; x < w; ++x) packed[x] = pack(column(x, base));
        std::byte *texels{};
        const D3D12_RANGE none{};
        hr(upload->Map(0, &none, reinterpret_cast<void **>(&texels)), "map upload");
        for (unsigned y = 0; y < h; ++y) std::memcpy(texels + y * footprint.Footprint.RowPitch, packed.data(), 4 * w);
        upload->Unmap(0, nullptr);
        D3D12_TEXTURE_COPY_LOCATION dst{texture.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}},
            src{upload.Get(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, {}};
        src.PlacedFootprint = footprint;
        f.list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        return packed;
    }
    // Records the readback; T goes back to COPY_DEST (GPU-based validation
    // checks the pass left it there).
    void read(Device &f) {
        auto b = transition(texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
        f.list->ResourceBarrier(1, &b);
        D3D12_TEXTURE_COPY_LOCATION dst{readback.Get(), D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, {}},
            src{texture.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {}};
        dst.PlacedFootprint = footprint;
        f.list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
        f.list->ResourceBarrier(1, &b);
    }
    std::uint32_t at(unsigned x, unsigned y) const {
        std::byte *texels{};
        hr(readback->Map(0, nullptr, reinterpret_cast<void **>(&texels)), "map readback");
        std::uint32_t v{};
        std::memcpy(&v, texels + y * footprint.Footprint.RowPitch + 4 * x, 4);
        const D3D12_RANGE none{};
        readback->Unmap(0, &none);
        return v;
    }
};

float pattern(unsigned x, float base) {
    if (x < texture_begin) return base;
    return base * (1.0f + 0.3f * std::sin(2.0f * pi * static_cast<float>(x - texture_begin) / 6.0f));
}

struct Case {
    const char *name;
    bool hdr;
    float white, base;
};

// Runs one case at 100 %: the flat columns are unchanged bit for bit, the
// texture's contrast grows (and in HDR10 its highlights stay above 1).
void run(Device &f, SharpenPass &pass, Scene &scene, const Case &c) {
    const auto input = scene.fill(f, pattern, c.base);
    mhw::sharpen::Constants k;
    require(mhw::sharpen::constants(100, scene.w, scene.h, c.hdr, c.white, k), std::string(c.name) + ": constants");
    require(pass.record(f.list.Get(), scene.texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, k, c.hdr) ==
                SharpenPass::Result::recorded,
            std::string(c.name) + ": recorded");
    scene.read(f);
    f.execute();
    float in_low = 1e9f, in_high = -1e9f, out_low = 1e9f, out_high = -1e9f;
    for (unsigned y = 0; y < scene.h; ++y)
        for (unsigned x = 0; x < scene.w; ++x) {
            const auto out = scene.at(x, y);
            if (x < flat_end)
                require(out == input[x], std::string(c.name) + ": flat column " + std::to_string(x) + " changed");
            // The texture's interior, away from the flat region and the edges.
            if (x >= texture_begin + 4 && x + 4 < scene.w && y >= 4 && y + 4 < scene.h) {
                in_low = std::min(in_low, unpack_red(input[x]));
                in_high = std::max(in_high, unpack_red(input[x]));
                out_low = std::min(out_low, unpack_red(out));
                out_high = std::max(out_high, unpack_red(out));
            }
        }
    std::cout << c.name << ": texture " << in_low << ".." << in_high << " -> " << out_low << ".." << out_high << '\n';
    require(out_high - out_low > (in_high - in_low) * 1.05f, std::string(c.name) + ": contrast grew");
    if (c.hdr && c.base > 1.0f) require(out_low > 1.0f, std::string(c.name) + ": no clip at 1");
}
} // namespace

int main() {
    try {
        require(SharpenPass::compile(false) != nullptr && SharpenPass::compile(true) != nullptr, "offline compile");
        Device f;
        SharpenPass pass(f.device.Get());
        Scene a(f, width, height), b(f, width, height);
        // SDR mid-grey; HDR10 at the game's default and game-109's white
        // levels, below and above 1.0.
        run(f, pass, a, {"sdr", false, 0.0f, 0.18f});
        run(f, pass, a, {"hdr-300", true, 300.0f, 0.3f});
        run(f, pass, a, {"hdr-720-bright", true, 720.69f, 3.0f});
        // A second attachment takes the next cached slot.
        run(f, pass, b, {"sdr-second-scene", false, 0.0f, 0.4f});

        // Another size waits for reset() (the runtime's drained rebuild).
        Scene other(f, 64, 24);
        other.fill(f, pattern, 0.2f);
        mhw::sharpen::Constants k;
        require(mhw::sharpen::constants(50, other.w, other.h, false, 0.0f, k), "other constants");
        require(pass.record(f.list.Get(), other.texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, k, false) ==
                    SharpenPass::Result::scratch,
                "another size before reset");
        f.execute();
        pass.reset();
        run(f, pass, other, {"after-reset", false, 0.0f, 0.2f});
        // Constants for another extent than the scene's are refused.
        require(mhw::sharpen::constants(50, width, height, false, 0.0f, k) &&
                    pass.record(f.list.Get(), other.texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, k, false) ==
                        SharpenPass::Result::layout,
                "extent mismatch");
        f.execute();
        f.messages();
        std::cout << "sharpen_pass_warp: ok\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "sharpen_pass_warp: " << e.what() << '\n';
        return 1;
    }
}
