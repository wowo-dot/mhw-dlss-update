#include "../src/scaled_temporal/frame_handoff.hpp"
#include "../src/scaled_temporal/stream_lifetime.hpp"
#include "../src/temporal_present/output.hpp"
#include <windows.h>
#include <dxgi1_4.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using Microsoft::WRL::ComPtr;
using H = mhw::scaled_temporal::FrameHandoff;
void require(bool value, const char *why) {
    if (!value) throw std::runtime_error(why);
}
void hr(HRESULT value, const char *why) {
    require(SUCCEEDED(value), why);
}
void transition(ID3D12GraphicsCommandList *list, ID3D12Resource *resource, D3D12_RESOURCE_STATES from,
                D3D12_RESOURCE_STATES to) {
    D3D12_RESOURCE_BARRIER b{};
    b.Transition = {resource, UINT_MAX, from, to};
    list->ResourceBarrier(1, &b);
}
struct Device {
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12InfoQueue> info;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12Fence> fence;
    std::array<ComPtr<ID3D12CommandAllocator>, 3> allocators;
    std::array<ComPtr<ID3D12GraphicsCommandList>, 3> lists;
    DXGI_ADAPTER_DESC1 adapter{};
    std::uint64_t signal{};
    explicit Device(bool warp, unsigned luid) {
        ComPtr<ID3D12Debug> debug;
        hr(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)), "debug");
        debug->EnableDebugLayer();
        ComPtr<ID3D12Debug1> validation;
        hr(debug.As(&validation), "validation");
        validation->SetEnableGPUBasedValidation(TRUE);
        ComPtr<IDXGIFactory4> factory;
        ComPtr<IDXGIAdapter1> selected;
        hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "factory");
        if (warp)
            hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&selected)), "warp");
        else
            hr(factory->EnumAdapterByLuid({luid, 0}, IID_PPV_ARGS(&selected)), "adapter");
        hr(selected->GetDesc1(&adapter), "identity");
        hr(D3D12CreateDevice(selected.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "device");
        hr(device.As(&info), "info");
        hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "fence");
        D3D12_COMMAND_QUEUE_DESC q{};
        hr(device->CreateCommandQueue(&q, IID_PPV_ARGS(&queue)), "queue");
        for (unsigned i = 0; i < 3; ++i) {
            hr(device->CreateCommandAllocator(q.Type, IID_PPV_ARGS(&allocators[i])), "allocator");
            hr(device->CreateCommandList(0, q.Type, allocators[i].Get(), nullptr, IID_PPV_ARGS(&lists[i])), "list");
            hr(lists[i]->Close(), "initial close");
        }
    }
    ComPtr<ID3D12Resource> resource(D3D12_RESOURCE_DESC desc, D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES state) {
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = type;
        ComPtr<ID3D12Resource> result;
        hr(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&result)),
           "resource");
        return result;
    }
    void wait() {
        HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        require(event != nullptr, "event");
        const auto result = fence->SetEventOnCompletion(signal, event);
        const auto waited = SUCCEEDED(result) ? WaitForSingleObject(event, 30000) : WAIT_FAILED;
        CloseHandle(event);
        require(waited == WAIT_OBJECT_0, "GPU wait");
        hr(device->GetDeviceRemovedReason(), "device health");
    }
    void messages() {
        unsigned errors{};
        for (UINT64 i = 0; i < info->GetNumStoredMessages(); ++i) {
            SIZE_T size{};
            hr(info->GetMessage(i, nullptr, &size), "message size");
            std::vector<std::byte> bytes(size);
            auto *message = reinterpret_cast<D3D12_MESSAGE *>(bytes.data());
            hr(info->GetMessage(i, message, &size), "message");
            if (message->Severity <= D3D12_MESSAGE_SEVERITY_WARNING) {
                std::cerr << message->pDescription << '\n';
                ++errors;
            }
        }
        require(!errors, "GPU warnings/errors");
    }
};
int wmain(int argc, wchar_t **argv) {
    try {
        require(argc == 4, "usage: warp|hardware output.json adapter-luid-low");
        const std::filesystem::path output = argv[2];
        require(!std::filesystem::exists(output), "preserve evidence");
        Device f(std::wstring_view(argv[1]) == L"warp", std::stoul(argv[3]));
        constexpr unsigned width = 128, height = 64;
        D3D12_RESOURCE_DESC d{};
        d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        d.Width = width;
        d.Height = height;
        d.DepthOrArraySize = d.MipLevels = 1;
        d.Format = DXGI_FORMAT_R11G11B10_FLOAT;
        d.SampleDesc.Count = 1;
        auto raw = f.resource(d, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);
        auto reconstructed = f.resource(d, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);
        auto a = f.resource(d, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);
        auto b = f.resource(d, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);
        d.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        auto resolved = f.resource(d, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT64 bytes{};
        f.device->GetCopyableFootprints(&d, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);
        D3D12_RESOURCE_DESC bd{};
        bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bd.Width = bytes;
        bd.Height = 1;
        bd.DepthOrArraySize = bd.MipLevels = 1;
        bd.SampleDesc.Count = 1;
        bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        auto raw_upload = f.resource(bd, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        auto full_upload = f.resource(bd, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        auto readback = f.resource(bd, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
        std::array<std::unique_ptr<mhw::temporal_present::Output>, 2> recovery;
        for (unsigned i = 0; i < 2; ++i) {
            recovery[i] = std::make_unique<mhw::temporal_present::Output>(
                f.device.Get(), mhw::temporal_present::SpatialSource{width, height, width / (i + 1), height / (i + 1)},
                width, height, true, true);
            recovery[i]->bind(raw.Get(), resolved.Get());
        }
        mhw::scaled_temporal::StreamLifetime lifetime;
        unsigned frames{}, fallbacks{}, reconstructed_frames{}, split_batches{}, early_consumers{};
        std::uint64_t pixels{};
        for (unsigned ratio = 1; ratio <= 2; ++ratio)
            for (unsigned success = 0; success < 2; ++success)
                for (unsigned split = 0; split < 3; ++split) {
                    std::array<unsigned, 3> order{0, 1, 2};
                    do {
                        ++frames;
                        const unsigned low_value =
                            ((15 + frames % 3) << 6) | (((14 + frames % 2) << 6) << 11) | ((13U << 5) << 22);
                        const unsigned high_value =
                            ((12 + frames % 3) << 6) | (((16 + frames % 2) << 6) << 11) | ((15U << 5) << 22);
                        const auto upload = [&](ID3D12Resource *resource, bool full) {
                            void *mapped{};
                            D3D12_RANGE none{};
                            hr(resource->Map(0, &none, &mapped), "map upload");
                            for (unsigned y = 0; y < height; ++y)
                                for (unsigned x = 0; x < width; ++x) {
                                    // Large, valid finite padding makes an accidental low-rect
                                    // consumer or a padding sample fail the exact comparison.
                                    const unsigned value =
                                        full ? high_value
                                             : (x < width / ratio && y < height / ratio ? low_value : 0x781e03c0U);
                                    std::memcpy(static_cast<std::byte *>(mapped) + y * footprint.Footprint.RowPitch +
                                                    x * 4,
                                                &value, 4);
                                }
                            resource->Unmap(0, nullptr);
                        };
                        upload(raw_upload.Get(), false);
                        upload(full_upload.Get(), true);
                        H handoff;
                        const std::array<std::uintptr_t, 5> packets{11, 12, 13, 14, 15};
                        require(handoff.prepare({frames, 1, 1, 2}, packets) == H::Result::ok, "prepare handoff");
                        std::array<H::Recording, 3> recordings{};
                        for (unsigned i = 0; i < 3; ++i) {
                            hr(f.allocators[i]->Reset(), "allocator reset");
                            hr(f.lists[i]->Reset(f.allocators[i].Get(), nullptr), "recording reset");
                            recordings[i] = {reinterpret_cast<std::uintptr_t>(f.lists[i].Get()), frames};
                        }
                        const auto retained = [&](unsigned i) {
                            std::array<IUnknown *, 7> pins{raw.Get(), reconstructed.Get(), resolved.Get(),   a.Get(),
                                                           b.Get(),   raw_upload.Get(),    full_upload.Get()};
                            require(lifetime.retain(f.lists[i].Get(), frames, pins), "retain per recording");
                        };
                        const auto mark = [&](unsigned node, unsigned list) {
                            require(handoff.record(node, packets[node], recordings[list]) == H::Result::ok,
                                    "record node");
                        };
                        for (unsigned i : order) {
                            auto *list = f.lists[i].Get();
                            retained(i);
                            if (i == 0) {
                                D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
                                dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                                src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                                src.PlacedFootprint = footprint;
                                dst.pResource = raw.Get();
                                src.pResource = raw_upload.Get();
                                list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
                                if (success) {
                                    dst.pResource = reconstructed.Get();
                                    src.pResource = full_upload.Get();
                                    list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
                                    transition(list, reconstructed.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                               D3D12_RESOURCE_STATE_COPY_SOURCE);
                                    transition(list, resolved.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                               D3D12_RESOURCE_STATE_COPY_DEST);
                                    list->CopyResource(resolved.Get(), reconstructed.Get());
                                    transition(list, resolved.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                               D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                                    transition(list, reconstructed.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                                               D3D12_RESOURCE_STATE_COPY_DEST);
                                    ++reconstructed_frames;
                                } else {
                                    recovery[ratio - 1]->record(list, D3D12_RESOURCE_STATE_COPY_DEST);
                                    ++fallbacks;
                                }
                                mark(0, i);
                                transition(list, resolved.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                           D3D12_RESOURCE_STATE_COPY_SOURCE);
                                list->CopyResource(a.Get(), resolved.Get());
                                transition(list, resolved.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                                           D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                                transition(list, a.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                           D3D12_RESOURCE_STATE_COPY_SOURCE);
                                mark(1, i);
                            } else if (i == 1) {
                                if (handoff.recorded() < 2) ++early_consumers;
                                list->CopyResource(b.Get(), a.Get());
                                transition(list, a.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                                           D3D12_RESOURCE_STATE_COPY_DEST);
                                transition(list, b.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                           D3D12_RESOURCE_STATE_COPY_SOURCE);
                                mark(2, i);
                            } else {
                                if (handoff.recorded() < 2) ++early_consumers;
                                list->CopyResource(a.Get(), b.Get());
                                transition(list, b.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                                           D3D12_RESOURCE_STATE_COPY_DEST);
                                transition(list, a.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                           D3D12_RESOURCE_STATE_COPY_SOURCE);
                                mark(3, i);
                                D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
                                dst.pResource = readback.Get();
                                dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                                dst.PlacedFootprint = footprint;
                                src.pResource = a.Get();
                                src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                                list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
                                transition(list, a.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                                           D3D12_RESOURCE_STATE_COPY_DEST);
                                mark(4, i);
                            }
                            hr(list->Close(), "close recording");
                        }
                        require(handoff.choose(success ? H::Choice::reconstructed : H::Choice::spatial) ==
                                    H::Result::ok,
                                "seal output choice");
                        unsigned end{};
                        const std::array<H::Recording, 3> reversed{recordings[2], recordings[1], recordings[0]};
                        require(handoff.validate_submission(reinterpret_cast<std::uintptr_t>(f.queue.Get()), reversed,
                                                            f.signal + 1, end) == H::Result::submission_order,
                                "reject reversed GPU order before submit");
                        for (unsigned start = 0; start < 3;) {
                            const unsigned count = split == 0 ? 3 : split == 1 ? (start == 0 ? 1 : 2) : 1;
                            auto batch = std::span(recordings).subspan(start, count);
                            require(handoff.validate_submission(reinterpret_cast<std::uintptr_t>(f.queue.Get()), batch,
                                                                f.signal + 1, end) == H::Result::ok,
                                    "validate GPU prefix");
                            std::array<ID3D12CommandList *, 3> api{};
                            for (unsigned i = 0; i < count; ++i) api[i] = f.lists[start + i].Get();
                            f.queue->ExecuteCommandLists(count, api.data());
                            hr(f.queue->Signal(f.fence.Get(), ++f.signal), "signal");
                            require(handoff.submitted(reinterpret_cast<std::uintptr_t>(f.queue.Get()), batch,
                                                      f.signal) == H::Result::ok,
                                    "submission receipt");
                            for (unsigned i = 0; i < count; ++i)
                                require(lifetime.submitted(f.lists[start + i].Get(), frames, f.signal),
                                        "pin submission");
                            start += count;
                            if (start < 3) ++split_batches;
                        }
                        f.wait();
                        lifetime.collect(f.fence->GetCompletedValue());
                        require(lifetime.live() == 3 && !handoff.retired(f.signal),
                                "fence alone does not release recordings");
                        void *mapped{};
                        D3D12_RANGE range{0, static_cast<SIZE_T>(bytes)};
                        hr(readback->Map(0, &range, &mapped), "readback");
                        for (unsigned y = 0; y < height; ++y)
                            for (unsigned x = 0; x < width; ++x) {
                                unsigned value{};
                                std::memcpy(&value,
                                            static_cast<std::byte *>(mapped) + y * footprint.Footprint.RowPitch + x * 4,
                                            4);
                                require(value == (success ? high_value : low_value), "full output exact pixels");
                                ++pixels;
                            }
                        D3D12_RANGE none{};
                        readback->Unmap(0, &none);
                        for (unsigned i = 0; i < 3; ++i) {
                            hr(f.lists[i]->Reset(f.allocators[i].Get(), nullptr), "retire reset");
                            hr(f.lists[i]->Close(), "retire close");
                            require(handoff.reset(recordings[i]) == H::Result::ok &&
                                        lifetime.reset(f.lists[i].Get(), frames),
                                    "reset receipt");
                        }
                        lifetime.collect(f.fence->GetCompletedValue());
                        require(!lifetime.live() && handoff.retired(f.signal), "all recordings retired");
                    } while (std::next_permutation(order.begin(), order.end()));
                }
        for (auto &r : recovery) r->retire(f.fence.Get(), f.signal, true);
        f.messages();
        std::ofstream log(output);
        log << "{\"accepted\":true,\"synthetic_resolve\":true,\"native_game_test\":false,\"sdk_evaluated\":false,\"frames\":"
            << frames << ",\"spatial_frames\":" << fallbacks
            << ",\"synthetic_reconstructed_frames\":" << reconstructed_frames << ",\"pixels\":" << pixels
            << ",\"split_batches\":" << split_batches << ",\"early_consumer_recordings\":" << early_consumers
            << ",\"retained\":" << lifetime.retained() << ",\"released\":" << lifetime.released()
            << ",\"live\":" << lifetime.live() << ",\"signals\":" << f.signal
            << ",\"gpu_warnings_errors\":0,\"adapter_luid\":[" << f.adapter.AdapterLuid.HighPart << ','
            << f.adapter.AdapterLuid.LowPart << "],\"vendor\":" << f.adapter.VendorId << "}\n";
        require(bool(log), "report");
        std::cout << "PASS " << frames << " reordered CPU frames, " << pixels
                  << " exact full-output pixels, spatial recovery and retirement\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
