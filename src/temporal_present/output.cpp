#include "output.hpp"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <array>
#include <stdexcept>
#include <string>

namespace mhw::temporal_present {
namespace {
using Microsoft::WRL::ComPtr;
void require(bool v, const char *why) { if (!v) throw std::runtime_error(why); }
void hr(HRESULT v, const char *why) { require(SUCCEEDED(v), why); }
constexpr char shader[] = R"(
cbuffer Extent : register(b0) { uint width; uint height; uint activeWidth; uint activeHeight; };
Texture2D<float4> source : register(t0);
RWTexture2D<float3> packed : register(u0);
float quantize(float value, uint mantissa) {
    // Explicit nearest-even quantization. Typed stores may truncate narrower
    // float formats; storing an already representable value is deterministic.
    uint q;
    if (value < 0.00006103515625) q = uint(round(value * exp2(float(14 + mantissa))));
    else {
        uint bits = asuint(value), shift = 23 - mantissa;
        q = ((bits + ((1u << (shift - 1)) - 1) + ((bits >> shift) & 1)) >> shift) - (112u << mantissa);
    }
    uint exponent = q >> mantissa, fraction = q & ((1u << mantissa) - 1);
    return exponent == 0 ? float(fraction) * exp2(-float(14 + mantissa)) :
        asfloat(((exponent + 112) << 23) | (fraction << (23 - mantissa)));
}
[numthreads(16,16,1)] void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= width || id.y >= height) return;
#ifdef SPATIAL_RECOVERY
    // Pixel-center bilinear reconstruction. Clamp every integer load to the
    // active rectangle; allocation padding must never enter the image.
    precise float2 position = (float2(id.xy) + 0.5) * float2(activeWidth,activeHeight) / float2(width,height) - 0.5;
    position = clamp(position, 0.0, float2(activeWidth-1,activeHeight-1));
    uint2 low = uint2(position), high = min(low+1, uint2(activeWidth-1,activeHeight-1));
    precise float2 fraction = position - float2(low);
    precise float3 a = source.Load(int3(low,0)).rgb;
    precise float3 b = source.Load(int3(high.x,low.y,0)).rgb;
    precise float3 c = source.Load(int3(low.x,high.y,0)).rgb;
    precise float3 d = source.Load(int3(high,0)).rgb;
    precise float3 upper = a + (b-a)*fraction.x;
    precise float3 lower = c + (d-c)*fraction.x;
    precise float3 color = upper + (lower-upper)*fraction.y;
#else
    float3 color = source.Load(int3(id.xy,0)).rgb;
#endif
    // Native HDR storage is unsigned. No tone mapping or exposure adjustment.
    float3 v = clamp(color, 0.0, float3(65024.0,65024.0,64512.0));
    packed[id.xy] = float3(quantize(v.r,6),quantize(v.g,6),quantize(v.b,5));
}
)";
void same_device(ID3D12DeviceChild *object, ID3D12Device *expected) {
    require(object && expected, "presentation null object/device");
    ComPtr<ID3D12Device> actual; ComPtr<IUnknown> a, b;
    hr(object->GetDevice(IID_PPV_ARGS(&actual)), "presentation device query");
    hr(actual.As(&a), "presentation device identity");
    hr(expected->QueryInterface(IID_PPV_ARGS(&b)), "presentation expected identity");
    require(a.Get() == b.Get(), "presentation foreign device");
}
bool texture(const D3D12_RESOURCE_DESC &d, unsigned w, unsigned h, DXGI_FORMAT f) {
    return d.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && d.Width == w && d.Height == h &&
        d.DepthOrArraySize == 1 && d.MipLevels == 1 && d.SampleDesc.Count == 1 &&
        d.SampleDesc.Quality == 0 && d.Format == f && d.Layout == D3D12_TEXTURE_LAYOUT_UNKNOWN;
}
void barrier(ID3D12GraphicsCommandList *list, ID3D12Resource *resource,
             D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
    D3D12_RESOURCE_BARRIER b{}; b.Transition = {resource, UINT_MAX, from, to};
    list->ResourceBarrier(1, &b);
}
}
struct Output::Impl {
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12Resource> source, destination, packed;
    ID3D12Resource *borrowed_destination{};
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> pipeline;
    ComPtr<ID3D12DescriptorHeap> heap;
    ComPtr<ID3D12GraphicsCommandList> list;
    // The streaming prototype keeps a bounded set of native destinations/lists
    // pinned until its final fence and recording-retirement proof. Descriptors
    // refer only to the unchanging private source and packed texture.
    std::array<ComPtr<ID3D12Resource>,16> destinations;
    std::array<ComPtr<ID3D12GraphicsCommandList>,128> lists;
    unsigned destination_count{},list_count{};
    bool continuous{},caller_retained{};
    unsigned width{}, height{};
    SpatialSource region{}; bool spatial{};
    bool bound{}, issued{}, retired{};
};
Output::Output(ID3D12Device *device,unsigned w,unsigned h,bool continuous,bool caller_retained)
    : Output(device,w,h,continuous,caller_retained,nullptr) {}
Output::Output(ID3D12Device *device,SpatialSource source,unsigned w,unsigned h,bool continuous,bool caller_retained)
    : Output(device,w,h,continuous,caller_retained,&source) {}
Output::Output(ID3D12Device *device, unsigned w, unsigned h, bool continuous,bool caller_retained,const SpatialSource *source) : impl_(std::make_unique<Impl>()) {
    require(device && w && h && w <= 8192 && h <= 8192 && UINT64(w)*h <= 16777216,
            "presentation extent/device");
    require(!caller_retained||continuous,"presentation caller retention mode");
    auto &p = *impl_; p.device = device; p.width = w; p.height = h;p.continuous=continuous;p.caller_retained=caller_retained;
    p.spatial=source!=nullptr;p.region=source?*source:SpatialSource{w,h,w,h};
    const auto &r=p.region;
    require(r.width&&r.height&&r.width<=r.allocation_width&&r.height<=r.allocation_height&&r.width<=w&&r.height<=h&&
        r.allocation_width<=8192&&r.allocation_height<=8192&&UINT64(r.allocation_width)*r.allocation_height<=16777216&&
        // Exact proportion, or the truncated half of a pillarboxed odd scene
        // (3413x1440 rendered at 1706x720).
        (UINT64(r.width)*h==UINT64(r.height)*w||(r.width==w/2&&r.height==h/2)),"presentation source region");
    D3D12_FEATURE_DATA_FORMAT_SUPPORT support{DXGI_FORMAT_R11G11B10_FLOAT};
    hr(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)), "presentation format query");
    require((support.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D) &&
            (support.Support1 & D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) &&
            (support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE), "presentation native typed store");
    support = {p.spatial?DXGI_FORMAT_R11G11B10_FLOAT:DXGI_FORMAT_R32G32B32A32_FLOAT};
    hr(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)), "presentation source format query");
    require((support.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D) &&
            (support.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_LOAD), "presentation source typed load");
    D3D12_RESOURCE_DESC desc{}; desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = w; desc.Height = h; desc.DepthOrArraySize = desc.MipLevels = 1;
    desc.SampleDesc.Count = 1; desc.Format = DXGI_FORMAT_R11G11B10_FLOAT;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    hr(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&p.packed)), "presentation packed texture");
    D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    hd.NumDescriptors = 2; hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&p.heap)), "presentation private descriptors");
    std::array<D3D12_DESCRIPTOR_RANGE, 2> ranges{};
    ranges[0] = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0};
    ranges[1] = {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 1};
    std::array<D3D12_ROOT_PARAMETER, 2> params{};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[0].Constants = {0, 0, 4};
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable = {2, ranges.data()};
    D3D12_ROOT_SIGNATURE_DESC rd{}; rd.NumParameters = 2; rd.pParameters = params.data();
    ComPtr<ID3DBlob> signature, code, errors;
    hr(D3D12SerializeRootSignature(&rd, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors), "presentation root serialization");
    hr(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&p.root)), "presentation root");
    const D3D_SHADER_MACRO macros[]{{"SPATIAL_RECOVERY","1"},{nullptr,nullptr}};
    const auto compiled = D3DCompile(shader, sizeof(shader)-1, "temporal_present", p.spatial?macros:nullptr, nullptr, "main", "cs_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
    if (FAILED(compiled)) throw std::runtime_error(errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()) : "presentation shader compile");
    D3D12_COMPUTE_PIPELINE_STATE_DESC pd{}; pd.pRootSignature = p.root.Get();
    pd.CS = {code->GetBufferPointer(), code->GetBufferSize()};
    hr(device->CreateComputePipelineState(&pd, IID_PPV_ARGS(&p.pipeline)), "presentation pipeline");
}
Output::~Output() { if (impl_ && impl_->issued && !impl_->retired) static_cast<void>(impl_.release()); }
void Output::bind(ID3D12Resource *source, ID3D12Resource *destination) {
    auto &p = *impl_; require((!p.bound && !p.issued)||(p.continuous&&!p.retired), "presentation binding lifetime");
    same_device(source, p.device.Get()); same_device(destination, p.device.Get());
    ComPtr<IUnknown> a, b; hr(source->QueryInterface(IID_PPV_ARGS(&a)), "presentation source identity");
    hr(destination->QueryInterface(IID_PPV_ARGS(&b)), "presentation destination identity");
    require(a.Get() != b.Get()&&source!=p.packed.Get()&&destination!=p.packed.Get(), "presentation source/destination alias");
    const auto s = source->GetDesc(), d = destination->GetDesc();
    require(texture(s, p.region.allocation_width, p.region.allocation_height, p.spatial?DXGI_FORMAT_R11G11B10_FLOAT:DXGI_FORMAT_R32G32B32A32_FLOAT) &&
            !(s.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE), "presentation source layout");
    require(texture(d, p.width, p.height, DXGI_FORMAT_R11G11B10_FLOAT) &&
            (d.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), "presentation destination layout");
    if(p.continuous&&!p.caller_retained){
        bool found=false;for(unsigned i=0;i<p.destination_count;++i)found=found||p.destinations[i].Get()==destination;
        require(found||p.destination_count<p.destinations.size(),"presentation destination capacity");
        if(!found)p.destinations[p.destination_count++]=destination;
    }
    if(p.bound){require(source==p.source.Get(),"presentation immutable source");
        if(p.caller_retained)p.borrowed_destination=destination;else p.destination=destination;return;}
    const auto cpu = p.heap->GetCPUDescriptorHandleForHeapStart();
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{}; srv.Format = s.Format; srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; srv.Texture2D.MipLevels = 1;
    p.device->CreateShaderResourceView(source, &srv, cpu);
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{}; uav.Format = d.Format; uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    const auto step = p.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    p.device->CreateUnorderedAccessView(p.packed.Get(), nullptr, &uav, {cpu.ptr + step});
    p.source = source;if(p.caller_retained)p.borrowed_destination=destination;else p.destination=destination;p.bound = true;
}
void Output::prepare_record(ID3D12GraphicsCommandList *list) {
    auto &p = *impl_; require(p.bound && (!p.issued||p.continuous)&&!p.retired, "presentation recording lifetime");
    same_device(list, p.device.Get()); require(list->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT, "presentation direct list");
    if(p.continuous&&!p.caller_retained){
        bool found=false;for(unsigned i=0;i<p.list_count;++i)found=found||p.lists[i].Get()==list;
        require(found||p.list_count<p.lists.size(),"presentation list capacity");
        if(!found)p.lists[p.list_count++]=list;
    }
}
void Output::record(ID3D12GraphicsCommandList *list,D3D12_RESOURCE_STATES source_state) {
    require(source_state==D3D12_RESOURCE_STATE_COPY_SOURCE||source_state==D3D12_RESOURCE_STATE_COPY_DEST||
        source_state==D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE||
        source_state==(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),"presentation source state");
    prepare_record(list);auto &p=*impl_;
    if(p.issued)barrier(list,p.packed.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    if(!p.caller_retained)p.list = list;p.issued = true;
    auto *destination=p.caller_retained?p.borrowed_destination:p.destination.Get();
    if(source_state!=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
        barrier(list, p.source.Get(), source_state, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    list->SetComputeRootSignature(p.root.Get()); list->SetPipelineState(p.pipeline.Get());
    ID3D12DescriptorHeap *heaps[]{p.heap.Get()}; list->SetDescriptorHeaps(1, heaps);
    const unsigned dims[]{p.width, p.height,p.region.width,p.region.height}; list->SetComputeRoot32BitConstants(0, 4, dims, 0);
    list->SetComputeRootDescriptorTable(1, p.heap->GetGPUDescriptorHandleForHeapStart());
    list->Dispatch((p.width+15)/16, (p.height+15)/16, 1);
    if(source_state!=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
        barrier(list, p.source.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, source_state);
    barrier(list, p.packed.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    barrier(list, destination, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyResource(destination, p.packed.Get());
    barrier(list, destination, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}
void Output::retire(ID3D12Fence *fence, std::uint64_t value, bool recording_retired) {
    auto &p = *impl_; require(p.issued && !p.retired && recording_retired && value && value != UINT64_MAX,
                            "presentation retirement receipt");
    same_device(fence, p.device.Get()); const auto completed = fence->GetCompletedValue();
    require(completed != UINT64_MAX && completed >= value, "presentation incomplete fence/device removed");
    hr(p.device->GetDeviceRemovedReason(), "presentation removed device"); p.retired = true;
}
bool Output::recorded() const noexcept { return impl_->issued; }
bool Output::spatial_contract(SpatialSource s,unsigned w,unsigned h) const noexcept {
    return impl_->spatial&&impl_->region==s&&impl_->width==w&&impl_->height==h&&impl_->continuous&&!impl_->retired;
}
bool Output::retired() const noexcept { return impl_->retired; }
ID3D12Resource *Output::packed() const noexcept { return impl_->packed.Get(); }
}
