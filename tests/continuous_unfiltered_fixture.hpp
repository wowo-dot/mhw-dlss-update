#pragma once
// Exercise the real unfiltered replacement and runtime submission/Reset hooks
// on WARP. No game, private depth lease, or NVIDIA initialization is involved.
void unfiltered_fixture() {
    using Microsoft::WRL::ComPtr;
    ComPtr<ID3D12Debug> debug;
    check(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))), "debug");
    debug->EnableDebugLayer();
    ComPtr<IDXGIFactory4> factory;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<ID3D12Device> device;
    check(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) &&
              SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))) &&
              SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))),
          "WARP device");
    cd::cfg.device = reinterpret_cast<std::uintptr_t>(device.Get());
    cd::accepting = true;
    cd::runtime.width = 17;
    cd::runtime.height = 13;
    check(cd::unfiltered_lifetime.initialize(device.Get()), "independent fence");
    cd::obs::begin();
    D3D12_COMMAND_QUEUE_DESC q{};
    std::array<ComPtr<ID3D12CommandQueue>, 2> queues;
    std::array<ComPtr<ID3D12CommandAllocator>, 3> allocators;
    std::array<ComPtr<ID3D12GraphicsCommandList>, 2> lists;
    for (auto &a : allocators) check(SUCCEEDED(device->CreateCommandAllocator(q.Type, IID_PPV_ARGS(&a))), "allocator");
    for (unsigned i = 0; i < 2; ++i)
        check(SUCCEEDED(device->CreateCommandQueue(&q, IID_PPV_ARGS(&queues[i]))) &&
                  SUCCEEDED(
                      device->CreateCommandList(0, q.Type, allocators[i].Get(), nullptr, IID_PPV_ARGS(&lists[i]))) &&
                  SUCCEEDED(lists[i]->Close()),
              "queue/list");
    auto table = *reinterpret_cast<void ***>(lists[0].Get());
    cd::originals[5] = table[10];
    cd::originals[6] = table[9];
    cd::originals[7] = (*reinterpret_cast<void ***>(queues[0].Get()))[10];
    D3D12_RESOURCE_DESC tex{};
    tex.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    tex.Width = 17;
    tex.Height = 13;
    tex.DepthOrArraySize = tex.MipLevels = 1;
    tex.SampleDesc.Count = 1;
    tex.Format = DXGI_FORMAT_R11G11B10_FLOAT;
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    ComPtr<ID3D12Resource> color, output;
    check(SUCCEEDED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &tex, D3D12_RESOURCE_STATE_COPY_DEST,
                                                    nullptr, IID_PPV_ARGS(&color))),
          "raw color");
    tex.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    check(SUCCEEDED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &tex,
                                                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                                                    IID_PPV_ARGS(&output))),
          "native output");
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT64 bytes{};
    device->GetCopyableFootprints(&tex, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);
    D3D12_RESOURCE_DESC buf{};
    buf.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buf.Width = bytes;
    buf.Height = buf.DepthOrArraySize = buf.MipLevels = 1;
    buf.SampleDesc.Count = 1;
    buf.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> upload, readback;
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    check(SUCCEEDED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buf,
                                                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload))),
          "upload");
    heap.Type = D3D12_HEAP_TYPE_READBACK;
    check(SUCCEEDED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buf, D3D12_RESOURCE_STATE_COPY_DEST,
                                                    nullptr, IID_PPV_ARGS(&readback))),
          "readback");
    void *mapped{};
    D3D12_RANGE empty{0, 0};
    check(SUCCEEDED(upload->Map(0, &empty, &mapped)), "map upload");
    const auto pixel = [](unsigned x, unsigned y) { return 0x701c0380U + x + 17 * y; };
    for (unsigned y = 0; y < 13; ++y)
        for (unsigned x = 0; x < 17; ++x)
            std::memcpy(static_cast<std::byte *>(mapped) + y * footprint.Footprint.RowPitch + 4 * x,
                        &(std::array{pixel(x, y)})[0], 4);
    upload->Unmap(0, nullptr);
    const auto barrier = [](ID3D12GraphicsCommandList *list, ID3D12Resource *r, D3D12_RESOURCE_STATES a,
                            D3D12_RESOURCE_STATES b) {
        D3D12_RESOURCE_BARRIER v{};
        v.Transition = {r, UINT_MAX, a, b};
        list->ResourceBarrier(1, &v);
    };
    const auto wait = [&](std::uint64_t value) {
        const auto event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        check(event && SUCCEEDED(cd::unfiltered_lifetime.fence()->SetEventOnCompletion(value, event)) &&
                  WaitForSingleObject(event, 5000) == WAIT_OBJECT_0,
              "completion");
        CloseHandle(event);
    };
    for (unsigned round = 0; round < 2; ++round) {
        for (unsigned i = 0; i < 2; ++i) {
            auto *list = lists[i].Get();
            check(SUCCEEDED(allocators[i]->Reset()) && SUCCEEDED(cd::reset_hook(list, allocators[i].Get(), nullptr)),
                  "recording reset");
            if (round == 0 && i == 0) {
                D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
                dst.pResource = color.Get();
                src.pResource = upload.Get();
                src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                src.PlacedFootprint = footprint;
                list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
                barrier(list, color.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            }
            const cd::ResourcePins pins{color, output, color, output};
            const auto generation = cd::find(list)->generation;
            check(cd::unfiltered_lifetime.retain(list, generation, pins) &&
                      cd::unfiltered_lifetime.retain(list, generation, pins),
                  "same recording retains exact resources once");
            check(!cd::unfiltered_lifetime.reset(list, generation), "cannot discard an unsubmitted copy");
            cd::consumer.resources = pins;
            cd::consumer.resources[2] = output;
            cd::prepared_resolve = {};
            check(cd::obs::inspect(list, cd::prepared_resolve.bindings), "owned state snapshot");
            check(cd::dispatch_replace(list).outcome == cd::ResolveOutcome::unfiltered && !cd::lease.list && !cd::pass,
                  "actual replacement needs no SDK/depth lease");
            if (i == 1) {
                barrier(list, output.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
                D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
                src.pResource = output.Get();
                dst.pResource = readback.Get();
                dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                dst.PlacedFootprint = footprint;
                list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
                barrier(list, output.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            }
            check(SUCCEEDED(cd::close_hook(list)), "close copy");
        }
        ID3D12CommandList *batch[]{lists[0].Get(), lists[1].Get()};
        cd::UnfilteredLifetime::Submission rejected;
        ID3D12CommandList *duplicate[]{lists[0].Get(), lists[0].Get()};
        check(!cd::unfiltered_lifetime.before(queues[round].Get(), duplicate, rejected),
              "duplicate batch rejected before execution");
        ComPtr<ID3D12Fence> gate;
        check(SUCCEEDED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gate))) &&
                  SUCCEEDED(queues[round]->Wait(gate.Get(), 1)),
              "hold GPU");
        cd::execute_hook(queues[round].Get(), 2, batch);
        check(cd::unfiltered_lifetime.completed() == round && cd::unfiltered_lifetime.live() == 2,
              "two retained lists with one pending signal");
        check(!cd::unfiltered_lifetime.before(queues[round].Get(), batch, rejected), "replay rejected");
        check(SUCCEEDED(cd::reset_hook(lists[0].Get(), allocators[2].Get(), nullptr)) &&
                  SUCCEEDED(cd::close_hook(lists[0].Get())),
              "Reset while original allocator is busy");
        cd::unfiltered_lifetime.collect();
        check(cd::unfiltered_lifetime.live() == 2, "Reset cannot release pending GPU resources");
        // Complete the GPU between serialization of the fence and the pin
        // counters. Both must still describe the one earlier observation.
        struct AdvanceOnRecords : std::stringbuf {
            ID3D12Fence *gate, *completion;
            std::uint64_t value;
            bool advanced{};
            AdvanceOnRecords(ID3D12Fence *g, ID3D12Fence *c, std::uint64_t v) : gate(g), completion(c), value(v) {}
            std::streamsize xsputn(const char *s, std::streamsize n) override {
                if (!advanced && std::string_view(s, static_cast<std::size_t>(n)).find("\"recordings\":") !=
                                     std::string_view::npos) {
                    advanced = true;
                    check(SUCCEEDED(gate->Signal(1)), "complete during telemetry write");
                    const auto event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
                    check(event && SUCCEEDED(completion->SetEventOnCompletion(value, event)) &&
                              WaitForSingleObject(event, 5000) == WAIT_OBJECT_0,
                          "telemetry completion");
                    CloseHandle(event);
                }
                return std::stringbuf::xsputn(s, n);
            }
        } advancing(gate.Get(), cd::unfiltered_lifetime.fence(), round + 1);
        std::ostream snapshot(&advancing);
        cd::unfiltered_lifetime.write(snapshot);
        check(advancing.advanced && cd::unfiltered_lifetime.completed() == round + 1, "GPU advanced inside snapshot");
        const auto text = advancing.str();
        check(text.find("\"completed_value\":" + std::to_string(round) + ",") != std::string::npos &&
                  text.find("\"awaiting_gpu\":2") != std::string::npos && text.find("\"live\":2") != std::string::npos,
              "one fence sample for collection and counters");
        std::ostringstream frozen;
        cd::unfiltered_lifetime.write(frozen, false);
        check(cd::unfiltered_lifetime.live() == 2, "fault snapshot must preserve completed pins");
        wait(round + 1);
        cd::unfiltered_lifetime.collect();
        check(cd::unfiltered_lifetime.live() == 1, "GPU completion does not release replayable second recording");
        check(SUCCEEDED(readback->Map(0, nullptr, &mapped)), "map output");
        for (unsigned y = 0; y < 13; ++y)
            for (unsigned x = 0; x < 17; ++x) {
                unsigned actual{};
                std::memcpy(&actual, static_cast<std::byte *>(mapped) + y * footprint.Footprint.RowPitch + 4 * x, 4);
                check(actual == pixel(x, y), "unfiltered output is bit-identical current color");
            }
        readback->Unmap(0, &empty);
        check(SUCCEEDED(cd::reset_hook(lists[1].Get(), allocators[1].Get(), nullptr)) &&
                  SUCCEEDED(cd::close_hook(lists[1].Get())),
              "retire replayable list");
        cd::unfiltered_lifetime.collect();
        check(cd::unfiltered_lifetime.live() == 0 && cd::healthy(), "all references retired without depth/SDK state");
    }
    // An unrelated queue/batch must not acquire device ownership or fault.
    cd::cfg.device = 0;
    cd::execute_hook(queues[0].Get(), 0, nullptr);
    check(cd::healthy(), "untracked submission is unrelated");
    ComPtr<ID3D12InfoQueue> messages;
    check(SUCCEEDED(device.As(&messages)), "validation");
    for (UINT64 i = 0; i < messages->GetNumStoredMessagesAllowedByRetrievalFilter(); ++i) {
        SIZE_T size{};
        messages->GetMessage(i, nullptr, &size);
        std::vector<std::byte> data(size);
        auto *m = reinterpret_cast<D3D12_MESSAGE *>(data.data());
        messages->GetMessage(i, m, &size);
        if (m->Severity <= D3D12_MESSAGE_SEVERITY_WARNING) std::cerr << m->pDescription << '\n';
        check(m->Severity > D3D12_MESSAGE_SEVERITY_WARNING, "clean GPU validation");
    }
    std::cout << "{\"unfiltered_lifetime\":";
    cd::unfiltered_lifetime.write(std::cout);
    std::cout << "}\nUnfiltered GPU copy/Reset/fence/cross-queue tests passed\n";
}
