#include "inline.hpp"
#include "../temporal_present/output.hpp"
#include "../temporal_gpu_inputs/shader.hpp"
#include "../perf/toggles.hpp"
#include "../astera/astera_hooks.hpp"
#include <nvsdk_ngx_helpers.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cmath>
#include <iomanip>
#include <stdexcept>

namespace mhw::dlss_sdk {
namespace {
using Microsoft::WRL::ComPtr;
void require(bool ok, const char *why) {
    if (!ok) throw std::runtime_error(why);
}
void hr(HRESULT code, const char *why) {
    require(SUCCEEDED(code), why);
}
void same_device(ID3D12DeviceChild *object, ID3D12Device *device) {
    require(object && device, "inline null device child");
    ComPtr<ID3D12Device> d;
    ComPtr<IUnknown> a, b;
    hr(object->GetDevice(IID_PPV_ARGS(&d)), "inline device query");
    hr(d.As(&a), "inline device identity");
    hr(device->QueryInterface(IID_PPV_ARGS(&b)), "inline expected device");
    require(a == b, "inline foreign device");
}
D3D12_RESOURCE_DESC texture(unsigned w, unsigned h, DXGI_FORMAT f) {
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width = w;
    d.Height = h;
    d.DepthOrArraySize = d.MipLevels = 1;
    d.Format = f;
    d.SampleDesc.Count = 1;
    d.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    return d;
}
ComPtr<ID3D12Resource> resource(ID3D12Device *device, const D3D12_RESOURCE_DESC &d, D3D12_HEAP_TYPE type,
                                D3D12_RESOURCE_STATES state) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = type;
    ComPtr<ID3D12Resource> r;
    hr(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, state, nullptr, IID_PPV_ARGS(&r)),
       "inline allocation");
    return r;
}
void transition(ID3D12GraphicsCommandList *list, ID3D12Resource *r, D3D12_RESOURCE_STATES from,
                D3D12_RESOURCE_STATES to) {
    D3D12_RESOURCE_BARRIER b{};
    b.Transition = {r, UINT_MAX, from, to};
    list->ResourceBarrier(1, &b);
}
} // namespace
struct InlineFrame::Impl {
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> create_list;
    ComPtr<ID3D12Fence> create_fence;
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> pipeline;
    ComPtr<ID3D12DescriptorHeap> heap, clear_heap;
    std::array<ComPtr<ID3D12Resource>, 3> raw, converted;
    ComPtr<ID3D12Resource> output;
    std::unique_ptr<temporal_present::Output> handoff;
    NVSDK_NGX_Parameter *parameters{};
    NVSDK_NGX_Handle *feature{};
    unsigned width{}, height{}, create_result{}, evaluate_result{}, release_result{}, destroy_result{};
    Geometry geometry;
    Extent optimal{}, minimum{}, maximum{};
    unsigned optimal_result{}, quality{};
    float sharpness{};
    bool creation_completed{}, attempted{}, issued{}, restored{}, copied{}, released{};
    bool reset{true};
    float frame_ms{};
    std::uintptr_t native_identity{}, list_identity{};
    RecordAttempt last;
    std::uint64_t evaluations{}, resets{};
    HistoryFrame history;
    std::uint64_t generation{}, allocation_bytes{}, completion_fence{}, completion_value{};
    std::string failure;
};
static_assert(max_side == D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION);
InlineFrame::InlineFrame(ID3D12Device *device, Geometry geometry, const std::array<ID3D12Resource *, 3> &raw,
                         const std::filesystem::path &runtime, const BorrowedInitialization &initialization,
                         std::uint64_t byte_limit)
    : impl_(std::make_unique<Impl>()) {
    auto &p = *impl_;
    try {
        require(device && device->GetNodeCount() == 1 && valid_geometry(geometry), "inline extent/device");
        const auto w = geometry.render.width, h = geometry.render.height, ow = geometry.output.width,
                   oh = geometry.output.height;
        p.geometry = geometry;
        require(initialization.validate && initialization.validate(device, runtime), "inline borrowed initialization");
        p.device = device;
        p.width = w;
        p.height = h;
        constexpr DXGI_FORMAT inputs[]{DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_UINT};
        constexpr DXGI_FORMAT outputs[]{DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32_FLOAT,
                                        DXGI_FORMAT_R32G32_FLOAT};
        for (auto format : inputs) {
            D3D12_FEATURE_DATA_FORMAT_SUPPORT support{format};
            hr(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)),
               "inline input format query");
            require((support.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D) &&
                        (support.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_LOAD),
                    "inline input typed load");
        }
        for (auto format : outputs) {
            D3D12_FEATURE_DATA_FORMAT_SUPPORT support{format};
            hr(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)),
               "inline output format query");
            require((support.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D) &&
                        (support.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_LOAD) &&
                        (support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE),
                    "inline output typed access");
        }
        for (unsigned i = 0; i < 3; ++i) {
            same_device(raw[i], device);
            const auto d = raw[i]->GetDesc();
            require(d.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && d.Width == geometry.allocation.width &&
                        d.Height == geometry.allocation.height && d.DepthOrArraySize == 1 && d.MipLevels == 1 &&
                        d.SampleDesc.Count == 1 && d.SampleDesc.Quality == 0 && d.Format == inputs[i] && d.Flags == 0,
                    "inline input layout");
            for (unsigned j = 0; j < i; ++j) require(raw[i] != raw[j], "inline input alias");
            p.raw[i] = raw[i];
            auto desc = texture(w, h, outputs[i]);
            p.converted[i] = resource(device, desc, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }
        auto out = texture(ow, oh, DXGI_FORMAT_R32G32B32A32_FLOAT);
        p.output = resource(device, out, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        // The packed handoff to the game's output: continuous, the caller
        // pinning its native outputs and lists.
        p.handoff = std::make_unique<temporal_present::Output>(device, ow, oh, true, true);
        const auto account = [&](ID3D12Resource *r) {
            const auto desc = r->GetDesc();
            const auto bytes = device->GetResourceAllocationInfo(0, 1, &desc).SizeInBytes;
            require(bytes && bytes != UINT64_MAX && bytes <= byte_limit - p.allocation_bytes,
                    "inline image allocation budget");
            p.allocation_bytes += bytes;
        };
        for (const auto &r : p.converted) account(r.Get());
        account(p.output.Get());
        account(p.handoff->packed());
        D3D12_DESCRIPTOR_HEAP_DESC hd{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 7,
                                      D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0};
        hr(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&p.heap)), "inline heap");
        const auto step = device->GetDescriptorHandleIncrementSize(hd.Type);
        const auto start = p.heap->GetCPUDescriptorHandleForHeapStart().ptr;
        for (unsigned i = 0; i < 3; ++i) {
            D3D12_SHADER_RESOURCE_VIEW_DESC s{};
            s.Format = inputs[i];
            s.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            s.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            s.Texture2D.MipLevels = 1;
            device->CreateShaderResourceView(p.raw[i].Get(), &s, {start + SIZE_T(i) * step});
            D3D12_UNORDERED_ACCESS_VIEW_DESC u{};
            u.Format = outputs[i];
            u.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            device->CreateUnorderedAccessView(p.converted[i].Get(), nullptr, &u, {start + SIZE_T(i + 3) * step});
        }
        hd.NumDescriptors = 1;
        hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        hr(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&p.clear_heap)), "inline clear heap");
        D3D12_UNORDERED_ACCESS_VIEW_DESC clear{};
        clear.Format = out.Format;
        clear.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(p.output.Get(), nullptr, &clear, {start + SIZE_T(6) * step});
        device->CreateUnorderedAccessView(p.output.Get(), nullptr, &clear,
                                          p.clear_heap->GetCPUDescriptorHandleForHeapStart());
        D3D12_DESCRIPTOR_RANGE ranges[]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0, 0},
                                        {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 3, 0, 0, 3}};
        D3D12_ROOT_PARAMETER args[2]{};
        args[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        args[0].Constants = {0, 0, 2};
        args[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        args[1].DescriptorTable = {2, ranges};
        D3D12_ROOT_SIGNATURE_DESC rd{2, args, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ComPtr<ID3DBlob> blob, shader, errors;
        hr(D3D12SerializeRootSignature(&rd, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors), "inline root blob");
        hr(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&p.root)),
           "inline root");
        hr(D3DCompile(temporal_gpu_inputs::shader_source, sizeof(temporal_gpu_inputs::shader_source) - 1, nullptr,
                      nullptr, nullptr, "main", "cs_5_0",
                      D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                      &shader, &errors),
           "inline converter compile");
        D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};
        pd.pRootSignature = p.root.Get();
        pd.CS = {shader->GetBufferPointer(), shader->GetBufferSize()};
        hr(device->CreateComputePipelineState(&pd, IID_PPV_ARGS(&p.pipeline)), "inline converter pipeline");
        D3D12_COMMAND_QUEUE_DESC q{};
        q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        // A private queue, never one of the game's: past every detour, so the
        // presentation layer neither proxies it through the SDK nor keeps it
        // (game-125: one per live rebuild filled its 16 slots).
        {
            astera::Internal internal;
            hr(device->CreateCommandQueue(&q, IID_PPV_ARGS(&p.queue)), "inline create queue");
        }
        hr(device->CreateCommandAllocator(q.Type, IID_PPV_ARGS(&p.allocator)), "inline create allocator");
        hr(device->CreateCommandList(0, q.Type, p.allocator.Get(), nullptr, IID_PPV_ARGS(&p.create_list)),
           "inline create list");
        hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&p.create_fence)), "inline create fence");
        require(NVSDK_NGX_D3D12_GetCapabilityParameters(&p.parameters) == NVSDK_NGX_Result_Success,
                "inline capabilities");
        int available{}, update{}, init{};
        require(p.parameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &available) ==
                        NVSDK_NGX_Result_Success &&
                    available == 1 &&
                    p.parameters->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &update) ==
                        NVSDK_NGX_Result_Success &&
                    update == 0 &&
                    p.parameters->Get(NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, &init) ==
                        NVSDK_NGX_Result_Success &&
                    init == 1,
                "inline capability values");
        // The SDK's quality value and the preset hint it reads for that value;
        // preset K (transformer) for every mode.
        NVSDK_NGX_PerfQuality_Value quality = NVSDK_NGX_PerfQuality_Value_MaxPerf;
        const char *preset = NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Performance;
        switch (geometry.mode) {
        case ReconstructionMode::dlaa:
            quality = NVSDK_NGX_PerfQuality_Value_DLAA;
            preset = NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA;
            break;
        case ReconstructionMode::quality:
            quality = NVSDK_NGX_PerfQuality_Value_MaxQuality;
            preset = NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Quality;
            break;
        case ReconstructionMode::balanced:
            quality = NVSDK_NGX_PerfQuality_Value_Balanced;
            preset = NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Balanced;
            break;
        case ReconstructionMode::performance: break;
        }
        p.quality = static_cast<unsigned>(quality);
        if (geometry.mode != ReconstructionMode::dlaa) {
            p.optimal_result = NGX_DLSS_GET_OPTIMAL_SETTINGS(p.parameters, ow, oh, quality, &p.optimal.width,
                                                             &p.optimal.height, &p.maximum.width, &p.maximum.height,
                                                             &p.minimum.width, &p.minimum.height, &p.sharpness);
            require(p.optimal_result == 1 && render_supported(geometry, p.optimal, p.minimum, p.maximum),
                    "inline SDK render extent outside the mode's range");
        }
        NVSDK_NGX_DLSS_Create_Params create{};
        create.Feature = {w, h, ow, oh, quality};
        create.InFeatureCreateFlags = 75;
        p.parameters->Set(preset, static_cast<unsigned>(NVSDK_NGX_DLSS_Hint_Render_Preset_K));
        p.create_result = NGX_D3D12_CREATE_DLSS_EXT(p.create_list.Get(), 1, 1, &p.feature, p.parameters, &create);
        require(p.create_result == 1 && p.feature, "inline CreateFeature");
        hr(p.create_list->Close(), "inline create close");
        ID3D12CommandList *lists[]{p.create_list.Get()};
        p.queue->ExecuteCommandLists(1, lists);
        hr(p.queue->Signal(p.create_fence.Get(), 1), "inline create signal");
        const auto start_tick = GetTickCount64();
        while (p.create_fence->GetCompletedValue() == 0 && GetTickCount64() - start_tick < 15000) Sleep(1);
        require(p.create_fence->GetCompletedValue() == 1, "inline create completion");
        hr(device->GetDeviceRemovedReason(), "inline create device");
        hr(p.allocator->Reset(), "inline create retirement");
        hr(p.create_list->Reset(p.allocator.Get(), nullptr), "inline create reset");
        hr(p.create_list->Close(), "inline create retired close");
        p.creation_completed = true;
    } catch (...) {
        static_cast<void>(impl_.release());
        throw;
    }
}
InlineFrame::~InlineFrame() {
    if (impl_ && !impl_->released) static_cast<void>(impl_.release());
}
bool InlineFrame::record(ID3D12GraphicsCommandList *list, ID3D12Resource *native,
                         const temporal_present::Bindings &state, HistoryFrame history, bool reset,
                         float frame_ms) noexcept {
    auto &p = *impl_;
    p.last = {};
    if (p.released || !p.failure.empty()) return false;
    p.attempted = true;
    try {
        require(p.creation_completed && state.ready() && history.frame && history.epoch &&
                    std::isfinite(history.jitter_x) && std::isfinite(history.jitter_y) &&
                    std::abs(history.jitter_x) <= .5F && std::abs(history.jitter_y) <= .5F,
                "inline recording contract");
        require(std::isfinite(frame_ms) && frame_ms > 0 && frame_ms <= 1000 && (p.evaluations || reset),
                "inline history timing/reset");
        require(!p.evaluations || history.frame > p.history.frame, "inline monotonic history");
        same_device(list, p.device.Get());
        require(list->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT, "inline DIRECT list");
        p.handoff->bind(p.output.Get(), native);
        temporal_present::RestoreBindings restore(list, state);
        p.native_identity = reinterpret_cast<std::uintptr_t>(native);
        p.list_identity = reinterpret_cast<std::uintptr_t>(list);
        p.history = history;
        p.generation = state.generation;
        p.reset = reset;
        p.frame_ms = frame_ms;
        try {
            if (p.evaluations) {
                for (auto &r : p.converted)
                    transition(list, r.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                               D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                transition(list, p.output.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                           D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            }
            p.issued = true;
            p.last.commands = true;
            for (auto &r : p.raw)
                transition(list, r.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                           D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            list->SetComputeRootSignature(p.root.Get());
            list->SetPipelineState(p.pipeline.Get());
            ID3D12DescriptorHeap *heaps[]{p.heap.Get()};
            list->SetDescriptorHeaps(1, heaps);
            const unsigned dims[]{p.width, p.height};
            list->SetComputeRoot32BitConstants(0, 2, dims, 0);
            list->SetComputeRootDescriptorTable(1, p.heap->GetGPUDescriptorHandleForHeapStart());
            list->Dispatch((p.width + 15) / 16, (p.height + 15) / 16, 1);
            for (unsigned i = 0; i < 3; ++i) {
                transition(list, p.raw[i].Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                           D3D12_RESOURCE_STATE_COPY_DEST);
                transition(list, p.converted[i].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                           D3D12_RESOURCE_STATE_COPY_SOURCE);
                transition(list, p.converted[i].Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                           D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            }
            NVSDK_NGX_D3D12_DLSS_Eval_Params eval{};
            eval.Feature.pInColor = p.converted[0].Get();
            eval.Feature.pInOutput = p.output.Get();
            eval.pInDepth = p.converted[1].Get();
            eval.pInMotionVectors = p.converted[2].Get();
            eval.InReset = reset;
            eval.InMVScaleX = eval.InMVScaleY = 1;
            eval.InPreExposure = eval.InExposureScale = 1;
            eval.InRenderSubrectDimensions = {p.width, p.height};
            eval.InFrameTimeDeltaInMsec = frame_ms;
            eval.InJitterOffsetX = history.jitter_x;
            eval.InJitterOffsetY = history.jitter_y;
            // Diagnostic sentinel: output texels the SDK leaves unwritten show
            // at full brightness.
            if (!perf::on(perf::no_output_sentinel)) {
                const float sentinel[]{65504, 65504, 65504, 65504};
                const auto stride = p.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                list->ClearUnorderedAccessViewFloat(
                    {p.heap->GetGPUDescriptorHandleForHeapStart().ptr + UINT64(6) * stride},
                    p.clear_heap->GetCPUDescriptorHandleForHeapStart(), p.output.Get(), sentinel, 0, nullptr);
                D3D12_RESOURCE_BARRIER order{};
                order.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                order.UAV.pResource = p.output.Get();
                list->ResourceBarrier(1, &order);
            }
            p.evaluate_result = NGX_D3D12_EVALUATE_DLSS_EXT(list, p.feature, p.parameters, &eval);
            require(p.evaluate_result == 1, "inline EvaluateFeature");
            transition(list, p.output.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
            p.handoff->record(list);
            p.copied = true;
            p.last.native_output_written = true;
        } catch (...) {
            restore.apply();
            p.restored = true;
            p.last.bindings_restored = true;
            throw;
        }
        restore.apply();
        p.restored = true;
        p.last.bindings_restored = true;
        p.last.success = true;
        ++p.evaluations;
        if (reset) ++p.resets;
        return true;
    } catch (const std::exception &e) {
        p.failure = e.what();
        return false;
    }
}
void InlineFrame::retire(ID3D12Fence *fence, std::uint64_t value) {
    auto &p = *impl_;
    require(p.creation_completed && !p.released && fence && value != UINT64_MAX, "inline retire contract");
    same_device(fence, p.device.Get());
    const auto completed = fence->GetCompletedValue();
    require(completed != UINT64_MAX && completed >= value, "inline retire incomplete fence");
    hr(p.device->GetDeviceRemovedReason(), "inline retire device");
    // The handoff was recorded only into submissions the fence covers.
    if (p.handoff->recorded() && !p.handoff->retired()) {
        require(value != 0, "inline retire handoff fence");
        p.handoff->retire(fence, value, true);
    }
    p.completion_fence = reinterpret_cast<std::uintptr_t>(fence);
    p.completion_value = value;
    p.release_result = NVSDK_NGX_D3D12_ReleaseFeature(p.feature);
    require(p.release_result == 1, "inline retire ReleaseFeature");
    p.feature = nullptr;
    p.destroy_result = NVSDK_NGX_D3D12_DestroyParameters(p.parameters);
    require(p.destroy_result == 1, "inline retire DestroyParameters");
    p.parameters = nullptr;
    p.released = true;
}
std::uint64_t InlineFrame::allocation_bytes() const noexcept {
    return impl_->allocation_bytes;
}
RecordAttempt InlineFrame::last_attempt() const noexcept {
    return impl_->last;
}
Geometry InlineFrame::geometry() const noexcept {
    return impl_->geometry;
}
ID3D12Resource *InlineFrame::raw_color() const noexcept {
    return impl_->raw[0].Get();
}
std::array<ID3D12Resource *, 5> InlineFrame::private_images() const noexcept {
    const auto &p = *impl_;
    return {p.converted[0].Get(), p.converted[1].Get(), p.converted[2].Get(), p.output.Get(), p.handoff->packed()};
}
void InlineFrame::write(std::ostream &o) const {
    const auto &p = *impl_;
    o << std::setprecision(9) << "{\"schema\":1,\"allocation_bytes\":" << p.allocation_bytes
      << ",\"creation_completed\":" << p.creation_completed << ",\"attempted\":" << p.attempted
      << ",\"issued\":" << p.issued << ",\"restored\":" << p.restored << ",\"copied\":" << p.copied
      << ",\"released\":" << p.released << ",\"completion_fence\":" << p.completion_fence
      << ",\"completion_value\":" << p.completion_value << ",\"create_result\":" << p.create_result
      << ",\"evaluate_result\":" << p.evaluate_result << ",\"release_result\":" << p.release_result
      << ",\"destroy_result\":" << p.destroy_result << ",\"failure\":" << std::quoted(p.failure)
      << ",\"frame\":" << p.history.frame << ",\"epoch\":" << p.history.epoch << ",\"generation\":" << p.generation
      << ",\"jitter\":[" << p.history.jitter_x << ',' << p.history.jitter_y << "],\"reset\":" << p.reset
      << ",\"frame_ms\":" << p.frame_ms << ",\"evaluations\":" << p.evaluations << ",\"resets\":" << p.resets
      << ",\"last_attempt\":{\"commands\":" << p.last.commands << ",\"bindings_restored\":" << p.last.bindings_restored
      << ",\"native_output_written\":" << p.last.native_output_written << ",\"success\":" << p.last.success << '}'
      << ",\"device\":" << reinterpret_cast<std::uintptr_t>(p.device.Get())
      << ",\"create_flags\":75,\"mode\":" << p.quality
      << ",\"preset\":11,\"input_formats\":[26,41,42],\"converted_formats\":[2,41,16],\"mv_scale\":[1,1],\"mv_jittered\":false,\"pre_exposure\":1,\"raw\":["
      << reinterpret_cast<std::uintptr_t>(p.raw[0].Get()) << ',' << reinterpret_cast<std::uintptr_t>(p.raw[1].Get())
      << ',' << reinterpret_cast<std::uintptr_t>(p.raw[2].Get()) << ']' << ",\"command\":" << p.list_identity
      << ",\"native_output\":" << p.native_identity << ",\"width\":" << p.width << ",\"height\":" << p.height
      << ",\"geometry_schema\":1,\"allocation\":[" << p.geometry.allocation.width << ',' << p.geometry.allocation.height
      << "],\"render\":[" << p.width << ',' << p.height << "],\"output_extent\":[" << p.geometry.output.width << ','
      << p.geometry.output.height
      << "],\"input_origin\":[0,0],\"motion_units\":\"render_pixels\",\"jitter_units\":\"render_pixels\""
      << ",\"optimal_result\":" << p.optimal_result << ",\"optimal\":[" << p.optimal.width << ',' << p.optimal.height
      << "],\"minimum\":[" << p.minimum.width << ',' << p.minimum.height << "],\"maximum\":[" << p.maximum.width << ','
      << p.maximum.height << "],\"shutdown_deferred\":true}";
}
} // namespace mhw::dlss_sdk
