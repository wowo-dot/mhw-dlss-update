#include "replay.hpp"
#include "live.hpp"
#include "shader.hpp"
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

namespace mhw::temporal_gpu_inputs {
namespace {
using Microsoft::WRL::ComPtr;
void require(bool value, const char *reason) { if (!value) throw std::runtime_error(reason); }
void hr(HRESULT value, const char *reason) {
    if (FAILED(value)) throw std::runtime_error(std::string(reason) + " HRESULT " + std::to_string(static_cast<unsigned>(value)));
}
struct Context {
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12DescriptorHeap> heap;
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> pipeline;
    ComPtr<ID3D12InfoQueue> messages;
    std::array<ComPtr<ID3D12Resource>, 3> inputs, outputs, uploads, readbacks;
    HANDLE event{};
    bool submitted{}, completed{};
    ~Context() { if (event) CloseHandle(event); }
};
struct Footprint { D3D12_PLACED_SUBRESOURCE_FOOTPRINT placed{}; unsigned rows{}; UINT64 row_bytes{}, bytes{}; };
constexpr std::array<DXGI_FORMAT, 3> input_formats{DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_UINT};
constexpr std::array<DXGI_FORMAT, 3> output_formats{DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT};
constexpr std::array<unsigned, 3> output_components{4, 1, 2};

D3D12_RESOURCE_DESC texture(unsigned width, unsigned height, DXGI_FORMAT format, bool output) {
    D3D12_RESOURCE_DESC d{}; d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width = width; d.Height = height; d.DepthOrArraySize = 1; d.MipLevels = 1;
    d.Format = format; d.SampleDesc.Count = 1;
    d.Flags = output ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
    return d;
}
ComPtr<ID3D12Resource> resource(Context &c, const D3D12_RESOURCE_DESC &desc, D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES state) {
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = type;
    ComPtr<ID3D12Resource> out;
    hr(c.device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&out)), "CreateCommittedResource");
    return out;
}
ComPtr<ID3D12Resource> buffer(Context &c, UINT64 bytes, D3D12_HEAP_TYPE type) {
    D3D12_RESOURCE_DESC d{}; d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width = bytes; d.Height = 1; d.DepthOrArraySize = 1; d.MipLevels = 1;
    d.SampleDesc.Count = 1; d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return resource(c, d, type, type == D3D12_HEAP_TYPE_UPLOAD ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST);
}
Footprint footprint(Context &c, const D3D12_RESOURCE_DESC &d, unsigned bytes_per_pixel) {
    Footprint f; c.device->GetCopyableFootprints(&d, 0, 1, 0, &f.placed, &f.rows, &f.row_bytes, &f.bytes);
    require(f.placed.Offset == 0 && f.rows == d.Height && f.row_bytes == d.Width * bytes_per_pixel &&
        f.placed.Footprint.Format == d.Format && f.placed.Footprint.Width == d.Width &&
        f.placed.Footprint.Height == d.Height && f.placed.Footprint.Depth == 1 &&
        f.placed.Footprint.RowPitch == ((f.row_bytes + 255) & ~255ULL) &&
        f.bytes == UINT64(f.placed.Footprint.RowPitch) * (d.Height - 1) + f.row_bytes, "Unexpected copy footprint");
    return f;
}
void transition(Context &c, ID3D12Resource *r, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition = {r, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after};
    c.list->ResourceBarrier(1, &b);
}
void supported(Context &c, DXGI_FORMAT format, bool output) {
    D3D12_FEATURE_DATA_FORMAT_SUPPORT s{}; s.Format = format;
    hr(c.device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &s, sizeof(s)), "format support query");
    require((s.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D) != 0, "Texture2D format unsupported");
    if (output) require((s.Support1 & D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) != 0 &&
        (s.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE) != 0, "Typed UAV store unsupported");
    else require((s.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_LOAD) != 0, "Typed shader load unsupported");
}
void compile(Context &c) {
    ComPtr<ID3DBlob> code, errors, signature;
    const auto result = D3DCompile(shader_source, sizeof(shader_source) - 1, "temporal_gpu_inputs", nullptr, nullptr,
        "main", "cs_5_0", D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
    if (FAILED(result) && errors) std::cerr.write(static_cast<const char *>(errors->GetBufferPointer()), static_cast<std::streamsize>(errors->GetBufferSize()));
    hr(result, "Input conversion shader compile");
    std::array<D3D12_DESCRIPTOR_RANGE, 2> ranges{};
    ranges[0] = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0, 0};
    ranges[1] = {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 3, 0, 0, 3};
    std::array<D3D12_ROOT_PARAMETER, 2> parameters{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[0].Constants = {0, 0, 2};
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].DescriptorTable = {static_cast<UINT>(ranges.size()), ranges.data()};
    D3D12_ROOT_SIGNATURE_DESC desc{}; desc.NumParameters = static_cast<UINT>(parameters.size()); desc.pParameters = parameters.data();
    hr(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors), "root signature serialization");
    hr(c.device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&c.root)), "root signature creation");
    D3D12_COMPUTE_PIPELINE_STATE_DESC pso{}; pso.pRootSignature = c.root.Get(); pso.CS = {code->GetBufferPointer(), code->GetBufferSize()};
    hr(c.device->CreateComputePipelineState(&pso, IID_PPV_ARGS(&c.pipeline)), "compute pipeline creation");
}
void debug_messages(Context &c, Result &out) {
    if (!c.messages) return;
    const auto n = c.messages->GetNumStoredMessagesAllowedByRetrievalFilter();
    for (UINT64 i = 0; i < n; ++i) {
        SIZE_T bytes{}; hr(c.messages->GetMessage(i, nullptr, &bytes), "debug message length");
        std::vector<std::byte> storage(bytes); auto *m = reinterpret_cast<D3D12_MESSAGE *>(storage.data());
        hr(c.messages->GetMessage(i, m, &bytes), "debug message");
        if (m->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) ++out.debug_errors;
        if (m->Severity == D3D12_MESSAGE_SEVERITY_WARNING) ++out.debug_warnings;
        if (m->Severity <= D3D12_MESSAGE_SEVERITY_WARNING && out.debug_errors + out.debug_warnings <= 16)
            std::cerr << "D3D12 diagnostic " << m->ID << ": " << m->pDescription << '\n';
    }
    require(!out.debug_errors && !out.debug_warnings, "D3D12 debug diagnostics rejected replay");
}
}

struct Live::Impl {
    std::unique_ptr<Context> context{std::make_unique<Context>()};
    std::array<Footprint, 3> layouts;
    unsigned width{}, height{};
    bool used{},ready{};
    std::uint64_t bytes{};
};
Live::~Live() = default;
ID3D12Resource *Live::converted(unsigned i) const noexcept {
    return impl_ && impl_->ready && impl_->context && i<3 ? impl_->context->outputs[i].Get() : nullptr;
}
ID3D12Fence *Live::completed_fence() const noexcept {
    return impl_ && impl_->ready && impl_->context ? impl_->context->fence.Get() : nullptr;
}
ID3D12Resource *Live::input(unsigned i) const noexcept {
    return impl_ && impl_->context && i < 3 ? impl_->context->inputs[i].Get() : nullptr;
}
std::uint64_t Live::allocation_bytes() const noexcept { return impl_->bytes; }
Live::Live(ID3D12Device *device, unsigned w, unsigned h) : impl_(std::make_unique<Impl>()) {
    require(device && w && h && w <= 8192 && h <= 8192 && UINT64(w)*h*4 <= 16ULL*1024*1024, "Live extent/device");
    auto &p = *impl_; auto &c = *p.context; c.device = device; p.width = w; p.height = h;
    for (const auto format : input_formats) supported(c, format, false);
    for (const auto format : output_formats) supported(c, format, true);
    D3D12_COMMAND_QUEUE_DESC q{}; q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr(device->CreateCommandQueue(&q, IID_PPV_ARGS(&c.queue)), "owned conversion queue");
    hr(device->CreateCommandAllocator(q.Type, IID_PPV_ARGS(&c.allocator)), "owned conversion allocator");
    hr(device->CreateCommandList(0, q.Type, c.allocator.Get(), nullptr, IID_PPV_ARGS(&c.list)), "owned conversion list");
    hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&c.fence)), "owned conversion fence");
    c.event = CreateEventW(nullptr, FALSE, FALSE, nullptr); require(c.event != nullptr, "owned conversion event");
    compile(c);
    D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    hd.NumDescriptors = 6; hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&c.heap)), "owned conversion views");
    const auto step = device->GetDescriptorHandleIncrementSize(hd.Type);
    const auto start = c.heap->GetCPUDescriptorHandleForHeapStart();
    for (unsigned i = 0; i < 3; ++i) {
        const auto src = texture(w, h, input_formats[i], false);
        const auto dst = texture(w, h, output_formats[i], true);
        p.layouts[i] = footprint(c, dst, output_components[i]*4);
        const auto in_bytes = device->GetResourceAllocationInfo(0, 1, &src).SizeInBytes;
        const auto out_bytes = device->GetResourceAllocationInfo(0, 1, &dst).SizeInBytes;
        const auto rb = buffer(c, p.layouts[i].bytes, D3D12_HEAP_TYPE_READBACK);
        const auto rb_desc = rb->GetDesc();
        const auto rb_bytes = device->GetResourceAllocationInfo(0, 1, &rb_desc).SizeInBytes;
        constexpr UINT64 budget = 300ULL*1024*1024;
        require(in_bytes <= budget && out_bytes <= budget && rb_bytes <= budget &&
            p.bytes + in_bytes + out_bytes + rb_bytes <= budget, "Live allocation budget");
        p.bytes += in_bytes + out_bytes + rb_bytes;
        c.inputs[i] = resource(c, src, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);
        c.outputs[i] = resource(c, dst, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        c.readbacks[i] = rb;
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{}; srv.Format = input_formats[i]; srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; srv.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(c.inputs[i].Get(), &srv, {start.ptr + SIZE_T(i)*step});
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{}; uav.Format = output_formats[i]; uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(c.outputs[i].Get(), nullptr, &uav, {start.ptr + SIZE_T(i+3)*step});
    }
}
Result Live::convert() {
    auto &p = *impl_;
    require(p.context && !p.used, "Live conversion is one-shot"); p.used = true;
    auto &c = *p.context; Result out;
    const auto luid = c.device->GetAdapterLuid(); out.adapter_low = luid.LowPart; out.adapter_high = luid.HighPart;
    out.allocation_bytes = p.bytes;
    out.queue_address=reinterpret_cast<std::uintptr_t>(c.queue.Get());out.list_address=reinterpret_cast<std::uintptr_t>(c.list.Get());
    out.allocator_address=reinterpret_cast<std::uintptr_t>(c.allocator.Get());out.fence_address=reinterpret_cast<std::uintptr_t>(c.fence.Get());
    out.heap_address=reinterpret_cast<std::uintptr_t>(c.heap.Get());
    for(unsigned i=0;i<3;++i){out.output_addresses[i]=reinterpret_cast<std::uintptr_t>(c.outputs[i].Get());
        out.readback_addresses[i]=reinterpret_cast<std::uintptr_t>(c.readbacks[i].Get());}
    // The caller has validated producer fences and recording retirement. Only
    // private resources appear on this list; native pipeline state is untouched.
    for (auto &r : c.inputs) transition(c, r.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    c.list->SetComputeRootSignature(c.root.Get()); c.list->SetPipelineState(c.pipeline.Get());
    ID3D12DescriptorHeap *heaps[]{c.heap.Get()}; c.list->SetDescriptorHeaps(1, heaps);
    const unsigned dims[]{p.width, p.height}; c.list->SetComputeRoot32BitConstants(0, 2, dims, 0);
    c.list->SetComputeRootDescriptorTable(1, c.heap->GetGPUDescriptorHandleForHeapStart());
    c.list->Dispatch((p.width+15)/16, (p.height+15)/16, 1);
    for (unsigned i = 0; i < 3; ++i) {
        transition(c, c.outputs[i].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION src{}, dst{}; src.pResource = c.outputs[i].Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; dst.pResource = c.readbacks[i].Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; dst.PlacedFootprint = p.layouts[i].placed;
        c.list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }
    hr(c.list->Close(), "Close live conversion");
    try {
        ID3D12CommandList *lists[]{c.list.Get()}; c.submitted = true; c.queue->ExecuteCommandLists(1, lists);
        hr(c.queue->Signal(c.fence.Get(), 1), "Live conversion signal");
        hr(c.fence->SetEventOnCompletion(1, c.event), "Live conversion event");
        require(WaitForSingleObject(c.event, 15000) == WAIT_OBJECT_0, "Live conversion timeout");
        out.fence_value = c.fence->GetCompletedValue();
        require(out.fence_value >= 1 && out.fence_value != UINT64_MAX, "Live conversion fence/device removed");
        hr(c.device->GetDeviceRemovedReason(), "Live conversion device removed");
        c.completed = out.completed = true;
        hr(c.allocator->Reset(), "Live conversion allocator retirement");
        hr(c.list->Reset(c.allocator.Get(), nullptr), "Live conversion list retirement");
        hr(c.list->Close(), "Close retired conversion"); out.allocator_retired = true;
        const std::array<std::vector<float> *, 3> targets{&out.color, &out.depth, &out.motion};
        for (unsigned i = 0; i < 3; ++i) {
            auto &values = *targets[i]; const auto &layout = p.layouts[i];
            values.resize(std::size_t(p.width)*p.height*output_components[i]);
            void *mapped{}; D3D12_RANGE range{0, static_cast<SIZE_T>(layout.bytes)};
            hr(c.readbacks[i]->Map(0, &range, &mapped), "Live conversion readback");
            for (unsigned y = 0; y < p.height; ++y)
                std::memcpy(values.data()+std::size_t(y)*p.width*output_components[i],
                    static_cast<const std::byte *>(mapped)+std::size_t(y)*layout.placed.Footprint.RowPitch,
                    static_cast<std::size_t>(layout.row_bytes));
            D3D12_RANGE empty{}; c.readbacks[i]->Unmap(0, &empty);
        }
    } catch (...) {
        if (c.submitted && !c.completed) static_cast<void>(p.context.release());
        throw;
    }
    p.ready=true;return out;
}

Result replay(const Frame &f, const Adapter &selection) {
    std::string why; if (!validate(f, why)) throw std::invalid_argument(why);
    require(selection.warp || selection.luid_low || selection.luid_high, "Explicit hardware LUID or WARP required");
    Result out;
    auto c = std::make_unique<Context>();
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
        debug->EnableDebugLayer(); out.debug_layer = true;
        ComPtr<ID3D12Debug1> validation;
        if (SUCCEEDED(debug.As(&validation))) { validation->SetEnableGPUBasedValidation(TRUE); out.gpu_validation = true; }
    }
    ComPtr<IDXGIFactory4> factory; ComPtr<IDXGIAdapter1> adapter;
    hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "DXGI factory");
    if (selection.warp) hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)), "WARP adapter");
    else hr(factory->EnumAdapterByLuid({selection.luid_low, selection.luid_high}, IID_PPV_ARGS(&adapter)), "Exact captured adapter");
    DXGI_ADAPTER_DESC1 desc{}; hr(adapter->GetDesc1(&desc), "Adapter identity");
    out.adapter_low = desc.AdapterLuid.LowPart; out.adapter_high = desc.AdapterLuid.HighPart;
    out.vendor = desc.VendorId; out.device = desc.DeviceId;
    hr(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&c->device)), "D3D12 device");
    if (out.debug_layer) hr(c->device.As(&c->messages), "D3D12 debug queue");
    for (const auto format : input_formats) supported(*c, format, false);
    for (const auto format : output_formats) supported(*c, format, true);
    D3D12_COMMAND_QUEUE_DESC q{}; q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr(c->device->CreateCommandQueue(&q, IID_PPV_ARGS(&c->queue)), "private direct queue");
    hr(c->device->CreateCommandAllocator(q.Type, IID_PPV_ARGS(&c->allocator)), "private allocator");
    hr(c->device->CreateCommandList(0, q.Type, c->allocator.Get(), nullptr, IID_PPV_ARGS(&c->list)), "private command list");
    hr(c->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&c->fence)), "private completion fence");
    c->event = CreateEventW(nullptr, FALSE, FALSE, nullptr); require(c->event != nullptr, "completion event");
    compile(*c);
    D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    hd.NumDescriptors = 6; hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr(c->device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&c->heap)), "owned typed view heap");
    const auto step = c->device->GetDescriptorHandleIncrementSize(hd.Type);
    const auto heap_start = c->heap->GetCPUDescriptorHandleForHeapStart();
    const std::array<std::span<const std::uint32_t>, 3> data{f.color, f.depth, f.motion};
    std::array<Footprint, 3> read_layouts;
    for (unsigned i = 0; i < 3; ++i) {
        const auto src = texture(f.width, f.height, input_formats[i], false);
        const auto dst = texture(f.width, f.height, output_formats[i], true);
        const auto upload = footprint(*c, src, 4); read_layouts[i] = footprint(*c, dst, output_components[i] * 4);
        const auto &read = read_layouts[i];
        out.allocation_bytes += c->device->GetResourceAllocationInfo(0, 1, &src).SizeInBytes +
            c->device->GetResourceAllocationInfo(0, 1, &dst).SizeInBytes + upload.bytes + read.bytes;
        require(out.allocation_bytes <= 512ULL * 1024 * 1024, "Owned resource budget exceeded");
        c->inputs[i] = resource(*c, src, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);
        c->outputs[i] = resource(*c, dst, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        c->uploads[i] = buffer(*c, upload.bytes, D3D12_HEAP_TYPE_UPLOAD);
        c->readbacks[i] = buffer(*c, read.bytes, D3D12_HEAP_TYPE_READBACK);
        void *mapped{}; D3D12_RANGE no_read{0, 0}; hr(c->uploads[i]->Map(0, &no_read, &mapped), "upload Map");
        std::memset(mapped, 0, static_cast<std::size_t>(upload.bytes));
        for (unsigned y = 0; y < f.height; ++y)
            std::memcpy(static_cast<std::byte *>(mapped) + std::size_t(y) * upload.placed.Footprint.RowPitch,
                data[i].data() + std::size_t(y) * f.width, static_cast<std::size_t>(upload.row_bytes));
        D3D12_RANGE written{0, static_cast<SIZE_T>(upload.bytes)}; c->uploads[i]->Unmap(0, &written);
        D3D12_TEXTURE_COPY_LOCATION from{}, to{}; from.pResource = c->uploads[i].Get();
        from.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; from.PlacedFootprint = upload.placed;
        to.pResource = c->inputs[i].Get(); to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        c->list->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
        transition(*c, c->inputs[i].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{}; srv.Format = input_formats[i]; srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; srv.Texture2D.MipLevels = 1;
        c->device->CreateShaderResourceView(c->inputs[i].Get(), &srv, {heap_start.ptr + SIZE_T(i) * step});
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{}; uav.Format = output_formats[i]; uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        c->device->CreateUnorderedAccessView(c->outputs[i].Get(), nullptr, &uav, {heap_start.ptr + SIZE_T(i + 3) * step});
    }
    c->list->SetComputeRootSignature(c->root.Get()); c->list->SetPipelineState(c->pipeline.Get());
    ID3D12DescriptorHeap *heaps[]{c->heap.Get()}; c->list->SetDescriptorHeaps(1, heaps);
    const unsigned dims[]{f.width, f.height}; c->list->SetComputeRoot32BitConstants(0, 2, dims, 0);
    c->list->SetComputeRootDescriptorTable(1, c->heap->GetGPUDescriptorHandleForHeapStart());
    c->list->Dispatch((f.width + 15) / 16, (f.height + 15) / 16, 1);
    for (unsigned i = 0; i < 3; ++i) {
        transition(*c, c->outputs[i].Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION from{}, to{}; from.pResource = c->outputs[i].Get();
        from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        to.pResource = c->readbacks[i].Get(); to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; to.PlacedFootprint = read_layouts[i].placed;
        c->list->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
    }
    hr(c->list->Close(), "Close replay list");
    try {
        ID3D12CommandList *lists[]{c->list.Get()}; c->submitted = true;
        c->queue->ExecuteCommandLists(1, lists);
        hr(c->queue->Signal(c->fence.Get(), 1), "completion Signal");
        hr(c->fence->SetEventOnCompletion(1, c->event), "completion event registration");
        require(WaitForSingleObject(c->event, 15000) == WAIT_OBJECT_0, "GPU completion timeout");
        out.fence_value = c->fence->GetCompletedValue();
        require(out.fence_value != std::numeric_limits<UINT64>::max() && out.fence_value >= 1, "GPU fence incomplete/device removed");
        hr(c->device->GetDeviceRemovedReason(), "device removed"); c->completed = true; out.completed = true;
        hr(c->allocator->Reset(), "retire completed recording allocator");
        hr(c->list->Reset(c->allocator.Get(), nullptr), "retire completed recording list");
        hr(c->list->Close(), "close retired list"); out.allocator_retired = true;
        debug_messages(*c, out);
        const std::array<std::vector<float> *, 3> destinations{&out.color, &out.depth, &out.motion};
        for (unsigned i = 0; i < 3; ++i) {
            auto &values = *destinations[i]; const auto &layout = read_layouts[i];
            values.resize(std::size_t(f.width) * f.height * output_components[i]);
            void *mapped{}; D3D12_RANGE read{0, static_cast<SIZE_T>(layout.bytes)};
            hr(c->readbacks[i]->Map(0, &read, &mapped), "readback Map");
            for (unsigned y = 0; y < f.height; ++y)
                std::memcpy(values.data() + std::size_t(y) * f.width * output_components[i],
                    static_cast<const std::byte *>(mapped) + std::size_t(y) * layout.placed.Footprint.RowPitch,
                    static_cast<std::size_t>(layout.row_bytes));
            D3D12_RANGE no_write{0, 0}; c->readbacks[i]->Unmap(0, &no_write);
        }
    } catch (...) {
        if (c->submitted && !c->completed) {
            // D3D12 lists do not own the resources they reference. Never destroy
            // them after an uncertain submission; this standalone process exits.
            static_cast<void>(c.release());
        }
        throw;
    }
    return out;
}
}
