// HudlessPass on WARP with the debug layer and GPU-based validation: both
// compute shaders compile as the DLL builds them; the brightness capture keeps
// the last valid CBSystem across lists and counts attempts/matches; the
// transform leaves the scene in RENDER_TARGET and the target in its tag state,
// matches the unit-tested screen transform with the captured g (g = 1 before
// any valid capture), and the probe reports the state it used.
#include "../src/frame_generation/hudless_pass.hpp"
#include "../src/frame_generation/screen_transform.hpp"
#include <windows.h>
#include <dxgi1_4.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#include <cmath>
#include <cstring>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using Microsoft::WRL::ComPtr;
using mhw::frame_generation::HudlessPass;
namespace screen = mhw::frame_generation::screen;

namespace {
void require(bool v, const std::string &why) { if (!v) throw std::runtime_error(why); }
void hr(HRESULT v, const char *why) { require(SUCCEEDED(v), why); }

constexpr unsigned width = 72, height = 40;  // Not a multiple of the 16x16 group.
constexpr auto tag_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
constexpr float gamma_correct = 1.0f / 2.2f;

// Values exact in R11G11B10_FLOAT; 2.0 checks the saturate, 1/512 the linear
// sRGB segment. Columns of width / 4 pixels.
constexpr float columns[4][3]{{0.5f, 0.25f, 0.125f}, {0.75f, 1.0f / 512, 0.0f}, {2.0f, 1.0f, 0.0625f}, {0.0f, 0.0f, 0.0f}};

// Candidate CBSystem contents: fGammaCorrect, fGammaCorrectEx.x.
constexpr float candidates[][2]{{gamma_correct, 2.0f}, {0.3f, 2.0f},          {gamma_correct, 0.5f},
                                {gamma_correct, 8.0f}, {gamma_correct, 1.0f}, {0.3f, 0.7f},
                                {gamma_correct, 0.1f}, {gamma_correct, 3.0f}};  // slider min, max
// Each step: captures in their own list (in order; -1 none), then the
// transform in the next list, as in the game (screen write list, next frame).
struct Step {
    int captures[2];
    bool ok;                        // A valid capture exists
    float gamma, gamma_ex;          // Last valid capture (probe), 0 before any
    std::uint32_t attempts, matches;
    float last_gamma, last_gamma_ex;  // Raw values of the last attempt
    int region;                       // Candidate whose iRegion the state keeps (-1 none)
};
constexpr Step steps[]{
    {{-1, -1}, false, 0.0f, 0.0f, 0, 0, 0.0f, 0.0f, -1},
    {{0, -1}, true, gamma_correct, 2.0f, 1, 1, gamma_correct, 2.0f, 0},
    {{1, -1}, true, gamma_correct, 2.0f, 2, 1, 0.3f, 2.0f, 0},  // bad layout: keep 2
    {{2, -1}, true, gamma_correct, 0.5f, 3, 2, gamma_correct, 0.5f, 2},
    {{3, -1}, true, gamma_correct, 0.5f, 4, 2, gamma_correct, 8.0f, 2},  // out of range: keep 0.5
    {{4, 5}, true, gamma_correct, 1.0f, 6, 3, 0.3f, 0.7f, 4},           // valid then invalid
    {{6, -1}, true, gamma_correct, 0.1f, 7, 4, gamma_correct, 0.1f, 6},  // the game's minimum
    {{7, -1}, true, gamma_correct, 3.0f, 8, 5, gamma_correct, 3.0f, 7}}; // and maximum

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
    ComPtr<ID3D12Resource> resource(const D3D12_RESOURCE_DESC &desc, D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES state) {
        D3D12_HEAP_PROPERTIES heap{type};
        ComPtr<ID3D12Resource> r;
        hr(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&r)), "resource");
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

// Exact R11G11B10_FLOAT encoding of 0 or a positive power-of-two multiple
// with a short mantissa (all `columns` values): 5-bit exponent, bias 15.
std::uint32_t small_float(float v, unsigned mantissa_bits) {
    if (v == 0.0f) return 0;
    int exponent{};
    const float fraction = std::frexp(v, &exponent) * 2.0f - 1.0f;  // v = (1 + fraction) * 2^(exponent - 1)
    const float scaled = fraction * float(1u << mantissa_bits);
    require(scaled == std::floor(scaled) && exponent - 1 + 15 > 0 && exponent - 1 + 15 < 31, "exact R11G11B10 value");
    return (std::uint32_t(exponent - 1 + 15) << mantissa_bits) | std::uint32_t(scaled);
}
std::uint32_t r11g11b10(const float (&c)[3]) {
    return small_float(c[0], 6) | small_float(c[1], 6) << 11 | small_float(c[2], 5) << 22;
}

D3D12_RESOURCE_BARRIER transition(ID3D12Resource *r, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
    D3D12_RESOURCE_BARRIER b{};
    b.Transition = {r, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, from, to};
    return b;
}
}  // namespace

int main() {
    try {
        require(HudlessPass::compile(HudlessPass::Shader::transform) != nullptr &&
                    HudlessPass::compile(HudlessPass::Shader::capture) != nullptr, "offline compile");
        Device f;
        HudlessPass pass(f.device.Get(), width, height);

        D3D12_RESOURCE_DESC d{};
        d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        d.Width = width;
        d.Height = height;
        d.DepthOrArraySize = d.MipLevels = 1;
        d.SampleDesc.Count = 1;
        d.Format = DXGI_FORMAT_R11G11B10_FLOAT;
        d.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        auto scene = f.resource(d, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT scene_footprint{};
        UINT64 scene_bytes{};
        f.device->GetCopyableFootprints(&d, 0, 1, 0, &scene_footprint, nullptr, nullptr, &scene_bytes);
        d.Format = HudlessPass::format;
        d.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        auto target = f.resource(d, D3D12_HEAP_TYPE_DEFAULT, tag_state);
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT64 bytes{};
        f.device->GetCopyableFootprints(&d, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);
        D3D12_RESOURCE_DESC b{};
        b.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        b.Width = bytes;
        b.Height = b.DepthOrArraySize = b.MipLevels = 1;
        b.SampleDesc.Count = 1;
        b.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        auto readback = f.resource(b, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
        b.Width = 256 * std::size(candidates);
        auto constants = f.resource(b, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);

        // CBSystem per candidate at 256-byte strides: iRegion@0, fGammaCorrect@20, fGammaCorrectEx@24.
        std::byte *cb{};
        const D3D12_RANGE none{};
        hr(constants->Map(0, &none, reinterpret_cast<void **>(&cb)), "map constants");
        for (unsigned i = 0; i < std::size(candidates); ++i) {
            const float values[3]{candidates[i][0], candidates[i][1], 7.0f};
            const std::uint32_t region[4]{i, 10 + i, 100 + i, 200 + i};
            std::memset(cb + 256 * i, 0, 256);
            std::memcpy(cb + 256 * i, region, sizeof(region));
            std::memcpy(cb + 256 * i + 20, values, sizeof(values));
        }
        constants->Unmap(0, nullptr);

        b.Width = scene_bytes;
        auto upload = f.resource(b, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        std::byte *texels{};
        hr(upload->Map(0, &none, reinterpret_cast<void **>(&texels)), "map scene upload");
        for (unsigned y = 0; y < height; ++y)
            for (unsigned x = 0; x < width; ++x) {
                const auto packed = r11g11b10(columns[x / (width / 4)]);
                std::memcpy(texels + y * scene_footprint.Footprint.RowPitch + x * 4, &packed, 4);
            }
        upload->Unmap(0, nullptr);

        unsigned checked{};
        for (unsigned i = 0; i < std::size(steps); ++i) {
            const auto &c = steps[i];
            const unsigned slot = i % HudlessPass::slots;
            if (c.captures[0] >= 0) {
                for (const int k : c.captures)
                    if (k >= 0) pass.capture(f.list.Get(), constants->GetGPUVirtualAddress() + UINT64(256) * unsigned(k));
                f.execute();
            }
            // Refill the scene; GPU-based validation checks that the previous
            // pass left it in RENDER_TARGET.
            auto fill = transition(scene.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
            f.list->ResourceBarrier(1, &fill);
            D3D12_TEXTURE_COPY_LOCATION scene_dst{}, scene_src{};
            scene_dst.pResource = scene.Get();
            scene_dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            scene_src.pResource = upload.Get();
            scene_src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            scene_src.PlacedFootprint = scene_footprint;
            f.list->CopyTextureRegion(&scene_dst, 0, 0, 0, &scene_src, nullptr);
            std::swap(fill.Transition.StateBefore, fill.Transition.StateAfter);
            f.list->ResourceBarrier(1, &fill);
            pass.record(f.list.Get(), slot, scene.Get(), target.Get(), tag_state);
            const auto to_copy = transition(target.Get(), tag_state, D3D12_RESOURCE_STATE_COPY_SOURCE);
            f.list->ResourceBarrier(1, &to_copy);
            D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
            dst.pResource = readback.Get();
            dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dst.PlacedFootprint = footprint;
            src.pResource = target.Get();
            src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            f.list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
            const auto back = transition(target.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, tag_state);
            f.list->ResourceBarrier(1, &back);
            f.execute();

            const auto probe = pass.probe(slot);
            require(probe.valid && probe.gamma == c.gamma && probe.gamma_ex == c.gamma_ex && probe.parameters_ok == c.ok &&
                        probe.attempts == c.attempts && probe.matches == c.matches && probe.last_gamma == c.last_gamma &&
                        probe.last_gamma_ex == c.last_gamma_ex &&
                        probe.region == (c.region < 0 ? std::array<std::uint32_t, 4>{}
                                                      : std::array<std::uint32_t, 4>{unsigned(c.region), 10u + c.region,
                                                                                     100u + c.region, 200u + c.region}),
                    "probe step " + std::to_string(i) + ": attempts " + std::to_string(probe.attempts) + " matches " +
                        std::to_string(probe.matches) + " g " + std::to_string(probe.gamma_ex));
            const float g = c.ok ? c.gamma_ex : 1.0f;
            void *mapped{};
            hr(readback->Map(0, nullptr, &mapped), "map readback");
            const auto *pixels = static_cast<const std::uint8_t *>(mapped);
            for (unsigned y = 0; y < height; ++y)
                for (unsigned x = 0; x < width; ++x) {
                    const auto *p = pixels + y * footprint.Footprint.RowPitch + x * 4;
                    const auto &v = columns[x / (width / 4)];
                    for (unsigned ch = 0; ch < 3; ++ch) {
                        const float expected = screen::srgb_encode(screen::brightness(v[ch], g)) * 255.0f;
                        require(std::abs(float(p[ch]) - expected) <= 1.0f,
                                "pixel step " + std::to_string(i) + " at " + std::to_string(x) + "," + std::to_string(y) +
                                    " channel " + std::to_string(ch) + ": " + std::to_string(p[ch]) + " vs " +
                                    std::to_string(expected));
                        ++checked;
                    }
                    require(p[3] == 255, "alpha");
                }
            const D3D12_RANGE nothing{};
            readback->Unmap(0, &nothing);
        }
        // An unwritten slot reads as invalid.
        HudlessPass fresh(f.device.Get(), width, height);
        require(!fresh.probe(0).valid, "unwritten probe");

        // v0.24.8: HDR10. The target R10G10B10A2_UNORM selects the PQ
        // transform; CBRenderFrame captures (layout-checked) set its
        // parameters, the shader's defaults before any.
        require(HudlessPass::compile(HudlessPass::Shader::transform_hdr) != nullptr &&
                    HudlessPass::compile(HudlessPass::Shader::capture_hdr) != nullptr, "offline HDR compile");
        require(pass.hdr_supported(), "R10G10B10A2 typed UAV store on WARP");
        d.Format = HudlessPass::hdr_format;
        auto hdr_target = f.resource(d, D3D12_HEAP_TYPE_DEFAULT, tag_state);
        // Brighter columns than SDR (exact in R11G11B10_FLOAT).
        constexpr float hdr_columns[4][3]{{0.5f, 0.25f, 0.125f}, {8.0f, 1.0f, 0.0f}, {32.0f, 2.0f, 0.75f}, {0.0f, 0.0f, 0.0f}};
        hr(upload->Map(0, &none, reinterpret_cast<void **>(&texels)), "map HDR scene upload");
        for (unsigned y = 0; y < height; ++y)
            for (unsigned x = 0; x < width; ++x) {
                const auto packed = r11g11b10(hdr_columns[x / (width / 4)]);
                std::memcpy(texels + y * scene_footprint.Footprint.RowPitch + x * 4, &packed, 4);
            }
        upload->Unmap(0, nullptr);
        // CBRenderFrame candidates at 256-byte strides: fDeltaTime@16,
        // bSSSEnable@24, bHdrOutput@48, white@52, ratio@56, gamma@60, GUI@64.
        struct Frame { float delta; std::uint32_t sss, hdr; float white, ratio, gamma; std::uint32_t gui; };
        constexpr Frame frames[]{{0.016f, 1, 1, 200.0f, 0.25f, 1.2f, 1}, {0.016f, 0, 0, 200.0f, 0.0f, 1.0f, 0},
                                 {0.016f, 0, 1, 5.0f, 0.0f, 1.0f, 0},    {0.016f, 0, 1, 200.0f, 1.5f, 1.0f, 0},
                                 {0.016f, 7, 1, 200.0f, 0.0f, 1.0f, 0},  {0.016f, 0, 1, 80.0f, 0.0f, 1.0f, 0}};
        b.Width = 256 * std::size(frames);
        auto frame_constants = f.resource(b, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        hr(frame_constants->Map(0, &none, reinterpret_cast<void **>(&cb)), "map frame constants");
        for (unsigned i = 0; i < std::size(frames); ++i) {
            const auto &v = frames[i];
            std::memset(cb + 256 * i, 0, 256);
            std::memcpy(cb + 256 * i + 16, &v.delta, 4);
            std::memcpy(cb + 256 * i + 24, &v.sss, 4);
            std::memcpy(cb + 256 * i + 48, &v.hdr, 4);
            std::memcpy(cb + 256 * i + 52, &v.white, 4);
            std::memcpy(cb + 256 * i + 56, &v.ratio, 4);
            std::memcpy(cb + 256 * i + 60, &v.gamma, 4);
            std::memcpy(cb + 256 * i + 64, &v.gui, 4);
        }
        frame_constants->Unmap(0, nullptr);
        // Each step: captures (candidate, root parameter), then the transform.
        struct HdrStep { int captures[2]; unsigned parameter; int kept; std::uint32_t attempts, matches; int last; };
        constexpr HdrStep hdr_steps[]{
            {{-1, -1}, 0, -1, 0, 0, -1},  // defaults before any capture
            {{0, -1}, 4, 0, 1, 1, 0},     // valid, GUI gamma on
            {{1, 2}, 4, 0, 3, 1, 2},      // SDR output, white level 5: keep 0
            {{3, 4}, 4, 0, 5, 1, 4},      // ratio 1.5, a garbage boolean: keep 0
            {{5, -1}, 2, 5, 6, 2, 5}};    // valid again, another parameter
        const auto readback_pixels = [&](ID3D12Resource *t) {
            const auto to_copy = transition(t, tag_state, D3D12_RESOURCE_STATE_COPY_SOURCE);
            f.list->ResourceBarrier(1, &to_copy);
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
            UINT64 total{};
            const auto td = t->GetDesc();
            f.device->GetCopyableFootprints(&td, 0, 1, 0, &fp, nullptr, nullptr, &total);
            D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
            dst.pResource = readback.Get();
            dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dst.PlacedFootprint = fp;
            src.pResource = t;
            src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            f.list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
            const auto back = transition(t, D3D12_RESOURCE_STATE_COPY_SOURCE, tag_state);
            f.list->ResourceBarrier(1, &back);
            return fp;
        };
        unsigned hdr_checked{};
        for (unsigned i = 0; i < std::size(hdr_steps); ++i) {
            const auto &c = hdr_steps[i];
            const unsigned slot = i % HudlessPass::slots;
            if (c.captures[0] >= 0) {
                for (const int k : c.captures)
                    if (k >= 0) pass.capture_hdr(f.list.Get(), frame_constants->GetGPUVirtualAddress() + UINT64(256) * unsigned(k), c.parameter);
                f.execute();
            }
            auto fill = transition(scene.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
            f.list->ResourceBarrier(1, &fill);
            D3D12_TEXTURE_COPY_LOCATION scene_dst{}, scene_src{};
            scene_dst.pResource = scene.Get();
            scene_dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            scene_src.pResource = upload.Get();
            scene_src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            scene_src.PlacedFootprint = scene_footprint;
            f.list->CopyTextureRegion(&scene_dst, 0, 0, 0, &scene_src, nullptr);
            std::swap(fill.Transition.StateBefore, fill.Transition.StateAfter);
            f.list->ResourceBarrier(1, &fill);
            pass.record(f.list.Get(), slot, scene.Get(), hdr_target.Get(), tag_state);
            const auto fp = readback_pixels(hdr_target.Get());
            f.execute();

            const auto probe = pass.probe(slot);
            screen::hdr::Parameters used;
            if (c.kept >= 0) {
                const auto &k = frames[c.kept];
                used = {k.white, k.ratio, k.gamma, k.gui != 0};
            }
            require(probe.valid && probe.hdr && probe.hdr_ok == (c.kept >= 0) && probe.white == used.white &&
                        probe.ratio == used.ratio && probe.hdr_gamma == used.gamma && probe.gui_gamma == used.gui_gamma &&
                        probe.hdr_attempts == c.attempts && probe.hdr_matches == c.matches &&
                        (c.kept < 0 || probe.hdr_parameter == (i == 4 ? 2u : 4u)) &&
                        (c.last < 0 || probe.last_white == frames[c.last].white),
                    "HDR probe step " + std::to_string(i) + ": attempts " + std::to_string(probe.hdr_attempts) + " matches " +
                        std::to_string(probe.hdr_matches) + " white " + std::to_string(probe.white));
            void *mapped{};
            hr(readback->Map(0, nullptr, &mapped), "map HDR readback");
            const auto *pixels = static_cast<const std::uint8_t *>(mapped);
            for (unsigned y = 0; y < height; ++y)
                for (unsigned x = 0; x < width; ++x) {
                    std::uint32_t packed{};
                    std::memcpy(&packed, pixels + y * fp.Footprint.RowPitch + x * 4, 4);
                    const auto &v = hdr_columns[x / (width / 4)];
                    const auto expected = screen::hdr::encode({v[0], v[1], v[2]}, used);
                    for (unsigned ch = 0; ch < 3; ++ch) {
                        const float actual = float((packed >> (10 * ch)) & 1023u);
                        require(std::abs(actual - expected[ch] * 1023.0f) <= 1.5f,
                                "HDR pixel step " + std::to_string(i) + " at " + std::to_string(x) + " channel " + std::to_string(ch) +
                                    ": " + std::to_string(actual) + " vs " + std::to_string(expected[ch] * 1023.0f));
                        ++hdr_checked;
                    }
                    require((packed >> 30) == 3u, "HDR alpha");
                }
            const D3D12_RANGE nothing{};
            readback->Unmap(0, &nothing);
        }
        // The SDR target after HDR steps: the SDR transform, HDR flags clear.
        pass.record(f.list.Get(), 0, scene.Get(), target.Get(), tag_state);
        f.execute();
        require(!pass.probe(0).hdr && pass.probe(0).parameters_ok, "SDR after HDR");
        f.messages();
        std::cout << "hudless_pass_warp: " << std::size(steps) << " steps, " << checked << " channels; HDR "
                  << std::size(hdr_steps) << " steps, " << hdr_checked << " channels\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "hudless_pass_warp: " << e.what() << '\n';
        return 1;
    }
}
