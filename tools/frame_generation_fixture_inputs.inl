// Included inside the owned fixture's private namespace. Static color/depth,
// empty UI agree by construction; this is not a game capture.
struct OwnedFgInputs {
    struct Texture { ComPtr<ID3D12Resource> resource, upload; D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{}; };
    std::array<Texture, 4> textures;
    sl::Constants camera{};
    sl::Extent extent{0, 0, 1280, 720};
    OwnedFgInputs(ID3D12Device* device, ID3D12GraphicsCommandList* list) {
        const DXGI_FORMAT formats[]{DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT,
            DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_R8_UNORM};
        D3D12_HEAP_PROPERTIES gpu{}, cpu{}; gpu.Type = D3D12_HEAP_TYPE_DEFAULT; cpu.Type = D3D12_HEAP_TYPE_UPLOAD;
        for (unsigned kind = 0; kind < textures.size(); ++kind) {
            auto& t = textures[kind];
            D3D12_RESOURCE_DESC desc{}; desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Width = extent.width; desc.Height = extent.height; desc.DepthOrArraySize = 1;
            desc.MipLevels = 1; desc.Format = formats[kind]; desc.SampleDesc.Count = 1;
            hr(device->CreateCommittedResource(&gpu, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr, IID_PPV_ARGS(&t.resource)), "owned FG input");
            UINT64 size{}; device->GetCopyableFootprints(&desc, 0, 1, 0, &t.layout, nullptr, nullptr, &size);
            D3D12_RESOURCE_DESC buffer{}; buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            buffer.Width = size; buffer.Height = 1; buffer.DepthOrArraySize = 1; buffer.MipLevels = 1;
            buffer.SampleDesc.Count = 1; buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            hr(device->CreateCommittedResource(&cpu, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr, IID_PPV_ARGS(&t.upload)), "owned FG input upload");
            void* mapped{}; const D3D12_RANGE no_reads{0, 0}; hr(t.upload->Map(0, &no_reads, &mapped), "map FG input");
            for (unsigned y = 0; y < extent.height; ++y) {
                auto* row = static_cast<std::byte*>(mapped) + t.layout.Offset + static_cast<SIZE_T>(y) * t.layout.Footprint.RowPitch;
                for (unsigned x = 0; x < extent.width; ++x) {
                    if (kind == 0) reinterpret_cast<float*>(row)[x] = 0.5f;
                    if (kind == 1) { reinterpret_cast<float*>(row)[x * 2] = 0; reinterpret_cast<float*>(row)[x * 2 + 1] = 0; }
                    if (kind == 2) reinterpret_cast<std::uint32_t*>(row)[x] = 0xff224466;
                    if (kind == 3) reinterpret_cast<std::uint8_t*>(row)[x] = 0;
                }
            }
            const D3D12_RANGE written{0, static_cast<SIZE_T>(size)}; t.upload->Unmap(0, &written);
            D3D12_TEXTURE_COPY_LOCATION dst{}, src{}; dst.pResource = t.resource.Get(); dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            src.pResource = t.upload.Get(); src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; src.PlacedFootprint = t.layout;
            list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
            D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = t.resource.Get(); barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            list->ResourceBarrier(1, &barrier);
        }
        const auto set = [](sl::float4x4& dst, DirectX::FXMMATRIX src) {
            DirectX::XMFLOAT4X4 m{}; DirectX::XMStoreFloat4x4(&m, src);
            for (unsigned y = 0; y < 4; ++y) dst[y] = {m.m[y][0], m.m[y][1], m.m[y][2], m.m[y][3]};
        };
        const auto projection = DirectX::XMMatrixPerspectiveFovLH(1.04719755f, 1280.0f / 720.0f, 0.1f, 1000.0f);
        set(camera.cameraViewToClip, projection); set(camera.clipToCameraView, DirectX::XMMatrixInverse(nullptr, projection));
        set(camera.clipToPrevClip, DirectX::XMMatrixIdentity()); set(camera.prevClipToClip, DirectX::XMMatrixIdentity());
        set(camera.clipToLensClip, DirectX::XMMatrixIdentity());
        camera.jitterOffset = {0, 0}; camera.mvecScale = {1, 1}; camera.cameraPinholeOffset = {0, 0};
        camera.cameraPos = {0, 0, 0}; camera.cameraRight = {1, 0, 0}; camera.cameraUp = {0, 1, 0}; camera.cameraFwd = {0, 0, 1};
        camera.cameraNear = 0.1f; camera.cameraFar = 1000.0f; camera.cameraFOV = 1.04719755f; camera.cameraAspectRatio = 1280.0f / 720.0f;
        camera.depthInverted = sl::eFalse; camera.cameraMotionIncluded = sl::eTrue; camera.motionVectors3D = sl::eFalse;
    }
    void tag(mhw::reflex::Streamline& sdk, const sl::FrameToken& token, const sl::ViewportHandle& viewport, unsigned frame) {
        camera.reset = frame == 1 || frame == 33 ? sl::eTrue : sl::eFalse;
        api(sdk.constants(camera, token, viewport), "same Reflex-token constants");
        std::array<sl::Resource, 4> r;
        for (unsigned i = 0; i < r.size(); ++i)
            r[i] = {sl::ResourceType::eTex2d, textures[i].resource.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};
        sl::ResourceTag tags[]{
            {frame >= 129 ? nullptr : &r[0], sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent, &extent},
            {frame >= 129 ? nullptr : &r[1], sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent, &extent},
            {frame >= 129 ? nullptr : &r[2], sl::kBufferTypeHUDLessColor, sl::ResourceLifecycle::eValidUntilPresent, &extent},
            {frame >= 129 ? nullptr : &r[3], sl::kBufferTypeUIAlpha, sl::ResourceLifecycle::eValidUntilPresent, &extent},
            {nullptr, sl::kBufferTypeBackbuffer, sl::ResourceLifecycle::eValidUntilPresent, &extent}
        };
        api(sdk.tags(token, viewport, tags, 5), "same Reflex-token immutable tags");
    }
    // Previous Present's input-consumption/host queue fences must complete
    // before calling this. The same queue records all writes before Present.
    void update(ID3D12GraphicsCommandList* list, unsigned frame) {
        for (unsigned kind = 0; kind < textures.size(); ++kind) {
            auto& t = textures[kind]; void* mapped{}; const D3D12_RANGE no_reads{0, 0};
            hr(t.upload->Map(0, &no_reads, &mapped), "update synthetic input");
            for (unsigned y = 0; y < extent.height; ++y) {
                auto* row = static_cast<std::byte*>(mapped) + t.layout.Offset + static_cast<SIZE_T>(y) * t.layout.Footprint.RowPitch;
                for (unsigned x = 0; x < extent.width; ++x) {
                    const bool object = x >= 100 + frame * 2 && x < 228 + frame * 2 && y >= 200 && y < 328;
                    if (kind == 0) reinterpret_cast<float*>(row)[x] = object ? 0.5f : 0.75f;
                    if (kind == 1) { reinterpret_cast<float*>(row)[x * 2] = object ? -2.0f / extent.width : 0;
                        reinterpret_cast<float*>(row)[x * 2 + 1] = 0; }
                    if (kind == 2) reinterpret_cast<std::uint32_t*>(row)[x] = object ? 0xffff8844 :
                        (((x / 32 + y / 32) & 1U) ? 0xff445588 : 0xff224466);
                    if (kind == 3) reinterpret_cast<std::uint8_t*>(row)[x] = 0;
                }
            }
            t.upload->Unmap(0, nullptr);
            D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = t.resource.Get(); barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST; list->ResourceBarrier(1, &barrier);
            D3D12_TEXTURE_COPY_LOCATION dst{}, src{}; dst.pResource = t.resource.Get(); dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            src.pResource = t.upload.Get(); src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; src.PlacedFootprint = t.layout;
            list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
            std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter); list->ResourceBarrier(1, &barrier);
        }
    }
    void release() noexcept { for (auto& t : textures) { t.upload.Reset(); t.resource.Reset(); } }
};
